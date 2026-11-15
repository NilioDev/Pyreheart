// A plain bot dives with each class (and once as a party of three) to see that nothing breaks and how far it gets.
#include <cstdio>
#include <cstdlib>
#include <deque>

#include "game.hpp"

using namespace data;

static json toward(Game& g, Hero& h, int tx, int ty) {
  Floor& f = *g.floor;
  std::vector<int> d(W * H, 1 << 30);
  std::deque<int> q{ty * W + tx};
  d[ty * W + tx] = 0;
  while (!q.empty()) {
    int i = q.front(); q.pop_front();
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        int nx = i % W + dx, ny = i / W + dy;
        if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
        int t = f.tile(nx, ny);
        if (blocks(t) && t != DOOR) continue;
        int j = ny * W + nx;
        if (d[j] > d[i] + 1) { d[j] = d[i] + 1; q.push_back(j); }
      }
  }
  int bd = d[h.y * W + h.x], bx = 0, by = 0;
  for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx) {
      int nx = h.x + dx, ny = h.y + dy;
      if ((!dx && !dy) || nx < 0 || nx >= W || ny < 0 || ny >= H || g.hero_at(nx, ny)) continue;
      if (d[ny * W + nx] < bd) { bd = d[ny * W + nx]; bx = dx; by = dy; }
    }
  return {{"t", "move"}, {"dx", bx}, {"dy", by}};
}
static bool still(const json& a) { return a["dx"] == 0 && a["dy"] == 0; }

static json choose(Game& g, Hero& h) {
  if (!h.perk_choices.empty()) g.free_action(h, {{"t", "perk"}, {"k", h.perk_choices.front().front()}});
  Stats st = g.stats(h);
  if (h.hp < st.maxhp * .4 && h.heal > 0) return {{"t", "quaff"}, {"k", "heal"}};
  if (h.mana < 5 && h.mana_pots > 0 && h.cls->key == "adept") return {{"t", "quaff"}, {"k", "mana"}};
  auto score = [](const ItemP& i) { if (!i) return 0; int s = i->lo + i->hi + i->ar * 2; for (auto& [k, v] : i->stats) s += v / 3; return s; };
  for (auto& it : h.pack) {
    if (!g.can_use(h, *it)) continue;
    auto cur = h.equip.find(it->slot);
    if (score(it) > score(cur == h.equip.end() ? nullptr : cur->second)) return {{"t", "equip"}, {"id", it->id}};
  }
  Floor& f = *g.floor;
  if (h.shopping) g.free_action(h, {{"t", "buyp"}, {"k", "heal"}});
  MonP seen;
  for (auto& m : f.mons) if (h.sees(m->x, m->y) && (!seen || cheb(h.x, h.y, m->x, m->y) < cheb(h.x, h.y, seen->x, seen->y))) seen = m;
  if (seen) {
    int d = cheb(h.x, h.y, seen->x, seen->y);
    int near = 0;
    for (auto& m : f.mons) if (cheb(h.x, h.y, m->x, m->y) <= 1) ++near;
    if (h.cls->key == "adept" && h.mana >= 4) {
      if (d <= 2 && h.mana >= 10 && near >= 2) return {{"t", "skill"}, {"n", 2}};
      return {{"t", "skill"}, {"n", 1}, {"x", seen->x}, {"y", seen->y}};
    }
    if (h.cls->key == "ranger" && d > 1) return {{"t", "shoot"}, {"x", seen->x}, {"y", seen->y}};
    if (h.cls->key == "warrior" && near >= 2 && h.mana >= 5) return {{"t", "skill"}, {"n", 1}};
    if (d == 1) return {{"t", "move"}, {"dx", seen->x - h.x}, {"dy", seen->y - h.y}};
    auto a = toward(g, h, seen->x, seen->y);
    if (!still(a)) return a;
  }
  if (f.tile(h.x, h.y) == STAIRS) return {{"t", "down"}};
  const FloorItem* loot = nullptr;
  for (auto& fi : f.items)
    if ((!fi.owner || fi.owner == h.pid) && cheb(h.x, h.y, fi.x, fi.y) < 14 && (!loot || cheb(h.x, h.y, fi.x, fi.y) < cheb(h.x, h.y, loot->x, loot->y))) loot = &fi;
  if (std::getenv("BOTDBG2")) std::printf("seen=%d loot=%d,%d down=%d,%d tile=%d\n", seen?1:0, loot?loot->x:-1, loot?loot->y:-1, f.down.x, f.down.y, f.tile(f.down.x,f.down.y));
  if (loot) { auto a = toward(g, h, loot->x, loot->y); if (!still(a)) return a; }
  auto a = toward(g, h, f.down.x, f.down.y);
  if (still(a)) return {{"t", "move"}, {"dx", g.rnd.range(-1, 1)}, {"dy", g.rnd.range(-1, 1)}};
  return a;
}

static void run(const std::vector<std::string>& classes, uint64_t seed) {
  Game g(seed);
  for (size_t i = 0; i < classes.size(); ++i) g.join(int(i) + 1, "Bot" + std::to_string(i), classes[i]);
  int steps = 0;
  while (g.state == "play" && steps < 20000) {
    std::map<int, json> acts;
    for (auto& [id, h] : g.heroes) if (h->present()) acts[id] = choose(g, *h);
    if (std::getenv("BOTDBG") && steps > 19990) for (auto& [id, a] : acts) { auto& h = *g.heroes[id]; std::printf("%d,%d hp%d mana%d %s\n", h.x, h.y, h.hp, h.mana, a.dump().c_str()); }
    g.step(acts);
    if (std::getenv("BOTDBG") && steps > 19990) for (auto& [id, h] : g.heroes) for (auto& l : h->log) std::printf("  %s\n", l.text.c_str());
    if (!std::getenv("BOTDBG")) for (auto& [id, h] : g.heroes) { view::snapshot(g, *h, steps % 50 == 0); }
    ++steps;
  }
  std::string names, lvls, gear;
  for (auto& c : classes) names += (names.empty() ? "" : "+") + c;
  int kills = 0;
  for (auto& [id, h] : g.heroes) { lvls += std::to_string(h->lvl) + " "; kills += h->kills; }
  for (auto& [s, it] : g.heroes.begin()->second->equip) gear += it->shortname() + ", ";
  std::printf("%-22s seed %2llu: %-4s depth %2d%s turn %5d lvl %s kills %3d gold %4d | %s\n", names.c_str(), (unsigned long long)seed,
              g.state.c_str(), g.depth, g.floor->rest ? " (rest)" : "", g.turn, lvls.c_str(), kills, g.heroes.begin()->second->gold, gear.c_str());
}

int main() {
  for (auto& c : CLASSES) for (uint64_t s = 1; s <= 3; ++s) run({c.key}, s);
  run({"warrior", "ranger", "adept"}, 9);
  return 0;
}
