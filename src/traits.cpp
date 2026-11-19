// The tricks of the foes. Each foe has one or two, and the same trick comes back in many foes, so a player learns it once.
// Everything a trick does is seen on the map: a count on the ground a turn before it lands, a coloured floor where a cloud
// or a trail lies, a grey statue, a heap of gold, a '?' over a blind hound that has lost you. (See the brainstorm, 4b.)
#include <deque>
#include <libtcod.hpp>

#include "game.hpp"

using namespace data;

namespace {
std::string cap(std::string s) { if (!s.empty() && s[0] >= 'a' && s[0] <= 'z') s[0] = char(s[0] - 32); return s; }
std::string the(const Mon& m) { return m.rank == "unique" ? m.name : m.name.rfind("the ", 0) == 0 ? m.name : "the " + m.name; }
int rnd_i(double v) { return int(std::lround(v)); }
json xy(int x, int y) { return json::array({x, y}); }
json cells_json(const std::vector<P>& cs) { json a = json::array(); for (auto c : cs) a.push_back(xy(c.x, c.y)); return a; }
bool wet(int t) { return t == WATER || t == SLUDGE || t == LAVA; }
}  // namespace

std::string Mon::trick(const std::string& t) const {
  for (auto& s : def->traits)
    if (s.rfind(t + ":", 0) == 0) return s.substr(t.size() + 1);
  return "";
}

// ground that does something, laid by a foe: a cloud, a puddle of acid, a trail; owner 0 is the foes'
void Game::lay(P c, int kind, int turns, int dmg) {
  if (!floor->walk(c.x, c.y) || way_down(floor->tile(c.x, c.y))) return;
  for (auto& g : floor->ground)
    if (g.p == c) {
      if (g.kind == G_EMBER || g.kind == G_HALLOW) return;  // the heroes' own ground stays
      g.kind = kind; g.turns = std::max(g.turns, turns); g.dmg = std::max(g.dmg, dmg); g.owner = 0;
      return;
    }
  floor->ground.push_back({c, kind, turns, dmg, 0});
}

// fire set to grease: the whole slick it lies in goes up
void Game::ignite(P c) {
  std::deque<P> q;
  for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx)
      for (auto& g : floor->ground)
        if (g.kind == G_GREASE && g.p == P{c.x + dx, c.y + dy}) { g.kind = G_FIRE; g.turns = 4; g.dmg = 3 + lvl; q.push_back(g.p); }
  bool any = !q.empty();
  while (!q.empty()) {
    P p = q.front(); q.pop_front();
    for (auto& g : floor->ground)
      if (g.kind == G_GREASE && cheb(g.p.x, g.p.y, p.x, p.y) == 1) { g.kind = G_FIRE; g.turns = 4; g.dmg = 3 + lvl; q.push_back(g.p); }
  }
  if (any) say_near(c.x, c.y, "The grease catches fire!", 0xFF8C28);
}

void Game::wake(Mon& m, const std::string& how) {
  if (!m.dormant && !m.hidden) return;
  bool was_hidden = m.hidden > 0;
  m.dormant = 0; m.hidden = 0; m.awake = true;
  m.energy = 0;  // it needs a turn to get up
  say_near(m.x, m.y, how.empty() ? (was_hidden ? "The heap of gold opens a mouth full of teeth!" : cap(the(m)) + " moves!") : how, 0xFFB060);
}

// a foe's own turn, when its trick has something to do; true when it used the turn
bool Game::trait_turn(Mon& m, Hero& h, int d, bool sees) {
  if (m.tcool > 0) --m.tcool;
  // a blind foe hunts by sound: a hero who stands still (and is not next to it) is lost to it
  if (m.can("hears")) {
    bool still = h.moved != turn;
    if (still && d > 1) {
      if (!m.listen) for (auto* o : active()) if (o->sees(m.x, m.y)) tip(*o, "hears", "A ? over a foe: it is blind, and has lost you. It hunts by sound: stand still.");
      m.listen = 1;
      if (rnd.real() < .5) step_random(m);
      return true;
    }
    m.listen = 0;
  }
  if (m.flee > 0) {  // a stalker after its blow: back off out of reach
    --m.flee;
    P best{m.x, m.y};
    int bd = d;
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        int nx = m.x + dx, ny = m.y + dy;
        if (floor->walk(nx, ny) && !mon_at(nx, ny) && !hero_at(nx, ny) && cheb(nx, ny, h.x, h.y) > bd) { bd = cheb(nx, ny, h.x, h.y); best = {nx, ny}; }
      }
    m.x = best.x; m.y = best.y;
    return true;
  }
  if (m.brace > 0 && m.can("brace")) --m.brace;
  if (!sees) return false;
  auto clear = [&](int range) {  // nothing in the way to the hero
    if (d > range) return false;
    for (auto&& [x, y] : tcod::BresenhamLine({m.x, m.y}, {h.x, h.y}).without_start()) {
      if (x == h.x && y == h.y) return true;
      if (!floor->walk(x, y) || mon_at(x, y)) return false;
    }
    return true;
  };
  // the crab: the shield goes down when it strikes and comes up again the turn after
  if (m.can("brace") && m.brace == 0 && d <= 1 && m.tcool == 0) {
    m.brace = 2; m.tcool = 3;
    say_near(m.x, m.y, cap(the(m)) + " raises its shell.", 0xC0A080);
    for (auto* o : active()) if (o->sees(m.x, m.y)) tip(*o, "brace", "A braced foe takes a quarter of the harm. Wait until it strikes: then its guard is down.");
    return true;
  }
  if (m.tcool > 0) return false;
  auto blow = [&](const std::string& name, int cool) {
    if (!wind_up(m, h, name)) return false;
    m.tcool = cool;
    return true;
  };
  std::string lob = m.trick("lob");
  if (!lob.empty() && d >= 2 && d <= m.def->rng && rnd.real() < .7)  // thrown over the others, onto where you stand
    return blow(lob == "jar" ? "jar" : "lob", 2);
  if (m.can("bolt") && d >= 2 && clear(m.def->rng)) return blow("bolt", 2);
  if (m.can("leap") && d >= 2 && d <= 3 && clear(3)) return blow("leap", 5);
  if (m.can("hook") && d >= 2 && d <= 5 && clear(5) && rnd.real() < .6) return blow("hook", 5);
  if (m.can("reach") && d == 2 && (h.x - m.x) % 2 == 0 && (h.y - m.y) % 2 == 0 && clear(2)) return blow("reach", 1);
  if (m.can("sweep") && d == 1 && rnd.real() < .5) return blow("swing", 3);
  if (m.can("heavy") && d == 1 && rnd.real() < .45) return blow("smash", 5);
  if (m.can("grab") && d == 1 && rnd.real() < .5) return blow("grab", 5);
  if (m.can("shriek") && d <= 2) return blow("shriek", 4);
  if (m.can("incense") && d <= 5) {
    for (int dy = -2; dy <= 2; ++dy)
      for (int dx = -2; dx <= 2; ++dx)
        if (dx * dx + dy * dy <= 5) lay({m.x + dx, m.y + dy}, G_SMOKE, 6, 0);
    m.tcool = 7;
    say_near(m.x, m.y, cap(the(m)) + " swings its censer, and smoke rolls out.", 0xB0A890);
    for (auto* o : active()) if (o->sees(m.x, m.y)) tip(*o, "incense", "Grey smoke: you cannot see through it, and the dead in it heal. Fight them out of it.");
    return true;
  }
  return false;
}

// where a trick's blow will land, as it starts
bool Game::trait_cells(Mon& m, Hero& h, const std::string& name, Blow& b) {
  auto add = [&](int x, int y) { if (floor->walk(x, y)) b.cells.push_back({x, y}); };
  if (name == "lob" || name == "leap" || name == "grab") add(h.x, h.y);
  else if (name == "jar" || name == "shriek") {
    P c = name == "jar" ? P{h.x, h.y} : P{m.x, m.y};
    int r = name == "jar" ? 1 : 2;
    for (int dy = -r; dy <= r; ++dy) for (int dx = -r; dx <= r; ++dx) if (name == "jar" || dx || dy) add(c.x + dx, c.y + dy);
  } else if (name == "bolt" || name == "hook" || name == "reach") {
    int range = name == "bolt" ? 8 : name == "hook" ? 5 : 2;
    int tx = m.x + (h.x - m.x) * 8, ty = m.y + (h.y - m.y) * 8;
    for (auto&& [x, y] : tcod::BresenhamLine({m.x, m.y}, {tx, ty}).without_start()) {
      if (!floor->walk(x, y) || cheb(x, y, m.x, m.y) > range) break;
      b.cells.push_back({x, y});
      if (name != "bolt" && x == h.x && y == h.y) break;
    }
  } else if (name == "swing") {
    // the hero's square and the two beside it that are also next to the foe: an arc in front of it
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        int x = h.x + dx, y = h.y + dy;
        if (cheb(x, y, m.x, m.y) == 1 && cheb(x, y, h.x, h.y) <= 1 && (std::abs(dx) + std::abs(dy) <= 1)) add(x, y);
      }
  } else return false;
  return true;
}

// a trick's blow lands on the heroes still in it
void Game::trait_land(Mon& m, const Blow& b, const std::vector<Hero*>& hit) {
  auto hurt_all = [&](double share) {
    for (auto* o : hit) hero_hurt(*o, &m, std::min(rnd_i(b.dmg * share), stats(*o).maxhp * 11 / 20), b.elem);
  };
  if (b.name == "lob") {
    std::string what = m.trick("lob");
    hurt_all(1.0);
    for (auto* o : hit) {
      if (what == "acid") { o->sunder = 4; o->say("The acid eats into your armour.", 0xE6E650); tip(*o, "sunder", "Your armour is split: it counts for half until it mends."); }
      if (what == "tar") { o->held = 1; o->say("The tar sticks you fast.", 0x907050); }
    }
    if (what == "acid") lay(b.cells[0], G_ACID, 3, 2 + lvl / 3);
    if (what == "tar") lay(b.cells[0], G_TAR, 6, 0);
  } else if (b.name == "jar") {
    hurt_all(1.0);
    for (auto* o : hit) o->numb = 3;
    if (rnd.real() < .3) {
      P c;
      if (free_near(b.cells[b.cells.size() / 2].x, b.cells[b.cells.size() / 2].y, 1, c))
        if (auto s = spawn("shade", c.x, c.y)) { s->summoned = true; s->awake = true; s->maxhp = s->hp = s->maxhp / 2; say_near(c.x, c.y, "A screaming spirit rises out of the broken jar!", 0xC0A0E0); }
    }
  } else if (b.name == "leap") {
    // it lands beside the marked square, nearest to where it sprang from
    P to{-1, -1};
    int bd = 1 << 30;
    P c = b.cells[0];
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        int x = c.x + dx, y = c.y + dy;
        if ((dx || dy) && floor->walk(x, y) && !mon_at(x, y) && !hero_at(x, y) && cheb(x, y, m.x, m.y) < bd) { bd = cheb(x, y, m.x, m.y); to = {x, y}; }
      }
    if (to.x >= 0) { add_fx({{"k", "inward"}, {"x", m.x}, {"y", m.y}, {"col", m.col}}); m.x = to.x; m.y = to.y; }
    hurt_all(1.0);
  } else if (b.name == "hook") {
    hurt_all(1.0);
    for (auto* o : hit) {
      // dragged along the chain to the foe's side
      P to{o->x, o->y};
      for (auto& c : b.cells) if (cheb(c.x, c.y, m.x, m.y) == 1 && !mon_at(c.x, c.y) && !hero_at(c.x, c.y)) { to = c; break; }
      if (to != P{o->x, o->y}) {
        add_fx({{"k", "bolt"}, {"cells", cells_json(b.cells)}, {"col", 0xA0A0A8}, {"ch", "~"}, {"src", xy(m.x, m.y)}});
        o->x = to.x; o->y = to.y;
        o->say(cap(the(m)) + " drags you in on its chain.", 0xFF9070);
        arrive(*o);
      }
    }
  } else if (b.name == "swing") {
    hurt_all(1.0);
    if (m.can("knock"))
      for (auto* o : hit) {
        int dx = (o->x > m.x) - (o->x < m.x), dy = (o->y > m.y) - (o->y < m.y);
        int nx = o->x + dx, ny = o->y + dy;
        if (floor->walk(nx, ny) && !mon_at(nx, ny) && !hero_at(nx, ny)) { o->x = nx; o->y = ny; o->say("The blow throws you back.", 0xFF9070); arrive(*o); }
      }
  } else if (b.name == "grab") {
    hurt_all(1.0);
    for (auto* o : hit) {
      o->held = 2; o->dazed = 1; o->bleed = 3; o->bleed_dmg = std::max(o->bleed_dmg, 2 + lvl / 3);
      o->say(cap(the(m)) + " shuts you in its cage!", 0xFF6060);
    }
  } else if (b.name == "shriek") {
    hurt_all(1.0);
    for (auto* o : hit) { o->dazed = 1; o->say("The shriek leaves you reeling.", 0xC0A0E0); }
  } else {
    hurt_all(1.0);  // reach, bolt
  }
}

// a foe's blow struck home
void Game::trait_hit(Mon& m, Hero& h, int dmg) {
  if (m.can("latch") && !h.latched) {
    h.latched = m.id;
    h.say(cap(the(m)) + " latches onto you!", 0xE0D0A0);
    tip(h, "latch", "Latched: it drinks your life each turn. Step away from it, or kill it.");
  }
  if (m.can("hold")) { h.held = 1; h.say(cap(the(m)) + "'s teeth hold you fast.", 0xE0C060); }
  if (m.can("bleed")) { h.bleed = 3; h.bleed_dmg = std::max(h.bleed_dmg, 2 + lvl / 4); }
  if (m.can("brand")) {
    h.brand = 3;
    h.say("The iron brands you. Everything hurts more.", 0xFF8050);
    tip(h, "brand", "Branded: for three turns every blow hurts you a quarter more.");
  }
  if (m.can("sunder")) { h.sunder = 4; h.say("The hammer splits your armour.", 0xE0B080); }
  if (m.can("cough")) { h.ashen = 3; h.say(cap(the(m)) + " coughs ash in your face.", 0xA0A0A0); }
  if (m.can("skittish")) m.flee = 2;
  if (m.can("brace")) m.brace = 0;  // the guard is down while it strikes
  (void)dmg;
}

// as it dies
void Game::trait_death(Mon& m) {
  std::string burst = m.trick("burst");
  if (!burst.empty()) {
    int kind = burst == "acid" ? G_ACID : G_GAS;
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) lay({m.x + dx, m.y + dy}, kind, 4, 2 + lvl / 2);
    say_near(m.x, m.y, kind == G_ACID ? cap(the(m)) + " bursts, and black acid spills out." : cap(the(m)) + " bursts in a cloud of foul gas.",
             kind == G_ACID ? 0xD8E040 : 0x90C860);
    for (auto* o : active()) if (o->sees(m.x, m.y)) tip(*o, "burst", "Some foes burst as they die. Kill them from afar, or step out of what they leave.");
  }
  if (m.can("embers") || m.def->fire) ignite({m.x, m.y});
  if (m.can("dim"))
    for (auto* o : active()) if (o->dimmed && o->sees(m.x, m.y)) o->say("Your light flares up again.", 0xFFE0A0);
  if (m.can("rich")) floor->items.push_back({m.x, m.y, nullptr, lvl * rnd.range(6, 12), "", 0});
  // the heroes it held let go
  for (auto* o : active()) if (o->latched == m.id) o->latched = 0;
}

// harm about to be done to a foe, as its trick changes it
int Game::trait_hurt(Mon& m, int dmg) {
  if (m.dormant || m.hidden) wake(m, "");
  if (m.brace > 0) dmg = std::max(1, dmg / 4);
  if (m.can("swarm") && !m.split && m.hp - dmg > 0 && (m.hp - dmg) * 2 < m.maxhp) {
    // the swarm breaks apart into rats
    m.split = true;
    say_near(m.x, m.y, cap(the(m)) + " breaks apart into squealing rats!", 0xC09080);
    for (int i = 0; i < 3; ++i) {
      P c;
      if (free_near(m.x, m.y, 2, c)) if (auto r = spawn("rat", c.x, c.y)) r->awake = true;
    }
  }
  return dmg;
}

void Game::trait_moved(Mon& m, P from) {
  std::string trail = m.trick("trail");
  if (trail == "grease") lay(from, G_GREASE, 30, 0);
  if (trail == "slag") lay(from, G_SLAG, 6, 3 + lvl / 2);
  if (floor->burning(from.x, from.y)) ignite(from);
}

// ghosts walk through stone; swimmers and the floating cross what others cannot
bool Game::trait_step(Mon& m, Hero& h) {
  bool ghost = m.can("ghost"), swim = m.can("swims") || m.can("float");
  if (!ghost && !swim) return false;
  if (m.can("rooted")) return true;
  P best{-1, -1};
  int bd = cheb(m.x, m.y, h.x, h.y);
  for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx) {
      int nx = m.x + dx, ny = m.y + dy;
      if ((!dx && !dy) || nx < 1 || ny < 1 || nx >= floor->w - 1 || ny >= floor->h - 1 || mon_at(nx, ny) || hero_at(nx, ny)) continue;
      int t = floor->tile(nx, ny);
      bool ok = floor->walk(nx, ny) || (ghost && t == WALL) || (swim && (wet(t) || t == CHASM));
      if (t == LAVA && !m.def->fire) ok = ghost;
      if (!ok) continue;
      int dd = cheb(nx, ny, h.x, h.y);
      // a ghost goes straight; the others keep to the ground where it is shorter
      if (!ghost && dist_[ny * W + nx] < (1 << 29) && dist_[ny * W + nx] < dist_[m.y * W + m.x]) dd -= 1;
      if (dd < bd) { bd = dd; best = {nx, ny}; }
    }
  if (best.x < 0) return !ghost;  // a swimmer with nowhere better goes the usual way
  if (floor->tile(best.x, best.y) == DOOR && !ghost) { floor->set(best.x, best.y, OPEN); return true; }
  m.x = best.x; m.y = best.y;
  return true;
}

// every turn: the ground of the foes, leeches drinking, a chill around the cold dead, the marks the tricks left
void Game::traits_tick() {
  for (auto* h : active()) {
    if (auto* g = floor->ground_at(h->x, h->y)) {
      if (g->kind == G_GAS) {
        h->poison = std::max(h->poison, 3);
        h->poison_dmg = std::max(h->poison_dmg, rnd_i(g->dmg * (1 - stats(*h).res_p / 100.0)));
        tip(*h, "gas", "Green cloud: gas that poisons you while you stand in it.");
      } else if (g->kind == G_ACID) {
        hero_hurt(*h, nullptr, g->dmg, "poison");
        tip(*h, "acid", "Yellow: acid. It burns you every turn you stand in it.");
      } else if (g->kind == G_SLAG || g->kind == G_FIRE) {
        hero_hurt(*h, nullptr, g->dmg, "fire");
      } else if (g->kind == G_TAR) {
        if (h->held == 0) { h->held = 1; h->say("Your feet stick in the tar.", 0x907050); }
      } else if (g->kind == G_GREASE) {
        tip(*h, "grease", "Black grease catches fire: burn it under your foes, and keep out of it yourself.");
      }
      if (h->dead) continue;
    }
    // a leech drinks while it is next to you
    if (h->latched) {
      MonP lm;
      for (auto& m : floor->mons) if (m->id == h->latched) lm = m;
      if (!lm || cheb(lm->x, lm->y, h->x, h->y) > 1) { h->latched = 0; h->say("The leech comes loose.", 0xA0A0A0); }
      else {
        int dr = 1 + lvl / 4;
        h->hp -= dr;
        lm->hp = std::min(lm->maxhp, lm->hp + dr);
        if (h->hp <= 0) hero_hurt(*h, lm.get(), 1, "");
      }
    }
    if (h->bleed > 0) { --h->bleed; hero_hurt(*h, nullptr, h->bleed_dmg, "bleed"); if (!h->bleed) h->bleed_dmg = 0; }
    for (auto& m : floor->mons)
      if (m->awake && m->can("chill") && cheb(m->x, m->y, h->x, h->y) <= 1 && rnd.below(100) >= stats(*h).res_c) h->numb = std::max(h->numb, 2);
    if (h->held > 0) --h->held;
    if (h->sunder > 0) --h->sunder;
    if (h->brand > 0) --h->brand;
    if (h->ashen > 0) --h->ashen;
  }
  // burning grease and smoke: the dead heal in incense, anything in fire burns
  auto ms = floor->mons;
  for (auto& m : ms) {
    if (m->hp <= 0) continue;
    auto* g = floor->ground_at(m->x, m->y);
    if (!g) continue;
    if (g->kind == G_SMOKE && m->def->undead) m->hp = std::min(m->maxhp, m->hp + 2 + lvl / 3);
    if (g->kind == G_FIRE && g->owner == 0 && !m->def->fire) hurt(*m, g->dmg, nullptr);
  }
  for (auto& g : floor->ground) if (g.owner == 0) --g.turns;
  floor->ground.erase(std::remove_if(floor->ground.begin(), floor->ground.end(), [](const Ground& g) { return g.turns <= 0; }), floor->ground.end());
}
