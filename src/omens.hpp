#pragma once
// The Frost omens (plan.md section 7, step 3): from Frost 1 on, each dive draws its own omens, one on Frost 1 to 3,
// two on 4 to 6, three on 7 to 9 and four from Frost 10. Each weighs light (1), medium (2) or heavy (3), and the draw
// keeps the total near two per omen, so a dive never gets three heavy ones at once. The hero hears them in Charnfen,
// before the stairs, and the Hero screen and the grave list them. Ten only make it harder; ten cut both ways.
#include <cstdint>
#include <vector>

namespace omen {
enum {
  // harder
  PACKS = 1, MUSTER, CLINGS, RISE, ANGRY, TWIN, LISTEN, THIN, FRENZY, KINGSCOLD,
  // two-sided
  BLOODMOON, LONGNIGHT, GOLDFEVER, RESTLESS, LONEWOLVES, EMBERTIDE, HUNGRYDARK, CRACKED, BROKEN, REMEMBERS,
  N = REMEMBERS
};
struct Def { const char* name; const char* text; int weight; };
inline constexpr Def DEF[N + 1] = {
    {"", "", 0},
    {"The packs grow", "every ordinary pack has one foe more", 2},
    {"Champions muster", "champion packs come four strong, and more often", 2},
    {"The cold clings", "stacks on you need five quiet turns to fade, not three", 1},
    {"The dead will not lie", "one in six fallen foes rises once more, at half its life", 2},
    {"Great foes wake angry", "the zone bosses and the King have 30% more life and hit 10% harder", 2},
    {"Twin-blooded elites", "champions and uniques lay two stacks with each blow", 2},
    {"The dark listens", "sleeping foes wake from two cells further", 1},
    {"Thin draughts", "healing potions heal a third less", 2},
    {"Frenzy", "a foe under a third of its life strikes a quarter harder", 1},
    {"The King's own cold", "champions and uniques carry one affix more", 3},
    {"Blood Moon", "everything drops loot more often, but foes hit 20% harder", 2},
    {"Long Night", "a foe slain before it woke gives double xp, but you see two cells less far", 1},
    {"Gold Fever", "foes drop twice the gold, but the trader asks half as much again", 1},
    {"Restless Shrines", "shrines are twice as strong, but a shrine wakes everything near it", 1},
    {"Lone Wolves", "packs are smaller, but every foe is much stronger", 2},
    {"Ember Tide", "you lay one stack more with each blow, and so do your foes", 2},
    {"Hungry Dark", "champions always drop a rare, but the torches and lamps are out", 2},
    {"Cracked Seals", "the first map of each zone hides a sealed hoard, but champions guard it", 2},
    {"Broken Fountains", "you go down with two more potions of each kind, but fountains do not heal", 1},
    {"The Pyre Remembers", "each zone boss always drops a unique, but rises once more when it falls", 3},
};
inline bool two_sided(int k) { return k >= BLOODMOON; }
inline int count(int frost) { return frost <= 0 ? 0 : frost <= 3 ? 1 : frost <= 6 ? 2 : frost <= 9 ? 3 : 4; }

// the dive's omens, drawn from its seed and Frost level: the same seed and level always bring the same ones
inline std::vector<int> draw(uint64_t seed, int frost) {
  int n = count(frost);
  std::vector<int> out;
  if (!n) return out;
  uint64_t s = seed * 6364136223846793005ULL + 1442695040888963407ULL * uint64_t(frost + 1) + 0x0E0E;
  auto next = [&] { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; };
  for (int tries = 0; tries < 400; ++tries) {
    out.clear();
    while (int(out.size()) < n) {
      int k = 1 + int(next() % N);
      bool have = false;
      for (int o : out) have = have || o == k;
      if (!have) out.push_back(k);
    }
    int w = 0;
    for (int o : out) w += DEF[o].weight;
    if (w >= 2 * n - 1 && w <= 2 * n + 1) break;
  }
  return out;
}
}  // namespace omen
