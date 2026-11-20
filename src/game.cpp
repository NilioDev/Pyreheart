#include "game.hpp"

#include <libtcod/bresenham.hpp>
#include <libtcod/fov.h>

#include <cmath>
#include <deque>
#include <fstream>

#include "k5.hpp"
#include "pyre.hpp"
#include "lore.hpp"

using namespace data;

const ClassDef& class_def(const std::string& key) {
  for (auto& c : CLASSES) if (c.key == key) return c;
  return CLASSES[0];
}
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

Game::Game(uint64_t seed, int frost_) : frost(std::clamp(frost_, 0, pyre::FROST_MAX)), rnd(std::getenv("PYREHEART_SEED") ? std::strtoull(std::getenv("PYREHEART_SEED"), nullptr, 10) : seed) {
  // for screenshots and tests: PYREHEART_ZONE=vaults starts the dive in that zone (by the first way there), and
  // PYREHEART_BOSS=1 on its boss's map, next to it; PYREHEART_ROUTE=cistern,vaults,prison picks the ways a whole dive takes
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
  floor = make_floor(depth, std::getenv("PYREHEART_REST") != nullptr);  // PYREHEART_REST=1: the waystation after it
  // a first look at Charnfen, drawn by hand: PYREHEART_TOWN=ref/charnfen.txt
  if (const char* town = std::getenv("PYREHEART_TOWN")) {
    std::ifstream in(town);
    std::string line;
    auto f = std::make_unique<Floor>();
    f->depth = 0; f->act = 0; f->rest = true; f->id = floor_ids_++; f->w = W; f->h = H;
    static const std::map<std::string, int> KEY = {
        {"#", WALL}, {".", SNOW}, {":", PATH}, {"+", DOOR}, {"n", GRAVE}, {"t", CROSS}, {"\u2663", TREE}, {"\u2261", PEW},
        {"\u2021", ALTAR}, {"E", EMBER}, {"O", WELL}, {"?", BOARD}, {"w", WINDOW}, {"L", LIT}, {"\u2248", ICE}, {"%", CRACK},
        {">", STAIRS}, {"<", UPSTAIRS}, {"o", PILLAR}, {"_", FLAGS}, {"F", FIRE}, {"r", STONES}, {"C", CHIMNEY}, {"h", HEALER},
        {"b", SMITH}, {"g", DIGGER}, {"T", TORCH}, {"i", CANDLE}, {"U", OPENGRAVE}, {"G", GUARD}, {"X", BARRICADE}, {",", RUBBLE},
        {"$", PATH}, {"@", PATH}};
    for (int y = 0; y < H && std::getline(in, line); ++y) {
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
    floor = std::move(f);
    depth = 0;
  }
}

bool Game::final_floor() const { return route.back() == DEEP && zfloor == ACTS[DEEP].floors - 1 && !floor->rest; }

void Game::set_level() {
  const ActDef& z = ACTS[route.back()];
  double at = z.step + double(zfloor) / z.floors;
  lvl = std::clamp(1 + int(std::lround(15 * at / (STEPS - .5))), 1, 16);
}

// ------------------------------------------------------------------ players
Hero& Game::join(int pid, const std::string& name, const std::string& cls, const json& talents) {
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
  h->cls = &class_def(cls);
  if (talents.is_object()) {
    std::map<std::string, int> want;
    for (auto& [k, v] : talents.items()) if (v.is_number_integer()) want[k] = v.get<int>();
    h->talents = pyre::clean(want);
  }
  h->heal += 2 * h->tal("credit");
  for (auto& b : h->cls->start) { auto it = loot::make(b); h->equip[it->slot] = it; }
  // for screenshots and tests: start with a full pack of things to compare
  if (const char* g = std::getenv("PYREHEART_GIFTS"))
    for (int i = 0; i < std::atoi(g) && int(h->pack.size()) < PACK_MAX; ++i) h->pack.push_back(loot::roll(3, rnd, h->cls->kinds));
  if (std::getenv("PYREHEART_BOSS") || std::getenv("PYREHEART_ZONE")) h->lvl = std::max(1, lvl * 2 - 1);
  h->hp = stats(*h).maxhp;
  h->mana = stats(*h).maxmana;
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
  if (heroes.size() > 1) say_all(r.name + " the " + r.cls->name + " joins the dive.", 0xB4DCB4);
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
  if (floor->depth == 0) { h.x = floor->up.x; h.y = floor->up.y; h.seen.assign(W * H, 1); return; }
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
  bool bow = w && w->kind == "bow";
  int ar = 0;
  for (auto& [slot, it] : h.equip) ar += it->ar;
  auto res = [&](const std::string& k, int more = 0) { return std::min(75, s(k) + 20 * h.perk("ward") + more); };
  Stats st{};
  st.maxhp = h.cls->hp + h.cls->hp_lvl * (h.lvl - 1) + 15 * h.perk("tough") + s("hp") + 5 * h.tal("hearthskin");
  st.maxmana = h.cls->fury() ? 10 : h.cls->mana + h.cls->mana_lvl * (h.lvl - 1) + 15 * h.perk("well") + s("mana");
  st.lo = (w ? w->lo : 1) + s("dmg");
  st.hi = (w ? w->hi : 3) + s("dmg");
  st.pct = s("dmg_pct") + 15 * h.perk("brute") + 4 * h.tal("kindled") + (h.fury_t > 0 ? 50 : 0);
  if (h.hp < st.maxhp / 2) st.pct += 5 * h.tal("fevered");
  st.acc = h.cls->acc + h.lvl + 8 * h.perk("eye") + s("acc");
  st.crit = 5 + 4 * h.perk("eye") + s("crit") + 2 * h.tal("coaleyed") + (bow ? 10 * (h.perk("sniper") + h.perk("sharpshooter")) : 0);
  st.armour = ar + s("armour") + 3 * h.perk("hide") + 4 * h.perk("guardian");
  if (h.sunder > 0) st.armour /= 2;  // split by acid or a smith's hammer
  st.res_f = res("res_fire"); st.res_c = res("res_cold", 10 * h.tal("warmhands")); st.res_p = res("res_poison");
  st.leech = s("leech"); st.spell = s("spell"); st.fire = s("fire_dmg");
  st.regen = 1 + h.perk("well") + s("regen"); st.thorns = s("thorns");
  st.bow = bow;
  return st;
}

int Game::need(int lvl) const { return rnd_i(10 * std::pow(lvl + 1.0, 2.2)); }  // total xp to reach lvl + 1

bool Game::can_use(const Hero& h, const Item& it) const {
  return it.slot != "weapon" || std::find(h.cls->kinds.begin(), h.cls->kinds.end(), it.kind) != h.cls->kinds.end();
}

// Puts the item on a copy of the hero and measures what moved. The weights only decide the one-glance mark;
// the player still sees every number.
Verdict Game::compare(const Hero& h, const Item& it) const {
  Verdict out;
  if (!can_use(h, it)) { out.v = -9; return out; }
  Hero t;
  t.cls = h.cls; t.lvl = h.lvl; t.perks = h.perks; t.equip = h.equip;
  t.equip[it.slot] = std::make_shared<Item>(it);
  Stats a = stats(h), b = stats(t);
  bool mage = h.cls->key == "adept" || h.cls->key == "runecarver";
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
  if (h.cls->fury()) out.d.erase(std::remove_if(out.d.begin(), out.d.end(), [](const Change& c) { return c.what == "mana"; }), out.d.end());
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
std::unique_ptr<Floor> Game::make_floor(int d, bool rest) {
  auto f = std::make_unique<Floor>();
  f->depth = d; f->act = route.back(); f->rest = rest; f->id = floor_ids_++;
  Floor* old = floor.release();
  floor.reset(f.release());
  delete old;
  Floor& fl = *floor;
  set_level();
  if (rest) { build_waystation(fl); return std::move(floor); }
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
  int budget = int((14 + lvl * 1.5) * room) - int(fl.sleepers.size());
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
  if (lvl >= 2 && rnd.real() < .65 && !far.empty()) {
    std::string kind = pick_kind();
    std::string aff = rnd.pick(AFFIXES).first;
    P c = rnd.pick(far);
    std::vector<std::string> mods{aff};
    if (frost > 0 && rnd.real() < .35 * frost) {  // Frost: a second affix
      std::string more = rnd.pick(AFFIXES).first;
      if (more != aff) mods.push_back(more);
    }
    for (int i = 0; i < 3; ++i) if (auto m = spawn_near(kind, c)) champion(*m, mods);
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
    P c = rnd.pick(far);
    for (int i = 0; i < n; ++i) spawn_near(kind, c);
    budget -= n;
  }
  zone_foes(fl);
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

void Game::build_waystation(Floor& f) {
  // one stair down for each way on from the zone behind you; '1' and '2' are the stairs to the first and second
  const auto& next = ACTS[route.back()].next;
  std::vector<std::string> art = {
      "##########################",
      "#...........M............#",
      "#..{....................1#",
      "#........................#",
      "#<.......................#",
      "#........................#",
      "#.......................2#",
      "##########################"};
  if (next.size() < 2) { art[2][24] = '.'; art[6][24] = '.'; art[4][24] = '1'; }
  int ox = (W - int(art[0].size())) / 2, oy = (H - int(art.size())) / 2;
  f.w = W; f.h = H;
  for (int j = 0; j < int(art.size()); ++j)
    for (int i = 0; i < int(art[j].size()); ++i) {
      int x = ox + i, y = oy + j;
      char ch = art[j][i];
      f.t[y * W + x] = ch == '#' ? WALL : ch == '{' ? FOUNTAIN : ch == '<' ? UPSTAIRS : (ch == '1' || ch == '2') ? STAIRS : FLOOR;
      if (ch == '<') f.up = {x, y};
      if (ch == '1' || ch == '2') {
        int k = ch - '1';
        if (k < int(next.size())) f.exits[y * W + x] = next[k];
        if (k == 0) f.down = {x, y};
      }
      if (ch == 'M') { f.merchant = {x, y}; f.has_merchant = true; }
    }
  auto kinds = party_kinds();
  int wares = 5;
  for (auto& [id, h] : heroes) if (h->tal("friend")) wares = 6;
  for (int i = 0; i < wares; ++i) f.shop.push_back(roll_loot(lvl + 1, kinds, "", "magic"));
}

MonP Game::spawn(const std::string& key, int x, int y) {
  auto m = std::make_shared<Mon>();
  const MonDef& d = mon_def(key);
  m->id = mon_id_++; m->def = &d; m->x = x; m->y = y;
  m->name = d.name; m->col = d.col; m->lo = d.lo; m->hi = d.hi; m->acc = d.acc; m->ar = d.ar; m->spd = d.spd;
  m->xp = d.xp * 2;
  m->rank = d.boss ? "boss" : "normal";
  double k = 1 + .6 * (std::max<size_t>(1, living().size()) - 1);
  m->maxhp = rnd_i(d.hp * k * (1 + .4 * frost)); m->hp = m->maxhp;
  if (frost > 0) {  // the deeper the frost, the harder they come
    m->lo = rnd_i(m->lo * (1 + .25 * frost)); m->hi = rnd_i(m->hi * (1 + .25 * frost));
    m->acc += 3 * frost; m->ar += frost;
    m->xp = rnd_i(m->xp * (1 + .3 * frost));
  }
  m->energy = rnd.below(100);
  if (d.has("sleeper")) m->dormant = 1;
  if (d.has("disguise")) m->hidden = 1;
  floor->mons.push_back(m);
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
}

void Game::unique(Mon& m) {
  m.rank = "unique";
  std::vector<std::string> all;
  for (auto& [k, v] : AFFIXES) all.push_back(k);
  rnd.shuffle(all);
  m.mods.assign(all.begin(), all.begin() + rnd.range(2, 3));
  apply_mods(m);
  m.maxhp *= 4; m.hp = m.maxhp;
  m.lo = rnd_i(m.lo * 1.5); m.hi = rnd_i(m.hi * 1.5); m.acc += 12; m.ar += 2; m.xp *= 6;
  m.col = 0xE0B050;
  m.name = rnd.pick(UNIQUE_FIRST) + " " + rnd.pick(UNIQUE_EPITHET);
}

void Game::next_floor() {
  bool was_rest = floor->rest;
  const ActDef& zone = ACTS[route.back()];
  // after a zone's boss comes a waystation, and from there the stairs the party took lead into the next zone
  bool to_rest = !was_rest && zfloor == zone.floors - 1;
  if (was_rest) {
    route.push_back(chosen_ >= 0 ? chosen_ : zone.next.empty() ? route.back() : zone.next[0]);
    zfloor = 0;
    chosen_ = -1;
  } else if (!to_rest) ++zfloor;
  if (!to_rest) ++depth;
  floor = make_floor(depth, to_rest);
  scaled_ = 1.0;
  rescale();
  for (auto& [id, h] : heroes) {
    h->seen.assign(W * H, 0);
    if (int r = h->tal("sense"))  // Treasure Sense: the chests (and shrines) of a new floor are known at once
      for (int i = 0; i < W * H; ++i) if (floor->t[i] == CHEST || (r >= 2 && floor->t[i] == SHRINE)) h->seen[i] = 1;
  }
  for (auto& [id, h] : heroes) {
    h->gone = false; h->shopping = false; h->drank = false;
    if (to_rest && h->dead && !h->away) {
      h->dead = false; h->hp = stats(*h).maxhp / 2; h->poison = 0;
      say_all(h->name + " wakes at the waystation, pale but alive.", 0xB4DCB4);
    }
    if (!h->away && !h->dead) place(*h);
  }
  if (to_rest) {
    say_all(lore::WAYSTATION, 0xD2B48C);
    for (auto* h : living())
      if (h->tal("forged") && !h->forged) h->say("The Spark in you could reforge your weapon's magic here, once this dive: [r].", 0xFFB040);
    auto& next = ACTS[route.back()].next;
    if (next.size() > 1)
      say_all("Two stairs lead on: to " + ACTS[next[0]].name + " and to " + ACTS[next[1]].name + ". Look at them with [l].",
              0xA0A0A0);
  } else {
    if (zfloor == 0) {
      auto& open = lore::ACT_OPEN[floor->act];
      say_all(open[0] + ": " + open[1], 0xFFD27F);
    }
    say_all("Depth " + std::to_string(depth) + ". " + ACTS[floor->act].name + ".", 0xA0A0A0);
  }
  look();
}

// ------------------------------------------------------------------ a step of the world
bool Game::free_action(Hero& h, const json& a) {
  std::string t = a.value("t", "");
  if (t == "perk") { choose_perk(h, a.value("k", "")); return true; }
  if (t == "buy") { buy(h, a.value("i", -1)); return true; }
  if (t == "buyp") { buy_potion(h, a.value("k", "")); return true; }
  if (t == "sell") { sell(h, a.value("id", 0)); return true; }
  if (t == "close") { h.shopping = false; return true; }
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
    if (hh.dazed > 0) { --hh.dazed; hh.say("You reel, and lose the turn.", 0xC0A0E0); continue; }
    act(hh, a == actions.end() ? json{{"t", "wait"}} : a->second);
    if (state != "play") return;
  }
  check_stairs();
  if (!floor->rest || floor->depth == 0) monsters();
  tick();
  look();
  if (!floor->rest) { wake_dead(); look(); }
  if (living().empty() && !heroes.empty() && state == "play") {
    state = "over";
    say_all("The dark takes the last of you. Charnfen waits in vain.", 0xFF6060);
  }
}

void Game::act(Hero& h, const json& a) {
  std::string t = a.value("t", "");
  if (t == "move") move(h, std::clamp(a.value("dx", 0), -1, 1), std::clamp(a.value("dy", 0), -1, 1));
  else if (t == "skill") skill(h, a.value("n", 0), a.value("x", 0), a.value("y", 0));
  else if (t == "shoot") shoot(h, a.value("x", 0), a.value("y", 0));
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
  if (auto m = mon_at(nx, ny)) { melee(h, *m); return; }
  if (hero_at(nx, ny)) return;
  if (floor->has_merchant && floor->merchant == P{nx, ny}) {
    h.shopping = true;
    h.say("The trader nods. 'Dwarf gold spends the same as any.'", 0xD2B48C);
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
      else if (fi.pot == "mana" && h.mana_pots < BELT_MAX) { ++h.mana_pots; h.say("A mana potion.", 0x7090FF); }
      else took = false;
    } else if (fi.item) {
      if (int(h.pack.size()) >= PACK_MAX) { h.say("Your pack is full: " + fi.item->name + " stays here.", 0x909090); took = false; }
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
  if (floor->exits.size() > 1) {
    // two ways down: the party goes together, the way most of them stand on (the first hero's if it is even)
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
  if (h.cls->key == "ranger") h.focus = std::min(h.perk("sharpshooter") ? 5 : 3, h.focus + 1);
}

void Game::bleed(Hero& h, Mon& m, int dmg) {
  if (m.can("stone")) return;
  bool poison = h.perk("poisoner") > 0;
  m.bleed = poison ? 5 : 3;
  m.bleed_dmg = std::max(m.bleed_dmg, std::max(1, poison ? dmg / 3 : dmg / 4));
}

bool Game::strike(Hero& h, Mon& m, double mult, bool ranged) {
  Stats st = stats(h);
  bool opening = h.cls->key == "cutthroat" && !ranged && busy(m, h);
  m.awake = true;
  m.open = 0;
  int chance = std::clamp(st.acc - m.ar * 2 + 12 + (opening ? 30 : 0), 25, 97);
  if (rnd.below(100) >= chance) { h.say("You miss " + the(m) + ".", 0x909090); num(m.x, m.y, "miss", 0x808080); return false; }
  double d = rnd.range(st.lo, st.hi) * (1 + st.pct / 100.0) * mult;
  if (h.numb > 0) d *= .75;
  if (h.perk("berserker")) d *= 1 + .03 * h.mana;
  if (ranged && h.cls->key == "ranger") d *= 1 + .2 * h.focus;
  if (m.frozen > 0 && h.perk("frostbinder")) d *= 1.25;
  if (h.perk("duelist")) {
    if (h.combo_id == m.id) h.combo = std::min(6, h.combo + 1); else { h.combo_id = m.id; h.combo = 1; }
    d *= 1 + .12 * (h.combo - 1);
  }
  if (h.cls->fury()) h.mana = std::min(10, h.mana + 1);
  bool crit = opening || rnd.below(100) < st.crit;
  if (crit) { d *= opening ? 2.5 : 2; add_fx({{"k", "crit"}, {"x", m.x}, {"y", m.y}}); }
  int dmg = std::max(1, rnd_i(d - m.ar / 2.0));
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
  crit_next = crit;
  hurt(m, dmg, &h, opening ? "An opening: you cut " + the(m) + " deep (" + std::to_string(dmg) + ")."
                   : crit ? "You strike " + the(m) + " hard (" + std::to_string(dmg) + ")." : "");
  return true;
}

void Game::melee(Hero& h, Mon& m) {
  add_fx({{"k", "swing"}, {"src", xy(h.x, h.y)}, {"cells", json::array({xy(m.x, m.y)})}});
  strike(h, m, stats(h).bow ? .5 : 1.0);
}

void Game::hurt(Mon& m, int dmg, Hero* by, const std::string& text) {
  dmg = trait_hurt(m, dmg);
  if (m.spent > 0) dmg = dmg * 3 / 2;
  m.hp -= dmg;
  num(m.x, m.y, std::to_string(dmg), crit_next ? 0xFFD25A : 0xFFFFFF);
  crit_next = false;
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
                   {"toll", "speaks a word of the old dwarves. Break it!"}, {"pyre", "opens his chest. The Pyreheart blazes. Find the gold!"}}},
};

bool Game::wind_up(Mon& m, Hero& h, const std::string& name) {
  const BlowDef& bd = BLOWS.at(name);
  Blow b;
  b.name = name; b.kind = bd.kind; b.turns = bd.turns; b.heavy = bd.heavy;
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
  if (b.heavy) {
    m.spent = 2;
    say_near(m.x, m.y, cap(the(m)) + " is spent, and stands open.", 0xE0D080);
    for (auto* o : active()) if (o->sees(m.x, m.y)) tip(*o, "spent", "A spent foe takes half again as much harm. Strike now.");
  }
}

void Game::boss_phase(Mon& m) {
  int ph = m.hp * 3 <= m.maxhp ? 2 : m.hp * 3 <= 2 * m.maxhp ? 1 : 0;
  if (ph <= m.phase) return;
  m.phase = ph;
  m.deck.clear();
  m.cool = 1;
  say_near(m.x, m.y, cap(the(m)) + (ph == 1 ? " staggers, and fights harder." : " is badly hurt. Now it holds nothing back."), 0xFFB060);
  Hero* h = nearest_hero(m, false);
  if (h) boss_move(m, *h, cheb(h->x, h->y, m.x, m.y), "summon");
}

void Game::kill(Mon& m, Hero* by) {
  m.hp = 0;
  MonP keep;
  for (auto it = floor->mons.begin(); it != floor->mons.end(); ++it)
    if (it->get() == &m) { keep = *it; floor->mons.erase(it); break; }
  int col = m.rank == "champion" ? 0x7090FF : (m.rank == "unique" || m.rank == "boss") ? 0xE0B050 : 0xC8C8C8;
  say_near(m.x, m.y, cap(the(m)) + " dies.", col);
  if (by) {
    ++by->kills;
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
  for (auto* h : living()) gain_xp(*h, m.xp);
  trait_death(m);
  drops(m);
  if (floor->boss.get() == &m) {
    slain.push_back(ACTS[floor->act].key);
    say_all(lore::BOSS_DIES.at(m.def->key), 0xFFD27F);
    if (final_floor()) {
      state = "won";
      for (auto& l : lore::VICTORY) say_all(l, 0xFFD27F);
    } else floor->set(floor->down.x, floor->down.y, STAIRS);
  }
}

void Game::gain_xp(Hero& h, int xp) {
  // the zones are short and a boss waits at the end of each, so a hero behind the level the map is made for learns
  // faster, and one far ahead slower: the fights stay fights
  double target = 3 + 1.7 * lvl, gap = target - h.lvl;
  h.xp += std::max(1, rnd_i(xp * std::clamp(std::pow(2.0, gap / 3), .25, 4.0)));
  while (h.xp >= need(h.lvl)) {
    ++h.lvl;
    Stats st = stats(h);
    h.hp = std::min(st.maxhp, h.hp + st.maxhp * 3 / 10 + h.cls->hp_lvl);
    h.mana = st.maxmana;
    h.say("You reach level " + std::to_string(h.lvl) + "!", 0xFFE680);
    add_fx({{"k", "rise"}, {"x", h.x}, {"y", h.y}, {"col", 0xFFE680}});
    for (int i = 0; i < 4; ++i)
      if (SKILL_LEVELS[i] == h.lvl && i < int(h.cls->skills.size()))
        h.say("You can now use " + skill_def(h.cls->skills[i]).name + ": [" + std::to_string(i + 1) + "].", 0xFFE680);
    if (h.lvl == PATH_LEVEL && !h.cls->paths.empty()) {
      h.perk_choices.push_back(h.cls->paths);
      tip(h, "path", "Choose your path: [p]. It decides which perks come to you after.");
      continue;
    }
    std::vector<std::string> pool;
    for (auto& p : PERKS) {
      if (!p.cls.empty() && p.cls != h.cls->key) continue;
      if (p.path == "*" || (!p.path.empty() && !h.perk(p.path)) || h.perk(p.key) >= p.max) continue;
      if (p.key == "well" && h.cls->fury()) continue;
      pool.push_back(p.key);
    }
    rnd.shuffle(pool);
    if (pool.size() > 3) pool.resize(3);
    if (!pool.empty()) { h.perk_choices.push_back(pool); tip(h, "perk", "You may learn a perk: [p]."); }
  }
}

void Game::choose_perk(Hero& h, const std::string& k) {
  if (h.perk_choices.empty()) return;
  auto& ch = h.perk_choices.front();
  if (std::find(ch.begin(), ch.end(), k) == ch.end()) return;
  h.perk_choices.erase(h.perk_choices.begin());
  h.perks[k] = h.perk(k) + 1;
  h.say("You learn " + perk_def(k).name + ".", 0xFFE680);
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
  if (rnd.real() < .35 || m.rank != "normal")
    drop_at({0, 0, nullptr, rnd_i(lvl * rnd.range(2, 5) * rank_mul * (m.rank == "normal" ? 1.0 : .6)), "", rnd.pick(owners)->pid});
  if (rnd.real() < (m.rank == "normal" ? .14 : .5))
    drop_at({0, 0, nullptr, 0, rnd.real() < .65 ? "heal" : "mana", rnd.pick(owners)->pid});
  if (m.rank == "boss") {
    // the great foes give each hero two things worth having
    for (auto* o : owners) {
      drop_at({0, 0, roll_loot(lvl + 1, o->cls->kinds, "", "rare"), 0, "", o->pid});
      drop_at({0, 0, roll_loot(lvl + 1, kinds, "", "magic"), 0, "", o->pid});
    }
    if (rnd.real() < .3) drop_at({0, 0, roll_loot(lvl, kinds, "unique"), 0, "", rnd.pick(owners)->pid});
    return;
  }
  std::vector<ItemP> items;
  if (m.rank == "unique") { items.push_back(roll_loot(lvl, kinds, "", "rare")); items.push_back(roll_loot(lvl, kinds, "", "magic")); }
  else if (m.rank == "champion") { if (m.def->gear || rnd.real() < .4) items.push_back(roll_loot(lvl, kinds, "", "magic")); }
  else if (m.def->gear && rnd.real() < .12) items.push_back(roll_loot(lvl, kinds));
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
    auto it = roll_loot(lvl, o->cls->kinds, rnd.real() < .45 ? "magic" : "");
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
      for (auto* o : living()) gain_xp(*o, 10 * lvl + 10);
      say_all(h.name + " touches the shrine: old knowledge settles in you.", 0xBEA0FF);
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
  if (n < 1 || n > int(h.cls->skills.size())) return;
  const std::string& key = h.cls->skills[n - 1];
  const SkillDef& sk = skill_def(key);
  if (h.lvl < SKILL_LEVELS[n - 1]) { h.say(sk.name + " comes to you at level " + std::to_string(SKILL_LEVELS[n - 1]) + ".", 0x909090); return; }
  int cost = sk.mana;
  if (key == "charge" && h.perk("frenzy")) cost = 0;
  if (key == "tumble") cost -= 2 * h.perk("quick");
  if (key == "kindle" && h.perk("deeprunes")) cost = 0;
  if (h.mana < cost) { h.say(std::string(h.cls->fury() ? "Not enough fury" : "Not enough mana") + " for " + sk.name + ".", 0x7090FF); return; }
  if (sk.target && !target(h, x, y)) { h.say("No target.", 0x909090); return; }
  Stats st = stats(h);
  double sp = 1 + st.spell / 100.0;
  if ((key == "volley" || key == "pierce" || key == "tumble" || key == "pin") && !st.bow) { h.say(sk.name + " needs a bow.", 0x909090); return; }
  if (key != "cleave" && key != "warcry" && key != "volley" && key != "pierce" && key != "firebolt" && key != "nova") {
    if (skill_new(h, key, x, y, st, sp)) h.mana -= cost;
    return;
  }
  h.mana -= cost;
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
    int dmg = rnd_i((rnd.range(4, 9) + 1.3 * h.lvl) * sp * (h.perk("pyromancer") ? 1.3 : 1.0));
    if (m->def->fire) dmg /= 2;
    if (m->frozen > 0) {
      dmg *= 3;
      m->frozen = 0;
      add_fx({{"k", "burst"}, {"x", m->x}, {"y", m->y}, {"r", 1}, {"col", 0xC8E6FF}, {"delay", .1}});
      h.say("The frost on " + the(*m) + " shatters (" + std::to_string(dmg) + ")!", 0xC8E6FF);
      tip(h, "shatter", "Fire on a frozen foe shatters it for three times the harm.");
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
        int dmg = rnd_i((rnd.range(3, 6) + .8 * h.lvl) * sp);
        m->frozen = m->rank == "boss" ? 1 : 2 + h.perk("deepfrost");
        hurt(*m, m->def->cold ? dmg / 2 : dmg, &h);
      }
  }
}

// ------------------------------------------------------------------ the skills that came with the six classes
int Game::rune_dmg(const Hero& h) {
  double sp = 1 + stats(h).spell / 100.0;
  return rnd_i((rnd.range(6, 10) + 1.8 * h.lvl) * sp * (h.perk("fireward") ? 1.3 : 1.0));
}

void Game::burst_rune(Hero& h, P at) {
  int r = h.perk("fireward") ? 2 : 1;
  add_fx({{"k", "burst"}, {"x", at.x}, {"y", at.y}, {"r", r}, {"col", 0x50E6E6}});
  int dmg = rune_dmg(h);
  auto ms = floor->mons;
  for (auto& m : ms) {
    if (m->hp <= 0 || cheb(m->x, m->y, at.x, at.y) > r) continue;
    int d = m->pinned > 0 && h.perk("heavyrunes") ? dmg * 5 / 4 : dmg;
    if (h.perk("stoneward")) m->pinned = std::max(m->pinned, 2);
    hurt(*m, m->def->fire ? d / 2 : d, &h);
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

// every turn: the lantern's light, burning and hallowed ground, bleeding, held foes, fury draining away
void Game::classes_tick() {
  for (auto* h : active()) {
    if (h->cls->key == "lamplighter") {
      int r = h->perk("shepherd") ? 3 : 2;
      for (auto* o : active()) {
        if (cheb(o->x, o->y, h->x, h->y) > r) continue;
        o->hp = std::min(stats(*o).maxhp, o->hp + 1 + h->perk("warmth") + h->lvl / 6);
      }
      auto ms = floor->mons;
      for (auto& m : ms)
        if (m->hp > 0 && m->def->undead && cheb(m->x, m->y, h->x, h->y) <= r && h->sees(m->x, m->y)) {
          m->awake = true;
          hurt(*m, 1 + h->lvl / 3, h);
        }
    }
    if (h->cls->fury() && turn % 3 == 0 && h->mana > 0) {
      bool fight = false;
      for (auto& m : floor->mons) if (m->awake && h->sees(m->x, m->y)) fight = true;
      if (!fight) --h->mana;
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
    int dmg = rnd_i((3 + .8 * h.lvl) * sp * (h.perk("pyromancer") ? 1.3 : 1.0));
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
    strike(h, *m, mult);
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
      int heal = rnd_i((os.maxhp * .25 + 2 * h.lvl) * (h.perk("shepherd") ? 1.5 : 1.0));
      o->hp = std::min(os.maxhp, o->hp + heal);
      o->poison = 0;
      add_fx({{"k", "rise"}, {"x", o->x}, {"y", o->y}, {"col", 0xFFE68C}});
      o->say(o == &h ? "Your wounds close." : h.name + "'s light closes your wounds.", 0xFFE68C);
    }
  } else if (key == "hallow") {
    int dmg = rnd_i((2 + .6 * h.lvl) * sp * (h.perk("pyre") ? 2 : 1));
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
      if (m->def->undead) hurt(*m, rnd_i(6 + 1.5 * h.lvl), &h);
    }
    h.say("Your lantern flares white.", 0xFFF0B4);
  } else if (key == "rune") {
    for (auto& r : floor->runes) if (r.p == P{h.x, h.y}) { h.say("A rune is already carved here.", 0x909090); return false; }
    floor->runes.push_back({{h.x, h.y}, h.pid});
    int mine = 0;
    for (auto& r : floor->runes) if (r.owner == h.pid) ++mine;
    if (mine > (h.perk("stoneward") ? 5 : 3))
      for (size_t i = 0; i < floor->runes.size(); ++i) if (floor->runes[i].owner == h.pid) { floor->runes.erase(floor->runes.begin() + i); break; }
    add_fx({{"k", "rise"}, {"x", h.x}, {"y", h.y}, {"col", 0x50E6E6}});
    h.say("You carve a rune into the floor.", 0x50E6E6);
    tip(h, "rune", "A foe that steps on your rune sets it off. Lure them over it, or burst them all with Kindle: [2].");
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
        m->bleed_dmg = std::max(m->bleed_dmg, 2 + h.lvl / 2);
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
      if (!was_busy) bleed(h, *m, 4 + h.lvl);
      say_near(m->x, m->y, cap(the(*m)) + " chokes and cannot act.", 0xC8A0C8);
    }
  } else return false;
  return true;
}

void Game::quaff(Hero& h, const std::string& k) {
  Stats st = stats(h);
  if (k == "heal") {
    if (h.heal <= 0) { h.say("No healing potions left.", 0x909090); return; }
    --h.heal;
    int before = h.hp;
    h.hp = std::min(st.maxhp, h.hp + rnd_i(st.maxhp * .45 * (1 + .4 * h.perk("second"))));
    num(h.x, h.y, "+" + std::to_string(h.hp - before), 0x60E060);
    h.poison = 0;
    h.say("You drink a healing potion.", 0xFF7070);
    add_fx({{"k", "rise"}, {"x", h.x}, {"y", h.y}, {"col", 0xFF7070}});
  } else {
    if (h.mana_pots <= 0) { h.say("No mana potions left.", 0x909090); return; }
    --h.mana_pots;
    h.mana = std::min(st.maxmana, h.mana + rnd_i(st.maxmana * .6 * (1 + .4 * h.perk("second"))));
    h.say("You drink a mana potion.", 0x7090FF);
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
  if (int(to->pack.size()) >= PACK_MAX) { h.say(to->name + "'s pack is full.", 0x909090); return; }
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
  if (int(h.pack.size()) >= PACK_MAX) { h.say("Your pack is full.", 0x909090); return; }
  h.gold -= price;
  floor->shop.erase(floor->shop.begin() + i);
  h.pack.push_back(it);
  h.say("You buy " + it->name + ".", rarity_col(it->rarity));
}

void Game::buy_potion(Hero& h, const std::string& k) {
  if (!h.shopping) return;
  int price = 25 + 5 * lvl;
  if (h.gold < price) { h.say("You cannot pay " + std::to_string(price) + " gold.", 0x909090); return; }
  if (k == "heal" && h.heal < belt(h)) { ++h.heal; h.gold -= price; }
  else if (k == "mana" && h.mana_pots < BELT_MAX) { ++h.mana_pots; h.gold -= price; }
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
    m->energy += m->spd;
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
  int chance = std::clamp(m.acc + lvl * 2 - st.armour - (h.cls->key == "cutthroat" && !ranged ? 15 : 0), 20, 95);
  if (!ranged) add_fx({{"k", "swing"}, {"src", xy(m.x, m.y)}, {"cells", json::array({xy(h.x, h.y)})}});
  if (rnd.below(100) >= chance) { if (h.perk("riposte")) m.open = 2; num(h.x, h.y, "dodge", 0x808080); return; }
  int dmg = std::max(1, rnd.range(m.lo, m.hi) - st.armour / 6);
  if (m.can("fervour")) dmg = rnd_i(dmg * (1 + .6 * (1 - double(m.hp) / m.maxhp)));  // the more it bleeds, the harder
  if (m.def->fire || m.has("Fire-blooded")) dmg += rnd_i(rnd.range(m.lo, m.hi) * .3 * (1 - st.res_f / 100.0));
  hero_hurt(h, &m, dmg, "");
  if (!h.dead) trait_hit(m, h, dmg);
  if (m.def->poison > 0 || m.has("Venomous")) {
    int p = rnd_i(std::max(m.def->poison, 2 + lvl / 2) * (1 - st.res_p / 100.0));
    if (p > 0) { h.poison = 4; h.poison_dmg = std::max(h.poison_dmg, p); }
  }
  if ((m.def->cold || m.has("Frost-touched")) && rnd.below(100) >= st.res_c) h.numb = 3;
  if (m.has("Vampiric")) m.hp = std::min(m.maxhp, m.hp + dmg / 2);
  if (!ranged && st.thorns > 0 && m.hp > 0) hurt(m, st.thorns, &h);
}

void Game::hero_hurt(Hero& h, Mon* m, int dmg0, const std::string& kind) {
  if (h.dead) return;
  Stats st = stats(h);
  int dmg = dmg0;
  if (kind == "fire") dmg = rnd_i(dmg * (1 - st.res_f / 100.0));
  if (kind == "cold") dmg = rnd_i(dmg * (1 - st.res_c / 100.0));
  if (h.brand > 0) dmg = rnd_i(dmg * 1.25);
  if (h.shield > 0) dmg = std::max(1, dmg / 2);
  if (h.standfast > 0) dmg = std::max(1, dmg / 2);
  if (landing_ && h.tal("mantle")) dmg = std::max(1, dmg * (100 - 10 * h.tal("mantle")) / 100);
  if (h.cls->fury()) dmg = std::max(1, rnd_i(dmg * (1 - .02 * h.mana)));  // fury dulls the pain
  if (h.cls->fury() && m) h.mana = std::min(10, h.mana + (h.perk("guardian") ? 2 : 1));
  h.hp -= dmg;
  num(h.x, h.y, std::to_string(dmg), kind == "poison" ? 0x70D050 : 0xFF5050);
  add_fx({{"k", "blood"}, {"x", h.x}, {"y", h.y}, {"src", xy(m ? m->x : h.x, m ? m->y : h.y - 1)}});
  h.say((m ? cap(the(*m)) : std::string(kind == "fire" ? "Fire" : kind == "bleed" ? "Bleeding" : "Poison")) + " hurts you (" + std::to_string(dmg) + ").", 0xFF8080);
  if (h.hp <= stats(h).maxhp / 3 && h.heal > 0) tip(h, "quaff", "You are badly hurt. Drink a healing potion: [q].");
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
    h.say("You die.", 0xFF4040);
    say_all(h.name + " has fallen.", 0xFF6060);
    if (!living().empty() && !final_floor()) say_all(h.name + " will wake at the next waystation, if the others reach it.", 0xA0A0A0);
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
        o->poison_dmg = std::max(o->poison_dmg, rnd_i(m.def->poison * (1 - stats(*o).res_p / 100.0)));
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
    if (turn % 3 == 0 && !h->cls->fury()) h->mana = std::min(st.maxmana, h->mana + st.regen);
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
    if (m->has("Regenerating") && m->hp < m->maxhp) m->hp = std::min(m->maxhp, m->hp + std::max(1, m->maxhp / 50));
  classes_tick();
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
    int sight = (floor->deep ? SIGHT + 5 : SIGHT) + h.tal("lantern");  // the lava lights the Deep: you see further there
    bool dim = false;
    for (auto& m : floor->mons) if (m->awake && m->can("dim") && cheb(m->x, m->y, h.x, h.y) <= 5) dim = true;
    if (dim) sight = 3;
    else if (h.ashen > 0) sight = std::min(sight, 4);
    if (dim && !h.dimmed) { h.say("Your light gutters.", 0xC0A060); tip(h, "dim", "A lantern wraith eats the light. Kill it, and your light flares up again."); }
    h.dimmed = dim;
    TCOD_map_compute_fov(map, h.x, h.y, sight, true, FOV_SYMMETRIC_SHADOWCAST);
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x) {
        h.vis[y * W + x] = whole_view || (TCOD_map_is_in_fov(map, x, y) && cheb(x, y, h.x, h.y) <= sight);
        if (h.vis[y * W + x]) h.seen[y * W + x] = 1;
      }
    if (!h.present()) continue;
    for (auto& m : floor->mons)
      if (!m->awake && !m->dormant && !m->hidden && !m->sleep && !whole_view && h.sees(m->x, m->y)) {
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
  auto it = loot::roll(d, rnd, kinds, rarity, at_least);
  int eye = 0;
  for (auto* h : living()) eye = std::max(eye, h->tal("goodeye"));
  if (rarity.empty() && eye && rnd.below(100) < 3 * eye) {
    std::string up = it->rarity == "normal" ? "magic" : it->rarity == "magic" ? "rare" : it->rarity == "rare" ? "unique" : "";
    if (!up.empty()) it = loot::roll(d, rnd, kinds, up);
  }
  return it;
}

// Spark-Forged: once a dive, at a waystation, the weapon's magic is rolled again (the same kind of weapon, as good)
void Game::forge(Hero& h) {
  if (!h.tal("forged")) return;
  if (!floor->rest) { h.say("Only at a waystation's fire.", 0x909090); return; }
  if (h.forged) { h.say("The Spark has already reforged once this dive.", 0x909090); return; }
  auto wi = h.equip.find("weapon");
  if (wi == h.equip.end() || wi->second->rarity == "normal" || wi->second->rarity == "unique") {
    h.say("Only a magic or rare weapon can be reforged.", 0x909090);
    return;
  }
  ItemP old = wi->second, best;
  for (int i = 0; i < 40 && !best; ++i) {
    auto it = loot::roll(lvl + 1 + 3 * frost, rnd, {old->kind}, old->rarity);
    if (it->base == old->base) best = it;
  }
  if (!best) best = loot::roll(lvl + 1 + 3 * frost, rnd, {old->kind}, old->rarity);
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
