// What one player is shown after a step, as JSON: the same for a player on this device and one across the internet.
#include "game.hpp"

namespace view {

std::string glyph(const Item& it) {
  if (it.slot == "weapon") return it.kind == "bow" ? "}" : it.kind == "staff" ? "/" : "|";
  if (it.slot == "body") return "[";
  if (it.slot == "helm") return "^";
  if (it.slot == "gloves" || it.slot == "boots") return "]";
  if (it.slot == "ring") return "=";
  return "\"";
}

static std::string describe(const Mon& m) {
  std::string out;
  auto add = [&](const std::string& s) { if (!out.empty()) out += "; "; out += s; };
  for (auto& a : m.mods)
    for (auto& [k, v] : data::AFFIXES) if (k == a) add(v);
  if (m.def->ai == "ranged") add("it strikes from afar");
  if (m.def->poison > 0) add("its bite poisons");
  if (m.def->fire) add("it burns");
  if (m.def->cold) add("its touch numbs");
  return out;
}

json snapshot(Game& g, Hero& h, bool with_map) {
  Floor& f = *g.floor;
  Stats st = g.stats(h);
  json out;
  out["turn"] = g.turn;
  out["state"] = g.state;
  out["depth"] = g.depth;
  out["act"] = f.act;
  out["rest"] = f.rest;
  out["floor"] = f.id;
  if (with_map) {
    std::string m(W * H, '0');
    for (int i = 0; i < W * H; ++i) m[i] = char('0' + f.t[i]);
    out["map"] = m;
  }
  std::string vis(W * H, '0');
  for (int i = 0; i < W * H; ++i) if (h.vis[i]) vis[i] = '1';
  out["vis"] = vis;
  json ents = json::array();
  for (auto& m : f.mons)
    if (h.sees(m->x, m->y))
      ents.push_back({{"id", m->id}, {"x", m->x}, {"y", m->y}, {"ch", m->def->ch}, {"col", m->col}, {"name", m->name},
                      {"hp", m->hp}, {"max", m->maxhp}, {"rank", m->rank}, {"frozen", m->frozen > 0}, {"stun", m->stun > 0},
                      {"about", describe(*m)}});
  if (f.has_merchant)
    ents.push_back({{"id", -1}, {"x", f.merchant.x}, {"y", f.merchant.y}, {"ch", int('@')}, {"col", 0xD2B48C},
                    {"name", "the trader"}, {"rank", "npc"}, {"hp", 1}, {"max", 1}, {"about", "a trader from Ashmoor"}});
  out["ents"] = ents;
  json heroes = json::array();
  for (auto& [id, o] : g.heroes)
    if (!o->away)
      heroes.push_back({{"pid", o->pid}, {"name", o->name}, {"cls", o->cls->key}, {"x", o->x}, {"y", o->y}, {"hp", o->hp},
                        {"max", g.stats(*o).maxhp}, {"lvl", o->lvl}, {"dead", o->dead}, {"gone", o->gone}, {"col", o->cls->col}});
  out["heroes"] = heroes;
  json items = json::array();
  for (auto& fi : f.items) {
    if ((fi.owner && fi.owner != h.pid) || !h.sees(fi.x, fi.y)) continue;
    if (fi.gold > 0) items.push_back({{"x", fi.x}, {"y", fi.y}, {"ch", "$"}, {"col", 0xFFD700}, {"name", std::to_string(fi.gold) + " gold"}});
    else if (!fi.pot.empty())
      items.push_back({{"x", fi.x}, {"y", fi.y}, {"ch", "!"}, {"col", fi.pot == "heal" ? 0xFF5050 : 0x5070FF},
                       {"name", fi.pot == "heal" ? "a healing potion" : "a mana potion"}});
    else if (fi.item)
      items.push_back({{"x", fi.x}, {"y", fi.y}, {"ch", glyph(*fi.item)}, {"col", data::rarity_col(fi.item->rarity)}, {"name", fi.item->name}});
  }
  out["items"] = items;
  json writ = json::array();
  for (auto& [i, w] : f.writings) if (h.vis[i]) writ.push_back(i);
  out["writ"] = writ;
  json equip = json::array();
  for (auto& s : data::SLOTS) {
    auto it = h.equip.find(s);
    equip.push_back(it == h.equip.end() ? json(nullptr) : it->second->to_json());
  }
  json pack = json::array();
  for (auto& it : h.pack) { json j = it->to_json(); j["use"] = g.can_use(h, *it); pack.push_back(j); }
  json perks = json::array();
  for (auto& [k, v] : h.perks) perks.push_back({perk_def(k).name, v});
  json choice = nullptr;
  if (!h.perk_choices.empty()) {
    choice = json::array();
    for (auto& k : h.perk_choices.front()) { auto& p = perk_def(k); choice.push_back({k, p.name, p.text}); }
  }
  json skills = json::array();
  for (auto& k : h.cls->skills) {
    auto& s = skill_def(k);
    skills.push_back({s.name, s.mana - (k == "cleave" ? 2 * h.perk("whirl") : 0), s.target, s.about});
  }
  std::string dmg = std::to_string(st.lo) + "-" + std::to_string(st.hi) + (st.pct > 0 ? " +" + std::to_string(st.pct) + "%" : "");
  out["me"] = {{"pid", h.pid}, {"name", h.name}, {"cls", h.cls->key}, {"clsname", h.cls->name}, {"x", h.x}, {"y", h.y},
               {"hp", h.hp}, {"maxhp", st.maxhp}, {"mana", h.mana}, {"maxmana", st.maxmana}, {"lvl", h.lvl}, {"xp", h.xp},
               {"next", g.need(h.lvl)}, {"prev", h.lvl > 1 ? g.need(h.lvl - 1) : 0}, {"gold", h.gold}, {"heal", h.heal},
               {"manap", h.mana_pots}, {"dead", h.dead}, {"gone", h.gone}, {"shopping", h.shopping}, {"poison", h.poison > 0},
               {"numb", h.numb > 0}, {"shield", h.shield > 0}, {"dmg", dmg}, {"armour", st.armour}, {"acc", st.acc},
               {"crit", st.crit}, {"res", {st.res_f, st.res_c, st.res_p}}, {"equip", equip}, {"pack", pack}, {"perks", perks},
               {"choice", choice}, {"skills", skills}, {"bow", st.bow}, {"kills", h.kills}, {"col", h.cls->col}};
  if (h.shopping) {
    json shop = json::array();
    for (auto& it : f.shop) { json j = it->to_json(); j["price"] = it->value * 2; shop.push_back(j); }
    json sell = json::array();
    for (auto& it : h.pack) sell.push_back({it->id, std::max(1, it->value / 3)});
    out["shop"] = {{"items", shop}, {"potion", 25 + 5 * g.depth}, {"sell", sell}};
  }
  if (f.boss && f.boss_woke && f.boss->hp > 0) out["boss"] = {{"name", f.boss->name}, {"hp", f.boss->hp}, {"max", f.boss->maxhp}};
  json log = json::array();
  for (auto& l : h.log) log.push_back({l.text, l.col});
  h.log.clear();
  out["log"] = log;
  out["fx"] = g.fx;
  json wait = json::array();
  for (auto* o : g.active()) wait.push_back(o->pid);
  out["wait"] = wait;
  return out;
}

}  // namespace view
