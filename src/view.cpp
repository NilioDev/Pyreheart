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

static json cmp(const Game& g, const Hero& h, const Item& it) {
  Verdict v = g.compare(h, it);
  json d = json::array();
  for (auto& c : v.d) d.push_back({c.what, c.by});
  return {{"v", v.v}, {"d", d}};
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
    std::string seen(W * H, '0');
    for (int i = 0; i < W * H; ++i) if (h.seen[i]) seen[i] = '1';
    out["seen"] = seen;
  }
  std::string vis(W * H, '0');
  for (int i = 0; i < W * H; ++i) if (h.vis[i]) vis[i] = '1';
  out["vis"] = vis;
  json ents = json::array();
  for (auto& m : f.mons)
    if (h.sees(m->x, m->y))
      ents.push_back({{"id", m->id}, {"x", m->x}, {"y", m->y}, {"ch", m->def->ch}, {"col", m->col}, {"name", m->name},
                      {"hp", m->hp}, {"max", m->maxhp}, {"rank", m->rank}, {"frozen", m->frozen > 0}, {"stun", m->stun > 0},
                      {"about", describe(*m)}, {"spent", m->spent > 0}, {"bleed", m->bleed > 0}, {"pinned", m->pinned > 0},
                      {"busy", g.busy(*m, h)}});
  json runes = json::array();
  for (auto& r : f.runes) if (h.sees(r.p.x, r.p.y) || r.owner == h.pid) runes.push_back({r.p.x, r.p.y, r.owner});
  out["runes"] = runes;
  json ground = json::array();
  for (auto& gr : f.ground) if (h.sees(gr.p.x, gr.p.y)) ground.push_back({gr.p.x, gr.p.y, gr.kind});
  out["ground"] = ground;
  // blows being wound up: where they land and in how many turns
  json marks = json::array();
  for (auto& m : f.mons) {
    if (m->blow.turns <= 0) continue;
    json cs = json::array();
    for (auto& c : m->blow.cells) if (h.sees(c.x, c.y)) { cs.push_back(c.x); cs.push_back(c.y); }
    if (!cs.empty()) marks.push_back({{"k", m->blow.kind}, {"n", m->blow.turns}, {"c", cs}});
  }
  out["marks"] = marks;
  if (f.has_merchant)
    ents.push_back({{"id", -1}, {"x", f.merchant.x}, {"y", f.merchant.y}, {"ch", int('@')}, {"col", 0xD2B48C},
                    {"name", "the trader"}, {"rank", "npc"}, {"hp", 1}, {"max", 1}, {"about", "a trader from Charnfen"}});
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
      items.push_back({{"x", fi.x}, {"y", fi.y}, {"ch", glyph(*fi.item)}, {"col", data::rarity_col(fi.item->rarity)}, {"name", fi.item->name},
                       {"cmp", cmp(g, h, *fi.item)}});
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
  for (auto& it : h.pack) { json j = it->to_json(); j["use"] = g.can_use(h, *it); j["cmp"] = cmp(g, h, *it); pack.push_back(j); }
  json perks = json::array();
  for (auto& [k, v] : h.perks) perks.push_back({perk_def(k).name, v});
  json choice = nullptr;
  if (!h.perk_choices.empty()) {
    choice = json::array();
    for (auto& k : h.perk_choices.front()) { auto& p = perk_def(k); choice.push_back({k, p.name, p.text}); }
  }
  json skills = json::array();
  for (size_t i = 0; i < h.cls->skills.size(); ++i) {
    auto& k = h.cls->skills[i];
    auto& s = skill_def(k);
    int cost = s.mana;
    if (k == "charge" && h.perk("frenzy")) cost = 0;
    if (k == "tumble") cost -= 2 * h.perk("quick");
    if (k == "kindle" && h.perk("deeprunes")) cost = 0;
    skills.push_back({s.name, cost, s.target, s.about, data::SKILL_LEVELS[i]});
  }
  std::string dmg = std::to_string(st.lo) + "-" + std::to_string(st.hi) + (st.pct > 0 ? " +" + std::to_string(st.pct) + "%" : "");
  out["me"] = {{"pid", h.pid}, {"name", h.name}, {"cls", h.cls->key}, {"clsname", h.cls->name}, {"x", h.x}, {"y", h.y},
               {"hp", h.hp}, {"maxhp", st.maxhp}, {"mana", h.mana}, {"maxmana", st.maxmana}, {"lvl", h.lvl}, {"xp", h.xp},
               {"next", g.need(h.lvl)}, {"prev", h.lvl > 1 ? g.need(h.lvl - 1) : 0}, {"gold", h.gold}, {"heal", h.heal},
               {"manap", h.mana_pots}, {"dead", h.dead}, {"gone", h.gone}, {"shopping", h.shopping}, {"poison", h.poison > 0},
               {"numb", h.numb > 0}, {"shield", h.shield > 0}, {"dmg", dmg}, {"armour", st.armour}, {"acc", st.acc},
               {"crit", st.crit}, {"res", {st.res_f, st.res_c, st.res_p}}, {"equip", equip}, {"pack", pack}, {"perks", perks},
               {"choice", choice}, {"skills", skills}, {"pool", h.cls->res}, {"focus", h.focus}, {"standfast", h.standfast}, {"bow", st.bow}, {"kills", h.kills}, {"col", h.cls->col}};
  if (h.shopping) {
    json shop = json::array();
    for (auto& it : f.shop) { json j = it->to_json(); j["price"] = it->value * 2; j["cmp"] = cmp(g, h, *it); shop.push_back(j); }
    json sell = json::array();
    for (auto& it : h.pack) sell.push_back({it->id, std::max(1, it->value / 3)});
    out["shop"] = {{"items", shop}, {"potion", 25 + 5 * g.depth}, {"sell", sell}};
  }
  if (f.boss && f.boss_woke && f.boss->hp > 0) {
    auto& b = *f.boss;
    std::string doing = b.spent > 0 ? "spent: strike now" : "";
    if (b.blow.turns > 0) doing = std::string(b.blow.kind == 0 ? "red" : b.blow.kind == 1 ? "purple: break it" : "gold: stand in it") + " in " + std::to_string(b.blow.turns);
    out["boss"] = {{"name", b.name}, {"hp", b.hp}, {"max", b.maxhp}, {"phase", b.phase}, {"doing", doing}};
  }
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
