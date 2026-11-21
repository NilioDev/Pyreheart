// The five elements (plan.md section 1, Elements): every hit lays a stack of its element on the target, a heavy hit
// sets off all the stacks of its own element at once. The same rules hold for heroes and foes, in both directions.
#pragma once
#include <string>

namespace elem {
enum E { EMBER, RIME, ASH, ROT, BLOOD, N };
constexpr const char* NAME[N] = {"Ember", "Rime", "Ash", "Rot", "Blood"};
constexpr int COL[N] = {0xFF8A30, 0xA8D8FF, 0xA8A8B8, 0x9CC850, 0xD8403A};
constexpr const char* ROLE[N] = {"damage", "control", "defence", "attrition", "sustain"};
// how it plays, three short lines each, for the coal window
constexpr const char* PLAYS[N][3] = {
    {"burns a little", "every turn", "the fire leaps to all around"},
    {"slows the foe", "", "shatters in a burst of harm"},
    {"soot in its eyes:", "it misses more", "smothered, it loses its turn"},
    {"decay: a rotting", "foe cannot heal", "all the rot bursts at once"},
    {"it bleeds", "", "drink: its bleeding heals you"},
};
constexpr int MAX = 5;   // stacks of one element on one target
constexpr int FADE = 3;  // turns without a new hit before one stack falls off

// what each zone's foes attack with, resist and fear, in the order of data::ACTS
struct Zone { int atk[2], resist, weak; };
constexpr Zone ZONE[10] = {
    {{ROT, RIME}, ROT, EMBER},      // the Crypt
    {{ROT, BLOOD}, ASH, BLOOD},     // the Caves
    {{EMBER, ASH}, EMBER, RIME},    // the Forge
    {{EMBER, ASH}, EMBER, RIME},    // the Deep
    {{ROT, RIME}, RIME, ASH},       // the Cistern
    {{ASH, RIME}, ASH, BLOOD},      // the City
    {{EMBER, BLOOD}, ROT, ASH},     // the Vaults
    {{BLOOD, EMBER}, BLOOD, ROT},   // the Barracks
    {{BLOOD, ROT}, RIME, EMBER},    // the Prison
    {{ASH, RIME}, BLOOD, ROT},      // the Sanctum
};
constexpr int KING_ATK[2] = {RIME, ASH};

// What a skill does with stacks, as its sign on the panel: \u25A0 lays them (it hits), \u263C sets them all off, \u00B7 neither.
inline int skill_sign(const std::string& k) {
  for (const char* d : {"charge", "pierce", "firebolt", "smite", "kindle", "garrote", "hurl"}) if (k == d) return 0x263C;
  for (const char* b : {"cleave", "volley", "pin", "nova", "emberwall", "beacon", "shadowstep", "fan", "scatter", "whirlwind"}) if (k == b) return 0x25A0;
  return 0xB7;
}
}  // namespace elem

// The stacks one target carries: how many of each element, the turns since each was last laid, and who laid it.
struct Stacks {
  int n[elem::N] = {0, 0, 0, 0, 0};
  int idle[elem::N] = {0, 0, 0, 0, 0};
  int by[elem::N] = {0, 0, 0, 0, 0};  // the player who laid it last (0 a foe), for the kill
  int total() const { int t = 0; for (int v : n) t += v; return t; }
  void clear() { for (int i = 0; i < elem::N; ++i) n[i] = idle[i] = 0; }
};
