// Dressing the crypt: rooms get a purpose (a chapel, a tomb hall, an ossuary, an embalming room) and torches on the
// walls. Every layout is mirrored, so a room that looks as if it should be symmetric is; aisles stay two cells wide and
// the ways in stay clear, so there is always room to fight. Some tombs and corpses are not quite dead: a cracked lid
// or a twitching corpse gives them away, and they climb out when a hero comes close (see Game::wake_dead).
#include <cstdlib>
#include <deque>
#include <functional>
#include <set>

#include "game.hpp"
#include "k5.hpp"

using namespace data;

namespace {

struct Rect { int x, y, w, h; };
std::string a_name(const std::string& n) { return (std::string("aeiou").find(n[0]) != std::string::npos ? "An " : "A ") + n; }
using Piece = std::vector<std::pair<P, int>>;  // cells and the tile each gets
using Set = std::vector<Piece>;                // pieces that stand or fall together, so the mirror image stays whole

// Offsets of n pieces of a length along a span, mirrored about its middle, with at least `margin` free at the ends.
// When the slack cannot be split evenly, an even count of pieces takes one more cell in the middle gap.
std::vector<int> spread(int len, int piece, int gap, int margin, int maxn) {
  for (int n = maxn; n >= 1; --n) {
    int base = n * piece + (n - 1) * gap, slack = len - base;
    if (slack < 2 * margin) continue;
    int mid_extra = 0;
    if (slack % 2) {
      if (n % 2 || slack - 1 < 2 * margin) continue;
      mid_extra = 1;
      --slack;
    }
    std::vector<int> v;
    int at = slack / 2;
    for (int i = 0; i < n; ++i) {
      v.push_back(at);
      at += piece + gap + (mid_extra && i == n / 2 - 1 ? 1 : 0);
    }
    return v;
  }
  return {};
}

}  // namespace

void Game::dress(Floor& fl, const k5::Plan& plan) {
  if (fl.act != 0) return;
  int d = fl.depth;
  // the largest rectangle of plain floor in a room: the furniture is mirrored within it
  auto inner = [&](const k5::Room& r, Rect& out) {
    int x0 = W, y0 = H, x1 = -1, y1 = -1;
    for (auto c : r.cells) { x0 = std::min(x0, c.x); y0 = std::min(y0, c.y); x1 = std::max(x1, c.x); y1 = std::max(y1, c.y); }
    if (x1 < 0) return false;
    int bw = x1 - x0 + 1, bh = y1 - y0 + 1;
    std::vector<int> sum((bw + 1) * (bh + 1), 0);  // counts of cells that are not plain floor
    for (int y = 0; y < bh; ++y)
      for (int x = 0; x < bw; ++x) {
        int t = fl.tile(x0 + x, y0 + y);
        int bad = !(r.cells.count({x0 + x, y0 + y}) && (t == FLOOR || t == RUBBLE));
        sum[(y + 1) * (bw + 1) + x + 1] = bad + sum[y * (bw + 1) + x + 1] + sum[(y + 1) * (bw + 1) + x] - sum[y * (bw + 1) + x];
      }
    auto bad = [&](int ax, int ay, int cx, int cy) {  // inclusive-exclusive
      return sum[cy * (bw + 1) + cx] - sum[ay * (bw + 1) + cx] - sum[cy * (bw + 1) + ax] + sum[ay * (bw + 1) + ax];
    };
    int best = 0;
    for (int ay = 0; ay < bh; ++ay)
      for (int cy = ay + 1; cy <= bh; ++cy)
        for (int ax = 0; ax < bw; ++ax)
          for (int cx = ax + 1; cx <= bw; ++cx) {
            int area = (cx - ax) * (cy - ay);
            if (area <= best || bad(ax, ay, cx, cy)) continue;
            best = area;
            out = {x0 + ax, y0 + ay, cx - ax, cy - ay};
          }
    return best > 0;
  };
  // a torch on the wall: the first wall reached going out from a floor cell, in a straight run of wall
  auto torch_at = [&](int x, int y, int dx, int dy, P& out) {
    for (int k = 0; k < 6; ++k) {
      x += dx; y += dy;
      int t = fl.tile(x, y);
      auto wall = [&](int a, int b) { int w = fl.tile(a, b); return w == WALL || w == BONEWALL; };
      if (wall(x, y)) {
        // the wall on either side along the run must be wall too, and the cell in front plain floor
        int ax = dy != 0, ay = dx != 0;
        if (!wall(x + ax, y + ay) || !wall(x - ax, y - ay)) return false;
        if (int f0 = fl.tile(x - dx, y - dy); f0 != FLOOR && f0 != RUBBLE) return false;
        out = {x, y};
        return true;
      }
      if (t != FLOOR && t != RUBBLE) return false;
    }
    return false;
  };

  std::vector<const k5::Room*> rooms;
  for (auto& r : plan.rooms)
    if (r.kind != "hall" && !r.cells.empty() && !r.cells.count(plan.up)) rooms.push_back(&r);
  rnd.shuffle(rooms);
  int chapels = 0, tombs = 0, biers = 0, ossuaries = 0;
  for (auto* r : rooms) {
    Rect R;
    if (!inner(*r, R) || R.w < 7 || R.h < 6) continue;
    // the ways in: room cells beside open ground that is not the room, and the stairs; keep two cells round them clear
    std::vector<P> ways;
    for (auto c : r->cells)
      for (P d4 : {P{1, 0}, P{-1, 0}, P{0, 1}, P{0, -1}}) {
        P n = c + d4;
        if (!r->cells.count(n) && fl.tile(n.x, n.y) != WALL) ways.push_back(c);
      }
    if (r->cells.count(plan.down)) ways.push_back(plan.down);
    auto in_way = [&](P c) {
      for (auto w : ways) if (cheb(c.x, c.y, w.x, w.y) <= 2) return true;
      return false;
    };
    std::vector<Set> sets;
    auto mirror_x = [&](int x) { return 2 * R.x + R.w - 1 - x; };
    auto mirror_y = [&](int y) { return 2 * R.y + R.h - 1 - y; };
    // a grid of pieces, grouped with their mirror images left-right and top-bottom
    auto grid = [&](const std::vector<int>& xs, const std::vector<int>& ys, const std::function<Piece(int, int)>& make) {
      int n = int(xs.size()), m = int(ys.size());
      for (int j = 0; j < (m + 1) / 2; ++j)
        for (int i = 0; i < (n + 1) / 2; ++i) {
          Set st;
          std::set<std::pair<int, int>> done;
          for (auto [a, b] : {std::pair{i, j}, std::pair{n - 1 - i, j}, std::pair{i, m - 1 - j}, std::pair{n - 1 - i, m - 1 - j}})
            if (done.insert({a, b}).second) st.push_back(make(xs[a], ys[b]));
          sets.push_back(st);
        }
    };
    // offsets along a side: two cells free at the walls if it fits, else one; a room that is not a plain box may sit
    // half a cell off its middle
    bool plain = r->shape == "plain";
    auto fit = [&](int len, int piece, int maxn) {
      std::vector<int> best;
      for (int margin : {2, 1})
        for (int off : {0, 1}) {
          if (off && plain) continue;
          auto v = spread(len - off, piece, 2, margin, maxn);
          if (v.size() > best.size()) best = v;
        }
      return best;
    };
    std::string theme;
    // pick a purpose the room is big enough for and that fits it mirrored; a plain room now and then
    std::vector<std::string> want;
    if (!chapels && R.w >= 11 && R.h >= 9) want.push_back("chapel");
    if (tombs < 2 && R.w >= 9 && R.h >= 7) want.push_back("tombs");
    if (biers < 1 && R.w >= 9 && R.h >= 7) want.push_back("biers");
    if (ossuaries < 1) want.push_back("ossuary");
    rnd.shuffle(want);
    if (rnd.real() < .2) want.clear();
    auto build = [&](const std::string& theme) -> bool {
      if (theme == "tombs") {
        // stone tombs, five wide and three deep, in a mirrored grid with two-cell aisles all round
        auto xs = fit(R.w, 5, 4), ys = fit(R.h, 3, 3);
        if (xs.empty() || ys.empty()) return false;
        if (!theme.empty())
          grid(xs, ys, [&](int ox, int oy) {
            Piece p;
            for (int dy = 0; dy < 3; ++dy)
              for (int dx = 0; dx < 5; ++dx) p.push_back({{R.x + ox + dx, R.y + oy + dy}, TOMB});
            return p;
          });
      } else if (theme == "biers") {
        // embalming tables, three long, with the dead laid out on some
        auto xs = fit(R.w, 3, 5), ys = fit(R.h, 1, 4);
        if (xs.empty() || ys.empty() || xs.size() * ys.size() < 2) return false;
        if (!theme.empty())
          grid(xs, ys, [&](int ox, int oy) {
            Piece p;
            for (int dx = 0; dx < 3; ++dx) p.push_back({{R.x + ox + dx, R.y + oy}, BIER});
            return p;
          });
        if (!theme.empty())
          for (int k = 0; k < 2; ++k) {  // candles in the corners
            int y = k ? R.y + R.h - 1 : R.y;
            sets.push_back({{{{R.x, y}, CANDLE}}, {{{R.x + R.w - 1, y}, CANDLE}}});
          }
      } else if (theme == "chapel") {
        // the altar against the wall with no way in, pews either side of a carpet up the middle
        bool top_free = true, bottom_free = true;
        for (int x = R.x; x < R.x + R.w; ++x) {
          if (in_way({x, R.y})) top_free = false;
          if (in_way({x, R.y + R.h - 1})) bottom_free = false;
        }
        if (!top_free && !bottom_free) return false;
        else {
          bool top = top_free && (!bottom_free || rnd.coin());
          auto Y = [&](int k) { return top ? R.y + k : R.y + R.h - 1 - k; };
          int a = R.w % 2 ? 3 : 2, ax = R.x + (R.w - a) / 2;
          Set altar;
          Piece pa;
          for (int x = ax; x < ax + a; ++x) pa.push_back({{x, Y(0)}, ALTAR});
          altar.push_back(pa);
          altar.push_back({{{ax - 2, Y(0)}, CANDLE}});
          altar.push_back({{{mirror_x(ax - 2), Y(0)}, CANDLE}});
          sets.push_back(altar);
          sets.push_back({{{{R.x, Y(0)}, CANDLE}}, {{{R.x + R.w - 1, Y(0)}, CANDLE}}});
          int len = ax - 1 - (R.x + 2) + 1;
          if (len >= 2)
            for (int k = 3; k <= R.h - 3; k += 2) {
              Piece l, rr;
              for (int x = R.x + 2; x <= ax - 1; ++x) { l.push_back({{x, Y(k)}, PEW}); rr.push_back({{mirror_x(x), Y(k)}, PEW}); }
              if (int(l.size()) >= 2) sets.push_back({l, rr});
            }
        }
      } else if (theme == "ossuary") {
        // the walls stacked with bones and skulls, bones on the floor along them, and pillars in a big one
        if (R.w >= 9 && R.h >= 9) {
          int px = R.x + 2 + (R.w >= 13), py = R.y + 2 + (R.h >= 13);
          sets.push_back({{{{px, py}, PILLAR}}, {{{mirror_x(px), py}, PILLAR}}, {{{px, mirror_y(py)}, PILLAR}},
                          {{{mirror_x(px), mirror_y(py)}, PILLAR}}});
        }
      }
      // only what stands clear of the ways in, a whole set or nothing of it
      std::erase_if(sets, [&](const Set& st) {
        for (auto& p : st)
          for (auto& [c, t] : p) if (in_way(c) || (fl.tile(c.x, c.y) != FLOOR && fl.tile(c.x, c.y) != RUBBLE)) return true;
        return false;
      });
      if (theme == "chapel" && (sets.empty() || sets[0].size() != 3 || sets[0][0][0].second != ALTAR)) return false;
      return !sets.empty() || theme == "ossuary";
    };
    for (auto& t : want) {
      sets.clear();
      if (build(t)) { theme = t; break; }
    }

    // place what stands clear of the ways in, a whole set or nothing of it
    std::vector<std::pair<P, int>> placed;
    for (auto& s : sets) {
      bool ok = true;
      for (auto& p : s)
        for (auto& [c, t] : p) if (in_way(c) || (fl.tile(c.x, c.y) != FLOOR && fl.tile(c.x, c.y) != RUBBLE)) ok = false;
      if (!ok) continue;
      for (auto& p : s)
        for (auto& [c, t] : p) { fl.set(c.x, c.y, t); placed.push_back({c, t}); }
    }
    if (!whole(fl)) {
      for (auto& [c, t] : placed) fl.set(c.x, c.y, FLOOR);
      placed.clear();
    }
    if (theme == "tombs" && !placed.empty()) {
      ++tombs;
      // about one tomb in four is cracked: something in it will climb out
      for (auto& [c, t] : placed) {
        if (fl.tile(c.x, c.y) != TOMB) continue;
        if (fl.tile(c.x - 1, c.y) == TOMB && fl.tile(c.x - 2, c.y) == TOMB && fl.tile(c.x + 1, c.y) == TOMB &&
            fl.tile(c.x + 2, c.y) == TOMB && fl.tile(c.x, c.y - 1) == TOMB && fl.tile(c.x, c.y + 1) == TOMB &&
            rnd.real() < .27) {
          fl.set(c.x, c.y, CRACKED);
          Sleeper s;
          s.at = c;
          for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -2; dx <= 2; ++dx) s.cells.push_back({c.x + dx, c.y + dy});
          s.kind = d >= 2 && rnd.real() < .4 ? "ghoul" : "bonearcher";
          fl.sleepers.push_back(s);
        }
      }
    }
    if (theme == "biers" && !placed.empty()) {
      ++biers;
      int restless = 0;
      for (auto& [c, t] : placed) {
        if (t != BIER || fl.tile(c.x - 1, c.y) != BIER || fl.tile(c.x + 1, c.y) != BIER) continue;
        double r2 = rnd.real();
        if (r2 < .2 && restless < 2) {
          ++restless;
          fl.set(c.x, c.y, RESTLESS);
          Sleeper s;
          s.at = c;
          s.cells = {{c.x - 1, c.y}, c, {c.x + 1, c.y}};
          s.kind = d >= 3 && rnd.real() < .4 ? "ghoul" : "shambler";
          fl.sleepers.push_back(s);
        } else if (r2 < .55) fl.set(c.x, c.y, CORPSE);
      }
    }
    if (theme == "chapel" && !placed.empty()) ++chapels;
    if (theme == "ossuary") {
      ++ossuaries;
      // the walls stacked with bones and skulls, and bones on the floor along them
      for (auto c : r->cells)
        for (int dy = -1; dy <= 1; ++dy)
          for (int dx = -1; dx <= 1; ++dx) {
            P n{c.x + dx, c.y + dy};
            if (fl.tile(n.x, n.y) == WALL && n.x > 0 && n.y > 0 && n.x < W - 1 && n.y < H - 1) fl.set(n.x, n.y, BONEWALL);
          }
      for (auto c : r->cells) {
        if (fl.tile(c.x, c.y) != FLOOR || in_way(c)) continue;
        bool wall = false;
        for (int dy = -1; dy <= 1; ++dy)
          for (int dx = -1; dx <= 1; ++dx) if (fl.tile(c.x + dx, c.y + dy) == BONEWALL) wall = true;
        if (wall && rnd.real() < .45) fl.set(c.x, c.y, BONES);
      }
    }

    // torches on both side walls, level with each other; two pairs in a tall room
    std::vector<int> rows;
    if (R.h >= 12) rows = {R.y + R.h / 4, mirror_y(R.y + R.h / 4)};
    else rows = {R.y + (R.h - 1) / 2};
    for (int y : rows) {
      P a, b;
      if (torch_at(R.x, y, -1, 0, a) && torch_at(R.x + R.w - 1, y, 1, 0, b) && R.x - a.x == b.x - (R.x + R.w - 1)) {
        fl.set(a.x, a.y, TORCH);
        fl.set(b.x, b.y, TORCH);
      }
    }
  }
  // the long halls get a torch now and then
  for (auto& r : plan.rooms) {
    if (r.kind != "hall") continue;
    std::vector<P> cs(r.cells.begin(), r.cells.end());
    std::sort(cs.begin(), cs.end(), [](P a, P b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
    for (size_t i = 0; i < cs.size(); ++i) {
      if (rnd.real() > .06) continue;
      P c = cs[i], t;
      for (P d4 : {P{0, -1}, P{0, 1}, P{-1, 0}, P{1, 0}})
        if (torch_at(c.x, c.y, d4.x, d4.y, t)) {
          bool near = false;
          for (int dy = -6; dy <= 6; ++dy)
            for (int dx = -6; dx <= 6; ++dx) if (fl.tile(t.x + dx, t.y + dy) == TORCH) near = true;
          if (!near) fl.set(t.x, t.y, TORCH);
          break;
        }
    }
  }
}

// A cracked tomb or a twitching corpse with a hero close by: the lid grinds for two turns, then the dead climb out.
// A hero who hits it while it grinds smashes it before it gets up.
void Game::wake_dead() {
  for (auto& s : floor->sleepers) {
    if (s.stage < 0) continue;
    bool tomb = floor->tile(s.at.x, s.at.y) == CRACKED;
    if (s.stage == 0) {
      for (auto* h : active()) {
        if (!h->present()) continue;
        bool near = false;
        for (auto c : s.cells) if (cheb(c.x, c.y, h->x, h->y) <= 2 && h->sees(c.x, c.y)) near = true;
        if (!near) continue;
        s.stage = 1;
        say_near(s.at.x, s.at.y, tomb ? "The lid of a tomb grinds against stone..." : "The corpse on the table twitches...", 0xC8BCA0);
        tip(*h, "rising", "Something is stirring. Hit it now and it will not get up.");
        break;
      }
      continue;
    }
    add_fx({{"k", "dust"}, {"x", s.at.x}, {"y", s.at.y}, {"tomb", tomb}, {"dur", 1.2}});
    if (++s.stage <= 3) continue;
    // it rises
    s.stage = -1;
    if (tomb) {
      for (auto c : s.cells)
        if (c.y == s.at.y && std::abs(c.x - s.at.x) <= 1) floor->set(c.x, c.y, OPENTOMB);
    } else floor->set(s.at.x, s.at.y, BIER);
    P at;
    if (!free_near(s.at.x, s.at.y, 3, at)) continue;
    auto m = spawn(s.kind, at.x, at.y);
    m->awake = true;
    m->energy = 0;
    say_near(at.x, at.y, tomb ? "The lid crashes down. " + a_name(m->name) + " climbs out of the tomb!"
                              : "The corpse sits up! " + a_name(m->name) + " crawls off the table.", 0xFF9060);
    add_fx({{"k", "dust"}, {"x", s.at.x}, {"y", s.at.y}, {"tomb", tomb}, {"fall", true}, {"dur", 1.6}});
  }
}

// a hero hits a stirring tomb or corpse before it is up
bool Game::smash_dead(Hero& h, int x, int y) {
  for (auto& s : floor->sleepers) {
    if (s.stage <= 0) continue;
    if (std::find(s.cells.begin(), s.cells.end(), P{x, y}) == s.cells.end()) continue;
    bool tomb = floor->tile(s.at.x, s.at.y) == CRACKED;
    s.stage = -1;
    if (tomb) {
      for (auto c : s.cells)
        if (c.y == s.at.y && std::abs(c.x - s.at.x) <= 1) floor->set(c.x, c.y, OPENTOMB);
    } else floor->set(s.at.x, s.at.y, BIER);
    gain_xp(h, mon_def(s.kind).xp);
    h.say(tomb ? "You slam the lid in. Whatever was in there lies still." : "You strike the corpse down before it can rise.", 0xE6D2A0);
    add_fx({{"k", "dust"}, {"x", s.at.x}, {"y", s.at.y}, {"tomb", tomb}, {"fall", true}, {"dur", 1.6}});
    return true;
  }
  return false;
}
