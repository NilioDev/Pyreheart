// Dressing the Cistern: the dwarves' old waterworks, choked with black sludge. Rooms get a purpose (the great sluice,
// a broken waterwheel, settling basins, a pump house, the shrine of the Deep-Tide, a drowned pillar hall, a leech nest,
// an aqueduct over a chasm); the walls sweat and grow glowing moss, broken pipes pour sludge, and shafts breathe the heat
// of the dead city below. Layouts that look as if they should be mirrored are; the ways in stay clear and every set of
// furniture stands or falls whole, so there is always room to fight and nothing is ever cut off.
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <set>

#include "game.hpp"
#include "k5.hpp"

using namespace data;

namespace {
struct Rect { int x, y, w, h; };
using Cells = std::vector<std::pair<P, int>>;
}  // namespace

void Game::dress_cistern(Floor& fl, const k5::Plan& plan) {
  auto plainish = [&](int t) { return t == FLOOR || t == RUBBLE; };
  // the largest rectangle of plain floor in a room
  auto inner = [&](const k5::Room& r, Rect& out) {
    int x0 = W, y0 = H, x1 = -1, y1 = -1;
    for (auto c : r.cells) { x0 = std::min(x0, c.x); y0 = std::min(y0, c.y); x1 = std::max(x1, c.x); y1 = std::max(y1, c.y); }
    if (x1 < 0) return false;
    int best = 0;
    for (int ay = y0; ay <= y1; ++ay)
      for (int ax = x0; ax <= x1; ++ax) {
        int maxw = x1 - ax + 1;
        for (int cy = ay; cy <= y1; ++cy) {
          int w = 0;
          while (w < maxw && r.cells.count({ax + w, cy}) && plainish(fl.tile(ax + w, cy))) ++w;
          maxw = w;
          if (!maxw) break;
          int area = maxw * (cy - ay + 1);
          if (area > best) { best = area; out = {ax, ay, maxw, cy - ay + 1}; }
        }
      }
    return best > 0;
  };
  std::vector<const k5::Room*> rooms;
  for (auto& r : plan.rooms)
    if (r.kind != "hall" && !r.cells.empty() && !r.cells.count(plan.up) && !r.cells.count(plan.down)) rooms.push_back(&r);
  rnd.shuffle(rooms);
  std::stable_sort(rooms.begin(), rooms.end(), [](auto* a, auto* b) { return a->cells.size() / 30 > b->cells.size() / 30; });
  // the themes that need a big room are tried first, so the small ones do not take the big rooms
  std::vector<std::string> themes = {"sluice", "wheel", "basins", "fountain", "pillars", "aqueduct"}, small = {"pumps", "nest"};
  rnd.shuffle(themes);
  rnd.shuffle(small);
  themes.insert(themes.end(), small.begin(), small.end());
  PSet themed;  // cells of rooms that got a purpose: no shafts there
  for (auto* r : rooms) {
    Rect R;
    if (!inner(*r, R) || R.w < 8 || R.h < 6) continue;
    std::vector<P> ways{fl.up, fl.down};
    for (auto c : r->cells)
      for (P d4 : {P{1, 0}, P{-1, 0}, P{0, 1}, P{0, -1}}) {
        P n = c + d4;
        if (!r->cells.count(n) && fl.tile(n.x, n.y) != WALL) ways.push_back(c);
      }
    auto in_way = [&](P c) {
      for (auto w : ways) if (cheb(c.x, c.y, w.x, w.y) <= 1) return true;
      return false;
    };
    auto in_R = [&](P c) { return c.x >= R.x && c.y >= R.y && c.x < R.x + R.w && c.y < R.y + R.h; };
    int mx2 = 2 * R.x + R.w - 1, my2 = 2 * R.y + R.h - 1;  // x + mirror x, y + mirror y
    for (size_t k = 0; k < themes.size(); ++k) {
      const std::string th = themes[k];
      std::vector<Cells> sets;  // each set stands or falls whole; the first is the core of the room
      Cells walkable;          // walkable dressing: only needs plain floor under it
      std::vector<std::pair<P, std::string>> sleepers;
      bool blocking_core = true;
      bool any_core = false;  // the room has a purpose if any of the sets goes in, not only the first
      if (th == "sluice") {
        // a channel of sludge across the room, a rusted floodgate in the middle, a grate either side to cross by, gears
        // by the gate, and a lever that opens the sluice and lets the channel drain
        if (R.w < 12 || R.h < 7) continue;
        int ch = R.h % 2 ? 1 : 2, cy = R.y + (R.h - ch) / 2, gw = R.w % 2 ? 1 : 2, gx = R.x + (R.w - gw) / 2;
        Cells core;
        for (int y = cy; y < cy + ch; ++y)
          for (int x = R.x; x < R.x + R.w; ++x) {
            bool grate = x == R.x + 2 || x == R.x + 3 || x == mx2 - (R.x + 2) || x == mx2 - (R.x + 3);
            bool gate = x >= gx && x < gx + gw;
            core.push_back({{x, y}, gate ? FLOODGATE : grate ? GRATE : SLUDGE});
          }
        for (int x = gx; x < gx + gw; ++x) { core.push_back({{x, cy - 1}, FLOODGATE}); core.push_back({{x, cy + ch}, FLOODGATE}); }
        sets.push_back(core);
        if (gx - 3 > R.x + 4)
          sets.push_back({{{gx - 3, cy - 2}, GEAR}, {{mx2 - (gx - 3), cy - 2}, GEAR}, {{gx - 3, cy + ch + 1}, GEAR}, {{mx2 - (gx - 3), cy + ch + 1}, GEAR}});
        sets.push_back({{{gx - 1, cy - 2}, LEVER}});
      } else if (th == "wheel") {
        // a channel down the middle with a broken waterwheel in it, still rocking; grates across at both ends
        if (R.w < 11 || R.h < 9) continue;
        int cw = R.w % 2 ? 5 : 4, cx0 = R.x + (R.w - cw) / 2;
        double cx = R.x + (R.w - 1) / 2.0, cy = R.y + (R.h - 1) / 2.0;
        Cells core;
        for (int y = R.y; y < R.y + R.h; ++y)
          for (int x = cx0; x < cx0 + cw; ++x) core.push_back({{x, y}, y == R.y + 1 || y == my2 - (R.y + 1) ? GRATE : SLUDGE});
        double rx = cw / 2.0 + .9, ry = std::min(3.0, (R.h - 6) / 2.0 + .3);
        for (int y = R.y; y < R.y + R.h; ++y)
          for (int x = R.x; x < R.x + R.w; ++x) {
            double e = std::hypot((x - cx) / rx, (y - cy) / ry);
            if (e > .78 && e < 1.18 && std::abs(y - cy) <= ry + .2) {
              for (auto& [c, t] : core) if (c == P{x, y}) t = WHEEL;
              if (x < cx0 || x >= cx0 + cw) core.push_back({{x, y}, WHEEL});
            }
          }
        for (auto& [c, t] : core)
          if (std::abs(c.x - cx) < .6 + (cw % 2 ? 0 : .5) && std::abs(c.y - cy) < .6) t = HUB;
        sets.push_back(core);
      } else if (th == "basins") {
        // settling basins of black sludge in a mirrored grid; something floats in one of them
        if (R.w < 12 || R.h < 6) continue;
        // a margin at the walls, two cells if it fits; the gap between the basins takes what is left, so the grid sits
        // mirrored
        int m = R.w >= 16 && R.h >= 8 ? 2 : 1;
        int bw = std::min(9, (R.w - 2 * m - 2) / 2), nr = R.h >= 12 + 2 * (m - 1) ? 2 : 1;
        int bh = nr == 2 ? std::min(7, (R.h - 2 * m - 2) / 2) : std::min(7, R.h - 2 * m);
        if (nr == 1 && (R.h - bh) % 2) --bh;
        if (bw < 5 || bh < 4) continue;
        int gapx = R.w - 2 * m - 2 * bw, gapy = nr == 2 ? R.h - 2 * m - 2 * bh : 0;
        int x0 = R.x + m, y0 = nr == 2 ? R.y + m : R.y + (R.h - bh) / 2;
        std::vector<P> mids;
        for (int j = 0; j < nr; ++j)
          for (int i = 0; i < 2; ++i) {
            int bx = x0 + i * (bw + gapx), by = y0 + j * (bh + gapy);
            Cells b;
            for (int y = by; y < by + bh; ++y)
              for (int x = bx; x < bx + bw; ++x)
                b.push_back({{x, y}, x == bx || y == by || x == bx + bw - 1 || y == by + bh - 1 ? BASIN : SLUDGE});
            sets.push_back(b);
            mids.push_back({bx + bw / 2, by + bh / 2});
          }
        // each row of basins is one set, so its mirror image stays whole
        std::vector<Cells> rows;
        for (size_t b = 0; b + 1 < sets.size(); b += 2) {
          Cells row = sets[b];
          row.insert(row.end(), sets[b + 1].begin(), sets[b + 1].end());
          rows.push_back(row);
        }
        sets = rows;
        any_core = true;
        P f = rnd.pick(mids);
        sleepers.push_back({f, "drowned"});
        if (nr == 2 && rnd.coin()) {
          P g = rnd.pick(mids);
          if (g != f) sleepers.push_back({g, "drowned"});
        }
      } else if (th == "pumps") {
        // the sluice-keeper's pump house: a pump with gears in the middle, an old lamp, pipes along the wall, a warning
        if (R.w < 9 || R.h < 7) continue;
        int pw = R.w % 2 ? 5 : 4, px = R.x + (R.w - pw) / 2, py = R.y + (R.h - 2) / 2 - 1;
        Cells core;
        for (int y = py; y < py + 2; ++y)
          for (int x = px; x < px + pw; ++x) core.push_back({{x, y}, y == py && (x == px || x == px + pw - 1) ? GEAR : PUMP});
        sets.push_back(core);
        sets.push_back({{{R.x + R.w / 2, py + 4}, LAMP}});
        if (R.w % 2 == 0) sets.back().push_back({{R.x + R.w / 2 - 1, py + 4}, LAMP});
        sets.push_back({{{px - 2, R.y}, CHAIN}, {{mx2 - (px - 2), R.y}, CHAIN}});
        for (int x = R.x; x < R.x + R.w; ++x)
          if (fl.tile(x, R.y - 1) == WALL && fl.tile(x - 1, R.y - 1) == WALL && fl.tile(x + 1, R.y - 1) == WALL)
            walkable.push_back({{x, R.y - 1}, PIPE});
      } else if (th == "fountain") {
        // the shrine of the Deep-Tide: a round basin, a statue in it still spouting black water, coins on the rim
        if (R.w < 11 || R.h < 9 || R.w % 2 == 0) continue;
        double cx = R.x + (R.w - 1) / 2.0, cy = R.y + (R.h - 1) / 2.0, rx = std::min(5.5, R.w / 2.0 - 2), ry = std::min(3.4, R.h / 2.0 - 1.5);
        Cells core;
        for (int y = R.y; y < R.y + R.h; ++y)
          for (int x = R.x; x < R.x + R.w; ++x) {
            double e = std::hypot((x - cx) / rx, (y - cy) / ry);
            if (e < .72) core.push_back({{x, y}, SLUDGE});
            else if (e < 1.05) core.push_back({{x, y}, BASIN});
          }
        for (auto& [c, t] : core) if (c == P{int(cx), int(std::lround(cy))}) t = TIDESTATUE;
        sets.push_back(core);
        for (int s : {-1, 1})
          for (int u : {-1, 1}) walkable.push_back({{int(cx + s * (rx + 1)), int(std::lround(cy + u * (ry * .6)))}, COIN});
      } else if (th == "pillars") {
        // a drowned pillar hall: columns standing in shallow sludge, with a dry grate walk up the middle
        if (R.w < 11 || R.h < 7) continue;
        int gw = R.w % 2 ? 1 : 2, gx = R.x + (R.w - gw) / 2;
        Cells core;
        for (int y = R.y; y < R.y + R.h; ++y)
          for (int x = R.x; x < R.x + R.w; ++x) core.push_back({{x, y}, x >= gx - 1 && x < gx + gw + 1 ? GRATE : MIRE});
        sets.push_back(core);
        blocking_core = false;
        for (int y = R.y + 1; y + 1 < R.y + R.h && y <= my2 - y; y += 3)
          for (int x = R.x + 1; x + 2 < gx; x += 3) {
            Cells p;
            for (P c : {P{x, y}, P{mx2 - x, y}, P{x, my2 - y}, P{mx2 - x, my2 - y}}) p.push_back({c, PILLAR});
            sets.push_back(p);
          }
      } else if (th == "nest") {
        // a leech nest: clutches of pale eggs, pools of sludge, slime trails; some eggs are about to hatch
        if (R.w < 8 || R.h < 6) continue;
        blocking_core = false;
        Cells pools;
        for (int i = 0; i < 2; ++i) {
          P c{rnd.range(R.x + 1, R.x + R.w - 2), rnd.range(R.y + 1, R.y + R.h - 2)};
          for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -3; dx <= 3; ++dx)
              if (dx * dx / 9.0 + dy * dy / 4.0 < 1) pools.push_back({{c.x + dx, c.y + dy}, MIRE});
        }
        sets.push_back(pools);
        for (int i = 0; i < 12; ++i) {
          P c{rnd.range(R.x, R.x + R.w - 2), rnd.range(R.y, R.y + R.h - 2)};
          Cells e{{c, EGG}, {{c.x + 1, c.y}, EGG}};
          if (rnd.coin()) e.push_back({{c.x, c.y + 1}, EGG});
          sets.push_back(e);
        }
        for (int i = 0; i < 30; ++i) walkable.push_back({{rnd.range(R.x, R.x + R.w - 1), rnd.range(R.y, R.y + R.h - 1)}, SLIME});
      } else if (th == "aqueduct") {
        // an aqueduct over a chasm: the floor falls away, and only a walk joins the ways in
        if (R.w < 12 || R.h < 7) continue;
        std::vector<P> ends;
        for (auto w : ways)
          if (r->cells.count(w) && cheb(w.x, w.y, std::clamp(w.x, R.x, R.x + R.w - 1), std::clamp(w.y, R.y, R.y + R.h - 1)) <= 2)
            ends.push_back({std::clamp(w.x, R.x, R.x + R.w - 1), std::clamp(w.y, R.y, R.y + R.h - 1)});
        if (ends.size() < 2) continue;
        // straight walks from every way in to the middle row, and along it
        int my = R.y + R.h / 2;
        std::set<std::pair<int, int>> walk;
        int lo = R.x + R.w, hi = R.x - 1;
        for (auto e : ends) {
          for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) walk.insert({e.x + dx, e.y + dy});
          for (int y = std::min(e.y, my); y <= std::max(e.y, my); ++y) { walk.insert({e.x, y}); walk.insert({e.x + (e.x + 1 < R.x + R.w ? 1 : -1), y}); }
          lo = std::min(lo, e.x); hi = std::max(hi, e.x);
        }
        for (int x = lo; x <= hi + 1 && x < R.x + R.w; ++x) { walk.insert({x, my}); walk.insert({x, my - 1}); }
        Cells core;
        for (int y = R.y; y < R.y + R.h; ++y)
          for (int x = R.x; x < R.x + R.w; ++x)
            if (!walk.count({x, y})) core.push_back({{x, y}, CHASM});
        sets.push_back(core);
      }
      // place: a set goes in whole or not at all; the core must go in or the room keeps no purpose
      Cells placed;
      bool ok_core = true;
      for (size_t si = 0; si < sets.size(); ++si) {
        bool ok = true;
        for (auto& [c, t] : sets[si]) {
          if (!in_R(c) && t != PIPE) { ok = false; break; }
          if (!plainish(fl.tile(c.x, c.y)) && fl.tile(c.x, c.y) != MIRE && fl.tile(c.x, c.y) != GRATE) { ok = false; break; }
          if (blocks(t) && in_way(c)) { ok = false; break; }
          if (!blocks(t) && t != MIRE && t != GRATE && in_way(c)) { ok = false; break; }
        }
        if (ok) {
          size_t from = placed.size();
          for (auto& [c, t] : sets[si]) { placed.push_back({c, fl.tile(c.x, c.y)}); fl.set(c.x, c.y, t); }
          if (!whole(fl)) {  // it would cut something off: take this set out again
            for (size_t q = placed.size(); q-- > from;) fl.set(placed[q].first.x, placed[q].first.y, placed[q].second);
            placed.resize(from);
            ok = false;
          }
        }
        if (!ok && si == 0 && !any_core) { ok_core = false; break; }
      }
      if (placed.empty()) ok_core = false;
      if (!ok_core) {
        for (auto it = placed.rbegin(); it != placed.rend(); ++it) fl.set(it->first.x, it->first.y, it->second);
        continue;
      }
      (void)blocking_core;
      for (auto& [c, t] : walkable) {
        int was = fl.tile(c.x, c.y);
        if (t == PIPE ? was == WALL : plainish(was)) fl.set(c.x, c.y, t);
      }
      if (th == "pumps") {
        // the warning, chalked on the wall
        for (int x = R.x; x < R.x + R.w; ++x)
          if (fl.tile(x, R.y) == FLOOR) {
            std::string w = "Chalked on the wall of a pump house, in big letters: 'DO NOT DRINK'";
            fl.writings[R.y * W + x] = w;
            used_writings_.insert(w);
            break;
          }
      }
      for (auto& [c, kind] : sleepers) {
        if (fl.tile(c.x, c.y) != SLUDGE) continue;
        fl.set(c.x, c.y, FLOATER);
        Sleeper s;
        s.at = c;
        s.cells = {c};
        s.kind = kind;
        fl.sleepers.push_back(s);
      }
      if (th == "nest") {
        // one egg in each clutch is about to hatch
        std::set<std::pair<int, int>> done;
        for (auto& [c, t] : placed) {
          if (fl.tile(c.x, c.y) != EGG || done.count({c.x / 3, c.y / 3})) continue;
          if (rnd.real() < .5) continue;
          done.insert({c.x / 3, c.y / 3});
          Sleeper s;
          s.at = c;
          s.cells = {c};
          s.kind = "leech";
          fl.sleepers.push_back(s);
        }
      }
      for (auto c : r->cells) themed.insert(c);
      themes.erase(themes.begin() + long(k));
      break;
    }
  }

  // the old aqueducts: a channel of sludge two wide across the level one way and one down it the other, wherever it runs
  // over open floor; grates cross it now and then, and wherever the level would otherwise be cut in two
  for (int dir = 0; dir < 2; ++dir) {
    bool across = dir == 0;
    int len = across ? ZONE_W : ZONE_H, span = across ? ZONE_H : ZONE_W;
    int at = rnd.range(span / 4, span * 3 / 4);
    auto cell = [&](int i, int o) { return across ? P{i, at + o} : P{at + o, i}; };
    auto open_here = [&](int i) {  // the channel and a bank either side of it on plain floor
      for (int o = -1; o < 3; ++o) {
        P c = cell(i, o);
        if (!plainish(fl.tile(c.x, c.y)) || themed.count(c) || cheb(c.x, c.y, fl.up.x, fl.up.y) <= 2 || cheb(c.x, c.y, fl.down.x, fl.down.y) <= 2)
          return false;
      }
      return true;
    };
    std::vector<std::pair<int, int>> runs;
    for (int i = 1; i < len - 1;) {
      if (!open_here(i)) { ++i; continue; }
      int j = i;
      while (j < len - 1 && open_here(j)) ++j;
      if (j - i >= 6) runs.push_back({i, j});
      i = j;
    }
    std::vector<std::pair<P, int>> put;
    for (auto [a, b] : runs) {
      int next = a + rnd.range(3, 7);
      for (int i = a; i < b; ++i) {
        bool grate = i == next && i < b - 2;
        if (grate) next += rnd.range(8, 13);
        for (int o = 0; o < 2; ++o) {
          P c = cell(i, o);
          put.push_back({c, fl.tile(c.x, c.y)});
          fl.set(c.x, c.y, grate ? GRATE : SLUDGE);
        }
      }
    }
    // more grates until nothing is cut off; if that cannot be done, no channel
    std::vector<int> cols;
    for (auto [a, b] : runs) for (int i = a; i < b; ++i) cols.push_back(i);
    rnd.shuffle(cols);
    size_t extra = 0;
    for (size_t k = 0; !whole(fl) && k < cols.size(); ++k, ++extra)
      for (int o = 0; o < 2; ++o) { P c = cell(cols[k], o); fl.set(c.x, c.y, GRATE); }
    if (!whole(fl) || extra * 4 > cols.size()) for (auto& [c, t] : put) fl.set(c.x, c.y, t);
  }

  // everywhere: glowing moss on the sweating walls, broken pipes pouring sludge, shafts down to the dead city
  std::vector<P> moss;
  for (int y = 1; y < ZONE_H - 1; ++y)
    for (int x = 1; x < ZONE_W - 1; ++x) {
      if (fl.tile(x, y) != WALL) continue;
      bool faces = false;
      for (P d4 : {P{1, 0}, P{-1, 0}, P{0, 1}, P{0, -1}})
        if (!blocks(fl.tile(x + d4.x, y + d4.y)) || fl.tile(x + d4.x, y + d4.y) == SLUDGE) faces = true;
      if (!faces) continue;
      bool near = false;
      for (auto m : moss) if (cheb(m.x, m.y, x, y) < 6) near = true;
      if (!near && rnd.real() < .06) { fl.set(x, y, MOSSWALL); moss.push_back({x, y}); }
    }
  std::vector<P> spouts;
  for (int y = 1; y < ZONE_H - 1; ++y)
    for (int x = 1; x < ZONE_W - 1; ++x) {
      if (fl.tile(x, y) != WALL || fl.tile(x, y + 1) != SLUDGE) continue;
      if (fl.tile(x - 1, y) != WALL || fl.tile(x + 1, y) != WALL) continue;
      bool near = false;
      for (auto s : spouts) if (cheb(s.x, s.y, x, y) < 9) near = true;
      if (!near && rnd.real() < .3) { fl.set(x, y, SPOUT); spouts.push_back({x, y}); }
    }
  int shafts = rnd.range(1, 2);
  for (int tries = 0; tries < 300 && shafts > 0; ++tries) {
    P c{rnd.range(3, ZONE_W - 5), rnd.range(3, ZONE_H - 5)};
    bool ok = true;
    for (int dy = -2; dy <= 3 && ok; ++dy)
      for (int dx = -2; dx <= 3; ++dx) {
        P n{c.x + dx, c.y + dy};
        if (fl.tile(n.x, n.y) != FLOOR || themed.count(n) || cheb(n.x, n.y, fl.up.x, fl.up.y) < 3 || cheb(n.x, n.y, fl.down.x, fl.down.y) < 3) { ok = false; break; }
      }
    if (!ok) continue;
    for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx) fl.set(c.x + dx, c.y + dy, SHAFT);
    if (!whole(fl)) { for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx) fl.set(c.x + dx, c.y + dy, FLOOR); continue; }
    --shafts;
  }
}

// a hero pulls a sluice lever: the gate grinds open and the sludge in the channel drains away, opening a way through
void Game::pull_lever(Hero& h, int x, int y) {
  std::deque<P> q;
  std::set<std::pair<int, int>> seen;
  for (int dy = -3; dy <= 3; ++dy)
    for (int dx = -3; dx <= 3; ++dx)
      if (floor->tile(x + dx, y + dy) == SLUDGE || floor->tile(x + dx, y + dy) == FLOODGATE) { q.push_back({x + dx, y + dy}); seen.insert({x + dx, y + dy}); }
  if (q.empty()) { h.say("The lever is rusted fast. Nothing happens.", 0xA0A0A0); return; }
  std::vector<P> drain;
  while (!q.empty() && drain.size() < 400) {
    P c = q.front(); q.pop_front();
    drain.push_back(c);
    for (P d4 : {P{1, 0}, P{-1, 0}, P{0, 1}, P{0, -1}}) {
      P n = c + d4;
      int t = floor->tile(n.x, n.y);
      if ((t == SLUDGE || t == FLOODGATE) && seen.insert({n.x, n.y}).second) q.push_back(n);
    }
  }
  for (auto c : drain) floor->set(c.x, c.y, floor->tile(c.x, c.y) == FLOODGATE ? GRATE : MIRE);
  say_near(x, y, "You haul on the lever. Gears shriek, the floodgate lifts, and the sludge drains away with a long gurgle.", 0xC8D8C0);
  add_fx({{"k", "dust"}, {"x", x}, {"y", y}, {"tomb", false}, {"fall", true}, {"dur", 1.4}});
}

// a hero steps onto a grate over the sludge: now and then something swims up from under it
void Game::under_grate(Hero& h) {
  if (rnd.real() > .04) return;
  P at;
  if (!free_near(h.x, h.y, 2, at)) return;
  auto m = spawn("leech", at.x, at.y);
  m->awake = true;
  m->energy = 0;
  say_near(h.x, h.y, "Something moves under the grate, and a " + m->name + " squeezes up through the bars!", 0xC8D0A0);
  tip(h, "grate", "Things swim under the grates. Cross them quickly.");
}
