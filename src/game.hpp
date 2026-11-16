// The whole dive, kept by the host. Players send actions; when every hero has one, the world takes a step.
#pragma once
#include <memory>
#include <nlohmann/json.hpp>

#include "data.hpp"
#include "defs.hpp"

using json = nlohmann::json;

constexpr int W = 76;
constexpr int H = 42;
constexpr int SIGHT = 9;

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
  const ClassDef* cls = nullptr;
  int x = 0, y = 0, hp = 1, mana = 0, lvl = 1, xp = 0, gold = 0;
  std::map<std::string, ItemP> equip;
  std::vector<ItemP> pack;
  int heal = 3, mana_pots = 2;
  std::map<std::string, int> perks;
  std::vector<std::vector<std::string>> perk_choices;
  bool dead = false, gone = false, away = false, drank = false, shopping = false;
  int shield = 0, poison = 0, poison_dmg = 0, numb = 0, kills = 0;
  std::vector<char> vis = std::vector<char>(W * H, 0);
  std::vector<LogLine> log;
  std::unordered_set<std::string> tips;

  int perk(const std::string& k) const { auto it = perks.find(k); return it == perks.end() ? 0 : it->second; }
  bool present() const { return !dead && !gone && !away; }
  void say(const std::string& t, int col = 0xC8C8C8) { log.push_back({t, col}); }
  bool sees(int x_, int y_) const { return x_ >= 0 && x_ < W && y_ >= 0 && y_ < H && vis[y_ * W + x_]; }
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
  bool has(const std::string& m) const { return std::find(mods.begin(), mods.end(), m) != mods.end(); }
};
using MonP = std::shared_ptr<Mon>;

struct FloorItem { int x, y; ItemP item; int gold = 0; std::string pot; int owner = 0; };  // owner 0: anyone

struct Floor {
  int depth = 1, act = 0, id = 0, version = 0;
  bool rest = false;
  std::vector<int> t = std::vector<int>(W * H, data::WALL);
  std::map<int, std::string> writings;
  std::vector<FloorItem> items;
  std::vector<MonP> mons;
  P up, down;
  bool has_merchant = false;
  P merchant;
  std::vector<ItemP> shop;
  MonP boss;
  bool boss_woke = false;
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
  bool bow;
};

const ClassDef& class_def(const std::string& key);
const MonDef& mon_def(const std::string& key);
const PerkDef& perk_def(const std::string& key);
const SkillDef& skill_def(const std::string& key);

class Game {
 public:
  explicit Game(uint64_t seed);
  Rng rnd;
  std::map<int, std::unique_ptr<Hero>> heroes;  // by player id, in join order
  std::unique_ptr<Floor> floor;
  int turn = 0, depth = 1;
  std::string state = "play";
  std::vector<json> fx;

  Hero& join(int pid, const std::string& name, const std::string& cls);
  void leave(int pid);
  bool free_action(Hero& h, const json& a);
  void step(const std::map<int, json>& actions);
  std::vector<Hero*> active();
  std::vector<Hero*> living();
  Stats stats(const Hero& h) const;
  int need(int lvl) const;
  bool can_use(const Hero& h, const Item& it) const;
  Verdict compare(const Hero& h, const Item& it) const;
  Hero* hero_at(int x, int y);
  MonP mon_at(int x, int y);
  void look();

 private:
  std::unordered_set<std::string> used_writings_;
  int mon_id_ = 1, floor_ids_ = 1;
  double scaled_ = 1.0;
  std::vector<int> dist_;

  std::unique_ptr<Floor> make_floor(int depth, bool rest);
  void build_waystation(Floor& f);
  MonP spawn(const std::string& key, int x, int y);
  MonP spawn_near(const std::string& key, P c);
  void apply_mods(Mon& m);
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

  void act(Hero& h, const json& a);
  void move(Hero& h, int dx, int dy);
  void arrive(Hero& h);
  void descend(Hero& h);
  void check_stairs();
  bool strike(Hero& h, Mon& m, double mult);
  void melee(Hero& h, Mon& m);
  void hurt(Mon& m, int dmg, Hero* by, const std::string& text = "");
  void kill(Mon& m, Hero* by);
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
  bool boss_move(Mon& m, Hero& h, int d);
  void tick();
  std::vector<std::string> party_kinds();
};

namespace view {
json snapshot(Game& g, Hero& h, bool with_map);
std::string glyph(const Item& it);
}
