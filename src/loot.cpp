#include <cmath>

#include "game.hpp"

namespace {
int next_id = 1;
const BaseDef& base_def(const std::string& k) {
  for (auto& b : data::BASES) if (b.key == k) return b;
  return data::BASES[0];
}
const std::vector<std::string> ORDER = {"normal", "magic", "rare", "unique"};
int order(const std::string& r) { return int(std::find(ORDER.begin(), ORDER.end(), r) - ORDER.begin()); }
bool contains(const std::vector<std::string>& v, const std::string& s) { return std::find(v.begin(), v.end(), s) != v.end(); }
}  // namespace

int Item::stat(const std::string& k) const {
  for (auto& [s, v] : stats) if (s == k) return v;
  return 0;
}
void Item::add(const std::string& k, int v) {
  for (auto& [s, x] : stats) if (s == k) { x += v; return; }
  stats.push_back({k, v});
}
std::vector<std::string> Item::lines() const {
  std::vector<std::string> out;
  if (!base_name.empty()) out.push_back(base_name);
  if (slot == "weapon") out.push_back("damage " + std::to_string(lo) + "-" + std::to_string(hi));
  else if (ar > 0) out.push_back("armour " + std::to_string(ar));
  for (auto& [k, v] : stats)
    for (auto& [sk, text] : data::STAT_TEXT)
      if (sk == k) {
        std::string t = text;
        auto p = t.find("{}");
        if (p != std::string::npos) t.replace(p, 2, std::to_string(v));
        out.push_back(t);
      }
  return out;
}
std::string Item::shortname() const {
  if (slot == "weapon") return name + " (" + std::to_string(lo) + "-" + std::to_string(hi) + ")";
  if (ar > 0) return name + " [" + std::to_string(ar) + "]";
  return name;
}
json Item::to_json() const {
  return {{"id", id}, {"name", name}, {"slot", slot}, {"kind", kind}, {"rarity", rarity}, {"short", shortname()},
          {"lines", lines()}, {"value", value}, {"glyph", view::glyph(*this)}};
}

namespace loot {

json save(const Item& it) {
  json st = json::array();
  for (auto& [k, v] : it.stats) st.push_back({k, v});
  return {{"base", it.base}, {"name", it.name}, {"slot", it.slot}, {"kind", it.kind}, {"rarity", it.rarity}, {"bname", it.base_name},
          {"tier", it.tier}, {"lo", it.lo}, {"hi", it.hi}, {"ar", it.ar}, {"value", it.value}, {"stats", st}};
}

ItemP load(const json& j) {
  if (!j.is_object() || !j.contains("base") || !j.contains("slot")) return nullptr;
  auto it = std::make_shared<Item>();
  it->id = next_id++;
  it->base = j.value("base", ""); it->name = j.value("name", ""); it->slot = j.value("slot", ""); it->kind = j.value("kind", "");
  it->rarity = j.value("rarity", "normal"); it->base_name = j.value("bname", "");
  it->tier = j.value("tier", 1); it->lo = j.value("lo", 0); it->hi = j.value("hi", 0); it->ar = j.value("ar", 0); it->value = j.value("value", 10);
  for (auto& s : j.value("stats", json::array())) if (s.is_array() && s.size() == 2) it->stats.push_back({s[0].get<std::string>(), s[1].get<int>()});
  return it;
}

ItemP make(const std::string& base, const std::string& rarity, int tier) {
  const BaseDef& b = base_def(base);
  auto it = std::make_shared<Item>();
  it->id = next_id++;
  it->base = base; it->name = b.name; it->slot = b.slot; it->kind = b.kind; it->rarity = rarity;
  it->tier = tier ? tier : b.tier;
  if (b.slot == "weapon") {
    it->lo = b.lo; it->hi = b.hi;
    if (b.spell > 0) it->add("spell", b.spell);
  } else it->ar = b.ar;
  it->value = 10 + 15 * it->tier;
  return it;
}

static std::string add_affix(Item& it, const std::vector<AffixDef>& table, std::vector<std::string>& used, Rng& rnd) {
  std::vector<const AffixDef*> choices;
  for (auto& a : table)
    if (!contains(used, a.name) && (a.slots.empty() || contains(a.slots, it.slot))) choices.push_back(&a);
  if (choices.empty()) return "";
  const AffixDef& a = *rnd.pick(choices);
  double f = std::pow(a.grow, it.tier - 1);
  int v = std::max(1, int(std::lround(a.lo * f + rnd.real() * (a.hi * f - a.lo * f))));
  it.add(a.stat, v);
  used.push_back(a.name);
  return a.name;
}

ItemP roll(int depth, Rng& rnd, const std::vector<std::string>& kinds, const std::string& rarity0, const std::string& at_least) {
  int tier = 1 + (depth - 1) / 4;
  double r0 = rnd.real();
  if (r0 < .15) --tier; else if (r0 > .85) ++tier;
  tier = std::clamp(tier, 1, 4);
  std::string rarity = rarity0;
  if (rarity.empty()) {
    double r = rnd.real(), u = .004 + depth * .0015;
    rarity = r < u ? "unique" : r < u + .06 + depth * .006 ? "rare" : r < .45 + depth * .01 ? "magic" : "normal";
  }
  if (order(rarity) < order(at_least)) rarity = at_least;
  auto usable = [&](const BaseDef& b) { return b.slot != "weapon" || kinds.empty() || contains(kinds, b.kind); };
  if (rarity == "unique") {
    std::vector<const UniqueDef*> pool;
    for (auto& u : data::UNIQUES) if (usable(base_def(u.base))) pool.push_back(&u);
    if (pool.empty()) for (auto& u : data::UNIQUES) pool.push_back(&u);
    const UniqueDef& u = *rnd.pick(pool);
    auto it = make(u.base, "unique", std::max(base_def(u.base).tier, tier));
    it->name = u.name;
    for (auto& [k, v] : u.stats) it->add(k, v);
    it->value = 400 + 100 * it->tier;
    return it;
  }
  std::vector<const BaseDef*> bases;
  for (auto& b : data::BASES) if (b.tier <= tier && b.tier >= tier - 2 && usable(b)) bases.push_back(&b);
  if (rarity == "normal") {
    std::vector<const BaseDef*> plain;
    for (auto* b : bases) if (b->slot != "ring" && b->slot != "amulet") plain.push_back(b);
    if (!plain.empty()) bases = plain;
  }
  int sum = 0;
  for (auto* b : bases) sum += (b->slot == "weapon" || b->slot == "body") ? 3 : 1;
  int pick = rnd.below(sum);
  const BaseDef* bd = bases[0];
  for (auto* b : bases) {
    pick -= (b->slot == "weapon" || b->slot == "body") ? 3 : 1;
    if (pick < 0) { bd = b; break; }
  }
  auto it = make(bd->key, rarity, tier);
  if (bd->tier < tier) {  // an old kind of thing found deep is made better
    double k = 1 + .25 * (tier - bd->tier);
    if (it->slot == "weapon") { it->lo = int(std::lround(it->lo * k)); it->hi = int(std::lround(it->hi * k)); }
    else it->ar = int(std::lround(it->ar * k));
  }
  std::vector<std::string> used;
  if (rarity == "magic") {
    std::string pre = rnd.real() < .7 ? add_affix(*it, data::PREFIXES, used, rnd) : "";
    std::string suf = (pre.empty() || rnd.real() < .5) ? add_affix(*it, data::SUFFIXES, used, rnd) : "";
    it->name = (pre.empty() ? "" : pre + " ") + it->name + (suf.empty() ? "" : " " + suf);
  } else if (rarity == "rare") {
    int n = rnd.range(3, 4);
    for (int i = 0; i < n; ++i) add_affix(*it, rnd.coin() ? data::PREFIXES : data::SUFFIXES, used, rnd);
    it->base_name = bd->name;
    for (auto& [slot, names] : data::RARE_B)
      if (slot == it->slot) it->name = rnd.pick(data::RARE_A) + " " + rnd.pick(names);
  }
  it->value = (10 + 15 * it->tier) * (rarity == "magic" ? 3 : rarity == "rare" ? 7 : 1);
  return it;
}

}  // namespace loot
