#include "game.hpp"

#include <libtcod/bresenham.hpp>
#include <libtcod/fov.h>

#include <cmath>
#include <deque>
#include <fstream>

#include "k5.hpp"
#include "pyre.hpp"
#include "lore.hpp"
#include "town_map.hpp"

using namespace data;

const MonDef& mon_def(const std::string& key) {
  for (auto& m : MONSTERS) if (m.key == key) return m;
  return MONSTERS[0];
}
const PerkDef& perk_def(const std::string& key) {
  for (auto& p : PERKS) if (p.key == key) return p;
  return PERKS[0];
}
const SkillDef& skill_def(const std::string& key) {
  for (auto& s : SKILLS) if (s.key == key) return s;
  return SKILLS[0];
}

// Can every open cell still be walked to from the stairs up? (Chests and shrines must never close a way.)
bool whole(const Floor& f) {
  std::vector<char> seen(W * H, 0);
  std::deque<int> q{f.up.y * W + f.up.x};
  seen[q.front()] = 1;
  int n = 1;
  auto open = [&](int x, int y) { int t = f.tile(x, y); return !data::blocks(t) || t == data::DOOR || t == data::VAULTDOOR; };
  while (!q.empty()) {
    int i = q.front(); q.pop_front();
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        int x = i % W + dx, y = i / W + dy;
        if (x < 0 || x >= W || y < 0 || y >= H || seen[y * W + x] || !open(x, y)) continue;
        seen[y * W + x] = 1; ++n; q.push_back(y * W + x);
      }
  }
  int all = 0;
  for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) if (open(x, y)) ++all;
  return n == all;
}
namespace {
int rnd_i(double v) { return int(std::lround(v)); }
std::string cap(std::string s) { if (!s.empty() && s[0] >= 'a' && s[0] <= 'z') s[0] = char(s[0] - 32); return s; }
std::string the(const Mon& m) { return m.rank == "unique" ? m.name : m.name.rfind("the ", 0) == 0 ? m.name : "the " + m.name; }
json xy(int x, int y) { return json::array({x, y}); }
json cells_json(const std::vector<P>& cs) { json a = json::array(); for (auto c : cs) a.push_back(xy(c.x, c.y)); return a; }
}  // namespace

Game::Game(uint64_t seed, int frost_) : frost(std::clamp(frost_, 0, 99)), rnd(std::getenv("PYREHEART_SEED") ? std::strtoull(std::getenv("PYREHEART_SEED"), nullptr, 10) : seed) {
  // for screenshots and tests: PYREHEART_ZONE=vaults starts the dive in that zone (by the first way there), and
  // PYREHEART_BOSS=1 on its boss's map, next to it; PYREHEART_ROUTE=cistern,vaults,prison picks the ways a whole dive takes
  this->seed = std::getenv("PYREHEART_SEED") ? std::strtoull(std::getenv("PYREHEART_SEED"), nullptr, 10) : seed;
  omens = omen::draw(this->seed, frost);
  if (const char* o = std::getenv("PYREHEART_OMEN"); o && frost > 0) omens = {std::clamp(std::atoi(o), 1, int(omen::N))};  // tests: this omen alone
  route = {0};
  auto zone_of = [](const std::string& k) { for (int i = 0; i < ZONES; ++i) if (ACTS[i].key == k) return i; return -1; };
  if (const char* z = std::getenv("PYREHEART_ZONE"); z && zone_of(z) > 0) {
    int want = zone_of(z);
    // the first way down to it: walk the route steps, taking the first link that can still lead there
    std::function<bool(int, std::vector<int>&)> find = [&](int at, std::vector<int>& path) {
      path.push_back(at);
      if (at == want) return true;
      for (int n : ACTS[at].next) if (find(n, path)) return true;
      path.pop_back();
      return false;
    };
    std::vector<int> path;
    if (find(0, path)) route = path;
    depth = 1;
    for (size_t i = 0; i + 1 < route.size(); ++i) depth += ACTS[route[i]].floors;
  }
  if (std::getenv("PYREHEART_BOSS") || std::getenv("PYREHEART_REST")) { zfloor = ACTS[route.back()].floors - 1; depth += zfloor; }
  else if (const char* zf = std::getenv("PYREHEART_ZFLOOR")) {  // which of the zone's maps, from 0: to look at a reported map again
    zfloor = std::clamp(std::atoi(zf), 0, ACTS[route.back()].floors - 1);
    depth += zfloor;
  }
  // a dive starts in Charnfen; a test start in a zone (above) begins on that map instead, and PYREHEART_TOWN=off skips
  // the village. PYREHEART_TOWN=<file> reads the village from a file instead of the copy built in (ref/charnfen.txt).
  const char* town_env = std::getenv("PYREHEART_TOWN");
  bool test_start = std::getenv("PYREHEART_ZONE") || std::getenv("PYREHEART_BOSS") || std::getenv("PYREHEART_REST") ||
                    std::getenv("PYREHEART_ZFLOOR");
  if (town_env ? std::string(town_env) != "off" : !test_start) {
    std::vector<std::string> rows;
    if (town_env && std::string(town_env) != "on") {
      std::ifstream in(town_env);
      for (std::string line; std::getline(in, line);) rows.push_back(line);
    }
    if (rows.empty()) rows.assign(town::MAP, town::MAP + town::ROWS);
    floor = make_town(rows);
    depth = 0;
  } else {
    floor = make_floor(depth);
    // PYREHEART_REST=1: the boss map with its boss already fallen and the helpers' camp up
    if (std::getenv("PYREHEART_REST") && floor->boss) {
      P at = floor->boss->x >= 0 ? P{floor->boss->x, floor->boss->y} : floor->down;
      floor->mons.erase(std::remove(floor->mons.begin(), floor->mons.end(), floor->boss), floor->mons.end());
      floor->boss = nullptr;
      floor->mons.erase(std::remove_if(floor->mons.begin(), floor->mons.end(), [](const MonP& m) { return !m->dormant; }), floor->mons.end());
      make_camp(*floor, at);
      std::fprintf(stderr, "CAMP %d %d\n", floor->fire.x, floor->fire.y);  // for screenshot tools
    }
  }
}

// Charnfen, from its hand-drawn rows
std::unique_ptr<Floor> Game::make_town(const std::vector<std::string>& rows) {
  auto f = std::make_unique<Floor>();
  f->depth = 0; f->act = 0; f->rest = true; f->id = floor_ids_++; f->w = W; f->h = H;
  static const std::map<std::string, int> KEY = {
      {"#", WALL}, {".", SNOW}, {":", PATH}, {"+", DOOR}, {"n", GRAVE}, {"t", CROSS}, {"\u2663", TREE}, {"\u2261", PEW},
      {"\u2021", ALTAR}, {"E", EMBER}, {"O", WELL}, {"?", BOARD}, {"w", WINDOW}, {"L", LIT}, {"\u2248", ICE}, {"%", CRACK},
      {">", STAIRS}, {"<", UPSTAIRS}, {"o", PILLAR}, {"_", FLAGS}, {"F", FIRE}, {"r", STONES}, {"C", CHIMNEY}, {"h", HEALER},
      {"b", SMITH}, {"g", DIGGER}, {"T", TORCH}, {"i", CANDLE}, {"U", OPENGRAVE}, {"G", GUARD}, {"X", BARRICADE}, {",", RUBBLE},
      {"$", PATH}, {"@", PATH}};
  for (int y = 0; y < H && y < int(rows.size()); ++y) {
    const std::string& line = rows[y];
    int x = 0;
    for (size_t i = 0; i < line.size() && x < W; ++x) {
      size_t n = (line[i] & 0x80) == 0 ? 1 : (line[i] & 0xE0) == 0xC0 ? 2 : (line[i] & 0xF0) == 0xE0 ? 3 : 4;
      std::string c = line.substr(i, n);
      i += n;
      auto k = KEY.find(c);
      f->t[y * W + x] = k == KEY.end() ? SNOW : k->second;
      if (c == "$") { f->has_merchant = true; f->merchant = {x, y}; }
      if (c == "@") f->up = {x, y};
      if (c == ">") f->down = {x, y};
      if (c == "U") f->open_graves.push_back({x, y});
    }
  }
  // the gatekeeper's rack beside the guards at the gate: one weapon each
  {
    int gx = W, gy = 0;
    for (int i = 0; i < W * H; ++i) if (f->t[i] == GUARD && i % W < gx) { gx = i % W; gy = i / W; }
    for (int dx = 2; dx < 6 && gx < W; ++dx)
      if (f->t[gy * W + gx - dx] == PATH || f->t[gy * W + gx - dx] == SNOW) { f->t[gy * W + gx - dx] = RACK; f->rack = {gx - dx, gy}; break; }
  }
  // five coals at the edge of the great fire, one of each element, on the path below its ring of stones
  int fx = 0, fy = 0, fn = 0;
  for (int i = 0; i < W * H; ++i) if (f->t[i] == FIRE) { fx += i % W; fy += i / W; ++fn; }
  if (fn) {
    fx /= fn; fy /= fn;
    for (int e = 0; e < elem::N; ++e) {
      P c{fx - 2 + e, fy + 3};
      f->t[c.y * W + c.x] = COALHEAP;
      f->coals.push_back({c, e});
    }
  }
  return f;
}

bool Game::final_floor() const { return route.back() == DEEP && zfloor == ACTS[DEEP].floors - 1 && !floor->rest; }

void Game::set_level() {
  const ActDef& z = ACTS[route.back()];
  double at = z.step + double(zfloor) / z.floors;
  lvl = std::clamp(1 + int(std::lround(15 * at / (STEPS - .5))), 1, 16);
}

// ------------------------------------------------------------------ players
Hero& Game::join(int pid, const std::string& name, const std::string& cls, const json& talents, const json& record) {
  for (auto& [id, h] : heroes)
    if (h->away && h->name == name) {  // the same player back again: they keep their hero
      auto back = std::move(h);
      heroes.erase(id);
      back->pid = pid;
      back->away = false;
      Hero& r = *back;
      heroes[pid] = std::move(back);
      place(r);
      rescale();
      say_all(r.name + " is back.", 0xB4DCB4);
      look();
      return r;
    }
  auto h = std::make_unique<Hero>();
  h->pid = pid;
  h->name = name;
  h->cls = &h->kit;
  if (talents.is_object()) {
    std::map<std::string, int> want;
    for (auto& [k, v] : talents.items()) if (v.is_number_integer()) want[k] = v.get<int>();
    h->talents = pyre::clean(want);
  }
  h->heal += 2 * h->tal("credit");
  { auto coat = loot::make("leather"); h->equip[coat->slot] = coat; }
  bool kept = record.is_object() && record.value("v", 0) == 1;
  if (kept && heroes.empty() && record.contains("group") && record["group"].is_object()) {  // the first hero brings its group
    group = record["group"].value("id", std::string());
    members = record["group"].value("members", std::vector<std::string>());
  }
  if (kept) restore(*h, record);  // the hero who came back up: all it grew and carries
  // a dive that starts below (tests, screenshots) skips the gatekeeper's rack: the weapon comes from the hello, an old
  // class name standing for the weapon it fought with
  if (floor->depth != 0 && !h->equip.count("weapon")) {
    static const std::map<std::string, std::string> OLD = {{"warrior", "sword"}, {"ranger", "bow"}, {"adept", "staff"},
                                                           {"lamplighter", "lantern"}, {"runecarver", "chisel"}, {"cutthroat", "dagger"}};
    std::string k = std::getenv("PYREHEART_WEAPON") ? std::getenv("PYREHEART_WEAPON") : cls;
    if (OLD.count(k)) k = OLD.at(k);
    if (!weapon_def(k)) k = "sword";
    auto w = loot::make(weapon_def(k)->start);
    h->equip["weapon"] = w;
  }
  refit(*h);
  // for screenshots and tests: start with a full pack of things to compare
  if (const char* l = std::getenv("PYREHEART_LEGEND"); l && !kept)  // tests: wear this legendary from the start
    for (auto& d : data::LEGENDARIES)
      if (d.key == l) {
        Rng r2(1);
        for (int i = 0; i < 200; ++i) {
          auto it = loot::legendary(r2, {}, 0);
          if (it->legend == d.key) { h->equip[it->slot] = it; refit(*h); break; }
        }
      }
  if (const char* g = std::getenv("PYREHEART_GIFTS"))
    for (int i = 0; i < std::atoi(g) && int(h->pack.size()) < PACK_MAX; ++i) h->pack.push_back(loot::roll(3, rnd, h->cls->kinds));
  if (!kept && (std::getenv("PYREHEART_BOSS") || std::getenv("PYREHEART_ZONE"))) {  // grown as far as a hero who came down to here
    int p = std::max(1, lvl * 2 - 1);
    h->lvl = 1 + (p - 1) / 3;
    int s = main_style(*h);
    h->srank[s] = p - 1 - 2 * (h->lvl - 1);
    for (size_t i = 0; i < STYLES[s].skills.size(); ++i) if (STYLE_RANKS[i] <= h->srank[s]) h->known.push_back(STYLES[s].skills[i]);
    refit(*h);
  }
  h->hp = stats(*h).maxhp;
  h->mana = stats(*h).maxmana;
  if (floor->depth != 0 && h->el < 0) {  // a dive that starts below (tests, screenshots): the coal is taken already
    const char* e = std::getenv("PYREHEART_ELEM");
    h->el = e ? std::clamp(std::atoi(e), 0, elem::N - 1) : (pid - 1) % elem::N;
    refit(*h);
  }
  Hero& r = *h;
  heroes[pid] = std::move(h);
  rescale();
  place(r);
  if (floor->depth == 0) {
    r.say("Charnfen. The great fire burns on the square; beyond it snow falls on the graves of the many who went down.", 0xD2B48C);
    r.say("The cathedral is a ruin; its portal fell in. Go round through the graves to the broken transept. Under the choir a hole leads down.", 0xFFD27F);
  } else {
    if (depth == 1) r.say("Charnfen, at the edge of ruin. The way down is through the graveyard.", 0xD2B48C);
    auto& open = lore::ACT_OPEN[floor->act];
    r.say(open[0] + ": " + open[1], 0xFFD27F);
  }
  r.say("Walk with the arrow keys or the number pad. Bump a foe to fight it. Help: [?].", 0x9CC89C);
  if (floor->depth == 0 && kept) r.say("You come back up into the cold, " + r.cls->name + ". The frost bites deeper now; so will the dark.", 0xFFD27F);
  if (floor->depth == 0 && frost > 0)  // the omens of this dive, told before the stairs (plan.md section 7)
    for (int o : omens) r.say(std::string("An omen of Frost ") + std::to_string(frost) + ": " + omen::DEF[o].name + ". " + cap(omen::DEF[o].text) + ".", 0x9CD2FF);
  if (floor->depth == 0 && !kept) {
    r.say("By the gate north of the square stands the gatekeeper's rack: bump it and take a weapon.", 0x9CC89C);
    r.say("Then walk to the great fire on the square and take a coal: bump one of the five at its edge.", 0x9CC89C);
  }
  if (heroes.size() > 1) say_all(r.name + " joins the dive.", 0xB4DCB4);
  look();
  return r;
}

void Game::leave(int pid) {
  auto it = heroes.find(pid);
  if (it == heroes.end()) return;
  it->second->away = true;
  say_all(it->second->name + " has left the dive.", 0xB4B4B4);
  check_stairs();
}

void Game::place(Hero& h) {
  P s;
  if (floor->depth == 0) {  // Charnfen: by the fire, side by side when friends come along
    h.x = h.y = -1;
    P at = floor->up;
    if (hero_at(at.x, at.y) && free_near(at.x, at.y, 4, s)) at = s;
    h.x = at.x; h.y = at.y;
    h.seen.assign(W * H, 1);
    return;
  }
  if (std::getenv("PYREHEART_REST") && floor->camp && free_near(floor->fire.x, floor->fire.y + 2, 4, s)) { h.x = s.x; h.y = s.y; return; }
  if (std::getenv("PYREHEART_BOSS") && floor->boss) {
    auto& b = *floor->boss;
    if (free_near(b.x, b.y, 8, s, [&](int x, int y) { return floor->walk(x, y) && cheb(x, y, b.x, b.y) >= 4; })) { h.x = s.x; h.y = s.y; return; }
  }
  // for screenshots: PYREHEART_NEAR=20 starts next to the first tile of that kind (20 a stone tomb)
  if (const char* n = std::getenv("PYREHEART_NEAR")) {
    // and PYREHEART_PEACE empties the floor of foes, so nothing dims the light or gets in the picture
    if (std::getenv("PYREHEART_PEACE")) { floor->mons.clear(); floor->boss = nullptr; }
    int want = std::atoi(n), skip = std::getenv("PYREHEART_NEAR_SKIP") ? std::atoi(std::getenv("PYREHEART_NEAR_SKIP")) : 0;
    for (int i = 0; i < W * H; ++i)
      if (floor->t[i] == want && skip-- <= 0 && free_near(i % W, i / W, 4, s)) { h.x = s.x; h.y = s.y; return; }
  }
  if (free_near(floor->up.x, floor->up.y, 4, s)) { h.x = s.x; h.y = s.y; }
  else { h.x = floor->up.x; h.y = floor->up.y; }
}

bool Game::free_near(int x, int y, int r, P& out, const std::function<bool(int, int)>& fn) {
  for (int d = 0; d <= r; ++d) {
    std::vector<P> ring;
    for (int dy = -d; dy <= d; ++dy)
      for (int dx = -d; dx <= d; ++dx)
        if (std::max(std::abs(dx), std::abs(dy)) == d) ring.push_back({x + dx, y + dy});
    rnd.shuffle(ring);
    for (auto c : ring)
      if (fn(c.x, c.y) && !hero_at(c.x, c.y) && !mon_at(c.x, c.y)) { out = c; return true; }
  }
  return false;
}
bool Game::free_near(int x, int y, int r, P& out) {
  return free_near(x, y, r, out, [&](int a, int b) { return floor->walk(a, b) && !way_down(floor->tile(a, b)); });
}

std::vector<Hero*> Game::active() {
  std::vector<Hero*> v;
  for (auto& [id, h] : heroes) if (h->present()) v.push_back(h.get());
  return v;
}
std::vector<Hero*> Game::living() {
  std::vector<Hero*> v;
  for (auto& [id, h] : heroes) if (!h->dead && !h->away) v.push_back(h.get());
  return v;
}
Hero* Game::hero_at(int x, int y) {
  for (auto& [id, h] : heroes) if (h->present() && h->x == x && h->y == y) return h.get();
  return nullptr;
}
MonP Game::mon_at(int x, int y) {
  for (auto& m : floor->mons) if (m->x == x && m->y == y && m->hp > 0) return m;
  return nullptr;
}
void Game::say_all(const std::string& t, int col) { for (auto& [id, h] : heroes) h->say(t, col); }
void Game::say_near(int x, int y, const std::string& t, int col) {
  for (auto& [id, h] : heroes) if (h->sees(x, y) || h->present()) h->say(t, col);
}
void Game::tip(Hero& h, const std::string& key, const std::string& text) {
  if (h.tips.insert(key).second) h.say(text, 0x9CC89C);
}
std::vector<std::string> Game::party_kinds() {
  std::vector<std::string> k;
  for (auto& [id, h] : heroes) for (auto& s : h->cls->kinds) k.push_back(s);
  return k;
}

// ------------------------------------------------------------------ numbers
Stats Game::stats(const Hero& h) const {
  auto s = [&](const std::string& k) { int t = 0; for (auto& [slot, it] : h.equip) t += it->stat(k); return t; };
  auto wi = h.equip.find("weapon");
  const Item* w = wi == h.equip.end() ? nullptr : wi->second.get();
  const WeaponDef* wd = w ? weapon_def(w->kind) : nullptr;
  int pw = power(h);
  int ar = 0;
  for (auto& [slot, it] : h.equip) ar += it->ar;
  auto res = [&](const std::string& k, int more = 0) { return std::min(75, s(k) + 20 * h.perk("ward") + more); };
  Stats st{};
  st.maxhp = h.cls->hp + h.cls->hp_lvl * (pw - 1) + 15 * h.perk("tough") + s("hp") + 5 * h.tal("hearthskin");
  if (h.scars) st.maxhp = std::max(10, st.maxhp * std::max(3, 10 - h.scars) / 10);  // each rekindling leaves a scar
  if (h.perk("glasspyre")) st.maxhp = std::max(10, st.maxhp * 4 / 5);
  if (h.leg("secondheart")) st.maxhp = std::max(10, st.maxhp * 7 / 10);            // the Second Heart: 30% less life               // Glass Pyre: a fifth less life
  st.maxmana = h.cls->mana + h.cls->mana_lvl * (pw - 1) + 15 * h.perk("well") + s("mana");
  st.lo = (w ? w->lo : 1) + s("dmg");
  st.hi = (w ? w->hi : 3) + s("dmg");
  st.pct = s("dmg_pct") + (h.boon > 0 ? 10 + 3 * std::min(frost, 10) : 0) + 15 * h.perk("brute") + 4 * h.tal("kindled") + (h.fury_t > 0 ? 50 : 0) + (w ? 4 * mastery(h, w->kind) : 0);
  // the style the weapon belongs to grows the weapon's blows: +4% a rank (the perks the old classes had, folded in)
  if (wd) st.pct += 4 * (h.srank[style_index(wd->style)] + (h.leg("lastcrown") ? 2 : 0));
  if (h.hp < st.maxhp / 2) st.pct += 5 * h.tal("fevered");
  st.acc = h.cls->acc + pw + 8 * h.perk("eye") + s("acc");
  st.crit = 5 + 4 * h.perk("eye") + s("crit") + 2 * h.tal("coaleyed") + (wd && wd->style == "ranged" ? 10 * h.perk("sharpshooter") : 0);
  st.armour = ar + s("armour") + 3 * h.perk("hide") + 4 * h.perk("guardian") + h.srank[0] + (h.leg("lastcrown") ? 2 : 0);  // a melee rank is a lesson in taking blows
  if (h.perk("crowdbreaker") && floor) {  // Crowd-breaker: +3 armour for each foe beside it, up to three
    int n = 0;
    for (auto& m : floor->mons) if (m->hp > 0 && cheb(m->x, m->y, h.x, h.y) == 1) ++n;
    st.armour += 3 * std::min(3, n);
  }
  if (h.sunder > 0) st.armour /= 2;  // split by acid or a smith's hammer
  if (h.leg("lastspear")) st.armour /= 2;  // the Last Dwarf's Spear: no guard but the point
  st.res_f = res("res_fire"); st.res_c = res("res_cold", 10 * h.tal("warmhands")); st.res_p = res("res_poison");
  st.leech = s("leech"); st.spell = s("spell") + 25 * h.perk("invoker"); st.fire = s("fire_dmg");
  st.regen = 1 + h.perk("well") + s("regen"); st.thorns = s("thorns");
  st.reach = wd ? wd->reach : 0;
  st.bow = st.reach > 0 || (w && w->kind == "chisel");
  return st;
}

int Game::need(int lvl) const { return rnd_i(25 * std::pow(lvl + 1.0, 2.7)); }  // total xp to reach lvl + 1

bool Game::can_use(const Hero& h, const Item& it) const {
  (void)h;
  return it.slot != "weapon" || weapon_def(it.kind) != nullptr;
}

// Puts the item on a copy of the hero and measures what moved. The weights only decide the one-glance mark;
// the player still sees every number.
Verdict Game::compare(const Hero& h, const Item& it) const {
  Verdict out;
  if (!can_use(h, it)) { out.v = -9; return out; }
  Hero t;
  t.lvl = h.lvl; t.perks = h.perks; t.equip = h.equip; t.wxp = h.wxp; t.talents = h.talents;
  for (int i = 0; i < 3; ++i) t.srank[i] = h.srank[i];
  t.kit = h.kit;
  t.equip[it.slot] = std::make_shared<Item>(it);
  Stats a = stats(h), b = stats(t);
  bool mage = main_style(h) == 2;
  auto avg = [](const Stats& s) { return (s.lo + s.hi) / 2.0 * (1 + s.pct / 100.0); };
  double score = 0; bool up = false, down = false;
  auto add = [&](const std::string& what, double by, double weight) {
    if (std::abs(by) < .05) return;
    out.d.push_back({what, by});
    score += by * weight;
    (by > 0 ? up : down) = true;
  };
  add("damage", std::round((avg(b) - avg(a)) * 10) / 10, mage ? .5 : 3);
  add("spell power %", b.spell - a.spell, mage ? 1.5 : 0);
  add("fire damage", b.fire - a.fire, 2);
  add("armour", b.armour - a.armour, 1);
  add("life", b.maxhp - a.maxhp, .5);
  add("mana", b.maxmana - a.maxmana, mage ? .6 : .2);
  add("accuracy", b.acc - a.acc, .3);
  add("crit %", b.crit - a.crit, 1.5);
  add("fire resist %", b.res_f - a.res_f, .4);
  add("cold resist %", b.res_c - a.res_c, .4);
  add("poison resist %", b.res_p - a.res_p, .4);
  add("life leech %", b.leech - a.leech, 1.5);
  add("mana regen", b.regen - a.regen, mage ? 3 : 1);
  add("thorns", b.thorns - a.thorns, .5);
  if (up && !down) out.v = 2;
  else if (down && !up) out.v = -2;
  else if (up) out.v = score > 0 ? 1 : score < 0 ? -1 : 0;
  return out;
}

void Game::rescale() {
  double k = 1 + .6 * (std::max<size_t>(1, living().size()) - 1);
  if (k == scaled_) return;
  for (auto& m : floor->mons) {
    double f = double(m->hp) / m->maxhp;
    m->maxhp = rnd_i(m->maxhp / scaled_ * k);
    m->hp = std::max(1, rnd_i(m->maxhp * f));
  }
  scaled_ = k;
}

// ------------------------------------------------------------------ the floors
// Each map is made from its own seed: the dive's seed, the zone and which of the zone's maps it is. So a map someone
// reports can be made again exactly (PYREHEART_SEED, PYREHEART_ZONE, PYREHEART_ZFLOOR), whatever happened before it.
std::unique_ptr<Floor> Game::make_floor(int d) {
  uint64_t fs = seed ^ (uint64_t(route.back() + 1) * 0x9E3779B97F4A7C15ull) ^ (uint64_t(zfloor + 1) * 0xC2B2AE3D27D4EB4Full);
  fs ^= fs >> 31; fs *= 0xBF58476D1CE4E5B9ull; fs ^= fs >> 29;
  Rng keep = rnd;
  rnd = Rng(fs);
  auto out = make_floor_from(d);
  rnd = keep;
  rnd.g.discard(1);
  return out;
}

std::unique_ptr<Floor> Game::make_floor_from(int d) {
  auto f = std::make_unique<Floor>();
  f->depth = d; f->act = route.back(); f->id = floor_ids_++;
  Floor* old = floor.release();
  floor.reset(f.release());
  delete old;
  Floor& fl = *floor;
  set_level();
  int act = fl.act;
  const ActDef& zone = ACTS[act];
  bool boss_floor = zfloor == zone.floors - 1;
  // the Deep's maps are a crust maze: no stairs, the way on lies at an edge of the map
  bool built = false;
  if (act == DEEP) {
    char entry = zfloor == 0 || deep_exit_ == 'N' ? 'S' : deep_exit_ == 'E' ? 'W' : 'E';
    for (int i = 0; i < 40 && !built; ++i) {
      fl.t.assign(W * H, WALL);
      char out = 0;
      built = build_deep(fl, entry, boss_floor, out);
      if (built) deep_exit_ = out;
    }
  }
  if (!built) {
    fl.t.assign(W * H, WALL);
    fl.deep = false;
    fl.w = zone.w; fl.h = zone.h;
    build_rooms(fl, d, boss_floor);
  }
  // creatures
  std::vector<P> far;
  {
    std::vector<int> dist(W * H, -1);
    std::deque<int> q{fl.up.y * W + fl.up.x};
    dist[q.front()] = 0;
    while (!q.empty()) {
      int i = q.front(); q.pop_front();
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
          int x = i % W + dx, y = i / W + dy;
          if (x < 0 || y < 0 || x >= W || y >= H || dist[y * W + x] >= 0) continue;
          int t = fl.tile(x, y);
          if (data::blocks(t) && t != DOOR) continue;
          dist[y * W + x] = dist[i] + 1;
          q.push_back(y * W + x);
        }
    }
    for (int i = 0; i < W * H; ++i) {
      int t = fl.t[i];
      if (dist[i] > 12 && (t == FLOOR || t == CRUST)) far.push_back({i % W, i / W});
    }
  }
  // as many foes for the room there is: a big map holds more of them
  double room = std::max(1.0, .85 * fl.w * fl.h / double(76 * 42));
  int budget = int((14 + lvl * 1.5) * room * (1 + .05 * frost)) - int(fl.sleepers.size());  // 5% more on each Frost level
  auto& spawns = SPAWNS[act];
  auto pick_kind = [&]() {
    int sum = 0;
    for (auto& [k, w] : spawns) sum += w;
    int r = rnd.below(sum);
    for (auto& [k, w] : spawns) { r -= w; if (r < 0) return k; }
    return spawns[0].first;
  };
  scaled_ = 1.0;
  if (boss_floor) {
    fl.boss = spawn(zone.boss, fl.down.x, fl.down.y);
    budget -= 4;
  }
  for (int pack = 0; pack < int(room + .5); ++pack) {
  if (lvl >= 2 && rnd.real() < (omen(omen::MUSTER) ? .85 : .65) && !far.empty()) {
    std::string kind = pick_kind();
    std::string aff = rnd.pick(AFFIXES).first;
    P c = rnd.pick(far);
    std::vector<std::string> mods{aff};
    if (frost > 0 && rnd.real() < .35 * frost) {  // Frost: a second affix
      std::string more = rnd.pick(AFFIXES).first;
      if (more != aff) mods.push_back(more);
    }
    if (omen(omen::KINGSCOLD))  // the King's own cold: one affix more
      for (int t = 0; t < 8; ++t) { std::string more = rnd.pick(AFFIXES).first; if (std::find(mods.begin(), mods.end(), more) == mods.end()) { mods.push_back(more); break; } }
    for (int i = 0, n = omen(omen::MUSTER) ? 4 : 3; i < n; ++i) if (auto m = spawn_near(kind, c)) champion(*m, mods);
    budget -= 4;
  }
  if (lvl >= 3 && rnd.real() < .5 && !far.empty()) {
    std::string kind = pick_kind();
    P c = rnd.pick(far);
    if (auto m = spawn_near(kind, c)) unique(*m);
    int n = rnd.range(2, 3);
    for (int i = 0; i < n; ++i) spawn_near(kind, c);
    budget -= 5;
  }
  }
  while (budget > 0 && !far.empty()) {
    std::string kind = pick_kind();
    auto pk = PACKS.find(kind);
    int n = pk == PACKS.end() ? rnd.range(1, 3) : rnd.range(pk->second.first, pk->second.second);
    int cost = n;
    if (omen(omen::PACKS)) ++n;                          // the packs grow
    if (omen(omen::LONEWOLVES)) n = std::max(1, n - 1);  // Lone Wolves: fewer, and stronger (spawn)
    P c = rnd.pick(far);
    for (int i = 0; i < n; ++i) spawn_near(kind, c);
    budget -= cost;
  }
  if (!boss_floor && floor->depth > 0 && !far.empty()) spawn_nemesis(fl, far);
  if (omen(omen::CRACKED) && zfloor == 0 && !boss_floor && floor->depth > 0 && !far.empty()) {  // Cracked Seals: a hoard, and its guards
    P c = rnd.pick(far);
    if (fl.tile(c.x, c.y) == FLOOR || fl.tile(c.x, c.y) == CRUST) {
      fl.set(c.x, c.y, CHEST);
      std::string kind = pick_kind();
      std::vector<std::string> mods{rnd.pick(AFFIXES).first};
      for (int i = 0; i < 3; ++i) if (auto m = spawn_near(kind, c)) champion(*m, mods);
    }
  }
  zone_foes(fl);
  if (omen(omen::HUNGRYDARK) && fl.depth > 0)  // Hungry Dark: the torches and lamps are out
    for (auto& t : fl.t) { if (t == TORCH) t = WALL; else if (t == LAMP || t == BRAZIER || t == CANDLE) t = PILLAR; }
  return std::move(floor);
}

// a floor of rooms and halls (k5), dressed, with treasure, a shrine and a writing now and then
void Game::build_rooms(Floor& fl, int d, bool boss_floor) {
  int act = fl.act;
  {
    std::vector<std::vector<P>> rooms, treasure;
    if (build_zone(fl, boss_floor, rooms, treasure)) {  // zones.cpp
      fl.t[fl.up.y * W + fl.up.x] = UPSTAIRS;
      if (!boss_floor) fl.t[fl.down.y * W + fl.down.x] = STAIRS;
      if (act == CISTERN) {
        // the Cistern's own layout: its themes go into the halls of the waterworks
        k5::Plan plan;
        plan.up = fl.up; plan.down = fl.down; plan.mood = "own";
        for (auto& r : side_rooms_) {
          k5::Room kr;
          kr.id = int(plan.rooms.size()); kr.kind = "room";
          for (auto c : r) kr.cells.insert(c);
          plan.rooms.push_back(kr);
        }
        dress_cistern(fl, plan);
      }
      furnish(fl, rooms, treasure);
      return;
    }
    fl.t.assign(W * H, WALL);
    fl.ground.clear();
  }
  k5::Plan plan;
  bool have = false;
  for (int i = 0; i < 30; ++i) {
    double room = fl.w * fl.h / double(76 * 42);
    auto p = k5::make(fl.w, fl.h, rnd, ACTS[act].mood, int(13 * room));
    if (p.up == p.down || !p.floor.count(p.down)) continue;  // a level too small to have two ends
    int rooms = 0;
    for (auto& r : p.rooms) if (r.kind != "hall") ++rooms;
    if (p.floor.size() >= 750 * room && rooms >= 6 * room) { plan = std::move(p); have = true; break; }
    if (!have || p.floor.size() > plan.floor.size()) { plan = std::move(p); have = true; }
  }
  for (auto& [c, k] : plan.floor)
    fl.t[c.y * W + c.x] = k == "water" ? (act == 2 ? LAVA : act == CISTERN ? SLUDGE : WATER)
                        : k == "shallow" ? (act == 2 ? RUBBLE : act == CISTERN ? MIRE : SHALLOW)
                        : k == "rubble" ? RUBBLE : FLOOR;
  for (auto d2 : plan.doors) fl.t[d2.y * W + d2.x] = DOOR;
  fl.up = plan.up; fl.down = plan.down;
  // deep water, lava or sludge must never shut in the stairs or cut the level in two: the pools by the stairs go
  // shallow, and if that is not enough, all of them do
  auto shallow_of = [&](int t) { return t == WATER ? int(SHALLOW) : t == LAVA ? int(RUBBLE) : t == SLUDGE ? int(MIRE) : -1; };
  for (P e : {plan.up, plan.down})
    for (int dy = -2; dy <= 2; ++dy)
      for (int dx = -2; dx <= 2; ++dx)
        if (int s2 = shallow_of(fl.tile(e.x + dx, e.y + dy)); s2 >= 0) fl.set(e.x + dx, e.y + dy, s2);
  if (!whole(fl))
    for (int i = 0; i < W * H; ++i) if (int s2 = shallow_of(fl.t[i]); s2 >= 0) fl.t[i] = s2;
  fl.t[plan.up.y * W + plan.up.x] = UPSTAIRS;
  if (!boss_floor) fl.t[plan.down.y * W + plan.down.x] = STAIRS;
  dress(fl, plan);
  if (act == CISTERN) dress_cistern(fl, plan);
  std::vector<std::vector<P>> rooms, treasure;
  for (auto& r : plan.rooms)
    if (r.kind != "hall" && !r.cells.empty() && !r.cells.count(plan.up)) rooms.push_back(std::vector<P>(r.cells.begin(), r.cells.end()));
  for (size_t i = 0; i < plan.themes.size() && i < 2; ++i)
    treasure.push_back(std::vector<P>(plan.rooms[plan.themes[i]].cells.begin(), plan.rooms[plan.themes[i]].cells.end()));
  furnish(fl, rooms, treasure);
}

// treasure in the rooms (the ones behind a single way in first), now and then a shrine, and a writing to find
void Game::furnish(Floor& fl, const std::vector<std::vector<P>>& rooms, std::vector<std::vector<P>> treasure) {
  auto floor_cells = [&](const std::vector<P>& r) {
    std::vector<P> v;
    for (auto c : r) if (fl.tile(c.x, c.y) == FLOOR && c != fl.up && c != fl.down) v.push_back(c);
    std::sort(v.begin(), v.end(), [](P a, P b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
    return v;
  };
  double room = std::max(1.0, fl.w * fl.h / double(76 * 42));
  if (!rooms.empty()) for (int i = 0; i < int(room); ++i) if (rnd.real() < .5) treasure.push_back(rnd.pick(rooms));
  for (auto& r : treasure) {
    auto cs = floor_cells(r);
    std::vector<P> corners;
    for (auto c : cs) {
      int walls = 0;
      for (P d4 : {P{1, 0}, P{-1, 0}, P{0, 1}, P{0, -1}}) if (fl.tile(c.x + d4.x, c.y + d4.y) == WALL) ++walls;
      if (walls >= 2) corners.push_back(c);
    }
    auto& pool = corners.empty() ? cs : corners;
    if (pool.empty()) continue;
    P c = rnd.pick(pool);
    fl.set(c.x, c.y, CHEST);
    if (!whole(fl)) fl.set(c.x, c.y, FLOOR);
  }
  // shrines: the Sanctum is full of them (and not all of them kind)
  int shrines = fl.act == SANCTUM ? 4 + int(room) : rnd.real() < .35 * room ? 1 : 0;
  for (int i = 0; i < shrines && !rooms.empty(); ++i) {
    auto cs = floor_cells(rnd.pick(rooms));
    if (cs.empty()) continue;
    P c = cs[cs.size() / 2];
    fl.set(c.x, c.y, SHRINE);
    if (!whole(fl)) fl.set(c.x, c.y, FLOOR);
  }
  for (int i = 0; i < int(room + .5) && !rooms.empty(); ++i) {
    if (rnd.real() > .7) continue;
    std::vector<std::string> left;
    for (auto& w : lore::WRITINGS[fl.act]) if (!used_writings_.count(w)) left.push_back(w);
    if (left.empty()) break;
    auto cs = floor_cells(rnd.pick(rooms));
    if (cs.empty()) continue;
    P c = rnd.pick(cs);
    std::string w = rnd.pick(left);
    fl.writings[c.y * W + c.x] = w;
    used_writings_.insert(w);
  }
}

// The helpers' camp (plan.md section 6): when a zone's boss falls, the helpers from Charnfen come down behind the heroes
// and make camp where it stood: the fire, a ring of stones and bedrolls, and round it Ysolde, Brann, the trader and old
// Ebb with their things. The boss room is the zone's waystation now; the ways on open when Ebb is asked.
void Game::make_camp(Floor& f, P at) {
  std::vector<char> used(W * H, 0);
  for (auto& fi : f.items) used[fi.y * W + fi.x] = 1;
  for (auto& [p, pid] : f.embers) used[p.y * W + p.x] = 1;
  for (auto& [id, h] : heroes) if (h->x >= 0) used[h->y * W + h->x] = 1;
  for (auto& m : f.mons) if (m->hp > 0) used[m->y * W + m->x] = 1;
  auto open_ground = [&](int x, int y) {
    int t = f.tile(x, y);
    return f.walk(x, y) && !way_down(t) && t != UPSTAIRS && t != GRATE && !used[y * W + x];
  };
  // a thing that stands in the way only goes where all round it is open, so no passage is ever shut
  auto roomy = [&](int x, int y) {
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx)
        if ((dx || dy) && (!f.walk(x + dx, y + dy) || way_down(f.tile(x + dx, y + dy)))) return false;
    return true;
  };
  auto place = [&](int dx, int dy, int tile) -> P {
    int best = -1, bd = 1 << 30;
    bool block = tile >= 0 && blocks(tile);
    for (int y = 1; y < H - 1; ++y)
      for (int x = 1; x < W - 1; ++x) {
        if (!open_ground(x, y) || (block && !roomy(x, y))) continue;
        int d = (x - at.x - dx) * (x - at.x - dx) + (y - at.y - dy) * (y - at.y - dy);
        if (d < bd) { bd = d; best = y * W + x; }
      }
    if (best < 0 || bd > 64) return {-1, -1};  // too far from the fire to belong to the camp
    used[best] = 1;
    if (tile >= 0) f.set(best % W, best / W, tile);
    return {best % W, best / W};
  };
  // a cramped room (the Webmother's nest) draws the camp in closer round the fire
  int room = 0;
  for (int y = at.y - 6; y <= at.y + 6; ++y)
    for (int x = at.x - 12; x <= at.x + 12; ++x) if (f.walk(x, y)) ++room;
  int hx = room > 180 ? 9 : 5, hy = room > 180 ? 4 : 3;
  // the ground the fire's room reaches before anything is set down, to check after that the camp shuts no one in
  auto reach = [&](P from) {
    std::vector<char> seen(W * H, 0);
    std::vector<int> todo{from.y * W + from.x};
    seen[todo[0]] = 1;
    while (!todo.empty()) {
      int i = todo.back(); todo.pop_back();
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
          int nx = i % W + dx, ny = i / W + dy;
          if (nx < 0 || ny < 0 || nx >= W || ny >= H || seen[ny * W + nx] || !f.walk(nx, ny)) continue;
          seen[ny * W + nx] = 1;
          todo.push_back(ny * W + nx);
        }
    }
    return seen;
  };
  std::vector<std::pair<int, int>> stones;  // where the ring of stones went, and what lay there before
  P fire0{-1, -1};
  for (int y = 1, bd = 1 << 30; y < H - 1; ++y)
    for (int x = 1; x < W - 1; ++x)
      if (open_ground(x, y) && roomy(x, y) && (x - at.x) * (x - at.x) + (y - at.y) * (y - at.y) < bd) { bd = (x - at.x) * (x - at.x) + (y - at.y) * (y - at.y); fire0 = {x, y}; }
  std::vector<char> before = fire0.x >= 0 ? reach(fire0) : std::vector<char>(W * H, 0);
  P fire = place(0, 0, CAMPFIRE);
  bool crowded = false;
  if (fire.x < 0) {  // the boss's drops cover its cramped nest: the camp goes up on them, and they are set aside after
    for (auto& fi : f.items) used[fi.y * W + fi.x] = 0;
    fire = place(0, 0, CAMPFIRE);
    crowded = true;
  }
  if (fire.x < 0) {  // nowhere roomy near: the fire goes on the nearest open ground, wherever that is
    int best = -1, bd = 1 << 30;
    for (int y = 1; y < H - 1; ++y)
      for (int x = 1; x < W - 1; ++x)
        if (open_ground(x, y) && roomy(x, y) && (x - at.x) * (x - at.x) + (y - at.y) * (y - at.y) < bd) { bd = (x - at.x) * (x - at.x) + (y - at.y) * (y - at.y); best = y * W + x; }
    if (best < 0) return;
    used[best] = 1;
    f.set(best % W, best / W, CAMPFIRE);
    fire = {best % W, best / W};
  }
  at = fire;
  f.fire = fire;
  f.camp_at = fire;
  for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx)
      if ((dx || dy) && open_ground(at.x + dx, at.y + dy)) {
        stones.push_back({(at.y + dy) * W + at.x + dx, f.tile(at.x + dx, at.y + dy)});
        f.set(at.x + dx, at.y + dy, STONES); used[(at.y + dy) * W + at.x + dx] = 1;
      }
  for (auto [dx, dy] : std::vector<P>{{-3, 1}, {3, 1}, {-2, -3}, {2, -3}, {-1, 3}, {2, 3}}) place(dx, dy, HIDE);
  // Ysolde with her kettle, herbs and bandages
  place(-hx, -hy, HEALER); place(-hx - 1, -hy, BANDAGE); place(-hx - 2, -hy, HIDE); place(-hx + 1, -hy - 1, CANDLE);
  // Brann at his anvil, his brazier and tongs
  place(hx, -hy, SMITH); place(hx + 1, -hy - 1, ANVIL); place(hx + 2, -hy, BRAZIER); place(hx + 2, -hy + 1, TONGS);
  // the trader at his cart and crates
  P m = place(-hx, hy, -1);
  if (m.x >= 0) { f.has_merchant = true; f.merchant = m; }
  place(-hx - 1, hy, CART); place(-hx + 1, hy - 1, CRATE); place(-hx + 1, hy + 1, CRATE); place(-hx - 1, hy + 1, KEG);
  // old Ebb with his book of the dead, candles and spade
  place(hx, hy, DIGGER); place(hx + 1, hy, LEDGER); place(hx + 2, hy, CANDLE); place(hx + 1, hy + 1, SHOVEL);
  // a ring of stones across a narrow hall would wall the camp off from the rest of the map: then the ring goes
  auto split = [&] {
    int from = -1;
    for (int i = 0; i < W * H && from < 0; ++i) if (before[i] && f.walk(i % W, i / W)) from = i;
    if (from < 0) return false;
    auto now = reach({from % W, from / W});
    for (int i = 0; i < W * H; ++i) if (before[i] && f.walk(i % W, i / W) && !now[i]) return true;
    return false;
  };
  if (split()) for (auto [i, t] : stones) f.set(i % W, i / W, t);
  if (crowded)
    for (auto& fi : f.items)
      if (!f.walk(fi.x, fi.y) || used[fi.y * W + fi.x] == 1) {
        P c;
        if (free_near(fi.x, fi.y, 6, c, [&](int a, int b) { return open_ground(a, b) && !f.burning(a, b); })) { fi.x = c.x; fi.y = c.y; }
      }
  f.camp = true;
  f.shop.clear();
  auto kinds = party_kinds();
  int wares = 5;
  for (auto& [id, h] : heroes) if (h->tal("friend")) wares = 6;
  for (int i = 0; i < wares; ++i) f.shop.push_back(roll_loot(lvl + 1, kinds, "", "magic"));
  add_fx({{"k", "rise"}, {"x", at.x}, {"y", at.y}, {"col", 0xFFB040}});
  say_all(lore::CAMP, 0xD2B48C);
  for (auto* h : living()) {
    h->drank = false;
    if (h->tal("forged") && !h->forged) h->say("The Spark in you could reforge your weapon's magic at the camp fire, once this dive: [r].", 0xFFB040);
    bool due = false;
    for (int s = 0; s < 3; ++s) if (h->srank[s] >= PATH_RANK && h->path[s] < 0) due = true;
    if (due) h->say("A path waits for you at the fire. Walk into it.", 0xFFE680);
    else if (h->known.size() > 4) h->say("More skills wait than your keys hold. Walk into the fire to choose.", 0xFFE680);
    else tip(*h, "fire", "Rest by the fire: walk into it to set your keys, choose a path, or take a second coal.");
    tip(*h, "camp", "Walk into a helper to talk: Ysolde heals, Brann works your gear, the trader buys and sells, and old Ebb opens the way on.");
  }
}

// Old Ebb opens the way to the zone the hero chose: a stair near the camp, each zone its own way of opening.
void Game::open_way(Hero& h, int zone) {
  Floor& f = *floor;
  const auto& next = ACTS[route.back()].next;
  if (!f.camp || std::find(next.begin(), next.end(), zone) == next.end()) return;
  for (auto [i, z] : f.exits) { h.say("Ebb taps the book. \"The way to " + ACTS[z].name + " is open. One way a camp.\"", 0x9696AA); return; }
  P c = f.camp_at;
  // only a cell the fire's ground reaches: a walled-off pocket would hold the way where no one can get to it
  std::vector<char> near(W * H, 0);
  std::vector<int> todo{c.y * W + c.x};
  near[todo[0]] = 1;
  while (!todo.empty()) {
    int i = todo.back(); todo.pop_back();
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        int nx = i % W + dx, ny = i / W + dy;
        if (nx < 0 || ny < 0 || nx >= W || ny >= H || near[ny * W + nx]) continue;
        if (!f.walk(nx, ny) && f.tile(nx, ny) != STONES) continue;  // out over the ring of stones round the fire
        near[ny * W + nx] = 1;
        todo.push_back(ny * W + nx);
      }
  }
  int best = -1, bd = 1 << 30;
  for (int gap : {5, 2})  // a little way off the fire, or closer where the fire's ground is small
    if (best < 0)
    for (int y = 1; y < H - 1; ++y)
    for (int x = 1; x < W - 1; ++x) {
      int t = f.tile(x, y);
      if (!near[y * W + x] || !f.walk(x, y) || way_down(t) || t == UPSTAIRS || hero_at(x, y) || mon_at(x, y)) continue;
      bool by_stair = false;
      for (auto [i, z] : f.exits) if (cheb(x, y, i % W, i / W) < 4) by_stair = true;
      int d = cheb(x, y, c.x, c.y);
      if (by_stair || d < gap) continue;  // apart from the other way
      int score = std::abs(d - 6) * 10 + (x < c.x ? 5 : 0);
      if (score < bd) { bd = score; best = y * W + x; }
    }
  if (best < 0) return;
  int x = best % W, y = best / W;
  for (auto& fi : f.items) if (fi.x == x && fi.y == y) { fi.x = c.x + 1; fi.y = c.y + 2; }
  f.set(x, y, STAIRS);
  f.exits[best] = zone;
  f.down = {x, y};
  add_fx({{"k", "rise"}, {"x", x}, {"y", y}, {"col", 0xFFE678}});
  add_fx({{"k", "quake"}, {"x", x}, {"y", y}, {"dur", 1.8}});
  say_all(lore::WAY_OPENS[route.back()] + " The way to " + ACTS[zone].name + " is open.", 0xD2B48C);
  h.talking.clear();
}

// A helper's window: the keys the client sends while talking (plan.md section 6).
void Game::talk(Hero& h, const json& a) {
  std::string k = a.value("k", "");
  if (k == "close") { h.talking.clear(); return; }
  if (h.talking == "healer") {
    if (k == "heal") {
      if (h.drank) { h.say("Ysolde shakes her head. \"Once a camp. The rest your body does itself.\"", 0xAADCAA); return; }
      h.drank = true;
      Stats st = stats(h);
      h.hp = st.maxhp; h.mana = st.maxmana; h.poison = 0;
      h.say("Ysolde binds your wounds. You feel whole again.", 0xAADCAA);
      if (h.scars > 0) h.say("\"The scars I cannot mend. Those you carry.\"", 0xAADCAA);
      add_fx({{"k", "rise"}, {"x", h.x}, {"y", h.y}, {"col", 0xAADCAA}});
    } else if (k == "potion") buy_potion(h, a.value("p", ""));
  } else if (h.talking == "smith" && k == "retune") {
    retune(h, a.value("id", 0), a.value("from", ""), a.value("e", -1));
  } else if (h.talking == "smith" && k == "temper") {
    temper(h, a.value("id", 0));
  } else if (h.talking == "gate" && k == "frost" && floor->depth == 0) {
    int open = h.frost_open;
    for (auto* o : active()) open = std::min(open, o->frost_open);
    int f = a.value("f", -1);
    if (f < 0 || f > open || f == frost) return;
    set_frost(f);
    say_all(std::string("The guard chalks it on his board: ") + (f ? "Frost " + std::to_string(f) : std::string("Normal")) + ".", 0xA8C0E0);
    for (int o : omens) say_all(std::string("An omen: ") + omen::DEF[o].name + ". " + cap(omen::DEF[o].text) + ".", 0x9CD2FF);
  } else if (h.talking == "guide" && k == "way") {
    open_way(h, a.value("z", -1));
  }
}

// Brann turns one resistance on a worn thing into another element, for gold.
void Game::retune(Hero& h, int id, const std::string& from, int to) {
  static const char* RES[elem::N] = {"res_fire", "res_cold", "res_ash", "res_poison", "res_blood"};
  if (to < 0 || to >= elem::N || from.rfind("res_", 0) != 0) return;
  ItemP it;
  for (auto& [slot, i] : h.equip) if (i->id == id) it = i;
  if (!it || from == RES[to]) return;
  auto st = std::find_if(it->stats.begin(), it->stats.end(), [&](auto& p) { return p.first == from; });
  if (st == it->stats.end()) return;
  int price = h.perk("brannsfriend") ? retune_price() / 2 : retune_price();
  if (h.gold < price) { h.say("Brann grunts. \"" + std::to_string(price) + " gold, and not a coin less.\"", 0xC8966E); return; }
  h.gold -= price;
  int v = st->second;
  it->stats.erase(st);
  auto same = std::find_if(it->stats.begin(), it->stats.end(), [&](auto& p) { return p.first == RES[to]; });
  if (same != it->stats.end()) same->second += v; else it->stats.push_back({RES[to], v});
  refit(h);
  h.say("Brann's hammer rings. " + it->name + " now turns " + elem::NAME[to] + " instead.", 0xC8966E);
  add_fx({{"k", "burst"}, {"x", h.x}, {"y", h.y}, {"r", 1}, {"col", elem::COL[to]}});
}

// Brann tempers a worn thing one step, for gold; never past what the Frost level could give (plan.md section 7)
void Game::temper(Hero& h, int id) {
  ItemP it;
  for (auto& [slot, i] : h.equip) if (i->id == id) it = i;
  if (!it) return;
  if (it->plus >= temper_cap()) {
    h.say("Brann turns it over. \"No more in it. Not at this Frost. Go deeper, and I'll go further.\"", 0xC8966E);
    return;
  }
  int price = temper_price(h, *it);
  if (h.gold < price) { h.say("Brann grunts. \"" + std::to_string(price) + " gold, and not a coin less.\"", 0xC8966E); return; }
  h.gold -= price;
  loot::temper(*it);
  refit(h);
  h.say("Brann's hammer rings on and on. " + it->name + ".", 0xC8966E);
  add_fx({{"k", "burst"}, {"x", h.x}, {"y", h.y}, {"r", 1}, {"col", 0xFFB040}});
}

// ------------------------------------------------------------------ the killer comes back
void Game::spawn_nemesis(Floor& fl, const std::vector<P>& far) {
  if (!nemesis.is_object() || nemesis_met || nemesis.value("zone", std::string()) != ACTS[fl.act].key) return;
  const std::string key = nemesis.value("key", std::string());
  bool known = false;
  for (auto& d : data::MONSTERS) if (d.key == key && !d.boss) known = true;
  if (!known) { nemesis_met = true; return; }
  P c = rnd.pick(far);
  auto m = spawn(key, c.x, c.y);
  if (!m) return;
  nemesis_met = true;
  unique(*m);
  std::string aff = nemesis.value("affix", std::string("Brutal"));
  if (std::find(m->mods.begin(), m->mods.end(), aff) == m->mods.end()) {  // the dead hero's element, in its blood now
    auto all = m->mods;
    m->mods = {aff};
    apply_mods(*m);
    all.push_back(aff);
    m->mods = all;
    assign_element(*m);
  }
  int kills = std::max(1, nemesis.value("kills", 1));  // each hero it slays makes it stronger
  m->maxhp = rnd_i(m->maxhp * (1 + .3 * (kills - 1))); m->hp = m->maxhp;
  m->lo = rnd_i(m->lo * (1 + .1 * (kills - 1))); m->hi = rnd_i(m->hi * (1 + .1 * (kills - 1)));
  std::string first = nemesis.value("name", std::string());
  if (first.empty()) first = m->name;
  auto slew = nemesis.value("slew", json::array());
  std::string hero = slew.empty() ? nemesis.value("hero", std::string("a hero")) : slew.back().get<std::string>();
  m->name = first + ", Slayer of " + hero;
  nemesis["name"] = first;
  m->nemesis = true;
  for (auto& it : nemesis.value("items", json::array())) if (auto i = loot::load(it)) m->carried.push_back(i);
}

std::string Game::nemesis_warning() const {
  if (!nemesis.is_object() || nemesis_met) return "";
  std::string what;
  auto items = nemesis.value("items", json::array());
  if (!items.empty()) what = " It carries " + items.back().value("name", std::string("what was theirs")) + ".";
  return "Ebb lowers his voice. \"The thing that killed " + nemesis.value("hero", std::string("your kin")) + " walks in " +
         nemesis.value("zname", std::string("the dark")) + " now." + what + " Be ready, or go another way.\"";
}

// ------------------------------------------------------------------ the Frost ladder: its level, its omens, its bite
int Game::res_held(const Hero& h) const {
  Stats st = stats(h);
  auto s = [&](const std::string& k) { int t = 0; for (auto& [slot, it] : h.equip) t += it->stat(k); return std::min(75, t); };
  return (st.res_f + st.res_c + st.res_p + s("res_ash") + s("res_blood")) / 5;
}

std::string Game::threshold(const Hero& h, int f) const {
  if (f < 0) f = frost;
  int bite = 10 * std::min(f, 5);
  return "The cold here bites " + std::to_string(bite) + "%. You hold " + std::to_string(res_held(h)) + "%.";
}

void Game::set_frost(int f) {
  frost = std::clamp(f, 0, 99);
  omens = omen::draw(seed, frost);
}

MonP Game::spawn(const std::string& key, int x, int y) {
  auto m = std::make_shared<Mon>();
  const MonDef& d = mon_def(key);
  m->id = mon_id_++; m->def = &d; m->x = x; m->y = y;
  m->name = d.name; m->col = d.col; m->lo = d.lo; m->hi = d.hi; m->acc = d.acc; m->ar = d.ar; m->spd = d.spd;
  m->xp = d.xp * 2;
  m->rank = d.boss ? "boss" : "normal";
  double k = 1 + .6 * (std::max<size_t>(1, living().size()) - 1);
  double angry = d.boss && omen(omen::ANGRY) ? 1.3 : 1.0;      // Great foes wake angry
  double lone = !d.boss && omen(omen::LONEWOLVES) ? 1.35 : 1.0;  // Lone Wolves: every foe much stronger
  m->maxhp = rnd_i(d.hp * k * frost_life() * angry * lone); m->hp = m->maxhp;
  if (frost > 0) {  // the deeper the frost, the harder they come; their resistances carry the rest
    double b = frost_blow() * (angry > 1 ? 1.1 : 1.0) * (lone > 1 ? 1.15 : 1.0);
    m->lo = rnd_i(m->lo * b); m->hi = rnd_i(m->hi * b);
    m->acc += 3 * frost; m->ar += frost;
    m->xp = rnd_i(m->xp * (1 + .3 * frost));
  }
  m->energy = rnd.below(100);
  if (d.has("sleeper")) m->dormant = 1;
  if (d.has("disguise")) m->hidden = 1;
  floor->mons.push_back(m);
  assign_element(*m);
  return m;
}

MonP Game::spawn_near(const std::string& key, P c) {
  P s;
  if (!free_near(c.x, c.y, 3, s, [&](int a, int b) { int t = floor->tile(a, b); return t == FLOOR || t == RUBBLE || t == CRUST; })) return nullptr;
  return spawn(key, s.x, s.y);
}

void Game::apply_mods(Mon& m) {
  for (auto& a : m.mods) {
    if (a == "Swift") m.spd = rnd_i(m.spd * 1.5);
    else if (a == "Brutal") { m.lo = rnd_i(m.lo * 1.5); m.hi = rnd_i(m.hi * 1.5); }
    else if (a == "Stone Skin") m.ar = m.ar * 2 + 4;
  }
}

void Game::champion(Mon& m, const std::vector<std::string>& mods) {
  m.rank = "champion";
  m.mods = mods;
  apply_mods(m);
  m.maxhp = rnd_i(m.maxhp * 2.2); m.hp = m.maxhp;
  m.lo = rnd_i(m.lo * 1.25); m.hi = rnd_i(m.hi * 1.25); m.acc += 8; m.xp *= 3;
  m.col = 0x7090FF;
  m.name = mods[0] + " " + m.def->name;
  assign_element(m);
}

void Game::unique(Mon& m) {
  m.rank = "unique";
  std::vector<std::string> all;
  for (auto& [k, v] : AFFIXES) all.push_back(k);
  rnd.shuffle(all);
  m.mods.assign(all.begin(), all.begin() + std::min<int>(all.size(), rnd.range(2, 3) + (omen(omen::KINGSCOLD) ? 1 : 0)));
  apply_mods(m);
  m.maxhp *= 4; m.hp = m.maxhp;
  m.lo = rnd_i(m.lo * 1.5); m.hi = rnd_i(m.hi * 1.5); m.acc += 12; m.ar += 2; m.xp *= 6;
  m.col = 0xE0B050;
  m.name = rnd.pick(UNIQUE_FIRST) + " " + rnd.pick(UNIQUE_EPITHET);
  assign_element(m);
}

void Game::next_floor() {
  for (auto& [at, pid] : floor->embers)  // an ember left lying in the dark goes out
    if (auto it = heroes.find(pid); it != heroes.end()) ember_out(*it->second, "it was left behind in the dark");
  floor->embers.clear();
  bool from_town = floor->depth == 0;  // down from Charnfen: the first map of the first zone
  bool was_camp = floor->camp && !from_town;
  // from a camp, the way Ebb opened (the one the party took) leads into the next zone
  if (from_town) zfloor = 0;
  else if (was_camp) {
    const ActDef& zone = ACTS[route.back()];
    route.push_back(chosen_ >= 0 ? chosen_ : zone.next.empty() ? route.back() : zone.next[0]);
    zfloor = 0;
    chosen_ = -1;
  } else ++zfloor;
  ++depth;  // Charnfen is depth 0, so the Crypt's first map is depth 1
  floor = make_floor(depth);
  scaled_ = 1.0;
  rescale();
  for (auto& [id, h] : heroes) {
    h->seen.assign(W * H, 0);
    if (int r = h->tal("sense"))  // Treasure Sense: the chests (and shrines) of a new floor are known at once
      for (int i = 0; i < W * H; ++i) if (floor->t[i] == CHEST || (r >= 2 && floor->t[i] == SHRINE)) h->seen[i] = 1;
  }
  for (auto& [id, h] : heroes) {
    h->gone = false; h->shopping = false; h->drank = false; h->talking.clear();
    if (!h->away && !h->lost) place(*h);  // the living, and the ghosts who walk with them
  }
  if (zfloor == 0) {
    auto& open = lore::ACT_OPEN[floor->act];
    say_all(open[0] + ": " + open[1], 0xFFD27F);
  }
  say_all("Depth " + std::to_string(depth) + ". " + ACTS[floor->act].name + ".", 0xA0A0A0);
  look();
}

// ------------------------------------------------------------------ the debug menu's tools, for testing
void Game::debug(Hero& h, const std::string& w) {
  if (w == "talk healer" || w == "talk smith" || w == "talk guide") {  // a helper's window without walking to them
    h.talking = w.substr(5);
    const auto& lines = h.talking == "healer" ? lore::YSOLDE : h.talking == "smith" ? lore::BRANN : lore::EBB;
    h.talk_line = h.talking == "guide" && floor->depth == 0 ? lore::EBB_TOWN : lines[0];
  } else if (w == "heal") {
    Stats st = stats(h);
    h.hp = st.maxhp; h.mana = st.maxmana; h.poison = 0; h.dead = false;
    h.say("[debug] Healed in full.", 0x80C0FF);
  } else if (w == "god") {
    h.god = !h.god;
    h.say(std::string("[debug] Harm ") + (h.god ? "off: nothing hurts you." : "on again."), 0x80C0FF);
  } else if (w == "reveal") {
    for (int i = 0; i < W * H; ++i) h.seen[i] = 1;
    ++floor->version;
    h.say("[debug] The whole map is shown.", 0x80C0FF);
  } else if (w == "level") {
    int was = h.lvl;
    for (int i = 0; i < 2000 && h.lvl == was; ++i) gain_xp(h, 40);
  } else if (w == "gold") {
    h.gold += 500;
    h.say("[debug] 500 gold.", 0x80C0FF);
  } else if (w == "legendary") {
    if (int(h.pack.size()) >= pack_max(h)) { h.say("[debug] Your pack is full.", 0x80C0FF); return; }
    auto it = loot::legendary(rnd, rnd.coin() ? h.cls->kinds : std::vector<std::string>{}, frost_band());
    h.pack.push_back(it);
    h.say("[debug] " + it->name + " in your pack.", data::rarity_col("legendary"));
  } else if (w == "next" || w == "boss") {
    if (w == "boss" && !floor->rest) zfloor = std::max(zfloor, ACTS[route.back()].floors - 2);
    say_all("[debug] On to the next map.", 0x80C0FF);
    next_floor();
  }
}

// ------------------------------------------------------------------ a step of the world
bool Game::free_action(Hero& h, const json& a) {
  std::string t = a.value("t", "");
  if (t == "perk") { choose_perk(h, a.value("k", "")); return true; }
  if (t == "reroll") { reroll_perks(h); return true; }
  if (t == "buy") { buy(h, a.value("i", -1)); return true; }
  if (t == "buyp") { buy_potion(h, a.value("k", "")); return true; }
  if (t == "sell") { sell(h, a.value("id", 0)); return true; }
  if (t == "close") { h.shopping = false; return true; }
  if (t == "talk") { talk(h, a); return true; }
  if (t == "debug") { debug(h, a.value("w", "")); return true; }
  if (t == "path" || t == "slot" || t == "slotel") { fire_action(h, t, a); return true; }
  if (t == "talents") {  // the Pyre by Charnfen's great fire: the gifts change before the hero goes down
    if (floor->depth != 0 || !a.contains("v") || !a["v"].is_object()) return true;
    std::map<std::string, int> want;
    for (auto& [k, v] : a["v"].items()) if (v.is_number_integer()) want[k] = v.get<int>();
    int credit = h.tal("credit");
    h.talents = pyre::clean(want);
    h.heal = std::max(0, h.heal + 2 * (h.tal("credit") - credit));
    refit(h);
    Stats st = stats(h);
    h.hp = st.maxhp; h.mana = st.maxmana;
    h.say("The Pyre's warmth settles in you.", 0xFFB040);
    return true;
  }
  return false;
}

void Game::step(const std::map<int, json>& actions) {
  if (state != "play") return;
  fx.clear();
  ++turn;
  std::vector<int> order;
  for (auto& [id, h] : heroes) order.push_back(id);
  for (int id : order) {
    auto it = heroes.find(id);
    if (it == heroes.end() || !it->second->present()) continue;
    auto a = actions.find(id);
    Hero& hh = *it->second;
    if (hh.dazed > 0 && hh.perk("unyielding")) hh.dazed = 0;  // Unyielding: nothing holds it
    if (hh.dazed > 0) { --hh.dazed; hh.say("You reel, and lose the turn.", 0xC0A0E0); continue; }
    if (!hh.carry.empty() && turn % 3 == 0 && !hh.perk("emberbearer")) { num(hh.x, hh.y, "ember", elem::COL[elem::EMBER]); continue; }  // its weight
    act(hh, a == actions.end() ? json{{"t", "wait"}} : a->second);
    if (state != "play") return;
  }
  embers_tick();
  check_stairs();
  if (floor->camp) {  // their master fallen, the last of the boss's kin slink off into the dark: the camp is safe ground
    auto& ms = floor->mons;
    size_t was = ms.size();
    ms.erase(std::remove_if(ms.begin(), ms.end(), [&](const MonP& m) { return m != floor->boss && !m->dormant; }), ms.end());
    if (ms.size() != was) say_all("With their master fallen, the last of them slink off into the dark.", 0xA0A0A0);
  }
  if (!floor->resting() || floor->depth == 0) monsters();
  tick();
  look();
  if (!floor->resting()) { wake_dead(); look(); }
  if (living().empty() && !heroes.empty() && state == "play") {
    state = "over";
    say_all("The dark takes the last of you. Charnfen waits in vain.", 0xFF6060);
  }
}

void Game::act(Hero& h, const json& a) {
  std::string t = a.value("t", "");
  struct Acting {
    Game& g;
    Acting(Game& game, Hero* h) : g(game) { g.acting_ = h; g.discharging_ = -1; g.acting_el_ = -1; g.credit_ = -1; g.credit_weapon_ = false; }
    ~Acting() { g.acting_ = nullptr; g.discharging_ = -1; g.acting_el_ = -1; g.credit_ = -1; g.credit_weapon_ = false; }
  } acting(*this, &h);
  int rime_legs = h.leg("kingsfrost") ? std::max(2, h.stk.n[elem::RIME]) : h.stk.n[elem::RIME];  // Kingsfrost Mail weighs like 2 stacks
  if (t == "move" && rime_legs > 0 && !h.perk("coldhands") && !h.perk("unyielding") && rnd.below(100) < std::min(30, 10 * rime_legs)) {
    h.say("Rime stiffens your legs; the step comes slow.", elem::COL[elem::RIME]);
    return;
  }
  if (t == "coal") { if (floor->depth == 0 || floor->resting()) take_coal(h, a.value("e", -1)); return; }
  if (t == "rack") { if (floor->depth == 0) take_weapon(h, a.value("w", "")); return; }
  if (t == "move") move(h, std::clamp(a.value("dx", 0), -1, 1), std::clamp(a.value("dy", 0), -1, 1));
  else if (t == "skill") skill(h, a.value("n", 0), a.value("x", 0), a.value("y", 0));
  else if (t == "shoot") shoot_with(h, a.value("x", 0), a.value("y", 0));
  else if (t == "quaff") quaff(h, a.value("k", ""));
  else if (t == "equip") equip(h, a.value("id", 0));
  else if (t == "drop") drop(h, a.value("id", 0));
  else if (t == "give") give(h, a.value("id", 0));
  else if (t == "down") descend(h);
  else if (t == "forge") forge(h);
  else if (t != "wait") free_action(h, a);
}

void Game::move(Hero& h, int dx, int dy) {
  if (!dx && !dy) return;
  int nx = h.x + dx, ny = h.y + dy;
  h.shopping = false;
  h.talking.clear();
  if (auto m = mon_at(nx, ny)) { melee(h, *m); return; }
  if (hero_at(nx, ny)) return;
  if (floor->has_merchant && floor->merchant == P{nx, ny}) {
    h.shopping = true;
    h.say("The trader nods. 'Dwarf gold spends the same as any.'", 0xD2B48C);
    return;
  }
  if (floor->depth == 0 && floor->tile(nx, ny) == GUARD) {  // the gate guard: which Frost to go down into (plan.md section 7)
    if (h.frost_open <= 0) { h.say("The guard nods you past. \"Mind the dead on the way down.\"", 0xA8C0E0); return; }
    h.talking = "gate";
    h.talk_line = "How deep, then? The cold below has grown since you last went down.";
    return;
  }
  if (int ht = floor->tile(nx, ny); ht == HEALER || ht == SMITH || ht == DIGGER) {  // the helpers: a word and their window
    const auto& lines = ht == HEALER ? lore::YSOLDE : ht == SMITH ? lore::BRANN : lore::EBB;
    h.talking = ht == HEALER ? "healer" : ht == SMITH ? "smith" : "guide";
    h.talk_line = ht == DIGGER && floor->depth == 0 ? lore::EBB_TOWN : lines[rnd.below(int(lines.size()))];
    if (ht == DIGGER && !nemesis_warning().empty()) {  // Ebb warns of the killer come back, before its zone
      int z = -1;
      for (int i = 0; i < ZONES; ++i) if (ACTS[i].key == nemesis.value("zone", std::string())) z = i;
      bool ahead = floor->depth == 0 ? z == 0 : false;
      if (floor->camp) for (int n : ACTS[floor->act].next) ahead = ahead || n == z;
      if (ahead) h.talk_line = nemesis_warning();
    }
    return;
  }
  switch (floor->tile(nx, ny)) {
    case DOOR: floor->set(nx, ny, OPEN); return;
    case CHEST: open_chest(h, nx, ny); return;
    case SHRINE: shrine(h, nx, ny); return;
    case TOMB: case CRACKED: case BIER: case RESTLESS: case FALLEN: case FLOATER: case EGG: smash_dead(h, nx, ny); return;
    case LEVER: pull_lever(h, nx, ny); return;
    case VAULTDOOR:
      if (floor->keys > 0) {
        --floor->keys;
        floor->set(nx, ny, OPENVAULT);
        say_all(h.name + " turns a heavy key, and the vault door grinds open.", 0xE0C060);
      } else {
        h.say("The vault door is locked. A brass-bound guardian keeps the key.", 0xC0A060);
        tip(h, "vault", "Locked vaults hold the best things. The brass guardians standing near them carry the keys.");
      }
      return;
    default: break;
  }
  if (!floor->walk(nx, ny)) return;
  if (h.held > 0 && h.perk("unyielding")) h.held = 0;
  if (h.held > 0) { h.say("You are held fast.", 0xE0C060); return; }
  h.x = nx; h.y = ny;
  h.moved = turn;
  h.focus = h.perk("skirmisher") ? std::max(0, h.focus - 1) : 0;
  arrive(h);
}

void Game::arrive(Hero& h) {
  int i = h.y * W + h.x;
  if (floor->tile(h.x, h.y) == GRATE) under_grate(h);
  auto w = floor->writings.find(i);
  if (w != floor->writings.end()) { h.say(w->second, 0xE6D2A0); floor->writings.erase(w); }
  for (size_t k = 0; k < floor->items.size();) {
    FloorItem& fi = floor->items[k];
    if (fi.x != h.x || fi.y != h.y || (fi.owner && fi.owner != h.pid)) { ++k; continue; }
    bool took = true;
    if (fi.gold > 0) {
      int g = fi.gold * (100 + 5 * h.tal("glint")) / 100;
      h.gold += g;
      num(h.x, h.y, "+" + std::to_string(g), 0xFFD700);
      h.say(std::to_string(g) + " gold.", 0xFFD700);
    }
    else if (!fi.pot.empty()) {
      if (fi.pot == "heal" && h.heal < belt(h)) { ++h.heal; h.say("A healing potion.", 0xFF7070); }
      else if (fi.pot == "mana" && h.mana_pots < belt(h)) { ++h.mana_pots; h.say("A mana potion.", 0x7090FF); }
      else took = false;
    } else if (fi.item) {
      if (int(h.pack.size()) >= pack_max(h)) { h.say("Your pack is full: " + fi.item->name + " stays here.", 0x909090); took = false; }
      else {
        h.pack.push_back(fi.item);
        Verdict v = compare(h, *fi.item);
        static const std::map<int, std::string> SAYS = {{2, " It is better than what you wear."}, {1, " It is mostly better than what you wear."},
                                                        {-1, " It is mostly worse than what you wear."}, {-2, " It is worse than what you wear."},
                                                        {-9, " It is not for a " + h.cls->name + "."}};
        std::string what = SAYS.count(v.v) ? SAYS.at(v.v) : "";
        if (v.v == 0 && !h.equip.count(fi.item->slot)) what = " You wear nothing like it.";
        h.say("You pick up " + fi.item->name + "." + what, rarity_col(fi.item->rarity));
        if (v.v > 0) tip(h, "better", "Open your pack with [i]: a green ▲ marks a thing better than what you wear.");
        tip(h, "pack", "Your pack: [i]. Wear or use the chosen thing with [Enter].");
      }
    }
    if (took) floor->items.erase(floor->items.begin() + k); else ++k;
  }
  int t = floor->tile(h.x, h.y);
  if (t == STAIRS) h.say("Stairs lead down. Press [>] to descend.", 0xFFE680);
  if (t == EDGE) h.say("The crust runs on into the dark. Press [>] to go on.", 0xFFE680);
  if (t == FOUNTAIN && !h.drank && omen(omen::BROKEN)) { h.drank = true; h.say("The fountain is cracked. Its water runs away into the stone.", 0x909090); }
  if (t == FOUNTAIN && !h.drank && h.leg("ashwalker")) { h.drank = true; h.say("The water runs grey off your boots. It does nothing for you.", 0x909090); }
  if (t == FOUNTAIN && !h.drank) {
    h.drank = true;
    Stats st = stats(h);
    h.hp = st.maxhp; h.mana = st.maxmana; h.poison = 0;
    h.say("You drink from the fountain. Your wounds close.", 0x6EBEFF);
    add_fx({{"k", "rise"}, {"x", h.x}, {"y", h.y}, {"col", 0x6EBEFF}});
  }
}

void Game::descend(Hero& h) {
  if (!way_down(floor->tile(h.x, h.y))) { h.say("There are no stairs here.", 0x909090); return; }
  if (floor->depth == 0 && !h.equip.count("weapon")) {
    h.say("A guard bars the way. \"Bare hands? Nobody goes down like that. The gatekeeper's rack, by the gate.\"", 0xD2B48C);
    return;
  }
  if (floor->depth == 0 && h.el < 0) {
    h.say("A guard bars the way with his crossbow. \"Nobody goes down cold. Take a coal from the great fire first.\"", 0xD2B48C);
    return;
  }
  if (floor->depth == 0 && !members.empty()) {  // a group goes down whole, or not at all
    std::string missing;
    for (auto& n : members) {
      bool here = false;
      for (auto& [id, o] : heroes) if (o->name == n && !o->away) here = true;
      if (!here) missing += (missing.empty() ? "" : ", ") + n;
    }
    if (!missing.empty()) { h.say("A guard shakes his head. \"Your group goes down together. Still missing: " + missing + ".\"", 0xD2B48C); return; }
  }
  if (!floor->embers.empty()) { h.say("A fallen friend's ember still lies on this map. Take it up first, or it goes out in the dark.", 0xFFA040); return; }
  if (floor->depth == 0 && omen(omen::BROKEN) && !h.omen_gift) {
    h.omen_gift = true;  // Broken Fountains: the healer sends you down with more
    h.heal += 2; h.mana_pots += 2;
    h.say("Ysolde presses two more of each into your belt. \"The fountains below are broken. You'll want these.\"", 0xAADCAA);
  }
  if (floor->depth == 0 && frost > 0) {  // the threshold, as the gate sees you go (plan.md section 7)
    h.say(threshold(h), 0x9CD2FF);
  }
  h.gone = true;
  if (!active().empty()) {
    h.say("You wait on the stairs for the others.", 0xA0A0A0);
    say_all(h.name + " waits at the stairs down.", 0xA0A0A0);
  }
}

void Game::check_stairs() {
  if (state != "play") return;
  auto alive = living();
  if (alive.empty()) return;
  for (auto* h : alive) if (!h->gone) return;
  if (!floor->embers.empty()) {  // a fallen friend's ember still lies here: the stairs wait until it is taken up
    auto it = heroes.find(floor->embers.front().second);
    std::string who = it == heroes.end() ? "a friend" : it->second->name;
    for (auto* h : alive) {
      h->gone = false;
      h->say(who + "'s ember still lies on this map. Take it up first, or it goes out in the dark.", 0xFFA040);
    }
    return;
  }
  if (!floor->exits.empty()) {
    // the ways Ebb opened: the party goes together, the way most of them stand on (the first hero's if it is even)
    std::map<int, int> votes;
    for (auto* h : alive) ++votes[floor->exits.count(h->y * W + h->x) ? floor->exits[h->y * W + h->x] : -1];
    int way = floor->exits.count(alive[0]->y * W + alive[0]->x) ? floor->exits[alive[0]->y * W + alive[0]->x] : -1;
    for (auto [z, n] : votes) if (n > votes[way]) way = z;
    bool split = false;
    for (auto* h : alive)
      if (!floor->exits.count(h->y * W + h->x) || floor->exits[h->y * W + h->x] != way) {
        h->gone = false;
        split = true;
        h->say("The others are going down to " + ACTS[way].name + ". Take the same stairs to go with them.", 0xD2B48C);
      }
    if (split) return;
    chosen_ = way;
  }
  next_floor();
}

// ------------------------------------------------------------------ fighting
// A foe with its mind on something else: winding up, stunned, frozen, spent, held, lost, caught off guard,
// or fighting another hero. The Cutthroat lives on these.
bool Game::busy(const Mon& m, const Hero& h) {
  if (m.blow.turns > 0 || m.stun > 0 || m.frozen > 0 || m.spent > 0 || m.pinned > 0 || m.lost > 0 || m.surprised > 0 || m.open > 0 || !m.awake)
    return true;
  for (auto* o : active()) if (o != &h && cheb(o->x, o->y, m.x, m.y) <= 1) return true;
  return false;
}

void Game::focus_up(Hero& h) {
  if (h.cls->key == "bow") h.focus = std::min(h.perk("sharpshooter") ? 5 : 3, h.focus + 1 + h.perk("steadyaim"));
}

void Game::bleed(Hero& h, Mon& m, int dmg) {
  if (m.can("stone")) return;
  bool poison = h.perk("poisoner") > 0;
  m.bleed = poison ? 5 : 3;
  m.bleed_dmg = std::max(m.bleed_dmg, std::max(1, poison ? dmg / 3 : dmg / 4));
}

bool Game::strike(Hero& h, Mon& m, double mult, bool ranged) {
  Stats st = stats(h);
  bool opening = h.cls->key == "dagger" && !ranged && busy(m, h);
  bool unseen = !m.awake;
  if (unseen) m.caught = true;  // for Long Night
  m.awake = true;
  m.open = 0;
  int ar = h.perk("festering") ? std::max(0, m.ar - m.stk.n[elem::ROT]) : m.ar;  // Festering: the rot eats its armour
  int chance = std::clamp(st.acc - ar * 2 + 12 + (opening ? 30 : 0) - 8 * h.stk.n[elem::ASH], 25, 97);
  if (h.perk("opener") && m.hp >= m.maxhp) chance = 100;
  if (rnd.below(100) >= chance) { h.say("You miss " + the(m) + ".", 0x909090); num(m.x, m.y, "miss", 0x808080); return false; }
  double d = rnd.range(st.lo, st.hi) * (1 + st.pct / 100.0) * mult;
  if (h.numb > 0) d *= .75;
  if (h.perk("berserker")) d *= 1 + .03 * h.fury;
  if (ranged && h.cls->key == "bow") d *= 1 + .2 * h.focus;
  if (m.frozen > 0 && h.perk("frostbinder")) d *= 1.25;
  if (m.stun > 0 && h.cls->key == "mace") d *= 1.25;  // the mace falls hardest on a foe that reels
  if (ranged && combo(h, 1, elem::RIME)) d *= 1 + .08 * m.stk.n[elem::RIME];  // Coldeye
  if (h.perk("duelist")) {
    if (h.combo_id == m.id) h.combo = std::min(6, h.combo + 1); else { h.combo_id = m.id; h.combo = 1; }
    d *= 1 + .12 * (h.combo - 1);
  }
  if (!ranged) h.fury = std::min(10, h.fury + 1);
  bool crit = opening || rnd.below(100) < st.crit;
  if (crit) { d *= opening ? 2.5 : h.leg("silentdirk") ? 3 : 2; add_fx({{"k", "crit"}, {"x", m.x}, {"y", m.y}}); }
  int dmg = std::max(1, rnd_i(d - ar / 2.0));
  if (ranged && m.can("wall")) { dmg = std::max(1, dmg / 2); tip(h, "wall", "A shield-bearer stops arrows and spells, for itself and whoever stands behind it. Close in."); }
  if (st.fire > 0) dmg += m.def->fire ? st.fire / 2 : st.fire;
  add_fx({{"k", "blood"}, {"x", m.x}, {"y", m.y}, {"src", xy(h.x, h.y)}});
  if (st.leech > 0) h.hp = std::min(st.maxhp, h.hp + std::max(1, dmg * st.leech / 100));
  if (opening) {
    bleed(h, m, dmg);
    h.mana = std::min(st.maxmana, h.mana + 2);
    tip(h, "opening", "An opening! A busy foe takes a critical blow from you, and bleeds.");
  }
  if (h.tal("embers") && rnd.below(100) < 5 * h.tal("embers") && !m.can("stone")) {
    m.bleed = std::max(m.bleed, 3);
    m.bleed_dmg = std::max(m.bleed_dmg, std::max(2, dmg / 3));
    add_fx({{"k", "burst"}, {"x", m.x}, {"y", m.y}, {"r", 0}, {"col", 0xFF8C28}});
    num(m.x, m.y, "burning", 0xFF8C28);
  }
  // the mace's weight: a blow may stun (not a great foe)
  if (!ranged && h.cls->key == "mace" && m.rank != "boss" && rnd.below(100) < 20 + 4 * mastery(h, "mace") && !m.can("stone")) {
    m.stun = std::max(m.stun, 2);  // it reels a turn, and the next blow finds it still reeling
    num(m.x, m.y, "stunned", 0xE0D080);
    // the shock runs on: the foes beside the stunned one take a quarter of the blow (plan.md section 7)
    int shock = std::max(1, dmg / 4);
    auto ms = floor->mons;
    for (auto& o : ms)
      if (o.get() != &m && o->hp > 0 && cheb(o->x, o->y, m.x, m.y) == 1) hurt(*o, shock, &h);
  }
  if (!ranged && h.leg("kingsbane") && !m.can("stone")) { m.stun = std::max(m.stun, 1); num(m.x, m.y, "stunned", 0xE0D080); }  // Kingsbane: every blow
  if (!ranged && h.leg("pyrehand")) lay_on_mon(h, m, 1, elem::EMBER);  // Pyrehand: the gauntlets set it alight
  crit_next = crit;
  if (ranged && &h == acting_) {  // Smokestalker and Plaguehunter lay a second stack before the blow lays its own
    if (unseen && combo(h, 1, elem::ASH) && hero_el(h) == elem::ASH) lay_on_mon(h, m, 1);
    if (combo(h, 1, elem::ROT) && hero_el(h) == elem::ROT && cheb(m.x, m.y, h.x, h.y) >= std::max(1, st.reach - 1)) lay_on_mon(h, m, 1);
  }
  hurt(m, dmg, &h, opening ? "An opening: you cut " + the(m) + " deep (" + std::to_string(dmg) + ")."
                   : crit ? "You strike " + the(m) + " hard (" + std::to_string(dmg) + ")." : "");
  return true;
}

void Game::melee(Hero& h, Mon& m) {
  add_fx({{"k", "swing"}, {"src", xy(h.x, h.y)}, {"cells", json::array({xy(m.x, m.y)})}});
  credit_ = 0;  // a blow in close is Melee, whatever it is struck with
  credit_weapon_ = true;
  int reach = stats(h).reach;
  int mx = m.x, my = m.y, id = m.id;
  if (h.leg("kingsbane") && ++h.lift >= 3) {  // Kingsbane: every third swing goes into lifting it again
    h.lift = 0;
    h.say("You heave the great maul up again.", 0xB0A890);
    return;
  }
  strike(h, m, reach >= 5 ? .5 : 1.0);
  if (h.leg("lastspear") && m.hp > 0 && h.cls->key == "spear") strike(h, m, reach >= 5 ? .5 : 1.0);  // the Last Dwarf's Spear: twice
  if (h.perk("skirmrhythm")) h.skirm = true;  // Skirmisher's Rhythm: the next shot comes harder
  // the axe bites a second foe beside you (and anyone's blow with Sweeping Blows)
  auto ms = floor->mons;
  if (h.cls->key == "axe" || h.perk("sweeping"))
    for (auto& o : ms)
      if (o->hp > 0 && o->id != id && cheb(o->x, o->y, h.x, h.y) == 1 && cheb(o->x, o->y, mx, my) <= 1) {
        add_fx({{"k", "swing"}, {"src", xy(h.x, h.y)}, {"cells", json::array({xy(o->x, o->y)})}});
        strike(h, *o, h.cls->key == "axe" ? .5 + .05 * mastery(h, "axe") : .5);
        break;
      }
}

void Game::hurt(Mon& m, int dmg, Hero* by, const std::string& text) {
  if (by && !by->perks.empty()) dmg = std::max(1, rnd_i(dmg * perk_harm(*by, m)));
  dmg = trait_hurt(m, dmg);
  if (m.spent > 0) dmg = dmg * 3 / 2;
  if (m.smothered > 0) dmg = dmg * 5 / 4;
  m.hp -= dmg;
  num(m.x, m.y, std::to_string(dmg), crit_next ? 0xFFD25A : 0xFFFFFF);
  if (by) {  // learn by doing: the harm counts for the style (and the weapon) that dealt it
    auto& w = m.work[by->pid];
    w[credit_ >= 0 ? credit_ : main_style(*by)] += dmg;
    if (credit_weapon_) w[3] += dmg;
    if (acting_el_ >= 0 && acting_el_ == by->el2) w[4] += dmg;
  }
  if (by && by == acting_ && m.hp > 0) {  // the hero's own hit: it lays the hero's element, or a heavy skill sets it off
    Hero* h = acting_;
    acting_ = nullptr;
    if (discharging_ >= 0 && m.stk.n[discharging_] > 0) discharge_mon(*h, m, discharging_);
    else lay_on_mon(*h, m, (credit_ == 1 && h->leg("recurve") ? 2 : 1) + (omen(omen::EMBERTIDE) ? 1 : 0));  // the Webmother's Recurve: two stacks an arrow; Ember Tide
    acting_ = h;
    if (m.hp <= 0) return;
  }
  crit_next = false;
  if (by && m.hp > 0 && by->leg("headsman") && m.hp * (m.rank == "boss" ? 10 : 5) < m.maxhp) {  // Headsman's Promise (a great foe at a tenth)
    num(m.x, m.y, "beheaded", 0xE0B050);
    m.hp = 0;
  }
  m.awake = true;
  if (by && !text.empty()) by->say(text, 0xFFD25A);
  if (m.hp <= 0) { kill(m, by); return; }
  if (m.blow.turns > 0 && m.blow.kind == BLOW_PURPLE && (m.blow.taken += dmg) >= m.maxhp / 10) {
    m.blow = {};
    m.spent = 2;
    say_near(m.x, m.y, "You break " + the(m) + "'s spell! It reels, open to your blows.", 0xD0A0FF);
    for (auto* h : active()) if (h->sees(m.x, m.y)) tip(*h, "spent", "A reeling foe takes half again as much harm. Strike now.");
  }
  if (m.rank == "boss") boss_phase(m);
}

// ------------------------------------------------------------------ blows that are seen coming
// Each boss plays from a deck of moves, a new deck at two thirds and one third of its life.
const std::map<std::string, std::array<std::vector<std::string>, 3>> BOSS_DECKS = {
    {"gravewarden", {{{"smash", "slam", "summon", "smash"},
                      {"smash", "sweep", "summon", "line", "slam"},
                      {"sweep", "smash", "toll", "summon", "line"}}}},
    {"webmother", {{{"spit", "line", "summon", "smash"},
                    {"line", "smash", "summon", "toll", "spit"},
                    {"sweep", "line", "toll", "summon", "spit"}}}},
    {"furnace", {{{"flame", "smash", "summon", "line"},
                  {"line", "sweep", "flame", "summon", "smash"},
                  {"pyre", "line", "flame", "summon", "sweep"}}}},
    {"sluicekeeper", {{{"flood", "spit", "chain", "summon"},
                       {"flood", "chain", "smash", "summon", "spit"},
                       {"flood", "chain", "sweep", "flood", "summon", "spit"}}}},
    {"ashmayor", {{{"ashbreath", "smash", "summon", "line"},
                  {"ashbreath", "line", "summon", "smash", "slam"},
                  {"ashbreath", "sweep", "summon", "line", "toll"}}}},
    {"hoardwarden", {{{"ingot", "arms", "summon", "smash"},
                     {"ingot", "arms", "line", "summon", "ingot"},
                     {"ingot", "sweep", "arms", "pyre", "summon"}}}},
    {"ironcaptain", {{{"smash", "charge", "rally", "line"},
                     {"charge", "sweep", "rally", "line", "brace"},
                     {"brace", "smash", "charge", "sweep", "rally", "brace"}}}},
    {"headgaoler", {{{"chain", "smash", "summon", "chain"},
                    {"chain", "sweep", "summon", "line", "smash"},
                    {"chain", "sweep", "toll", "summon", "chain"}}}},
    {"prelate", {{{"void", "line", "summon", "smash"},
                 {"void", "toll", "line", "summon", "sweep"},
                 {"void", "pyre", "line", "toll", "summon"}}}},
    {"ashenking", {{{"nova", "smash", "line", "blink", "summon"},
                    {"toll", "smash", "line", "blink", "summon", "sweep"},
                    {"pyre", "toll", "line", "blink", "summon", "smash"}}}},
};
// how each blow looks: kind, turns to land, damage as a share of the foe's average hit, heavy (it is spent after)
struct BlowDef { int kind, turns; double mult; bool heavy; };
const std::map<std::string, BlowDef> BLOWS = {
    {"smash", {BLOW_RED, 2, 1.6, false}}, {"line", {BLOW_RED, 2, 1.6, false}}, {"sweep", {BLOW_RED, 2, 2.0, true}},
    {"toll", {BLOW_PURPLE, 3, 1.5, true}}, {"pyre", {BLOW_GOLD, 3, 2.2, true}},
    // the tricks of the common foes (traits.cpp): mostly one turn to step out of the way
    {"lob", {BLOW_RED, 1, 1.0, false}}, {"jar", {BLOW_RED, 2, 1.1, false}}, {"leap", {BLOW_RED, 1, 1.3, false}},
    {"hook", {BLOW_RED, 1, .6, false}}, {"reach", {BLOW_RED, 1, 1.3, false}}, {"swing", {BLOW_RED, 1, 1.4, false}},
    {"grab", {BLOW_RED, 1, .8, false}}, {"shriek", {BLOW_RED, 2, .6, false}}, {"bolt", {BLOW_RED, 1, 1.5, false}},
    // and the great foes' own
    {"web", {BLOW_RED, 2, .5, false}}, {"flood", {BLOW_RED, 2, 1.2, false}}, {"chain", {BLOW_RED, 1, .7, false}},
    {"ingot", {BLOW_RED, 2, 1.4, false}}, {"charge", {BLOW_RED, 1, 1.5, false}}, {"ashbreath", {BLOW_PURPLE, 2, .5, false}},
};
const std::map<std::string, std::string> TRICK_SAYS = {
    {"lob", "throws something high over the others."}, {"jar", "hurls a sealed jar."}, {"leap", "crouches to spring."},
    {"hook", "swings a hooked chain."}, {"reach", "levels its long weapon."}, {"swing", "draws back for a wide swing."},
    {"grab", "opens its iron door to you."}, {"shriek", "opens all its mouths."}, {"bolt", "takes aim along a line."},
    {"web", "spins a net of greasy threads."}, {"flood", "hauls on the sluice wheel. Black sludge comes in waves!"},
    {"chain", "swings his chain."}, {"ingot", "hurls a white-hot ingot."}, {"charge", "lowers the red-hot hammer and charges."},
    {"ashbreath", "draws a breath of ash. Break it, or the hall goes dark!"},
};
// what the players read when it starts, by foe; "" for anyone else
const std::map<std::string, std::map<std::string, std::string>> BLOW_SAYS = {
    {"gravewarden", {{"smash", "raises its grave-hammer."}, {"line", "drags its hammer back for a long swing."},
                     {"sweep", "whirls its hammer up. Get clear!"}, {"toll", "begins to toll a death-knell. Break it!"}}},
    {"webmother", {{"smash", "rears up over you."}, {"line", "draws back a lash of web."}, {"sweep", "curls all its legs in."},
                   {"toll", "begins to keen. Break it!"}}},
    {"furnace", {{"smash", "lifts a fist of slag."}, {"line", "opens its grate toward you."}, {"sweep", "glows white. Get clear!"},
                 {"pyre", "roars, and the whole hall begins to burn. Find the gold!"}}},
    {"ashmayor", {{"smash", "lifts the iron staff."}, {"line", "points the staff down the hall."}, {"sweep", "swings the staff round."},
                  {"toll", "calls on the dead of his city. Break it!"}}},
    {"hoardwarden", {{"smash", "brings the vault key down."}, {"line", "flings a stream of molten coin."}, {"sweep", "heaves the whole mound."},
                     {"pyre", "melts his hoard. Find the gold that is not molten!"}}},
    {"ironcaptain", {{"smash", "raises the warhammer."}, {"line", "points the hammer: the ranks surge."}, {"sweep", "swings the hammer round."}}},
    {"headgaoler", {{"smash", "lifts the broken manacle."}, {"line", "flings a chain along the floor."}, {"sweep", "whirls his chains."},
                    {"toll", "beats the manacle on the floor. Break it!"}}},
    {"prelate", {{"smash", "spreads his frozen hands."}, {"line", "opens his ribs at you."}, {"sweep", "turns, and the cold spreads."},
                 {"toll", "begins a black prayer. Break it!"}, {"pyre", "opens the void in his chest. Find the gold!"}}},
    {"ashenking", {{"smash", "raises the black blade."}, {"line", "points, and the frost gathers."}, {"sweep", "draws the cold around him."},
                   {"toll", "speaks a word of the old dwarves. Break it!"}, {"pyre", "opens his chest. The Pyreheart blazes. Find the gold!"},
                   // what he borrows from the great foes slain on the way down (plan.md section 7, The King learns)
                   {"flood", "spills black meltwater across the floor."}, {"chain", "flings a chain of frozen links."},
                   {"ingot", "hurls a shard of burning heart-iron."}, {"charge", "lowers the black blade and charges."},
                   {"ashbreath", "breathes the ash of the dead city. Break it, or the hall goes dark!"}}},
};
// The King learns (plan.md section 7): a new move at Frost 1, 4 and 7, and from every zone boss slain on the way down
// he borrows its heaviest trick, so the route makes each King fight different
const std::map<std::string, std::string> KING_BORROWS = {
    {"crypt", "slam"}, {"caves", "spit"}, {"forge", "flame"}, {"cistern", "flood"}, {"city", "ashbreath"},
    {"vaults", "ingot"}, {"barracks", "charge"}, {"prison", "chain"}, {"sanctum", "void"},
};

bool Game::wind_up(Mon& m, Hero& h, const std::string& name) {
  const BlowDef& bd = BLOWS.at(name);
  Blow b;
  b.name = name; b.kind = bd.kind; b.turns = bd.turns + h.perk("sixthsense"); b.heavy = bd.heavy;
  b.dmg = rnd_i((m.lo + m.hi) / 2.0 * bd.mult);
  b.elem = m.def->fire ? "fire" : m.def->cold ? "cold" : "";
  auto add = [&](int x, int y) { if (floor->walk(x, y)) b.cells.push_back({x, y}); };
  if (name == "smash") {
    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) add(h.x + dx, h.y + dy);
  } else if (name == "sweep") {
    for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx) if (dx || dy) add(m.x + dx, m.y + dy);
  } else if (name == "line") {
    if (h.x == m.x && h.y == m.y) return false;
    // past the hero, to the wall
    int tx = m.x + (h.x - m.x) * 8, ty = m.y + (h.y - m.y) * 8;
    for (auto&& [x, y] : tcod::BresenhamLine({m.x, m.y}, {tx, ty}).without_start()) {
      if (!floor->walk(x, y) || cheb(x, y, m.x, m.y) > 8) break;
      b.cells.push_back({x, y});
    }
  } else if (name == "toll") {
    b.cells.push_back({m.x, m.y});
  } else if (name == "pyre") {
    // a little safe ground near each hero, reachable before it lands
    for (auto* o : active()) {
      if (cheb(o->x, o->y, m.x, m.y) > 9) continue;
      std::vector<P> near;
      PMap<int> dist{{P{o->x, o->y}, 0}};
      std::deque<P> q{{o->x, o->y}};
      while (!q.empty()) {
        P c = q.front(); q.pop_front();
        int dc = dist[c];
        if (dc >= 2 && cheb(c.x, c.y, m.x, m.y) >= 3) near.push_back(c);
        if (dc == bd.turns - 1) continue;
        for (int dy = -1; dy <= 1; ++dy)
          for (int dx = -1; dx <= 1; ++dx) {
            P n{c.x + dx, c.y + dy};
            if (floor->walk(n.x, n.y) && !dist.count(n)) { dist[n] = dc + 1; q.push_back(n); }
          }
      }
      if (near.empty()) near.push_back({o->x, o->y});
      P c = rnd.pick(near);
      for (int dy = 0; dy <= 1; ++dy) for (int dx = 0; dx <= 1; ++dx) add(c.x + dx, c.y + dy);
    }
  } else if (!trait_cells(m, h, name, b)) return false;
  if (b.cells.empty()) return false;
  m.blow = b;
  std::string says = "rears back for a heavy blow.";
  auto bs = BLOW_SAYS.find(m.def->key);
  if (bs != BLOW_SAYS.end() && bs->second.count(name)) says = bs->second.at(name);
  else if (TRICK_SAYS.count(name)) says = TRICK_SAYS.at(name);
  int col = b.kind == BLOW_RED ? 0xFF7060 : b.kind == BLOW_PURPLE ? 0xD0A0FF : 0xFFD060;
  say_near(m.x, m.y, cap(the(m)) + " " + says, col);
  if (m.discharger || m.rank != "normal")
    for (auto* o : active())
      for (int e = 0; e < elem::N; ++e)
        if (o->stk.n[e] > 0 && sets_off(m, e) && o->sees(m.x, m.y)) {
          o->say("! " + cap(the(m)) + " glows " + elem::NAME[e] + ": step away before your stacks go off.", elem::COL[e]);
          break;
        }
  for (auto* o : active()) {
    if (!o->sees(m.x, m.y)) continue;
    if (b.kind == BLOW_RED) tip(*o, "red", "Red ground: a heavy blow lands there when the count runs out. Step off it.");
    if (b.kind == BLOW_PURPLE) tip(*o, "purple", "Purple: it is gathering a spell. Stun it, or hurt it hard, to break it.");
    if (b.kind == BLOW_GOLD) tip(*o, "gold", "Gold ground: the only safe place from what comes. Stand in it.");
  }
  return true;
}

void Game::land(Mon& m) {
  struct Landing { bool& f; Landing(bool& x) : f(x) { f = true; } ~Landing() { f = false; } } landing(landing_);
  Blow b = m.blow;
  m.blow = {};
  if ((b.name == "smash" || b.name == "sweep") && floor->act != DEEP) add_fx({{"k", "quake"}, {"x", m.x}, {"y", m.y}, {"dur", 1.8}});
  m.brace = 0;  // a raised shield comes down to strike
  auto on = [&](Hero* o) { for (auto& c : b.cells) if (c.x == o->x && c.y == o->y) return true; return false; };
  std::vector<Hero*> hit;
  for (auto* o : active()) {
    bool in = b.kind == BLOW_RED ? on(o)
              : b.kind == BLOW_PURPLE ? o->sees(m.x, m.y)
                                 : cheb(o->x, o->y, m.x, m.y) <= 9 && !on(o);
    if (in) hit.push_back(o);
  }
  int col = b.kind == BLOW_RED ? 0xDC5A3C : b.kind == BLOW_PURPLE ? 0xB070FF : 0xFFB040;
  if (b.name == "line") add_fx({{"k", "bolt"}, {"cells", cells_json(b.cells)}, {"col", col}, {"ch", "*"}, {"src", xy(m.x, m.y)}});
  else if (b.name == "sweep") add_fx({{"k", "wave"}, {"x", m.x}, {"y", m.y}, {"r", 2}, {"col", col}, {"sign", "*"}});
  else if (b.kind == BLOW_RED) {
    int cx = 0, cy = 0;
    for (auto& c : b.cells) { cx += c.x; cy += c.y; }
    add_fx({{"k", "burst"}, {"x", rnd_i(double(cx) / b.cells.size())}, {"y", rnd_i(double(cy) / b.cells.size())}, {"r", 1}, {"col", col}});
  }
  else add_fx({{"k", "burst"}, {"x", m.x}, {"y", m.y}, {"r", 6}, {"col", col}, {"hollow", true}});
  // a seen blow never kills from full life
  if (TRICK_SAYS.count(b.name)) trait_land(m, b, hit);
  else for (auto* o : hit) hero_hurt(*o, &m, std::min(b.dmg, stats(*o).maxhp * 11 / 20), b.elem);
  if (hit.empty() && !TRICK_SAYS.count(b.name)) say_near(m.x, m.y, cap(the(m)) + " strikes nothing.", 0xA0A0A0);
  // a heavy blow sets off the stacks of its element on everyone it hits
  bool sets_off = m.discharger || (m.rank != "normal" && (b.heavy || b.name == "smash" || b.name == "charge" || b.name == "ingot"));
  if (sets_off)
    for (auto* o : hit) {
      if (o->dead) continue;
      int e = m.el;
      // a zone boss past two thirds of its life sets off whichever of its two elements you carry most of
      if (m.rank == "boss" && m.el2 >= 0 && m.phase >= 1 && o->stk.n[m.el2] > o->stk.n[m.el]) e = m.el2;
      discharge_hero(m, *o, e);
    }
  if (b.heavy) {
    m.spent = 2;
    say_near(m.x, m.y, cap(the(m)) + " is spent, and stands open.", 0xE0D080);
    for (auto* o : active()) if (o->sees(m.x, m.y)) tip(*o, "spent", "A spent foe takes half again as much harm. Strike now.");
  }
}

void Game::boss_phase(Mon& m) {
  int ph = m.hp * 3 <= m.maxhp ? 2 : m.hp * 3 <= 2 * m.maxhp ? 1 : 0;
  bool fourth = m.def->key == "ashenking" && frost >= 10 && m.hp * 7 <= m.maxhp;  // from Frost 10 the King has a fourth phase
  if (fourth) ph = 3;
  if (ph <= m.phase) return;
  m.phase = ph;
  m.deck.clear();
  m.cool = 1;
  if (ph == 3) {
    say_all("The Ashen King tears the Pyreheart half out of his chest. Its light and its cold pour over the hall: he holds nothing back now.", 0xFF8C28);
    add_fx({{"k", "wave"}, {"x", m.x}, {"y", m.y}, {"r", 4}, {"col", 0xFF8C28}, {"sign", "*"}});
    m.spd = m.spd * 5 / 4;
    m.stk = {};
    Hero* h = nearest_hero(m, false);
    if (h) boss_move(m, *h, cheb(h->x, h->y, m.x, m.y), "hollowcourt");
    return;
  }
  say_near(m.x, m.y, cap(the(m)) + (ph == 1 ? " staggers, and fights harder." : " is badly hurt. Now it holds nothing back."), 0xFFB060);
  if (ph == 1 && m.el2 >= 0) {  // its second element wakes (plan.md section 8)
    static const char* WAKES[elem::N] = {"begin to glow like coals", "turn white with frost", "trail black soot",
                                         "drip with rot", "run red"};
    say_near(m.x, m.y, cap(the(m)) + "'s blows " + WAKES[m.el2] + ": now they lay " + elem::NAME[m.el2] + " too.", elem::COL[m.el2]);
    add_fx({{"k", "burst"}, {"x", m.x}, {"y", m.y}, {"r", 1}, {"col", elem::COL[m.el2]}});
  }
  Hero* h = nearest_hero(m, false);
  if (h) boss_move(m, *h, cheb(h->x, h->y, m.x, m.y), "summon");
}

void Game::kill(Mon& m, Hero* by) {
  if (m.rank == "boss" && omen(omen::REMEMBERS) && !m.reborn) {  // the Pyre Remembers: a zone boss gets up once more
    m.reborn = true;
    m.hp = std::max(1, m.maxhp * 2 / 5);
    m.phase = 1;
    m.deck.clear();
    m.stk = {};
    add_fx({{"k", "wave"}, {"x", m.x}, {"y", m.y}, {"r", 2}, {"col", 0xFF8C28}, {"sign", "*"}});
    say_all("The Pyre remembers " + the(m) + ". It will not fall yet: it rises, burning.", 0xFFB060);
    return;
  }
  m.hp = 0;
  MonP keep;
  for (auto it = floor->mons.begin(); it != floor->mons.end(); ++it)
    if (it->get() == &m) { keep = *it; floor->mons.erase(it); break; }
  if (int r = m.stk.n[elem::ROT]; r > 0) {  // Blightreaver: the rot goes to the neighbours; Rotspeaker: to the nearest foe
    auto it = heroes.find(m.stk.by[elem::ROT]);
    Hero* rh = it == heroes.end() ? nullptr : it->second.get();
    auto give = [&](Mon& o) { o.stk.n[elem::ROT] = std::min(elem::MAX, o.stk.n[elem::ROT] + r); o.stk.idle[elem::ROT] = 0; o.stk.by[elem::ROT] = rh->pid; };
    if (rh && combo(*rh, 0, elem::ROT)) {
      for (auto& o : floor->mons) if (o->hp > 0 && cheb(o->x, o->y, m.x, m.y) <= 1) give(*o);
    } else if (rh && combo(*rh, 2, elem::ROT)) {
      Mon* best = nullptr;
      int bd = 6;
      for (auto& o : floor->mons) if (o->hp > 0 && cheb(o->x, o->y, m.x, m.y) < bd) { bd = cheb(o->x, o->y, m.x, m.y); best = o.get(); }
      if (best) { give(*best); add_fx({{"k", "line"}, {"cells", json::array({xy(m.x, m.y), xy(best->x, best->y)})}, {"col", elem::COL[elem::ROT]}, {"dur", .3}}); }
    }
  }
  int col = m.rank == "champion" ? 0x7090FF : (m.rank == "unique" || m.rank == "boss") ? 0xE0B050 : 0xC8C8C8;
  say_near(m.x, m.y, cap(the(m)) + " dies.", col);
  if (Hero* w = stacks_by(m, elem::EMBER); w && w->perk("wildfire") && m.stk.n[elem::EMBER] > 0)  // Wildfire
    for (auto& o : floor->mons) if (o->hp > 0 && cheb(o->x, o->y, m.x, m.y) <= 1) { o->stk.n[elem::EMBER] = std::min(elem::MAX, o->stk.n[elem::EMBER] + 2); o->stk.idle[elem::EMBER] = 0; o->stk.by[elem::EMBER] = w->pid; }
  if (by) {
    ++by->kills;
    Stats bst = stats(*by);
    if (by->perk("stackthief")) {  // Stack Thief: the kill takes the heaviest stack off you
      int e = int(std::max_element(by->stk.n, by->stk.n + elem::N) - by->stk.n);
      if (by->stk.n[e] > 0) --by->stk.n[e];
    }
    int heal = 0;
    if (by->perk("carrion") && m.stk.n[elem::ROT] > 0) heal += 3;
    if (by->perk("ironoath")) heal += std::max(1, bst.maxhp / 20);
    by->kill_turn = turn;
    if (by->leg("hungering")) heal += std::max(1, bst.maxhp / 10);  // Hungering Edge: it feeds
    if (by->perk("deathwish")) heal /= 2;
    if (heal > 0) by->hp = std::min(bst.maxhp, by->hp + heal_amt(*by, heal));
    int mana = 0;
    if (by->perk("deepwell2")) mana += std::max(1, bst.maxmana / 10);
    if (by->perk("quiver") && credit_ == 1) mana += 2;
    if (by->perk("spellblade") && credit_ == 0) mana += std::max(1, bst.maxmana / 10);
    if (mana > 0) by->mana = std::min(bst.maxmana, by->mana + mana);
    if (by->perk("blood")) by->hp = std::min(stats(*by).maxhp, by->hp + 3 * by->perk("blood"));
    if (by->tal("heat") && by->heat_floor != floor->id) {  // Heat of Battle
      by->heat_floor = floor->id;
      Stats st = stats(*by);
      by->mana = std::min(st.maxmana, by->mana + std::max(2, st.maxmana / 5));
    }
    if (int r = by->tal("pyrewake")) {  // Pyre Wake: the dead flare up and burn their neighbours
      add_fx({{"k", "burst"}, {"x", m.x}, {"y", m.y}, {"r", 1}, {"col", 0xFF8C28}});
      std::vector<MonP> near;
      for (auto& o : floor->mons) if (cheb(o->x, o->y, m.x, m.y) <= 1) near.push_back(o);
      for (auto& o : near) if (o->hp > 0) hurt(*o, rnd.range(2, 4) * r + lvl / 2, nullptr, "");
    }
  }
  if (m.has("Fire-blooded")) {
    add_fx({{"k", "burst"}, {"x", m.x}, {"y", m.y}, {"r", 1}, {"col", 0xFF8C28}});
    for (auto* h : active()) if (cheb(h->x, h->y, m.x, m.y) <= 1) hero_hurt(*h, nullptr, 4 + lvl, "fire");
  }
  if (m.def->undead) add_fx({{"k", "soul"}, {"x", m.x}, {"y", m.y}, {"dur", 2.2}});  // what held it up lets go
  learn(m);
  int xp = m.caught && omen(omen::LONGNIGHT) ? m.xp * 2 : m.xp;  // Long Night: slain before it woke
  if (!m.risen) for (auto* h : living()) gain_xp(*h, xp);
  trait_death(m);
  if (!m.risen) drops(m);
  if (m.nemesis) {  // the killer falls: what it took comes back
    nemesis_beaten = true;
    for (auto& c : m.carried) {
      P at;
      if (!free_near(m.x, m.y, 2, at)) at = {m.x, m.y};
      floor->items.push_back({at.x, at.y, c, 0, "", 0});
    }
    say_all("The slayer of " + nemesis.value("hero", std::string("your kin")) + " falls at last. What it took lies where it fell.", 0xE0B050);
  }
  // the dead will not lie: one in six of the ordinary dead gets up once more, at half its life
  if (omen(omen::RISE) && m.rank == "normal" && !m.risen && !m.summoned && !m.def->boss && rnd.below(6) == 0 && state == "play") {
    P c;
    if (free_near(m.x, m.y, 1, c))
      if (auto r = spawn(m.def->key, c.x, c.y)) {
        r->risen = true; r->awake = true; r->dormant = r->hidden = r->sleep = 0; r->xp = 0;
        r->hp = r->maxhp = std::max(1, r->maxhp / 2);
        r->col = 0x9AA6B8;
        say_near(c.x, c.y, cap(the(*r)) + " will not lie: it rises again.", 0x9CD2FF);
      }
  }
  if (floor->boss.get() == &m) {
    slain.push_back(ACTS[floor->act].key);
    say_all(lore::BOSS_DIES.at(m.def->key), 0xFFD27F);
    if (final_floor()) {
      state = "won";
      for (auto& l : lore::VICTORY) say_all(l, 0xFFD27F);
    } else make_camp(*floor, {m.x, m.y});
  }
}

double Game::catch_up(const Hero& h) const {
  double target = 1 + 11.0 * depth / 17.0, gap = target - h.lvl;
  return std::clamp(std::pow(2.0, gap / 2), .5, 2.0);
}

void Game::gain_xp(Hero& h, int xp) {
  // about two levels a zone, ~12 by the Ashen King (plan.md section 1, Pace); a hero behind that learns up to twice as
  // fast and one ahead slower, enough for a friend who joined late
  h.xp += std::max(1, rnd_i(xp * catch_up(h)));
  while (h.xp >= need(h.lvl)) {
    ++h.lvl;
    Stats st = stats(h);
    h.hp = std::min(st.maxhp, h.hp + st.maxhp * 3 / 10 + 2 * h.cls->hp_lvl);
    h.mana = st.maxmana;
    h.say("You reach level " + std::to_string(h.lvl) + "!", 0xFFE680);
    add_fx({{"k", "rise"}, {"x", h.x}, {"y", h.y}, {"col", 0xFFE680}});
    offer_perk(h, true);
  }
}

// a choice of three perks the hero has not filled yet: at each level, and at each second rank of a style. From level 15
// (or on any Frost level) the deep perks take over, and every 5th level from 15 offers two keystones (plan.md section 7)
void Game::offer_perk(Hero& h, bool level) {
  if (h.lvl >= 15 || frost >= 1) {
    bool keystone = level && h.lvl >= 15 && h.lvl % 5 == 0;
    auto offer = perk_offer(h, keystone);
    if (offer.size() > (keystone ? 1u : 0u)) {
      h.perk_choices.push_back(offer);
      tip(h, keystone ? "keystone" : "deepperk", keystone ? "A keystone: great power at a price. Choose one, or neither: [p]." : "You may learn a deep perk: [p].");
      return;
    }
  }
  std::vector<std::string> pool;
  for (auto& p : PERKS) {
    if (!p.cls.empty() || !p.path.empty() || !p.kind.empty() || h.perk(p.key) >= p.max) continue;  // the deep perks and keystones come only later
    bool offered = false;
    for (auto& c : h.perk_choices) for (auto& k : c) if (k == p.key) offered = true;
    if (!offered || h.perk(p.key) + 1 < p.max) pool.push_back(p.key);
  }
  rnd.shuffle(pool);
  if (pool.size() > 3) pool.resize(3);
  if (!pool.empty()) { h.perk_choices.push_back(pool); tip(h, "perk", "You may learn a perk: [p]."); }
}

void Game::choose_perk(Hero& h, const std::string& k) {
  if (h.perk_choices.empty()) return;
  auto& ch = h.perk_choices.front();
  if (std::find(ch.begin(), ch.end(), k) == ch.end()) return;
  h.perk_choices.erase(h.perk_choices.begin());
  if (k == "neither") { h.say("You let the keystones lie.", 0xB4A078); return; }
  h.perks[k] = h.perk(k) + 1;
  h.say("You learn " + perk_def(k).name + ".", 0xFFE680);
  if (k == "onecraft" && h.el2 >= 0) { h.say(std::string("Your second coal gutters out: only ") + elem::NAME[h.el] + " is left in your hands.", elem::COL[h.el]); h.el2 = -1; }
  if (k == "glasspyre" || k == "packmule") { Stats st = stats(h); h.hp = std::min(h.hp, st.maxhp); }
}

void Game::drops(const Mon& m) {
  auto owners = living();
  if (owners.empty()) return;
  auto kinds = party_kinds();
  auto drop_at = [&](FloorItem fi) {
    P c;
    if (!free_near(m.x, m.y, 2, c, [&](int a, int b) {
          if (!floor->walk(a, b) || way_down(floor->tile(a, b))) return false;
          for (auto& i : floor->items) if (i.x == a && i.y == b) return false;
          return true;
        }))
      c = {m.x, m.y};
    fi.x = c.x; fi.y = c.y;
    floor->items.push_back(fi);
  };
  int rank_mul = m.rank == "champion" ? 3 : m.rank == "unique" ? 6 : m.rank == "boss" ? 15 : 1;
  double moon = omen(omen::BLOODMOON) ? 1.5 : 1.0;  // Blood Moon: everything drops more often
  if (rnd.real() < .35 * moon || m.rank != "normal")
    drop_at({0, 0, nullptr, rnd_i(lvl * rnd.range(2, 5) * rank_mul * (m.rank == "normal" ? 1.0 : .6) * (omen(omen::GOLDFEVER) ? 2 : 1)), "", rnd.pick(owners)->pid});
  if (rnd.real() < (m.rank == "normal" ? .14 : .5) * moon)
    drop_at({0, 0, nullptr, 0, rnd.real() < .65 ? "heal" : "mana", rnd.pick(owners)->pid});
  if (m.rank == "boss") {
    // the great foes give each hero two things worth having
    for (auto* o : owners) {
      drop_at({0, 0, roll_loot(lvl + 1, o->cls->kinds, "", "rare"), 0, "", o->pid});
      drop_at({0, 0, roll_loot(lvl + 1, kinds, "", "magic"), 0, "", o->pid});
    }
    if (omen(omen::REMEMBERS) || rnd.real() < .3) drop_at({0, 0, roll_loot(lvl, kinds, "unique"), 0, "", rnd.pick(owners)->pid});
    if (frost >= 5 && rnd.real() < (omen(omen::REMEMBERS) ? .2 : .1)) drop_at({0, 0, roll_loot(lvl, kinds, "legendary"), 0, "", rnd.pick(owners)->pid});
    return;
  }
  std::vector<ItemP> items;
  if (m.rank == "unique") { items.push_back(roll_loot(lvl, kinds, "", "rare")); items.push_back(roll_loot(lvl, kinds, "", "magic")); }
  else if (m.rank == "champion") { if (omen(omen::HUNGRYDARK)) items.push_back(roll_loot(lvl, kinds, "", "rare")); else if (m.def->gear || rnd.real() < .4 * moon) items.push_back(roll_loot(lvl, kinds, "", "magic")); }
  else if (m.def->gear && rnd.real() < .12 * moon) items.push_back(roll_loot(lvl, kinds));
  for (auto& it : items) drop_at({0, 0, it, 0, "", rnd.pick(owners)->pid});
}

void Game::open_chest(Hero& h, int x, int y) {
  floor->set(x, y, OPENCHEST);
  h.say("You open the chest.", 0xC8963C);
  auto spot = [&](P& c) {
    return free_near(x, y, 2, c, [&](int a, int b) {
      if (!floor->walk(a, b) || way_down(floor->tile(a, b))) return false;
      for (auto& i : floor->items) if (i.x == a && i.y == b) return false;
      return true;
    });
  };
  for (auto* o : living()) {  // something for each hero
    auto it = roll_loot(lvl, o->cls->kinds, rnd.real() < (o->perk("treasurenose") ? .75 : .45) ? "magic" : "");
    P c;
    if (spot(c)) floor->items.push_back({c.x, c.y, it, 0, "", o->pid});
  }
  P c;
  if (spot(c)) floor->items.push_back({c.x, c.y, nullptr, lvl * rnd.range(4, 10), "", h.pid});
}

void Game::shrine(Hero& h, int x, int y) {
  floor->set(x, y, USED);
  if (floor->act == SANCTUM && rnd.real() < .35) {
    // a shrine of the fallen god: it curses instead
    if (rnd.coin()) {
      for (auto* o : active()) { o->hp = std::max(1, o->hp - stats(*o).maxhp / 4); o->numb = 4; }
      say_all(h.name + " touches the altar. A cold voice answers, and frost bites into you all.", 0x9CD2FF);
    } else {
      for (int i = 0; i < 3; ++i) { P c; if (free_near(x, y, 3, c)) if (auto z = spawn("zealot", c.x, c.y)) z->awake = true; }
      say_all(h.name + " touches the altar. Zealots come running at the sound.", 0xFF8C6A);
    }
    tip(h, "curse", "In the Sanctum some altars belong to the fallen god: they curse instead of bless.");
    add_fx({{"k", "wave"}, {"x", x}, {"y", y}, {"r", 3}, {"col", 0x9CD2FF}, {"sign", "*"}});
    return;
  }
  int strong = omen(omen::RESTLESS) ? 2 : 1;  // Restless Shrines: twice as strong, and loud
  if (omen(omen::RESTLESS)) {
    for (auto& m : floor->mons) if (m->hp > 0 && !m->dormant && !m->hidden && cheb(m->x, m->y, x, y) <= 12) { m->awake = true; m->sleep = 0; }
    say_all("The shrine's voice rolls through the halls. Everything near it wakes.", 0xFF8C6A);
  }
  switch (rnd.below(3)) {
    case 0:
      for (auto* o : active()) { o->hp = stats(*o).maxhp; o->poison = 0; }
      say_all(h.name + " touches the shrine: warmth runs through you all.", 0xBEA0FF);
      break;
    case 1:
      for (auto* o : active()) o->mana = stats(*o).maxmana;
      say_all(h.name + " touches the shrine: your minds fill with light.", 0xBEA0FF);
      break;
    default:
      for (auto* o : living()) gain_xp(*o, (10 * lvl + 10) * (1 + frost / 3) * strong);
      say_all(h.name + " touches the shrine: old knowledge settles in you.", 0xBEA0FF);
  }
  if (frost > 0) {  // deeper in the Frost the shrines bless harder: a while of fire in the blood (plan.md section 7)
    for (auto* o : active()) o->boon = std::max(o->boon, (20 + 4 * std::min(frost, 10)) * strong);
    say_all("The shrine's blessing burns on in you: your blows land harder for a while.", 0xBEA0FF);
  }
  add_fx({{"k", "wave"}, {"x", x}, {"y", y}, {"r", 3}, {"col", 0xBEA0FF}, {"sign", "*"}});
}

MonP Game::target(const Hero& h, int x, int y) {
  auto m = mon_at(x, y);
  return m && h.sees(x, y) ? m : nullptr;
}

void Game::path(const Hero& h, int tx, int ty, int range, bool through, std::vector<P>& cells, std::vector<MonP>& hit) {
  int dx = tx - h.x, dy = ty - h.y;
  double k = through ? double(range) / std::max(1, std::max(std::abs(dx), std::abs(dy))) : 1.0;
  int ex = h.x + rnd_i(dx * k), ey = h.y + rnd_i(dy * k);
  for (auto&& [x, y] : tcod::BresenhamLine({h.x, h.y}, {ex, ey}).without_start()) {
    if (cheb(x, y, h.x, h.y) > range) break;
    if (floor->opaque(x, y)) break;
    cells.push_back({x, y});
    if (auto m = mon_at(x, y)) { hit.push_back(m); if (!through || m->can("wall")) return; }
    if (!through && x == tx && y == ty) break;
  }
}

void Game::shoot(Hero& h, int x, int y) {
  if (!stats(h).bow) { h.say("You have no bow.", 0x909090); return; }
  if (!target(h, x, y)) return;
  std::vector<P> cells; std::vector<MonP> hit;
  path(h, x, y, 9, false, cells, hit);
  add_fx({{"k", "bolt"}, {"cells", cells_json(cells)}, {"col", 0xDCDCC8}, {"ch", "-"}, {"src", xy(h.x, h.y)}});
  if (!hit.empty()) strike(h, *hit[0], 1.0, true);
  focus_up(h);
}

void Game::skill(Hero& h, int n, int x, int y) {
  if (n < 1 || n > int(h.cls->skills.size())) { if (n > 1) h.say("No skill on [" + std::to_string(n) + "] yet: skills come as your styles grow.", 0x909090); return; }
  const std::string key = h.cls->skills[n - 1];
  if (key.empty()) { h.say("You have no weapon: its skill sits on [1].", 0x909090); return; }
  const SkillDef& sk = skill_def(key);
  bool fury = fury_skill(key);
  int& pool = fury ? h.fury : h.mana;
  int cost = sk.mana;
  if (key == "charge" && h.perk("frenzy")) cost = 0;
  if (key == "tumble") cost -= 2 * h.perk("quick");
  if (key == "kindle" && h.perk("deeprunes")) cost = 0;
  bool in_blood = !fury && pool < cost && combo(h, 2, elem::BLOOD) && h.hp > 2 * cost + 1;  // Bloodbinder
  int life = h.perk("bloodprice") ? (cost * 3 + 1) / 2 : 0;  // Bloodprice: paid in life, half as much again
  if (life > 0) {
    if (h.hp <= life) { h.say("Too little life left to pay for " + sk.name + ".", elem::COL[elem::BLOOD]); return; }
    in_blood = false;
  } else if (pool < cost && !in_blood) { h.say(std::string(fury ? "Not enough fury" : "Not enough mana") + " for " + sk.name + ".", fury ? 0xE08040 : 0x7090FF); return; }
  if (sk.target && !target(h, x, y)) { h.say("No target.", 0x909090); return; }
  if (h.smother > 0 && !h.perk("clearlungs")) { h.say("Soot chokes you: your skills will not come this turn.", elem::COL[elem::ASH]); return; }
  full_mana_ = h.mana >= stats(h).maxmana;
  if (life > 0) {
    h.hp -= life;
    num(h.x, h.y, std::to_string(life), elem::COL[elem::BLOOD]);
    cost = 0;
  }
  if (h.perk("spellblade") && !fury && cost > 0) h.fury = std::min(10, h.fury + 2);
  if (in_blood) {
    h.hp -= 2 * cost;
    num(h.x, h.y, std::to_string(2 * cost), elem::COL[elem::BLOOD]);
    cost = 0;
  }
  if (n >= 2 && h.el2 >= 0 && h.slot_el[n - 1] == 1) acting_el_ = h.el2;  // a key set to the second coal at the fire
  if (elem::skill_sign(key) == 0x263C) discharging_ = hero_el(h);
  Stats st = stats(h);
  double sp = 1 + st.spell / 100.0;
  if ((key == "volley" || key == "tumble") && st.reach < 5) { h.say(sk.name + " needs a weapon that shoots.", 0x909090); return; }
  credit_ = skill_style(h, key);
  credit_weapon_ = key == h.cls->skills[0];
  if (key != "cleave" && key != "warcry" && key != "volley" && key != "pierce" && key != "firebolt" && key != "nova") {
    if (skill_new(h, key, x, y, st, sp)) pool -= cost;
    return;
  }
  pool -= cost;
  if (key == "cleave") {
    add_fx({{"k", "spin"}, {"x", h.x}, {"y", h.y}});
    auto ms = floor->mons;
    for (auto& m : ms)
      if (m->hp > 0 && cheb(m->x, m->y, h.x, h.y) == 1 && strike(h, *m, 1.0 + .25 * h.perk("whirl")) && h.perk("whirl"))
        h.hp = std::min(st.maxhp, h.hp + 2 * h.perk("whirl"));
  } else if (key == "warcry") {
    add_fx({{"k", "wave"}, {"x", h.x}, {"y", h.y}, {"r", 2}, {"col", 0xFFDC96}, {"sign", "sound"}});
    for (auto& m : floor->mons) if (cheb(m->x, m->y, h.x, h.y) <= 2) { m->stun = m->rank == "boss" ? 1 : 2; m->awake = true; }
    if (h.perk("iron")) h.shield = 3;
    h.say("You roar. The foes around you reel.", 0xFFDC96);
  } else if (key == "volley") {
    std::vector<MonP> foes;
    for (auto& m : floor->mons) if (h.sees(m->x, m->y) && cheb(m->x, m->y, h.x, h.y) <= 9) foes.push_back(m);
    std::sort(foes.begin(), foes.end(), [&](const MonP& a, const MonP& b) { return cheb(a->x, a->y, h.x, h.y) < cheb(b->x, b->y, h.x, h.y); });
    if (int(foes.size()) > 3 + h.perk("multi")) foes.resize(3 + h.perk("multi"));
    int i = 0;
    for (auto& m : foes) {
      std::vector<P> cells; std::vector<MonP> hit;
      path(h, m->x, m->y, 9, false, cells, hit);
      add_fx({{"k", "bolt"}, {"cells", cells_json(cells)}, {"col", 0xDCDCC8}, {"ch", "-"}, {"src", xy(h.x, h.y)}, {"delay", i++ * .05}});
      if (!hit.empty() && hit[0]->hp > 0) strike(h, *hit[0], .8, true);
    }
    focus_up(h);
  } else if (key == "pierce") {
    std::vector<P> cells; std::vector<MonP> hit;
    path(h, x, y, 10, true, cells, hit);
    add_fx({{"k", "line"}, {"cells", cells_json(cells)}, {"col", 0xFFFFC8}, {"dur", .3}});
    for (auto& m : hit) if (m->hp > 0) strike(h, *m, 1.3, true);
    focus_up(h);
  } else if (key == "firebolt") {
    std::vector<P> cells; std::vector<MonP> hit;
    path(h, x, y, 9, false, cells, hit);
    add_fx({{"k", "bolt"}, {"cells", cells_json(cells)}, {"col", 0xFF8C28}, {"ch", "*"}});
    if (hit.empty()) return;
    MonP m = hit[0];
    ignite({m->x, m->y});
    int dmg = rnd_i((rnd.range(4, 9) + 1.15 * power(h)) * sp * (h.perk("pyromancer") ? 1.3 : 1.0));
    if (m->def->fire) dmg /= 2;
    if (m->frozen > 0) {
      dmg = dmg * 5 / 2;
      m->frozen = 0;
      add_fx({{"k", "burst"}, {"x", m->x}, {"y", m->y}, {"r", 1}, {"col", 0xC8E6FF}, {"delay", .1}});
      h.say("The frost on " + the(*m) + " shatters (" + std::to_string(dmg) + ")!", 0xC8E6FF);
      tip(h, "shatter", "Fire on a frozen foe shatters it for two and a half times the harm.");
    } else h.say("Your firebolt hits " + the(*m) + " (" + std::to_string(dmg) + ").", 0xFF8C28);
    int mx = m->x, my = m->y;
    hurt(*m, dmg, &h);
    if (h.perk("blast")) {
      add_fx({{"k", "burst"}, {"x", mx}, {"y", my}, {"r", 1}, {"col", 0xFF8C28}, {"delay", .1}});
      auto ms = floor->mons;
      for (auto& o : ms) if (o != m && o->hp > 0 && cheb(o->x, o->y, mx, my) <= 1) hurt(*o, rnd_i(dmg * .6), &h);
    }
  } else if (key == "nova") {
    int r = 2 + h.perk("deepfrost");
    add_fx({{"k", "burst"}, {"x", h.x}, {"y", h.y}, {"r", r}, {"col", 0x9CD2FF}, {"hollow", true}});
    auto ms = floor->mons;
    for (auto& m : ms)
      if (m->hp > 0 && cheb(m->x, m->y, h.x, h.y) <= r && h.sees(m->x, m->y)) {
        int dmg = rnd_i((rnd.range(3, 6) + .7 * power(h)) * sp);
        m->frozen = m->rank == "boss" ? 1 : 2 + h.perk("deepfrost");
        hurt(*m, m->def->cold ? dmg / 2 : dmg, &h);
      }
  }
}

// ------------------------------------------------------------------ the skills that came with the six classes
int Game::rune_dmg(const Hero& h) {
  double sp = 1 + stats(h).spell / 100.0;
  return rnd_i((rnd.range(6, 10) + 1.8 * power(h)) * sp * (h.perk("fireward") ? 1.3 : 1.0));
}

void Game::burst_rune(Hero& h, P at) {
  int r = h.perk("fireward") ? 2 : 1;
  for (int twice = h.leg("gravechisel") ? 2 : 1; twice > 0; --twice) {  // the Gravechisel's runes burst twice
    add_fx({{"k", "burst"}, {"x", at.x}, {"y", at.y}, {"r", r}, {"col", 0x50E6E6}, {"delay", twice == 1 ? 0.0 : .15}});
    int dmg = rune_dmg(h);
    auto ms = floor->mons;
    for (auto& m : ms) {
      if (m->hp <= 0 || cheb(m->x, m->y, at.x, at.y) > r) continue;
      int d = m->pinned > 0 && h.perk("heavyrunes") ? dmg * 5 / 4 : dmg;
      if (h.perk("stoneward")) m->pinned = std::max(m->pinned, 2);
      hurt(*m, m->def->fire ? d / 2 : d, &h);
    }
  }
}

// a foe has stepped: runes under it go off
void Game::trod(Mon& m) {
  if (m.hp <= 0) return;
  for (size_t i = 0; i < floor->runes.size(); ++i) {
    if (floor->runes[i].p != P{m.x, m.y}) continue;
    Rune r = floor->runes[i];
    floor->runes.erase(floor->runes.begin() + i);
    auto it = heroes.find(r.owner);
    if (it == heroes.end()) return;
    say_near(m.x, m.y, cap(the(m)) + " steps on a rune. It flares!", 0x50E6E6);
    burst_rune(*it->second, r.p);
    return;
  }
}

// every turn: what the legendaries do, and what they cost (plan.md section 7, Legendaries: power at a price)
void Game::legends_tick() {
  for (auto* h : active()) {
    if (h->dead) continue;
    if (h->boon > 0) --h->boon;  // a shrine's blessing burns down
    bool fight = false, near = false;
    for (auto& m : floor->mons)
      if (m->hp > 0 && m->awake && !m->dormant && !m->hidden) {
        if (h->sees(m->x, m->y)) fight = true;
        if (cheb(m->x, m->y, h->x, h->y) <= 2) near = true;
      }
    // Hungering Edge: in a fight, each turn without a kill it drinks a little of you (never the last of it)
    if (h->leg("hungering") && fight && h->kill_turn != turn && h->hp > 1) { --h->hp; num(h->x, h->y, "1", 0xA03030); }
    // Pyrehand Gauntlets: with foes close the gauntlets burn you too
    if (h->leg("pyrehand") && near) { hero_hurt(*h, nullptr, 1, "ember"); if (h->dead) continue; }
    // Whispering Hood: every foe on the map knows where you are
    if (h->leg("whispering"))
      for (auto& m : floor->mons) if (m->hp > 0 && !m->dormant && !m->hidden && !m->sleep) m->awake = true;
    // Ashwalker Boots: the foes that walk in your last steps choke on the ash you leave
    if (h->leg("ashwalker")) {
      if (h->trail.empty() || h->trail.back() != P{h->x, h->y}) h->trail.push_back({h->x, h->y});
      if (h->trail.size() > 5) h->trail.erase(h->trail.begin());
      auto ms = floor->mons;
      for (auto& m : ms)
        if (m->hp > 0 && P{m->x, m->y} != P{h->x, h->y} && std::find(h->trail.begin(), h->trail.end(), P{m->x, m->y}) != h->trail.end())
          lay_on_mon(*h, *m, 1, elem::ASH);
    } else h->trail.clear();
    // Ring of the Hollow Court: a dead dwarf rises on each map and fights beside you
    if (!h->leg("hollowcourt") || floor->depth == 0 || floor->resting()) { h->court = {-1, -1}; continue; }
    if (h->court_floor != floor->id || h->court.x < 0 || cheb(h->court.x, h->court.y, h->x, h->y) > 12) {
      P c;
      if (!free_near(h->x, h->y, 3, c, [&](int a, int b) { return floor->walk(a, b) && !mon_at(a, b) && !hero_at(a, b) && P{a, b} != P{h->x, h->y}; })) continue;
      if (h->court_floor != floor->id) {
        h->say("A dead dwarf of the Hollow Court climbs out of the stone, and takes its place at your side.", 0x8CC8B4);
        add_fx({{"k", "soul"}, {"x", c.x}, {"y", c.y}, {"dur", 1.2}});
      }
      h->court_floor = floor->id;
      h->court = c;
    }
    MonP foe;
    int bd = 9;
    for (auto& m : floor->mons)
      if (m->hp > 0 && m->awake && !m->dormant && !m->hidden && h->sees(m->x, m->y) && cheb(m->x, m->y, h->court.x, h->court.y) < bd) {
        bd = cheb(m->x, m->y, h->court.x, h->court.y);
        foe = m;
      }
    if (foe && bd <= 1) {
      Stats st = stats(*h);
      int dmg = std::max(1, rnd_i(rnd.range(st.lo, st.hi) * .5 + lvl / 2.0));
      add_fx({{"k", "swing"}, {"src", xy(h->court.x, h->court.y)}, {"cells", json::array({xy(foe->x, foe->y)})}});
      foe->awake = true;
      hurt(*foe, dmg, h, "");
      continue;
    }
    P goal = foe ? P{foe->x, foe->y} : P{h->x, h->y};
    if (!foe && cheb(goal.x, goal.y, h->court.x, h->court.y) <= 2) continue;
    P best = h->court;
    int bdist = cheb(goal.x, goal.y, best.x, best.y);
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        int nx = h->court.x + dx, ny = h->court.y + dy;
        if (!floor->walk(nx, ny) || mon_at(nx, ny) || hero_at(nx, ny)) continue;
        int d = cheb(goal.x, goal.y, nx, ny);
        if (d < bdist) { bdist = d; best = {nx, ny}; }
      }
    h->court = best;
  }
}

// every turn: the lantern's light, burning and hallowed ground, bleeding, held foes, fury draining away
void Game::classes_tick() {
  for (auto* h : active()) {
    if (h->cls->key == "lantern") {
      int r = h->perk("shepherd") ? 3 : 2;
      for (auto* o : active()) {
        if (cheb(o->x, o->y, h->x, h->y) > r) continue;
        o->hp = std::min(stats(*o).maxhp, o->hp + heal_amt(*o, (1 + h->perk("warmth") + power(*h) / 12) * (h->leg("drowned") ? 2 : 1)));
      }
      auto ms = floor->mons;
      for (auto& m : ms)
        if (m->hp > 0 && m->def->undead && cheb(m->x, m->y, h->x, h->y) <= r && h->sees(m->x, m->y)) {
          m->awake = true;
          credit_ = 2; credit_weapon_ = true;
          hurt(*m, 1 + power(*h) / 3, h);
          credit_ = -1; credit_weapon_ = false;
        }
    }
    if (h->fury > 0 && turn % (h->perk("secondbreath") ? 6 : 3) == 0) {
      bool fight = false;
      for (auto& m : floor->mons) if (m->awake && h->sees(m->x, m->y)) fight = true;
      if (!fight) h->fury = h->perk("berserkheart") ? 0 : h->fury - 1;  // Berserk Heart: it all goes at once
    }
  }
  for (auto& g : floor->ground) {
    if (g.owner == 0) continue;  // the foes' ground: see traits_tick
    if (g.kind == G_EMBER) ignite(g.p);
    auto ms = floor->mons;
    for (auto& m : ms) {
      if (m->hp <= 0 || m->x != g.p.x || m->y != g.p.y) continue;
      auto it = heroes.find(g.owner);
      int d = g.kind == 1 && m->def->undead ? g.dmg * 2 : g.dmg;
      if (g.kind == 0 && m->def->fire) d /= 2;
      hurt(*m, std::max(1, d), it == heroes.end() ? nullptr : it->second.get());
    }
    --g.turns;
  }
  floor->ground.erase(std::remove_if(floor->ground.begin(), floor->ground.end(), [](const Ground& g) { return g.turns <= 0 && g.owner != 0; }), floor->ground.end());
  auto ms = floor->mons;
  for (auto& m : ms) {
    if (m->hp <= 0) continue;
    if (m->pinned > 0) --m->pinned;
    if (m->open > 0) --m->open;
    if (m->bleed > 0) {
      --m->bleed;
      hurt(*m, m->bleed_dmg, nullptr);
      if (m->bleed == 0) m->bleed_dmg = 0;
    }
  }
}

bool Game::skill_new(Hero& h, const std::string& key, int x, int y, const Stats& st, double sp) {
  auto free = [&](int cx, int cy) { return floor->walk(cx, cy) && !mon_at(cx, cy) && !hero_at(cx, cy); };
  auto seen_foes = [&](int r) {
    std::vector<MonP> out;
    for (auto& m : floor->mons) if (m->hp > 0 && h.sees(m->x, m->y) && cheb(m->x, m->y, h.x, h.y) <= r) out.push_back(m);
    return out;
  };
  auto adjacent = [&]() -> MonP {  // the foe next to you, the busy or the weakest first
    MonP best;
    for (auto& m : floor->mons) {
      if (m->hp <= 0 || cheb(m->x, m->y, h.x, h.y) != 1) continue;
      if (!best || (busy(*m, h) && !busy(*best, h)) || (busy(*m, h) == busy(*best, h) && m->hp < best->hp)) best = m;
    }
    return best;
  };
  auto hold = [&](Mon& m, int turns) { if (m.rank == "boss") m.stun = std::max(m.stun, 1); else m.stun = std::max(m.stun, turns); };
  if (key == "charge") {
    MonP m = target(h, x, y);
    if (!m || cheb(m->x, m->y, h.x, h.y) > 5) { h.say("No foe within 5 to charge.", 0x909090); return false; }
    P best{-1, -1};
    int bd = 99;
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        int cx = m->x + dx, cy = m->y + dy;
        if ((dx || dy) && free(cx, cy) && h.sees(cx, cy) && cheb(cx, cy, h.x, h.y) < bd) { bd = cheb(cx, cy, h.x, h.y); best = {cx, cy}; }
      }
    if (cheb(m->x, m->y, h.x, h.y) == 1) best = {h.x, h.y};
    if (best.x < 0) { h.say("No room to charge in.", 0x909090); return false; }
    add_fx({{"k", "bolt"}, {"cells", json::array({xy(h.x, h.y), xy(best.x, best.y)})}, {"col", 0xFFDC96}, {"ch", "*"}});
    h.x = best.x; h.y = best.y;
    if (strike(h, *m, 1.2) && m->hp > 0) hold(*m, 1);
    arrive(h);
  } else if (key == "standfast") {
    h.standfast = 4 + 3 * h.perk("bulwark");
    add_fx({{"k", "wave"}, {"x", h.x}, {"y", h.y}, {"r", 3}, {"col", 0xE6C878}, {"sign", "sound"}});
    h.say("You plant your feet. Come, then.", 0xE6C878);
  } else if (key == "tumble") {
    auto foes = seen_foes(9);
    if (foes.empty()) { h.say("No foe in sight.", 0x909090); return false; }
    std::sort(foes.begin(), foes.end(), [&](const MonP& a, const MonP& b) { return cheb(a->x, a->y, h.x, h.y) < cheb(b->x, b->y, h.x, h.y); });
    MonP m = foes[0];
    for (int step = 0; step < 2; ++step) {
      P best{h.x, h.y};
      int bd = cheb(h.x, h.y, m->x, m->y);
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
          int cx = h.x + dx, cy = h.y + dy;
          if ((dx || dy) && free(cx, cy) && !floor->burning(cx, cy) && cheb(cx, cy, m->x, m->y) > bd) { bd = cheb(cx, cy, m->x, m->y); best = {cx, cy}; }
        }
      h.x = best.x; h.y = best.y;
    }
    std::vector<P> cells; std::vector<MonP> hit;
    path(h, m->x, m->y, 9, false, cells, hit);
    add_fx({{"k", "bolt"}, {"cells", cells_json(cells)}, {"col", 0xDCDCC8}, {"ch", "-"}, {"src", xy(h.x, h.y)}});
    if (!hit.empty()) strike(h, *hit[0], 1.0, true);
    focus_up(h);
    arrive(h);
  } else if (key == "pin") {
    if (!target(h, x, y)) { h.say("No target.", 0x909090); return false; }
    std::vector<P> cells; std::vector<MonP> hit;
    path(h, x, y, 9, false, cells, hit);
    add_fx({{"k", "bolt"}, {"cells", cells_json(cells)}, {"col", 0xC8C8FF}, {"ch", "-"}, {"src", xy(h.x, h.y)}});
    if (!hit.empty() && strike(h, *hit[0], 1.0, true) && hit[0]->hp > 0) {
      if (hit[0]->rank == "boss") hit[0]->stun = std::max(hit[0]->stun, 1);
      else hit[0]->pinned = std::max(hit[0]->pinned, 3);
      h.say(cap(the(*hit[0])) + " is pinned where it stands.", 0xC8C8FF);
    }
    focus_up(h);
  } else if (key == "blink") {
    auto foes = seen_foes(12);
    P best{-1, -1};
    int bd = -1;
    for (int dy = -5; dy <= 5; ++dy)
      for (int dx = -5; dx <= 5; ++dx) {
        int cx = h.x + dx, cy = h.y + dy;
        if (cheb(cx, cy, h.x, h.y) < 3 || !free(cx, cy) || !h.sees(cx, cy) || floor->burning(cx, cy)) continue;
        int near = 99;
        for (auto& m : foes) near = std::min(near, cheb(cx, cy, m->x, m->y));
        int score = near * 10 + int(rnd.below(10));
        if (score > bd) { bd = score; best = {cx, cy}; }
      }
    if (best.x < 0) { h.say("Nowhere to blink to.", 0x909090); return false; }
    add_fx({{"k", "inward"}, {"x", h.x}, {"y", h.y}, {"col", 0xA0B4FF}});
    h.x = best.x; h.y = best.y;
    add_fx({{"k", "burst"}, {"x", h.x}, {"y", h.y}, {"r", 1}, {"col", 0xA0B4FF}, {"delay", .1}});
    arrive(h);
  } else if (key == "emberwall") {
    MonP m = target(h, x, y);
    if (!m) { h.say("No target.", 0x909090); return false; }
    int dx = (m->x > h.x) - (m->x < h.x), dy = (m->y > h.y) - (m->y < h.y);
    int dist = std::max(1, std::min(2, cheb(m->x, m->y, h.x, h.y) - 1));
    P c{h.x + dx * dist, h.y + dy * dist};
    int px = -dy, py = dx;
    int dmg = rnd_i((3 + .8 * power(h)) * sp * (h.perk("pyromancer") ? 1.3 : 1.0));
    json cells = json::array();
    for (int k = -2; k <= 2; ++k) {
      P p{c.x + px * k, c.y + py * k};
      if (!floor->walk(p.x, p.y) || hero_at(p.x, p.y)) continue;
      floor->ground.push_back({p, 0, 5, dmg, h.pid});
      cells.push_back(xy(p.x, p.y));
    }
    if (cells.empty()) { h.say("No room for a wall there.", 0x909090); return false; }
    add_fx({{"k", "line"}, {"cells", cells}, {"col", 0xFF8C28}, {"dur", .4}});
    h.say("A wall of embers rises.", 0xFF8C28);
  } else if (key == "smite") {
    MonP m = adjacent();
    if (!m) { h.say("No foe next to you.", 0x909090); return false; }
    double mult = (m->def->undead ? 2.5 : 1.6) * (h.perk("zealot") ? 1.4 : 1.0);
    add_fx({{"k", "burst"}, {"x", m->x}, {"y", m->y}, {"r", 1}, {"col", 0xFFE68C}});
    int mx = m->x, my = m->y;
    bool landed = strike(h, *m, mult);
    // the blow throws the foe a step back; a wall or another foe behind it takes the rest of the blow (plan.md section 7)
    if (landed && m->hp > 0 && m->rank != "boss" && !m->can("stone")) {
      int dx = (mx > h.x) - (mx < h.x), dy = (my > h.y) - (my < h.y), nx = mx + dx, ny = my + dy;
      auto thud = [&] { return std::max(1, rnd_i(rnd.range(st.lo, st.hi) * .5)); };
      if (floor->walk(nx, ny) && !mon_at(nx, ny) && !hero_at(nx, ny)) {
        m->x = nx; m->y = ny;
        add_fx({{"k", "burst"}, {"x", nx}, {"y", ny}, {"r", 0}, {"col", 0xE0D080}});
      } else if (auto o = mon_at(nx, ny); o && o.get() != m.get()) {
        h.say(cap(the(*m)) + " is thrown into " + the(*o) + ".", 0xE0D080);
        hurt(*m, thud(), &h);
        if (o->hp > 0) hurt(*o, thud(), &h);
      } else if (!floor->walk(nx, ny)) {
        h.say(cap(the(*m)) + " is thrown against the wall.", 0xE0D080);
        hurt(*m, thud(), &h);
      }
    }
    if (h.perk("zealot"))
      for (auto& o : floor->mons)
        if (o != m && o->hp > 0 && cheb(o->x, o->y, mx, my) <= 2 && h.sees(o->x, o->y)) {
          add_fx({{"k", "bolt"}, {"cells", json::array({xy(mx, my), xy(o->x, o->y)})}, {"col", 0xFFE68C}, {"ch", "*"}});
          strike(h, *o, (o->def->undead ? 1.6 : 1.0));
          break;
        }
  } else if (key == "mend") {
    for (auto* o : active()) {
      if (cheb(o->x, o->y, h.x, h.y) > 3) continue;
      Stats os = stats(*o);
      int heal = rnd_i((os.maxhp * .25 + 2 * power(h)) * (h.perk("shepherd") ? 1.5 : 1.0));
      o->hp = std::min(os.maxhp, o->hp + heal);
      o->poison = 0;
      add_fx({{"k", "rise"}, {"x", o->x}, {"y", o->y}, {"col", 0xFFE68C}});
      o->say(o == &h ? "Your wounds close." : h.name + "'s light closes your wounds.", 0xFFE68C);
    }
  } else if (key == "hallow") {
    int dmg = rnd_i((2 + .6 * power(h)) * sp * (h.perk("pyre") ? 2 : 1));
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx)
        if (floor->walk(h.x + dx, h.y + dy)) floor->ground.push_back({{h.x + dx, h.y + dy}, 1, 5, dmg, h.pid});
    add_fx({{"k", "burst"}, {"x", h.x}, {"y", h.y}, {"r", 1}, {"col", 0xFFE68C}, {"hollow", true}});
    h.say("The ground around you is hallowed.", 0xFFE68C);
  } else if (key == "beacon") {
    add_fx({{"k", "burst"}, {"x", h.x}, {"y", h.y}, {"r", 4}, {"col", 0xFFF0B4}, {"hollow", true}});
    auto ms = seen_foes(4);
    for (auto& m : ms) {
      hold(*m, 2);
      m->awake = true;
      if (m->def->undead) hurt(*m, rnd_i(6 + 1.5 * power(h)), &h);
    }
    h.say("Your lantern flares white.", 0xFFF0B4);
  } else if (key == "rune") {
    for (auto& r : floor->runes) if (r.p == P{h.x, h.y}) { h.say("A rune is already carved here.", 0x909090); return false; }
    floor->runes.push_back({{h.x, h.y}, h.pid});
    if (h.leg("gravechisel")) hero_hurt(h, nullptr, std::max(1, stats(h).maxhp / 20), "blood");  // the Gravechisel takes its toll
    int mine = 0;
    for (auto& r : floor->runes) if (r.owner == h.pid) ++mine;
    if (mine > (h.perk("stoneward") ? 5 : 3))
      for (size_t i = 0; i < floor->runes.size(); ++i) if (floor->runes[i].owner == h.pid) { floor->runes.erase(floor->runes.begin() + i); break; }
    add_fx({{"k", "rise"}, {"x", h.x}, {"y", h.y}, {"col", 0x50E6E6}});
    h.say("You carve a rune into the floor.", 0x50E6E6);
    tip(h, "rune", "A foe that steps on your rune sets it off. Lure them over it, or burst them all with Kindle: [1].");
  } else if (key == "kindle") {
    std::vector<P> mine;
    for (auto& r : floor->runes) if (r.owner == h.pid) mine.push_back(r.p);
    if (mine.empty()) { h.say("You have no runes carved.", 0x909090); return false; }
    floor->runes.erase(std::remove_if(floor->runes.begin(), floor->runes.end(), [&](const Rune& r) { return r.owner == h.pid; }), floor->runes.end());
    for (auto p : mine) burst_rune(h, p);
  } else if (key == "ward") {
    MonP m = target(h, x, y);
    if (!m || cheb(m->x, m->y, h.x, h.y) > 7) { h.say("No foe within 7.", 0x909090); return false; }
    add_fx({{"k", "burst"}, {"x", m->x}, {"y", m->y}, {"r", 1}, {"col", 0x50E6E6}, {"gather", true}});
    if (m->rank == "boss") m->stun = std::max(m->stun, 1); else m->pinned = std::max(m->pinned, 3);
    m->awake = true;
    say_near(m->x, m->y, "Runes close round " + the(*m) + ".", 0x50E6E6);
    hurt(*m, rune_dmg(h) / 2, &h);
  } else if (key == "recall") {
    int idx = -1;
    for (size_t i = 0; i < floor->runes.size(); ++i) if (floor->runes[i].owner == h.pid) { idx = int(i); break; }
    if (idx < 0) { h.say("You have no runes carved.", 0x909090); return false; }
    P to = floor->runes[idx].p;
    if (!free(to.x, to.y)) { h.say("Something stands on your rune.", 0x909090); return false; }
    floor->runes.erase(floor->runes.begin() + idx);
    P from{h.x, h.y};
    add_fx({{"k", "inward"}, {"x", h.x}, {"y", h.y}, {"col", 0x50E6E6}});
    h.x = to.x; h.y = to.y;
    burst_rune(h, from);
    arrive(h);
  } else if (key == "shadowstep") {
    MonP m = target(h, x, y);
    if (!m || cheb(m->x, m->y, h.x, h.y) > 4) { h.say("No foe within 4.", 0x909090); return false; }
    P best{-1, -1};
    int bd = -1;
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        int cx = m->x + dx, cy = m->y + dy;
        if ((dx || dy) && free(cx, cy) && !floor->burning(cx, cy) && cheb(cx, cy, h.x, h.y) > bd) { bd = cheb(cx, cy, h.x, h.y); best = {cx, cy}; }
      }
    if (best.x < 0) { h.say("No room behind it.", 0x909090); return false; }
    add_fx({{"k", "inward"}, {"x", h.x}, {"y", h.y}, {"col", 0x8C7896}});
    h.x = best.x; h.y = best.y;
    m->surprised = 1;
    m->open = 2;
    m->awake = true;
    h.say("You step out of the shadows behind " + the(*m) + ".", 0xC8A0C8);
    tip(h, "shadowstep", "A foe caught off guard is busy: strike it now for an opening.");
    arrive(h);
  } else if (key == "fan") {
    auto ms = seen_foes(2);
    if (ms.empty()) { h.say("No foe within 2.", 0x909090); return false; }
    add_fx({{"k", "burst"}, {"x", h.x}, {"y", h.y}, {"r", 2}, {"col", 0xC8C8D2}, {"signs", "/\\-|"}});
    for (auto& m : ms)
      if (m->hp > 0 && strike(h, *m, .8) && m->hp > 0 && h.perk("venom")) {
        m->bleed = 5;
        m->bleed_dmg = std::max(m->bleed_dmg, 2 + power(h) / 2);
      }
  } else if (key == "smoke") {
    add_fx({{"k", "burst"}, {"x", h.x}, {"y", h.y}, {"r", 3}, {"col", 0x8C8C96}, {"signs", "~"}});
    for (auto& m : seen_foes(3)) m->lost = 2;
    h.say("Smoke fills the air. They have lost you.", 0xA0A0AA);
  } else if (key == "garrote") {
    MonP m = adjacent();
    if (!m) { h.say("No foe next to you.", 0x909090); return false; }
    bool was_busy = busy(*m, h);
    if (strike(h, *m, 1.2) && m->hp > 0) {
      hold(*m, 2);
      if (!was_busy) bleed(h, *m, 4 + power(h));
      say_near(m->x, m->y, cap(the(*m)) + " chokes and cannot act.", 0xC8A0C8);
    }
  } else if (key == "hurl") {
    MonP m = target(h, x, y);
    if (!m || cheb(m->x, m->y, h.x, h.y) > 6) { h.say("No foe within 6.", 0x909090); return false; }
    std::vector<P> cells; std::vector<MonP> hit;
    path(h, m->x, m->y, 6, false, cells, hit);
    add_fx({{"k", "bolt"}, {"cells", cells_json(cells)}, {"col", 0xD8C8A0}, {"ch", "/"}, {"src", xy(h.x, h.y)}});
    if (!hit.empty()) strike(h, *hit[0], 1.5, true);
    h.say("Your spear flies, and comes back to your hand.", 0xD8C8A0);
  } else if (key == "scatter") {
    std::vector<MonP> foes = seen_foes(7);
    if (foes.empty()) { h.say("No foe within 7.", 0x909090); return false; }
    std::sort(foes.begin(), foes.end(), [&](const MonP& a, const MonP& b) { return cheb(a->x, a->y, h.x, h.y) < cheb(b->x, b->y, h.x, h.y); });
    if (foes.size() > 3) foes.resize(3);
    int i = 0;
    for (auto& m : foes) {
      std::vector<P> cells; std::vector<MonP> hit;
      path(h, m->x, m->y, 7, false, cells, hit);
      add_fx({{"k", "bolt"}, {"cells", cells_json(cells)}, {"col", 0xC8B4FF}, {"ch", "*"}, {"src", xy(h.x, h.y)}, {"delay", i++ * .05}});
      if (!hit.empty() && hit[0]->hp > 0) strike(h, *hit[0], .8 * sp, true);
    }
  } else if (key == "whirlwind") {
    auto ms = seen_foes(2);
    if (ms.empty()) { h.say("No foe within 2.", 0x909090); return false; }
    add_fx({{"k", "burst"}, {"x", h.x}, {"y", h.y}, {"r", 2}, {"col", 0xE6C878}, {"hollow", true}});
    for (auto& m : ms) if (m->hp > 0) strike(h, *m, .8);
  } else return false;
  return true;
}

// [f]: what it does depends on the weapon in hand (plan.md section 1, Three layers)
void Game::shoot_with(Hero& h, int x, int y) {
  Stats st = stats(h);
  const std::string k = h.cls->key;
  const WeaponDef* wd = weapon_def(k);
  if (!wd) { h.say("You have no weapon.", 0x909090); return; }
  credit_ = style_index(wd->style);
  credit_weapon_ = true;
  if (k == "chisel") { skill_new(h, "rune", x, y, st, 1.0); return; }
  if (st.reach <= 0) { h.say("Your " + wd->name + " does not shoot. Walk into a foe to strike it.", 0x909090); return; }
  MonP m = target(h, x, y);
  if (!m) return;
  if (cheb(m->x, m->y, h.x, h.y) > st.reach) { h.say("Too far: your " + wd->name + " reaches " + std::to_string(st.reach) + ".", 0x909090); return; }
  if (k == "bow" && h.leg("recurve"))  // the Webmother's Recurve will not draw with a foe beside you
    for (auto& o : floor->mons)
      if (o->hp > 0 && !o->hidden && !o->dormant && cheb(o->x, o->y, h.x, h.y) == 1) { h.say("Too close: the great recurve will not draw with a foe beside you.", 0x909090); return; }
  double sp = 1 + st.spell / 100.0;
  std::vector<P> cells; std::vector<MonP> hit;
  if (k == "crossbow") {
    if (h.reload > 0) { --h.reload; h.say(h.reload ? "You wind the great crossbow. Halfway." : "You wind the crossbow.", 0xB0A890); return; }
    path(h, x, y, st.reach, true, cells, hit);
    add_fx({{"k", "line"}, {"cells", cells_json(cells)}, {"col", 0xE0D8C0}, {"dur", .25}});
    bool freeze = h.leg("coldheart");  // the Coldheart Arbalest: every bolt freezes, and it winds twice as long
    for (auto& o : hit)
      if (o->hp > 0 && strike(h, *o, 1.0, true) && freeze && o->hp > 0) { o->frozen = std::max(o->frozen, o->rank == "boss" ? 1 : 2); num(o->x, o->y, "frozen", elem::COL[elem::RIME]); }
    h.reload = freeze ? 2 : 1;
    tip(h, "crossbow", "A bolt goes through every foe in its line. Then the crossbow wants a turn to wind: [f] again.");
    return;
  }
  if (k == "spear") {
    path(h, x, y, 2, false, cells, hit);
    add_fx({{"k", "line"}, {"cells", cells_json(cells)}, {"col", 0xD8C8A0}, {"dur", .2}});
    if (!hit.empty()) strike(h, *hit[0], 1.0, false);
    if (!hit.empty() && hit[0]->hp > 0 && h.leg("lastspear")) strike(h, *hit[0], 1.0, false);
    return;
  }
  path(h, x, y, st.reach, false, cells, hit);
  bool magic = k == "staff" || k == "wand";
  int col = magic ? (h.el >= 0 ? elem::COL[h.el] : 0xC8B4FF) : 0xDCDCC8;
  add_fx({{"k", "bolt"}, {"cells", cells_json(cells)}, {"col", col}, {"ch", magic ? "*" : "-"}, {"src", xy(h.x, h.y)}});
  if (hit.empty()) return;
  MonP t = hit[0];
  double mult = k == "staff" ? .9 * sp : k == "wand" ? .7 * sp : k == "thrown" && h.moved == turn - 1 ? 1.25 : 1.0;
  if (h.perk("skirmisher") && h.moved == turn - 1) mult *= 1.2;
  bool burning = t->stk.n[elem::EMBER] > 0;
  if (k == "wand" && h.leg("splinter")) {  // the Splinterwand: three strikes a bolt, but a quarter fizzle
    if (rnd.below(4) == 0) { h.say("The Splinterwand sputters. The bolt fizzles.", 0x909090); num(h.x, h.y, "fizzle", 0x808080); return; }
    for (int i = 0; i < 2 && t->hp > 0; ++i) strike(h, *t, mult, true);
  }
  if (k == "thrown" && h.leg("bleedingstars")) {  // the Bleeding Stars cut both ways
    lay_on_mon(h, *t, 3, elem::BLOOD);
    h.stk.n[elem::BLOOD] = std::min(elem::MAX, h.stk.n[elem::BLOOD] + 1);
    h.stk.idle[elem::BLOOD] = 0;
  }
  if (t->hp <= 0) { focus_up(h); return; }
  strike(h, *t, mult, true);
  if (burning && combo(h, 1, elem::EMBER)) {  // Kindler: the shot goes on through a burning foe
    std::vector<P> c2; std::vector<MonP> h2;
    path(h, x, y, st.reach, true, c2, h2);
    for (size_t i = 1; i < h2.size(); ++i) {
      if (h2[i]->hp <= 0) continue;
      bool on = h2[i]->stk.n[elem::EMBER] > 0;
      strike(h, *h2[i], mult, true);
      if (!on) break;
    }
  }
  if (k == "wand" && t->hp > 0 && rnd.below(100) < 25 + 5 * mastery(h, "wand")) {
    add_fx({{"k", "bolt"}, {"cells", cells_json(cells)}, {"col", col}, {"ch", "*"}, {"src", xy(h.x, h.y)}, {"delay", .08}});
    strike(h, *t, mult, true);
  }
  focus_up(h);
}

void Game::quaff(Hero& h, const std::string& k) {
  Stats st = stats(h);
  if (h.leg("lastcrown")) { h.say("The Crown of the Last Dwarf will not let a draught past your lips.", 0xB4A078); return; }
  if (k == "heal") {
    if (h.perk("ironoath")) { h.say("Your oath forbids it: no draught passes your lips.", 0xB4A078); return; }
    if (h.heal <= 0) { h.say("No healing potions left.", 0x909090); return; }
    --h.heal;
    int before = h.hp;
    double cut = h.perk("rotproof") ? std::min(25, heal_cut(h.stk, true)) : heal_cut(h.stk, true);
    int top = h.leg("headsman") ? std::max(h.hp, st.maxhp / 2) : st.maxhp;  // Headsman's Promise: a potion stops at half
    h.hp = std::min(top, h.hp + heal_amt(h, rnd_i(st.maxhp * .45 * (omen(omen::THIN) ? 2 / 3.0 : 1.0) * (h.perk("deathwish") ? .5 : 1) * (1 + .4 * h.perk("second")) * (100 - cut) / 100)));
    if (h.stk.n[elem::ROT] > 0) h.say("The rot in you takes some of it.", elem::COL[elem::ROT]);
    num(h.x, h.y, "+" + std::to_string(h.hp - before), 0x60E060);
    h.poison = 0;
    h.say("You drink a healing potion.", 0xFF7070);
    add_fx({{"k", "rise"}, {"x", h.x}, {"y", h.y}, {"col", 0xFF7070}});
  } else {
    if (h.mana_pots <= 0) { h.say("No mana potions left.", 0x909090); return; }
    --h.mana_pots;
    h.mana = std::min(st.maxmana, h.mana + rnd_i(st.maxmana * .6 * (1 + .4 * h.perk("second"))));
    h.fury = std::min(10, h.fury + 5);
    h.say("You drink a mana potion. It burns in you: mana, and fury too.", 0x7090FF);
    add_fx({{"k", "rise"}, {"x", h.x}, {"y", h.y}, {"col", 0x7090FF}});
  }
}

static ItemP take(std::vector<ItemP>& pack, int id) {
  for (auto it = pack.begin(); it != pack.end(); ++it)
    if ((*it)->id == id) { ItemP r = *it; pack.erase(it); return r; }
  return nullptr;
}

void Game::equip(Hero& h, int id) {
  ItemP it;
  for (auto& i : h.pack) if (i->id == id) it = i;
  if (!it) return;
  if (!can_use(h, *it)) { h.say("A " + h.cls->name + " cannot fight with that.", 0x909090); return; }
  take(h.pack, id);
  auto old = h.equip.find(it->slot);
  if (old != h.equip.end()) h.pack.push_back(old->second);
  h.equip[it->slot] = it;
  Stats st = stats(h);
  h.hp = std::min(h.hp, st.maxhp); h.mana = std::min(h.mana, st.maxmana);
  h.say("You put on " + it->name + ".", rarity_col(it->rarity));
}

void Game::drop(Hero& h, int id) {
  ItemP it = take(h.pack, id);
  if (!it) return;
  floor->items.push_back({h.x, h.y, it, 0, "", 0});
  h.say("You put down " + it->name + ". Anyone may take it.", 0xA0A0A0);
}

void Game::give(Hero& h, int id) {
  Hero* to = nullptr;
  for (auto* o : active()) if (o != &h && cheb(o->x, o->y, h.x, h.y) <= 1) { to = o; break; }
  if (!to) { h.say("Nobody is next to you to take it.", 0x909090); return; }
  if (int(to->pack.size()) >= pack_max(*to)) { h.say(to->name + "'s pack is full.", 0x909090); return; }
  ItemP it = take(h.pack, id);
  if (!it) return;
  to->pack.push_back(it);
  h.say("You give " + it->name + " to " + to->name + ".", 0xB4DCB4);
  to->say(h.name + " gives you " + it->name + ".", 0xB4DCB4);
}

void Game::buy(Hero& h, int i) {
  if (!h.shopping || i < 0 || i >= int(floor->shop.size())) return;
  ItemP it = floor->shop[i];
  int price = this->price(h, *it);
  if (h.gold < price) { h.say("You cannot pay " + std::to_string(price) + " gold.", 0x909090); return; }
  if (int(h.pack.size()) >= pack_max(h)) { h.say("Your pack is full.", 0x909090); return; }
  h.gold -= price;
  floor->shop.erase(floor->shop.begin() + i);
  h.pack.push_back(it);
  h.say("You buy " + it->name + ".", rarity_col(it->rarity));
}

void Game::buy_potion(Hero& h, const std::string& k) {
  if (h.talking != "healer") return;  // only Ysolde sells potions
  int price = h.perk("haggler") ? potion_price() * 4 / 5 : potion_price();
  if (h.gold < price) { h.say("You cannot pay " + std::to_string(price) + " gold.", 0x909090); return; }
  if (k == "heal" && h.heal < belt(h)) { ++h.heal; h.gold -= price; }
  else if (k == "mana" && h.mana_pots < belt(h)) { ++h.mana_pots; h.gold -= price; }
}

void Game::sell(Hero& h, int id) {
  if (!h.shopping) return;
  ItemP it = take(h.pack, id);
  if (!it) return;
  int g = std::max(1, it->value / 3);
  h.gold += g;
  h.say("You sell " + it->name + " for " + std::to_string(g) + " gold.", 0xFFD700);
}

// ------------------------------------------------------------------ the creatures
void Game::compute_dist() {
  dist_.assign(W * H, 1 << 30);
  std::deque<int> q;
  for (auto* h : active()) { int i = h->y * W + h->x; dist_[i] = 0; q.push_back(i); }
  while (!q.empty()) {
    int i = q.front(); q.pop_front();
    int x = i % W, y = i / W;
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        if (!dx && !dy) continue;
        int nx = x + dx, ny = y + dy;
        if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
        int t = floor->tile(nx, ny);
        if (blocks(t) && t != DOOR) continue;
        int j = ny * W + nx;
        if (dist_[j] > dist_[i] + 1) { dist_[j] = dist_[i] + 1; q.push_back(j); }
      }
  }
}

void Game::monsters() {
  if (active().empty()) return;
  if (floor->depth == 0) town_tick();
  if (floor->act == PRISON && floor->depth > 0) prison_tick();
  compute_dist();
  auto ms = floor->mons;
  for (auto& m : ms) {
    if (m->hp <= 0 || m->blow.turns <= 0) continue;
    if (m->blow.kind == BLOW_PURPLE && (m->stun > 0 || m->frozen > 0)) {
      m->blow = {};
      m->spent = 2;
      say_near(m->x, m->y, "The spell breaks! " + cap(the(*m)) + " reels, open to your blows.", 0xD0A0FF);
      continue;
    }
    if (--m->blow.turns == 0) land(*m);
    if (state != "play") return;
  }
  for (auto& m : ms) {
    if (m->hp <= 0) continue;
    m->energy += m->spd * (100 - rime_slow(*m)) / 100;
    while (m->energy >= 100 && m->hp > 0) {
      m->energy -= 100;
      mon_turn(*m);
      if (state != "play") return;
    }
  }
}

Hero* Game::nearest_hero(const Mon& m, bool seen_only) {
  for (auto* h : active()) if (h->standfast > 0 && cheb(h->x, h->y, m.x, m.y) <= 3) return h;
  Hero* best = nullptr;
  int bd = 1 << 30;
  for (auto* h : active()) {
    if (seen_only && !h->sees(m.x, m.y)) continue;
    int d = cheb(h->x, h->y, m.x, m.y);
    if (d < bd) { bd = d; best = h; }
  }
  return best;
}

void Game::mon_turn(Mon& m) {
  if (m.smothered > 0) --m.smothered;
  if (m.stun > 0) { --m.stun; return; }
  if (m.frozen > 0) { --m.frozen; return; }
  if (!m.awake) return;
  if (m.blow.turns > 0) return;               // winding up: it does nothing else
  if (m.spent > 0) { --m.spent; return; }     // open after a great blow
  if (m.rank == "boss" && m.brace > 0) --m.brace;
  if (m.surprised > 0) { --m.surprised; return; }
  if (m.lost > 0) { --m.lost; if (m.pinned <= 0) { step_random(m); trod(m); } return; }
  Hero* h = nearest_hero(m, true);
  if (!h) h = nearest_hero(m, false);
  if (!h) return;
  int d = cheb(h->x, h->y, m.x, m.y);
  bool sees = h->sees(m.x, m.y);
  // a statue stands until someone comes close; a heap of gold until someone is next to it
  if (m.dormant || m.hidden) {
    if (d <= (m.hidden ? 1 : 2)) wake(m, "");
    return;
  }
  if (m.sleep) {
    // the Sanctum's altar-guards kneel until someone comes near the empty altar
    bool kneels = floor->act == SANCTUM && m.def->key == "altarguard";
    if (kneels && (d <= 2 || cheb(h->x, h->y, floor->down.x, floor->down.y + 3) <= 6)) {
      m.sleep = 0;
      say_near(m.x, m.y, cap(the(m)) + " rises from its knees, black armour grinding.", 0xB0B0C8);
    } else if (!kneels && d <= 1) { m.sleep = 0; say_near(m.x, m.y, cap(the(m)) + " wakes with a start.", 0xC0A080); }
    return;
  }
  if (m.rank == "boss" && !m.def->moves.empty() && sees && --m.cool <= 0) {
    if (m.deck.empty()) {
      auto it = BOSS_DECKS.find(m.def->key);
      if (it != BOSS_DECKS.end()) m.deck = it->second[std::min(m.phase, 2)];
      else m.deck = m.def->moves;
      if (m.def->key == "ashenking") {  // what the King has learned
        std::vector<std::string> more;
        if (frost >= 1) more.push_back("frozenground");
        if (frost >= 4) more.push_back("ashveil");
        if (frost >= 7) more.push_back("hollowcourt");
        for (auto& z : slain) if (auto b = KING_BORROWS.find(z); b != KING_BORROWS.end()) more.push_back(b->second);
        for (int k = 0; k < (m.phase >= 3 ? 2 : 1); ++k) m.deck.insert(m.deck.end(), more.begin(), more.end());
        if (m.phase >= 3) m.deck.push_back("pyre");
      }
      rnd.shuffle(m.deck);
    }
    std::string mv = m.deck.back();
    m.deck.pop_back();
    bool done = BLOWS.count(mv) ? wind_up(m, *h, mv) : boss_move(m, *h, d, mv);
    if (done) { m.cool = rnd.range(2, 3); return; }
    m.deck.insert(m.deck.begin(), mv);        // not now: later in the round
    m.cool = 1;
  } else if ((m.rank == "champion" || m.rank == "unique") && sees && d <= 2 && --m.cool <= 0) {
    if (wind_up(m, *h, "smash")) { m.cool = rnd.range(6, 8); return; }
  }
  if (m.discharger && m.rank == "normal" && sees && d <= 1 && h->stk.n[m.el] >= 2 && --m.cool <= 0) {
    if (wind_up(m, *h, "smash")) { m.cool = rnd.range(5, 7); return; }
  }
  if (m.rank != "boss" && trait_turn(m, *h, d, sees)) return;
  if (m.def->ai == "erratic" && rnd.real() < .4) { step_random(m); return; }
  if (m.def->ai == "ranged" && sees && d >= 2 && d <= m.def->rng && rnd.real() < .75) {
    std::vector<P> line;
    bool clear = true;
    for (auto&& [x, y] : tcod::BresenhamLine({m.x, m.y}, {h->x, h->y}).without_start()) {
      line.push_back({x, y});
      if (x == h->x && y == h->y) break;
      if (floor->opaque(x, y) || mon_at(x, y)) { clear = false; break; }
    }
    if (clear) {
      add_fx({{"k", "bolt"}, {"cells", cells_json(line)}, {"col", m.def->shot_col}, {"ch", std::string(1, char(m.def->shot_ch))}, {"src", xy(m.x, m.y)}});
      mon_hit(m, *h, true);
      return;
    }
  }
  if (d <= 1) { mon_hit(m, *h, false); return; }
  if (m.pinned > 0 || m.can("rooted")) return;
  P from{m.x, m.y};
  if (!trait_step(m, *h)) step_toward(m);
  if (P{m.x, m.y} != from) trait_moved(m, from);
  trod(m);
}

void Game::step_random(Mon& m) {
  std::vector<P> dirs;
  for (int a = -1; a <= 1; ++a) for (int b = -1; b <= 1; ++b) if (a || b) dirs.push_back({a, b});
  rnd.shuffle(dirs);
  for (auto c : dirs) {
    int nx = m.x + c.x, ny = m.y + c.y;
    if (floor->walk(nx, ny) && !mon_at(nx, ny) && !hero_at(nx, ny) && !floor->burning(nx, ny)) { m.x = nx; m.y = ny; return; }
  }
}

void Game::step_toward(Mon& m) {
  int bd = dist_[m.y * W + m.x];
  int bx = -1, by = -1;
  for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx) {
      if (!dx && !dy) continue;
      int nx = m.x + dx, ny = m.y + dy;
      if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
      int dd = dist_[ny * W + nx];
      if (dd < bd && !mon_at(nx, ny) && !hero_at(nx, ny) && !floor->burning(nx, ny)) { bd = dd; bx = nx; by = ny; }
    }
  if (bx < 0) return;
  if (floor->tile(bx, by) == DOOR) { floor->set(bx, by, OPEN); return; }
  if (!floor->walk(bx, by)) return;
  m.x = bx; m.y = by;
}

void Game::mon_hit(Mon& m, Hero& h, bool ranged) {
  Stats st = stats(h);
  int door = floor->tile(h.x, h.y) == DOOR || floor->tile(h.x, h.y) == OPEN;
  int chance = std::clamp(m.acc + lvl * 2 - st.armour - (h.cls->key == "dagger" && !ranged ? 15 : 0) - 8 * m.stk.n[elem::ASH] -
                              (door && h.perk("draughtwalker") ? 15 : 0) - (h.perk("whiteout") ? 5 * m.stk.n[elem::RIME] : 0), 20, 95);
  if (h.perk("unyielding")) chance = 100;  // Unyielding: it no longer dodges
  if (!ranged) add_fx({{"k", "swing"}, {"src", xy(m.x, m.y)}, {"cells", json::array({xy(h.x, h.y)})}});
  if (rnd.below(100) >= chance) {
    if (h.perk("riposte")) m.open = 2;
    if (h.perk("blindbleeding") && !m.can("stone")) { m.bleed = std::max(m.bleed, 3); m.bleed_dmg = std::max(m.bleed_dmg, 2 + lvl / 3); }
    num(h.x, h.y, "dodge", 0x808080);
    if (!ranged && m.stk.n[elem::ASH] > 0 && cheb(m.x, m.y, h.x, h.y) == 1 && combo(h, 0, elem::ASH)) {  // Cinderguard
      h.say(cap(the(m)) + " swings blind through the ash, and you strike back.", elem::COL[elem::ASH]);
      Hero* was = acting_;
      acting_ = &h;
      credit_ = 0; credit_weapon_ = true;
      strike(h, m, .6);
      credit_ = -1; credit_weapon_ = false;
      acting_ = was;
    }
    return;
  }
  // the sword's parry: a blow turned aside, and a riposte
  if (!ranged && h.cls->key == "sword" && cheb(m.x, m.y, h.x, h.y) == 1 && rnd.below(100) < 12 + 3 * mastery(h, "sword")) {
    num(h.x, h.y, "parry", 0xE0E0F0);
    h.say("You turn " + the(m) + "'s blow aside, and strike back.", 0xE0E0F0);
    Hero* was = acting_;
    acting_ = &h;
    credit_ = 0; credit_weapon_ = true;
    strike(h, m, .8);
    credit_ = -1; credit_weapon_ = false;
    acting_ = was;
    return;
  }
  int dmg = std::max(1, rnd.range(m.lo, m.hi) - st.armour / 6);
  if (h.leg("silentdirk") && rnd.below(100) < 10) { dmg *= 2; num(h.x, h.y, "crit", 0xFF5050); }  // the Silent Dirk's price
  if (omen(omen::FRENZY) && m.hp * 3 < m.maxhp) dmg = rnd_i(dmg * 1.25);  // Frenzy
  if (omen(omen::BLOODMOON)) dmg = rnd_i(dmg * 1.2);                       // Blood Moon
  if (m.can("fervour")) dmg = rnd_i(dmg * (1 + .6 * (1 - double(m.hp) / m.maxhp)));  // the more it bleeds, the harder
  hero_hurt(h, &m, dmg, "");
  if (!h.dead) trait_hit(m, h, dmg);
  // a quick builder lays its element with every blow, a slow discharger now and then
  int lays = 1 + (omen(omen::TWIN) && (m.rank == "champion" || m.rank == "unique") ? 1 : 0) + (omen(omen::EMBERTIDE) ? 1 : 0);
  if (!h.dead && (!m.discharger || rnd.below(2) == 0)) lay_on_hero(m, h, lays);
  // a zone boss past two thirds of its life lays its second element with every other blow (plan.md section 8)
  if (!h.dead && m.rank == "boss" && m.el2 >= 0 && m.phase >= 1 && ++m.lays % 2 == 0) lay_on_hero(m, h, 1, m.el2);
  if (m.has("Vampiric")) m.hp = std::min(m.maxhp, m.hp + dmg / 2 * (100 - heal_cut(m.stk, m.rank == "boss")) / 100);
  if (!ranged && st.thorns > 0 && m.hp > 0) hurt(m, st.thorns, &h);
}

void Game::hero_hurt(Hero& h, Mon* m, int dmg0, const std::string& kind) {
  if (h.dead || h.god) return;
  Stats st = stats(h);
  int dmg = dmg0;
  if (kind == "fire") dmg = rnd_i(dmg * (1 - st.res_f / 100.0));
  if (kind == "cold") dmg = rnd_i(dmg * (1 - st.res_c / 100.0));
  if (h.brand > 0) dmg = rnd_i(dmg * 1.25);
  if (h.shield > 0) dmg = std::max(1, dmg / 2);
  if (h.standfast > 0) dmg = std::max(1, dmg / 2);
  if (landing_ && h.tal("mantle")) dmg = std::max(1, dmg * (100 - 10 * h.tal("mantle")) / 100);
  if (h.fury > 0) dmg = std::max(1, rnd_i(dmg * (1 - .02 * h.fury)));  // fury dulls the pain
  if (h.perk("willward") && h.mana * 2 > st.maxmana) dmg = std::max(1, rnd_i(dmg * .85));
  if (h.perk("hermit") && heroes.size() == 1) dmg = std::max(1, rnd_i(dmg * .8));
  if (h.perk("slowbleeder") && (kind == "poison" || kind == "bleed" || kind == "blood")) dmg = std::max(1, rnd_i(dmg * .75));
  if (m && (h.srank[0] > 0 || main_style(h) == 0)) h.fury = std::min(10, h.fury + 1 + h.perk("guardian"));
  h.hp -= dmg;
  num(h.x, h.y, std::to_string(dmg), kind == "poison" ? 0x70D050 : kind == "ember" ? elem::COL[elem::EMBER] : kind == "blood" ? elem::COL[elem::BLOOD] : 0xFF5050);
  add_fx({{"k", "blood"}, {"x", h.x}, {"y", h.y}, {"src", xy(m ? m->x : h.x, m ? m->y : h.y - 1)}});
  h.say((m ? cap(the(*m)) : std::string(kind == "fire" ? "Fire" : kind == "bleed" || kind == "blood" ? "Bleeding" : kind == "ember" ? "Ember" : kind == "elem" ? "It" : "Poison")) + " hurts you (" + std::to_string(dmg) + ").", 0xFF8080);
  if (h.hp <= stats(h).maxhp / 3 && h.heal > 0) tip(h, "quaff", "You are badly hurt. Drink a healing potion: [q].");
  if (h.hp <= 0 && h.leg("secondheart") && h.heart_floor != floor->id) {  // the Second Heart: once a map, whole again
    h.heart_floor = floor->id;
    h.hp = stats(h).maxhp;
    h.say("Your second heart beats once, hard. You stand up whole.", 0x3CE0B4);
    num(h.x, h.y, "second heart", 0x3CE0B4);
    add_fx({{"k", "rise"}, {"x", h.x}, {"y", h.y}, {"col", 0x3CE0B4}});
    return;
  }
  if (h.hp <= 0 && h.perk("lastember") && h.ember_floor != floor->id) {  // Last Ember: once a map
    h.ember_floor = floor->id;
    h.hp = 1;
    h.say("Something in you refuses. You stand, on one last ember of life.", 0xFFB040);
    num(h.x, h.y, "last ember", 0xFFB040);
    return;
  }
  if (h.hp <= 0 && h.tal("unquenched") && !h.unquenched) {  // Unquenched: once a dive, the fire will not go out
    h.unquenched = true;
    Stats st = stats(h);
    h.hp = 1 + st.maxhp * 3 / 10;
    h.say("The Spark in you will not go out. You burst into healing flame!", 0xFFB040);
    add_fx({{"k", "wave"}, {"x", h.x}, {"y", h.y}, {"r", 2}, {"col", 0xFF8C28}, {"sign", "*"}});
    num(h.x, h.y, "unquenched", 0xFFB040);
    std::vector<MonP> near;
    for (auto& o : floor->mons) if (cheb(o->x, o->y, h.x, h.y) <= 2) near.push_back(o);
    for (auto& o : near) if (o->hp > 0) hurt(*o, 8 + lvl * 2, &h);
    return;
  }
  if (h.hp > 0 && h.tal("heartsfury") && h.fury_floor != floor->id && h.hp <= stats(h).maxhp / 4) {  // Heart's Fury
    h.fury_floor = floor->id;
    h.fury_t = 5;
    h.say("Your heart flares! The flames stun all around you.", 0xFF8040);
    add_fx({{"k", "wave"}, {"x", h.x}, {"y", h.y}, {"r", 3}, {"col", 0xFF6020}, {"sign", "^"}});
    for (auto& o : floor->mons) if (cheb(o->x, o->y, h.x, h.y) <= 3) o->stun = std::max(o->stun, o->rank == "boss" ? 1 : 2);
  }
  if (h.hp <= 0) {
    h.hp = 0;
    h.dead = true;
    h.killer = m ? m->name : kind.empty() ? "the dark" : kind == "ember" ? "fire" : kind;
    if (m && floor->depth > 0) {  // what this hero fell to: it may come back for the next one (plan.md section 7)
      static const char* AFF[elem::N] = {"Fire-blooded", "Frost-touched", "Ash", "Venomous", "Vampiric"};
      ItemP best;
      for (auto& [slot, it] : h.equip) if (it && (!best || it->value > best->value)) best = it;
      json items = json::array();
      if (m->nemesis) for (auto& c : m->carried) items.push_back(loot::save(*c));
      if (best) items.push_back(loot::save(*best));
      std::string aff = h.el >= 0 && h.el < elem::N ? AFF[h.el] : "Brutal";
      if (aff == "Ash") aff = "Ashen";
      last_fell = {{"key", m->def->key}, {"zone", ACTS[floor->act].key}, {"zname", ACTS[floor->act].name}, {"hero", h.name},
                   {"affix", m->nemesis && nemesis.is_object() ? nemesis.value("affix", aff) : aff}, {"items", items},
                   {"kills", m->nemesis && nemesis.is_object() ? nemesis.value("kills", 1) + 1 : 1},
                   {"name", m->nemesis ? m->name.substr(0, m->name.find(", Slayer of ")) : std::string()},
                   {"slew", m->nemesis && nemesis.is_object() ? nemesis.value("slew", json::array()) : json::array()}};
      last_fell["slew"].push_back(h.name);
    }
    h.say("You die.", 0xFF4040);
    say_all(h.name + " has fallen.", 0xFF6060);
    // the embers this hero carried go out with it, and those friends are gone for good
    for (int pid : h.carry) if (auto it = heroes.find(pid); it != heroes.end()) ember_out(*it->second, "the one who carried it fell");
    h.carry.clear();
    if (!living().empty()) {  // co-op: the ember drops where the hero fell, and the hero walks on as a ghost
      floor->embers.push_back({{h.x, h.y}, h.pid});
      add_fx({{"k", "burst"}, {"x", h.x}, {"y", h.y}, {"r", 1}, {"col", elem::COL[elem::EMBER]}});
      say_all(h.name + "'s ember lies where they fell. Carry it to the fire of the next camp to rekindle them.", 0xFFA040);
      h.say("You walk on as a ghost. Your friends can carry your ember to the fire of the next camp.", 0xA0A0A0);
      for (auto* o : living()) tip(*o, "ember", "Walk over a fallen friend's ember to carry it. It glows: foes see you from further off, and every third turn its weight holds you back.");
    }
    check_stairs();
  }
}

bool Game::boss_move(Mon& m, Hero& h, int d, const std::string& mv0) {
  std::string mv = mv0;
  if (mv == "summon") for (auto& k : m.def->moves) if (k.rfind("summon:", 0) == 0) mv = k;
  if (mv.rfind("summon:", 0) == 0) {
    int n = 0;
    for (auto& o : floor->mons) if (o->summoned) ++n;
    if (n >= 6) return false;
    for (int i = 0; i < 2; ++i) {
      P c;
      if (free_near(m.x, m.y, 2, c)) if (auto s = spawn(mv.substr(7), c.x, c.y)) { s->summoned = true; s->awake = true; s->dormant = s->hidden = 0; }
    }
    add_fx({{"k", "inward"}, {"x", m.x}, {"y", m.y}, {"col", m.col}});
    say_near(m.x, m.y, cap(the(m)) + " calls, and things answer.", m.col);
  } else if (mv == "slam") {
    if (d > 1) return false;
    add_fx({{"k", "wave"}, {"x", m.x}, {"y", m.y}, {"r", 1}, {"col", 0xDCC8A0}, {"sign", "*"}});
    if (floor->act != DEEP) add_fx({{"k", "quake"}, {"x", m.x}, {"y", m.y}, {"dur", 1.8}});  // the vault shakes
    for (auto* o : active()) if (cheb(o->x, o->y, m.x, m.y) <= 1) hero_hurt(*o, &m, rnd.range(m.lo, m.hi) + 4, "");
  } else if (mv == "spit") {
    if (d < 2 || d > 7) return false;
    std::vector<P> line;
    for (auto&& [x, y] : tcod::BresenhamLine({m.x, m.y}, {h.x, h.y}).without_start()) line.push_back({x, y});
    add_fx({{"k", "bolt"}, {"cells", cells_json(line)}, {"col", 0x96E65A}, {"ch", "*"}});
    add_fx({{"k", "burst"}, {"x", h.x}, {"y", h.y}, {"r", 1}, {"col", 0x96E65A}, {"delay", .1}});
    for (auto* o : active())
      if (cheb(o->x, o->y, h.x, h.y) <= 1) {
        hero_hurt(*o, &m, rnd.range(m.lo, m.hi) / 2, "");
        o->poison = 5;
        o->poison_dmg = std::max(o->poison_dmg, rnd_i(std::max(m.def->poison, 2 + lvl / 3) * (1 - stats(*o).res_p / 100.0)));
      }
  } else if (mv == "flame") {
    if (d > 5) return false;
    add_fx({{"k", "burst"}, {"x", h.x}, {"y", h.y}, {"r", 1}, {"col", 0xFF6A00}, {"gather", true}});
    for (auto* o : active()) if (cheb(o->x, o->y, h.x, h.y) <= 1) hero_hurt(*o, &m, rnd.range(m.lo, m.hi), "fire");
  } else if (mv == "nova") {
    if (d > 3) return false;
    add_fx({{"k", "burst"}, {"x", m.x}, {"y", m.y}, {"r", 3}, {"col", 0xFF7A28}, {"hollow", true}});
    for (auto* o : active()) if (cheb(o->x, o->y, m.x, m.y) <= 3) hero_hurt(*o, &m, rnd.range(m.lo, m.hi) * 3 / 4, "fire");
  } else if (mv == "rally") {  // the Iron-Captain calls his ranks: two pikemen, and all near him quicken
    for (int i = 0; i < 2; ++i) {
      P c;
      if (free_near(m.x, m.y, 2, c)) if (auto s2 = spawn("pikeman", c.x, c.y)) { s2->summoned = true; s2->awake = true; }
    }
    for (auto& o : floor->mons) if (o.get() != &m && cheb(o->x, o->y, m.x, m.y) <= 6) o->energy += 100;
    add_fx({{"k", "wave"}, {"x", m.x}, {"y", m.y}, {"r", 3}, {"col", 0xE07850}, {"sign", "sound"}});
    say_near(m.x, m.y, cap(the(m)) + " roars, and the ranks close up around him.", 0xE07850);
  } else if (mv == "arms") {  // six arms: every hero next to it
    if (d > 1) return false;
    add_fx({{"k", "spin"}, {"x", m.x}, {"y", m.y}});
    for (auto* o : active()) if (cheb(o->x, o->y, m.x, m.y) <= 1) hero_hurt(*o, &m, rnd.range(m.lo, m.hi), "");
    say_near(m.x, m.y, "Six arms lash out of the gold.", 0xFFD060);
  } else if (mv == "void") {  // the cold in the Prelate's chest pulls everyone two steps nearer
    bool moved = false;
    for (auto* o : active()) {
      if (cheb(o->x, o->y, m.x, m.y) > 8 || !o->sees(m.x, m.y)) continue;
      for (int k = 0; k < 2; ++k) {
        int dx = (m.x > o->x) - (m.x < o->x), dy = (m.y > o->y) - (m.y < o->y);
        int nx = o->x + dx, ny = o->y + dy;
        if (cheb(nx, ny, m.x, m.y) < 1 || !floor->walk(nx, ny) || mon_at(nx, ny) || hero_at(nx, ny)) break;
        o->x = nx; o->y = ny; moved = true;
      }
      o->numb = 3;
      arrive(*o);
    }
    if (!moved) return false;
    add_fx({{"k", "inward"}, {"x", m.x}, {"y", m.y}, {"col", 0x9CD2FF}});
    say_near(m.x, m.y, "The void in " + the(m) + "'s chest drags you toward him.", 0x9CD2FF);
  } else if (mv == "brace") {  // shield up: a quarter of the harm, until his next blow
    m.brace = 3;
    say_near(m.x, m.y, cap(the(m)) + " raises his shield. Wait for his blow.", 0xC0A080);
  } else if (mv == "frozenground") {  // the King learns, Frost 1: the floor around him turns to ice and bites
    if (d > 3) return false;
    add_fx({{"k", "wave"}, {"x", m.x}, {"y", m.y}, {"r", 3}, {"col", 0x9CD2FF}, {"sign", "*"}});
    for (auto* o : active())
      if (cheb(o->x, o->y, m.x, m.y) <= 3) {
        int res = hero_res(*o, elem::RIME), laid = 0;
        for (int i = 0; i < 2; ++i) if (res < 100 && (res <= 0 || rnd.below(100) >= res)) ++laid;
        o->stk.n[elem::RIME] = std::min(elem::MAX, o->stk.n[elem::RIME] + laid);
        o->stk.idle[elem::RIME] = 0;
        o->numb = std::max(o->numb, 2);
        hero_hurt(*o, &m, std::max(1, rnd.range(m.lo, m.hi) / 3), "cold");
      }
    say_near(m.x, m.y, "Frost runs out from the King's feet. The floor turns to ice under you.", 0x9CD2FF);
  } else if (mv == "ashveil") {  // Frost 4: he goes into ash and comes out somewhere else
    std::vector<P> spots;
    for (int dy = -8; dy <= 8; ++dy)
      for (int dx = -8; dx <= 8; ++dx) {
        int x = h.x + dx, y = h.y + dy, dd = cheb(x, y, h.x, h.y);
        if (dd >= 4 && dd <= 7 && floor->walk(x, y) && !mon_at(x, y) && !hero_at(x, y)) spots.push_back({x, y});
      }
    if (spots.empty()) return false;
    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) if (floor->walk(m.x + dx, m.y + dy)) floor->ground.push_back({{m.x + dx, m.y + dy}, G_SMOKE, 4, 0, 0});
    for (auto* o : active()) if (cheb(o->x, o->y, m.x, m.y) <= 1) { o->stk.n[elem::ASH] = std::min(elem::MAX, o->stk.n[elem::ASH] + 1); o->stk.idle[elem::ASH] = 0; }
    add_fx({{"k", "burst"}, {"x", m.x}, {"y", m.y}, {"r", 1}, {"col", 0x8A8A8A}});
    P c = rnd.pick(spots);
    m.x = c.x; m.y = c.y;
    add_fx({{"k", "inward"}, {"x", m.x}, {"y", m.y}, {"col", 0x8A8A8A}});
    say_near(m.x, m.y, "The King comes apart into ash, and stands again elsewhere.", 0xA0A0A0);
  } else if (mv == "hollowcourt") {  // Frost 7: his dead court climbs out of the floor
    int n = 0;
    for (auto& o : floor->mons) if (o->summoned) ++n;
    if (n >= 8) return false;
    std::string kind = "wraith";
    for (auto& k : m.def->moves) if (k.rfind("summon:", 0) == 0) kind = k.substr(7);
    int made = 0;
    for (auto* o : active())
      for (int i = 0; i < 2; ++i) {
        P c;
        if (free_near(o->x, o->y, 3, c, [&](int a, int b) { return floor->walk(a, b) && !mon_at(a, b) && !hero_at(a, b) && cheb(a, b, o->x, o->y) >= 2; }))
          if (auto s2 = spawn(kind, c.x, c.y)) { s2->summoned = true; s2->awake = true; s2->dormant = s2->hidden = 0; s2->name = "dead dwarf of the Hollow Court"; s2->col = 0x8C9AA8; ++made; }
      }
    if (!made) return false;
    add_fx({{"k", "wave"}, {"x", m.x}, {"y", m.y}, {"r", 4}, {"col", 0x8C9AA8}, {"sign", "sound"}});
    say_near(m.x, m.y, "The King lifts his hand. His Hollow Court climbs out of the stone around you.", 0x8C9AA8);
  } else if (mv == "blink") {
    P c;
    if (!free_near(h.x, h.y, 2, c)) return false;
    add_fx({{"k", "inward"}, {"x", m.x}, {"y", m.y}, {"col", m.col}});
    m.x = c.x; m.y = c.y;
  } else return false;
  return true;
}

void Game::tick() {
  for (auto* h : active()) {
    Stats st = stats(*h);
    if (h->poison > 0) {
      --h->poison;
      hero_hurt(*h, nullptr, h->poison_dmg, "poison");
      if (h->poison == 0) h->poison_dmg = 0;
      if (h->dead) continue;
    }
    if (h->numb > 0) --h->numb;
    if (h->shield > 0) --h->shield;
    if (turn % 3 == 0) h->mana = std::min(st.maxmana, h->mana + st.regen);
    if (h->standfast > 0) --h->standfast;
    if (h->fury_t > 0) --h->fury_t;
    if (int r = h->tal("pulse"); r && turn % (9 - 2 * r) == 0) h->hp += std::max(1, st.maxhp / 60);  // Steady Pulse
    if (h->tal("shared") && turn % 3 == 0)  // Shared Hearth
      for (auto* o : active()) if (o != h && cheb(o->x, o->y, h->x, h->y) <= 3) o->hp = std::min(stats(*o).maxhp, o->hp + std::max(1, stats(*o).maxhp / 80));
    if (h->tal("thaw") && turn % 2 == 0) {  // Thaw: what holds you wears off twice as fast
      if (h->numb > 0) --h->numb;
      if (h->held > 0) --h->held;
      if (h->dazed > 0) --h->dazed;
    }
    h->hp = std::min(h->hp, st.maxhp);
  }
  for (auto& m : floor->mons)
    if (m->has("Regenerating") && m->hp < m->maxhp && heal_cut(m->stk, m->rank == "boss") < 100)
      m->hp = std::min(m->maxhp, m->hp + std::max(1, m->maxhp / 50 * (100 - heal_cut(m->stk, m->rank == "boss")) / 100));
  stacks_tick();
  if (state != "play") return;
  classes_tick();
  legends_tick();
  if (state != "play") return;
  traits_tick();
  // a status that has just taken hold floats up over the hero once, in its own colour
  for (auto* h : active()) {
    struct S { int on; const char* word; int col; };
    const S ss[] = {{h->held, "held", 0xE0C060},   {h->dazed, "dazed", 0xC0A0E0},  {h->bleed, "bleeding", 0xD03030},
                    {h->brand, "branded", 0xFF8050}, {h->sunder, "sundered", 0xE0B080}, {h->latched, "latched", 0xE0D0A0},
                    {h->poison, "poisoned", 0x70D050}, {h->numb, "chilled", 0x90C8FF}, {h->ashen, "blinded", 0xA0A0A0}};
    int mask = 0;
    for (int i = 0; i < int(std::size(ss)); ++i)
      if (ss[i].on > 0) {
        mask |= 1 << i;
        if (!(h->shown & 1 << i) && !h->dead) num(h->x, h->y, ss[i].word, ss[i].col);
      }
    h->shown = mask;
  }
}

void Game::look() {
  TCOD_Map* map = TCOD_map_new(W, H);
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x) TCOD_map_set_properties(map, x, y, !floor->opaque(x, y), floor->walk(x, y));
  for (auto& g : floor->ground) if (g.kind == G_SMOKE) TCOD_map_set_properties(map, g.p.x, g.p.y, false, true);
  // for screenshots: PYREHEART_WHOLE shows the whole map at once, and nothing wakes for it
  static const bool whole_view = std::getenv("PYREHEART_WHOLE") != nullptr;
  for (auto& [id, hp] : heroes) {
    Hero& h = *hp;
    if (h.away) continue;
    int sight = (floor->deep ? SIGHT + 5 : SIGHT) + h.tal("lantern") + 3 * h.perk("lanternbound");  // the lava lights the Deep: you see further there
    bool dim = false;
    for (auto& m : floor->mons) if (m->awake && m->can("dim") && cheb(m->x, m->y, h.x, h.y) <= 5) dim = true;
    if (dim) sight = 3;
    else if (h.ashen > 0) sight = std::min(sight, 4);
    if (dim && !h.dimmed) { h.say("Your light gutters.", 0xC0A060); tip(h, "dim", "A lantern wraith eats the light. Kill it, and your light flares up again."); }
    h.dimmed = dim;
    if (omen(omen::LONGNIGHT)) sight = std::max(3, sight - 2);  // Long Night
    int glow = (h.carry.empty() ? 0 : 3) + (h.leg("drowned") ? 4 : 0) + (omen(omen::LISTEN) ? 2 : 0);  // and the dark listens  // an ember carried glows (and the Drowned's lantern): foes see its bearer from further off
    TCOD_map_compute_fov(map, h.x, h.y, sight + glow, true, FOV_SYMMETRIC_SHADOWCAST);
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x) {
        h.vis[y * W + x] = whole_view || (TCOD_map_is_in_fov(map, x, y) && cheb(x, y, h.x, h.y) <= sight);
        if (h.vis[y * W + x]) h.seen[y * W + x] = 1;
      }
    if (!h.present()) continue;
    for (auto& m : floor->mons)
      if (!m->awake && !m->dormant && !m->hidden && !m->sleep && !whole_view && !(h.perk("shadowstep") && rnd.below(4) == 0) &&
          (h.sees(m->x, m->y) || (glow && TCOD_map_is_in_fov(map, m->x, m->y) && cheb(m->x, m->y, h.x, h.y) <= sight + glow))) {
        m->awake = true;
        if (m->rank == "champion") tip(h, "champion", "Blue foes are champions: stronger, and they carry better things.");
        if (m->rank == "unique") tip(h, "unique", "A gold name is a unique foe. Look at foes with [l].");
      }
    if (floor->boss && !floor->boss_woke && h.sees(floor->boss->x, floor->boss->y)) {
      floor->boss_woke = true;
      say_all(lore::BOSS_WAKE.at(floor->boss->def->key), 0xFFD27F);
    }
  }
  TCOD_map_delete(map);
}

// ------------------------------------------------------------------ the Pyre's gifts
// Drops: the Frost makes them deeper, and Good Eye (the best in the party) sometimes makes them a rarity better.
ItemP Game::roll_loot(int d, const std::vector<std::string>& kinds, const std::string& rarity, const std::string& at_least) {
  d += 3 * frost;
  int band = frost_band();
  // legendaries: from Frost 5, a thin chance on every drop that grows with the Frost
  if (rarity.empty() && frost >= 5 && rnd.real() < .002 + .001 * frost) return loot::roll(d, rnd, kinds, "legendary", "normal", band, frost);
  auto it = loot::roll(d, rnd, kinds, rarity, at_least, band, frost);
  int eye = 0;
  for (auto* h : living()) eye = std::max(eye, h->tal("goodeye"));
  if (rarity.empty() && eye && rnd.below(100) < 3 * eye) {
    std::string up = it->rarity == "normal" ? "magic" : it->rarity == "magic" ? "rare" : it->rarity == "rare" ? "unique" : "";
    if (!up.empty()) it = loot::roll(d, rnd, kinds, up, "normal", band, frost);
  }
  return it;
}

// Spark-Forged: once a dive, at a camp fire, the weapon's magic is rolled again (the same kind of weapon, as good)
void Game::forge(Hero& h) {
  if (!h.tal("forged")) return;
  if (!floor->camp) { h.say("Only at a camp's fire.", 0x909090); return; }
  if (h.forged) { h.say("The Spark has already reforged once this dive.", 0x909090); return; }
  auto wi = h.equip.find("weapon");
  if (wi == h.equip.end() || wi->second->rarity == "normal" || wi->second->rarity == "unique" || wi->second->rarity == "legendary") {
    h.say("Only a magic or rare weapon can be reforged.", 0x909090);
    return;
  }
  ItemP old = wi->second, best;
  for (int i = 0; i < 40 && !best; ++i) {
    auto it = loot::roll(lvl + 1 + 3 * frost, rnd, {old->kind}, old->rarity, "normal", old->band, frost);
    if (it->base == old->base) best = it;
  }
  if (!best) best = loot::roll(lvl + 1 + 3 * frost, rnd, {old->kind}, old->rarity, "normal", old->band, frost);
  for (int i = 0; i < old->plus; ++i) loot::temper(*best);
  if (best->slot != "weapon") return;
  h.forged = true;
  wi->second = best;
  add_fx({{"k", "burst"}, {"x", h.x}, {"y", h.y}, {"r", 1}, {"col", 0xFFB040}});
  h.say("The Spark reforges " + old->name + " into " + best->name + ".", rarity_col(best->rarity));
}

// the Prison: a dead prisoner in its cell gets up, rattling its chains, when one of the living comes close
void Game::prison_tick() {
  Floor& f = *floor;
  for (auto& h : living())
    for (size_t k = 0; k < f.open_graves.size(); ++k) {
      P g = f.open_graves[k];
      if (cheb(g.x, g.y, h->x, h->y) > 3 || mon_at(g.x, g.y) || rnd.below(100) >= 40) continue;
      f.open_graves.erase(f.open_graves.begin() + long(k));
      f.set(g.x, g.y, FLOOR);
      if (auto m = spawn("prisoner", g.x, g.y)) {
        m->awake = true;
        add_fx({{"k", "rise"}, {"x", g.x}, {"y", g.y}, {"col", 0xB0A090}});
        say_all("The bones in a cell stir. A dead prisoner drags itself up, its chains scraping the stone.", 0xC0B090);
        tip(*h, "prisoners", "In the Prison the dead in the cells rise when you come close.");
      }
      return;
    }
}

// Charnfen: the dead climb from the open graves as heroes pass, and the guards at the gate shoot what comes near
void Game::town_tick() {
  Floor& f = *floor;
  for (auto* h : active())  // back from the King: a guard at the gate calls out, once
    if (h->frost_open > 0 && !h->gate_called)
      for (int i = 0; i < W * H; ++i)
        if (f.t[i] == GUARD && cheb(i % W, i / W, h->x, h->y) <= 5) {
          h->gate_called = true;
          h->say("A guard calls out: \"Back from the King? The cold below has grown. Tell me how deep you mean to go.\"", 0xA8C0E0);
          tip(*h, "gate", "Walk into the gate guard to choose the Frost level of this dive: any up to one above the highest you have beaten.");
          break;
        }
  int most = 1 + int(living().size());  // one or two alone, one more for each friend
  for (auto& h : living()) {
    if (!f.warned)
      for (int i = 0; i < W * H; ++i)
        if (f.t[i] == GUARD && cheb(i % W, i / W, h->x, h->y) <= 3) {
          f.warned = true;
          say_all("A guard calls out: \"They rise every night now. Mind the open graves.\"", 0xA8C0E0);
          break;
        }
    for (auto& g : f.open_graves) {
      if (f.risen >= most || cheb(g.x, g.y, h->x, h->y) > 5 || mon_at(g.x, g.y) || rnd.below(100) >= 35) continue;
      if (auto m = spawn("shambler", g.x, g.y)) {
        m->awake = true;
        ++f.risen;
        add_fx({{"k", "rise"}, {"x", g.x}, {"y", g.y}, {"col", 0x8CA070}});
        say_all("The earth of an open grave heaves. A bloated corpse claws its way out.", 0xC0B090);
      }
    }
  }
  // the guards hold the gate: anything dead that comes within a few steps of them takes a bolt
  for (int i = 0; i < W * H; ++i) {
    if (f.t[i] != GUARD) continue;
    int gx = i % W, gy = i / W;
    for (auto& m : std::vector<MonP>(f.mons)) {
      if (m->hp <= 0 || cheb(m->x, m->y, gx, gy) > 4) continue;
      std::vector<P> cells;
      int n = std::max(std::abs(m->x - gx), std::abs(m->y - gy));
      for (int k = 1; k <= n; ++k) cells.push_back({gx + (m->x - gx) * k / n, gy + (m->y - gy) * k / n});
      add_fx({{"k", "bolt"}, {"cells", cells_json(cells)}, {"col", 0xDCDCC8}, {"ch", "-"}, {"src", xy(gx, gy)}});
      say_near(m->x, m->y, "A guard's bolt strikes " + the(*m) + ".", 0xA8C0E0);
      hurt(*m, 6 + int(rnd.below(5)), nullptr, "");
      break;
    }
  }
}
