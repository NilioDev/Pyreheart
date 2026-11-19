// The Deep: the last two maps, where there are no stairs any more. Black crust plates float on one sea of lava;
// the veins between them are too wide and too hot to cross. Plates are joined by narrow ridges of crust into a maze,
// a tree with dead ends, from a landing on one edge of the map to the way on at another. The second map ends at the
// Ashen King's island: a round slab of a colder, cut stone with a ring of pillars and the throne at the back.
#include <deque>
#include <map>
#include <set>

#include "game.hpp"

using namespace data;

namespace {

constexpr int PX = 13, PY = 6;   // the spacing of the plates
constexpr int A = 13;            // the radius of the island
const double PI = 3.14159265358979323846;

enum Kind { K_OCEAN, K_VEIN, K_POOL, K_PLATE, K_NECK, K_ARENA };
struct Cell { Kind k = K_OCEAN; int plate = -1; int a = -1, b = -1; int role = 0; };  // a, b: the plates a neck joins
enum Role { R_FLOOR = 1, R_RIM, R_PILLAR, R_DAIS };

struct Maker {
  Rng& rnd;
  int GW = W / PX, GH = H / PY;
  std::vector<Cell> K = std::vector<Cell>(W * H);
  std::map<int, P> seeds;
  std::map<int, std::vector<P>> pc;  // plate -> its cells
  Cell& at(int x, int y) { return K[y * W + x]; }
  bool in(int x, int y) const { return x >= 0 && y >= 0 && x < W && y < H; }
  int id(int i, int j) const { return j * 64 + i; }
  P gp(int pl) const { return {pl % 64, pl / 64}; }

  P seed(int i, int j) {
    auto it = seeds.find(id(i, j));
    if (it != seeds.end()) return it->second;
    P s{i * PX + rnd.range(1, PX - 2), j * PY + rnd.range(1, PY - 2)};
    seeds[id(i, j)] = s;
    return s;
  }

  // the two nearest plate seeds, squashed so plates come out wider than tall
  void nearest(int x, int y, double& d1, int& p1, double& d2, int& p2) {
    int i0 = x / PX, j0 = y / PY;
    d1 = d2 = 1e9; p1 = p2 = -1;
    for (int j = j0 - 1; j <= j0 + 1; ++j)
      for (int i = i0 - 1; i <= i0 + 1; ++i) {
        if (i < 0 || j < 0) continue;
        P s = seed(i, j);
        double d = std::hypot(x - s.x, (y - s.y) * 2.2);
        if (d < d1) { d2 = d1; p2 = p1; d1 = d; p1 = id(i, j); }
        else if (d < d2) { d2 = d; p2 = id(i, j); }
      }
  }

  bool touches_other(int X, int Y, int p, int q) {
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        if (!in(X + dx, Y + dy)) continue;
        auto& c = at(X + dx, Y + dy);
        if (c.k == K_PLATE && c.plate != p && c.plate != q) return true;
        if (c.k == K_NECK && !(c.a == p && c.b == q)) return true;
      }
    return false;
  }

  struct Step { int x, y; bool h; };
  // the shortest straight-ish line between two plates that passes no third plate
  std::vector<Step> clean_line(int p, int q, int limit) {
    struct Pair { int d, ax, ay, bx, by; };
    std::vector<Pair> pairs;
    auto& A_ = pc[p];
    auto& B_ = pc[q];
    for (size_t i = 0; i < A_.size(); i += 2)
      for (size_t j = 0; j < B_.size(); j += 2)
        pairs.push_back({std::abs(A_[i].x - B_[j].x) + std::abs(A_[i].y - B_[j].y), A_[i].x, A_[i].y, B_[j].x, B_[j].y});
    std::stable_sort(pairs.begin(), pairs.end(), [](const Pair& a, const Pair& b) { return a.d < b.d; });
    for (size_t k = 0; k < pairs.size() && k < 60; ++k) {
      auto& pr = pairs[k];
      if (pr.d > limit) break;
      for (int pass = 0; pass < 2; ++pass) {
        bool hfirst = (std::abs(pr.bx - pr.ax) >= std::abs(pr.by - pr.ay)) == (pass == 0);
        int x = pr.ax, y = pr.ay;
        std::vector<Step> path;
        while (x != pr.bx || y != pr.by) {
          if ((hfirst && x != pr.bx) || y == pr.by) { x += pr.bx > x ? 1 : -1; path.push_back({x, y, true}); }
          else { y += pr.by > y ? 1 : -1; path.push_back({x, y, false}); }
        }
        bool ok = true;
        for (auto& s : path) {
          auto& c = at(s.x, s.y);
          if (!(c.k == K_VEIN || (c.k == K_PLATE && (c.plate == p || c.plate == q)))) { ok = false; break; }
        }
        for (size_t i = 0; ok && i + 1 < path.size(); ++i) if (touches_other(path[i].x, path[i].y, p, q)) ok = false;
        if (ok) return path;
      }
    }
    return {};
  }
};

P edge_pt(char sd, int px, int py) {
  switch (sd) {
    case 'S': return {px, H - 1};
    case 'N': return {px, 0};
    case 'W': return {0, py};
    default: return {W - 1, py};
  }
}
P step_in(char sd) { return sd == 'S' ? P{0, -1} : sd == 'N' ? P{0, 1} : sd == 'W' ? P{1, 0} : P{-1, 0}; }

}  // namespace

bool Game::build_deep(Floor& fl, char entry, bool last, char& exit_side) {
  Maker m{rnd};
  int GW = m.GW, GH = m.GH;
  std::set<int> pools;
  for (int i = 0; i < 14; ++i) pools.insert(m.id(rnd.range(1, GW - 1), rnd.range(1, GH - 2)));
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x) {
      auto& c = m.at(x, y);
      int edge_d = std::min({x, y * 2, W - 1 - x, (H - 1 - y) * 2});
      double shore = 7 + 3 * std::sin(x * .21 + y * .13) + 2 * std::sin(x * .07 - y * .4);
      if (edge_d < shore) { c.k = K_OCEAN; continue; }
      double d1, d2;
      int p1, p2;
      m.nearest(x, y, d1, p1, d2, p2);
      if (pools.count(p1) && d1 < 9) { c.k = K_POOL; continue; }
      if (d2 - d1 < 2.4) { c.k = (pools.count(p1) || pools.count(p2)) ? K_POOL : K_VEIN; continue; }
      c.k = K_PLATE; c.plate = p1;
    }
  // no slipping through corners: where two plates touch only diagonally, the vein fills the gap
  for (bool changed = true; changed;) {
    changed = false;
    for (int y = 2; y < H - 3; ++y)
      for (int x = 2; x < W - 3; ++x)
        for (int dx : {1, -1}) {
          auto &p = m.at(x, y), &q = m.at(x + dx, y + 1);
          if (p.k == K_PLATE && q.k == K_PLATE && p.plate != q.plate)
            for (P c : {P{x + dx, y}, P{x, y + 1}})
              if (m.at(c.x, c.y).k == K_PLATE) { m.at(c.x, c.y).k = K_VEIN; changed = true; }
        }
  }
  // the island of the Ashen King, with a moat of open lava round it
  int ax0 = W / 2, ay0 = A + 5;
  std::set<std::pair<int, int>> ring;
  for (int k = 0; k < 12; ++k)
    ring.insert({int(std::lround(9.5 * std::cos(PI * k / 6 + PI / 12))), int(std::lround(9.5 * std::sin(PI * k / 6 + PI / 12)))});
  auto role_at = [&](int dx, int dy) {
    double r = std::hypot(dx, dy);
    if (r > A + .5) return 0;
    if (r > A - .6) return int(R_RIM);
    if (dy <= -A + 4 && std::abs(dx) <= 3) return int(R_DAIS);
    if (ring.count({dx, dy}) && !(dy > 0 && std::abs(dx) < 3)) return int(R_PILLAR);
    return int(R_FLOOR);
  };
  if (last)
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x) {
        int dx = x - ax0, dy = y - ay0;
        auto& c = m.at(x, y);
        if (int r = role_at(dx, dy)) { c.k = K_ARENA; c.role = r; c.plate = -1; continue; }
        if (std::hypot(dx, dy) < A + 5 && c.k != K_OCEAN) { c.k = K_VEIN; c.plate = -1; }
      }
  // every plate keeps only its biggest piece; scraps cut off by veins sink
  {
    std::vector<char> seen(W * H, 0);
    std::map<int, std::vector<std::vector<P>>> pieces;
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x) {
        if (m.at(x, y).k != K_PLATE || seen[y * W + x]) continue;
        int pl = m.at(x, y).plate;
        std::vector<P> comp{{x, y}};
        seen[y * W + x] = 1;
        for (size_t i = 0; i < comp.size(); ++i)
          for (P d4 : {P{1, 0}, P{-1, 0}, P{0, 1}, P{0, -1}}) {
            P c = comp[i] + d4;
            if (m.in(c.x, c.y) && !seen[c.y * W + c.x] && m.at(c.x, c.y).k == K_PLATE && m.at(c.x, c.y).plate == pl) {
              seen[c.y * W + c.x] = 1;
              comp.push_back(c);
            }
          }
        pieces[pl].push_back(comp);
      }
    for (auto& [pl, comps] : pieces) {
      std::stable_sort(comps.begin(), comps.end(), [](auto& a, auto& b) { return a.size() > b.size(); });
      for (size_t i = 1; i < comps.size(); ++i) for (auto c : comps[i]) m.at(c.x, c.y).k = K_VEIN;
      if (comps[0].size() >= 12) m.pc[pl] = comps[0];
      else for (auto c : comps[0]) m.at(c.x, c.y).k = K_VEIN;
    }
  }
  // which plates could be joined by a clean neck: only neighbours on the grid, so no two necks cross
  std::map<std::pair<int, int>, std::vector<Maker::Step>> cand;
  std::map<int, std::vector<int>> nbr;
  for (auto& [p, cp] : m.pc)
    for (auto& [q, cq] : m.pc) {
      P a = m.gp(p), b = m.gp(q);
      if (p < q && std::abs(a.x - b.x) + std::abs(a.y - b.y) == 1) {
        auto path = m.clean_line(p, q, 12);
        if (!path.empty()) { cand[{p, q}] = path; nbr[p].push_back(q); nbr[q].push_back(p); }
      }
    }
  // the biggest joined group of plates is the level; the rest sinks
  std::set<int> level;
  {
    std::set<int> gseen;
    for (auto& [p, ns] : nbr) {
      if (gseen.count(p)) continue;
      std::set<int> g{p};
      std::vector<int> st{p};
      gseen.insert(p);
      while (!st.empty()) {
        int a = st.back(); st.pop_back();
        for (int b : nbr[a]) if (!gseen.count(b)) { gseen.insert(b); g.insert(b); st.push_back(b); }
      }
      if (g.size() > level.size()) level = g;
    }
  }
  if (level.size() < 8) return false;
  for (auto it = m.pc.begin(); it != m.pc.end();) {
    if (!level.count(it->first)) { for (auto c : it->second) m.at(c.x, c.y).k = K_VEIN; it = m.pc.erase(it); }
    else ++it;
  }
  auto edge_cell = [&](char sd, int pl) {  // the plate cell nearest an edge
    auto& cs = m.pc[pl];
    return *std::min_element(cs.begin(), cs.end(), [&](P a, P b) {
      switch (sd) {
        case 'S': return a.y > b.y;
        case 'N': return a.y < b.y;
        case 'W': return a.x < b.x;
        default: return a.x > b.x;
      }
    });
  };
  // a straight tongue from the edge must reach this plate without passing any other crust
  auto landing_ok = [&](char sd, int pl) {
    P c = edge_cell(sd, pl), e = edge_pt(sd, c.x, c.y), d = step_in(sd);
    int x = e.x, y = e.y;
    while (m.in(x, y) && !(m.at(x, y).k == K_PLATE && m.at(x, y).plate == pl)) {
      for (int o = -2; o <= 2; ++o) {
        int X = sd == 'N' || sd == 'S' ? x + o : x, Y = sd == 'N' || sd == 'S' ? y : y + o;
        if (m.in(X, Y) && ((m.at(X, Y).k == K_PLATE && m.at(X, Y).plate != pl) || m.at(X, Y).k == K_ARENA)) return false;
      }
      x += d.x; y += d.y;
    }
    return m.in(x, y);
  };
  auto side_cands = [&](char sd) {
    std::vector<int> v;
    for (int pl : level) if (landing_ok(sd, pl)) v.push_back(pl);
    return v;
  };
  auto starts = side_cands(entry);
  if (starts.empty()) return false;
  int start = rnd.pick(starts);
  // the goal: the plate below the island's gate, or a plate with a clean landing on another edge
  std::vector<int> goals;
  std::map<int, char> gside;
  int goal_a = -1;
  if (last) {
    int x = ax0, y = ay0 + A + 1;
    while (y < H && !(m.at(x, y).k == K_PLATE && level.count(m.at(x, y).plate))) ++y;
    if (y >= H) return false;
    goal_a = m.at(x, y).plate;
    goals.push_back(goal_a);
  } else {
    for (char sd : {'N', 'W', 'E'}) {
      if (sd == entry) continue;
      for (int pl : side_cands(sd)) if (pl != start && !gside.count(pl)) { goals.push_back(pl); gside[pl] = sd; }
    }
  }
  if (goals.empty()) return false;
  // a winding maze: many depth-first walks from the entry, keeping the one whose way out is nearest the wanted length
  std::map<int, std::pair<double, double>> cen;
  for (auto& [pl, cs] : m.pc) {
    double sx = 0, sy = 0;
    for (auto c : cs) { sx += c.x; sy += c.y; }
    cen[pl] = {sx / cs.size(), sy / cs.size()};
  }
  auto hop = [&](int p, int q) { return std::max(std::abs(cen[p].first - cen[q].first), std::abs(cen[p].second - cen[q].second)); };
  const double target = 250;
  double best_sc = 1e9;
  std::vector<std::pair<int, int>> chosen;
  int goal = -1;
  for (int attempt = 0; attempt < 300; ++attempt) {
    std::set<int> seen_p{start};
    std::vector<int> st{start};
    std::vector<std::pair<int, int>> ch;
    std::map<int, double> depth{{start, 0}};
    while (!st.empty()) {
      int p = st.back();
      std::vector<int> opts;
      for (int q : nbr[p]) if (!seen_p.count(q)) opts.push_back(q);
      if (opts.empty()) { st.pop_back(); continue; }
      int q = rnd.pick(opts);
      seen_p.insert(q); ch.push_back({p, q}); st.push_back(q); depth[q] = depth[p] + hop(p, q);
    }
    for (int g : goals) {
      if (!depth.count(g)) continue;
      double sc = std::abs(depth[g] - target);
      if (sc < best_sc) { best_sc = sc; chosen = ch; goal = g; }
    }
  }
  if (goal < 0) return false;
  // carve the necks: a clean core two wide, roughened where it cannot touch anything else
  for (auto [p0, q0] : chosen) {
    int p = std::min(p0, q0), q = std::max(p0, q0);
    auto& path = cand[{p, q}];
    int n = int(path.size());
    auto put_neck = [&](int X, int Y) { auto& c = m.at(X, Y); c.k = K_NECK; c.a = p; c.b = q; };
    for (int i = 0; i < n; ++i) {
      auto& s = path[i];
      for (int o : {0, 1}) {
        int X = s.h ? s.x : s.x + o, Y = s.h ? s.y + o : s.y;
        if (m.in(X, Y) && m.at(X, Y).k == K_VEIN && (o == 0 || !m.touches_other(X, Y, p, q))) put_neck(X, Y);
      }
      bool flare = i < 2 || i > n - 3;
      std::vector<int> offs = flare ? std::vector<int>{-1, 2} : std::vector<int>{-1};
      for (int o : offs)
        if (rnd.real() < (flare ? .7 : .35)) {
          int X = s.h ? s.x : s.x + o, Y = s.h ? s.y + o : s.y;
          if (m.in(X, Y) && m.at(X, Y).k == K_VEIN && !m.touches_other(X, Y, p, q)) put_neck(X, Y);
        }
    }
  }
  // landings: a tongue of crust from the map edge straight into its plate; edge cells of the way out are marked
  std::vector<P> edge_cells;
  auto landing = [&](char sd, int pl, bool out) {
    P c = edge_cell(sd, pl), e = edge_pt(sd, c.x, c.y), d = step_in(sd);
    int x = e.x, y = e.y;
    bool first = true;
    while (m.in(x, y) && !(m.at(x, y).k == K_PLATE && m.at(x, y).plate == pl)) {
      for (int o = -1; o <= 1; ++o) {
        int X = sd == 'N' || sd == 'S' ? x + o : x, Y = sd == 'N' || sd == 'S' ? y : y + o;
        if (!m.in(X, Y)) continue;
        auto& k = m.at(X, Y);
        if (k.k == K_OCEAN || k.k == K_VEIN || k.k == K_POOL) { k.k = K_NECK; k.a = k.b = -2; }
        if (out && first) edge_cells.push_back({X, Y});
      }
      first = false;
      x += d.x; y += d.y;
    }
    return e;
  };
  P in_pt = landing(entry, start, false);
  P sd_in = step_in(entry);
  fl.up = {in_pt.x + sd_in.x * 2, in_pt.y + sd_in.y * 2};
  P throne{ax0, ay0 - A + 1};
  if (last) {
    int x = ax0, y = ay0 + A + 1;
    while (y < H && !(m.at(x, y).k == K_PLATE && m.at(x, y).plate == goal_a)) {
      for (int o = -1; o <= 1; ++o) {
        auto& k = m.at(x + o, y);
        if (k.k == K_OCEAN || k.k == K_VEIN || k.k == K_POOL) { k.k = K_NECK; k.a = k.b = -2; }
      }
      ++y;
    }
    fl.down = {ax0, ay0 - A + 2};
    exit_side = 0;
  } else {
    exit_side = gside[goal];
    P e = landing(exit_side, goal, true);
    fl.down = e;
  }
  // into tiles
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x) {
      auto& c = m.at(x, y);
      int t = LAVA;
      if (c.k == K_PLATE) t = CRUST;
      else if (c.k == K_NECK) t = NECK;
      else if (c.k == K_ARENA) {
        int dx = x - ax0, dy = y - ay0;
        double r = std::hypot(dx, dy);
        t = c.role == R_PILLAR ? PILLAR : c.role == R_DAIS ? DAIS : BASALT;
        if (c.role == R_FLOOR && std::abs(r - 5.5) < .5 && (x + y) % 2 == 0) t = RUNE;
      }
      fl.t[y * W + x] = t;
    }
  for (auto c : edge_cells) fl.t[c.y * W + c.x] = EDGE;
  if (last) fl.t[throne.y * W + throne.x] = THRONE;
  fl.w = W; fl.h = H;
  fl.deep = true;
  return whole(fl);
}
