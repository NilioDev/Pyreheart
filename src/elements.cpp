// The five elements in play (plan.md section 1): stacks laid by every hit, set off by a heavy one, fading after three
// quiet turns, washed off by water and thawed by fire. Heroes and foes follow the same rules; the limits that keep a
// player in control (Rime slows a hero at most 30%, Rot halves healing at most, Ash never takes a whole turn) hold
// for heroes and great foes alike.
#include "game.hpp"

using namespace elem;

namespace {
std::string cap(std::string s) { if (!s.empty() && s[0] >= 'a' && s[0] <= 'z') s[0] = char(s[0] - 32); return s; }
std::string the(const Mon& m) { return m.rank == "unique" ? m.name : m.name.rfind("the ", 0) == 0 ? m.name : "the " + m.name; }
int rnd_i(double v) { return int(std::lround(v)); }
}  // namespace
bool elem::watery(int t) { return t == data::SHALLOW || t == data::WATER || t == data::QUENCH || t == data::TROUGH || t == data::BASIN || t == data::FOUNTAIN || t == data::WELL || t == data::MIRE; }
bool elem::fiery(int t) { return t == data::FIRE || t == data::TORCH || t == data::BRAZIER || t == data::CAMPFIRE || t == data::HEARTH || t == data::LAVA || t == data::EMBERS || t == data::SLAGPIT || t == data::LIT; }
namespace {
std::string stacks_word(int n, int e) { return std::to_string(n) + " " + NAME[e] + (n == 1 ? " stack" : " stacks"); }
}  // namespace

// ------------------------------------------------------------------ resistances
// A foe resists its zone's element (25%, and 10% more on each Frost level, at most 75%) and fears its weakness.
int Game::mon_res(const Mon& m, int e) const {
  const Zone& z = ZONE[std::clamp(floor->act, 0, 9)];
  int pact = 0;  // Ashen Pact: every foe resists 15% less
  for (auto& [id, h] : heroes) if (h->perk("ashenpact")) pact = 15;
  if (e == z.resist) return std::min(75, 25 + 10 * frost) - pact;
  if (e == z.weak) return -25 - pact;
  return -pact;
}
// A hero's resistances come from gear, less 10% on each Frost level, but never more than 50% (plan.md section 7).
int Game::hero_res(const Hero& h, int e) const {
  Stats st = stats(h);
  auto s = [&](const std::string& k) { int t = 0; for (auto& [slot, it] : h.equip) t += it->stat(k); return t; };
  int r = e == EMBER ? st.res_f : e == RIME ? st.res_c : e == ROT ? st.res_p : e == ASH ? std::min(75, s("res_ash")) : std::min(75, s("res_blood"));
  if (e == RIME && (h.perk("coldblood") || h.leg("kingsfrost"))) return 100;  // Cold Blood, Kingsfrost Mail: Rime cannot touch it
  return r - frost_res_cut() - 15 * h.perk("ashenpact");
}
// Rot cuts healing: 20% a stack; a hero or a great foe keeps at least half.
int Game::heal_cut(const Stacks& s, bool capped) const {
  int blood = 0;  // Septic: a hero's Blood on a foe cuts its healing like Rot
  if (s.n[BLOOD] > 0) if (auto it = heroes.find(s.by[BLOOD]); it != heroes.end() && it->second->perk("septic")) blood = s.n[BLOOD];
  return std::min(capped ? 50 : 100, 20 * (s.n[ROT] + blood));
}
// Rime slows a foe 10% a stack; a great foe at most 30%.
int Game::rime_slow(const Mon& m) const { return std::min(m.rank == "boss" ? 30 : 50, 10 * m.stk.n[RIME]); }

// What a foe's blows lay, and whether it is a slow, heavy discharger or a quick builder.
void Game::assign_element(Mon& m) {
  const Zone& z = ZONE[std::clamp(floor->act, 0, 9)];
  unsigned hsh = 0;
  for (char c : m.def->key) hsh = hsh * 31 + (unsigned char)c;
  m.el = z.atk[hsh % 2];
  if (m.def->key == "ashenking") m.el = KING_ATK[0];
  if (m.def->fire || m.has("Fire-blooded")) m.el = EMBER;
  if (m.def->cold || m.has("Frost-touched")) m.el = RIME;
  if (m.def->poison > 0 || m.has("Venomous")) m.el = ROT;
  if (m.has("Vampiric")) m.el = BLOOD;
  if (m.has("Ashen")) m.el = ASH;
  m.discharger = m.rank != "boss" && (m.def->has("heavy") || m.def->has("sweep") || m.def->has("knock") || m.def->spd <= 80);
}

// ------------------------------------------------------------------ laying stacks
void Game::lay_on_mon(Hero& h, Mon& m, int n, int e) {
  if (e < 0) e = hero_el(h);
  if (e < 0 || m.hp <= 0) return;
  if (e == BLOOD && m.can("stone")) return;  // stone does not bleed
  int res = mon_res(m, e), laid = 0;
  if (h.perk("hoarfrost") && e == RIME && m.stk.n[RIME] > 0) ++n;                        // Hoarfrost
  if (h.perk("gravedust") && e == ROT && (m.smothered > 0 || m.stun > 0)) n *= 2;        // Grave Dust
  if (h.perk("fletching") && credit_ == 1 && rnd.below(3) == 0) ++n;                     // Arcane Fletching
  for (int i = 0; i < n; ++i)
    if ((res <= 0 || rnd.below(100) >= res) && !(h.perk("slowfire") && rnd.below(3) == 0)) ++laid;  // Slow Fire lays fewer
  if (res < 0 && rnd.below(100) < -res) ++laid;  // a weakness takes a second stack now and then
  if (!laid) return;
  m.stk.n[e] = std::min(MAX, m.stk.n[e] + laid);
  m.stk.idle[e] = 0;
  m.stk.by[e] = h.pid;
  if (e == EMBER && h.perk("cinderstorm"))  // Cinderstorm: the fire spreads ash to the foes beside it
    for (auto& o : floor->mons)
      if (o.get() != &m && o->hp > 0 && cheb(o->x, o->y, m.x, m.y) <= 1) { o->stk.n[ASH] = std::min(MAX, o->stk.n[ASH] + 1); o->stk.idle[ASH] = 0; o->stk.by[ASH] = h.pid; }
}

void Game::lay_on_hero(Mon& m, Hero& h, int n) {
  if (h.dead || h.god) return;
  int e = m.el, res = hero_res(h, e), laid = 0;
  if (res >= 100) return;
  for (int i = 0; i < n; ++i) if (res <= 0 || rnd.below(100) >= res) ++laid;
  if (!laid) return;
  bool first = h.stk.total() == 0;
  h.stk.n[e] = std::min(MAX, h.stk.n[e] + laid);
  h.stk.idle[e] = 0;
  if (first)
    tip(h, "stacks", std::string("Each blow lays a stack of its element on you; ") + NAME[e] +
                         " shows under your orbs. Three quiet turns and one falls off. Water and fire wash some away.");
}

// ------------------------------------------------------------------ setting them off
// A hero's heavy skill sets off all the stacks of the hero's element on a foe at once.
void Game::discharge_mon(Hero& h, Mon& m, int e) {
  int k = m.stk.n[e];
  if (k <= 0 || m.hp <= 0) return;
  if (h.leg("openchest") && !chest_chain_) {  // the Open Chest: one discharge sets off every foe in sight, and you
    chest_chain_ = true;
    discharge_mon(h, m, e);
    auto ms = floor->mons;
    for (auto& o : ms) if (o.get() != &m && o->hp > 0 && o->stk.n[e] > 0 && h.sees(o->x, o->y)) discharge_mon(h, *o, e);
    chest_chain_ = false;
    if (int mine = h.stk.n[e]; mine > 0 && !h.dead) {
      h.stk.n[e] = 0;
      add_fx({{"k", "burst"}, {"x", h.x}, {"y", h.y}, {"r", 0}, {"col", COL[e]}});
      h.say("☼ The Open Chest takes your own " + stacks_word(mine, e) + " with them.", COL[e]);
      hero_hurt(h, nullptr, std::min(stats(h).maxhp * 2 / 5, mine * (3 + lvl)), "elem");
    }
    return;
  }
  m.stk.n[e] = 0;
  m.stk.idle[e] = 0;
  Stats st = stats(h);
  // the titles' talents that go with a discharge (before the harm: the foe may not outlive it)
  if (combo(h, 0, EMBER)) h.fury = std::min(10, h.fury + 3);                               // Forgeborn
  if (e == RIME && combo(h, 0, RIME))                                                      // Rimeguard
    for (auto& o : floor->mons) if (o.get() != &m && o->hp > 0 && cheb(o->x, o->y, m.x, m.y) <= 1) { o->stk.n[RIME] = std::min(MAX, o->stk.n[RIME] + 1); o->stk.idle[RIME] = 0; o->stk.by[RIME] = h.pid; }
  if (combo(h, 2, EMBER))                                                                  // Pyrebinder
    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx)
      if (floor->walk(m.x + dx, m.y + dy)) floor->ground.push_back({{m.x + dx, m.y + dy}, 0, 3, 1 + power(h) / 4, h.pid});
  if (combo(h, 2, RIME)) {                                                                 // Rimecaller
    add_fx({{"k", "burst"}, {"x", m.x}, {"y", m.y}, {"r", 1}, {"col", COL[RIME]}});
    for (auto& o : floor->mons) if (o->hp > 0 && o->rank != "boss" && cheb(o->x, o->y, m.x, m.y) <= 1) o->frozen = std::max(o->frozen, 1);
  }
  if (e == ASH && combo(h, 2, ASH)) m.smothered = 3;                                       // Ashseer
  if (e == ASH && h.perk("choking"))                                                       // Choking Cloud
    for (auto& o : floor->mons) if (o.get() != &m && o->hp > 0 && cheb(o->x, o->y, m.x, m.y) <= 1) { o->stk.n[ASH] = std::min(MAX, o->stk.n[ASH] + 1); o->stk.idle[ASH] = 0; o->stk.by[ASH] = h.pid; }
  if (e == BLOOD && h.perk("frozenveins")) m.frozen = std::max(m.frozen, 1);                 // Frozen Veins
  double b = (st.lo + st.hi) / 2.0 * (1 + st.pct / 100.0) * .6 * k * (1 - mon_res(m, e) / 100.0);
  int x = m.x, y = m.y;
  add_fx({{"k", "burst"}, {"x", x}, {"y", y}, {"r", e == EMBER ? 1 : 0}, {"col", COL[e]}});
  std::string what = cap(the(m)) + ": " + stacks_word(k, e) + " go off";
  switch (e) {
    case EMBER: {
      int d = std::max(1, rnd_i(b));
      h.say("☼ " + what + ", and the fire leaps (" + std::to_string(d) + ").", COL[e]);
      std::vector<MonP> near;
      for (auto& o : floor->mons) if (o.get() != &m && o->hp > 0 && cheb(o->x, o->y, x, y) <= 1) near.push_back(o);
      hurt(m, d, &h);
      for (auto& o : near) if (o->hp > 0) { lay_on_mon(h, *o, 1); hurt(*o, std::max(1, rnd_i(b * .5)), &h); }
      break;
    }
    case RIME: {
      double mult = m.frozen > 0 ? 1.5 : 1.0;
      int d = std::max(1, rnd_i(b * 1.1 * mult));
      if (m.frozen > 0) m.frozen = 0;
      h.say("☼ " + what + ": it shatters (" + std::to_string(d) + ").", COL[e]);
      hurt(m, d, &h);
      break;
    }
    case ASH: {
      int d = std::max(1, rnd_i(b * .4));
      if (m.rank == "boss") {
        if (m.blow.turns > 0) { m.blow = {}; m.spent = 1; h.say("☼ " + what + ": smothered, its blow dies in the ash.", COL[e]); }
        else h.say("☼ " + what + ": it chokes on the soot.", COL[e]);
      } else {
        m.stun = std::max(m.stun, 1);
        if (m.blow.turns > 0) m.blow = {};
        h.say("☼ " + what + ": smothered, it loses its turn.", COL[e]);
      }
      hurt(m, d, &h);
      break;
    }
    case ROT: {
      int d = std::max(1, rnd_i(b * 1.25));
      h.say("☼ " + what + ": all the rot bursts at once (" + std::to_string(d) + ").", COL[e]);
      hurt(m, d, &h);
      break;
    }
    case BLOOD: {
      int d = std::max(1, rnd_i(b * .6));
      int heal = std::max(1, rnd_i(d * (100 - heal_cut(h.stk, true)) / 100.0));
      if (cheb(h.x, h.y, x, y) > 1 && !combo(h, 1, BLOOD)) heal = std::max(1, heal / 2);  // at range half, but for a Bloodhunter
      if (h.hp + heal > st.maxhp && combo(h, 0, BLOOD)) h.shield = std::max(h.shield, 2);     // Bloodsworn: the overflow shields
      h.hp = std::min(st.maxhp, h.hp + heal);
      num(h.x, h.y, "+" + std::to_string(heal), 0x60E060);
      h.say("☼ " + what + ": you drink its blood (" + std::to_string(d) + ", +" + std::to_string(heal) + " life).", COL[e]);
      hurt(m, d, &h);
      break;
    }
  }
}

// A foe's heavy blow sets off the stacks of its element on a hero. Milder on the player: Rime is a hard hit and never
// lost turns, Ash only blocks the skills for a turn.
void Game::discharge_hero(Mon& m, Hero& h, int e) {
  int k = h.stk.n[e];
  if (k <= 0 || h.dead) return;
  h.stk.n[e] = 0;
  h.stk.idle[e] = 0;
  double b = (m.lo + m.hi) / 2.0 * .55 * k * (1 - std::max(-50, hero_res(h, e)) / 100.0);
  add_fx({{"k", "burst"}, {"x", h.x}, {"y", h.y}, {"r", 1}, {"col", COL[e]}});
  std::string what = "☼ It strikes. Your " + stacks_word(k, e) + " go off at once";
  int d = std::max(1, rnd_i(b * (e == ROT ? 1.25 : e == ASH ? .4 : e == BLOOD ? .6 : 1.0)));
  d = std::min(d, stats(h).maxhp * 2 / 5);  // never more than two fifths of a hero's life in one go
  if (h.perk("grounded")) d = std::max(1, d * 3 / 4);
  if (e == ASH) { if (!h.perk("clearlungs")) h.smother = std::max(h.smother, 1); h.say(what + ": " + std::to_string(d) + " harm, and the soot chokes your skills.", COL[e]); }
  else if (e == BLOOD) {
    m.hp = std::min(m.maxhp, m.hp + d * (100 - heal_cut(m.stk, m.rank == "boss")) / 100);
    h.say(what + ": " + std::to_string(d) + " harm, and " + the(m) + " drinks it.", COL[e]);
  } else h.say(what + ": " + std::to_string(d) + " harm.", COL[e]);
  hero_hurt(h, &m, d, "elem");
  if (e == EMBER)  // the fire leaps to a friend beside you
    for (auto* o : active()) if (o != &h && cheb(o->x, o->y, h.x, h.y) <= 1) hero_hurt(*o, &m, std::max(1, d / 2), "elem");
}

// ------------------------------------------------------------------ each turn
void Game::stacks_tick() {
  auto near = [&](int x, int y, bool (*fn)(int)) {
    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) if (fn(floor->tile(x + dx, y + dy))) return true;
    return floor->burning(x, y) && fn == fiery;
  };
  // fading, and the ground: water douses Ember, fire thaws Rime and slows Rot, a draught in a doorway blows Ash away
  auto settle = [&](Stacks& s, int x, int y, Hero* h, Mon* m) {
    bool wet = watery(floor->tile(x, y)) || near(x, y, watery), warm = near(x, y, fiery), draught = floor->tile(x, y) == data::OPEN;
    auto wash = [&](int e, const char* says) {
      if (s.n[e] <= 0) return;
      s.n[e] = 0;
      if (h) h->say(says, COL[e]);
    };
    if (wet) wash(EMBER, "The water hisses: your Ember goes out.");
    if (warm) wash(RIME, "The fire's warmth thaws the Rime off you.");
    if (draught) wash(ASH, "The draught in the doorway blows the Ash away.");
    for (int e = 0; e < N; ++e) {
      if (s.n[e] <= 0) continue;
      int fade = e == ROT && warm ? 1 : FADE;
      if (m) {  // the perks of the hero who laid it
        Hero* by = stacks_by(*m, e);
        if (by && by->perk("slowfire")) continue;                                                                   // Slow Fire
        if (by && e == EMBER && by->perk("kindling")) fade += 2;                                                     // Kindling
        if (by && e == EMBER && by->perk("banked") && !by->dead && cheb(by->x, by->y, m->x, m->y) <= 1) continue;    // Banked Coals
        if (by && e == ROT && by->perk("coldstorage") && m->frozen > 0) continue;                                    // Cold Storage
      }
      if (++s.idle[e] >= fade) { --s.n[e]; s.idle[e] = 0; }
    }
  };
  int per = scale_el();
  for (auto* h : active()) {
    if (h->smother > 0) --h->smother;
    if (h->dead) continue;
    if (h->perk("emberheart") && h->stk.n[EMBER] < 1) { h->stk.n[EMBER] = 1; h->stk.idle[EMBER] = 0; }  // Ember Heart: it always burns
    int burn = h->stk.n[EMBER] * per, bleed = h->stk.n[BLOOD] * std::max(1, per / 2);
    if (h->perk("coldblood")) burn *= 2;
    if (h->perk("thickblood")) bleed /= 2;
    if (burn > 0) hero_hurt(*h, nullptr, burn, "ember");
    if (bleed > 0 && !h->dead) hero_hurt(*h, nullptr, bleed, "blood");
    if (h->dead) continue;
    Stats st = stats(*h);
    int heal = 0;
    if (h->perk("emberheart") && h->stk.n[EMBER] > 0) heal += 2;
    if (h->perk("quenchborn") && watery(floor->tile(h->x, h->y))) heal += 2;
    if (h->perk("haemophage")) for (auto& m : floor->mons) if (m->hp > 0 && (m->stk.n[BLOOD] > 0 || m->bleed > 0) && h->sees(m->x, m->y)) ++heal;
    if (h->perk("deathwish")) heal /= 2;
    if (heal > 0 && h->hp < st.maxhp) h->hp = std::min(st.maxhp, h->hp + heal_amt(*h, heal));
    settle(h->stk, h->x, h->y, h, nullptr);
  }
  auto ms = floor->mons;
  for (auto& m : ms) {
    if (m->hp <= 0) continue;
    int burn = m->stk.n[EMBER] * per, bleed = m->stk.n[BLOOD] * std::max(1, per / 2);
    auto by = [&](int e) -> Hero* { auto it = heroes.find(m->stk.by[e]); return it == heroes.end() ? nullptr : it->second.get(); };
    if (Hero* b = by(EMBER); b && burn > 0) {
      if (b->perk("fever") && m->stk.n[ROT] > 0) burn = burn * 3 / 2;     // Fever
      if (b->perk("onecraft") && b->el == EMBER) burn = burn * 3 / 2;     // One Craft
    }
    if (Hero* b = by(BLOOD); b && b->perk("boilingblood") && m->stk.n[BLOOD] > 0) burn += per;  // Boiling Blood
    if (burn > 0) { num(m->x, m->y, std::to_string(burn), COL[EMBER]); m->hp -= burn; if (m->hp <= 0) { kill(*m, by(EMBER)); continue; } }
    if (bleed > 0) { num(m->x, m->y, std::to_string(bleed), COL[BLOOD]); m->hp -= bleed; if (m->hp <= 0) { kill(*m, by(BLOOD)); continue; } }
    settle(m->stk, m->x, m->y, nullptr, m.get());
  }
}

// The first coal: taken from the edge of Charnfen's great fire, it gives the hero an element.
void Game::take_coal(Hero& h, int e) {
  if (e < 0 || e >= N) return;
  if (h.el < 0) {
    h.el = e;
    refit(h);
    h.say(std::string("You take a coal from the great fire. It glows ") + NAME[e] + " in your hand, and its warmth runs into your arms.", COL[e]);
    tip(h, "coal", std::string("Your blows now lay ") + NAME[e] + " on your foes; your heavy skills (marked ☼) set it all off at once.");
    add_fx({{"k", "rise"}, {"x", h.x}, {"y", h.y}, {"col", COL[e]}});
  } else if (h.el2 < 0 && e != h.el && floor->camp && h.perk("onecraft")) {
    h.say("One craft only: your hands will not take a second coal.", COL[h.el]);
  } else if (h.el2 < 0 && e != h.el && floor->camp) {
    h.el2 = e;
    h.say(std::string("You take a second coal. ") + NAME[e] + " glows beside your " + NAME[h.el] + ".", COL[e]);
  }
}
