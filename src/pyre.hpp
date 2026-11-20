#pragma once
// The Pyre: a tree of small gifts that stay between dives, bought with Sparks at the fire in Charnfen.
// Three branches; a row opens after 3 points in the rows above it, in that branch. Respec is free between dives.
// Sparks: the first kill of each zone's boss on each Frost level gives 1; the Ashen King gives 3 and opens the next
// Frost level, where foes are harder and the loot better.
#include <map>
#include <string>

namespace pyre {

struct Talent {
  const char* key;
  int branch;  // 0 Flame, 1 Hearth, 2 Ember
  int row;     // 1..5
  int ranks;
  const char* name;
  const char* text;
};

inline const char* BRANCHES[3] = {"Flame", "Hearth", "Ember"};

inline const Talent TALENTS[] = {
    {"kindled", 0, 1, 3, "Kindled Edge", "+4% damage per rank."},
    {"embers", 0, 1, 2, "Ember Strikes", "Each rank: a 5% chance that a blow sets the foe burning."},
    {"coaleyed", 0, 2, 3, "Coal-Eyed", "+2% critical chance per rank."},
    {"heat", 0, 2, 1, "Heat of Battle", "The first kill on each floor gives back a fifth of your mana or fury."},
    {"pyrewake", 0, 3, 2, "Pyre Wake", "Foes you kill burst into flame, burning the foes beside them."},
    {"fevered", 0, 4, 3, "Fevered", "+5% damage per rank while you are under half life."},
    {"heartsfury", 0, 5, 1, "Heart's Fury", "Once a floor, at a quarter life: flames stun all within 3, then 5 turns of +50% damage."},
    {"hearthskin", 1, 1, 3, "Hearthstone Skin", "+5 life per rank."},
    {"warmhands", 1, 1, 2, "Warm Hands", "+10% cold resistance per rank."},
    {"pulse", 1, 2, 3, "Steady Pulse", "Your life slowly comes back; faster with each rank."},
    {"banked", 1, 2, 1, "Banked Coals", "Carry one more healing potion."},
    {"mantle", 1, 3, 2, "Mantle", "Blows you see coming do 10% less per rank."},
    {"shared", 1, 3, 1, "Shared Hearth", "Friends within 3 of you slowly heal. (Only with friends.)"},
    {"thaw", 1, 4, 1, "Thaw", "Chill, being held and dazed wear off twice as fast."},
    {"unquenched", 1, 5, 1, "Unquenched", "Once a dive, a killing blow leaves you at 1 life, and you burst into healing flame."},
    {"glint", 2, 1, 3, "Glint", "+5% gold per rank."},
    {"lantern", 2, 1, 1, "Lantern", "You see one cell further."},
    {"sense", 2, 2, 2, "Treasure Sense", "On each new floor you know where the chests are (rank 1) and the shrines (rank 2)."},
    {"credit", 2, 2, 1, "Charnfen Credit", "Start each dive with 2 more healing potions."},
    {"goodeye", 2, 3, 3, "Good Eye", "Each rank: a 3% chance that a drop comes one rarity better."},
    {"friend", 2, 4, 1, "Waystation Friend", "The waystation merchant has one more thing, and asks 10% less."},
    {"forged", 2, 5, 1, "Spark-Forged", "Once a dive, at a waystation fire: roll your weapon's magic again. [f] by the fire."},
};
inline constexpr int N = int(sizeof(TALENTS) / sizeof(TALENTS[0]));

inline const Talent* find(const std::string& k) {
  for (auto& t : TALENTS) if (k == t.key) return &t;
  return nullptr;
}

// points spent in a branch on the rows above `row`
inline int spent_above(const std::map<std::string, int>& have, int branch, int row) {
  int n = 0;
  for (auto& t : TALENTS) if (t.branch == branch && t.row < row) { auto it = have.find(t.key); if (it != have.end()) n += it->second; }
  return n;
}
inline bool open(const std::map<std::string, int>& have, const Talent& t) { return spent_above(have, t.branch, t.row) >= 3 * (t.row - 1); }
inline int spent(const std::map<std::string, int>& have) { int n = 0; for (auto& [k, v] : have) n += v; return n; }

// only what the tree allows: known talents, within their ranks, each row open
inline std::map<std::string, int> clean(const std::map<std::string, int>& in, int sparks = 1 << 20) {
  std::map<std::string, int> out;
  for (int row = 1; row <= 5; ++row)
    for (auto& t : TALENTS) {
      if (t.row != row) continue;
      auto it = in.find(t.key);
      if (it == in.end() || it->second <= 0 || !open(out, t)) continue;
      int r = std::min(it->second, t.ranks);
      r = std::min(r, sparks - spent(out));
      if (r > 0) out[t.key] = r;
    }
  return out;
}

inline const int FROST_MAX = 5;

}  // namespace pyre
