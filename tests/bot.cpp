// A plain bot dives with each starting weapon (and once as a party of three) to see that nothing breaks and how far it gets.
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <set>
#include <tuple>

#include "game.hpp"
#include "pyre.hpp"

using namespace data;

static json toward(Game& g, Hero& h, int tx, int ty) {
  Floor& f = *g.floor;
  std::vector<int> d(W * H, 1 << 30);
  std::deque<int> q{ty * W + tx};
  d[ty * W + tx] = 0;
  while (!q.empty()) {
    int i = q.front(); q.pop_front();
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        int nx = i % W + dx, ny = i / W + dy;
        if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
        int t = f.tile(nx, ny);
        if (blocks(t) && t != DOOR) continue;
        if (f.has_merchant && f.merchant == P{nx, ny}) continue;  // the trader stands in the way
        int j = ny * W + nx;
        if (d[j] > d[i] + 1) { d[j] = d[i] + 1; q.push_back(j); }
      }
  }
  int bd = d[h.y * W + h.x], bx = 0, by = 0;
  for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx) {
      int nx = h.x + dx, ny = h.y + dy;
      if ((!dx && !dy) || nx < 0 || nx >= W || ny < 0 || ny >= H || g.hero_at(nx, ny)) continue;
      if (d[ny * W + nx] < bd) { bd = d[ny * W + nx]; bx = dx; by = dy; }
    }
  return {{"t", "move"}, {"dx", bx}, {"dy", by}};
}
static bool still(const json& a) { return a["dx"] == 0 && a["dy"] == 0; }

static json skill(int n, const MonP& m = nullptr) {
  json a{{"t", "skill"}, {"n", n}};
  if (m) { a["x"] = m->x; a["y"] = m->y; }
  return a;
}

// what a careful player does about a blow being wound up: step off red, into gold
static json dodge(Game& g, Hero& h) {
  Floor& f = *g.floor;
  for (auto& m : f.mons) {
    if (m->blow.turns <= 0) continue;
    auto in = [&](int x, int y) { for (auto& c : m->blow.cells) if (c.x == x && c.y == y) return true; return false; };
    bool here = in(h.x, h.y);
    if (m->blow.kind == BLOW_RED && !here) continue;
    if (m->blow.kind == BLOW_GOLD && here) continue;
    if (m->blow.kind == BLOW_PURPLE) continue;
    int bx = 0, by = 0, best = 1 << 30;
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        int nx = h.x + dx, ny = h.y + dy;
        if ((!dx && !dy) || !f.walk(nx, ny) || g.mon_at(nx, ny) || g.hero_at(nx, ny)) continue;
        int cost = 0;
        if (m->blow.kind == BLOW_RED) cost = in(nx, ny) ? 100 : 0;
        else { cost = 1000; for (auto& c : m->blow.cells) cost = std::min(cost, cheb(nx, ny, c.x, c.y)); }
        if (cost < best) { best = cost; bx = dx; by = dy; }
      }
    if (bx || by) return {{"t", "move"}, {"dx", bx}, {"dy", by}};
  }
  return nullptr;
}

static int bot_elem = 0;  // BOTELEM=n, else one after another
static std::map<int, std::string> bot_weapon;  // the weapon each bot takes from the rack
static json choose(Game& g, Hero& h) {
  if (!h.equip.count("weapon") && g.depth == 0) return {{"t", "rack"}, {"w", bot_weapon[h.pid]}};
  if (h.el < 0) return {{"t", "coal"}, {"e", std::getenv("BOTELEM") ? std::atoi(std::getenv("BOTELEM")) : bot_elem++ % elem::N}};
  if (g.floor->rest && g.depth > 0) {  // at a waystation's fire: a path where one is due, and a second coal for keys [2]-[3]
    for (int s = 0; s < 3; ++s) if (h.srank[s] >= data::PATH_RANK && h.path[s] < 0) g.free_action(h, {{"t", "path"}, {"s", s}, {"c", (h.pid + s) % 2}});
    if (h.el2 < 0 && !std::getenv("BOTONECOAL")) {
      g.free_action(h, {{"t", "slotel"}, {"k", 2}});
      return {{"t", "coal"}, {"e", (h.el + 2) % elem::N}};
    }
    if (h.el2 >= 0 && !h.slot_el[1]) g.free_action(h, {{"t", "slotel"}, {"k", 2}});
  }
  if (!h.perk_choices.empty()) g.free_action(h, {{"t", "perk"}, {"k", h.perk_choices.front()[g.rnd.below(int(h.perk_choices.front().size()))]}});
  Stats st = g.stats(h);
  if (h.hp < st.maxhp * .4 && h.heal > 0) return {{"t", "quaff"}, {"k", "heal"}};
  auto score = [](const ItemP& i) { if (!i) return 0; int s = i->lo + i->hi + i->ar * 2; for (auto& [k, v] : i->stats) s += v / 3; return s; };
  for (auto& it : h.pack) {
    if (!g.can_use(h, *it)) continue;
    if (it->slot == "weapon" && it->kind != bot_weapon[h.pid]) continue;  // each bot keeps to its weapon
    auto cur = h.equip.find(it->slot);
    if (score(it) > score(cur == h.equip.end() ? nullptr : cur->second)) return {{"t", "equip"}, {"id", it->id}};
  }
  Floor& f = *g.floor;
  if (h.shopping) g.free_action(h, {{"t", "buyp"}, {"k", "heal"}});
  // the keys the skills sit on now: [1] the weapon's, [2]-[5] the styles'
  auto slot = [&](const char* key) { for (size_t i = 0; i < h.cls->skills.size(); ++i) if (h.cls->skills[i] == key) return int(i) + 1; return 0; };
  // Shadowstep answers a blow by stepping behind the one who swings it
  if (int n = slot("shadowstep"); n && h.fury >= 3)
    for (auto& m : f.mons)
      if (m->blow.turns > 0 && m->blow.kind == BLOW_RED && cheb(m->x, m->y, h.x, h.y) <= 4)
        for (auto& c : m->blow.cells) if (c.x == h.x && c.y == h.y) return skill(n, m);
  if (auto d = dodge(g, h); !d.is_null()) return d;
  MonP seen;
  int visible = 0;
  for (auto& m : f.mons)
    if ((h.sees(m->x, m->y) || cheb(m->x, m->y, h.x, h.y) <= 1) && !(m->dormant || m->hidden)) {  // statues and gold wait
      ++visible;
      if (!seen || cheb(h.x, h.y, m->x, m->y) < cheb(h.x, h.y, seen->x, seen->y)) seen = m;
    }
  if (seen && std::getenv("BOTDBG") && g.turn > 19990) std::printf("seen %s at %d,%d tile %d dormant %d hidden %d hp %d sees %d\n", seen->name.c_str(), seen->x, seen->y, f.tile(seen->x, seen->y), seen->dormant, seen->hidden, seen->hp, int(h.sees(seen->x, seen->y)));
  if (seen) {
    int d = cheb(h.x, h.y, seen->x, seen->y);
    int near = 0, within2 = 0;
    for (auto& m : f.mons) { if (cheb(h.x, h.y, m->x, m->y) <= 1) ++near; if (cheb(h.x, h.y, m->x, m->y) <= 2 && h.sees(m->x, m->y)) ++within2; }
    MonP purple;
    for (auto& m : f.mons) if (m->blow.turns > 0 && m->blow.kind == BLOW_PURPLE && h.sees(m->x, m->y)) purple = m;
    MonP adj;
    for (auto& m : f.mons) if (cheb(h.x, h.y, m->x, m->y) == 1 && (!adj || g.busy(*m, h))) adj = m;
    const std::string& k = h.cls->key;
    if (std::getenv("BOTWHY")) std::printf("   why: k=%s d=%d near=%d seen=%s\n", k.c_str(), d, near, seen->name.c_str());
    int M = h.mana, F = h.fury;
    bool caster = g.main_style(h) != 0;
    if (caster && M < 5 && h.mana_pots > 0) return {{"t", "quaff"}, {"k", "mana"}};
    // the style skills, wherever they sit
    if (int n = slot("warcry"); n && F >= 3 && ((purple && cheb(h.x, h.y, purple->x, purple->y) <= 2) || near >= 3)) return skill(n);
    if (int n = slot("standfast"); n && F >= 4 && h.standfast == 0 && h.hp < st.maxhp * .6 && near >= 1) return skill(n);
    if (int n = slot("whirlwind"); n && F >= 5 && within2 >= 2) return skill(n);
    if (int n = slot("ward"); n && purple && M >= 6 && cheb(h.x, h.y, purple->x, purple->y) <= 7) return skill(n, purple);
    if (int n = slot("smoke"); n && M >= 7 && near >= 2 && h.hp < st.maxhp * .4) return skill(n);
    if (int n = slot("volley"); n && M >= 7 && visible >= 3 && st.reach >= 5) return skill(n);
    if (int n = slot("tumble"); n && M >= 4 && d <= 1 && st.reach >= 5) return skill(n);
    if (int n = slot("mend"); n && M >= 6 && h.hp < st.maxhp * .5) return skill(n);
    if (int n = slot("nova"); n && M >= 10 && d <= 2 && (near >= 1 || purple)) return skill(n);
    if (int n = slot("blink"); n && M >= 5 && d <= 1 && h.hp < st.maxhp * .5) return skill(n);
    if (int n = slot("hallow"); n && M >= 7 && near >= 2) return skill(n);
    if (int n = slot("emberwall"); n && M >= 8 && d >= 3 && seen->rank != "normal") return skill(n, seen);
    // the weapon's own skill on [1], and what [f] does with it
    if (k == "dagger") {
      if (adj && g.busy(*adj, h)) return {{"t", "move"}, {"dx", adj->x - h.x}, {"dy", adj->y - h.y}};
      if (adj && F >= 5) return skill(1);
    } else if (k == "sword") {
      if (F >= 2 && d >= 2 && d <= 5) return skill(1, seen);
    } else if (k == "axe") {
      if (near >= 2 && F >= 2) return skill(1);
    } else if (k == "mace") {
      if (adj && F >= 3) return skill(1);
    } else if (k == "bow") {
      if (M >= 5 && d > 1 && seen->rank != "normal") return skill(1, seen);
      if (d > 1 && d <= 8) return {{"t", "shoot"}, {"x", seen->x}, {"y", seen->y}};
    } else if (k == "crossbow") {
      if (M >= 6 && d > 1 && d <= 8 && (purple || seen->rank != "normal")) return skill(1, purple ? purple : seen);
      if (d > 1 && d <= 8) return {{"t", "shoot"}, {"x", seen->x}, {"y", seen->y}};
    } else if (k == "thrown") {
      if (within2 >= 2 && M >= 5) return skill(1);
      if (d > 1 && d <= 5) return {{"t", "shoot"}, {"x", seen->x}, {"y", seen->y}};
    } else if (k == "spear") {
      if (M >= 4 && d >= 3 && d <= 6) return skill(1, seen);
      if (d == 2) return {{"t", "shoot"}, {"x", seen->x}, {"y", seen->y}};
    } else if (k == "staff") {
      if (M >= 4 && d <= 9) return skill(1, seen);
      if (d > 1 && d <= 6) return {{"t", "shoot"}, {"x", seen->x}, {"y", seen->y}};
    } else if (k == "wand") {
      if (M >= 5 && visible >= 2) return skill(1);
      if (d > 1 && d <= 7) return {{"t", "shoot"}, {"x", seen->x}, {"y", seen->y}};
    } else if (k == "lantern") {
      if (M >= 9 && (near >= 2 || (purple && cheb(h.x, h.y, purple->x, purple->y) <= 4))) return skill(1);
    } else if (k == "chisel") {
      int mine = 0; bool on = false, foe_on = false;
      for (auto& r : f.runes) if (r.owner == h.pid) { ++mine; if (r.p == P{h.x, h.y}) on = true; for (auto& m : f.mons) if (cheb(m->x, m->y, r.p.x, r.p.y) <= 1) foe_on = true; }
      if (foe_on && M >= 4) return skill(1);
      if (!on && d <= 6 && d > 1) return {{"t", "shoot"}, {"x", h.x}, {"y", h.y}};
      static std::map<int, int> waited;  // a foe that does not come is gone after, runes or not
      if (d >= 2 && on && ++waited[seen->id] < 16) {  // step back and let them come over it
        int bx = 0, by = 0, bd = d;
        for (int dy = -1; dy <= 1; ++dy)
          for (int dx = -1; dx <= 1; ++dx) {
            int nx = h.x + dx, ny = h.y + dy;
            if ((dx || dy) && f.walk(nx, ny) && !g.mon_at(nx, ny) && !g.hero_at(nx, ny) && cheb(nx, ny, seen->x, seen->y) > bd) { bd = cheb(nx, ny, seen->x, seen->y); bx = dx; by = dy; }
          }
        if (bx || by) return {{"t", "move"}, {"dx", bx}, {"dy", by}};
      }
    }
    if (d == 1) return {{"t", "move"}, {"dx", seen->x - h.x}, {"dy", seen->y - h.y}};
    auto a = toward(g, h, seen->x, seen->y);
    if (!still(a)) return a;
  }
  if (way_down(f.tile(h.x, h.y))) return {{"t", "down"}};
  const FloorItem* loot = nullptr;
  // things it stands on and could not take (a full belt or pack): leave them be
  static std::set<std::tuple<int, int, int>> left;
  for (auto& fi : f.items) if (fi.x == h.x && fi.y == h.y) left.insert({f.id, fi.x, fi.y});
  for (auto& fi : f.items)
    if (!left.count({f.id, fi.x, fi.y}) && (!fi.owner || fi.owner == h.pid) && cheb(h.x, h.y, fi.x, fi.y) < 14 && (!loot || cheb(h.x, h.y, fi.x, fi.y) < cheb(h.x, h.y, loot->x, loot->y))) loot = &fi;
  if (std::getenv("BOTDBG2")) std::printf("seen=%d loot=%d,%d down=%d,%d tile=%d\n", seen?1:0, loot?loot->x:-1, loot?loot->y:-1, f.down.x, f.down.y, f.tile(f.down.x,f.down.y));
  // a thing it has walked toward for long without getting there lies the long way round (the Deep's maze): let it be
  static std::map<std::tuple<int, int, int>, int> chased;
  if (loot && ++chased[{f.id, loot->x, loot->y}] > 40) { left.insert({f.id, loot->x, loot->y}); loot = nullptr; }
  if (loot) { auto a = toward(g, h, loot->x, loot->y); if (!still(a)) return a; }
  P down = f.down;
  if (f.exits.size() > 1) {  // a waystation with two ways on: the dives spread over the routes
    auto it = f.exits.begin();
    std::advance(it, (g.depth + int(g.heroes.begin()->second->cls->key.size())) % 2);
    down = {it->first % W, it->first / W};
  }
  auto a = toward(g, h, down.x, down.y);
  if (still(a)) return {{"t", "move"}, {"dx", g.rnd.range(-1, 1)}, {"dy", g.rnd.range(-1, 1)}};
  return a;
}

// Returns the first hero as kept for the next Frost level when the dive is won, else null.
static json run(const std::vector<std::string>& classes, uint64_t seed, int frost = -1, const json& kept = json()) {
  // BOTFROST=n plays on that Frost level; BOTTALENTS=all gives every bot the whole Pyre tree (or "kindled:3,glint:1")
  Game g(seed, frost >= 0 ? frost : std::getenv("BOTFROST") ? std::atoi(std::getenv("BOTFROST")) : 0);
  json talents = json::object();
  if (const char* t = std::getenv("BOTTALENTS")) {
    std::string s = t;
    if (s == "all") for (auto& d : pyre::TALENTS) talents[d.key] = d.ranks;
    else
      for (size_t i = 0; i < s.size();) {
        size_t j = s.find(',', i);
        if (j == std::string::npos) j = s.size();
        auto part = s.substr(i, j - i);
        auto c = part.find(':');
        if (c != std::string::npos) talents[part.substr(0, c)] = std::atoi(part.c_str() + c + 1);
        i = j + 1;
      }
  }
  for (size_t i = 0; i < classes.size(); ++i) { bot_weapon[int(i) + 1] = classes[i]; g.join(int(i) + 1, "Bot" + std::to_string(i), classes[i], talents, i == 0 ? kept : json()); }
  int steps = 0, last_depth = -1;
  std::string pace;  // the first hero's level as each new depth is reached (BOTPACE=1 prints it)
  while (g.state == "play" && steps < 20000) {
    if (g.depth != last_depth) { last_depth = g.depth; pace += std::to_string(g.heroes.begin()->second->lvl) + (g.floor->rest ? "r " : " "); }
    std::map<int, json> acts;
    for (auto& [id, h] : g.heroes) if (h->present()) acts[id] = choose(g, *h);
    if (std::getenv("BOTACT")) for (auto& [id, a] : acts) { auto& h = *g.heroes[id]; std::printf("%5d %d,%d hp%d mp%d %s\n", g.turn + 1, h.x, h.y, h.hp, h.mana, a.dump().c_str()); }
    if (std::getenv("BOTDBG") && steps == 19995) for (auto& [id, h] : g.heroes) {
      for (int y = h->y - 6; y <= h->y + 6; ++y) { for (int x = h->x - 10; x <= h->x + 10; ++x) std::printf("%c", x == h->x && y == h->y ? '@' : char(TILE0 + g.floor->tile(x, y))); std::printf("\n"); }
      std::printf("up %d,%d down %d,%d whole %d\n", g.floor->up.x, g.floor->up.y, g.floor->down.x, g.floor->down.y, int(whole(*g.floor)));
    }
    if (std::getenv("BOTDBG") && steps > 19990) for (auto& [id, a] : acts) { auto& h = *g.heroes[id]; int tx = h.x + a.value("dx", 0), ty = h.y + a.value("dy", 0); std::printf("%d,%d hp%d mana%d %s tile %d mon %d\n", h.x, h.y, h.hp, h.mana, a.dump().c_str(), g.floor->tile(tx, ty), g.mon_at(tx, ty) ? 1 : 0); }
    g.step(acts);
    if (std::getenv("BOTLOG")) for (auto& [id, h] : g.heroes) { for (auto& l : h->log) std::printf("%5d hp%3d mp%2d %s\n", g.turn, h->hp, h->mana, l.text.c_str()); h->log.clear(); }
    if (std::getenv("BOTDBG") && steps > 19990) for (auto& [id, h] : g.heroes) for (auto& l : h->log) std::printf("  %s\n", l.text.c_str());
    if (!std::getenv("BOTDBG")) for (auto& [id, h] : g.heroes) { view::snapshot(g, *h, steps % 50 == 0); }
    ++steps;
  }
  std::string names, lvls, gear;
  for (auto& c : classes) names += (names.empty() ? "" : "+") + c;
  int kills = 0;
  for (auto& [id, h] : g.heroes) {
    lvls += std::to_string(h->lvl) + "(" + std::to_string(h->srank[0]) + "/" + std::to_string(h->srank[1]) + "/" + std::to_string(h->srank[2]) +
            " w" + std::to_string(g.mastery(*h, h->cls->key)) + " e" + std::to_string(h->el >= 0 ? h->erank[h->el] : 0) + ") ";
    kills += h->kills;
  }
  for (auto& [s, it] : g.heroes.begin()->second->equip) gear += it->shortname() + ", ";
  std::string way;
  for (int z : g.route) way += (way.empty() ? "" : ">") + ACTS[z].key.substr(0, 4);
  std::string els;
  for (auto& [id, h] : g.heroes) els += (els.empty() ? "" : "+") + std::string(h->el >= 0 ? elem::NAME[h->el] : "-");
  names += "/" + els;
  std::printf("%-22s seed %2llu: %-4s depth %2d%s turn %5d lvl %s kills %3d gold %4d | %s | %s\n", names.c_str(), (unsigned long long)seed,
              g.state.c_str(), g.depth, g.floor->rest ? " (rest)" : "", g.turn, lvls.c_str(), kills, g.heroes.begin()->second->gold, way.c_str(),
              gear.c_str());
  if (std::getenv("BOTPACE")) std::printf("   pace (level at each depth): %s\n", pace.c_str());
  return g.state == "won" ? g.record(*g.heroes.begin()->second) : json();
}

// The hero kept between dives comes back whole: what it grew, what it carries, where its keys and coals are.
static int kept_test() {
  Game a(3, 0);
  Hero& h = a.join(1, "Kept", "", json::object());
  h.equip["weapon"] = loot::make("staff", "magic", 2);
  h.pack.push_back(loot::roll(5, a.rnd, {"bow"}));
  h.lvl = 7; h.xp = 1234; h.gold = 99; h.srank[2] = 6; h.sxp[2] = 400; h.el = 2; h.el2 = 4; h.erank[2] = 5; h.wxp["staff"] = 300;
  h.known = {"nova", "blink", "mend"}; h.path[2] = 1; h.slot_el[2] = 1; h.perks["tough"] = 2;
  a.refit(h);
  json rec = json::parse(a.record(h).dump());
  Game b(4, 1);
  Hero& k = b.join(1, "Kept", "", json::object(), rec);
  bool ok = k.lvl == 7 && k.xp == 1234 && k.gold == 99 && k.srank[2] == 6 && k.sxp[2] == 400 && k.el == 2 && k.el2 == 4 &&
            k.erank[2] == 5 && k.wxp["staff"] == 300 && k.known == h.known && k.path[2] == 1 && k.slot_el[2] == 1 && k.perk("tough") == 2 &&
            k.equip.count("weapon") && k.equip["weapon"]->name == h.equip["weapon"]->name && k.equip["weapon"]->stats == h.equip["weapon"]->stats &&
            k.pack.size() == 1 && k.pack[0]->name == h.pack[0]->name && k.cls->name == h.cls->name && b.frost == 1;
  std::printf("kept hero: %s (%s, level %d, %s)\n", ok ? "comes back whole" : "BROKEN", k.cls->name.c_str(), k.lvl, k.equip.count("weapon") ? k.equip["weapon"]->name.c_str() : "-");
  return ok ? 0 : 1;
}

int main() {
  if (kept_test()) return 1;
  // BOTLADDER=n: the first weapon's hero goes on down the Frost ladder, n dives at most, kept between them
  if (const char* l = std::getenv("BOTLADDER")) {
    std::string w = std::getenv("BOTWEAPON") ? std::getenv("BOTWEAPON") : "staff";
    json kept;
    for (int f = 0; f < std::atoi(l); ++f) {
      kept = run({w}, 100 + f, f, kept);
      if (kept.is_null()) break;
    }
    return 0;
  }
  const char* only = std::getenv("BOTWEAPON") ? std::getenv("BOTWEAPON") : std::getenv("BOTCLASS");
  int seeds = std::getenv("BOTSEEDS") ? std::atoi(std::getenv("BOTSEEDS")) : 2;
  for (auto& w : WEAPONS)
    if (!only || w.key == only)
      for (uint64_t s = 1; s <= uint64_t(seeds); ++s) run({w.key}, s);
  if (!only) run({"sword", "lantern", "dagger"}, 9);
  return 0;
}
