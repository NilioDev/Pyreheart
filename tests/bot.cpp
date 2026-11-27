// A plain bot dives with each starting weapon (and once as a party of three) to see that nothing breaks and how far it gets.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <deque>
#include <fstream>
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
static int bot_force_elem = -1;  // the matrix (BOTMATRIX) sets each element in turn
static int dive_depth = 0;       // how deep the last dive came
static bool dive_stalled = false;  // the last dive hit the step cap still playing
// which way the bot takes at a fork: by its weapon name, or BOTROUTE=n to send every weapon the same way
static int route_bias(const Game& g) { return std::getenv("BOTROUTE") ? std::atoi(std::getenv("BOTROUTE")) : int(g.heroes.begin()->second->cls->key.size()); }
static std::map<int, std::string> bot_weapon;  // the weapon each bot takes from the rack
// BOTSKILL: how well the bot plays. 0 a beginner (drinks late, often forgets to dodge, judges gear by its numbers
// alone, uses its style skills only now and then), 1 the ordinary bot, 2 a good player (drinks in time, steps out of
// a crowd, picks perks for its element and style, has Brann temper its gear)
static int bot_skill() { static int s = std::getenv("BOTSKILL") ? std::atoi(std::getenv("BOTSKILL")) : 1; return s; }
// an advanced habit, unless BOTNO=perk,trade,temper,kite,douse turns it off (to see what each
// is worth; tried and dropped: drinking at half life, stepping out of a crowd, holding a narrow place, the weakest foe first,
// sparing potions for the boss)
static bool adv(const char* habit) { const char* no = std::getenv("BOTNO"); return bot_skill() >= 2 && !(no && std::strstr(no, habit)); }
static json choose(Game& g, Hero& h) {
  if (!h.equip.count("weapon") && g.depth == 0) return {{"t", "rack"}, {"w", bot_weapon[h.pid]}};
  if (h.el < 0) return {{"t", "coal"}, {"e", bot_force_elem >= 0 ? bot_force_elem : std::getenv("BOTELEM") ? std::atoi(std::getenv("BOTELEM")) : bot_elem++ % elem::N}};
  if (g.floor->camp) {  // at a camp fire: a path where one is due, and a second coal for keys [2]-[3]
    for (int s = 0; s < 3; ++s) if (h.srank[s] >= data::PATH_RANK && h.path[s] < 0) g.free_action(h, {{"t", "path"}, {"s", s}, {"c", (h.pid + s) % 2}});
    if (h.el2 < 0 && !std::getenv("BOTONECOAL") && !h.perk("onecraft")) {
      g.free_action(h, {{"t", "slotel"}, {"k", 2}});
      return {{"t", "coal"}, {"e", (h.el + 2) % elem::N}};
    }
    if (h.el2 >= 0 && !h.slot_el[1]) g.free_action(h, {{"t", "slotel"}, {"k", 2}});
  }
  if (!h.perk_choices.empty()) {
    const auto& ch = h.perk_choices.front();
    std::string pick = ch[g.rnd.below(int(ch.size()))];
    if (adv("perk")) {  // a good player takes what fits: its element first, then its style
      static const char* STYLE_WORDS[3][3] = {{"melee", "blow", "fury"}, {"shot", "arrow", "ranged"}, {"spell", "mana", "magic"}};
      int best = -1;
      for (auto& k : ch) {
        const std::string& t = perk_def(k).text;
        int sc = int(g.rnd.below(2));
        if (h.el >= 0 && t.find(elem::NAME[h.el]) != std::string::npos) sc += 4;
        for (auto* w : STYLE_WORDS[g.main_style(h)]) if (t.find(w) != std::string::npos) sc += 2;
        if (sc > best) { best = sc; pick = k; }
      }
    }
    g.free_action(h, {{"t", "perk"}, {"k", pick}});
  }
  Stats st = g.stats(h);
  double drink_at = bot_skill() == 0 ? .25 : .4;  // drinking at half life all the way only empties the belt sooner (fewer wins)
  if (h.hp < st.maxhp * drink_at && h.heal > 0 && (bot_skill() > 0 || g.rnd.below(3))) return {{"t", "quaff"}, {"k", "heal"}};
  // a beginner judges a thing by its damage and armour alone
  auto score = [](const ItemP& i) { if (!i) return 0; int s = i->lo + i->hi + i->ar * 2; if (bot_skill() > 0) for (auto& [k, v] : i->stats) s += v / 3; return s; };
  for (auto& it : h.pack) {
    if (!g.can_use(h, *it)) continue;
    if (it->slot == "weapon" && it->kind != bot_weapon[h.pid]) continue;  // each bot keeps to its weapon
    auto cur = h.equip.find(it->slot);
    if (score(it) > score(cur == h.equip.end() ? nullptr : cur->second)) return {{"t", "equip"}, {"id", it->id}};
  }
  Floor& f = *g.floor;
  // co-op: take up a fallen friend's ember, and bear it to the waystation's fire
  if (!f.embers.empty()) {
    auto [at, pid] = f.embers.front();
    auto a = toward(g, h, at.x, at.y);
    if (!still(a)) return a;
  }
  if (!h.carry.empty() && f.camp && f.fire.x >= 0 && cheb(h.x, h.y, f.fire.x, f.fire.y) > 1)
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx)
        if ((dx || dy) && f.walk(f.fire.x + dx, f.fire.y + dy)) {
          auto a = toward(g, h, f.fire.x + dx, f.fire.y + dy);
          if (!still(a)) return a;
        }
  if (f.camp) {  // Ysolde: healed once a camp, and the belt filled while the gold lasts
    static std::set<std::pair<int, int>> visited;
    if (visited.insert({f.id, h.pid}).second) {
      h.talking = "healer";
      g.free_action(h, {{"t", "talk"}, {"k", "heal"}});
      for (int i = 0; i < 6; ++i) g.free_action(h, {{"t", "talk"}, {"k", "potion"}, {"p", "heal"}});
      h.talking.clear();
      if (adv("trade")) {  // a good player sells what it will never wear and buys what beats what it wears
        auto worth = [](const ItemP& i) { if (!i) return 0; int s = i->lo + i->hi + i->ar * 2; for (auto& [k, v] : i->stats) s += v / 3; return s; };
        auto worn = [&](const std::string& sl) { auto e = h.equip.find(sl); return e == h.equip.end() ? 0 : worth(e->second); };
        h.shopping = true;
        std::vector<int> junk;
        for (auto& it : h.pack)
          if (!g.can_use(h, *it) || (it->slot == "weapon" && it->kind != bot_weapon[h.pid]) || worth(it) <= worn(it->slot)) junk.push_back(it->id);
        for (int id : junk) g.free_action(h, {{"t", "sell"}, {"id", id}});
        for (bool bought = true; bought;) {
          bought = false;
          for (int i = 0; i < int(f.shop.size()) && !bought; ++i) {
            auto& it = f.shop[i];
            if (!g.can_use(h, *it) || (it->slot == "weapon" && it->kind != bot_weapon[h.pid]) || worth(it) <= worn(it->slot)) continue;
            if (h.gold < g.price(h, *it)) continue;
            size_t n = f.shop.size();
            g.free_action(h, {{"t", "buy"}, {"i", i}});
            bought = f.shop.size() < n;  // a full pack buys nothing
            if (!bought) break;
          }
        }
        h.shopping = false;
      }
      if (adv("temper")) {  // a good player has Brann temper the weapon and the body armour, as far as the gold goes
        h.talking = "smith";
        for (const char* sl : {"weapon", "body", "weapon", "body"})
          if (auto it = h.equip.find(sl); it != h.equip.end()) g.free_action(h, {{"t", "talk"}, {"k", "temper"}, {"id", it->second->id}});
        h.talking.clear();
      }
    }
    if (f.exits.empty()) {  // old Ebb opens the way on: the dives spread over the routes
      const auto& next = data::ACTS[g.route.back()].next;
      if (!next.empty()) {
        h.talking = "guide";
        g.free_action(h, {{"t", "talk"}, {"k", "way"}, {"z", next[(g.depth + route_bias(g)) % next.size()]}});
        h.talking.clear();
      }
    }
  }
  // the keys the skills sit on now: [1] the weapon's, [2]-[5] the styles'
  auto slot = [&](const char* key) { for (size_t i = 0; i < h.cls->skills.size(); ++i) if (h.cls->skills[i] == key) return int(i) + 1; return 0; };
  // Shadowstep answers a blow by stepping behind the one who swings it
  if (int n = slot("shadowstep"); n && h.fury >= 3)
    for (auto& m : f.mons)
      if (m->blow.turns > 0 && m->blow.kind == BLOW_RED && cheb(m->x, m->y, h.x, h.y) <= 4)
        for (auto& c : m->blow.cells) if (c.x == h.x && c.y == h.y) return skill(n, m);
  if (bot_skill() > 0 || g.rnd.below(2)) if (auto d = dodge(g, h); !d.is_null()) return d;  // a beginner often stands in it
  // a good player puts out what burns on it: Ember in water, Ash in a doorway's draught, if one is a few steps off
  if (adv("douse") && (h.stk.n[elem::EMBER] >= 2 || h.stk.n[elem::ASH] >= 3)) {
    bool ember = h.stk.n[elem::EMBER] >= 2;
    auto washes = [&](int x, int y) {
      if (!ember) return f.tile(x, y) == data::OPEN;
      for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) if (elem::watery(f.tile(x + dx, y + dy))) return true;
      return false;
    };
    std::vector<int> dist(W * H, -1);
    std::deque<int> q{h.y * W + h.x};
    dist[q[0]] = 0;
    int goal = -1;
    while (!q.empty() && goal < 0) {
      int i = q.front(); q.pop_front();
      if (washes(i % W, i / W)) { goal = i; break; }
      if (dist[i] >= 6) continue;
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
          int nx = i % W + dx, ny = i / W + dy;
          if (nx < 0 || ny < 0 || nx >= W || ny >= H || dist[ny * W + nx] >= 0 || !f.walk(nx, ny) || g.mon_at(nx, ny) || f.burning(nx, ny)) continue;
          dist[ny * W + nx] = dist[i] + 1;
          q.push_back(ny * W + nx);
        }
    }
    if (goal >= 0 && goal != h.y * W + h.x) if (auto a = toward(g, h, goal % W, goal / W); !still(a)) return a;
  }
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
    auto open_round = [&](int x, int y) { int n = 0; for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) if ((dx || dy) && f.walk(x + dx, y + dy)) ++n; return n; };
    // a good shooter steps back from a slow foe on it, to shoot again
    if (adv("kite") && d == 1 && g.main_style(h) != 0 && near == 1 && seen->spd * (100 - g.rime_slow(*seen)) / 100 < 100) {
      int bx = 0, by = 0;
      for (int dy = -1; dy <= 1 && !(bx || by); ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
          int nx = h.x + dx, ny = h.y + dy;
          if ((!dx && !dy) || !f.walk(nx, ny) || g.mon_at(nx, ny) || g.hero_at(nx, ny) || f.burning(nx, ny)) continue;
          bool clear = true;
          for (auto& m : f.mons) if (m->hp > 0 && cheb(nx, ny, m->x, m->y) <= 1) clear = false;
          if (clear && open_round(nx, ny) >= 2) { bx = dx; by = dy; break; }
        }
      if (bx || by) return {{"t", "move"}, {"dx", bx}, {"dy", by}};
    }
    // a beginner forgets its style skills half the time
    if (bot_skill() == 0 && g.rnd.below(2)) {
      if (d == 1) return {{"t", "move"}, {"dx", seen->x - h.x}, {"dy", seen->y - h.y}};
      if (auto a = toward(g, h, seen->x, seen->y); !still(a)) return a;
    }
    // the style skills, wherever they sit
    int reeling = 0;  // a roar on foes that already reel only loses the turn (Bloodprice makes it free, so it never ends)
    for (auto& m : f.mons) if (cheb(h.x, h.y, m->x, m->y) <= 2 && m->stun > 0) ++reeling;
    if (int n = slot("warcry"); n && F >= 3 && reeling == 0 && ((purple && cheb(h.x, h.y, purple->x, purple->y) <= 2) || near >= 3)) return skill(n);
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
      if (M >= 5 && visible >= 2 && d <= 7) return skill(1);  // Scatter only reaches 7
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
    std::advance(it, (g.depth + route_bias(g)) % 2);
    down = {it->first % W, it->first / W};
  }
  auto a = toward(g, h, down.x, down.y);
  if (still(a)) return {{"t", "move"}, {"dx", g.rnd.range(-1, 1)}, {"dy", g.rnd.range(-1, 1)}};
  return a;
}

// BOTHARM=1: what hurt each hero in its last 25 turns before it fell, read from its log, summed over all dives at the end.
static std::map<int, std::deque<std::map<std::string, int>>> harm_win;
static std::map<std::string, long> harm_total;
static std::map<int, bool> harm_dead;
static int harm_deaths = 0, harm_boss_deaths = 0, harm_empty_belt = 0;
static void harm_step(Game& g) {
  for (auto& [id, h] : g.heroes) {
    auto& win = harm_win[id];
    win.emplace_back();
    if (win.size() > 25) win.pop_front();
    bool blast = false;
    for (auto& l : h->log) {
      const std::string& t = l.text;
      if (t.rfind("☼ It strikes.", 0) == 0) { blast = true; continue; }
      auto p = t.find(" hurts you (");
      if (p == std::string::npos) continue;
      int n = std::atoi(t.c_str() + p + 12);
      std::string who = t.substr(0, p), k = blast ? "element blast" : who == "Ember" ? "Ember burn" : who == "Poison" ? "poison" :
                                                   who == "Bleeding" ? "bleeding" : who == "Fire" ? "fire ground" : "blows";
      blast = false;
      win.back()[k] += n;
    }
    h->log.clear();
    if (h->dead && !harm_dead[id]) {
      harm_dead[id] = true;
      ++harm_deaths;
      if (g.floor->boss && g.floor->boss->hp > 0) ++harm_boss_deaths;
      if (h->heal == 0) ++harm_empty_belt;
      std::map<std::string, int> sum;
      for (auto& turn : win) for (auto& [k, n] : turn) { sum[k] += n; harm_total[k] += n; }
      std::string line;
      for (auto& [k, n] : sum) line += k + " " + std::to_string(n) + ", ";
      std::printf("   fell at depth %d%s to %s: %s\n", g.depth, g.floor->boss && g.floor->boss->hp > 0 ? " (boss)" : "", h->killer.c_str(), line.c_str());
    }
  }
}
static void harm_report() {
  if (!std::getenv("BOTHARM") || !harm_deaths) return;
  long all = 0;
  for (auto& [k, n] : harm_total) all += n;
  std::printf("\nHARM in the last 25 turns of %d deaths (%d with a boss alive, %d with an empty belt):\n", harm_deaths, harm_boss_deaths, harm_empty_belt);
  for (auto& [k, n] : harm_total) std::printf("HARM %-14s %3ld%%\n", k.c_str(), all ? 100 * n / all : 0);
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
  harm_win.clear(); harm_dead.clear();
  int steps = 0, last_depth = -1;
  std::string pace;  // the first hero's level as each new depth is reached (BOTPACE=1 prints it)
  while (g.state == "play" && steps < 20000) {
    if (g.depth != last_depth) { last_depth = g.depth; pace += std::to_string(g.heroes.begin()->second->lvl) + (g.floor->camp ? "c " : " "); }
    std::map<int, json> acts;
    for (auto& [id, h] : g.heroes) if (h->present()) acts[id] = choose(g, *h);
    if (std::getenv("BOTACT")) for (auto& [id, a] : acts) { auto& h = *g.heroes[id]; std::printf("%5d %d,%d hp%d mp%d %s\n", g.turn + 1, h.x, h.y, h.hp, h.mana, a.dump().c_str()); }
    if (std::getenv("BOTDBG") && steps == 19995) for (auto& [id, h] : g.heroes) {
      for (int y = h->y - 6; y <= h->y + 6; ++y) { for (int x = h->x - 10; x <= h->x + 10; ++x) std::printf("%c", x == h->x && y == h->y ? '@' : char(TILE0 + g.floor->tile(x, y))); std::printf("\n"); }
      std::printf("up %d,%d down %d,%d whole %d\n", g.floor->up.x, g.floor->up.y, g.floor->down.x, g.floor->down.y, int(whole(*g.floor)));
    }
    if (std::getenv("BOTDBG") && steps > 19990) for (auto& [id, a] : acts) { auto& h = *g.heroes[id]; int tx = h.x + a.value("dx", 0), ty = h.y + a.value("dy", 0); std::printf("%d,%d hp%d mana%d %s tile %d mon %d\n", h.x, h.y, h.hp, h.mana, a.dump().c_str(), g.floor->tile(tx, ty), g.mon_at(tx, ty) ? 1 : 0); }
    g.step(acts);
    if (std::getenv("BOTHARM") && !std::getenv("BOTLOG")) harm_step(g);
    if (std::getenv("BOTLOG")) for (auto& [id, h] : g.heroes) { for (auto& l : h->log) std::printf("%5d hp%3d mp%2d %s\n", g.turn, h->hp, h->mana, l.text.c_str()); h->log.clear(); }
    if (std::getenv("BOTDBG") && steps > 19990) for (auto& [id, h] : g.heroes) for (auto& l : h->log) std::printf("  %s\n", l.text.c_str());
    if (std::getenv("BOTMAP") && steps == 19999) { auto& f = *g.floor; auto& h = *g.heroes.begin()->second; for (int y = 0; y < H; ++y) { std::string r; for (int x = 0; x < W; ++x) { int t = f.tile(x, y); r += (x == h.x && y == h.y) ? '@' : (x == f.down.x && y == f.down.y) ? '>' : g.mon_at(x, y) ? 'M' : f.walk(x, y) ? '.' : '#'; } std::printf("%s\n", r.c_str()); } std::printf("exits %zu camp_at %d,%d\n", f.exits.size(), f.camp_at.x, f.camp_at.y); if (f.boss) std::printf("boss %s at %d,%d hp %d dormant %d hidden %d\n", f.boss->name.c_str(), f.boss->x, f.boss->y, f.boss->hp, f.boss->dormant, f.boss->hidden); for (auto& m : f.mons) std::printf("mon %s %d,%d hp %d d%d h%d\n", m->name.c_str(), m->x, m->y, m->hp, m->dormant, m->hidden); }
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
    if (h->rekindled || h->dead) lvls += std::string(h->lost ? "LOST " : h->dead ? "ghost " : "") + (h->rekindled ? "rekindled" + std::to_string(h->rekindled) + " " : "");
  }
  for (auto& [s, it] : g.heroes.begin()->second->equip) gear += it->shortname() + ", ";
  std::string way;
  for (int z : g.route) way += (way.empty() ? "" : ">") + ACTS[z].key.substr(0, 4);
  std::string els;
  for (auto& [id, h] : g.heroes) els += (els.empty() ? "" : "+") + std::string(h->el >= 0 ? elem::NAME[h->el] : "-");
  names += "/" + els;
  std::printf("%-22s seed %2llu: %-4s depth %2d%s turn %5d lvl %s kills %3d gold %4d | %s | %s\n", names.c_str(), (unsigned long long)seed,
              g.state.c_str(), g.depth, g.floor->camp ? " (camp)" : "", g.turn, lvls.c_str(), kills, g.heroes.begin()->second->gold, way.c_str(),
              gear.c_str());
  if (std::getenv("BOTPACE")) std::printf("   pace (level at each depth): %s\n", pace.c_str());
  dive_depth = g.depth;
  dive_stalled = g.state == "play";
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

// Heroes who won together go down again only together: the group comes with the first hero, strangers are turned
// away, and the stairs in Charnfen wait for every member.
static int group_test() {
  Game a(5, 0);
  Hero& x = a.join(1, "Ash", "", json::object());
  a.join(2, "Bryn", "", json::object());
  json rx = json::parse(a.record(x).dump());
  bool ok = rx.contains("group") && rx["group"]["members"].size() == 2;
  Game b(6, 1);
  b.join(1, "Ash", "", json::object(), rx);
  ok = ok && b.group == rx["group"]["id"].get<std::string>() && b.members.size() == 2;
  ok = ok && !b.can_join(json()).empty();  // a stranger
  ok = ok && b.can_join(rx).empty();       // a member
  Game c(7, 0);
  c.join(1, "Solo", "", json::object());
  ok = ok && !c.can_join(rx).empty();      // a grouped hero cannot join a stranger's dive
  std::printf("groups: %s\n", ok ? "hold together" : "BROKEN");
  return ok ? 0 : 1;
}

int main() {
  if (kept_test()) return 1;
  if (group_test()) return 1;
  // BOTLADDER=n: the first weapon's hero goes on down the Frost ladder, n dives at most, kept between them
  if (const char* l = std::getenv("BOTLADDER")) {
    std::string w = std::getenv("BOTWEAPON") ? std::getenv("BOTWEAPON") : "staff";
    // BOTKEEP=dir saves the hero after each won dive; BOTFROM=f picks the ladder up from that hero at Frost f.
    // A dive the bot cannot finish (stuck at the step cap) is tried once more on another seed.
    const char* keep = std::getenv("BOTKEEP");
    int from = std::getenv("BOTFROM") ? std::atoi(std::getenv("BOTFROM")) : 0;
    auto path = [&](int f) { return std::string(keep) + "/" + w + "-" + std::to_string(f) + ".json"; };
    json kept;
    if (keep && from > 0) { std::ifstream in(path(from - 1)); if (in) in >> kept; }
    for (int f = from; f < std::atoi(l); ++f) {
      json prev = kept;
      int s0 = std::getenv("BOTSEED0") ? std::atoi(std::getenv("BOTSEED0")) : 100;  // another set of maps for a second sample
      kept = run({w}, s0 + f, f, prev);
      if (kept.is_null() && dive_stalled) kept = run({w}, s0 + 1000 + f, f, prev);
      if (kept.is_null()) break;
      if (keep) { std::ofstream out(path(f)); out << kept.dump(); }
    }
    return 0;
  }
  const char* only = std::getenv("BOTWEAPON") ? std::getenv("BOTWEAPON") : std::getenv("BOTCLASS");
  int seeds = std::getenv("BOTSEEDS") ? std::atoi(std::getenv("BOTSEEDS")) : 2;
  // BOTMATRIX=1: every weapon (or BOTWEAPON) with every element, BOTSEEDS dives each; a table of wins and depth at the end
  if (std::getenv("BOTMATRIX")) {
    std::vector<std::string> rows;
    for (auto& w : WEAPONS)
      if (!only || w.key == only) {
        std::string row = w.key;
        row.resize(10, ' ');
        for (int e = 0; e < elem::N; ++e) {
          bot_force_elem = e;
          int wins = 0, depth = 0;
          for (uint64_t s = 1; s <= uint64_t(seeds); ++s) { if (!run({w.key}, 100 * uint64_t(e) + s).is_null()) ++wins; depth += dive_depth; }
          char b[32];
          std::snprintf(b, sizeof b, "  %d/%d d%4.1f", wins, seeds, double(depth) / seeds);
          row += b;
        }
        rows.push_back(row);
      }
    std::printf("\nMATRIX (wins/dives, mean depth reached) ");
    for (int e = 0; e < elem::N; ++e) std::printf("| %s ", elem::NAME[e]);
    std::printf("\n");
    for (auto& r : rows) std::printf("MATRIX %s\n", r.c_str());
    return 0;
  }
  for (auto& w : WEAPONS)
    if (!only || w.key == only)
      for (uint64_t s = 1; s <= uint64_t(seeds); ++s) run({w.key}, s);
  if (!only && !std::getenv("BOTHARM")) run({"sword", "lantern", "dagger"}, 9);
  harm_report();
  // BOTCOOP=n: n co-op dives of three, to see embers carried and friends rekindled
  if (const char* c = std::getenv("BOTCOOP"))
    for (int i = 0; i < std::atoi(c); ++i) run({"sword", "lantern", "dagger"}, 20 + uint64_t(i));
  return 0;
}
