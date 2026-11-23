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

bool Game::combo(const Hero& h, int style, int e) const {
  return h.el == e && std::max({h.srank[0], h.srank[1], h.srank[2]}) > 0 && main_style(h) == style;
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
        if (h.srank[s] == PATH_RANK && h.path[s] < 0) {
          h.say(sd.name + " rank " + std::to_string(PATH_RANK) + ": two paths open. Choose one at a camp fire.", 0xFFE680);
          tip(h, "path", "A path is chosen at the camp fire, after a zone's great foe. Walk into the fire.");
        }
        for (size_t i = 0; i < sd.skills.size(); ++i)
          if (STYLE_RANKS[i] == h.srank[s] && std::find(h.known.begin(), h.known.end(), sd.skills[i]) == h.known.end()) {
            h.known.push_back(sd.skills[i]);
            int slot = int(h.known.size()) + 1;
            const SkillDef& sk = skill_def(sd.skills[i]);
            if (slot <= 5) h.say("You learn " + sk.name + ": [" + std::to_string(slot) + "]. " + sk.about + ".", 0xFFE680);
            else h.say("You learn " + sk.name + ". Your keys are full; it waits for the camp fire.", 0xFFE680);
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
    // the element: the first coal learns from all the harm but what the keys set to the second coal dealt
    int second = h.el2 >= 0 ? std::min(w[4], total) : 0;
    for (int e : {h.el, h.el2}) {
      if (e < 0) continue;
      int part = e == h.el ? total - second : second;
      if (part <= 0) continue;
      h.exq[e] += std::max(1, xp * part / total);
      while (h.erank[e] < 20 && h.exq[e] >= rank_need(h.erank[e])) {
        ++h.erank[e];
        h.say(std::string("Your ") + elem::NAME[e] + " burns stronger: rank " + std::to_string(h.erank[e]) + ".", elem::COL[e]);
      }
    }
    refit(h);
  }
}

// At a camp fire: take a path where a style reached rank 5, put a known skill on a key, or set a key to the
// second coal. All free: the fire waits.
void Game::fire_action(Hero& h, const std::string& t, const json& a) {
  if (!floor->camp) { h.say("Only at a camp's fire.", 0x909090); return; }
  if (t == "path") {
    int s = a.value("s", -1), c = a.value("c", -1);
    if (s < 0 || s > 2 || c < 0 || c > 1 || h.path[s] >= 0 || h.srank[s] < PATH_RANK) return;
    h.path[s] = c;
    const PathDef& p = PATHS[s][c];
    h.perks[p.key] = 1;
    h.say("You take the path of the " + std::string(p.name) + ": " + p.about + ".", 0xFFE680);
    add_fx({{"k", "rise"}, {"x", h.x}, {"y", h.y}, {"col", STYLES[s].col}});
  } else if (t == "slot") {
    // key [k] (2..5) takes the known skill at index i, and the skill that was on it goes where that one was; the key
    // keeps its coal
    int k = a.value("k", 0), i = a.value("i", -1), at = k - 2;
    if (k < 2 || k > 5 || i < 0 || i >= int(h.known.size()) || at >= int(h.known.size()) || i == at) return;
    std::swap(h.known[at], h.known[i]);
    refit(h);
    h.say(skill_def(h.known[at]).name + " is on [" + std::to_string(k) + "] now.", 0xE0C080);
  } else if (t == "slotel") {
    int k = a.value("k", 0);
    if (k < 2 || k > 5 || h.el2 < 0) return;
    h.slot_el[k - 1] ^= 1;
  }
}

// ------------------------------------------------------------------ the hero kept between dives

json Game::record(const Hero& h) const {
  json equip = json::object(), pack = json::array();
  for (auto& [slot, it] : h.equip) equip[slot] = loot::save(*it);
  for (auto& it : h.pack) pack.push_back(loot::save(*it));
  json r = {{"v", 1}, {"name", h.name}, {"lvl", h.lvl}, {"xp", h.xp}, {"gold", h.gold}, {"kills", h.kills},
          {"srank", h.srank}, {"sxp", h.sxp}, {"erank", h.erank}, {"exq", h.exq}, {"wxp", h.wxp}, {"known", h.known},
          {"path", h.path}, {"slotel", h.slot_el}, {"el", h.el}, {"el2", h.el2}, {"perks", h.perks}, {"choices", h.perk_choices},
          {"equip", equip}, {"pack", pack}, {"heal", h.heal}, {"manap", h.mana_pots}, {"scars", h.scars}, {"rekindled", h.rekindled}, {"reroll", h.reroll_frost},
          {"frost_open", std::max(h.frost_open, state == "won" ? frost + 1 : 0)},
          {"tips", std::vector<std::string>(h.tips.begin(), h.tips.end())}};
  r.update(group_json());
  return r;
}

// the group a hero who won belongs to: everyone who came through with it (not the lost)
json Game::group_json() const {
  std::vector<std::string> names;
  for (auto& [id, o] : heroes) if (!o->lost) names.push_back(o->name);
  if (names.size() < 2 && group.empty()) return json::object();
  return {{"group", {{"id", group_id()}, {"members", names}}}};
}

std::string Game::can_join(const json& record) const {
  std::string want = record.is_object() && record.contains("group") ? record["group"].value("id", std::string()) : std::string();
  if (heroes.empty()) return "";
  if (!group.empty() && want != group) return "This dive belongs to another group: its heroes only go down together.";
  if (group.empty() && !want.empty()) return "Your hero belongs to a group, and only goes down with it.";
  return "";
}

void Game::restore(Hero& h, const json& j) {
  if (!j.is_object() || j.value("v", 0) != 1) return;
  auto arr = [&](const char* k, int* out, int n) {
    if (!j.contains(k) || !j[k].is_array()) return;
    for (int i = 0; i < n && i < int(j[k].size()); ++i) out[i] = j[k][i].get<int>();
  };
  h.lvl = std::max(1, j.value("lvl", 1)); h.xp = j.value("xp", 0); h.gold = j.value("gold", 0); h.kills = j.value("kills", 0);
  arr("srank", h.srank, 3); arr("sxp", h.sxp, 3); arr("erank", h.erank, 5); arr("exq", h.exq, 5);
  arr("path", h.path, 3); arr("slotel", h.slot_el, 5);
  h.el = std::clamp(j.value("el", -1), -1, elem::N - 1); h.el2 = std::clamp(j.value("el2", -1), -1, elem::N - 1);
  if (j.contains("wxp") && j["wxp"].is_object()) for (auto& [k, v] : j["wxp"].items()) h.wxp[k] = v.get<int>();
  if (j.contains("perks") && j["perks"].is_object()) for (auto& [k, v] : j["perks"].items()) h.perks[k] = v.get<int>();
  h.known.clear();
  for (auto& k : j.value("known", json::array())) h.known.push_back(k.get<std::string>());
  for (auto& c : j.value("choices", json::array())) h.perk_choices.push_back(c.get<std::vector<std::string>>());
  h.equip.clear();
  h.pack.clear();
  if (j.contains("equip") && j["equip"].is_object())
    for (auto& [slot, it] : j["equip"].items()) if (auto i = loot::load(it)) h.equip[slot] = i;
  for (auto& it : j.value("pack", json::array())) if (auto i = loot::load(it); i && int(h.pack.size()) < pack_max(h)) h.pack.push_back(i);
  h.scars = j.value("scars", 0); h.rekindled = j.value("rekindled", 0); h.reroll_frost = j.value("reroll", -1);
  h.frost_open = std::max(0, j.value("frost_open", 0));
  h.heal = j.value("heal", h.heal); h.mana_pots = j.value("manap", h.mana_pots);
  for (auto& t : j.value("tips", json::array())) h.tips.insert(t.get<std::string>());
  refit(h);
}

// ------------------------------------------------------------------ co-op: ghosts and the ember-run

void Game::ember_out(Hero& o, const std::string& why) {
  if (o.lost || !o.dead) return;
  o.lost = true;
  say_all(o.name + "'s ember goes out: " + why + ". " + o.name + " is gone for good.", 0xFF6060);
}

// Each turn: a ghost drifts after the nearest living friend; a living hero who steps on an ember takes it up; a carrier
// who reaches a camp fire rekindles the friend, who wakes there scarred.
void Game::embers_tick() {
  auto alive = living();
  if (alive.empty()) return;
  for (auto& [id, hp] : heroes) {
    Hero& g = *hp;
    if (!g.dead || g.lost || g.away) continue;
    Hero* near = nullptr;
    for (auto* o : alive) if (!near || cheb(o->x, o->y, g.x, g.y) < cheb(near->x, near->y, g.x, g.y)) near = o;
    if (near && cheb(near->x, near->y, g.x, g.y) > 2) {
      P c;
      if (free_near(near->x, near->y, 2, c)) { g.x = c.x; g.y = c.y; }
      else { g.x = near->x; g.y = near->y; }
    }
  }
  for (auto* h : alive)
    for (size_t i = 0; i < floor->embers.size();) {
      auto [at, pid] = floor->embers[i];
      if (at.x != h->x || at.y != h->y) { ++i; continue; }
      floor->embers.erase(floor->embers.begin() + long(i));
      h->carry.push_back(pid);
      auto it = heroes.find(pid);
      std::string who = it == heroes.end() ? "a friend" : it->second->name;
      say_all(h->name + " takes up " + who + "'s ember. It glows in their hands.", 0xFFA040);
    }
  if (floor->camp && floor->fire.x >= 0)
    for (auto* h : alive) {
      if (h->carry.empty() || cheb(h->x, h->y, floor->fire.x, floor->fire.y) > 1) continue;
      for (int pid : h->carry) {
        auto it = heroes.find(pid);
        if (it == heroes.end() || it->second->lost) continue;
        Hero& o = *it->second;
        o.dead = false;
        ++o.scars; ++o.rekindled;
        o.hp = stats(o).maxhp / 2; o.poison = 0; o.stk.clear();
        P c;
        if (free_near(floor->fire.x, floor->fire.y, 2, c)) { o.x = c.x; o.y = c.y; }
        add_fx({{"k", "rise"}, {"x", o.x}, {"y", o.y}, {"col", elem::COL[elem::EMBER]}});
        say_all(h->name + " lays the ember in the fire. " + o.name + " is rekindled, and keeps a scar.", 0xFFD27F);
        o.say("You wake by the fire. A scar stays: a tenth of your life is gone for good.", 0xB4DCB4);
      }
      h->carry.clear();
    }
}

// ------------------------------------------------------------------ deep perks and keystones (plan.md section 7)
// A deep perk fits a hero whose element or style it names; a duo perk is only open once the hero owns both halves; the
// friends' perks only come in co-op.
bool Game::owns(const Hero& h, const std::string& need) const {
  if (need.empty() || need == "helper") return true;
  if (need == "coop") return heroes.size() > 1;
  if (auto plus = need.find('+'); plus != std::string::npos) return owns(h, need.substr(0, plus)) && owns(h, need.substr(plus + 1));
  static const char* STYLE[3] = {"melee", "ranged", "magic"};
  for (int s = 0; s < 3; ++s) if (need == STYLE[s]) return h.srank[s] >= 3 || main_style(h) == s;
  for (int e = 0; e < elem::N; ++e) {
    std::string n = elem::NAME[e];
    for (auto& c : n) c = char(std::tolower(c));
    if (need == n) return h.el == e || h.el2 == e;
  }
  return false;
}

std::vector<std::string> Game::perk_offer(const Hero& h, bool keystone) {
  std::vector<std::string> fit, open;
  for (auto& p : PERKS) {
    if (h.perk(p.key) || (keystone ? p.kind != "key" : p.kind != "deep" && p.kind != "duo")) continue;
    bool offered = false;
    for (auto& c : h.perk_choices) for (auto& k : c) if (k == p.key) offered = true;
    if (offered) continue;
    if (p.kind == "duo" && !owns(h, p.need)) continue;
    if (p.need == "coop" && !owns(h, p.need)) continue;
    if (!p.need.empty() && p.need != "helper" && p.need != "coop" && owns(h, p.need)) fit.push_back(p.key); else open.push_back(p.key);
  }
  rnd.shuffle(fit);
  rnd.shuffle(open);
  std::vector<std::string> out;
  if (keystone) {
    for (auto& k : open) if (out.size() < 2) out.push_back(k);
    if (!out.empty()) out.push_back("neither");
    return out;
  }
  // at least one that fits the hero's element or style; the rest from everything
  if (!fit.empty()) { out.push_back(fit.back()); fit.pop_back(); }
  for (auto& k : fit) open.push_back(k);
  rnd.shuffle(open);
  for (auto& k : open) if (out.size() < 3) out.push_back(k);
  return out;
}

// once a Frost level, by a camp's fire, the offer in front may be rolled again
void Game::reroll_perks(Hero& h) {
  if (h.perk_choices.empty()) return;
  if (!floor->camp && floor->depth != 0) { h.say("Only by a camp's fire can an offer be weighed again.", 0x909090); return; }
  if (h.reroll_frost == frost) { h.say("You have weighed an offer again once on this Frost level already.", 0x909090); return; }
  auto& front = h.perk_choices.front();
  bool keystone = std::find(front.begin(), front.end(), "neither") != front.end();
  bool deep = keystone || (!front.empty() && perk_def(front[0]).kind != "");
  auto old = front;
  h.perk_choices.erase(h.perk_choices.begin());
  std::vector<std::string> fresh;
  if (deep) fresh = perk_offer(h, keystone);
  if (fresh.size() <= (keystone ? 1u : 0u)) { h.perk_choices.insert(h.perk_choices.begin(), old); h.say("Nothing else is on offer.", 0x909090); return; }
  h.perk_choices.insert(h.perk_choices.begin(), fresh);
  h.reroll_frost = frost;
  h.say("You stare into the fire, and other roads show themselves.", 0xFFB040);
}

// the perks that change the harm a hero deals to a foe (Game::hurt)
double Game::perk_harm(Hero& h, Mon& m) {
  double k = 1;
  if (h.perk("giantslayer") && m.rank != "normal") k *= 1.10;
  if (h.perk("finisher") && m.hp * 4 < m.maxhp) k *= 1.25;
  if (h.perk("brittle") && m.frozen > 0) k *= 1.25;
  if (h.perk("blindrage")) k *= 1 + .10 * m.stk.n[elem::ASH];
  if (h.perk("hearthkin")) {
    bool warm = floor->burning(h.x, h.y);
    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) if (elem::fiery(floor->tile(h.x + dx, h.y + dy))) warm = true;
    if (warm) k *= 1.15;
  }
  if (h.perk("shoulder")) for (auto* o : active()) if (o != &h && cheb(o->x, o->y, h.x, h.y) == 1) { k *= 1.10; break; }
  if (h.perk("pointblank") && credit_ == 1 && cheb(m.x, m.y, h.x, h.y) <= 1) k *= 1.25;
  if (h.perk("skirmrhythm") && credit_ == 1 && h.skirm) { k *= 1.30; h.skirm = false; }
  if (h.perk("stillhand") && discharging_ >= 0 && h.moved < turn - 1) k *= 1.20;
  if (h.perk("openveins") && discharging_ == elem::BLOOD && m.hp * 2 < m.maxhp) k *= 1.20;
  if (h.perk("overchannel") && full_mana_ && &h == acting_) k *= 1.5;
  if (h.perk("thermalshock") && m.frozen > 0 && hero_el(h) == elem::EMBER) {
    k *= 3;
    m.frozen = 0;
    say_near(m.x, m.y, "The frozen " + m.name + " cracks apart in the heat!", elem::COL[elem::EMBER]);
  }
  // keystones
  if (h.perk("glasspyre")) k *= 1.30;
  if (h.perk("packmule")) k *= .90;
  if (h.perk("hermit")) k *= heroes.size() > 1 ? .90 : 1.20;
  if (h.perk("berserkheart") && h.fury >= 10) k *= 1.25;
  if (h.perk("onecraft") && discharging_ >= 0 && discharging_ == h.el) k *= 1.5;
  if (h.perk("deathwish")) { Stats st = stats(h); k *= 1 + .05 * ((st.maxhp - h.hp) * 10 / std::max(1, st.maxhp)); }
  if (h.perk("gambler")) k *= .5 + 1.5 * rnd.real();
  return k;
}
