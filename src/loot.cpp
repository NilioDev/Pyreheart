#include <cmath>

#include "game.hpp"

namespace {
int next_id = 1;
const BaseDef& base_def(const std::string& k) {
  for (auto& b : data::BASES) if (b.key == k) return b;
  return data::BASES[0];
}
const std::vector<std::string> ORDER = {"normal", "magic", "rare", "unique", "legendary"};
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
  if (auto* l = loot::legend_def(legend)) { out.push_back("Power: " + l->power); out.push_back("Price: " + l->price); }
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
          {"tier", it.tier}, {"lo", it.lo}, {"hi", it.hi}, {"ar", it.ar}, {"value", it.value}, {"stats", st},
          {"band", it.band}, {"plus", it.plus}, {"legend", it.legend}};
}

ItemP load(const json& j) {
  if (!j.is_object() || !j.contains("base") || !j.contains("slot")) return nullptr;
  auto it = std::make_shared<Item>();
  it->id = next_id++;
  it->base = j.value("base", ""); it->name = j.value("name", ""); it->slot = j.value("slot", ""); it->kind = j.value("kind", "");
  it->rarity = j.value("rarity", "normal"); it->base_name = j.value("bname", "");
  it->band = j.value("band", 0); it->plus = j.value("plus", 0); it->legend = j.value("legend", std::string());
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

const LegendDef* legend_def(const std::string& key) {
  if (key.empty()) return nullptr;
  for (auto& l : data::LEGENDARIES) if (l.key == key) return &l;
  return nullptr;
}

// the Frost band: the thing is forged deeper, its base and its magic stronger, and its name says how deep
static void apply_band(Item& it, int band, const std::string& base_name) {
  if (band <= 0) return;
  band = std::min(band, int(data::BANDS.size()) - 1);
  it.band = band;
  double k = 1 + .3 * band, a = 1 + .25 * band;
  it.lo = int(std::lround(it.lo * k)); it.hi = int(std::lround(it.hi * k)); it.ar = int(std::lround(it.ar * k));
  for (auto& [s, v] : it.stats) v = int(std::lround(v * a));
  std::string bn = data::BANDS[band] + " " + base_name;
  if (it.rarity == "normal" || it.rarity == "magic") {
    auto p = it.name.find(base_name);
    if (p != std::string::npos) it.name.replace(p, base_name.size(), bn);
  } else it.base_name = bn;
  it.value = it.value * (1 + band);
}

ItemP legendary(Rng& rnd, const std::vector<std::string>& kinds, int band) {
  std::vector<const LegendDef*> pool;
  for (auto& l : data::LEGENDARIES) {
    const BaseDef& b = base_def(l.base);
    if (b.slot != "weapon" || kinds.empty() || contains(kinds, b.kind)) pool.push_back(&l);
  }
  if (pool.empty()) for (auto& l : data::LEGENDARIES) pool.push_back(&l);
  const LegendDef& l = *rnd.pick(pool);
  auto it = make(l.base, "legendary", 4);
  it->name = l.name;
  it->legend = l.key;
  it->base_name = base_def(l.base).name;
  for (auto& [k, v] : l.stats) it->add(k, v);
  it->value = 1000;
  apply_band(*it, band, base_def(l.base).name);
  return it;
}

// Brann's tempering: a quarter more to the base, a gentler rise to its magic, and a step in its name
void temper(Item& it) {
  ++it.plus;
  auto up = [](int v) { return v > 0 ? std::max(v + 1, int(std::lround(v * 1.25))) : v; };
  it.lo = up(it.lo); it.hi = up(it.hi); it.ar = up(it.ar);
  for (auto& [s, v] : it.stats) if (v > 0) v += std::max(1, v / 10);
  auto p = it.name.rfind(" +");
  if (p != std::string::npos && p + 2 < it.name.size() && std::isdigit((unsigned char)it.name[p + 2])) it.name.erase(p);
  it.name += " +" + std::to_string(it.plus);
  it.value += it.value / 4;
}

ItemP roll(int depth, Rng& rnd, const std::vector<std::string>& kinds, const std::string& rarity0, const std::string& at_least, int band, int frost) {
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
  if (rarity == "legendary") return legendary(rnd, kinds, band);
  auto usable = [&](const BaseDef& b) { return b.slot != "weapon" || kinds.empty() || contains(kinds, b.kind); };
  if (rarity == "unique") {
    std::vector<const UniqueDef*> pool;
    for (auto& u : data::UNIQUES) if (usable(base_def(u.base)) && u.frost <= frost) pool.push_back(&u);
    if (pool.empty()) for (auto& u : data::UNIQUES) if (u.frost <= frost) pool.push_back(&u);
    const UniqueDef& u = *rnd.pick(pool);
    auto it = make(u.base, "unique", std::max(base_def(u.base).tier, tier));
    it->name = u.name;
    for (auto& [k, v] : u.stats) it->add(k, v);
    it->value = 400 + 100 * it->tier;
    it->base_name = base_def(u.base).name;
    apply_band(*it, band, base_def(u.base).name);
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
  apply_band(*it, band, bd->name);
  return it;
}

}  // namespace loot
