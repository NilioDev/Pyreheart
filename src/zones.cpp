#include <cstdio>
#include <cstdlib>
// A maker of maps for each zone that is not built of the Crypt's rooms (k5), the Cistern's or the Deep's crust:
// the Caves grow like a cave, the Forge is halls of cut stone with molten metal in channels, the City is a planned
// dwarven city round a plaza, the Vaults a mirrored warren of locked vaults, the Barracks a fortress round a drill yard,
// the Prison a star of cell blocks round the pit, the Sanctum a temple with a nave, chapels and the empty altar.
// Each returns the rooms (for treasure) and leaves fl.up and fl.down set; the way from one to the other always exists.
#include <deque>

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
      for (size_t i = 0; i < halls.size(); ++i) {
        auto [x0, y0, x1, y1] = halls[i];
        int cx = (x0 + x1) / 2;
        // two rows of pillars, mirrored, two in from the walls
        for (int x = x0 + 3; x <= cx - 2; x += 5) for (int y : {y0 + 2, y1 - 2}) { m.place(x, y, PILLAR); m.place(x0 + x1 - x, y, PILLAR); }
        if (int(i) == furnace || rnd.real() < .5) {
          // a channel of molten metal across the hall, with two stone crossings
          bool from_furnace = int(i) == furnace;
          int y = (y0 + y1) / 2 + 4;
          for (int x = x0; x <= x1; ++x) for (int o = 0; o < 2; ++o) m.put(x, y + o, LAVA);
          for (int x : {x0 + (x1 - x0) / 4, x1 - (x1 - x0) / 4}) for (int o = 0; o < 2; ++o) { m.put(x, y + o, FLOOR); m.put(x + 1, y + o, FLOOR); }
          if (from_furnace) {  // the furnace at the top of the hall, the channel running from it
            m.rect(cx - 2, y0, cx + 2, y0 + 1, HEARTH);
            for (int yy = y0 + 2; yy < y; ++yy) m.put(cx, yy, LAVA);
            down = {cx + 4, y0 + 3};
          }
          // anvils and slag on the banks
          for (int k = 0; k < 4; ++k) m.place(rnd.range(x0 + 2, x1 - 2), y + (rnd.coin() ? -2 : 3), rnd.coin() ? ANVIL : SLAG);
        }
        for (int k = 0; k < 6; ++k) { int x = rnd.range(x0 + 1, x1 - 1), y = rnd.range(y0 + 1, y1 - 1); if (m.at(x, y) == FLOOR) m.put(x, y, CHAIN); }
      }
      m.treasure.push_back(m.rooms[cols]);

    } else if (act == CITY) {
      // ---------------------------------------------------------------- the City: avenues, a plaza, houses in rows
      P c{w / 2, h / 2};
      // the cavern: everything open inside a rough oval
      for (int y = 1; y < h - 1; ++y)
        for (int x = 1; x < w - 1; ++x) {
          double dx = (x - c.x) / (w / 2.0 - 2), dy = (y - c.y) / (h / 2.0 - 2);
          if (dx * dx + dy * dy < .97 + .06 * rnd.real()) m.put(x, y, FLOOR);
        }
      // houses: blocks between the streets, each a box with its door on the street side
      for (int by = 4; by < h - 8; by += 9)
        for (int bx = 6; bx < w - 12; bx += 12) {
          int x0 = bx + rnd.range(0, 2), y0 = by + rnd.range(0, 1), x1 = x0 + rnd.range(7, 9), y1 = y0 + rnd.range(5, 6);
          if (std::abs((x0 + x1) / 2 - c.x) < 12 || std::abs((y0 + y1) / 2 - c.y) < 9) continue;  // the avenues
          bool all_open = true;
          for (int y = y0 - 1; y <= y1 + 1; ++y) for (int x = x0 - 1; x <= x1 + 1; ++x) if (m.at(x, y) != FLOOR) all_open = false;
          if (!all_open) continue;
          m.rect(x0, y0, x1, y1, WALL);
          m.rect(x0 + 1, y0 + 1, x1 - 1, y1 - 1, FLOOR);
          m.rooms.push_back(m.cells(x0 + 1, y0 + 1, x1 - 1, y1 - 1));
          int door_y = (y0 + y1) / 2 < c.y ? y1 : y0;
          m.put((x0 + x1) / 2, door_y, DOOR);
          if (rnd.real() < .25) {  // fallen in: part of it rubble and open to the street
            int cx0 = rnd.coin() ? x0 : x1 - 2;
            for (int y = y0; y <= y1; ++y) for (int x = cx0; x <= cx0 + 2; ++x) m.put(x, y, rnd.real() < .6 ? RUBBLE : FLOOR);
          }
        }
      // the avenues: a canal down the middle of each, with bridges; statues on plinths along them
      for (int x = 2; x < w - 2; ++x)
        if (std::abs(x - c.x) > 10 && m.at(x, c.y) == FLOOR && x % 10 != 0) m.put(x, c.y, WATER);
      for (int y = 2; y < h - 2; ++y)
        if (std::abs(y - c.y) > 10 && m.at(c.x, y) == FLOOR && y % 8 != 4) m.put(c.x, y, WATER);
      for (int x = 4; x < w - 4; x += 8)
        if (std::abs(x - c.x) > 12) for (int y : {c.y - 4, c.y + 4}) if (m.at(x, y) == FLOOR) m.put(x, y, STATUE);
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
      up = {c.x + 3, h - 5};
      {
        int bd = 1 << 30;
        P best{-1, -1};
        for (int y = c.y + 12; y < h - 1; ++y)
          for (int x = c.x - 12; x <= c.x + 12; ++x)
            if (m.at(x, y) == FLOOR && std::abs(x - c.x) > 1 && (h - y) * 4 + std::abs(x - c.x) < bd) { bd = (h - y) * 4 + std::abs(x - c.x); best = {x, y}; }
        if (best.x >= 0) up = best;
      }
      fl.up = up;
      // market stalls on the plaza's edge
      for (int k = 0; k < 6; ++k) {
        int x = c.x + (k % 2 ? 1 : -1) * rnd.range(14, 20), y = c.y + rnd.range(-6, 6);
        if (m.at(x, y) == FLOOR && std::abs(y - c.y) > 1) m.place(x, y, STALL);
      }
      // the Ash-Mayor's hall at the head of the avenue, the way in at its foot
      int hx0 = c.x - 10, hx1 = c.x + 10, hy0 = 3, hy1 = 12;
      m.rect(hx0 - 1, hy0 - 1, hx1 + 1, hy1 + 1, WALL);
      m.room(hx0, hy0, hx1, hy1);
      for (int x = c.x - 1; x <= c.x + 1; ++x) m.put(x, hy1 + 1, FLOOR);
      for (int y = hy1 + 2; y < c.y; ++y) for (int x = c.x - 1; x <= c.x + 1; ++x) if (m.at(x, y) == WALL || m.at(x, y) == WATER) m.put(x, y, FLOOR);
      for (int x = hx0 + 3; x <= c.x - 3; x += 4) for (int y : {hy0 + 2, hy1 - 2}) { m.put(x, y, PILLAR); m.put(2 * c.x - x, y, PILLAR); }
      m.put(c.x, hy0, THRONE);
      down = {c.x, hy0 + 2};
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
            }
          }
        }
      }
      for (auto vd : vault_doors) fl.writings.erase(vd.y * W + vd.x);
      up = {4, h - 5};
      down = {cx, cy + 5};

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
      // the halls: three above the yard, three below, mirrored; each with its door to the patrol way and to the yard
      auto hall = [&](int x0, int y0, int x1, int y1, int kind, bool top) {
        m.room(x0, y0, x1, y1);
        m.put((x0 + x1) / 2, top ? y0 - 1 : y1 + 1, DOOR);  // to the patrol way
        m.put((x0 + x1) / 2, top ? y1 + 1 : y0 - 1, DOOR);  // to the yard side
        if (kind == 0) for (int y = y0 + 1; y <= y1 - 1; y += 2) for (int x = x0 + 2; x <= x1 - 2; x += 3) if (x != (x0 + x1) / 2) m.put(x, y, BUNK);
        if (kind == 1) for (int y = y0 + 2; y <= y1 - 2; y += 3) for (int x = x0 + 3; x <= x1 - 3; ++x) if (std::abs(x - (x0 + x1) / 2) > 1) m.put(x, y, TABLE);
        if (kind == 2) { for (int x = x0 + 1; x <= x1 - 1; x += 2) if (std::abs(x - (x0 + x1) / 2) > 1) { m.put(x, y0, RACK); m.put(x, y1, RACK); } m.treasure.push_back(m.rooms.back()); }
        if (kind == 3) for (int k = 0; k < 8; ++k) m.put(rnd.range(x0, x1), rnd.range(y0, y1), CRATE);
      };
      int hw = (w - 12) / 3;
      int kinds[3] = {0, 1, 2};
      for (int i = 0; i < 3; ++i) {
        int x0 = 6 + i * hw, x1 = x0 + hw - 3;
        hall(x0, 6, x1, yy0 - 4, kinds[i], true);
        hall(x0, yy1 + 4, x1, h - 7, i == 1 ? 3 : i == 2 ? 0 : 1, false);
      }
      // the ways along the yard, between it and the halls
      for (int o = 0; o < 2; ++o) { m.rect(5, yy0 - 2 + o, w - 6, yy0 - 2 + o, FLOOR); m.rect(5, yy1 + 1 + o, w - 6, yy1 + 1 + o, FLOOR); }
      // ways from the patrol round to the yard's sides
      m.way({4, cy}, {yx0, cy}, 3);
      m.way({w - 5, cy}, {yx1, cy}, 3);
      // the gate at the foot of the west side: a portcullis half raised
      m.put(1, cy - 1, BARS); m.put(1, cy + 1, BARS);
      up = {5, cy};
      fl.up = up;
      down = {cx, yy0 + 3};
      m.treasure.push_back(m.rooms[0]);

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
      // the pit, the chains coiled from it, the torn vault door at its edge
      m.disc(c, 3.2, CHASM);
      for (int k = 0; k < 8; ++k) {
        double a = k * 3.14159265 / 4 + .3;
        for (double r = 4; r <= 7; r += 1) m.put(c.x + int(std::lround(std::cos(a) * r * 1.3)), c.y + int(std::lround(std::sin(a) * r)), CHAIN);
      }
      m.put(c.x - 1, c.y + 4, OPENVAULT); m.put(c.x, c.y + 4, OPENVAULT); m.put(c.x + 1, c.y + 4, OPENVAULT);
      // the wings: west and east long, north and south short, each a corridor with barred cells on both sides
      struct Wing { int dx, dy, len; };
      std::vector<Wing> wings = {{-1, 0, c.x - 21 - 10}, {1, 0, w - 1 - (c.x + 21) - 10}, {0, -1, c.y - 14 - 9}, {0, 1, h - 1 - (c.y + 14) - 9}};
      std::vector<P> guards;
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
            if (broken) m.treasure.push_back(inside);
          }
        }
        // the guard room at the end
        P g{e.x + wg.dx * 5, e.y + wg.dy * 4};
        m.room(g.x - 4, g.y - 3, g.x + 4, g.y + 3);
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
      down = {c.x, c.y - 5};

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
