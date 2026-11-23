// What one player is shown after a step, as JSON: the same for a player on this device and one across the internet.
#include "game.hpp"
#include "lore.hpp"

namespace view {

std::string glyph(const Item& it) {
  if (it.slot == "weapon") return it.kind == "bow" || it.kind == "crossbow" ? "}" : it.kind == "staff" || it.kind == "wand" ? "/" : "|";
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
  // what its trick is, in a line
  static const std::vector<std::pair<std::string, std::string>> TRICKS = {
      {"burst:gas", "it bursts into a cloud of gas as it dies"}, {"burst:acid", "it bursts into acid as it dies"},
      {"embers", "its coal sets grease alight"}, {"trail:grease", "it leaves a trail of grease that burns"},
      {"trail:slag", "it leaves molten slag behind it"}, {"hears", "it is blind: stand still and it loses you"},
      {"heavy", "it winds up heavy blows"}, {"lob:tar", "it throws tar that sticks you fast"},
      {"lob:acid", "it throws acid that splits your armour"}, {"lob:shard", "it throws glowing shards over the others"},
      {"lob:jar", "it throws jars; a spirit may come out"}, {"leap", "it springs at you from 3 away"},
      {"latch", "it latches on and drinks your life; step away"}, {"swims", "it swims through the sludge"},
      {"brace", "its shell takes most blows; strike when it strikes"}, {"sunder", "its hammer splits your armour"},
      {"stone", "stone: it does not bleed"}, {"cough", "it coughs ash: you see less"}, {"skittish", "it strikes and runs"},
      {"dim", "near it your light shrinks"}, {"sleeper", "it stands like a statue until you come near"},
      {"rooted", "it never moves"}, {"disguise", "it looks like a heap of gold"}, {"hold", "its bite holds you"},
      {"ghost", "it goes through walls"}, {"rich", "it is heavy with gold"}, {"bleed", "its bite makes you bleed"},
      {"bolt", "it shoots heavy bolts along a line"}, {"reach", "its pike strikes from 2 away"},
      {"wall", "its shield stops arrows and spells"}, {"sweep", "it swings at all in front of it"},
      {"knock", "its blow throws you back"}, {"hook", "it hooks you with a chain and drags you in"},
      {"swarm", "it breaks into rats when hurt"}, {"grab", "it shuts you in its cage"}, {"brand", "its brand makes you hurt more"},
      {"fervour", "it hits harder the more it bleeds"}, {"incense", "its smoke hides the dead and heals them"},
      {"chill", "next to it you grow numb"}, {"shriek", "it shrieks: all within 2 reel"}, {"float", "it floats over everything"},
  };
  for (auto& [k, v] : TRICKS) if (m.def->has(k)) add(v);
  return out;
}

// the dive's omens, by name
static json omen_names(const Game& g) {
  json o = json::array();
  for (int k : g.omens) o.push_back(omen::DEF[k].name);
  return o;
}
static json omen_list(const std::vector<int>& ks) {
  json o = json::array();
  for (int k : ks) o.push_back({{"name", omen::DEF[k].name}, {"text", omen::DEF[k].text}, {"two", omen::two_sided(k)}});
  return o;
}

// the keystones a hero took, for its grave
static json keystones(const Hero& h) {
  json a = json::array();
  for (auto& [k, v] : h.perks) if (v > 0 && perk_def(k).kind == "key") a.push_back(perk_def(k).name);
  return a;
}

static json stacks(const Stacks& s) { json a = json::array(); for (int v : s.n) a.push_back(v); return a; }

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
  // the end of a dive: a hero who beat the King is kept for the next Frost level; one who fell gets a grave (a friend who fell in a dive won is woken by the victory)
  if (g.state == "won" && !h.lost) out["record"] = g.record(h);
  if (g.state == "over" || h.lost)
    out["grave"] = {{"name", h.name}, {"title", h.cls->name}, {"lvl", h.lvl}, {"frost", std::max(g.frost, h.frost_open - 1)}, {"depth", f.depth},
                    {"omens", omen_names(g)}, {"died_at", g.frost},
                    {"zone", data::ACTS[f.act].name}, {"killer", h.killer.empty() ? "the dark" : h.killer}, {"kills", h.kills},
                    {"coop", g.heroes.size() > 1}, {"rekindled", h.rekindled}, {"el", h.el}, {"el2", h.el2},
                    {"weapon", h.equip.count("weapon") ? h.equip.at("weapon")->kind : std::string()}, {"keystones", keystones(h)}};
  out["depth"] = g.depth;
  out["act"] = f.act;
  out["rest"] = f.rest;
  out["camp"] = f.camp;
  out["town"] = f.depth == 0;
  out["floor"] = f.id;
  out["mw"] = f.w;
  out["mh"] = f.h;
  out["deep"] = f.deep;
  out["zfloor"] = g.zfloor;
  out["seed"] = std::to_string(g.seed);
  out["zone"] = data::ACTS[f.act].key;
  out["zname"] = data::ACTS[f.act].name;
  out["zfloors"] = data::ACTS[f.act].floors;
  out["god"] = h.god;
  out["frost"] = g.frost;
  out["route"] = g.route;
  if (!f.exits.empty()) {
    json ex = json::array();
    for (auto [i, z] : f.exits) ex.push_back({i % W, i / W, z});
    out["exits"] = ex;
  }
  if (with_map) {
    out["map"] = encode_map(f.t);
    std::string seen(W * H, '0');
    for (int i = 0; i < W * H; ++i) if (h.seen[i]) seen[i] = '1';
    out["seen"] = seen;
  }
  std::string vis(W * H, '0');
  for (int i = 0; i < W * H; ++i) if (h.vis[i] || f.depth == 0) vis[i] = '1';
  out["vis"] = vis;
  json ents = json::array();
  bool whisper = h.leg("whispering");  // the Whispering Hood: every foe on the map, seen or not
  for (auto& m : f.mons) {
    if (!h.sees(m->x, m->y) && !(whisper && !m->hidden && !m->dormant)) continue;
    if (m->hidden) {  // a mimic is a heap of gold like any other until it moves
      ents.push_back({{"id", m->id}, {"x", m->x}, {"y", m->y}, {"ch", int('$')}, {"col", 0xFFD700}, {"name", "gold"}, {"hp", 1}, {"max", 1},
                      {"rank", "npc"}, {"about", ""}, {"hidden", true}});
      continue;
    }
    json e = {{"id", m->id}, {"x", m->x}, {"y", m->y}, {"ch", m->def->ch}, {"col", m->dormant ? 0x8C8A86 : m->col},
              {"name", m->dormant ? "a still figure of stone" : m->name},
              {"hp", m->hp}, {"max", m->maxhp}, {"rank", m->rank}, {"frozen", m->frozen > 0}, {"stun", m->stun > 0},
              {"about", describe(*m)}, {"spent", m->spent > 0}, {"bleed", m->bleed > 0}, {"pinned", m->pinned > 0},
              {"busy", g.busy(*m, h)}};
    if (m->listen) e["listen"] = true;
    if (m->awake && !m->dormant) e["awake"] = true;
    if (m->blow.turns > 0) e["wind"] = true;
    if (m->stk.total() > 0) e["st"] = stacks(m->stk);
    e["el"] = m->el;
    if (m->discharger) e["disch"] = true;
    if (m->brace > 0) e["braced"] = true;
    if (m->awake && m->can("chill")) e["chill"] = true;
    if (m->dormant) e["dormant"] = true;
    if (m->sleep) e["asleep"] = true;
    ents.push_back(e);
  }
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
  json court = json::array();  // the Hollow Court's dead dwarves
  for (auto& [id, o] : g.heroes) if (o->present() && o->court_floor == f.id && o->court.x >= 0 && o->leg("hollowcourt") && (h.sees(o->court.x, o->court.y) || o.get() == &h)) court.push_back({o->court.x, o->court.y});
  if (!court.empty()) out["court"] = court;
  out["ents"] = ents;
  if (f.rack.x >= 0) out["rack"] = {f.rack.x, f.rack.y};
  if (f.fire.x >= 0) out["fire"] = {f.fire.x, f.fire.y};
  if (f.depth == 0 && !f.coals.empty()) out["pyre"] = {f.coals[2].first.x, f.coals[2].first.y - 3};  // the great fire's heart
  if (!f.coals.empty()) {
    json coals = json::array();
    for (auto& [c, e] : f.coals) coals.push_back({c.x, c.y, e});
    out["coals"] = coals;
  }
  if (const elem::Zone* z = f.depth > 0 && !f.rest ? &elem::ZONE[std::clamp(f.act, 0, 9)] : nullptr)
    out["zel"] = {z->atk[0], z->atk[1], z->resist, z->weak};
  json heroes = json::array();
  for (auto& [id, o] : g.heroes)
    if (!o->away)
      heroes.push_back({{"pid", o->pid}, {"name", o->name}, {"cls", o->cls->key}, {"x", o->x}, {"y", o->y}, {"hp", o->hp},
                        {"max", g.stats(*o).maxhp}, {"lvl", o->lvl}, {"st", stacks(o->stk)}, {"el", o->el}, {"dead", o->dead}, {"gone", o->gone}, {"lost", o->lost}, {"carry", o->carry.size()}, {"col", o->cls->col}});
  out["heroes"] = heroes;
  json embers = json::array();
  for (auto& [c, pid] : f.embers) if (h.sees(c.x, c.y)) embers.push_back({c.x, c.y, pid});
  out["embers"] = embers;
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
    for (auto& k : h.perk_choices.front()) { auto& p = perk_def(k); choice.push_back({k, p.name, p.text, p.kind}); }
  }
  json skills = json::array();
  bool uses_fury = h.srank[0] > 0 || g.main_style(h) == 0, uses_mana = false;
  for (size_t i = 0; i < h.cls->skills.size(); ++i) {
    auto& k = h.cls->skills[i];
    if (k.empty()) { skills.push_back({"(no weapon)", 0, false, "take a weapon from the gatekeeper's rack", 0, 0xB7, false, 0}); continue; }
    auto& s = skill_def(k);
    bool fury = Game::fury_skill(k);
    (fury ? uses_fury : uses_mana) = true;
    int cost = s.mana;
    if (k == "charge" && h.perk("frenzy")) cost = 0;
    if (k == "tumble") cost -= 2 * h.perk("quick");
    if (k == "kindle" && h.perk("deeprunes")) cost = 0;
    skills.push_back({s.name, cost, s.target, s.about, 0, elem::skill_sign(k), fury, g.skill_style(h, k)});
  }
  // the three layers: each style's rank, the xp toward the next and the rank its next skill comes at; the weapon's
  // mastery; the elements' ranks
  json styles = json::array();
  for (int i = 0; i < 3; ++i) {
    int r = h.srank[i], nxt = 0;
    for (size_t j = 0; j < data::STYLES[i].skills.size(); ++j) if (data::STYLE_RANKS[j] > r) { nxt = data::STYLE_RANKS[j]; break; }
    styles.push_back({data::STYLES[i].name, r, h.sxp[i], r > 0 ? Game::rank_need(r - 1) : 0, Game::rank_need(r), nxt, data::STYLES[i].col});
  }
  json weapon = nullptr;
  if (const WeaponDef* wd = weapon_def(h.cls->key)) {
    int m = g.mastery(h, wd->key);
    auto it = h.wxp.find(wd->key);
    weapon = {{"name", wd->name}, {"trait", wd->trait}, {"style", wd->style}, {"mastery", m}, {"xp", it == h.wxp.end() ? 0 : it->second},
              {"prev", m > 0 ? Game::mastery_need(m - 1) : 0}, {"next", m < data::MASTERY_MAX ? Game::mastery_need(m) : 0}, {"reach", wd->reach}};
  }
  json eranks = json::array();
  for (int e = 0; e < elem::N; ++e) eranks.push_back({h.erank[e], h.exq[e], h.erank[e] > 0 ? Game::rank_need(h.erank[e] - 1) : 0, Game::rank_need(h.erank[e])});
  json known = json::array();
  for (auto& k : h.known) {  // the style each came from and the rank it came at, for the fire
    int st = g.skill_style(h, k), rk = 0;
    auto& sk = data::STYLES[st].skills;
    for (size_t j = 0; j < sk.size(); ++j) if (sk[j] == k) rk = data::STYLE_RANKS[j];
    known.push_back({{"name", skill_def(k).name}, {"style", st}, {"rank", rk}, {"sign", elem::skill_sign(k)}, {"about", skill_def(k).about}, {"fury", Game::fury_skill(k)}});
  }
  std::string dmg = std::to_string(st.lo) + "-" + std::to_string(st.hi) + (st.pct > 0 ? " +" + std::to_string(st.pct) + "%" : "");
  // the elements a foe winding up near you will set off on you
  json warn = json::array();
  for (auto& m : f.mons)
    if (m->blow.turns > 0 && h.sees(m->x, m->y) && (m->discharger || m->rank != "normal"))
      for (int e = 0; e < elem::N; ++e)
        if (h.stk.n[e] > 0 && (e == m->el || m->rank == "boss")) warn.push_back(e);
  json res = json::array();
  for (int e = 0; e < elem::N; ++e) res.push_back(g.hero_res(h, e));
  out["me"] = {{"pid", h.pid}, {"st", stacks(h.stk)}, {"el", h.el}, {"el2", h.el2}, {"smother", h.smother}, {"warn", warn}, {"eres", res}, {"name", h.name}, {"cls", h.cls->key}, {"clsname", h.cls->name}, {"x", h.x}, {"y", h.y},
               {"hp", h.hp}, {"maxhp", st.maxhp}, {"mana", h.mana}, {"maxmana", st.maxmana}, {"lvl", h.lvl}, {"xp", h.xp},
               {"next", g.need(h.lvl)}, {"prev", h.lvl > 1 ? g.need(h.lvl - 1) : 0}, {"gold", h.gold}, {"heal", h.heal},
               {"manap", h.mana_pots}, {"scars", h.scars}, {"dead", h.dead}, {"gone", h.gone}, {"shopping", h.shopping}, {"talking", h.talking}, {"poison", h.poison > 0},
               {"numb", h.numb > 0}, {"shield", h.shield > 0}, {"held", h.held > 0}, {"latched", h.latched > 0},
               {"bleeding", h.bleed > 0}, {"branded", h.brand > 0}, {"sundered", h.sunder > 0}, {"dazed", h.dazed > 0}, {"dmg", dmg}, {"armour", st.armour}, {"acc", st.acc},
               {"crit", st.crit}, {"res", {st.res_f, st.res_c, st.res_p}}, {"equip", equip}, {"pack", pack}, {"perks", perks},
               {"choice", choice}, {"skills", skills}, {"pool", h.cls->res}, {"fury", h.fury}, {"maxfury", 10}, {"usesfury", uses_fury},
               {"usesmana", uses_mana || g.main_style(h) != 0}, {"styles", styles}, {"weapon", weapon}, {"eranks", eranks}, {"known", known}, {"slotel", h.slot_el}, {"path", h.path}, {"combo", h.el >= 0 && std::max({h.srank[0], h.srank[1], h.srank[2]}) > 0 ? g.main_style(h) * 5 + h.el : -1},
               {"power", g.power(h)}, {"reload", h.reload}, {"focus", h.focus}, {"standfast", h.standfast}, {"bow", st.bow}, {"kills", h.kills}, {"col", h.cls->col},
               {"packmax", g.pack_max(h)}, {"reroll", !h.perk_choices.empty() && (f.camp || f.depth == 0) && h.reroll_frost != g.frost}};
  if (h.shopping) {
    json shop = json::array();
    for (auto& it : f.shop) { json j = it->to_json(); j["price"] = g.price(h, *it); j["cmp"] = cmp(g, h, *it); shop.push_back(j); }
    json sell = json::array();
    for (auto& it : h.pack) sell.push_back({it->id, std::max(1, it->value / 3)});
    out["shop"] = {{"items", shop}, {"potion", h.perk("haggler") ? g.potion_price() * 4 / 5 : g.potion_price()}, {"sell", sell}};
  }
  if (!h.talking.empty()) {
    // the helper's window: Ysolde's potions, Brann's retuning, old Ebb's ways
    json t = {{"who", h.talking}, {"potion", h.perk("haggler") ? g.potion_price() * 4 / 5 : g.potion_price()}, {"retune", h.perk("brannsfriend") ? g.retune_price() / 2 : g.retune_price()}, {"healed", h.drank}, {"line", h.talk_line}};
    if (h.talking == "smith") {  // each resistance on worn gear, one row each
      static const char* RES[elem::N] = {"res_fire", "res_cold", "res_ash", "res_poison", "res_blood"};
      json rows = json::array();
      for (auto& sl : data::SLOTS) {
        auto it = h.equip.find(sl);
        if (it == h.equip.end()) continue;
        for (auto& [k, v] : it->second->stats)
          for (int e = 0; e < elem::N; ++e)
            if (k == RES[e]) rows.push_back({{"id", it->second->id}, {"name", it->second->shortname()}, {"rarity", it->second->rarity}, {"from", k}, {"el", e}, {"v", v}});
      }
      t["gear"] = rows;
      json tem = json::array();  // what Brann can temper: every worn thing, its step, his price
      for (auto& sl : data::SLOTS) {
        auto it = h.equip.find(sl);
        if (it == h.equip.end()) continue;
        tem.push_back({{"id", it->second->id}, {"name", it->second->shortname()}, {"rarity", it->second->rarity}, {"plus", it->second->plus}, {"price", g.temper_price(h, *it->second)}});
      }
      t["temper"] = tem;
      t["cap"] = g.temper_cap();
    }
    if (h.talking == "gate") {  // the gate guard: the Frost levels this hero (and the whole party) may choose
      int open = h.frost_open;
      for (auto& [id, o] : g.heroes) if (o->present()) open = std::min(open, o->frost_open);
      json lv = json::array();
      for (int fr = 0; fr <= open; ++fr)
        lv.push_back({{"f", fr}, {"omens", omen_list(omen::draw(g.seed, fr))}, {"bite", 10 * std::min(fr, 5)}, {"cur", fr == g.frost}});
      t["levels"] = lv;
      t["hold"] = g.res_held(h);
    }
    if (h.talking == "guide" && f.camp) {
      json ways = json::array();
      for (int z : data::ACTS[f.act].next) ways.push_back({{"z", z}, {"key", data::ACTS[z].key}, {"name", data::ACTS[z].name}, {"text", lore::EBB_WAYS[z]}});
      t["ways"] = ways;
      t["open"] = !f.exits.empty();
    }
    out["talk"] = t;
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
  if (!g.omens.empty()) out["omens"] = omen_list(g.omens);
  out["slain"] = g.slain;  // the zones whose boss fell: Sparks for the players' Pyre
  if (h.tal("forged") && !h.forged && f.camp) out["forge"] = true;
  json wait = json::array();
  for (auto* o : g.active()) wait.push_back(o->pid);
  out["wait"] = wait;
  return out;
}

}  // namespace view
