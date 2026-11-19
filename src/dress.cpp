// Dressing the crypt: rooms get a purpose (a chapel, a tomb hall, an ossuary, an embalming room) and torches on the
// walls. Every layout is mirrored, so a room that looks as if it should be symmetric is; aisles stay two cells wide and
// the ways in stay clear, so there is always room to fight. Some tombs and corpses are not quite dead: a cracked lid
// or a twitching corpse gives them away, and they climb out when a hero comes close (see Game::wake_dead).
#include <cstdlib>
#include <deque>
#include <functional>
#include <map>
#include <set>

#include "game.hpp"
#include "k5.hpp"

using namespace data;

namespace {

struct Rect { int x, y, w, h; };
struct PLess { bool operator()(P a, P b) const { return a.y != b.y ? a.y < b.y : a.x < b.x; } };
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
    int t0 = floor->tile(s.at.x, s.at.y);
    bool tomb = t0 == CRACKED, dwarf = t0 == FALLEN, floater = t0 == FLOATER, egg = t0 == EGG;
    if (s.stage == 0) {
      for (auto* h : active()) {
        if (!h->present()) continue;
        bool near = false;
        for (auto c : s.cells) if (cheb(c.x, c.y, h->x, h->y) <= 2 && h->sees(c.x, c.y)) near = true;
        if (!near) continue;
        s.stage = 1;
        say_near(s.at.x, s.at.y, tomb ? "The lid of a tomb grinds against stone..." : dwarf ? "A charred dwarf stirs among the bones..."
                                 : floater ? "The black sludge begins to bubble..." : egg ? "One of the pale eggs bulges and splits..."
                                           : "The corpse on the table twitches...", 0xC8BCA0);
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
    } else floor->set(s.at.x, s.at.y, dwarf ? BONES : floater ? SLUDGE : egg ? SLIME : BIER);
    P at;
    if (!free_near(s.at.x, s.at.y, 3, at)) continue;
    auto m = spawn(s.kind, at.x, at.y);
    m->awake = true;
    m->energy = 0;
    say_near(at.x, at.y, tomb ? "The lid crashes down. " + a_name(m->name) + " climbs out of the tomb!"
                         : dwarf ? "The fallen dwarf rises! " + a_name(m->name) + " comes for you."
                         : floater ? "The floating corpse rolls over and drags itself out: " + a_name(m->name) + "!"
                         : egg ? "The egg bursts! " + a_name(m->name) + " slithers out."
                                 : "The corpse sits up! " + a_name(m->name) + " crawls off the table.", 0xFF9060);
    add_fx({{"k", "dust"}, {"x", s.at.x}, {"y", s.at.y}, {"tomb", tomb}, {"fall", true}, {"dur", 1.6}});
  }
}

// a hero hits a stirring tomb or corpse before it is up
bool Game::smash_dead(Hero& h, int x, int y) {
  for (auto& s : floor->sleepers) {
    if (s.stage <= 0) continue;
    if (std::find(s.cells.begin(), s.cells.end(), P{x, y}) == s.cells.end()) continue;
    int t0 = floor->tile(s.at.x, s.at.y);
    bool tomb = t0 == CRACKED, dwarf = t0 == FALLEN, floater = t0 == FLOATER, egg = t0 == EGG;
    s.stage = -1;
    if (tomb) {
      for (auto c : s.cells)
        if (c.y == s.at.y && std::abs(c.x - s.at.x) <= 1) floor->set(c.x, c.y, OPENTOMB);
    } else floor->set(s.at.x, s.at.y, dwarf ? BONES : floater ? SLUDGE : egg ? SLIME : BIER);
    gain_xp(h, mon_def(s.kind).xp);
    h.say(tomb ? "You slam the lid in. Whatever was in there lies still."
               : egg ? "You crush the egg before it can hatch." : floater ? "You push the corpse under. It does not come up again."
                     : "You strike the corpse down before it can rise.", 0xE6D2A0);
    add_fx({{"k", "dust"}, {"x", s.at.x}, {"y", s.at.y}, {"tomb", tomb}, {"fall", true}, {"dur", 1.6}});
    return true;
  }
  // an egg that is not about to hatch: crush it anyway
  if (floor->tile(x, y) == EGG) {
    floor->set(x, y, SLIME);
    h.say("You crush a pale egg. It pops wetly.", 0xC8D0A0);
    return true;
  }
  return false;
}

// Dressing the Deep: a few crust plates on the way get a story. A dwarven forge still glowing, the Ashen King's ash
// shrine, a field of steam vents and obsidian, and the plate where the clan fell, where some of the dead get up again.
// The forge and the shrine are mirrored; the vents and the battlefield lie where they lie. The ways onto a plate stay
// clear and nothing may cut a plate off.
void Game::dress_deep(Floor& fl, std::vector<std::vector<P>> plates) {
  // the big plates first, so a story has room around it to fight in
  rnd.shuffle(plates);
  std::stable_sort(plates.begin(), plates.end(), [](auto& a, auto& b) { return a.size() / 12 > b.size() / 12; });
  std::vector<std::string> themes = {"forge", "shrine", "steam", "field"};
  rnd.shuffle(themes);
  for (auto& cells : plates) {
    if (themes.empty()) break;
    std::set<P, PLess> S(cells.begin(), cells.end());
    std::vector<P> ways{fl.up, fl.down};
    for (auto c : cells)
      for (P d4 : {P{1, 0}, P{-1, 0}, P{0, 1}, P{0, -1}}) {
        P n = c + d4;
        if (!S.count(n) && fl.walk(n.x, n.y)) ways.push_back(c);
      }
    auto in_way = [&](P c) {
      for (auto w : ways) if (cheb(c.x, c.y, w.x, w.y) <= 2) return true;
      return false;
    };
    // the largest rectangle of plain crust on the plate
    int x0 = W, y0 = H, x1 = -1, y1 = -1;
    for (auto c : cells) { x0 = std::min(x0, c.x); y0 = std::min(y0, c.y); x1 = std::max(x1, c.x); y1 = std::max(y1, c.y); }
    Rect R{0, 0, 0, 0};
    int best = 0;
    for (int ay = y0; ay <= y1; ++ay)
      for (int ax = x0; ax <= x1; ++ax) {
        int maxw = x1 - ax + 1;
        for (int cy = ay; cy <= y1; ++cy) {
          int w = 0;
          while (w < maxw && S.count({ax + w, cy}) && fl.tile(ax + w, cy) == CRUST) ++w;
          maxw = w;
          if (!maxw) break;
          int area = maxw * (cy - ay + 1);
          if (area > best) { best = area; R = {ax, ay, maxw, cy - ay + 1}; }
        }
      }
    if (R.w < 6 || R.h < 4) continue;
    auto free_cell = [&](P c) { return S.count(c) && fl.tile(c.x, c.y) == CRUST && !in_way(c); };
    for (size_t k = 0; k < themes.size(); ++k) {
      std::string theme = themes[k];
      std::vector<Set> sets;
      std::vector<int> tags;              // tagged sets are tries: only so many of a tag get placed
      std::map<int, int> quota;
      auto tried = [&](int tag, Set st) { while (tags.size() < sets.size()) tags.push_back(-1); sets.push_back(st); tags.push_back(tag); };
      std::vector<std::pair<P, int>> loose;  // walkable litter: needs no clear way, only crust
      if (theme == "forge") {
        if (R.w < 9 || R.h < 4) continue;
        // the hearth against one long side, an anvil either side of it, slag heaps in the far corners, a rail
        bool top = true;
        for (int x = R.x; x < R.x + R.w; ++x) if (in_way({x, R.y})) top = false;
        auto Y = [&](int k2) { return top ? R.y + k2 : R.y + R.h - 1 - k2; };
        int hw = R.w % 2 ? 3 : 4, hx = R.x + (R.w - hw) / 2;
        Piece hearth;
        hearth.push_back({{hx - 1, Y(0)}, STONEWORK});
        for (int x = hx; x < hx + hw; ++x) hearth.push_back({{x, Y(0)}, HEARTH});
        hearth.push_back({{hx + hw, Y(0)}, STONEWORK});
        sets.push_back({hearth});
        int mx = 2 * R.x + R.w - 1;
        if (hx - 3 >= R.x) sets.push_back({{{{hx - 3, Y(1)}, ANVIL}}, {{{mx - (hx - 3), Y(1)}, ANVIL}}});
        if (R.h >= 5) sets.push_back({{{{R.x, Y(R.h - 1)}, SLAG}}, {{{mx - R.x, Y(R.h - 1)}, SLAG}}});
        if (R.h >= 6) {
          for (int x = R.x + 1; x < R.x + R.w - 1; ++x) loose.push_back({{x, Y(R.h - 2)}, RAIL});
          sets.push_back({{{{R.x + R.w - 2, Y(R.h - 2)}, CART}}});
        }
        for (int i = 0; i < 2; ++i) loose.push_back({{rnd.range(R.x, R.x + R.w - 1), Y(rnd.range(2, R.h - 1))}, CHAIN});
      } else if (theme == "shrine") {
        // the King's brazier in the middle of a ring of ash, obsidian obelisks at the corners
        int w = R.w - !(R.w % 2), h = R.h - !(R.h % 2);
        if (w < 9 || h < 5) continue;
        int rx = R.x + (R.w - w) / 2, ry = R.y + (R.h - h) / 2, cx = rx + w / 2, cy = ry + h / 2;
        sets.push_back({{{{cx, cy}, BRAZIER}}});
        // four obelisks round it, mirrored: as far out as the ways onto the plate allow
        quota[0] = 1;
        for (int in = 0; in <= (w - 7) / 2; ++in)
          tried(0, {{{{rx + in, ry}, OBELISK}}, {{{rx + w - 1 - in, ry}, OBELISK}}, {{{rx + in, ry + h - 1}, OBELISK}}, {{{rx + w - 1 - in, ry + h - 1}, OBELISK}}});
        for (int y = ry; y < ry + h; ++y)
          for (int x = rx; x < rx + w; ++x) {
            double r = std::hypot((x - cx) / 1.7, double(y - cy));
            if (r >= 1.5 && r < 2.4) loose.push_back({{x, y}, ASH});
          }
        for (int dy : {-1, 1})
          for (int dx : {-1, 1}) loose.push_back({{cx + dx, cy + dy}, BONES});
      } else if (theme == "steam") {
        // vents hissing out of the crust, glassy spikes of obsidian between them
        quota[0] = 2 + (R.w * R.h >= 60); quota[1] = 3;
        for (int i = 0; i < 14; ++i) tried(0, {{{{rnd.range(R.x + 1, R.x + R.w - 2), rnd.range(R.y + 1, R.y + R.h - 2)}, VENT}}});
        for (int i = 0; i < 14; ++i) {
          P c{rnd.range(R.x, R.x + R.w - 2), rnd.range(R.y, R.y + R.h - 1)};
          Piece p{{c, OBSIDIAN}};
          if (rnd.coin()) p.push_back({{c.x + 1, c.y}, OBSIDIAN});
          tried(1, {p});
        }
      } else if (theme == "field") {
        if (R.w < 8) continue;
        // where the clan fell: a toppled statue, a scorched banner, the dead among broken axes and shields
        quota[0] = 1; quota[1] = 1; quota[2] = 3;
        for (int i = 0; i < 10; ++i) {
          P st{rnd.range(R.x, R.x + R.w - 3), rnd.range(R.y, R.y + R.h - 1)};
          Piece sp;
          for (int j = 0; j < 3; ++j) sp.push_back({{st.x + j, st.y}, STATUE});
          tried(0, {sp});
        }
        for (int i = 0; i < 10; ++i) tried(1, {{{{rnd.range(R.x, R.x + R.w - 1), rnd.range(R.y, R.y + R.h - 1)}, BANNER}}});
        for (int i = 0; i < 20; ++i) tried(2, {{{{rnd.range(R.x, R.x + R.w - 1), rnd.range(R.y, R.y + R.h - 1)}, FALLEN}}});
        for (int i = 0; i < 7; ++i) loose.push_back({{rnd.range(R.x, R.x + R.w - 1), rnd.range(R.y, R.y + R.h - 1)}, BONES});
        for (int i = 0; i < 3; ++i) loose.push_back({{rnd.range(R.x, R.x + R.w - 1), rnd.range(R.y, R.y + R.h - 1)}, WRECK});
        for (int i = 0; i < 2; ++i) loose.push_back({{rnd.range(R.x, R.x + R.w - 1), rnd.range(R.y, R.y + R.h - 1)}, SHIELD});
      }
      // place what stands clear, a whole set or nothing of it; the forge stands or falls with its hearth, the shrine
      // with its brazier
      std::vector<std::pair<P, int>> placed;
      bool core = true;
      while (tags.size() < sets.size()) tags.push_back(-1);
      for (size_t si = 0; si < sets.size(); ++si) {
        if (tags[si] >= 0 && quota[tags[si]] <= 0) continue;
        bool ok = true;
        std::set<P, PLess> mine;
        for (auto& p : sets[si])
          for (auto& [c, t] : p) if (!free_cell(c) || !mine.insert(c).second) ok = false;
        // nothing blocking may stand next to something else blocking that is not of its own set
        for (auto c : mine)
          for (int dy = -1; dy <= 1 && ok; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
              if (!mine.count({c.x + dx, c.y + dy}) && blocks(fl.tile(c.x + dx, c.y + dy)) && fl.tile(c.x + dx, c.y + dy) != LAVA) ok = false;
        if (!ok) { if (si == 0 && (theme == "forge" || theme == "shrine")) { core = false; break; } continue; }
        for (auto& p : sets[si])
          for (auto& [c, t] : p) { fl.set(c.x, c.y, t); placed.push_back({c, t}); }
        if (tags[si] >= 0) --quota[tags[si]];
      }
      if (core && !whole(fl)) core = false;
      if (!core || placed.empty()) {
        for (auto& [c, t] : placed) fl.set(c.x, c.y, CRUST);
        continue;
      }
      for (auto& [c, t] : loose)
        if (S.count(c) && fl.tile(c.x, c.y) == CRUST) fl.set(c.x, c.y, t);
      // some of the fallen are not done fighting
      for (auto& [c, t] : placed) {
        if (t != FALLEN || fl.tile(c.x, c.y) != FALLEN) continue;
        if (rnd.real() < .6) {
          Sleeper s;
          s.at = c;
          s.cells = {c};
          s.kind = rnd.real() < .5 ? "deepknight" : "wraith";
          fl.sleepers.push_back(s);
        }
      }
      themes.erase(themes.begin() + long(k));
      break;
    }
  }
}
