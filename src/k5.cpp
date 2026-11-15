#include "k5.hpp"

#include <cmath>
#include <deque>
#include <optional>

namespace k5 {
namespace {

const std::vector<P> D4 = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
const std::vector<P> DIRS8 = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
const std::vector<P> ALL8 = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
const std::vector<std::string> SHAPES = {"plain", "chamfer", "round", "ell", "tee", "cross", "overlap", "chunky", "pillars",
                                         "rotunda", "inner", "gallery", "zed", "bays", "cave", "ell", "chamfer", "cross"};
const std::unordered_set<std::string> BIG = {"pillars", "rotunda", "inner", "gallery", "cross", "tee"};

struct Mood { double corridor, hallway, narrow, caves, near; };
const std::map<std::string, Mood> MOODS = {
    {"mixed", {.15, .5, .35, .5, .7}},
    {"halls and corridors", {.2, .6, .2, .2, .4}},
    {"close-packed", {.08, .4, .5, .4, .9}},
};

Rng* R_ = nullptr;
int R(int a, int b) { return R_->range(a, b); }
double chance() { return R_->real(); }

PSet rect(int x, int y, int w, int h) {
  PSet s;
  for (int xx = x; xx < x + w; ++xx)
    for (int yy = y; yy < y + h; ++yy) s.insert({xx, yy});
  return s;
}
void add_all(PSet& a, const PSet& b) { a.insert(b.begin(), b.end()); }
void remove_all(PSet& a, const PSet& b) { for (auto& c : b) a.erase(c); }
bool has(const PSet& s, P c) { return s.count(c) > 0; }
template <class V> bool has(const PMap<V>& s, P c) { return s.count(c) > 0; }

PSet room_cells(int w, int h, const std::string& shape) {
  PSet out;
  double cx = (w - 1) / 2.0, cy = (h - 1) / 2.0;
  int cut = R(1, 3);
  for (int yy = 0; yy < h; ++yy)
    for (int xx = 0; xx < w; ++xx) {
      int dx = std::min(xx, w - 1 - xx), dy = std::min(yy, h - 1 - yy);
      if (shape == "chamfer" && dx + dy < cut) continue;
      if (shape == "round") {
        double a = (xx - cx) / (w / 2.0), b = (yy - cy) / (h / 2.0);
        if (a * a + b * b > 1.0) continue;
      }
      out.insert({xx, yy});
    }
  return out;
}

std::pair<PSet, std::string> shape_cells(const std::string& shape) {
  if (shape == "plain") return {rect(0, 0, R(11, 18), R(8, 12)), "room"};
  if (shape == "chamfer") { int w = R(12, 20), h = R(8, 13); return {room_cells(w, h, "chamfer"), "room"}; }
  if (shape == "round") { int w = R(13, 19), h = R(10, 14); return {room_cells(w, h, "round"), "room"}; }
  if (shape == "bays") {
    int w = R(13, 21), h = R(7, 10);
    PSet cells = rect(1, 1, w, h);
    for (int xx = 3; xx < w - 1; xx += 4) cells.insert({{xx, 0}, {xx + 1, 0}, {xx, h + 1}, {xx + 1, h + 1}});
    return {cells, "room"};
  }
  if (shape == "ell") {
    int a = R(16, 24), b = R(14, 20), t1 = R(6, 9), t2 = R(6, 9);
    PSet c = rect(0, 0, a, t1); add_all(c, rect(0, 0, t2, b)); return {c, "room"};
  }
  if (shape == "tee") {
    int a = R(18, 26), t = R(6, 8), s = R(7, 9), ln = R(8, 12);
    PSet c = rect(0, 0, a, t); add_all(c, rect(a / 2 - s / 2, t, s, ln)); return {c, "room"};
  }
  if (shape == "cross") {
    int a = R(16, 24), b = R(6, 8), c2 = R(7, 10), d = R(12, 17);
    PSet c = rect(0, (d - b) / 2, a, b); add_all(c, rect((a - c2) / 2, 0, c2, d)); return {c, "room"};
  }
  if (shape == "overlap") {
    int w1 = R(10, 16), h1 = R(7, 10), w2 = R(10, 16), h2 = R(7, 10);
    PSet c = rect(0, 0, w1, h1); int ox = R(4, 9), oy = R(3, 6); add_all(c, rect(ox, oy, w2, h2)); return {c, "room"};
  }
  if (shape == "zed") {
    int w1 = R(10, 14), h1 = R(7, 9), w2 = R(10, 14), h2 = R(7, 9);
    PSet c = rect(0, 0, w1, h1); int ox = w1 - R(4, 6), oy = h1 - R(2, 3); add_all(c, rect(ox, oy, w2, h2)); return {c, "room"};
  }
  if (shape == "chunky") {
    PSet cells;
    int cx = 0, cy = 0, n = R(3, 5);
    for (int i = 0; i < n; ++i) {
      int r = R(3, 5);
      for (int x = cx - r * 2; x <= cx + r * 2; ++x)
        for (int y = cy - r; y <= cy + r; ++y) {
          double a = (x - cx) / (r * 1.6), b = (y - cy) / double(r);
          if (a * a + b * b <= 1) cells.insert({x, y});
        }
      cx += R(-6, 7); cy += R(-3, 4);
    }
    return {cells, "room"};
  }
  if (shape == "pillars") {
    int w = R(20, 28), h = R(12, 16);
    PSet cells = room_cells(w, h, "chamfer");
    int step = chance() < .5 ? 4 : 5;
    for (int px = 3; px < w - 3; px += step) {
      if (chance() < .5) { cells.erase({px, 3}); cells.erase({px, h - 4}); }
      else for (int py = 3; py < h - 3; py += 4) cells.erase({px, py});
    }
    return {cells, "room"};
  }
  if (shape == "rotunda") {
    int w = R(17, 23), h = R(13, 17), cw = R(3, 5), ch = R(3, 4);
    PSet c = room_cells(w, h, "round"); remove_all(c, rect((w - cw) / 2, (h - ch) / 2, cw, ch)); return {c, "room"};
  }
  if (shape == "inner") {
    int w = R(17, 23), h = R(12, 15);
    PSet ring = rect(4, 3, w - 8, h - 6); remove_all(ring, rect(5, 4, w - 10, h - 8));
    std::vector<P> gaps = {{w / 2, 3}, {w / 2, h - 4}, {4, h / 2}, {w - 5, h / 2}};
    ring.erase(R_->pick(gaps));
    PSet c = rect(0, 0, w, h); remove_all(c, ring); return {c, "room"};
  }
  if (shape == "gallery") {
    int w = R(22, 30);
    PSet c = rect(0, 0, w, 7);
    for (int px = 3; px < w - 3; px += 4) c.erase({px, 3});
    return {c, "room"};
  }
  // cave
  int w = R(16, 26), h = R(10, 16);
  PSet s;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      if (std::hypot((x - w / 2.0) / (w / 2.0), (y - h / 2.0) / (h / 2.0)) < 1 + (chance() * .35 - .25)) s.insert({x, y});
  return {s, "cave"};
}

PSet turn(const PSet& cells) {
  int k = R(0, 3);
  bool flip = chance() < .5;
  PSet out;
  for (auto c : cells) {
    int x = flip ? -c.x : c.x, y = c.y;
    for (int i = 0; i < k; ++i) { int nx = -y; y = x; x = nx; }
    out.insert({x, y});
  }
  int mx = 1 << 30, my = 1 << 30;
  for (auto& c : out) { mx = std::min(mx, c.x); my = std::min(my, c.y); }
  PSet res;
  for (auto& c : out) res.insert({c.x - mx, c.y - my});
  return res;
}

using Sites = std::vector<std::pair<P, std::vector<P>>>;

Sites door_sites(const PSet& cells) {
  Sites sites;
  // sorted so a seed always gives the same level
  std::vector<P> sorted(cells.begin(), cells.end());
  std::sort(sorted.begin(), sorted.end(), [](P a, P b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
  for (auto d : D4) {
    P p{d.y, d.x};
    std::vector<P> good;
    for (auto c : sorted) {
      if (has(cells, {c.x + d.x, c.y + d.y})) continue;
      bool ok = true;
      for (int k : {-2, -1, 1, 2})
        if (!has(cells, {c.x + k * p.x, c.y + k * p.y}) || has(cells, {c.x + k * p.x + d.x, c.y + k * p.y + d.y})) { ok = false; break; }
      if (ok) good.push_back({c.x + d.x, c.y + d.y});
    }
    if (!good.empty()) sites.push_back({d, good});
  }
  return sites;
}

struct Hall { PSet cells; P end; P d; };

Hall hallway(P start, P d, const std::vector<int>& lengths, int width = 3) {
  Hall out;
  int half = width / 2;
  int x = start.x, y = start.y;
  for (size_t i = 0; i < lengths.size(); ++i) {
    for (int n = 0; n < lengths[i]; ++n) {
      for (int o = -half; o <= half; ++o) out.cells.insert({x + (d.y ? o : 0), y + (d.x ? o : 0)});
      x += d.x; y += d.y;
    }
    if (i + 1 < lengths.size()) {
      for (int a = -half; a <= half; ++a)
        for (int b = -half; b <= half; ++b) out.cells.insert({x - d.x + a, y - d.y + b});
      P nd = chance() < .5 ? P{d.y, d.x} : P{-d.y, -d.x};
      x = x - d.x + nd.x * (half + 1); y = y - d.y + nd.y * (half + 1);
      d = nd;
    }
  }
  out.end = {x, y};
  out.d = d;
  return out;
}

struct Level {
  int w, h;
  PMap<int> owner;
  PMap<std::string> floor;
  PSet narrow, edge;
  std::vector<char> near;
  Level(int w_, int h_) : w(w_), h(h_), near(w_ * h_, 0) {}

  void dig(P c, const std::string& kind, int who) {
    floor[c] = kind; owner[c] = who;
    for (int a = -1; a <= 1; ++a)
      for (int b = -1; b <= 1; ++b) {
        int x = c.x + a, y = c.y + b;
        if (x >= 0 && x < w && y >= 0 && y < h) near[y * w + x] = 1;
      }
    edge.erase(c);
    for (auto d : D4) {
      P n = c + d;
      if (!has(floor, n) && n.x >= 2 && n.x < w - 2 && n.y >= 2 && n.y < h - 2) edge.insert(n);
    }
  }
  void undig(P c) {
    owner.erase(c); floor.erase(c);
    for (auto d : D4) if (has(floor, c + d)) { edge.insert(c); break; }
    for (auto d : D4) {
      P n = c + d;
      if (!has(edge, n)) continue;
      bool any = false;
      for (auto e : D4) if (has(floor, n + e)) any = true;
      if (!any) edge.erase(n);
    }
  }
  int doubles(const PSet& area, const PMap<int>& extra = {}, const PSet& gone = {}, const PSet& narrow_x = {}) const {
    auto who = [&](P c) -> std::optional<int> {
      auto e = extra.find(c);
      if (e != extra.end()) return e->second;
      if (has(gone, c)) return std::nullopt;
      auto o = owner.find(c);
      if (o != owner.end()) return o->second;
      return std::nullopt;
    };
    auto in_nar = [&](P c) { return has(narrow, c) || has(narrow_x, c); };
    int out = 0;
    for (auto c : area) {
      if (who(c)) continue;
      for (auto d : DIRS8)
        for (int sgn : {1, -1}) {
          P a{c.x + sgn * d.x, c.y + sgn * d.y};
          auto wa = who(a);
          if (!wa) continue;
          P b{c.x - sgn * d.x, c.y - sgn * d.y};
          if (who(b)) continue;
          P cc{c.x - 2 * sgn * d.x, c.y - 2 * sgn * d.y};
          auto wc = who(cc);
          if (wc && (d.x == 0 || d.y == 0 || (*wa != *wc && !in_nar(a) && !in_nar(cc)))) ++out;
        }
    }
    return out;
  }
  bool free(const PSet& rel, int ox, int oy) const {
    int x0 = 2 - ox, y0 = 2 - oy, x1 = w - 3 - ox, y1 = h - 3 - oy;
    for (auto c : rel)
      if (c.x < x0 || c.x > x1 || c.y < y0 || c.y > y1 || near[(c.y + oy) * w + c.x + ox]) return false;
    return true;
  }
  bool fits(const PSet& placed, P door, const PSet& narrow_x) const {
    PSet nw = placed; nw.insert(door);
    auto in_nar = [&](P c) { return has(narrow, c) || has(narrow_x, c) || (!narrow_x.empty() && c == door); };
    for (auto a : nw)
      for (auto d : ALL8) {
        P b = a + d;
        if (has(nw, b) || has(owner, b)) continue;
        P c = b + d;
        if (has(nw, c) || has(owner, c)) continue;
        P e = c + d;
        if (has(nw, e)) {
          if (d.x == 0 || d.y == 0) return false;
          continue;
        }
        if (has(owner, e) && (d.x == 0 || d.y == 0 || (!in_nar(a) && !in_nar(e)))) return false;
      }
    return true;
  }
};

bool straight_door(const std::function<bool(P)>& is_floor, P c) {
  for (auto [d, p] : std::vector<std::pair<P, P>>{{{1, 0}, {0, 1}}, {{0, 1}, {1, 0}}}) {
    if (is_floor({c.x + d.x, c.y + d.y}) && is_floor({c.x - d.x, c.y - d.y})) {
      bool ok = true;
      for (int k : {-2, -1, 1, 2})
        if (is_floor({c.x + k * p.x, c.y + k * p.y})) ok = false;
      if (ok) return true;
    }
  }
  return false;
}

struct Piece {
  PSet cells, hall;
  Sites sites;
  std::string kind, shape;
  std::vector<std::pair<PSet, P>> arms;
  PSet narrow;
};

std::optional<Piece> design(std::vector<std::string>& deck, const Mood& mood) {
  if (chance() < mood.corridor) {
    Piece pc;
    pc.cells = rect(-1, -1, 3, 3);
    pc.kind = "hall"; pc.shape = "corridor";
    std::vector<P> dirs = D4;
    R_->shuffle(dirs);
    static const std::vector<int> counts = {2, 2, 3, 3, 4};
    int k = R_->pick(counts);
    for (int i = 0; i < k; ++i) {
      P d = dirs[i];
      Hall hw = hallway({d.x * 2, d.y * 2}, d, {R(4, 10)});
      add_all(pc.cells, hw.cells);
      pc.sites.push_back({d, {hw.end}});
      pc.arms.push_back({hw.cells, hw.end});
    }
    return pc;
  }
  if (deck.empty()) { deck = SHAPES; R_->shuffle(deck); }
  std::string shape = deck.back(); deck.pop_back();
  if (shape == "cave" && chance() > mood.caves) {
    if (!deck.empty()) { shape = deck.back(); deck.pop_back(); } else shape = "plain";
  }
  auto [raw, kind] = shape_cells(shape);
  Piece pc;
  pc.cells = turn(raw);
  pc.kind = kind; pc.shape = shape;
  pc.sites = door_sites(pc.cells);
  if (pc.sites.empty()) return std::nullopt;
  if (kind == "cave" || chance() < mood.hallway) {
    auto& site = R_->pick(pc.sites);
    P d = site.first;
    P s = R_->pick(site.second);
    Hall hw;
    if (!BIG.count(shape) && chance() < mood.narrow) {
      std::vector<int> l = {R(3, 6)};
      if (chance() < .4) l.push_back(R(2, 4));
      hw = hallway(s, d, l, 1);
      pc.narrow = hw.cells;
    } else {
      std::vector<int> l = {R(5, 12)};
      if (chance() < .45) l.push_back(R(5, 10));
      hw = hallway(s, d, l);
    }
    pc.hall = hw.cells;
    pc.sites = {{hw.d, {hw.end}}};
  }
  return pc;
}

std::vector<P> sorted(const PSet& s) {
  std::vector<P> v(s.begin(), s.end());
  std::sort(v.begin(), v.end(), [](P a, P b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
  return v;
}

PSet around(const std::vector<P>& cs, int r = 3) {
  PSet s;
  for (auto c : cs)
    for (int i = -r; i <= r; ++i)
      for (int j = -r; j <= r; ++j) s.insert({c.x + i, c.y + j});
  return s;
}

void prune(Level& lv, const std::vector<std::pair<int, std::vector<PSet>>>& arms_of, PSet& doors) {
  auto fill = [&](const PSet& gone) {
    if (lv.doubles(around(sorted(gone)), {}, gone) > 0) return false;
    for (auto c : gone) { lv.undig(c); doors.erase(c); }
    return true;
  };
  for (auto& [me, parts] : arms_of) {
    std::vector<const PSet*> used;
    for (auto& arm : parts) {
      bool u = false;
      for (auto c : arm) {
        for (auto d : D4) {
          P n = c + d;
          auto o = lv.owner.find(n);
          if ((o != lv.owner.end() && o->second != me) || has(doors, n)) { u = true; break; }
        }
        if (u) break;
      }
      if (u) used.push_back(&arm);
    }
    if (used.size() <= 1) {
      PSet mine;
      for (auto& [c, o] : lv.owner) if (o == me) mine.insert(c);
      fill(mine);
      continue;
    }
    for (auto& arm : parts) {
      if (std::find(used.begin(), used.end(), &arm) != used.end()) continue;
      PSet mine;
      for (auto c : arm) { auto o = lv.owner.find(c); if (o != lv.owner.end() && o->second == me) mine.insert(c); }
      fill(mine);
    }
  }
}

void loops(Level& lv, PSet& doors) {
  auto& floor = lv.floor;
  int n = 0;
  std::vector<P> cand = sorted(lv.edge);
  R_->shuffle(cand);
  for (auto c : cand) {
    if (n >= 8) break;
    for (P d : {P{1, 0}, P{0, 1}}) {
      P a = c + d, b{c.x - d.x, c.y - d.y};
      if (!has(floor, a) || !has(floor, b) || floor[a] == "hall" || floor[b] == "hall" || has(floor, c)) continue;
      if (!straight_door([&](P p) { return p == c || has(floor, p); }, c)) continue;
      auto w = walk(floor, a, {}, 31);
      auto it = w.find(b);
      if (it != w.end() && it->second <= 30) continue;
      if (lv.doubles(around({c}), {{c, -1 - n}}) > 0) continue;
      lv.dig(c, "hall", -1 - n);
      doors.insert(c);
      ++n;
      break;
    }
  }
}

void shortcuts(Level& lv, PSet& doors) {
  auto& floor = lv.floor;
  int tunnels = 0;
  std::vector<P> cand = sorted(lv.edge);
  R_->shuffle(cand);
  auto roomy = [&](P c) { auto f = floor.find(c); return f != floor.end() && (f->second == "room" || f->second == "cave"); };
  for (auto c0 : cand) {
    if (tunnels >= 4) break;
    std::vector<P> nb;
    for (auto d : D4) if (roomy({c0.x - d.x, c0.y - d.y})) nb.push_back(d);
    int touching = 0;
    for (auto d : D4) if (has(floor, c0 + d)) ++touching;
    if (nb.size() != 1 || touching != 1) continue;
    P d = nb[0];
    P src{c0.x - d.x, c0.y - d.y};
    std::vector<P> way;
    std::optional<P> end;
    for (int k = 0; k < 10; ++k) {
      P c{c0.x + d.x * k, c0.y + d.y * k};
      if (has(floor, c)) { end = c; break; }
      way.push_back(c);
    }
    if (!end || way.size() < 3 || !roomy(*end) || lv.owner[*end] == lv.owner[src]) continue;
    P p{d.y, d.x};
    bool side = false;
    for (auto v : way) if (has(floor, {v.x + p.x, v.y + p.y}) || has(floor, {v.x - p.x, v.y - p.y})) side = true;
    if (side) continue;
    PSet ws(way.begin(), way.end());
    auto isf = [&](P q) { return has(ws, q) || has(floor, q); };
    if (!(straight_door(isf, way.front()) && straight_door(isf, way.back()))) continue;
    auto w = walk(floor, src, {}, 31);
    auto it = w.find(*end);
    if (it != w.end() && it->second <= 30) continue;
    PMap<int> extra;
    for (auto v : way) extra[v] = -100 - tunnels;
    if (lv.doubles(around(way), extra, {}, ws) > 0) continue;
    lv.narrow.insert(way.begin(), way.end());
    for (auto v : way) lv.dig(v, "hall", -100 - tunnels);
    doors.insert(chance() < .5 ? way.front() : way.back());
    ++tunnels;
  }
}

void lakes(Level& lv, const PSet& doors) {
  auto& floor = lv.floor;
  for (int size : {16, 12, 9, 7}) {
    for (int t = 0; t < 10; ++t) {
      int cx = R(8, lv.w - 9), cy = R(6, lv.h - 7);
      double rx = size / 2.0, ry = size / 3.2;
      PSet blob;
      for (int x = int(cx - rx - 1); x < int(cx + rx + 2); ++x)
        for (int y = int(cy - ry - 1); y < int(cy + ry + 2); ++y)
          if (std::hypot((x - cx) / rx, (y - cy) / ry) < 1 + (chance() * .3 - .2)) blob.insert({x, y});
      std::vector<P> wet;
      for (auto c : sorted(blob)) if (has(floor, c) && !has(doors, c)) wet.push_back(c);
      if (wet.size() < 12) continue;
      PSet deep;
      for (auto c : wet) {
        bool ok = true;
        for (auto d : D4) if (!has(blob, c + d)) ok = false;
        for (int a = -1; a <= 1 && ok; ++a)
          for (int b = -1; b <= 1; ++b) if (!has(floor, {c.x + a, c.y + b})) ok = false;
        if (ok) deep.insert(c);
      }
      std::vector<P> rest;
      for (auto& [c, k] : floor) if (!has(deep, c)) rest.push_back(c);
      if (rest.empty() || walk(floor, rest[0], deep).size() != rest.size()) continue;
      for (auto c : wet) floor[c] = has(deep, c) ? "water" : "shallow";
      break;
    }
  }
}

void centre(Plan& plan) {
  int x0 = 1 << 30, x1 = -(1 << 30), y0 = 1 << 30, y1 = -(1 << 30);
  for (auto& [c, k] : plan.floor) { x0 = std::min(x0, c.x); x1 = std::max(x1, c.x); y0 = std::min(y0, c.y); y1 = std::max(y1, c.y); }
  int dx = (plan.w - x1 - 1 - x0) / 2, dy = (plan.h - y1 - 1 - y0) / 2;
  if (!dx && !dy) return;
  auto mv = [&](P c) { return P{c.x + dx, c.y + dy}; };
  PMap<std::string> f;
  for (auto& [c, k] : plan.floor) f[mv(c)] = k;
  plan.floor = std::move(f);
  PSet d, a;
  for (auto c : plan.doors) d.insert(mv(c));
  for (auto c : plan.arches) a.insert(mv(c));
  plan.doors = d; plan.arches = a;
  for (auto& r : plan.rooms) { PSet s; for (auto c : r.cells) s.insert(mv(c)); r.cells = s; }
  plan.up = mv(plan.up); plan.down = mv(plan.down);
}

}  // namespace

P Room::middle(const PMap<std::string>& floor) const {
  std::vector<P> fl;
  for (auto c : cells) if (has(floor, c)) fl.push_back(c);
  if (fl.empty()) fl.assign(cells.begin(), cells.end());
  double mx = 0, my = 0;
  for (auto c : fl) { mx += c.x; my += c.y; }
  mx /= fl.size(); my /= fl.size();
  P best = fl[0];
  double bd = 1e18;
  for (auto c : fl) {
    double d = (c.x - mx) * (c.x - mx) + (c.y - my) * (c.y - my);
    if (d < bd || (d == bd && (c.y < best.y || (c.y == best.y && c.x < best.x)))) { bd = d; best = c; }
  }
  return best;
}

PMap<int> walk(const PMap<std::string>& floor, P start, const PSet& block, int limit) {
  PMap<int> dist;
  dist[start] = 0;
  std::deque<P> q{start};
  while (!q.empty()) {
    P c = q.front(); q.pop_front();
    int dc = dist[c] + 1;
    if (limit && dc > limit) continue;
    for (auto d : D4) {
      P n = c + d;
      if (has(floor, n) && !has(dist, n) && !has(block, n)) { dist[n] = dc; q.push_back(n); }
    }
  }
  return dist;
}

Plan make(int w, int h, Rng& rnd, const std::string& mood_name, int rooms_wanted) {
  R_ = &rnd;
  std::string name = mood_name;
  if (!MOODS.count(name)) {
    std::vector<std::string> keys;
    for (auto& [k, v] : MOODS) keys.push_back(k);
    name = rnd.pick(keys);
  }
  const Mood mood = MOODS.at(name);
  Level lv(w, h);
  Plan plan;
  plan.w = w; plan.h = h; plan.mood = name;
  int want = rooms_wanted ? rooms_wanted : std::max(8, int(std::lround(20.0 * w * h / (120 * 70))));
  std::vector<std::string> deck;
  PSet doors, open_ends;
  std::vector<std::pair<int, std::vector<PSet>>> arms_of;
  auto& rooms = plan.rooms;
  std::optional<Piece> first;
  for (int i = 0; i < 50 && !first; ++i) {
    auto f = design(deck, {0, 0, mood.narrow, mood.caves, mood.near});
    if (!f) continue;
    int mx = 0, my = 0;
    for (auto c : f->cells) { mx = std::max(mx, c.x); my = std::max(my, c.y); }
    if (mx < w - 8 && my < h - 8) first = f;
  }
  while (!first) first = design(deck, mood);
  {
    int bw = 0, bh = 0;
    for (auto c : first->cells) { bw = std::max(bw, c.x + 1); bh = std::max(bh, c.y + 1); }
    int ox = std::max(3, (w - bw) / 2), oy = std::max(3, (h - bh) / 2);
    PSet placed;
    for (auto c : first->cells) placed.insert({c.x + ox, c.y + oy});
    for (auto c : sorted(placed)) lv.dig(c, first->kind, 0);
    rooms.push_back({0, placed, first->kind, first->shape});
  }
  int misses = 0;
  for (int attempt = 0; attempt < 700; ++attempt) {
    int real = 0;
    for (auto& r : rooms) if (r.kind != "hall") ++real;
    if (real >= want || misses > 120) break;
    auto piece = design(deck, mood);
    if (!piece) continue;
    PSet both = piece->cells;
    add_all(both, piece->hall);
    int me = int(rooms.size());
    std::vector<std::array<int, 4>> found;
    std::vector<P> edges = sorted(lv.edge);
    rnd.shuffle(edges);
    if (edges.size() > 500) edges.resize(500);
    for (auto wc : edges) {
      std::vector<P> nr;
      for (auto d : D4) if (has(lv.floor, {wc.x - d.x, wc.y - d.y})) nr.push_back(d);
      if (nr.size() != 1) continue;
      P d = nr[0];
      P back{-d.x, -d.y};
      const std::vector<P>* sl = nullptr;
      for (auto& s : piece->sites) if (s.first == back) sl = &s.second;
      if (!sl) continue;
      P onto{wc.x - d.x, wc.y - d.y};
      if (has(doors, onto)) continue;
      if (lv.floor[onto] == "hall" && (!has(open_ends, wc) || piece->kind == "hall" || !piece->hall.empty())) continue;
      P s = rnd.pick(*sl);
      int ox = wc.x - s.x, oy = wc.y - s.y;
      if (!lv.free(both, ox, oy)) continue;
      PSet placed;
      for (auto c : both) placed.insert({c.x + ox, c.y + oy});
      PSet nar;
      if (piece->kind != "hall") for (auto c : piece->narrow) nar.insert({c.x + ox, c.y + oy});
      if (lv.fits(placed, wc, nar) &&
          straight_door([&](P q) { return has(placed, q) || q == wc || has(lv.floor, q); }, wc)) {
        found.push_back({ox, oy, wc.x, wc.y});
        if (found.size() >= 8) break;
      }
    }
    if (found.empty()) { ++misses; continue; }
    misses = 0;
    std::array<int, 4> pick;
    if (chance() < mood.near) {
      double mx = 0, my = 0, ccx = 0, ccy = 0;
      for (auto& [c, k] : lv.floor) { mx += c.x; my += c.y; }
      mx /= lv.floor.size(); my /= lv.floor.size();
      for (auto c : both) { ccx += c.x; ccy += c.y; }
      ccx /= both.size(); ccy /= both.size();
      double bd = 1e18;
      for (auto& f : found) {
        double a = f[0] + ccx - mx, b = (f[1] + ccy - my) * 1.6;
        if (a * a + b * b < bd) { bd = a * a + b * b; pick = f; }
      }
    } else pick = rnd.pick(found);
    int ox = pick[0], oy = pick[1];
    P door{pick[2], pick[3]};
    PSet rc;
    for (auto c : piece->cells) rc.insert({c.x + ox, c.y + oy});
    for (auto c : sorted(rc)) lv.dig(c, piece->kind, me);
    for (auto c : sorted(piece->hall)) lv.dig({c.x + ox, c.y + oy}, "hall", me);
    lv.dig(door, "hall", me);
    if (piece->kind != "hall" && !piece->narrow.empty()) {
      for (auto c : piece->narrow) lv.narrow.insert({c.x + ox, c.y + oy});
      lv.narrow.insert(door);
    }
    doors.insert(door);
    rooms.push_back({me, rc, piece->kind, piece->shape});
    open_ends.erase(door);
    if (piece->kind == "hall") {
      std::vector<PSet> parts;
      for (auto& [a, e] : piece->arms) {
        PSet s;
        for (auto c : a) s.insert({c.x + ox, c.y + oy});
        parts.push_back(s);
        P pe{e.x + ox, e.y + oy};
        if (pe != door) open_ends.insert(pe);
      }
      arms_of.push_back({me, parts});
    }
  }
  prune(lv, arms_of, doors);
  loops(lv, doors);
  shortcuts(lv, doors);
  lakes(lv, doors);
  auto& floor = lv.floor;
  std::vector<P> pool;
  for (auto c : sorted(PSet([&] { PSet s; for (auto& [c, k] : floor) s.insert(c); return s; }())))
    if ((floor[c] == "hall" || floor[c] == "room") && !has(doors, c)) pool.push_back(c);
  rnd.shuffle(pool);
  for (size_t i = 0; i < std::min(pool.size() / 60 + 6, pool.size()); ++i) floor[pool[i]] = "rubble";
  std::vector<P> bad;
  for (auto d : doors) if (!straight_door([&](P q) { return has(floor, q); }, d)) bad.push_back(d);
  for (auto d : bad) doors.erase(d);
  plan.bad_doors = int(bad.size());
  {
    PSet area;
    for (int x = 2; x < w - 2; ++x) for (int y = 2; y < h - 2; ++y) area.insert({x, y});
    plan.doubles = lv.doubles(area);
  }
  plan.floor = floor;
  for (auto d : sorted(doors)) (chance() < .6 ? plan.doors : plan.arches).insert(d);
  for (auto& r : rooms) { PSet s; for (auto c : r.cells) if (has(plan.floor, c)) s.insert(c); r.cells = s; }
  std::vector<int> real;
  for (auto& r : rooms) if (r.kind != "hall" && !r.cells.empty()) real.push_back(r.id);
  int up_room = real[0];
  plan.up = rooms[up_room].middle(plan.floor);
  auto dist = walk(plan.floor, plan.up);
  std::vector<P> cut;
  for (auto& [c, k] : plan.floor) if (!has(dist, c)) cut.push_back(c);
  for (auto c : cut) { plan.floor.erase(c); plan.doors.erase(c); plan.arches.erase(c); }
  for (auto& r : rooms) { PSet s; for (auto c : r.cells) if (has(plan.floor, c)) s.insert(c); r.cells = s; }
  std::vector<int> real2;
  for (int id : real) if (!rooms[id].cells.empty()) real2.push_back(id);
  int far = real2.size() > 1 ? real2[1] : real2[0];
  int fard = -1;
  for (size_t i = 1; i < real2.size(); ++i) {
    int m = 0;
    for (auto c : rooms[real2[i]].cells) { auto it = dist.find(c); if (it != dist.end()) m = std::max(m, it->second); }
    if (m > fard) { fard = m; far = real2[i]; }
  }
  plan.down = rooms[far].middle(plan.floor);
  auto entries = [&](const PSet& cs) {
    PSet ring;
    for (auto c : cs) for (auto d : D4) if (!has(cs, c + d)) ring.insert(c + d);
    int n = 0;
    for (auto c : ring) if (has(plan.floor, c)) ++n;
    return n;
  };
  for (int id : real2)
    if (id != up_room && id != far && rooms[id].kind == "room" && entries(rooms[id].cells) <= 3) plan.themes.push_back(id);
  std::sort(plan.themes.begin(), plan.themes.end(), [&](int a, int b) { return rooms[a].cells.size() > rooms[b].cells.size(); });
  centre(plan);
  return plan;
}

}  // namespace k5
