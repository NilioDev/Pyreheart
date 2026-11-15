// The shapes of the design tables (filled in data.hpp) and a few small helpers used everywhere.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <numeric>
#include <optional>
#include <cstdint>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

struct TileDef { int ch; int col; bool walk; bool see; std::string name; };
struct ActDef { std::string name; int wall, floor; std::string mood, boss; };
struct MonDef {
  std::string key, name; int ch; int col; int hp, lo, hi, acc, ar, spd, xp; std::string ai; int rng; int shot_ch; int shot_col;
  bool boss, gear; int poison; bool fire, cold; std::vector<std::string> moves;
};
struct ClassDef {
  std::string key, name; int col; int hp, mana, hp_lvl, mana_lvl, acc;
  std::vector<std::string> start, skills, kinds; std::string about;
};
struct SkillDef { std::string key, name; int mana; bool target; std::string about; };
struct PerkDef { std::string key, name, text; int max; std::string cls; };
struct BaseDef { std::string key, name, slot, kind; int tier, lo, hi, ar, spell; };
struct AffixDef { std::string name, stat; int lo, hi; double grow; std::vector<std::string> slots; };
struct UniqueDef { std::string name, base; std::vector<std::pair<std::string, int>> stats; };

struct P {
  int x = 0, y = 0;
  bool operator==(const P& o) const { return x == o.x && y == o.y; }
  bool operator!=(const P& o) const { return !(*this == o); }
  P operator+(const P& o) const { return {x + o.x, y + o.y}; }
};
struct PHash { size_t operator()(const P& p) const { return std::hash<int64_t>()((int64_t(p.x) << 32) ^ uint32_t(p.y)); } };
using PSet = std::unordered_set<P, PHash>;
template <class V> using PMap = std::unordered_map<P, V, PHash>;

/** One random stream per game: every roll goes through it, so a seed gives the same dive. */
struct Rng {
  std::mt19937_64 g;
  explicit Rng(uint64_t seed = 1) : g(seed) {}
  // the same numbers from the same seed on every system: the standard distributions differ between compilers
  int range(int lo, int hi) {  // lo..hi
    if (hi <= lo) return lo;
    uint64_t span = uint64_t(int64_t(hi) - lo) + 1, limit = UINT64_MAX - UINT64_MAX % span, v;
    do v = g(); while (v >= limit);
    return int(lo + int64_t(v % span));
  }
  int below(int n) { return n <= 1 ? 0 : range(0, n - 1); }
  double real() { return double(g() >> 11) * 0x1.0p-53; }
  bool coin() { return real() < .5; }
  template <class T> const T& pick(const std::vector<T>& v) { return v[below(int(v.size()))]; }
  template <class T> void shuffle(std::vector<T>& v) {
    for (int i = int(v.size()) - 1; i > 0; --i) std::swap(v[i], v[range(0, i)]);
  }
};

inline int cheb(int ax, int ay, int bx, int by) { return std::max(std::abs(ax - bx), std::abs(ay - by)); }
