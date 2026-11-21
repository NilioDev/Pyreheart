// The classless hero (plan.md section 1): no class to choose. A weapon from the gatekeeper's rack, and whatever the
// hero does grows: the fighting style that dealt the harm (melee, ranged, magic), mastery with the weapon in hand, and
// the element the blows lay. The style ranks bring the skills; the hero's level and its best style make its power.
#include "game.hpp"

using namespace data;

const WeaponDef* weapon_def(const std::string& kind) {
  for (auto& w : WEAPONS) if (w.key == kind) return &w;
  return nullptr;
}
int style_index(const std::string& key) {
  for (size_t i = 0; i < STYLES.size(); ++i) if (STYLES[i].key == key) return int(i);
  return 0;
}

static const Item* weapon_in_hand(const Hero& h) {
  auto it = h.equip.find("weapon");
  return it == h.equip.end() ? nullptr : it->second.get();
}

int Game::main_style(const Hero& h) const {
  const Item* w = weapon_in_hand(h);
  const WeaponDef* wd = w ? weapon_def(w->kind) : nullptr;
  int best = wd ? style_index(wd->style) : 0;
  for (int s = 0; s < 3; ++s) if (h.srank[s] > h.srank[best]) best = s;
  return best;
}

// Two steps of power per hero level and one per rank of the best style: a pure hero at the end of Normal (level ~12,
// style rank ~10) stands where an old class stood at level ~33, so the foes keep their numbers.
int Game::power(const Hero& h) const {
  int top = std::max({h.srank[0], h.srank[1], h.srank[2]});
  return 2 * (h.lvl - 1) + 1 + top;
}

int Game::rank_need(int r) { return int(std::lround(2.3 * std::pow(r + 1.0, 3.8))); }
int Game::mastery_need(int r) { static const int N[] = {60, 220, 600, 1400, 3000}; return N[std::clamp(r, 0, MASTERY_MAX - 1)]; }

int Game::mastery(const Hero& h, const std::string& kind) const {
  auto it = h.wxp.find(kind);
  int xp = it == h.wxp.end() ? 0 : it->second, r = 0;
  while (r < MASTERY_MAX && xp >= mastery_need(r)) ++r;
  return r;
}

bool Game::fury_skill(const std::string& key) {
  for (auto& k : STYLES[0].skills) if (k == key) return true;
  for (auto& w : WEAPONS) if (w.sig == key) return w.style == "melee";
  return false;
}

int Game::skill_style(const Hero& h, const std::string& key) const {
  for (int s = 0; s < 3; ++s) for (auto& k : STYLES[s].skills) if (k == key) return s;
  for (auto& w : WEAPONS) if (w.sig == key) return style_index(w.style);
  return main_style(h);
}

std::string Game::title(const Hero& h) const {
  int top = std::max({h.srank[0], h.srank[1], h.srank[2]});
  if (h.el >= 0 && top > 0) return TITLES[main_style(h)][h.el];
  const Item* w = weapon_in_hand(h);
  if (const WeaponDef* wd = w ? weapon_def(w->kind) : nullptr) return wd->name;
  return "Delver";
}

void Game::refit(Hero& h) {
  const Item* w = weapon_in_hand(h);
  const WeaponDef* wd = w ? weapon_def(w->kind) : nullptr;
  const StyleDef& st = STYLES[main_style(h)];
  ClassDef& k = h.kit;
  k.key = wd ? wd->key : "fists";
  k.name = title(h);
  k.col = st.col;
  k.hp = st.hp; k.hp_lvl = st.hp_lvl; k.mana = st.mana; k.mana_lvl = st.mana_lvl; k.acc = st.acc;
  k.res = st.res;
  k.about = wd ? wd->trait : "";
  // the weapons the dark gives you: the kinds of your best style, and the kind in your hand (any can be used)
  k.kinds.clear();
  for (auto& wp : WEAPONS) if (wp.style == st.key) k.kinds.push_back(wp.key);
  if (wd && wd->style != st.key) k.kinds.push_back(wd->key);
  k.start.clear();
  k.paths.clear();
  // [1] the weapon's own skill, [2]-[5] the style skills in the order they came
  k.skills.clear();
  k.skills.push_back(wd ? wd->sig : "");
  for (size_t i = 0; i < h.known.size() && k.skills.size() < 5; ++i) k.skills.push_back(h.known[i]);
}

// The gatekeeper's rack in Charnfen: one weapon each, a first weapon and not a class.
void Game::take_weapon(Hero& h, const std::string& kind) {
  const WeaponDef* wd = weapon_def(kind);
  if (!wd) return;
  if (h.equip.count("weapon")) { h.say("The gatekeeper shakes his head. \"One each. That one is yours.\"", 0xD2B48C); return; }
  auto it = loot::make(wd->start);
  h.equip["weapon"] = it;
  refit(h);
  h.say("You take " + it->name + " from the rack. The old gatekeeper nods. \"" +
            (wd->style == "melee" ? std::string("Close in, and keep your feet.") : wd->style == "ranged" ? std::string("Keep your distance, and keep moving.") : std::string("Mind your mana. The dark eats it.")) + "\"",
        0xD2B48C);
  tip(h, "weapon", "Your hero has no class: what you do grows. Bump attacks teach Melee, [f] teaches what your weapon shoots with, every skill its own style. Skill [1] is your weapon's own.");
  if (h.el < 0) h.say("\"Cold steel won't help you down there. Take a coal from the great fire first.\"", 0xD2B48C);
  add_fx({{"k", "rise"}, {"x", h.x}, {"y", h.y}, {"col", 0xE0C080}});
}

// A kill teaches each hero who hurt it: the styles by the harm they dealt, the weapon by the harm done with it, and the
// hero's element in full. A style's new rank may bring a skill.
void Game::learn(Mon& m) {
  for (auto& [pid, w] : m.work) {
    auto it = heroes.find(pid);
    if (it == heroes.end() || !it->second->present()) continue;
    Hero& h = *it->second;
    int total = w[0] + w[1] + w[2];
    if (total <= 0) continue;
    int xp = std::max(1, int(std::lround(m.xp * catch_up(h))));  // behind the pace, learn faster
    for (int s = 0; s < 3; ++s) {
      if (!w[s]) continue;
      h.sxp[s] += std::max(1, xp * w[s] / total);
      while (h.srank[s] < 20 && h.sxp[s] >= rank_need(h.srank[s])) {
        ++h.srank[s];
        const StyleDef& sd = STYLES[s];
        h.say("Your " + sd.name + " grows: rank " + std::to_string(h.srank[s]) + ".", sd.col);
        if (h.srank[s] % 2 == 0) offer_perk(h);
        for (size_t i = 0; i < sd.skills.size(); ++i)
          if (STYLE_RANKS[i] == h.srank[s] && std::find(h.known.begin(), h.known.end(), sd.skills[i]) == h.known.end()) {
            h.known.push_back(sd.skills[i]);
            int slot = int(h.known.size()) + 1;
            const SkillDef& sk = skill_def(sd.skills[i]);
            if (slot <= 5) h.say("You learn " + sk.name + ": [" + std::to_string(slot) + "]. " + sk.about + ".", 0xFFE680);
            else h.say("You learn " + sk.name + ". Your keys are full; it waits for the fire at a waystation.", 0xFFE680);
            add_fx({{"k", "rise"}, {"x", h.x}, {"y", h.y}, {"col", sd.col}});
          }
      }
    }
    const Item* wi = weapon_in_hand(h);
    if (wi && w[3] > 0) {
      int before = mastery(h, wi->kind);
      h.wxp[wi->kind] += std::max(1, xp * w[3] / total);
      int after = mastery(h, wi->kind);
      if (after > before) {
        const WeaponDef* wd = weapon_def(wi->kind);
        h.say("Your hand knows the " + std::string(wd ? wd->name : wi->kind) + " better: mastery " + std::to_string(after) + " of " + std::to_string(MASTERY_MAX) + ".", 0xE0C080);
      }
    }
    if (h.el >= 0) {
      int e = h.el;
      h.exq[e] += xp;
      while (h.erank[e] < 20 && h.exq[e] >= rank_need(h.erank[e])) {
        ++h.erank[e];
        h.say(std::string("Your ") + elem::NAME[e] + " burns stronger: rank " + std::to_string(h.erank[e]) + ".", elem::COL[e]);
      }
    }
    refit(h);
  }
}
