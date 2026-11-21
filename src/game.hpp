// The whole dive, kept by the host. Players send actions; when every hero has one, the world takes a step.
#pragma once
#include <memory>
#include <nlohmann/json.hpp>

#include "data.hpp"
#include "defs.hpp"
#include "elements.hpp"

using json = nlohmann::json;

constexpr int CISTERN = 4;  // zone indexes the game reads by number (see ACTS in ref/data.py)
constexpr int DEEP = 3;
constexpr int CITY = 5, VAULTS = 6, BARRACKS = 7, PRISON = 8, SANCTUM = 9;
constexpr int W = 120;  // the largest map; a zone's floors use the top left w x h of it (ACTS[].w, .h)
constexpr int H = 66;
constexpr int SIGHT = 9;
// A map goes over the wire as printable text, one character a cell: TILE0 + its tile, and for the tiles past what
// that reaches, '~' then TILE0 + (tile - TILE_ESC), and past those '~~' then TILE0 + (tile - 2 * TILE_ESC) (tiles up to
// 223, since a decoded cell is one byte). Decoded, it is one byte a cell, TILE0 + tile; tile_of() reads one.
constexpr char TILE0 = ' ';
constexpr int TILE_ESC = 94;  // ' ' + 94 = '~', the escape
inline int tile_of(char c) { return int((unsigned char)c) - TILE0; }
static_assert(data::TILE_COUNT <= 224, "a decoded map cell is one byte: TILE0 + tile must fit");
inline std::string encode_map(const std::vector<int>& t) {
  std::string m;
  m.reserve(t.size());
  for (int v : t) {
    if (v < TILE_ESC) m += char(TILE0 + v);
    else if (v < 2 * TILE_ESC) { m += '~'; m += char(TILE0 + v - TILE_ESC); }
    else { m += "~~"; m += char(TILE0 + v - 2 * TILE_ESC); }  // and past those, a second '~'
  }
  return m;
}
inline std::string decode_map(const std::string& s) {
  std::string m;
  m.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '~' && i + 2 < s.size() && s[i + 1] == '~') { m += char((unsigned char)(TILE0 + 2 * TILE_ESC + (s[i + 2] - TILE0))); i += 2; }
    else if (s[i] == '~' && i + 1 < s.size()) { m += char((unsigned char)(TILE0 + TILE_ESC + (s[i + 1] - TILE0))); ++i; }
    else m += s[i];
  }
  return m;
}

struct Item {
  int id = 0;
  std::string base, name, slot, kind, rarity = "normal", base_name;
  int tier = 1, lo = 0, hi = 0, ar = 0, value = 10;
  std::vector<std::pair<std::string, int>> stats;
  int stat(const std::string& k) const;
  void add(const std::string& k, int v);
  std::vector<std::string> lines() const;
  std::string shortname() const;
  json to_json() const;
};
using ItemP = std::shared_ptr<Item>;

namespace loot {
ItemP make(const std::string& base, const std::string& rarity = "normal", int tier = 0);
ItemP roll(int depth, Rng& rnd, const std::vector<std::string>& kinds, const std::string& rarity = "", const std::string& at_least = "normal");
}

struct LogLine { std::string text; int col; };

struct Hero {
  int pid = 0;
  std::string name;
  const ClassDef* cls = &kit;  // what its weapon and its ranks make of the hero (Game::refit fills kit)
  ClassDef kit;
  int x = 0, y = 0, hp = 1, mana = 0, lvl = 1, xp = 0, gold = 0;
  // The classless hero (plan.md section 1): fury for the melee skills beside mana for the rest; the fighting styles
  // (melee, ranged, magic) and the elements grow by use, each with its rank and the xp toward the next; mastery with
  // each kind of weapon; the style skills learned, in the order they came; a crossbow to wind
  int fury = 0;
  int srank[3] = {0, 0, 0}, sxp[3] = {0, 0, 0};
  int erank[5] = {0, 0, 0, 0, 0}, exq[5] = {0, 0, 0, 0, 0};
  std::map<std::string, int> wxp;
  std::vector<std::string> known;
  int reload = 0;
  std::map<std::string, ItemP> equip;
  std::vector<ItemP> pack;
  int heal = 3, mana_pots = 2;
  std::map<std::string, int> perks;
  std::vector<std::vector<std::string>> perk_choices;
  bool dead = false, gone = false, away = false, drank = false, shopping = false;
  bool god = false;  // the debug menu's: takes no harm
  int shield = 0, poison = 0, poison_dmg = 0, numb = 0, kills = 0;
  int focus = 0, standfast = 0, combo_id = 0, combo = 0;  // ranger's focus, warrior's Stand Fast, duelist's run of blows
  // what the foes' tricks leave on a hero, in turns: held fast, dazed (no turn at all), armour split, branded (hurts more),
  // bleeding, coughing in ash (sees less); latched: the id of the leech on it; moved: the turn it last took a step
  int shown = 0;  // the statuses already floated up over the hero
  int held = 0, dazed = 0, sunder = 0, brand = 0, bleed = 0, bleed_dmg = 0, ashen = 0, latched = 0, moved = 0;
  bool dimmed = false;
  // the elements: the coals taken (the first from Charnfen's great fire, -1 none yet), the stacks carried, the turns
  // the skills stay smothered by Ash, and the foes' discharges already warned of
  int el = -1, el2 = -1, smother = 0;
  Stacks stk;
  std::vector<char> vis = std::vector<char>(W * H, 0);
  std::vector<char> seen = std::vector<char>(W * H, 0);  // all it has seen of this floor, for a dive taken up again
  std::vector<LogLine> log;
  std::unordered_set<std::string> tips;

  std::map<std::string, int> talents;  // the Pyre's gifts this player brought (pyre.hpp)
  int heat_floor = -1, fury_floor = -1, fury_t = 0;  // Heat of Battle and Heart's Fury: once a floor
  bool unquenched = false, forged = false;           // Unquenched and Spark-Forged: once a dive
  int tal(const std::string& k) const { auto it = talents.find(k); return it == talents.end() ? 0 : it->second; }
  int perk(const std::string& k) const { auto it = perks.find(k); return it == perks.end() ? 0 : it->second; }
  bool present() const { return !dead && !gone && !away; }
  void say(const std::string& t, int col = 0xC8C8C8) { log.push_back({t, col}); }
  bool sees(int x_, int y_) const { return x_ >= 0 && x_ < W && y_ >= 0 && y_ < H && vis[y_ * W + x_]; }
};

// A blow a foe winds up for a few turns, drawn on the ground so a player can answer it.
// Red: step off it. Purple: a spell, broken by a stun or a hard hit. Gold: the only safe ground.
enum BlowKind { BLOW_RED, BLOW_PURPLE, BLOW_GOLD };
struct Blow {
  std::string name;
  int kind = BLOW_RED, turns = 0, dmg = 0, taken = 0;
  bool heavy = false;
  std::string elem;
  std::vector<P> cells;
};

struct Mon {
  int id = 0;
  const MonDef* def = nullptr;
  int x = 0, y = 0;
  std::string name;
  int col = 0, hp = 1, maxhp = 1, lo = 1, hi = 1, acc = 50, ar = 0, spd = 100, xp = 1;
  std::string rank = "normal";
  std::vector<std::string> mods;
  int energy = 0, stun = 0, frozen = 0, cool = 3;
  bool awake = false, summoned = false;
  Blow blow;                       // turns > 0 while it winds up
  int bleed = 0, bleed_dmg = 0;    // turns left, harm each turn
  int pinned = 0, lost = 0, surprised = 0, open = 0;  // held fast; lost in smoke; caught off guard; open after missing
  int spent = 0, phase = 0;        // spent: turns it stands open after a great blow
  // its trick: dormant (a statue until you come near), hidden (a heap of gold), braced (a quarter of the harm), listening
  // (a blind foe that has lost you), flee (turns it runs off), tcool (turns before the trick again), split (a swarm broken)
  int dormant = 0, hidden = 0, brace = 0, listen = 0, flee = 0, tcool = 0, sleep = 0;  // sleep: until a noise
  bool split = false, swum = false;
  Stacks stk;              // the elements on it
  std::map<int, std::array<int, 4>> work;  // harm done to it by each hero: melee, ranged, magic, and with the weapon
  int el = 0;              // the element its blows lay
  bool discharger = false; // a slow, heavy foe: its telegraphed blow sets off the stacks of its element
  bool can(const std::string& t) const { return def->has(t); }
  std::string trick(const std::string& t) const;  // the part after the colon of a trait like "lob:acid", or 
  std::vector<std::string> deck;   // a boss's next moves
  bool has(const std::string& m) const { return std::find(mods.begin(), mods.end(), m) != mods.end(); }
};
using MonP = std::shared_ptr<Mon>;

// a rune carved by a hero, or ground that does something: the heroes' (0 a wall of embers, 1 hallowed) and the foes'
// (2 a cloud of gas, 3 acid, 4 grease that catches fire, 5 molten slag, 6 tar, 7 burning grease, 8 smoke of incense)
enum GroundKind { G_EMBER, G_HALLOW, G_GAS, G_ACID, G_GREASE, G_SLAG, G_TAR, G_FIRE, G_SMOKE };
struct Rune { P p; int owner; };
struct Ground { P p; int kind, turns, dmg, owner; };

// the dead in a cracked tomb or on an embalming table: stage 0 asleep, 1-3 stirring, -1 risen or smashed
struct Sleeper { P at; std::vector<P> cells; std::string kind; int stage = 0; };

struct FloorItem { int x, y; ItemP item; int gold = 0; std::string pot; int owner = 0; };  // owner 0: anyone

struct Floor {
  int depth = 1, act = 0, id = 0, version = 0;
  int w = 76, h = 42;  // the part of the map in use
  bool rest = false, deep = false;
  std::vector<int> t = std::vector<int>(W * H, data::WALL);
  std::map<int, std::string> writings;
  std::vector<FloorItem> items;
  std::vector<MonP> mons;
  P up, down;
  int keys = 0;  // vault keys the party carries (the Vaults' guardians drop them)
  std::vector<int> flooded;  // the Cistern's reservoir: the low floor the Sluice-Keeper flooded, top rows first
  int flood_t = 0;           // turns the sludge stands before it drains
  std::vector<P> open_graves;  // Charnfen: where the dead climb out; the Prison: the dead prisoners that will rise
  int risen = 0;
  bool warned = false;
  bool has_merchant = false;
  P merchant;
  std::vector<ItemP> shop;
  MonP boss;
  bool boss_woke = false;
  std::vector<Rune> runes;
  std::vector<Ground> ground;
  std::vector<Sleeper> sleepers;
  std::vector<std::pair<P, int>> coals;  // Charnfen: the coals at the edge of the great fire, and their elements
  P rack{-1, -1};                        // Charnfen: the gatekeeper's rack of first weapons
  std::map<int, int> exits;  // in a waystation: the stairs down (cell index) and the zone each leads to
  bool burning(int x, int y) const { for (auto& g : ground) if ((g.kind == G_EMBER || g.kind == G_FIRE) && g.p == P{x, y}) return true; return false; }
  const Ground* ground_at(int x, int y) const { for (auto& g : ground) if (g.p == P{x, y}) return &g; return nullptr; }
  int tile(int x, int y) const { return x >= 0 && x < W && y >= 0 && y < H ? t[y * W + x] : data::WALL; }
  void set(int x, int y, int v) { t[y * W + x] = v; ++version; }
  bool walk(int x, int y) const { return !data::blocks(tile(x, y)); }
  bool opaque(int x, int y) const { return !data::TILES[tile(x, y)].see; }
};

// what wearing an item would change, for a player who is not sure which of two things is better
struct Change { std::string what; double by; };
struct Verdict { int v = 0; std::vector<Change> d; };  // v: 2 better, 1 mostly better, 0 the same, -1 mostly worse, -2 worse, -9 not yours

struct Stats {
  int maxhp, maxmana, lo, hi, pct, acc, crit, armour, res_f, res_c, res_p, leech, spell, fire, regen, thorns;
  bool bow;    // [f] does something with this weapon
  int reach;   // how far [f] reaches
};

namespace k5 { struct Plan; }
// stairs down, or in the Deep the way on at the edge of the map
inline bool way_down(int t) { return t == data::STAIRS || t == data::EDGE; }
// can every open cell still be walked to from the stairs up?
bool whole(const Floor& f);

const WeaponDef* weapon_def(const std::string& kind);  // nullptr for no weapon
int style_index(const std::string& key);
const MonDef& mon_def(const std::string& key);
const PerkDef& perk_def(const std::string& key);
const SkillDef& skill_def(const std::string& key);

class Game {
 public:
  explicit Game(uint64_t seed, int frost = 0);
  uint64_t seed = 0;  // the dive's seed: each map is made from it, its zone and which of the zone's maps it is
  int frost = 0;                  // the Frost level: 0 the game as it is, each one up harder foes and better loot
  std::vector<std::string> slain;  // the zones whose boss died in this dive (for the Sparks)
  bool landing_ = false;           // a blow seen coming is landing (for Mantle)
  int belt(const Hero& h) const { return data::BELT_MAX + h.tal("banked"); }
  int price(const Hero& h, const Item& it) const { return h.tal("friend") ? it.value * 2 * 9 / 10 : it.value * 2; }
  ItemP roll_loot(int d, const std::vector<std::string>& kinds, const std::string& rarity = "", const std::string& at_least = "normal");
  void forge(Hero& h);
  Rng rnd;
  std::map<int, std::unique_ptr<Hero>> heroes;  // by player id, in join order
  std::unique_ptr<Floor> floor;
  int turn = 0, depth = 1;  // depth: the maps gone down so far, this one too
  std::vector<int> route;    // the zones of this dive so far; the last is the one you are in
  int zfloor = 0;            // which of the zone's maps this is, from 0; the boss waits on the last
  int lvl = 1;               // how hard this map is, 1 at the top to 16 at the King: the depth on the old scale
  bool final_floor() const;  // the King's
  std::string state = "play";
  std::vector<json> fx;

  Hero& join(int pid, const std::string& name, const std::string& cls, const json& talents = json());
  void leave(int pid);
  bool free_action(Hero& h, const json& a);
  void step(const std::map<int, json>& actions);
  std::vector<Hero*> active();
  std::vector<Hero*> living();
  Stats stats(const Hero& h) const;
  int need(int lvl) const;
  bool can_use(const Hero& h, const Item& it) const;
  Verdict compare(const Hero& h, const Item& it) const;
  bool busy(const Mon& m, const Hero& h);
  Hero* hero_at(int x, int y);
  MonP mon_at(int x, int y);
  void look();

 private:
  std::unordered_set<std::string> used_writings_;
  std::vector<std::vector<P>> side_rooms_;  // the Cistern's halls of the waterworks, for its themes
  int mon_id_ = 1, floor_ids_ = 1;
  double scaled_ = 1.0;
  std::vector<int> dist_;

  std::unique_ptr<Floor> make_floor(int depth, bool rest);
  std::unique_ptr<Floor> make_floor_from(int d, bool rest);
  std::unique_ptr<Floor> make_town(const std::vector<std::string>& rows);
  void debug(Hero& h, const std::string& what);
  void build_waystation(Floor& f);
  void build_rooms(Floor& fl, int d, bool boss_floor);
  void furnish(Floor& fl, const std::vector<std::vector<P>>& rooms, std::vector<std::vector<P>> treasure);
  void zone_foes(Floor& fl);  // zones.cpp
  bool build_zone(Floor& fl, bool boss_floor, std::vector<std::vector<P>>& rooms, std::vector<std::vector<P>>& treasure);  // zones.cpp
  bool build_deep(Floor& fl, char entry, bool last, char& exit_side);  // deep.cpp
  char deep_exit_ = 'N';
  int chosen_ = -1;      // the zone the party took the stairs to, from a waystation
  void set_level();
  void dress(Floor& fl, const k5::Plan& plan);  // dress.cpp
  void dress_deep(Floor& fl, std::vector<std::vector<P>> plates);
  void dress_cistern(Floor& fl, const k5::Plan& plan);  // cistern.cpp
  void pull_lever(Hero& h, int x, int y);
  void under_grate(Hero& h);
  void wake_dead();
  bool smash_dead(Hero& h, int x, int y);
  MonP spawn(const std::string& key, int x, int y);
  MonP spawn_near(const std::string& key, P c);
  void apply_mods(Mon& m);
  // traits.cpp: the tricks of the foes
  bool trait_turn(Mon& m, Hero& h, int d, bool sees);
  void trait_hit(Mon& m, Hero& h, int dmg);
  void trait_death(Mon& m);
  int trait_hurt(Mon& m, int dmg);
  void trait_moved(Mon& m, P from);
  void traits_tick();
  bool trait_step(Mon& m, Hero& h);
  bool trait_cells(Mon& m, Hero& h, const std::string& name, Blow& b);
  void trait_land(Mon& m, const Blow& b, const std::vector<Hero*>& hit);
  void lay(P c, int kind, int turns, int dmg);
  void ignite(P c);
  void wake(Mon& m, const std::string& how);
  void champion(Mon& m, const std::vector<std::string>& mods);
  void unique(Mon& m);
  void next_floor();
  void rescale();
  void place(Hero& h);
  bool free_near(int x, int y, int r, P& out, const std::function<bool(int, int)>& fn);
  bool free_near(int x, int y, int r, P& out);
  void say_all(const std::string& t, int col = 0xC8C8C8);
  void say_near(int x, int y, const std::string& t, int col);
  void tip(Hero& h, const std::string& key, const std::string& text);
  void add_fx(json j) { fx.push_back(std::move(j)); }
  // a number or word that floats up from a cell and fades; a second one on the same cell waits its turn
  void num(int x, int y, const std::string& t, int col) {
    int n = 0;
    for (auto& f : fx) if (f.value("k", "") == "num" && f.value("x", -1) == x && f.value("y", -1) == y) ++n;
    add_fx({{"k", "num"}, {"x", x}, {"y", y}, {"t", t}, {"col", col}, {"delay", .14 * n}});
  }
  bool crit_next = false;  // the next harm to a foe was a critical blow: its number is gold
  // elements.cpp: stacks laid, set off and fading
  Hero* acting_ = nullptr;  // the hero whose action this is: its hits lay its element
  int discharging_ = -1;    // and this element is set off instead, by a heavy skill
  int scale_el() const { return 1 + lvl / 4; }
 public:
  int mon_res(const Mon& m, int e) const;
  int hero_res(const Hero& h, int e) const;
  int heal_cut(const Stacks& s, bool capped) const;
  int rime_slow(const Mon& m) const;
 private:
  void assign_element(Mon& m);
  void lay_on_mon(Hero& h, Mon& m, int n);
  void lay_on_hero(Mon& m, Hero& h, int n);
  void discharge_mon(Hero& h, Mon& m, int e);
  void discharge_hero(Mon& m, Hero& h, int e);
  void stacks_tick();
  void take_coal(Hero& h, int e);
 public:
  // hero.cpp: the classless hero (plan.md section 1)
  int power(const Hero& h) const;
  double catch_up(const Hero& h) const;  // how much faster a hero behind the pace learns (up to 2x), or slower ahead         // how far the hero has grown: what a level was to the old classes
  int main_style(const Hero& h) const;    // the style with the highest rank (the weapon's on a tie)
  int mastery(const Hero& h, const std::string& kind) const;
  static int rank_need(int r);            // style or element xp to reach rank r + 1
  static int mastery_need(int r);
  static bool fury_skill(const std::string& key);
  int skill_style(const Hero& h, const std::string& key) const;
  std::string title(const Hero& h) const;  // the combo title, or what the hero carries
  void refit(Hero& h);
 private:
  int credit_ = -1;  // the style (0..2) the harm being dealt now is learned in; -1: the hero's main style
  bool credit_weapon_ = false;  // and whether it was dealt with the weapon in hand
  void learn(Mon& m);           // a kill teaches each hero who hurt it, by what did the work
  void take_weapon(Hero& h, const std::string& kind);
  void offer_perk(Hero& h);
  void shoot_with(Hero& h, int x, int y);

  void act(Hero& h, const json& a);
  void move(Hero& h, int dx, int dy);
  void arrive(Hero& h);
  void descend(Hero& h);
  void check_stairs();
  bool strike(Hero& h, Mon& m, double mult, bool ranged = false);
  void focus_up(Hero& h);
  void bleed(Hero& h, Mon& m, int dmg);
  int rune_dmg(const Hero& h);
  void burst_rune(Hero& h, P at);
  void trod(Mon& m);
  void classes_tick();
  bool skill_new(Hero& h, const std::string& key, int x, int y, const Stats& st, double sp);
  void melee(Hero& h, Mon& m);
  void hurt(Mon& m, int dmg, Hero* by, const std::string& text = "");
  void kill(Mon& m, Hero* by);
  void town_tick();
  void prison_tick();
  void gain_xp(Hero& h, int xp);
  void choose_perk(Hero& h, const std::string& k);
  void drops(const Mon& m);
  void open_chest(Hero& h, int x, int y);
  void shrine(Hero& h, int x, int y);
  MonP target(const Hero& h, int x, int y);
  void path(const Hero& h, int tx, int ty, int range, bool through, std::vector<P>& cells, std::vector<MonP>& hit);
  void shoot(Hero& h, int x, int y);
  void skill(Hero& h, int n, int x, int y);
  void quaff(Hero& h, const std::string& k);
  void equip(Hero& h, int id);
  void drop(Hero& h, int id);
  void give(Hero& h, int id);
  void buy(Hero& h, int i);
  void buy_potion(Hero& h, const std::string& k);
  void sell(Hero& h, int id);

  void compute_dist();
  void monsters();
  Hero* nearest_hero(const Mon& m, bool seen_only);
  void mon_turn(Mon& m);
  void step_random(Mon& m);
  void step_toward(Mon& m);
  void mon_hit(Mon& m, Hero& h, bool ranged);
  void hero_hurt(Hero& h, Mon* m, int dmg, const std::string& kind);
  bool boss_move(Mon& m, Hero& h, int d, const std::string& mv);
  bool wind_up(Mon& m, Hero& h, const std::string& blow);
  void land(Mon& m);
  void boss_phase(Mon& m);
  void tick();
  std::vector<std::string> party_kinds();
};

namespace view {
json snapshot(Game& g, Hero& h, bool with_map);
std::string glyph(const Item& it);
}
