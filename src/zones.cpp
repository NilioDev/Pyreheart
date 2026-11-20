#include <cstdio>
#include <cstdlib>
// A maker of maps for each zone that is not built of the Crypt's rooms (k5), the Cistern's or the Deep's crust:
// the Caves grow like a cave, the Forge is halls of cut stone with molten metal in channels, the City is a planned
// dwarven city round a plaza, the Vaults a mirrored warren of locked vaults, the Barracks a fortress round a drill yard,
// the Prison a star of cell blocks round the pit, the Sanctum a temple with a nave, chapels and the empty altar.
// Each returns the rooms (for treasure) and leaves fl.up and fl.down set; the way from one to the other always exists.
#include <deque>
#include <map>
#include <array>

#include "game.hpp"

using namespace data;

namespace {

// a plan drawn on the map's tiles, with a few tools for rooms, rings and lines
struct Map {
  Floor& f;
  Rng& rnd;
  int w, h;
  std::vector<std::vector<P>> rooms, treasure;
  Map(Floor& fl, Rng& r) : f(fl), rnd(r), w(fl.w), h(fl.h) { f.t.assign(W * H, WALL); }
  bool in(int x, int y) const { return x >= 1 && y >= 1 && x < w - 1 && y < h - 1; }
  int at(int x, int y) const { return f.tile(x, y); }
  void put(int x, int y, int t) { if (in(x, y)) f.t[y * W + x] = t; }
  void rect(int x0, int y0, int x1, int y1, int t) { for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) put(x, y, t); }
  std::vector<P> cells(int x0, int y0, int x1, int y1) const {
    std::vector<P> v;
    for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) if (in(x, y)) v.push_back({x, y});
    return v;
  }
  void room(int x0, int y0, int x1, int y1) { rect(x0, y0, x1, y1, FLOOR); rooms.push_back(cells(x0, y0, x1, y1)); }
  // a wide way between two points: across first, then down
  void way(P a, P b, int width, int t = FLOOR) {
    int lo = -(width - 1) / 2, hi = width / 2;
    for (int x = std::min(a.x, b.x); x <= std::max(a.x, b.x); ++x) for (int o = lo; o <= hi; ++o) if (at(x, a.y + o) == WALL) put(x, a.y + o, t);
    for (int y = std::min(a.y, b.y); y <= std::max(a.y, b.y); ++y) for (int o = lo; o <= hi; ++o) if (at(b.x + o, y) == WALL) put(b.x + o, y, t);
  }
  void disc(P c, double r, int t) {
    for (int y = int(c.y - r - 1); y <= int(c.y + r + 1); ++y)
      for (int x = int(c.x - r - 1); x <= int(c.x + r + 1); ++x)
        if ((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y) <= r * r) put(x, y, t);
  }
  // walking distances from a cell over open ground (doors and vault doors count as open)
  std::vector<int> walk(P from) const {
    std::vector<int> d(W * H, -1);
    std::deque<int> q{from.y * W + from.x};
    d[q.front()] = 0;
    while (!q.empty()) {
      int i = q.front(); q.pop_front();
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
          int x = i % W + dx, y = i / W + dy;
          if (!in(x, y) || d[y * W + x] >= 0) continue;
          int t = at(x, y);
          if (blocks(t) && t != DOOR && t != VAULTDOOR) continue;
          d[y * W + x] = d[i] + 1;
          q.push_back(y * W + x);
        }
    }
    return d;
  }
  // put a piece of furniture only where it closes no way
  bool place(int x, int y, int t) {
    if (!in(x, y) || at(x, y) != FLOOR) return false;
    put(x, y, t);
    if (!whole(f)) { put(x, y, FLOOR); return false; }
    return true;
  }
};

}  // namespace

// The Caves' themed places, laid into the grown cave: a straight tunnel the dwarves cut through the rock, held up by
// timber frames (one of them broken), a shaft that fell in
// with daylight far above, veins of copper and gold in the walls, and on the boss's map the Webmother's nest.
static void dress_caves(Map& m, Floor& fl, const std::vector<int>& cave, P up, P down, bool boss_floor) {
  Rng& rnd = m.rnd;
  int w = m.w, h = m.h;
  auto open = [&](int x, int y) { return m.at(x, y) == FLOOR; };
  auto near_stairs = [&](int x, int y, int r) { return cheb(x, y, up.x, up.y) <= r || cheb(x, y, down.x, down.y) <= r; };
  // a straight cut, three wide, between two open cells on one row (or column) with rock between them
  auto find_cut = [&](bool across, int lo, int hi, P& a, P& b) {
    for (int tries = 0; tries < 400; ++tries) {
      int j = cave[rnd.below(int(cave.size()))];
      a = {j % W, j / W};
      if (across ? (a.y < 3 || a.y > h - 4) : (a.x < 3 || a.x > w - 4)) continue;
      int len = rnd.range(lo, hi), dir = rnd.coin() ? 1 : -1;
      b = across ? P{a.x + dir * len, a.y} : P{a.x, a.y + dir * len};
      if (!m.in(b.x, b.y) || (across ? (b.x < 2 || b.x > w - 3) : (b.y < 2 || b.y > h - 3)) || !open(b.x, b.y)) continue;
      int rock = 0;
      for (int k = 1; k < len; ++k) {
        int x = across ? a.x + dir * k : a.x, y = across ? a.y : a.y + dir * k;
        rock += m.at(x, y) == WALL;
      }
      if (rock * 10 < len * 4) continue;
      if (b.x < a.x || b.y < a.y) std::swap(a, b);
      return true;
    }
    return false;
  };
  auto carve = [&](P a, P b, bool across) {
    for (int k = 0; k <= (across ? b.x - a.x : b.y - a.y); ++k)
      for (int o = -1; o <= 1; ++o) {
        int x = across ? a.x + k : a.x + o, y = across ? a.y + o : a.y + k;
        if (m.at(x, y) == WALL) m.put(x, y, FLOOR);
      }
  };
  // the propped tunnel: timber frames every four steps, one of them broken and the ceiling come down a little
  {
    P a, b;
    bool across = rnd.coin();
    if (find_cut(across, 14, 22, a, b)) {
      carve(a, b, across);
      int n = across ? b.x - a.x : b.y - a.y, broken = rnd.range(1, std::max(1, n / 4 - 1));
      for (int k = 2, f = 0; k <= n - 2; k += 4, ++f) {
        auto cell = [&](int o) { return across ? P{a.x + k, a.y + o} : P{a.x + o, a.y + k}; };
        for (int o : {-2, 2}) { P c = cell(o); if (m.at(c.x, c.y) == WALL) m.put(c.x, c.y, TIMBER); }
        if (f == broken) {
          P c = cell(rnd.coin() ? -1 : 1);
          m.place(c.x, c.y, BEAM);
          for (int r = 0; r < 4; ++r) {
            P q = cell(rnd.range(-1, 1));
            q.x += across ? rnd.range(-1, 1) : 0; q.y += across ? 0 : rnd.range(-1, 1);
            if (m.at(q.x, q.y) == FLOOR) m.put(q.x, q.y, RUBBLE);
          }
        } else {
          for (int o : {-1, 1}) { P c = cell(o); if (!near_stairs(c.x, c.y, 1)) m.place(c.x, c.y, PROP); }
        }
      }
    }
  }
  // a shaft that fell in: a heap of rock and beams in an open part of the cave, daylight from far above
  for (int s = 0, made = 0; s < 60 && made < (rnd.coin() ? 1 : 2); ++s) {
    int j = cave[rnd.below(int(cave.size()))];
    P c{j % W, j / W};
    if (!open(c.x, c.y) || near_stairs(c.x, c.y, 8)) continue;
    int room = 0;
    for (int y = c.y - 3; y <= c.y + 3; ++y) for (int x = c.x - 4; x <= c.x + 4; ++x) room += open(x, y);
    if (room < 50) continue;
    for (P d : {P{0, 0}, P{-1, 0}, P{1, 0}, P{0, -1}}) m.place(c.x + d.x, c.y + d.y, HEAP);
    for (int k = 0; k < 2; ++k) {
      P d{rnd.range(-2, 2), rnd.range(-1, 1)};
      m.place(c.x + d.x, c.y + d.y, BEAM);
    }
    for (int k = 0; k < 12; ++k) {
      int x = c.x + rnd.range(-3, 3), y = c.y + rnd.range(-2, 2);
      if (open(x, y)) m.put(x, y, RUBBLE);
    }
    ++made;
  }
  // veins of copper and gold in the walls, a pick left by one of them
  for (int v = 0, made = 0; v < 80 && made < 3; ++v) {
    int j = cave[rnd.below(int(cave.size()))];
    P c{j % W, j / W};
    P wall{-1, -1};
    for (P d : {P{1, 0}, P{-1, 0}, P{0, 1}, P{0, -1}}) if (m.at(c.x + d.x, c.y + d.y) == WALL && m.in(c.x + d.x, c.y + d.y)) wall = {c.x + d.x, c.y + d.y};
    if (wall.x < 0 || !open(c.x, c.y)) continue;
    std::vector<P> vein{wall};
    int want = rnd.range(5, 9);
    for (size_t k = 0; k < vein.size() && int(vein.size()) < want; ++k)
      for (P d : {P{1, 0}, P{-1, 0}, P{0, 1}, P{0, -1}, P{1, 1}, P{-1, -1}}) {
        P q{vein[k].x + d.x, vein[k].y + d.y};
        if (!m.in(q.x, q.y) || m.at(q.x, q.y) != WALL || int(vein.size()) >= want) continue;
        bool face = false;
        for (P e : {P{1, 0}, P{-1, 0}, P{0, 1}, P{0, -1}}) face |= open(q.x + e.x, q.y + e.y) || m.at(q.x + e.x, q.y + e.y) == ORE;
        if (face && std::find(vein.begin(), vein.end(), q) == vein.end()) vein.push_back(q);
      }
    for (P q : vein) m.put(q.x, q.y, ORE);
    if (!near_stairs(c.x, c.y, 1)) m.put(c.x, c.y, PICK);
    ++made;
  }
  // somewhere with this much open ground round it in a box of (2rx+1) x (2ry+1)
  auto roomy = [&](int rx, int ry, int need, int far, P& c) {
    for (int tries = 0; tries < 120; ++tries) {
      int j = cave[rnd.below(int(cave.size()))];
      c = {j % W, j / W};
      if (!open(c.x, c.y) || near_stairs(c.x, c.y, far)) continue;
      int n = 0;
      for (int y = c.y - ry; y <= c.y + ry; ++y) for (int x = c.x - rx; x <= c.x + rx; ++x) n += open(x, y);
      if (n >= need) return true;
    }
    return false;
  };
  // an underground stream winding across the cave, too deep to wade except at the fords, with a plank bridge
  if (rnd.real() < .55) {
    auto path_to = [&](P a, P b) {
      std::vector<int> prev(W * H, -1);
      std::deque<int> q{a.y * W + a.x};
      prev[q.front()] = q.front();
      while (!q.empty()) {
        int i = q.front(); q.pop_front();
        if (i == b.y * W + b.x) break;
        for (P d : {P{1, 0}, P{-1, 0}, P{0, 1}, P{0, -1}}) {
          int x = i % W + d.x, y = i / W + d.y;
          if (!m.in(x, y) || prev[y * W + x] >= 0 || !open(x, y)) continue;
          prev[y * W + x] = i;
          q.push_back(y * W + x);
        }
      }
      std::vector<P> out;
      if (prev[b.y * W + b.x] < 0) return out;
      for (int i = b.y * W + b.x; i != a.y * W + a.x; i = prev[i]) out.push_back({i % W, i / W});
      out.push_back(a);
      return out;
    };
    // it comes out of the rock at one edge of the map and runs back into it at the other
    bool across = rnd.coin();
    auto pick_near = [&](bool low) {
      std::vector<int> c;
      for (int j : cave) {
        int v = across ? j % W : j / W, size = across ? w : h;
        if (low ? v < size / 6 : v > size - 1 - size / 6) c.push_back(j);
      }
      if (c.empty()) return P{-1, -1};
      int j = c[rnd.below(int(c.size()))];
      return P{j % W, j / W};
    };
    // from a cell out through the rock to the edge, wandering a little
    auto to_edge = [&](P from, int dir) {
      std::vector<P> out;
      P c = from;
      for (int k = 0; k < 200; ++k) {
        if (across ? (c.x <= 1 || c.x >= w - 2) : (c.y <= 1 || c.y >= h - 2)) break;
        if (across) c.x += dir; else c.y += dir;
        out.push_back(c);
        if (rnd.real() < .25) {
          int o = rnd.coin() ? 1 : -1;
          if (across && c.y + o >= 3 && c.y + o <= h - 4) { c.y += o; out.push_back(c); }
          if (!across && c.x + o >= 3 && c.x + o <= w - 4) { c.x += o; out.push_back(c); }
        }
      }
      return out;
    };
    for (int tries = 0; tries < 20; ++tries) {
      P a = pick_near(true), b = pick_near(false);
      if (a.x < 0 || b.x < 0) break;
      int jm = cave[rnd.below(int(cave.size()))];
      P mid{jm % W, jm / W};
      int va = across ? a.x : a.y, vb = across ? b.x : b.y, vm = across ? mid.x : mid.y;
      if (vm < va + 10 || vm > vb - 10) continue;
      auto p1 = path_to(a, mid), p2 = path_to(mid, b);
      if (p1.empty() || p2.empty()) continue;
      auto ea = to_edge(a, -1), eb = to_edge(b, 1);
      std::vector<P> path(ea.rbegin(), ea.rend());
      path.insert(path.end(), p1.rbegin(), p1.rend());
      path.insert(path.end(), p2.rbegin() + 1, p2.rend());
      path.insert(path.end(), eb.begin(), eb.end());
      std::vector<std::pair<P, int>> laid;
      int n = int(path.size()), bridge = int(ea.size()) + int(p1.size() + p2.size()) / 2;
      P side = across ? P{0, 1} : P{1, 0};
      for (int k = 0; k < n; ++k) {
        P c = path[k];
        bool ford = k % 9 >= 6, plank = k == bridge;
        for (P d : {P{0, 0}, side}) {
          int x = c.x + d.x, y = c.y + d.y;
          if (!m.in(x, y) || near_stairs(x, y, 2)) continue;
          int was = m.at(x, y);
          if (was == WALL) {  // out in the rock it runs deep, where nobody walks
            laid.push_back({{x, y}, WALL});
            m.put(x, y, WATER);
            continue;
          }
          if (!open(x, y)) continue;
          laid.push_back({{x, y}, FLOOR});
          // deep only out in the open: where the stream runs down a narrow way it is shallow, or it would close it
          int room = 0;
          for (int yy = y - 2; yy <= y + 2; ++yy) for (int xx = x - 2; xx <= x + 2; ++xx) room += open(xx, yy) || m.at(xx, yy) == WATER;
          m.put(x, y, plank ? PLANK : ford || room < 20 ? SHALLOW : WATER);
        }
      }
      if (getenv("PYREHEART_GENDBG")) {
        int deep = 0; for (auto& [c, t] : laid) deep += m.at(c.x, c.y) == WATER;
        fprintf(stderr, "stream %zu cells, %d deep, whole %d\n", laid.size(), deep, int(whole(fl)));
      }
      if (!whole(fl))
        for (auto& [c, t] : laid) if (m.at(c.x, c.y) == WATER && t == FLOOR) m.put(c.x, c.y, SHALLOW);
      if (!whole(fl)) for (auto& [c, t] : laid) m.put(c.x, c.y, t);
      break;
    }
  }
  // a hall of dripstone: stalagmites great and small, with room left to fight among them
  if (P c; rnd.real() < .6 && roomy(6, 4, 70, 6, c)) {
    for (int y = c.y - 4; y <= c.y + 4; ++y)
      for (int x = c.x - 6; x <= c.x + 6; ++x) {
        if (!open(x, y)) continue;
        double r = rnd.real();
        if (r < .11) m.place(x, y, STALAG);
        else if (r < .19) m.put(x, y, STUB);
      }
  }
  // a chasm across an open part of the cave, a plank bridge over it
  if (P c; rnd.real() < .45 && roomy(5, 5, 90, 8, c)) {
    std::vector<P> laid;
    for (int dy = -5; dy <= 5; ++dy) {
      int wx = c.x + int(std::lround(std::sin(dy * .7) * 1.2));
      for (int x = wx - 2; x <= wx + 1 + (std::abs(dy) < 3); ++x)
        if (open(x, c.y + dy)) { m.put(x, c.y + dy, dy == 0 ? PLANK : CHASM); laid.push_back({x, c.y + dy}); }
    }
    if (!whole(fl)) for (P q : laid) m.put(q.x, q.y, FLOOR);
  }
  // a rat midden: a heap of gnawed bones and filth with holes in it (the rats come with the foes)
  if (P c; rnd.real() < .6 && roomy(4, 3, 40, 7, c)) {
    std::vector<P> heap;
    for (int y = c.y - 2; y <= c.y + 2; ++y)
      for (int x = c.x - 3; x <= c.x + 3; ++x)
        if (open(x, y) && (x - c.x) * (x - c.x) + 2 * (y - c.y) * (y - c.y) <= 9 + rnd.below(3)) { m.put(x, y, MIDDEN); heap.push_back({x, y}); }
    rnd.shuffle(heap);
    int holes = 0;
    for (P q : heap) {
      if (holes >= 3) break;
      m.put(q.x, q.y, FLOOR);
      if (m.place(q.x, q.y, BURROW)) ++holes;
      else m.put(q.x, q.y, MIDDEN);
    }
  }
  // a trog camp: a fire, hides round it, totems of bone at the edge (the trogs sleep by the fire, with the foes)
  if (P c; rnd.real() < .5 && roomy(5, 4, 75, 9, c)) {
    if (m.place(c.x, c.y, CAMPFIRE)) {
      for (int k = 0; k < 4; ++k) {
        double a = k * 1.5707963 + rnd.real() * .6;
        int x = c.x + int(std::lround(std::cos(a) * 3)), y = c.y + int(std::lround(std::sin(a) * 2));
        for (int o = 0; o < 2; ++o) if (open(x + o, y)) m.put(x + o, y, HIDE);
      }
      for (int k = 0; k < 4; ++k) {
        double a = k * 1.5707963 + .7853982;
        m.place(c.x + int(std::lround(std::cos(a) * 5)), c.y + int(std::lround(std::sin(a) * 3)), TOTEM);
      }
    }
  }
  // a crystal geode: a small pocket in the rock, its walls grown with crystals
  for (int tries = 0; tries < 200; ++tries) {
    int j = cave[rnd.below(int(cave.size()))];
    P c{j % W, j / W};
    if (!open(c.x, c.y) || near_stairs(c.x, c.y, 4)) continue;
    int n = 0;
    for (int y = c.y - 2; y <= c.y + 2; ++y) for (int x = c.x - 2; x <= c.x + 2; ++x) n += open(x, y);
    if (n < 8 || n > 15) continue;
    std::vector<P> lining;
    for (int y = c.y - 3; y <= c.y + 3; ++y)
      for (int x = c.x - 3; x <= c.x + 3; ++x) {
        if (!m.in(x, y) || m.at(x, y) != WALL) continue;
        bool face = false;
        for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) face |= open(x + dx, y + dy) && cheb(x + dx, y + dy, c.x, c.y) <= 2;
        if (face) lining.push_back({x, y});
      }
    if (lining.size() < 6) continue;
    for (P q : lining) m.put(q.x, q.y, CRYSTAL);
    break;
  }
  // the Webmother's nest round the way down, where she waits: egg sacs, the bodies she spun, bones; along the walls,
  // so the middle stays free to fight in
  if (boss_floor) {
    std::vector<P> hug;
    for (int y = down.y - 10; y <= down.y + 10; ++y)
      for (int x = down.x - 12; x <= down.x + 12; ++x) {
        if (!open(x, y) || cheb(x, y, down.x, down.y) < 3) continue;
        int walls = 0;
        for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) walls += m.at(x + dx, y + dy) == WALL;
        if (walls >= 3) hug.push_back({x, y});
      }
    rnd.shuffle(hug);
    int eggs = 0, bodies = 0, bones = 0;
    for (P c : hug) {
      if (eggs < 14 && m.place(c.x, c.y, EGG)) { ++eggs; continue; }
      if (bodies < 5 && m.place(c.x, c.y, COCOON)) { ++bodies; continue; }
      if (bones < 16 && open(c.x, c.y)) { m.put(c.x, c.y, BONES); ++bones; }
    }
  }
}

// The Forge, left in haste long ago: every hall gets one of the smiths' places as they left it, tools dropped where
// they fell and the work half done. The fires are out; only the metal in the channels still runs, and the old embers
// in the great furnace and the slag still glow a little.
static void dress_forge(Map& m, Floor& fl, const std::vector<std::array<int, 4>>& halls, const std::vector<int>& chan,
                        int furnace, P up, P down) {
  Rng& rnd = m.rnd;
  auto near_stairs = [&](int x, int y, int r) { return cheb(x, y, up.x, up.y) <= r || cheb(x, y, down.x, down.y) <= r; };
  // one sign a thing; the ones that block are only laid where they close no way
  auto tile_for = [](const std::string& c) -> int {
    static const std::map<std::string, int> K = {
        {"π", ANVIL}, {"I", WORKPIECE}, {"τ", HAMMER}, {"√", TONGS}, {"n", STOOL}, {"a", APRON}, {"o", BUCKET},
        {":", STAIN}, {"u", TANKARD}, {"!", BLADE}, {"¥", RACK}, {"▲", SLAG}, {"%", BONES}, {"Ω", SKULL}, {"S", SLAGPIT},
        {"e", EMBERS}, {"Ü", LADLE}, {"~", SLUNG}, {"¦", HOOK}, {"■", HOIST}, {"§", CHAIN}, {"V", CRUCIBLE}, {"s", SPILL},
        {"=", TROUGH}, {"m", MOULD}, {"c", CAST}, {"H", STACK}, {"E", EMBERS}, {"B", BONEHEAP}, {"k", COALHEAP},
        {"∩", BARROW}, {"/", SHOVEL}, {"b", BENCH}, {"G", HUSK}, {"◘", PLATES}, {"¤", BOLTS}, {"¬", ARM}, {"R", RIM},
        {"Q", QUENCH}, {"J", SCRAP}, {"O", STONEWORK}, {"L", BELLOWS}};
    auto it = K.find(c);
    return it == K.end() ? -1 : it->second;
  };
  // the signs of a row, one cell each (some are more than a byte)
  auto cells_of = [](const std::string& row) {
    std::vector<std::string> v;
    for (size_t i = 0; i < row.size();) {
      unsigned char c = row[i];
      size_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
      v.push_back(row.substr(i, n));
      i += n;
    }
    return v;
  };
  auto stamp = [&](const std::vector<std::string>& plan, int x0, int y0, bool force = false) {
    for (size_t r = 0; r < plan.size(); ++r) {
      auto cs = cells_of(plan[r]);
      for (size_t k = 0; k < cs.size(); ++k) {
        int t = tile_for(cs[k]), x = x0 + int(k), y = y0 + int(r);
        if (t < 0 || !m.in(x, y) || near_stairs(x, y, 1)) continue;
        if (force) { m.put(x, y, t); continue; }
        if (m.at(x, y) != FLOOR) continue;
        if (blocks(t)) m.place(x, y, t);
        else m.put(x, y, t);
      }
    }
  };
  auto size_of = [&](const std::vector<std::string>& plan) {
    int wd = 0;
    for (auto& r : plan) wd = std::max(wd, int(cells_of(r).size()));
    return P{wd, int(plan.size())};
  };
  // the room for a place in a hall: above its channel, or the whole hall between the pillar rows
  auto put_in = [&](const std::vector<std::string>& plan, size_t i) {
    auto [x0, y0, x1, y1] = halls[i];
    int top = y0 + 4, bot = chan[i] >= 0 ? chan[i] - 2 : y1 - 4;
    P sz = size_of(plan);
    if (sz.x > x1 - x0 - 3 || sz.y > bot - top + 1) return;
    int x = x0 + 2 + rnd.range(0, std::max(0, (x1 - x0 - 3) - sz.x)), y = top + (bot - top + 1 - sz.y) / 2;
    if (rnd.coin()) {  // the other way round, now and then
      std::vector<std::string> flipped;
      for (auto& r : plan) { auto cs = cells_of(r); std::string o; for (auto it = cs.rbegin(); it != cs.rend(); ++it) o += *it; flipped.push_back(o); }
      stamp(flipped, x, y);
    } else stamp(plan, x, y);
  };

  static const std::vector<std::vector<std::string>> PLACES = {
      // the smiths' row: anvils with the work still on them, hammers thrown down, a stool kicked over
      {"¥.¥.¥.¥........¥...¥.",
       "..........τ.........!.",
       "πI....π.....πI....π.%Ω",
       ".n√.....a..τ......o:..",
       "...........τ...::....."},
      // the chain hall's hoist, stopped half way with its load, and a chain that snapped
      {".¦..",
       ".■..",
       "§§§.",
       "..§."},
      // the slag pit, gone grey and cold, a ladle dropped by it
      {"....▲...SSSSSS...▲.",
       "......SSSSSSSSSS...",
       "...√SSSSSeSSSSSSS..",
       ".....SSSSSSSSeeSS..",
       "......SSSSSSSSSS.τ.",
       "....▲....SSSS......",
       ".........Ü........."},
      // the casting floor: the crucible tipped over, the metal set where it ran, two blades cast and the rest not
      {".........Vsss...",
       "..........sss...",
       "===========s====",
       ".c...c...m.s.m..",
       ".c...c...m.s.m..",
       "ccc.ccc.mmmsmmm.",
       ".c...c...m.s.m..",
       ".....τ.....s...."},
      // the chimney hall: soot-black stacks, the fire doors gone dull
      {"HHH.......HHH.......",
       "HHH.......HHH.......",
       "HEH....k..HEH.......",
       ".......k....../....."},
      // the bone-coal store: a barrow tipped on the way to the fire pit, its bones strewn behind it
      {"BBBBBΩ............OOOO.",
       "BBBBBBB..%....%...OEEO.",
       "BBBB/BB..%.∩%%....OOOO.",
       "BBBBB......%.....%Ω....",
       "............/....kkkk..",
       "..................kkkk."},
      // the golem workshop: golems left half built on the benches
      {"¤.◘.¤.◘..........◘.¤",
       "...........τ........",
       ".bGb.....bGb....bGb.",
       ".........◘.......¬..",
       ".bGb.....bGb....bbb.",
       "......√.......◘.◘..."},
      // the quench troughs: still black water, a blade left in it
      {"RRRRRRRRRR.....¥...¥",
       "RQQQQQQQQR....!.....",
       "RRRRRRRRRR..........",
       ".......o::..........",
       "..........RRRRRRRRRR",
       "..........RQQQQQQQQR",
       "..........RRRRRRRRRR"},
      // the scrap heaps, sorted by hand and left half sorted
      {".JJJ.....bbbb.....JJJJJ.",
       "JJJJJ....b!bb....JJJJJJJ",
       ".JJJ...∩.........JJJJJ..",
       ".......¬..........J.JJ..",
       ".........o.............."},
  };
  std::vector<int> bag;
  for (int k = 0; k < int(PLACES.size()); ++k) bag.push_back(k);
  rnd.shuffle(bag);
  size_t next = 0;
  for (size_t i = 0; i < halls.size(); ++i) {
    auto [x0, y0, x1, y1] = halls[i];
    int cx = (x0 + x1) / 2;
    if (int(i) == furnace) {
      // the great furnace in the hall's top wall: a bed of embers that still glows, the bellows hanging slack,
      // and before it a barrow of coal turned over, the shovel and hammer where they were dropped
      stamp({"LLOEEEEEEEOLL", "LLOEEEEEEEOLL", "..OOOO.OOOO.."}, cx - 6, y0, true);
      stamp({"...k.k.......", "..k∩k.....n..", "τ..k...../..u"}, cx - 13, y0 + 4);
      continue;
    }
    int pick = bag[next++ % bag.size()];
    if (pick == 1) {
      // the chain hall: chains slung between the pillars of each row, sagging in the middle
      for (int y : {y0 + 2, y1 - 2})
        for (int x = x0 + 3; x + 5 <= cx - 2 || x + 5 <= x1 - 3; x += 5) {
          if (m.at(x, y) != PILLAR) continue;
          int x2 = x + 1;
          while (x2 <= x1 && m.at(x2, y) != PILLAR) ++x2;
          if (x2 > x1 || x2 - x > 12) continue;
          for (int k = x + 1; k < x2; ++k) {
            int sag = (k - x == 1 || x2 - k == 1) ? 0 : 1;
            int yy = y + (y == y0 + 2 ? sag : -sag);
            if (m.at(k, yy) == FLOOR && !near_stairs(k, yy, 0)) m.put(k, yy, SLUNG);
          }
        }
    }
    put_in(PLACES[pick], i);
  }
  // all over: things dropped in the flight, and here and there the bones of one who did not get away
  static const int LITTER[] = {HAMMER, TONGS, TANKARD, STOOL, BUCKET, APRON, BLADE, SHOVEL, SKULL, BONES};
  for (int k = 0, made = 0; k < 400 && made < 14; ++k) {
    int x = rnd.range(2, m.w - 3), y = rnd.range(2, m.h - 3);
    if (m.at(x, y) != FLOOR || near_stairs(x, y, 2)) continue;
    int t = LITTER[rnd.below(10)];
    m.put(x, y, t);
    if (t == SKULL) for (int j = 0; j < 2; ++j) { int bx = x + rnd.range(-1, 1), by = y + rnd.range(-1, 1); if (m.at(bx, by) == FLOOR) m.put(bx, by, BONES); }
    ++made;
  }
  (void)fl;
}

bool Game::build_zone(Floor& fl, bool boss_floor, std::vector<std::vector<P>>& rooms, std::vector<std::vector<P>>& treasure) {
  int act = fl.act;
  if (act != 1 && act != 2 && act < CITY) return false;
  for (int tries = 0; tries < 12; ++tries) {
    fl.ground.clear();
    fl.writings.clear();
    Map m(fl, rnd);
    int w = m.w, h = m.h;
    P up{-1, -1}, down{-1, -1};

    if (act == 1) {
      // ---------------------------------------------------------------- the Caves: grown, not dug
      std::vector<char> rock(W * H, 1);
      for (int y = 1; y < h - 1; ++y) for (int x = 1; x < w - 1; ++x) rock[y * W + x] = rnd.real() < .46;
      for (int it = 0; it < 5; ++it) {
        auto next = rock;
        for (int y = 1; y < h - 1; ++y)
          for (int x = 1; x < w - 1; ++x) {
            int n = 0;
            for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) n += rock[(y + dy) * W + x + dx];
            next[y * W + x] = n >= 5 || (it == 0 && n == 0);  // early on, open space gets a pillar of rock
          }
        rock = next;
      }
      for (int y = 1; y < h - 1; ++y) for (int x = 1; x < w - 1; ++x) if (!rock[y * W + x]) m.put(x, y, FLOOR);
      // keep the largest cave
      std::vector<int> best;
      std::vector<char> done(W * H, 0);
      for (int i = 0; i < W * H; ++i) {
        if (fl.t[i] != FLOOR || done[i]) continue;
        auto d = m.walk({i % W, i / W});
        std::vector<int> part;
        for (int j = 0; j < W * H; ++j) if (d[j] >= 0) { part.push_back(j); done[j] = 1; }
        if (part.size() > best.size()) best = part;
      }
      if (getenv("PYREHEART_GENDBG")) fprintf(stderr, "cave size %zu\n", best.size());
      if (best.size() < 3200) continue;
      std::vector<char> keep(W * H, 0);
      for (int j : best) keep[j] = 1;
      for (int i = 0; i < W * H; ++i) if (fl.t[i] == FLOOR && !keep[i]) fl.t[i] = WALL;
      int u = best[rnd.below(int(best.size()))];
      up = {u % W, u / W};
      fl.up = up;
      auto d = m.walk(up);
      int far = 0;
      for (int j : best) if (d[j] > d[far]) far = j;
      down = {far % W, far / W};
      // cut the cave into regions round a few seeds: those are its rooms
      std::vector<P> seeds;
      for (int i = 0; i < 12; ++i) { int j = best[rnd.below(int(best.size()))]; seeds.push_back({j % W, j / W}); }
      m.rooms.assign(seeds.size(), {});
      for (int j : best) {
        int bi = 0, bd = 1 << 30;
        for (size_t k = 0; k < seeds.size(); ++k) { int dd = cheb(j % W, j / W, seeds[k].x, seeds[k].y); if (dd < bd) { bd = dd; bi = int(k); } }
        m.rooms[bi].push_back({j % W, j / W});
      }
      // the zone's own: pools of black grease that catch fire (laid as ground that stays until it burns)
      int pools = rnd.range(6, 9);
      for (int i = 0; i < pools; ++i) {
        int j = best[rnd.below(int(best.size()))];
        P c{j % W, j / W};
        if (cheb(c.x, c.y, up.x, up.y) < 6 || cheb(c.x, c.y, down.x, down.y) < 6) continue;
        double r = rnd.range(15, 35) / 10.0;
        for (int y = c.y - 4; y <= c.y + 4; ++y)
          for (int x = c.x - 4; x <= c.x + 4; ++x)
            if (m.at(x, y) == FLOOR && (x - c.x) * (x - c.x) + (y - c.y) * (y - c.y) * 2 <= r * r * 2 + rnd.below(3))
              fl.ground.push_back({{x, y}, G_GREASE, 1 << 29, 0, 0});
      }
      // glowing cracks in the floor, and timber props against the walls
      for (int i = 0; i < 9; ++i) {
        int j = best[rnd.below(int(best.size()))];
        P c{j % W, j / W};
        for (int k = 0; k < rnd.range(5, 12); ++k) {
          if (m.at(c.x, c.y) == FLOOR) m.put(c.x, c.y, FISSURE);
          c.x += rnd.range(-1, 1); c.y += rnd.range(-1, 1);
        }
      }
      for (int j : best) {
        int x = j % W, y = j / W;
        bool wall_l = m.at(x - 1, y) == WALL, wall_r = m.at(x + 1, y) == WALL;
        if ((wall_l || wall_r) && rnd.real() < .012 && cheb(x, y, up.x, up.y) > 2 && cheb(x, y, down.x, down.y) > 2) m.place(x, y, PROP);
      }
      dress_caves(m, fl, best, up, down, boss_floor);

    } else if (act == 2) {
      // ---------------------------------------------------------------- the Forge: halls of cut granite
      const int cols = 3, rows = 2;
      int cw = (w - 2) / cols, ch = (h - 2) / rows;
      std::vector<std::array<int, 4>> halls;
      for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c) {
          int hw = cw - rnd.range(5, 8), hh = ch - rnd.range(6, 9);
          hw -= hw % 2 == 0;  // odd, so it has a middle
          hh -= hh % 2 == 0;
          int x0 = 1 + c * cw + (cw - hw) / 2, y0 = 1 + r * ch + (ch - hh) / 2;
          halls.push_back({x0, y0, x0 + hw - 1, y0 + hh - 1});
          m.room(x0, y0, x0 + hw - 1, y0 + hh - 1);
        }
      auto mid = [&](int i) { auto& a = halls[i]; return P{(a[0] + a[2]) / 2, (a[1] + a[3]) / 2}; };
      for (int r = 0; r < rows; ++r) for (int c = 0; c + 1 < cols; ++c) m.way(mid(r * cols + c), mid(r * cols + c + 1), 3);
      for (int c = 0; c < cols; ++c) if (c != 1 || rnd.coin()) m.way(mid(c), mid(cols + c), 3);
      up = mid(0);
      fl.up = up;
      int furnace = cols * rows - 1;
      down = mid(furnace);
      std::vector<int> chan(halls.size(), -1);
      for (size_t i = 0; i < halls.size(); ++i) {
        auto [x0, y0, x1, y1] = halls[i];
        int cx = (x0 + x1) / 2;
        // two rows of pillars, mirrored, two in from the walls
        for (int x = x0 + 3; x <= cx - 2; x += 5) for (int y : {y0 + 2, y1 - 2}) { m.place(x, y, PILLAR); m.place(x0 + x1 - x, y, PILLAR); }
        if (int(i) == furnace || rnd.real() < .5) {
          // a channel of molten metal across the hall, with two stone crossings: left running when the smiths fled
          int y = (y0 + y1) / 2 + 4;
          chan[i] = y;
          for (int x = x0; x <= x1; ++x) for (int o = 0; o < 2; ++o) m.put(x, y + o, LAVA);
          for (int x : {x0 + (x1 - x0) / 4, x1 - (x1 - x0) / 4}) for (int o = 0; o < 2; ++o) { m.put(x, y + o, FLOOR); m.put(x + 1, y + o, FLOOR); }
          if (int(i) == furnace) {  // the great furnace at the top of the hall, its metal still running down to the channel
            for (int yy = y0 + 2; yy < y; ++yy) m.put(cx, yy, LAVA);
            down = {cx + 5, y0 + 4};
          }
          // anvils and slag on the banks
          for (int k = 0; k < 3; ++k) m.place(rnd.range(x0 + 2, x1 - 2), y + (rnd.coin() ? -2 : 3), rnd.coin() ? ANVIL : SLAG);
        }
        for (int k = 0; k < 2; ++k) { int x = rnd.range(x0 + 1, x1 - 1), y = rnd.range(y0 + 1, y1 - 1); if (m.at(x, y) == FLOOR) m.put(x, y, CHAIN); }
      }
      dress_forge(m, fl, halls, chan, furnace, up, down);
      m.treasure.push_back(m.rooms[cols]);

    } else if (act == CITY) {
      // ---------------------------------------------------------------- the City: rich once, buried in one night of ash
      // a cavern round a plaza with the dry fountain; the palace at the head of the north avenue, walled in red like a
      // forbidden city; houses in rows, a bath house, an inn, a scribes' hall and a sun temple among them; a great
      // stair down to the way in; statues of the ancestors along the avenue, grates of the air shafts, drifts of ash
      P o{w / 2, h / 2}, c{w / 2, h / 2 + 2};
      for (int y = 1; y < h - 1; ++y)
        for (int x = 1; x < w - 1; ++x) {
          double dx = (x - o.x) / (w / 2.0 - 2), dy = (y - o.y) / (h / 2.0 - 2);
          if (dx * dx + dy * dy < .97 + .06 * rnd.real()) m.put(x, y, FLOOR);
        }
      // the palace: outer wall, the golden river with five bridges, the inner wall and its lions, the marble terrace
      // with the great hall, and a side hall either side
      int px0 = c.x - 24, px1 = c.x + 24, py0 = 1, py1 = 21;
      m.rect(px0 - 2, py0, px1 + 2, py1 + 3, FLOOR);
      for (int x = px0; x <= px1; ++x) { m.put(x, py0, REDWALL); m.put(x, py1, REDWALL); }
      for (int y = py0; y <= py1; ++y) { m.put(px0, y, REDWALL); m.put(px1, y, REDWALL); }
      for (int x : {c.x - 1, c.x, c.x + 1, c.x - 9, c.x - 8, c.x + 8, c.x + 9}) m.put(x, py1, FLOOR);
      for (int y : {py1 - 3, py1 - 2})
        for (int x = px0 + 1; x <= px1 - 1; ++x) m.put(x, y, GOLDBED);
      for (int x : {c.x - 1, c.x, c.x + 1, c.x - 5, c.x + 5, c.x - 9, c.x - 8, c.x + 8, c.x + 9})
        for (int y : {py1 - 3, py1 - 2}) m.put(x, y, BRIDGE);
      int iw = py1 - 5;  // the inner wall
      for (int x = px0 + 1; x <= px1 - 1; ++x) if (std::abs(x - c.x) > 2) m.put(x, iw, REDWALL);
      m.put(c.x - 4, iw + 1, LION); m.put(c.x + 4, iw + 1, LION);
      int tx0 = c.x - 16, tx1 = c.x + 16, ty0 = py0 + 2, ty1 = iw - 2;
      m.rect(tx0, ty0, tx1, ty1, MARBLE);
      for (int x = tx0; x <= tx1; ++x) { m.put(x, ty0, BALUSTER); m.put(x, ty1, std::abs(x - c.x) <= 2 ? STEP : BALUSTER); }
      for (int y = ty0; y <= ty1; ++y) { m.put(tx0, y, BALUSTER); m.put(tx1, y, BALUSTER); }
      int hx0 = c.x - 12, hx1 = c.x + 12, hy0 = ty0 + 1, hy1 = ty1 - 3;
      for (int x = hx0; x <= hx1; ++x) { m.put(x, hy0, REDWALL); m.put(x, hy1, REDWALL); }
      for (int y = hy0; y <= hy1; ++y) { m.put(hx0, y, REDWALL); m.put(hx1, y, REDWALL); }
      m.rect(hx0 + 1, hy0 + 1, hx1 - 1, hy1 - 1, FLOOR);
      m.rooms.push_back(m.cells(hx0 + 1, hy0 + 1, hx1 - 1, hy1 - 1));
      for (int x = c.x - 1; x <= c.x + 1; ++x) m.put(x, hy1, FLOOR);
      for (int dx : {3, 6, 9}) for (int y : {hy0 + 2, hy1 - 2}) { m.put(c.x - dx, y, REDCOL); m.put(c.x + dx, y, REDCOL); }
      m.put(c.x, hy0 + 1, THRONE);
      for (int dx : {-2, -1, 1, 2}) m.put(c.x + dx, hy0 + 1, SCREEN);
      down = {c.x, hy0 + 3};
      std::vector<std::array<int, 4>> sides;
      for (int s : {-1, 1}) {
        int ax = s < 0 ? px0 + 1 : c.x + 18, bx = s < 0 ? c.x - 18 : px1 - 1, ay = ty0, by = ty1 - 3;
        for (int x = ax; x <= bx; ++x) { m.put(x, ay, REDWALL); m.put(x, by, REDWALL); }
        for (int y = ay; y <= by; ++y) { m.put(ax, y, REDWALL); m.put(bx, y, REDWALL); }
        m.rect(ax + 1, ay + 1, bx - 1, by - 1, FLOOR);
        m.put(s < 0 ? bx : ax, (ay + by) / 2, FLOOR);
        m.rooms.push_back(m.cells(ax + 1, ay + 1, bx - 1, by - 1));
        sides.push_back({ax + 1, ay + 1, bx - 1, by - 1});
      }
      m.treasure.push_back(m.rooms.back());
      auto in_palace = [&](int x0, int y0, int x1, int y1) { return x1 >= px0 - 3 && x0 <= px1 + 3 && y0 <= py1 + 3; };
      auto all_floor = [&](int x0, int y0, int x1, int y1) {
        for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) if (m.at(x, y) != FLOOR) return false;
        return true;
      };
      auto box = [&](int x0, int y0, int x1, int y1, bool shutters) {
        m.rect(x0, y0, x1, y1, HWALL);
        m.rect(x0 + 1, y0 + 1, x1 - 1, y1 - 1, FLOOR);
        if (shutters)
          for (int x = x0 + 2; x <= x1 - 2; x += 3) for (int y : {y0, y1}) if (rnd.real() < .55) m.put(x, y, SHUTTER);
      };
      // the great buildings: a bath house, an inn, a scribes' hall, the sun temple
      struct Bld { int kind, x0, y0, x1, y1, door_y; };
      std::vector<Bld> blds;
      for (int kind : {3, 0, 2, 1})
        for (int t = 0; t < 600; ++t) {
          int x0 = rnd.range(3, w - 24), y0 = rnd.range(3, h - 12), x1 = x0 + 19, y1 = y0 + 8;
          int mx = (x0 + x1) / 2, my = (y0 + y1) / 2;
          if (std::abs(mx - c.x) < 18 || std::abs(my - c.y) < 9 || in_palace(x0, y0, x1, y1)) continue;
          if (!all_floor(x0 - 1, y0 - 2, x1 + 1, y1 + 2)) continue;
          box(x0, y0, x1, y1, kind != 3);
          int door_y = my < c.y ? y1 : y0;
          m.put(mx, door_y, OPEN); m.put(mx + 1, door_y, OPEN);
          m.rooms.push_back(m.cells(x0 + 1, y0 + 1, x1 - 1, y1 - 1));
          blds.push_back({kind, x0, y0, x1, y1, door_y});
          break;
        }
      // houses: blocks between the streets, each a box with its door on the street side
      struct House { int x0, y0, x1, y1, door_x, door_y; };
      std::vector<House> houses;
      for (int by = 4; by < h - 8; by += 9)
        for (int bx = 6; bx < w - 12; bx += 12) {
          int x0 = bx + rnd.range(0, 2), y0 = by + rnd.range(0, 1), x1 = x0 + rnd.range(7, 9), y1 = y0 + rnd.range(5, 6);
          if (std::abs((x0 + x1) / 2 - c.x) < 12 || std::abs((y0 + y1) / 2 - c.y) < 9 || in_palace(x0, y0, x1, y1)) continue;
          if (!all_floor(x0 - 1, y0 - 1, x1 + 1, y1 + 1)) continue;
          box(x0, y0, x1, y1, true);
          m.rooms.push_back(m.cells(x0 + 1, y0 + 1, x1 - 1, y1 - 1));
          int door_y = (y0 + y1) / 2 < c.y ? y1 : y0, door_x = (x0 + x1) / 2;
          m.put(door_x, door_y, rnd.real() < .6 ? OPEN : DOOR);
          if (rnd.real() < .2) {  // fallen in: part of it rubble and open to the street
            int cx0 = rnd.coin() ? x0 : x1 - 2;
            for (int y = y0; y <= y1; ++y) for (int x = cx0; x <= cx0 + 2; ++x) m.put(x, y, rnd.real() < .6 ? RUBBLE : FLOOR);
          } else houses.push_back({x0, y0, x1, y1, door_x, door_y});
        }
      // and more of them wherever a house still fits, so the streets run narrow between them
      for (int k = 0; k < 900; ++k) {
        int x0 = rnd.range(3, w - 13), y0 = rnd.range(3, h - 10), x1 = x0 + rnd.range(6, 9), y1 = y0 + rnd.range(5, 6);
        if (std::abs((x0 + x1) / 2 - c.x) < 11 || std::abs((y0 + y1) / 2 - c.y) < 8 || in_palace(x0, y0, x1, y1)) continue;
        if (!all_floor(x0 - 1, y0 - 1, x1 + 1, y1 + 1)) continue;
        box(x0, y0, x1, y1, true);
        m.rooms.push_back(m.cells(x0 + 1, y0 + 1, x1 - 1, y1 - 1));
        int door_y = (y0 + y1) / 2 < c.y ? y1 : y0, door_x = (x0 + x1) / 2;
        m.put(door_x, door_y, rnd.real() < .6 ? OPEN : DOOR);
        houses.push_back({x0, y0, x1, y1, door_x, door_y});
      }
      // the avenue across: a canal down its middle with bridges, the ancestors on their plinths either side
      for (int x = 2; x < w - 2; ++x)
        if (std::abs(x - c.x) > 10 && m.at(x, c.y) == FLOOR && x % 10 != 0) m.put(x, c.y, WATER);
      for (int x = 4; x < w - 4; x += 6)
        if (std::abs(x - c.x) > 13) for (int y : {c.y - 4, c.y + 4}) if (m.at(x, y) == FLOOR) m.put(x, y, rnd.real() < .2 ? STATUE : ANCESTOR);
      // the plaza: paving, a ring of columns, the dry fountain
      for (int y = c.y - 9; y <= c.y + 9; ++y)
        for (int x = c.x - 12; x <= c.x + 12; ++x) {
          double dx = (x - c.x) / 12.0, dy = (y - c.y) / 9.0;
          if (dx * dx + dy * dy <= 1) m.put(x, y, PLAZA);
        }
      for (int k = 0; k < 12; ++k) {
        double a = k * 3.14159265 / 6 + .26;
        m.put(c.x + int(std::lround(std::cos(a) * 9)), c.y + int(std::lround(std::sin(a) * 6.5)), PILLAR);
      }
      m.rect(c.x - 1, c.y - 1, c.x + 1, c.y + 1, DRYFOUNT);
      m.rooms.push_back(m.cells(c.x - 11, c.y - 8, c.x + 11, c.y + 8));
      // the way up to the palace gate, lantern posts along it
      for (int y = py1 + 1; y < c.y; ++y) for (int x = c.x - 1; x <= c.x + 1; ++x) if (m.at(x, y) == WALL || m.at(x, y) == WATER) m.put(x, y, FLOOR);
      // the great stair down from the plaza to the way in
      int sy0 = c.y + 10, sy1 = std::min(h - 6, c.y + 17);
      for (int y = sy0; y <= sy1; ++y) for (int x = c.x - 4; x <= c.x + 4; ++x) if (m.in(x, y)) m.put(x, y, STEP);
      m.put(c.x - 6, sy0, ANCESTOR); m.put(c.x + 6, sy0, ANCESTOR);
      up = {c.x, h - 5};
      {
        int bd = 1 << 30;
        P best{-1, -1};
        for (int y = sy1 + 1; y < h - 1; ++y)
          for (int x = c.x - 8; x <= c.x + 8; ++x)
            if (m.at(x, y) == FLOOR && (h - y) * 4 + std::abs(x - c.x) < bd) { bd = (h - y) * 4 + std::abs(x - c.x); best = {x, y}; }
        if (best.x >= 0) up = best;
      }
      for (int y = sy1 + 1; y <= up.y; ++y) for (int x = c.x - 1; x <= c.x + 1; ++x) if (m.at(x, y) == WALL) m.put(x, y, FLOOR);
      if (m.at(up.x, up.y) != FLOOR) m.put(up.x, up.y, FLOOR);
      fl.up = up;
      fl.down = down;
      auto near_stairs = [&](int x, int y, int r) { return cheb(x, y, up.x, up.y) <= r || cheb(x, y, down.x, down.y) <= r; };
      auto lay = [&](int x, int y, int t) {  // a thing on the floor; the ones that block only where they close no way
        if (!m.in(x, y) || near_stairs(x, y, 1)) return false;
        int u = m.at(x, y);
        if (u != FLOOR && u != PLAZA && u != MARBLE && u != MOSAIC && u != STEP) return false;
        if (!blocks(t)) { m.put(x, y, t); return true; }
        if (u != FLOOR) { m.put(x, y, FLOOR); if (m.place(x, y, t)) return true; m.put(x, y, u); return false; }
        return m.place(x, y, t);
      };
      // the dead on the great stair, stopped where they stood
      for (int k = 0; k < 7; ++k) lay(c.x + rnd.range(-4, 4), rnd.range(sy0, sy1), k % 3 ? BONES : SKULL);
      // the market on the plaza's edge: stalls with their awnings fallen, coins nobody came back for, a column down
      for (int k = 0; k < 7; ++k) {
        int x = c.x + (k % 2 ? 1 : -1) * rnd.range(13, 20), y = c.y + rnd.range(-7, 7);
        if (std::abs(y - c.y) > 1 && lay(x, y, STALL)) lay(x, y + 1, AWNING), lay(x + 1, y + 1, AWNING);
      }
      for (int k = 0; k < 10; ++k) lay(c.x + rnd.range(-11, 11), c.y + rnd.range(-7, 7), COIN);
      lay(c.x + rnd.range(4, 8) * (rnd.coin() ? 1 : -1), c.y + rnd.range(3, 5), STATUE);
      // round the dry fountain the dead lie in white heaps, and a notice nobody read in time
      for (int k = 0; k < 10; ++k) {
        double a = rnd.real() * 6.2832, r = 2.6 + rnd.real() * 1.6;
        lay(c.x + int(std::lround(std::cos(a) * r * 1.4)), c.y + int(std::lround(std::sin(a) * r)), DEADHEAP);
      }
      lay(c.x + 3, c.y - 3, NOTICE);
      // lantern posts by the palace way, the lions' bronze vats, the palace litter
      for (int y = py1 + 2; y < c.y - 9; y += 4) { lay(c.x - 3, y, POST); lay(c.x + 3, y, POST); }
      for (int s : {-1, 1}) { lay(c.x + s * 20, iw - 1, VAT); lay(c.x + s * 14, iw + 1, VAT); lay(c.x + s * 18, py1 - 1, VAT); }
      for (int k = 0; k < 8; ++k) lay(c.x + rnd.range(-10, 10), rnd.range(hy0 + 1, hy1 - 1), COIN);
      for (auto& sd : sides) {
        lay(sd[0], sd[1], VAT); lay(sd[2], sd[1], VAT);
        for (int k = 0; k < 4; ++k) lay(rnd.range(sd[0], sd[2]), rnd.range(sd[1], sd[3]), k % 2 ? SKULL : BONES);
        for (int k = 0; k < 3; ++k) lay(rnd.range(sd[0], sd[2]), rnd.range(sd[1], sd[3]), COIN);
      }
      for (int k = 0; k < 6; ++k) lay(rnd.range(px0 + 2, px1 - 2), rnd.range(iw + 1, py1 - 1), k % 2 ? BONES : SKULL);
      // the great buildings, furnished
      for (auto& bl : blds) {
        int a = bl.x0 + 1, b = bl.x1 - 1, t = bl.y0 + 1, d = bl.y1 - 1, mx = (bl.x0 + bl.x1) / 2;
        bool down_door = bl.door_y == bl.y1;
        int far = down_door ? t : d, dir = down_door ? 1 : -1;  // the end away from the door, and the way towards it
        auto row = [&](int k) { return far + dir * k; };
        if (bl.kind == 0) {
          // the bath house: a mosaic floor, two dry pools full of ash behind their rims, columns
          m.rect(a, t, b, d, MOSAIC);
          for (int s : {0, 1}) {
            int qa = s ? b - 7 : a + 1, qb = s ? b - 1 : a + 7;
            for (int y = t + 1; y <= d - 1; ++y)
              for (int x = qa; x <= qb; ++x)
                m.put(x, y, (x == qa || x == qb || y == t + 1 || y == d - 1) ? BASIN : DRYBATH);
            m.put(s ? qa : qb, (t + d) / 2, DRYBATH);  // the steps down into it
            m.put(rnd.range(qa + 1, qb - 1), (t + d) / 2, rnd.coin() ? BONES : SKULL);
          }
          for (int x : {a, b}) for (int y : {t, d}) m.put(x, y, PILLAR);
        } else if (bl.kind == 1) {
          // the inn: long tables with the tankards still on them, kegs along the back wall
          for (int k : {1, 3}) for (int x = a + 2; x <= b - 2; ++x) if (std::abs(x - mx) > 1) m.put(x, row(k + 1), TABLE);
          for (int x = a; x <= b; ++x) if (std::abs(x - mx) > 2 && rnd.real() < .5) m.put(x, row(0), KEG);
          for (int k = 0; k < 7; ++k) lay(rnd.range(a, b), row(rnd.coin() ? 3 : 1), k % 3 ? TANKARD : STOOL);
          for (int k = 0; k < 3; ++k) lay(rnd.range(a, b), row(rnd.range(1, 5)), k ? BONES : SKULL);
        } else if (bl.kind == 2) {
          // the scribes' hall: shelves of scrolls along the walls, rows of desks, scrolls dropped on the floor
          for (int x = a; x <= b; ++x) { if (std::abs(x - mx) > 2) m.put(x, row(0), BOOKS); if (std::abs(x - mx) > 2 && x != a && x != b) m.put(x, row(6), BOOKS); }
          for (int x = a + 2; x <= b - 2; x += 4) for (int k : {2, 4}) m.put(x, row(k), LECTERN);
          for (int k = 0; k < 8; ++k) lay(rnd.range(a, b), row(rnd.range(1, 5)), SCROLL);
          lay(rnd.range(a, b), row(rnd.range(1, 5)), SKULL);
        } else {
          // the sun temple: a golden image of the Pyreheart on the far wall, sagged where it melted, its gold run down
          // over the steps; the dead kneeling in rows before it
          for (int x = mx - 2; x <= mx + 2; ++x) { m.put(x, row(0), SUNDISC); if (std::abs(x - mx) <= 1) m.put(x, row(1), SUNDISC); }
          m.put(mx, row(0), SUNHEART);
          for (int x = mx - 3; x <= mx + 3; ++x) m.put(x, row(2), GOLDCRUST);
          for (int x : {a + 2, b - 2}) for (int k : {1, 4}) m.put(x, row(k), PILLAR);
          for (int k : {4, 5}) for (int x = mx - 6; x <= mx + 6; x += 3) if (x != mx) lay(x + rnd.range(0, 1), row(k), rnd.real() < .7 ? DEADHEAP : BONES);
        }
      }
      // the houses, as their people left them in the night
      for (auto& ho : houses) {
        int a = ho.x0 + 1, b = ho.x1 - 1, t = ho.y0 + 1, d = ho.y1 - 1;
        int back = ho.door_y == ho.y1 ? t : d;
        bool left = rnd.coin();
        int bx = left ? a : b - 1;
        lay(bx, back, BUNK); lay(bx + 1, back, BUNK);
        int tx = left ? b - 2 : a + 2, ty = (t + d) / 2;
        if (lay(tx, ty, TABLE)) { lay(tx + (left ? 1 : -1), ty, STOOL); if (rnd.coin()) lay(tx, ty + 1, TANKARD); }
        if (rnd.real() < .4) lay(left ? b : a, back, rnd.coin() ? BOOKS : CRATE);
        int n = rnd.range(0, 3);
        for (int k = 0; k < n; ++k) lay(rnd.range(a, b), rnd.range(t, d), k == 0 ? SKULL : BONES);
        if (rnd.real() < .4) lay(rnd.range(a, b), rnd.range(t, d), BUCKET);
        if (rnd.real() < .5) lay(ho.door_x + (rnd.coin() ? 2 : -2), ho.door_y + (ho.door_y == ho.y1 ? 1 : -1), SIGN);
      }
      // the streets: grates of the air shafts, lantern posts, drifts of ash, the dead where they fell
      for (int k = 0, made = 0; k < 400 && made < 6; ++k) {
        int x = rnd.range(4, w - 8), y = rnd.range(4, h - 6);
        if (in_palace(x, y, x + 2, y + 1) || near_stairs(x, y, 4)) continue;
        bool ok = true;
        for (int yy = y - 1; yy <= y + 2 && ok; ++yy) for (int xx = x - 1; xx <= x + 3; ++xx) if (m.at(xx, yy) != FLOOR) { ok = false; break; }
        if (!ok) continue;
        m.rect(x, y, x + 2, y + 1, GRATE);
        ++made;
      }
      for (int k = 0, made = 0; k < 400 && made < 16; ++k) {
        int x = rnd.range(2, w - 3), y = rnd.range(2, h - 3);
        if (in_palace(x, y, x, y)) continue;
        bool street = true;
        for (int yy = y - 1; yy <= y + 1; ++yy) for (int xx = x - 1; xx <= x + 1; ++xx) if (m.at(xx, yy) != FLOOR && m.at(xx, yy) != WALL) street = false;
        if (street && lay(x, y, POST)) ++made;
      }
      for (int k = 0; k < 34; ++k) {
        int cx = rnd.range(3, w - 4), cy = rnd.range(3, h - 4);
        double r = 1 + rnd.real() * 1.8;
        for (int y = cy - 2; y <= cy + 2; ++y)
          for (int x = cx - 3; x <= cx + 3; ++x)
            if ((x - cx) * (x - cx) / 2.0 + (y - cy) * (y - cy) <= r * r && (m.at(x, y) == FLOOR || m.at(x, y) == PLAZA || m.at(x, y) == MARBLE) && !near_stairs(x, y, 0))
              m.put(x, y, DRIFT);
      }
      for (int k = 0; k < 26; ++k) lay(rnd.range(2, w - 3), rnd.range(2, h - 3), k % 4 == 0 ? SKULL : k % 4 == 1 ? RUBBLE : BONES);
      m.treasure.push_back(m.rooms[rnd.below(int(m.rooms.size()))]);

    } else if (act == VAULTS) {
      // ---------------------------------------------------------------- the Vaults: mirrored, locked, barred
      int cx = w / 2, cy = h / 2;
      // the ring, the spine across and down, and the great hall in the middle
      for (int o = 0; o < 3; ++o) {
        m.rect(3, 3 + o, w - 4, 3 + o, FLOOR); m.rect(3, h - 4 - o, w - 4, h - 4 - o, FLOOR);
        m.rect(3 + o, 3, 3 + o, h - 4, FLOOR); m.rect(w - 4 - o, 3, w - 4 - o, h - 4, FLOOR);
      }
      m.rect(cx - 1, 3, cx + 1, h - 4, FLOOR);
      m.rect(3, cy - 1, w - 4, cy + 1, FLOOR);
      m.room(cx - 15, cy - 7, cx + 15, cy + 7);
      // the treasury in the heart of the hall, behind a vault door
      m.rect(cx - 5, cy - 4, cx + 5, cy + 2, WALL);
      m.rect(cx - 4, cy - 3, cx + 4, cy + 1, FLOOR);
      m.put(cx, cy + 2, VAULTDOOR);
      std::vector<P> treasury = m.cells(cx - 4, cy - 3, cx + 4, cy + 1);
      for (auto t2 : treasury) if (rnd.real() < .3) m.put(t2.x, t2.y, COIN);
      m.put(cx - 3, cy - 3, CHEST); m.put(cx + 3, cy - 3, CHEST);
      for (int x = cx - 12; x <= cx + 12; x += 4) if (std::abs(x - cx) > 6) for (int y : {cy - 5, cy + 5}) m.put(x, y, PILLAR);
      // in each quarter a gallery, with vaults above and below it; the right half mirrors the left
      std::vector<P> vault_doors;
      struct VRoom { int kind, a, b, y0, y1; P door; };
      std::vector<VRoom> vrooms;
      for (int q = 0; q < 2; ++q) {
        int gy = q == 0 ? (3 + 3 + cy - 1) / 2 : (cy + 2 + h - 4) / 2;  // the gallery's row
        int band0 = q == 0 ? 6 : cy + 2, band1 = q == 0 ? cy - 2 : h - 5;
        for (int x = 6; x <= cx - 2; ++x) for (int o = 0; o < 2; ++o) { m.put(x, gy + o, FLOOR); m.put(w - 1 - x, gy + o, FLOOR); }
        for (int x0 = 7; x0 + 8 <= cx - 17; x0 += 10) {
          for (int side = 0; side < 2; ++side) {
            int y0 = side == 0 ? band0 + 1 : gy + 3, y1 = side == 0 ? gy - 2 : band1 - 1;
            if (y1 - y0 < 3) continue;
            int kind = rnd.below(4);  // 0 a vault, 1 an archive, 2 behind bars, 3 a plain store
            for (int mirror = 0; mirror < 2; ++mirror) {
              auto X = [&](int x) { return mirror ? w - 1 - x : x; };
              int a = std::min(X(x0), X(x0 + 8)), b = std::max(X(x0), X(x0 + 8));
              m.room(a, y0, b, y1);
              int doorx = X(x0 + 4), doory = side == 0 ? y1 + 1 : y0 - 1;
              if (kind == 2) for (int x = a; x <= b; ++x) m.put(x, doory, BARS);
              m.put(doorx, doory, kind == 0 ? VAULTDOOR : DOOR);
              if (kind == 0) vault_doors.push_back({doorx, doory});
              if (kind == 1) for (int y = y0 + 1; y <= y1 - 1; y += 2) for (int x = a + 1; x <= b - 1; ++x) if (x != (a + b) / 2) m.put(x, y, BOOKS);
              if (kind == 0) { for (int k = 0; k < 5; ++k) m.put(rnd.range(a, b), rnd.range(y0, y1), COIN); m.treasure.push_back(m.rooms.back()); }
              vrooms.push_back({kind, a, b, y0, y1, {doorx, doory}});
            }
          }
        }
      }
      for (auto vd : vault_doors) fl.writings.erase(vd.y * W + vd.x);
      up = {4, h - 5};
      down = {cx, cy + 5};
      fl.up = up; fl.down = down;
      // ---- the dressing: what the night of ash left in the treasuries and archives
      auto near_stairs = [&](int x, int y, int r) { return cheb(x, y, up.x, up.y) <= r || cheb(x, y, down.x, down.y) <= r; };
      auto lay = [&](int x, int y, int t) {
        if (!m.in(x, y) || near_stairs(x, y, 1) || m.at(x, y) != FLOOR) return false;
        if (!blocks(t)) { m.put(x, y, t); return true; }
        return m.place(x, y, t);
      };
      auto puddle = [&](int x, int y) {  // gold that ran and set, a hand's bones in it now and then
        for (int k = 0; k < 4; ++k) lay(x + rnd.range(-1, 1), y + rnd.range(0, 1) * (k % 2), GOLDCRUST);
        lay(x, y, GOLDCRUST);
        if (rnd.real() < .5) { if (m.at(x, y) == GOLDCRUST) m.put(x, y, GOLDHAND); }
      };
      auto runes = [&](P dr) {  // runes of binding cut round a vault's door
        for (int dx : {-2, -1, 1, 2}) if (m.at(dr.x + dx, dr.y) == WALL) m.put(dr.x + dx, dr.y, BINDRUNE);
      };
      std::vector<int> bag;
      for (auto& r : vrooms) {
        int a = r.a, b = r.b, y0 = r.y0, y1 = r.y1;
        auto rx = [&]() { return rnd.range(a, b); };
        auto ry = [&]() { return rnd.range(y0, y1); };
        int kind = r.kind;
        if (kind == 3) {  // a plain store: the regalia, a counting house, the gem store or a coin hall
          if (bag.empty()) bag = {0, 1, 2, 3};
          int k = rnd.below(int(bag.size()));
          kind = 10 + bag[k];
          bag.erase(bag.begin() + k);
        }
        if (kind == 0) {
          runes(r.door);
          for (int k = 0; k < 3; ++k) lay(rx(), ry(), COINHEAP);
          puddle(rx(), ry());
          lay(rx(), ry(), CHESTWRECK);
        } else if (kind == 1) {
          for (int y = y0; y <= y1; ++y) for (int x = a; x <= b; ++x) if (m.at(x, y) == BOOKS && rnd.real() < .25) m.put(x, y, FALLSHELF);
          for (int k = 0; k < 7; ++k) lay(rx(), ry(), SCROLLDRIFT);
          for (int k = 0; k < 3; ++k) lay(rx(), ry(), SCROLL);
        } else if (kind == 2) {
          int by = r.door.y;
          for (int x = a; x <= b; ++x) if (m.at(x, by) == BARS && rnd.real() < .2) m.put(x, by, MELTBAR);
          for (int k = 0; k < 3; ++k) lay(rx(), ry(), CHESTWRECK);
          for (int k = 0; k < 2; ++k) lay(rx(), ry(), COIN);
          lay(rx(), ry(), rnd.coin() ? BONES : SKULL);
        } else if (kind == 10) {
          // the regalia: crowns on pedestals, the treaties, a burned velvet tapestry on the far wall with a warning
          int far = r.door.y > y1 ? y0 - 1 : y1 + 1, fy = r.door.y > y1 ? y0 : y1;
          for (int x = a; x <= b; ++x) if (m.at(x, far) == WALL && rnd.real() < .6) m.put(x, far, TAPESTRY);
          for (int x = a + 1; x <= b - 1; x += 3) lay(x, fy, CROWN);
          for (int k = 0; k < 3; ++k) lay(rx(), ry(), TABLET);
          lay(rx(), ry(), NOTICE);
        } else if (kind == 11) {
          // a counting house: desks in rows, ledgers dropped, brass scales
          for (int y = y0 + 1; y <= y1 - 1; y += 2) for (int x = a + 1; x <= b - 1; x += 2) if (x != (a + b) / 2) lay(x, y, LECTERN);
          for (int k = 0; k < 4; ++k) lay(rx(), ry(), LEDGER);
          lay(rx(), ry(), SCALES);
          for (int k = 0; k < 3; ++k) lay(rx(), ry(), COIN);
        } else if (kind == 12) {
          // the gem store: smashed glass cases, cut stones in the dust
          for (int y = y0 + 1; y <= y1 - 1; y += 2) for (int x = a + 1; x <= b - 1; x += 2) if (x != (a + b) / 2) lay(x, y, CASE);
          for (int k = 0; k < 7; ++k) lay(rx(), ry(), GEM);
        } else {
          // a coin hall: heaps of tarnished coins, gold run together with the bones of the hands that held it
          for (int k = 0; k < 4; ++k) lay(rx(), ry(), COINHEAP);
          for (int k = 0; k < 2; ++k) puddle(rx(), ry());
          for (int k = 0; k < 4; ++k) lay(rx(), ry(), COIN);
        }
      }
      // the inner treasury's gate: runes either side, the inscription over it; heaps inside, gold run out under the door
      runes({cx, cy + 2});
      for (int x : {cx - 4, cx + 4}) if (m.at(x, cy + 2) == WALL) m.put(x, cy + 2, INSCRIPTION);
      lay(cx - 4, cy + 1, COINHEAP); lay(cx + 4, cy + 1, COINHEAP);
      puddle(cx - 2, cy - 1);
      // the great hall: gold puddles round the pillars, coins welded to bone
      for (int k = 0; k < 4; ++k) puddle(cx + rnd.range(-13, 13), cy + (rnd.coin() ? rnd.range(-6, -4) : rnd.range(4, 6)));
      // on the Hoard-Warden's map: the mound of fused gold and silver he lies under, stacks of ingots
      if (boss_floor) {
        for (int y = cy + 3; y <= cy + 7; ++y)
          for (int x = cx - 9; x <= cx + 9; ++x) {
            double ex = (x - cx) / 8.5, ey = (y - (cy + 5)) / 2.6, e = ex * ex + ey * ey;
            if (e <= 1 && e > .25 && rnd.real() < .85) lay(x, y, MOUND);
          }
        for (int k = 0; k < 4; ++k) lay(cx + (k % 2 ? 1 : -1) * rnd.range(11, 14), cy + rnd.range(-6, 6), INGOTS);
      }
      // the ways between: coins, gold run together, the dead, a chest dropped and burst
      for (int k = 0; k < 16; ++k) lay(rnd.range(3, w - 4), rnd.range(3, h - 4), COIN);
      for (int k = 0; k < 6; ++k) puddle(rnd.range(3, w - 4), rnd.range(3, h - 4));
      for (int k = 0; k < 12; ++k) lay(rnd.range(3, w - 4), rnd.range(3, h - 4), k % 3 ? BONES : SKULL);
      for (int k = 0; k < 5; ++k) lay(rnd.range(3, w - 4), rnd.range(3, h - 4), CHESTWRECK);

    } else if (act == BARRACKS) {
      // ---------------------------------------------------------------- the Barracks: a fortress round the drill yard
      int cx = w / 2, cy = h / 2;
      // the patrol way round inside the outer wall
      for (int o = 0; o < 3; ++o) {
        m.rect(2, 2 + o, w - 3, 2 + o, FLOOR); m.rect(2, h - 3 - o, w - 3, h - 3 - o, FLOOR);
        m.rect(2 + o, 2, 2 + o, h - 3, FLOOR); m.rect(w - 3 - o, 2, w - 3 - o, h - 3, FLOOR);
      }
      // the yard, scorched, with dummies in a grid and a colonnade round it
      int yx0 = cx - 20, yx1 = cx + 20, yy0 = cy - 10, yy1 = cy + 10;
      m.room(yx0, yy0, yx1, yy1);
      for (int x = yx0 + 2; x <= cx; x += 4) for (int y : {yy0 + 1, yy1 - 1}) { m.put(x, y, PILLAR); m.put(2 * cx - x, y, PILLAR); }
      for (int y = yy0 + 3; y <= yy1 - 3; y += 4) for (int x : {yx0 + 1, yx1 - 1}) m.put(x, y, PILLAR);
      for (int x = cx - 12; x <= cx - 4; x += 4) for (int y = cy - 4; y <= cy + 4; y += 4) { m.put(x, y, DUMMY); m.put(2 * cx - x, y, DUMMY); }
      // the halls: three above the yard, three below, mirrored; each with its door to the patrol way and to the yard.
      // Each has a small side room walled off at one end. The halls and the side rooms are dressed as the garrison
      // left them: sleeping halls, the mess, the armoury, the banner hall, the archery range; the captain's room,
      // the infirmary, the kennels, the drum room and the stores.
      enum { SLEEP, MESS, ARMOURY, BANNERS, RANGE, CAPTAIN, INFIRMARY, KENNEL, DRUMS, STORES };
      std::vector<int> big = {SLEEP, MESS, ARMOURY, BANNERS, RANGE, SLEEP}, small = {CAPTAIN, INFIRMARY, KENNEL, DRUMS, STORES, KENNEL};
      rnd.shuffle(big); rnd.shuffle(small);
      auto lay = [&](int x, int y, int t) {
        if (!m.in(x, y) || m.at(x, y) != FLOOR || cheb(x, y, 5, cy) <= 1) return false;
        if (!blocks(t)) { m.put(x, y, t); return true; }
        return m.place(x, y, t);
      };
      auto on_wall = [&](int x, int y, int t) { if (m.at(x, y) == WALL) m.put(x, y, t); };
      auto dress = [&](int kind, int x0, int y0, int x1, int y1, int mx, bool top) {
        auto rx = [&]() { return rnd.range(x0, x1); };
        auto ry = [&]() { return rnd.range(y0, y1); };
        int fy = top ? y0 : y1, ny = top ? y1 : y0;  // the far wall's row (away from the yard) and the near one
        switch (kind) {
          case SLEEP:
            // rows of bunks, a soldier's chest at the foot of each, some still with their sleeper's bones
            // along the long walls only, two deep, so the middle of the hall stays open to fight in
            for (int y : {y0, y0 + 2, y1 - 2, y1})
              for (int x = x0 + 1; x <= x1 - 1; x += 3) {
                if (std::abs(x - mx) <= 1) continue;
                m.put(x, y, BUNK); m.put(x + 1 <= x1 - 1 && std::abs(x + 1 - mx) > 1 ? x + 1 : x, y, BUNK);
                if (y == y0 + 2 || y == y1 - 2) lay(x, y + (y == y0 + 2 ? 1 : -1), CRATE);
              }
            for (int k = 0; k < 4; ++k) lay(rx(), ry(), k % 2 ? BONES : SKULL);
            for (int k = 0; k < 2; ++k) lay(rx(), ry(), TANKARD);
            break;
          case MESS:
            // long tables with benches at the ends, stools knocked over, tankards, kegs, a cold hearth on the far wall
            // two long tables, near the walls, so the middle stays open
            for (int y : {y0 + 2, y1 - 2}) {
              for (int x = x0 + 3; x <= x1 - 3; ++x) if (std::abs(x - mx) > 1) m.put(x, y, TABLE);
              for (int x : {x0 + 2, x1 - 2}) m.put(x, y, BENCH);
            }
            for (int k = 0; k < 6; ++k) lay(rx(), ry(), STOOL);
            for (int k = 0; k < 6; ++k) lay(rx(), ry(), TANKARD);
            for (int x : {x0, x1}) lay(x, fy, KEG);
            for (int dx = -1; dx <= 1; ++dx) { int hx = mx + (mx > (x0 + x1) / 2 ? -5 : 5) + dx; lay(hx, fy, EMBERS); }
            break;
          case ARMOURY:
            // the racks along the walls, many overturned; spears, split shields and axes all over, heaps of shields
            for (int x = x0 + 1; x <= x1 - 1; x += 2) if (std::abs(x - mx) > 1) { lay(x, y0, RACK); lay(x, y1, RACK); }
            for (int k = 0; k < 4; ++k) { int x = rx(), y = ry(); lay(x, y, SHIELDHEAP); lay(x + 1, y, SHIELDHEAP); }
            for (int k = 0; k < 9; ++k) lay(rx(), ry(), SPEAR);
            for (int k = 0; k < 5; ++k) lay(rx(), ry(), SHIELD);
            for (int k = 0; k < 2; ++k) lay(rx(), ry(), WRECK);
            for (int k = 0; k < 2; ++k) lay(rx(), ry(), BOLTS);
            break;
          case BANNERS:
            // two rows of pillars, scorched banners hanging in tatters along the walls, the Iron-Vanguard's in the middle
            for (int x = x0 + 3; x <= x1 - 3; x += 4) if (std::abs(x - mx) > 2) { m.put(x, y0 + 3, PILLAR); m.put(x, y1 - 3, PILLAR); }
            for (int x = x0 + 1; x <= x1 - 1; x += 3) if (std::abs(x - mx) > 1) { on_wall(x, y0 - 1, HANGING); on_wall(x, y1 + 1, HANGING); }
            lay(mx + 3, (y0 + y1) / 2, BANNER);
            lay(rx(), ry(), SKULL);
            break;
          case RANGE:
            // targets along the far wall with bolts in them, a shooting line of benches, crates of bolts behind it
            for (int x = x0 + 2; x <= x1 - 2; x += 5) if (std::abs(x - mx) > 1) lay(x, fy, TARGET);
            for (int x = x0 + 1; x <= x1 - 1; ++x) if (std::abs(x - mx) > 2 && (x - x0) % 7 != 3) lay(x, ny + (top ? -2 : 2), BENCH);
            for (int k = 0; k < 3; ++k) lay(rx(), ny, BOLTS);
            for (int k = 0; k < 2; ++k) lay(rx(), ry(), BONES);
            break;
          case CAPTAIN:
            // the map of the passes on its table, armour welded on its stands, a bed, a desk with candles and the log
            for (int x = x0 + 2; x <= x0 + 5; ++x) for (int y : {(y0 + y1) / 2, (y0 + y1) / 2 + 1}) lay(x, y, x == x0 + 2 || x == x0 + 5 ? TABLE : PASSMAP);
            lay(x0, fy, BUNK); lay(x0 + 1, fy, BUNK);
            for (int x = x0 + 4; x <= x1; x += 2) lay(x, fy, ARMOUR);
            lay(x1, ny, TABLE); lay(x1 - 1, ny, CANDLE); lay(x1 - 2, ny, LEDGER);
            lay(x0, ny, RACK);
            lay(rx(), ry(), CRATE);
            m.treasure.push_back(m.cells(x0, y0, x1, y1));
            break;
          case INFIRMARY:
            for (int y = y0; y <= y1; y += 2) { lay(x0, y, BUNK); if (rnd.real() < .3) lay(x0 + 1, y, BONES); }
            for (int k = 0; k < 4; ++k) lay(rx(), ry(), BANDAGE);
            lay(rx(), ry(), BUCKET);
            lay(x1, ny, TABLE); lay(x1, ny + (top ? -1 : 1), CANDLE); lay(x1 - 1, ny, LEDGER);
            break;
          case KENNEL:
            // pens of bars along the far wall, a gap into each; chains, gnawed bones, a trough of black water
            for (int x = x0; x <= x1; ++x) if ((x - x0) % 4 != 1) lay(x, fy + (top ? 2 : -2), BARS);
            for (int k = 0; k < 2; ++k) lay(rx(), ry(), CHAIN);
            for (int k = 0; k < 4; ++k) lay(rx(), ry(), k % 2 ? BONES : MIDDEN);
            for (int x = x0 + 2; x <= x0 + 4; ++x) lay(x, ny, QUENCH);
            break;
          case DRUMS:
            // the war-drums, their skins split by the heat, and the bones of the drummer
            for (int k = 0; k < 3; ++k) { int x = rx(), y = ry(); lay(x, y, DRUM); if (k == 0) { lay(x + 1, y, DRUM); lay(x, y + 1, DRUM); lay(x + 1, y + 1, DRUM); } }
            lay(rx(), ry(), SKULL);
            break;
          default:
            for (int k = 0; k < 8; ++k) lay(rx(), ry(), CRATE);
            for (int k = 0; k < 2; ++k) lay(rx(), ry(), KEG);
            break;
        }
      };
      // the ways along the yard (first, so the furniture laid in the halls can tell what it would cut off), between it and the halls
      for (int o = 0; o < 2; ++o) { m.rect(5, yy0 - 2 + o, w - 6, yy0 - 2 + o, FLOOR); m.rect(5, yy1 + 1 + o, w - 6, yy1 + 1 + o, FLOOR); }
      // ways from the patrol round to the yard's sides
      m.way({4, cy}, {yx0, cy}, 3);
      m.way({w - 5, cy}, {yx1, cy}, 3);
      up = {5, cy};
      fl.up = up;  // whole() walks from here while the halls are furnished
      int hn = 0;
      auto hall = [&](int x0, int y0, int x1, int y1, bool top, bool side_left) {
        m.room(x0, y0, x1, y1);
        int mx = (x0 + x1) / 2;
        m.put(mx, top ? y0 - 1 : y1 + 1, DOOR);  // to the patrol way
        m.put(mx, top ? y1 + 1 : y0 - 1, DOOR);  // to the yard side
        // the side room, ten wide, behind a wall with a door
        int xs = side_left ? x0 + 10 : x1 - 10;
        for (int y = y0; y <= y1; ++y) m.put(xs, y, WALL);
        m.put(xs, (y0 + y1) / 2, DOOR);
        int a0 = side_left ? xs + 1 : x0, a1 = side_left ? x1 : xs - 1;  // the hall itself
        int b0 = side_left ? x0 : xs + 1, b1 = side_left ? xs - 1 : x1;  // the side room
        m.rooms.push_back(m.cells(b0, y0, b1, y1));
        dress(big[hn], a0, y0, a1, y1, mx, top);
        if (big[hn] == ARMOURY) m.treasure.push_back(m.cells(a0, y0, a1, y1));
        dress(small[hn], b0, y0, b1, y1, -9, top);
        ++hn;
      };
      int hw = (w - 12) / 3;
      for (int i = 0; i < 3; ++i) {
        int x0 = 6 + i * hw, x1 = x0 + hw - 3;
        hall(x0, 6, x1, yy0 - 4, true, i == 0 || (i == 1 && rnd.coin()));
        hall(x0, yy1 + 4, x1, h - 7, false, i == 0 || (i == 1 && rnd.coin()));
      }
      // the gate at the foot of the west side: a portcullis half raised
      m.put(1, cy - 1, BARS); m.put(1, cy + 1, BARS);
      up = {5, cy};
      fl.up = up;
      down = {cx, yy0 + 3};
      m.treasure.push_back(m.rooms[0]);
      // the yard: drifts of soot, spears and shields where the ranks broke, the dead; banners round its walls
      for (int k = 0; k < 10; ++k) {
        int x = rnd.range(yx0 + 2, yx1 - 2), y = rnd.range(yy0 + 2, yy1 - 2);
        for (int j = 0; j < 5; ++j) lay(x + rnd.range(-2, 2), y + rnd.range(-1, 1), DRIFT);
      }
      for (int k = 0; k < 6; ++k) lay(rnd.range(yx0 + 1, yx1 - 1), rnd.range(yy0 + 1, yy1 - 1), SPEAR);
      for (int k = 0; k < 3; ++k) lay(rnd.range(yx0 + 1, yx1 - 1), rnd.range(yy0 + 1, yy1 - 1), SHIELD);
      for (int k = 0; k < 3; ++k) lay(rnd.range(yx0 + 1, yx1 - 1), rnd.range(yy0 + 1, yy1 - 1), BONES);
      for (int x = yx0 + 4; x <= yx1 - 4; x += 6) if (std::abs(x - cx) > 2) { on_wall(x, yy0 - 1, HANGING); on_wall(x, yy1 + 1, HANGING); }
      // on the Iron-Captain's map: his stand at the head of the yard, where the ranks muster before him
      if (boss_floor) for (int x = cx - 3; x <= cx + 3; ++x) if (m.at(x, yy0 + 1) == FLOOR) m.put(x, yy0 + 1, DAIS);
      // the patrol way: a spear or a shield dropped, the dead
      for (int k = 0; k < 8; ++k) lay(rnd.range(2, w - 3), rnd.range(2, h - 3), k % 3 ? SPEAR : BONES);

    } else if (act == PRISON) {
      // ---------------------------------------------------------------- the Prison: a star of cell blocks round the pit
      P c{w / 2, h / 2};
      m.disc(c, 8.5, FLOOR);
      m.rooms.push_back(m.cells(c.x - 6, c.y - 6, c.x + 6, c.y + 6));
      // the ring gallery round the pit hall, and the spokes into it
      for (int y = c.y - 14; y <= c.y + 14; ++y)
        for (int x = c.x - 20; x <= c.x + 20; ++x) {
          double dx = (x - c.x) / 1.4, dy = y - c.y, r = std::sqrt(dx * dx + dy * dy);
          if (r >= 11.5 && r <= 13.6) m.put(x, y, FLOOR);
        }
      for (int k = 0; k < 6; ++k) {
        double a = k * 3.14159265 / 3;
        for (double r = 7; r <= 12.5; r += .5) {
          int x = c.x + int(std::lround(std::cos(a) * r * 1.4)), y = c.y + int(std::lround(std::sin(a) * r));
          m.rect(x - 1, y - 1, x + 1, y + 1, FLOOR);
        }
      }
      // the wardens' post at the heart of the gaol: a ring of pillars, the watch fire in the middle, the open floor
      // round it to fight on
      for (int k = 0; k < 8; ++k) {
        double a = k * 3.14159265 / 4 + .39;
        m.put(c.x + int(std::lround(std::cos(a) * 5.6)), c.y + int(std::lround(std::sin(a) * 4.6)), PILLAR);
      }
      m.put(c.x, c.y, BRAZIER);
      // the wings: west and east long, north and south short, each a corridor with barred cells on both sides
      struct Wing { int dx, dy, len; };
      std::vector<Wing> wings = {{-1, 0, c.x - 21 - 10}, {1, 0, w - 1 - (c.x + 21) - 10}, {0, -1, c.y - 14 - 9}, {0, 1, h - 1 - (c.y + 14) - 9}};
      std::vector<P> guards;
      struct Box { int x0, y0, x1, y1; };
      std::vector<Box> ends;
      std::vector<std::vector<P>> cells_in;
      for (auto& wg : wings) {
        P s{c.x + wg.dx * (wg.dx ? 19 : 0), c.y + wg.dy * (wg.dy ? 13 : 0)};
        P e{s.x + wg.dx * wg.len, s.y + wg.dy * wg.len};
        m.way(s, e, 3);
        // cells along it, every four steps, three on a side
        for (int i = 3; i + 3 <= wg.len; i += 4) {
          for (int side : {-1, 1}) {
            P a{s.x + wg.dx * i, s.y + wg.dy * i};
            int px = wg.dy ? side : 0, py = wg.dx ? side : 0;   // across the wing
            P front{a.x + px * 2, a.y + py * 2};
            std::vector<P> inside;
            for (int k = 3; k <= 5; ++k) for (int l = -1; l <= 1; ++l) {
              P q{a.x + px * k + (wg.dx ? l : 0), a.y + py * k + (wg.dy ? l : 0)};
              if (!m.in(q.x, q.y)) continue;
              m.put(q.x, q.y, FLOOR);
              inside.push_back(q);
            }
            if (inside.size() < 9) { for (auto q : inside) m.put(q.x, q.y, WALL); continue; }
            for (int l = -1; l <= 1; ++l) m.put(front.x + (wg.dx ? l : 0), front.y + (wg.dy ? l : 0), BARS);
            bool broken = rnd.real() < .25;
            m.put(front.x, front.y, broken ? RUBBLE : DOOR);
            m.rooms.push_back(inside);
            cells_in.push_back(inside);
            if (broken) m.treasure.push_back(inside);
          }
        }
        // the guard room at the end
        P g{e.x + wg.dx * 5, e.y + wg.dy * 4};
        m.room(g.x - 5, g.y - 4, g.x + 5, g.y + 4);  // big enough to fight in
        ends.push_back({g.x - 5, g.y - 4, g.x + 5, g.y + 4});
        m.way(e, g, 3);
        guards.push_back(g);
      }
      // the ways between the guard rooms, round the outside
      // west to north and east to south go up or down first, the other two across first, so they keep to the outside
      P gw = guards[0], ge = guards[1], gn = guards[2], gs = guards[3];
      m.way(gw, {gw.x, gn.y}, 2); m.way({gw.x, gn.y}, gn, 2);
      m.way(gn, ge, 2);
      m.way(ge, {ge.x, gs.y}, 2); m.way({ge.x, gs.y}, gs, 2);
      m.way(gs, gw, 2);
      up = guards[0];
      down = {c.x, c.y - 3};
      fl.up = up; fl.down = down;
      // ---- the dressing: the dwarves' own gaol, left locked when the ash came
      auto lay = [&](int x, int y, int t) {
        if (!m.in(x, y) || m.at(x, y) != FLOOR || cheb(x, y, up.x, up.y) <= 1 || cheb(x, y, down.x, down.y) <= 1) return false;
        if (!blocks(t)) { m.put(x, y, t); return true; }
        return m.place(x, y, t);
      };
      auto on_wall = [&](int x, int y, int t) { if (m.at(x, y) == WALL) { m.put(x, y, t); return true; } return false; };
      // the wardens' post: a table, the racks, keys on the pillars' side
      lay(c.x - 2, c.y + 2, TABLE); lay(c.x + 2, c.y + 2, TABLE); lay(c.x - 6, c.y - 1, RACK); lay(c.x + 6, c.y - 1, RACK);
      // the cells: the prisoners nobody came to free, still in their irons; some cells an oubliette with a grate,
      // some flooded where the drains backed up
      for (auto& cl : cells_in) {
        P mid = cl[cl.size() / 2];
        double r = rnd.real();
        if (r < .45) { lay(mid.x, mid.y, rnd.coin() ? BONES : SKULL); lay(cl[rnd.below(int(cl.size()))].x, cl[rnd.below(int(cl.size()))].y, CHAIN); }
        else if (r < .6) m.put(mid.x, mid.y, GRATE);
        else if (r < .72) for (auto q : cl) if (rnd.real() < .6 && m.at(q.x, q.y) == FLOOR) m.put(q.x, q.y, SHALLOW);
      }
      // the end rooms: the judgement hall, the torture chamber, the gaoler's room, the stone-breaking yard, the
      // execution yard; furniture along the walls, the floor left open
      std::vector<int> themes = {0, 1, 2, 3, 4};
      rnd.shuffle(themes);
      for (size_t k = 0; k < ends.size(); ++k) {
        auto b = ends[k];
        int cxr = (b.x0 + b.x1) / 2;
        auto rx = [&]() { return rnd.range(b.x0, b.x1); };
        auto edge = [&]() {  // a cell along the room's walls
          int s2 = rnd.below(4);
          return s2 == 0 ? P{rx(), b.y0} : s2 == 1 ? P{rx(), b.y1} : s2 == 2 ? P{b.x0, rnd.range(b.y0, b.y1)} : P{b.x1, rnd.range(b.y0, b.y1)};
        };
        switch (themes[k]) {
          case 0:  // the judgement hall: the laws cut in the wall, the judge's seat behind his bench, benches, the dock
            for (int x = b.x0; x <= b.x1; x += 2) on_wall(x, b.y0 - 1, LAWS);
            lay(cxr, b.y0, JUDGESEAT); lay(cxr - 1, b.y0 + 1, STONEWORK); lay(cxr, b.y0 + 1, STONEWORK); lay(cxr + 1, b.y0 + 1, STONEWORK);
            for (int x = b.x0 + 1; x <= b.x0 + 3; ++x) { lay(x, b.y1 - 1, BENCH); lay(x + 6, b.y1 - 1, BENCH); }
            lay(b.x1, b.y0 + 3, BARS); lay(b.x1 - 1, b.y0 + 3, BARS); lay(b.x1, b.y0 + 5, BARS); lay(b.x1 - 1, b.y0 + 5, BARS);
            break;
          case 1:  // the torture chamber: the rack, iron maidens, hooks on chains, the brazier with its irons
            lay(b.x0 + 1, b.y0 + 1, TORTURERACK); lay(b.x0 + 2, b.y0 + 1, TORTURERACK); lay(b.x0 + 3, b.y0 + 1, TORTURERACK);
            lay(b.x1, b.y0, MAIDEN); lay(b.x1, b.y1, MAIDEN); lay(b.x0, b.y1, MAIDEN);
            lay(b.x1 - 2, b.y1, BRAZIER); lay(b.x1 - 3, b.y1, TONGS);
            for (int j = 0; j < 4; ++j) { P e2 = edge(); lay(e2.x, e2.y, HOOK); }
            for (int j = 0; j < 3; ++j) lay(rx(), rnd.range(b.y0, b.y1), STAIN);
            break;
          case 2:  // the gaoler's room: keys on their hooks, his table and the tally-book, a candle, a rack, a bunk
            for (int x = b.x0 + 1; x <= b.x0 + 5; x += 2) on_wall(x, b.y0 - 1, KEYS);
            lay(b.x1, b.y0, TABLE); lay(b.x1 - 1, b.y0, TABLE); lay(b.x1, b.y0 + 1, CANDLE); lay(b.x1 - 1, b.y0 + 1, LEDGER);
            lay(b.x0, b.y1, BUNK); lay(b.x0 + 1, b.y1, BUNK); lay(b.x1, b.y1, RACK);
            break;
          case 3:  // the stone-breaking yard: heaps of broken stone along the walls, hammers, chains to rings, a cart
            for (int j = 0; j < 7; ++j) { P e2 = edge(); lay(e2.x, e2.y, HEAP); }
            for (int j = 0; j < 4; ++j) lay(rx(), rnd.range(b.y0, b.y1), CHAIN);
            for (int j = 0; j < 3; ++j) lay(rx(), rnd.range(b.y0, b.y1), HAMMER);
            lay(rx(), b.y1, BARROW);
            lay(rx(), rnd.range(b.y0, b.y1), BONES);
            break;
          default:  // the execution yard: the gallows along the far wall, the headsman's block, an old stain
            for (int x = b.x0 + 1; x <= b.x0 + 4; ++x) lay(x, b.y0, GALLOWS);
            for (int x = b.x1 - 4; x <= b.x1 - 1; ++x) lay(x, b.y0, GALLOWS);
            lay(cxr, (b.y0 + b.y1) / 2 + 1, BLOCK); lay(cxr - 1, (b.y0 + b.y1) / 2 + 2, STAIN); lay(cxr + 1, (b.y0 + b.y1) / 2 + 2, STAIN);
            lay(rx(), b.y1, SKULL);
            break;
        }
      }
      // the corridors and the ring: chains slung overhead now and then, an old stain, a dropped chain
      for (int k = 0; k < 14; ++k) lay(rnd.range(3, w - 4), rnd.range(3, h - 4), k % 3 ? SLUNG : CHAIN);
      for (int k = 0; k < 6; ++k) lay(rnd.range(3, w - 4), rnd.range(3, h - 4), k % 2 ? STAIN : BONES);

    } else if (act == SANCTUM) {
      // ---------------------------------------------------------------- the Sanctum: nave, chapels, the empty altar
      int cx = w / 2;
      int top = 4, bottom = h - 4;
      // the nave, with two rows of pillars
      m.room(cx - 12, 16, cx + 12, bottom - 4);
      for (int y = 19; y <= bottom - 7; y += 4) { m.put(cx - 7, y, PILLAR); m.put(cx + 7, y, PILLAR); }
      // the porch and the way in
      m.room(cx - 5, bottom - 3, cx + 5, bottom);
      // the raised platform, the empty altar, melted gold, and the hole dug behind it
      m.rect(cx - 14, top, cx + 14, 15, FLOOR);
      m.rooms.push_back(m.cells(cx - 14, top, cx + 14, 15));
      m.rect(cx - 10, top + 3, cx + 10, 13, DAIS);
      m.put(cx, top + 6, ALTAR);
      for (int k = 0; k < 12; ++k) { int x = cx + rnd.range(-6, 6), y = top + rnd.range(4, 9); if (m.at(x, y) == DAIS) m.put(x, y, COIN); }
      m.rect(cx - 3, top, cx + 3, top + 2, CHASM);
      for (int x : {cx - 9, cx + 9}) for (int y = top + 4; y <= 12; y += 4) m.put(x, y, CANDLE);
      // the chapels down both sides of the nave, mirrored, each with an altar of its own (a shrine: not all are kind)
      for (int y0 = 18; y0 + 7 <= bottom - 5; y0 += 10)
        for (int side : {-1, 1}) {
          int x0 = side < 0 ? cx - 26 : cx + 15, x1 = x0 + 11;
          m.room(x0, y0, x1, y0 + 7);
          int door_x = side < 0 ? x1 + 1 : x0 - 1;
          for (int y = y0 + 3; y <= y0 + 4; ++y) for (int x = std::min(door_x, side < 0 ? cx - 13 : cx + 13); x <= std::max(door_x, side < 0 ? cx - 13 : cx + 13); ++x) m.put(x, y, FLOOR);
          int ax = side < 0 ? x0 + 1 : x1 - 1;
          m.put(ax, y0 + 2, CANDLE); m.put(ax, y0 + 5, CANDLE);
        }
      // the side halls off the platform: a sacristy and a scriptorium, and the aisles down to the porch
      for (int side : {-1, 1}) {
        int x0 = side < 0 ? 3 : w - 22, x1 = x0 + 18;
        m.room(x0, top, x1, 13);
        int hx = side < 0 ? x1 : x0;
        m.way({hx, 9}, {side < 0 ? cx - 14 : cx + 14, 9}, 3);
        if (side > 0) {
          for (int y = top + 1; y <= 12; y += 2)
            for (int x = x0 + 2; x <= x1 - 2; ++x) if (std::abs(x - (x0 + x1) / 2) > 1) m.put(x, y, BOOKS);
        } else m.treasure.push_back(m.rooms.back());
        int ax = side < 0 ? 6 : w - 7;
        m.way({ax, 13}, {ax, bottom - 1}, 3);
        m.way({ax, bottom - 1}, {side < 0 ? cx - 5 : cx + 5, bottom - 1}, 3);
      }
      up = {cx, bottom - 1};
      fl.up = up;
      down = {cx, top + 4};
      if (m.at(down.x, down.y) != DAIS && m.at(down.x, down.y) != FLOOR) down = {cx + 2, top + 4};
    }

    if (up.x < 0 || down.x < 0) continue;
    if (m.at(up.x, up.y) == WALL) m.put(up.x, up.y, FLOOR);
    if (blocks(m.at(down.x, down.y))) m.put(down.x, down.y, FLOOR);
    fl.up = up; fl.down = down;
    // nothing may stand on the stairs, and every open cell must be reachable
    if (!whole(fl)) { if (getenv("PYREHEART_GENDBG")) fprintf(stderr, "not whole\n"); continue; }
    auto d = m.walk(up);
    if (d[down.y * W + down.x] < 0) { if (getenv("PYREHEART_GENDBG")) fprintf(stderr, "no way down\n"); continue; }
    rooms = m.rooms;
    treasure = m.treasure;
    // the zone's own foes that guard something: the Vaults' keys, the Prison's sleepers in their cells
    if (act == VAULTS) {
      int doors = 0;
      for (int i = 0; i < W * H; ++i) if (fl.t[i] == VAULTDOOR) ++doors;
      fl.keys = 0;
      (void)doors;
    }
    (void)boss_floor;
    return true;
  }
  if (getenv("PYREHEART_GENDBG")) fprintf(stderr, "build_zone failed for act %d\n", act);
  return false;
}

// The foes a zone sets to guard something, after the rest are out: the Vaults' guardians by the locked doors (they
// carry the keys) and mimics among the coins, the Prison's prisoners asleep in their cells, the Barracks' squads
// (a shield in front, bows behind), the Sanctum's knights kneeling by the empty altar.
void Game::zone_foes(Floor& fl) {
  int act = fl.act;
  auto open = [&](int x, int y) { return fl.tile(x, y) == FLOOR && !mon_at(x, y) && P{x, y} != fl.up && P{x, y} != fl.down; };
  if (act == 1) {
    for (int i = 0; i < W * H; ++i) {
      P c{i % W, i / W};
      if (fl.t[i] == BURROW && rnd.real() < .7) {
        P at;
        if (free_near(c.x, c.y, 2, at, [&](int x, int y) { return open(x, y) || fl.tile(x, y) == MIDDEN; })) spawn("giantrat", at.x, at.y);
      }
      if (fl.t[i] == CAMPFIRE)
        for (int k = 0; k < 2 + rnd.below(2); ++k) {
          P at;
          if (free_near(c.x, c.y, 3, at, [&](int x, int y) { return (open(x, y) || fl.tile(x, y) == HIDE) && cheb(x, y, c.x, c.y) >= 2; }))
            if (auto mm = spawn("trog", at.x, at.y)) mm->sleep = 1;
        }
    }
  }
  if (act == VAULTS) {
    std::vector<P> doors, coins;
    for (int i = 0; i < W * H; ++i) {
      if (fl.t[i] == VAULTDOOR) doors.push_back({i % W, i / W});
      if (fl.t[i] == COIN) coins.push_back({i % W, i / W});
    }
    for (auto d : doors) {
      P c;
      if (free_near(d.x, d.y, 2, c, [&](int x, int y) { return open(x, y); })) spawn("guardian", c.x, c.y);
    }
    rnd.shuffle(coins);
    for (size_t i = 0; i < coins.size() && i < 3; ++i) { fl.set(coins[i].x, coins[i].y, FLOOR); spawn("mimic", coins[i].x, coins[i].y); }
    for (size_t i = 3; i < coins.size() && i < 9; ++i) { fl.set(coins[i].x, coins[i].y, FLOOR); fl.items.push_back({coins[i].x, coins[i].y, nullptr, lvl * rnd.range(3, 7), "", 0}); }
  }
  if (act == PRISON) {
    // a cell is a room of nine behind bars; some hold a sleeper
    for (int i = 0; i < W * H; ++i) {
      if (fl.t[i] != BARS || rnd.real() > .12) continue;
      P b{i % W, i / W};
      for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -2; dx <= 2; ++dx) {
          if (std::abs(dx) + std::abs(dy) != 2 || (dx && dy)) continue;
          int x = b.x + dx, y = b.y + dy;
          if (!open(x, y)) continue;
          static const std::vector<std::string> who = {"prisoner", "prisoner", "ratswarm", "turnkey"};
          if (auto mm = spawn(rnd.pick(who), x, y)) { mm->sleep = 1; }
          dx = dy = 3;
        }
    }
  }
  if (act == BARRACKS) {
    // the war hounds, asleep in their kennels by the trough
    for (int i = 0; i < W * H; ++i) {
      if (fl.t[i] != QUENCH || fl.t[i - 1] == QUENCH || rnd.real() > .8) continue;
      for (int k = 0; k < 1 + rnd.below(2); ++k) {
        P at;
        if (free_near(i % W, i / W, 4, at, [&](int x, int y) { return open(x, y); }))
          if (auto mm = spawn("warhound", at.x, at.y)) mm->sleep = 1;
      }
    }
    // squads: a shield-bearer in front, two cross-bowmen behind it, a pikeman beside
    for (int k = 0; k < 4; ++k) {
      P c;
      int tries = 0;
      do { c = {rnd.range(5, fl.w - 6), rnd.range(5, fl.h - 6)}; } while (++tries < 200 && (!open(c.x, c.y) || cheb(c.x, c.y, fl.up.x, fl.up.y) < 14));
      if (tries >= 200) continue;
      spawn_near("shieldbearer", c);
      spawn_near("crossbow", {c.x, c.y - 2});
      spawn_near("crossbow", {c.x, c.y + 2});
      spawn_near("pikeman", {c.x + 1, c.y});
    }
  }
  if (act == SANCTUM) {
    for (int dx : {-6, 6}) if (open(fl.down.x + dx, fl.down.y + 2)) spawn("altarguard", fl.down.x + dx, fl.down.y + 2);
  }
}
