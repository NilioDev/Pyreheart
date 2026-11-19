// How the floors look: worked stone, light and shadow, the dressing of the rooms, and small particles over the glyphs.
// The dark is only lifted by living flame: torches, candles, fire and lava, each flickering on its own, and the
// light every hero carries. Everything is a glyph of the font, except the particles: dust in the light, embers and
// smoke over a flame, a drip in a puddle, the dust of a grinding tomb lid.
#include <libtcod/renderer_sdl2.h>

#include <cstring>

#include "client.hpp"
#include "data.hpp"
#include "game.hpp"

using namespace data;

// libtcod carries lodepng; its PNG writer saves the screenshots
extern "C" unsigned lodepng_encode32_file(const char* filename, const unsigned char* image, unsigned w, unsigned h);

namespace {
int scale(int c, double f) {
  auto ch = [&](int v) { return std::clamp(int(v * f), 0, 255); };
  return (ch((c >> 16) & 255) << 16) | (ch((c >> 8) & 255) << 8) | ch(c & 255);
}
int mixc(int a, int b, double f) {
  f = std::clamp(f, 0.0, 1.0);
  auto m = [&](int s) { int x = (a >> s) & 255, y = (b >> s) & 255; return std::clamp(int(x + (y - x) * f), 0, 255) << s; };
  return m(16) | m(8) | m(0);
}
uint32_t hash(int x, int y, int s = 0) {
  uint32_t h = uint32_t(x) * 73856093u ^ uint32_t(y) * 19349663u ^ uint32_t(s) * 83492791u;
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  return h;
}
const double PI = 3.14159265358979323846;
double nz(int k) { return (hash(k, k * 7 + 3, 11) % 10000) / 10000.0; }
// smooth value noise, so moss and stains come in clumps instead of salt and pepper
double noise(int x, int y, double sc, int s) {
  double gx = x / sc, gy = y / sc;
  int x0 = int(std::floor(gx)), y0 = int(std::floor(gy));
  double fx = gx - x0, fy = gy - y0;
  auto v = [&](int i, int j) { return (hash(i, j, s) % 1000) / 1000.0; };
  fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
  double a = v(x0, y0) + (v(x0 + 1, y0) - v(x0, y0)) * fx, b = v(x0, y0 + 1) + (v(x0 + 1, y0 + 1) - v(x0, y0 + 1)) * fx;
  return a + (b - a) * fy;
}
double flick(double t, int seed) { return .86 + .08 * std::sin(t * 4.1 + seed * 1.7) + .06 * std::sin(t * 9.7 + seed * 3.1); }
const int FIREC[] = {0xFFF4C0, 0xFFD050, 0xFFA030, 0xFF7020, 0xE04818, 0xA02810};
bool tombish(int t) { return t == TOMB || t == CRACKED || t == OPENTOMB; }
bool bierish(int t) { return t == BIER || t == CORPSE || t == RESTLESS; }
}  // namespace

void Client::draw_tiles() {
  auto& s = snap_;
  int fl = s["floor"];
  const std::string& map = maps_[fl];
  auto& seen = seen_[fl];
  const std::string& vis = s["vis"].get_ref<const std::string&>();
  auto& act = ACTS[std::clamp(s["act"].get<int>(), 0, ZONES - 1)];
  int actn = s["act"].get<int>();
  bool rest = s.value("rest", false);
  double tt = now();
  emits_.clear();
  auto tile = [&](int x, int y) { return x < 0 || y < 0 || x >= W || y >= H ? int(WALL) : tile_of(map[y * W + x]); };
  int x0 = std::max(0, camx_), y0 = std::max(0, camy_), x1 = std::min(W, camx_ + vw_), y1 = std::min(H, camy_ + vh_);
  // how far each lava cell lies from land: the lava is one mass, crusted dark at the shore and bright far out
  if (ldist_floor_ != fl || ldist_map_ != map) {
    ldist_floor_ = fl; ldist_map_ = map;
    ldist_.assign(W * H, 0);
    std::vector<int> q;
    for (int i = 0; i < W * H; ++i) {
      if (tile_of(map[i]) == LAVA) ldist_[i] = 99;
      else q.push_back(i);
    }
    for (size_t k = 0; k < q.size(); ++k) {
      int i = q[k];
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
          int x = i % W + dx, y = i / W + dy;
          if (x < 0 || y < 0 || x >= W || y >= H || ldist_[y * W + x] <= ldist_[i] + 1) continue;
          ldist_[y * W + x] = ldist_[i] + 1;
          q.push_back(y * W + x);
        }
    }
  }
  double glow = .35 + .2 * std::sin(tt * .8);

  // the light: a dim base, pools round every flame, and each hero's own light
  static const bool whole_view = SDL_getenv("PYREHEART_WHOLE") != nullptr;  // a screenshot of the whole map
  double base = rest ? .9 : whole_view ? .8 : .6;
  std::vector<float> L(W * H, float(base)), warm(W * H, 0.f), cold(W * H, 0.f);
  auto pool = [&](double cx, double cy, double r, double a, double w, bool cool = false) {
    int ax = std::max(x0, int(cx - r - 1)), bx = std::min(x1 - 1, int(cx + r + 1));
    int ay = std::max(y0, int(cy - r - 1)), by = std::min(y1 - 1, int(cy + r + 1));
    for (int y = ay; y <= by; ++y)
      for (int x = ax; x <= bx; ++x) {
        double d = std::hypot(x - cx, y - cy);
        if (d > r) continue;
        double k = 1 - d / (r + 1), sm = k * k * (3 - 2 * k);
        int i = y * W + x;
        L[i] = std::max(L[i], float(base + (a - base) * sm));
        if (cool) cold[i] = std::max(cold[i], float(w * k));
        else warm[i] = std::max(warm[i], float(w * k));
      }
  };
  for (int y = std::max(0, y0 - 6); y < std::min(H, y1 + 6); ++y)
    for (int x = std::max(0, x0 - 6); x < std::min(W, x1 + 6); ++x) {
      int t = tile(x, y), i = y * W + x;
      if (!seen[i]) continue;
      int sd = int(hash(x, y) % 97);
      double f = flick(tt, sd);
      if (t == TORCH) {
        // the flame lights the floor in front of the wall it hangs on
        double cx = x, cy = y;
        for (P d4 : {P{0, 1}, P{0, -1}, P{1, 0}, P{-1, 0}})
          if (!blocks(tile(x + d4.x, y + d4.y))) { cx += d4.x * .6; cy += d4.y * .6; break; }
        pool(cx, cy, 4.5 * f, 1.05 * f, .8);
        if (vis[i] == '1') { emits_.push_back({float(x), float(y), 't', sd}); emits_.push_back({float(cx), float(cy), 'd', sd}); }
      } else if (t == CANDLE) {
        pool(x, y, 3.5 * f, 1.0 * f, .6);
        if (vis[i] == '1') emits_.push_back({float(x), float(y), 'c', sd});
      } else if (t == LAVA) {
        if (ldist_[i] <= 2) pool(x, y, 2.2, .95, .9);
        if (vis[i] == '1' && ldist_[i] >= 2 && hash(x, y, 8) % 41 == 0) emits_.push_back({float(x), float(y), 'l', sd});
      } else if (t == SHALLOW && vis[i] == '1' && hash(x, y, 5) % 19 == 0) {
        emits_.push_back({float(x), float(y), 'w', sd});
      } else if (t == CRACKED && vis[i] == '1') {
        emits_.push_back({float(x), float(y), 's', sd});
      } else if (t == HEARTH || t == BRAZIER) {
        // the forge still burns; the King's brazier too
        pool(x, y, (t == HEARTH ? 5.5 : 6) * f, 1.25 * f, 1.);
        if (vis[i] == '1') emits_.push_back({float(x), float(y), 'f', sd});
        if (vis[i] == '1' && t == HEARTH && hash(x, y, 4) % 2) emits_.push_back({float(x), float(y), 'k', sd});
      } else if (t == VENT && vis[i] == '1') {
        emits_.push_back({float(x), float(y), 'v', sd});
      } else if (t == MOSSWALL) {
        // the moss glows cold, and breathes out pale spores
        pool(x, y, 5 * (.95 + .05 * std::sin(tt * .7 + sd)), 1.0, .9, true);
        if (vis[i] == '1') emits_.push_back({float(x), float(y), 'p', sd});
      } else if (t == EGG) {
        pool(x, y, 2.5, .85, .5, true);
      } else if (t == LAMP) {
        pool(x, y, 5.5 * f, 1.15 * f, 1.);
        if (vis[i] == '1') emits_.push_back({float(x), float(y), 'c', sd});
      } else if (t == SHAFT) {
        // the heat of the dead city below, and ash rising out of the dark
        pool(x + .5, y + .5, 4.5 * f, 1.1 * f, 1.);
        if (vis[i] == '1' && hash(x, y, 3) % 2) emits_.push_back({float(x), float(y), 'h', sd});
      } else if (t == SPOUT && vis[i] == '1') {
        emits_.push_back({float(x), float(y), 'z', sd});
      } else if (t == FLOATER && vis[i] == '1') {
        emits_.push_back({float(x), float(y), 'u', sd});
      } else if ((t == SLUDGE || t == MIRE) && vis[i] == '1') {
        if (hash(x, y, 15) % 11 == 0) emits_.push_back({float(x), float(y), 'm', sd});
      } else if (actn == CISTERN && (t == FLOOR || t == GRATE) && vis[i] == '1') {
        if (hash(x, y, 16) % 47 == 0) emits_.push_back({float(x), float(y), 'r', sd});
      } else if ((t == CRUST || t == NECK) && vis[i] == '1') {
        // smoke seeping out of the crust, red embers drifting in loops, ash falling
        if (hash(x, y, 12) % 37 == 0) emits_.push_back({float(x), float(y), 'g', sd});
        if (hash(x, y, 13) % 29 == 0) emits_.push_back({float(x), float(y), 'e', sd});
        if (hash(x, y, 14) % 61 == 0) emits_.push_back({float(x), float(y), 'a', sd});
      }
    }
  if (s.contains("ground"))
    for (auto& g : s["ground"])
      if (g[2] == 0) {
        int gx = g[0], gy = g[1];
        pool(gx, gy, 3 * flick(tt, gx * 7 + gy), 1.1, .9);
        if (vis[gy * W + gx] == '1') emits_.push_back({float(gx), float(gy), 'f', gx * 7 + gy});
      }
  for (auto& h : s["heroes"])
    if (!h.value("gone", false) && !h.value("dead", false)) {
      int hx = h["x"].get<int>(), hy = h["y"].get<int>();
      pool(hx, hy, 6, 1.25, 0);
      if (tile(hx, hy) == GRATE) emits_.push_back({float(hx), float(hy), 'q', hx * 31 + hy});  // something follows below
    }

  // a tomb or corpse that is grinding open: its lid or body shudders
  std::vector<P> grinding;
  for (auto& f : fx_)
    if (f.kw.value("k", "") == "dust" && !f.kw.value("fall", false)) grinding.push_back({f.kw.value("x", 0), f.kw.value("y", 0)});
  auto shudder = [&](int x, int y) {
    for (auto g : grinding) if (std::abs(g.x - x) <= 2 && std::abs(g.y - y) <= 1) return true;
    return false;
  };

  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x) {
      int X = x - camx_ + vx_, Y = y - camy_ + vy_;
      int i = y * W + x;
      if (!seen[i]) continue;
      int t = tile(x, y);
      auto& td = TILES[t];
      int col = td.col >= 0 ? td.col : t == WALL ? act.wall : act.floor;
      int ch = td.ch, fg = col, bg = 0;
      bool bright = false, keep = false;  // a flame is never dimmed; doors and stairs never fall below full light
      uint32_t hh = hash(x, y);
      auto floor_bg = [&]() {
        // worked slabs, three by two, each a slightly different shade
        return scale(act.floor, .36 + .14 * ((hash(x / 3, y / 2, 1) % 6) / 6.0));
      };
      switch (t) {
        case WALL: {
          // laid blocks in courses, with dark joints; now and then a skull niche facing a room (the crypt)
          int course = (x + (y % 2) * 2) / 4;
          bg = scale(col, .22 + .08 * ((hash(course, y, 2) % 7) / 7.0));
          if ((x + (y % 2) * 2) % 4) { ch = 0x2593; fg = scale(col, .4 + .1 * noise(x, y, 3, 4)); }
          else { ch = 0x2502; fg = scale(col, .16); }
          if (actn == CISTERN) {
            fg = scale(fg, .75);
            if (noise(x, y, 2.5, 4) > .66) fg = mixc(fg, 0x2E4A30, .45);  // slime running down the wet stone
          }
          if (actn == 0 && hh % 41 == 0 && !blocks(tile(x, y + 1)) && blocks(tile(x - 1, y)) && blocks(tile(x + 1, y))) {
            ch = 0x3A9; fg = 0xC8BCA0; bg = 0x14120F;
          } else if (hh % 23 == 0) { ch = '/'; fg = scale(col, .18); }  // a crack
          break;
        }
        case FLOOR: case RUBBLE: {
          bg = floor_bg();
          if (t == FLOOR) {
            bool joint = x % 3 == 0 || y % 2 == 0;
            ch = joint ? 0x00B7 : ' ';
            fg = scale(act.floor, .62);
            // moss grows out from the walls in clumps (crypt and caves)
            bool by_wall = false;
            for (P d4 : {P{1, 0}, P{-1, 0}, P{0, 1}, P{0, -1}}) if (tile(x + d4.x, y + d4.y) == WALL) by_wall = true;
            double n = noise(x, y, 4, 7);
            auto wet = [&](int tt2) { return tt2 == SLUDGE || tt2 == FLOATER; };
            if (actn == CISTERN && wet(tile(x, y + 1)) && !wet(tile(x, y - 1))) { ch = 0x2584; fg = 0x4A5450; bg = 0x1C2224; }  // the curb of a channel
            else if (actn == CISTERN && wet(tile(x, y - 1)) && !wet(tile(x, y + 1))) { ch = 0x2580; fg = 0x4A5450; bg = 0x1C2224; }
            else if (actn == CISTERN && noise(x, y, 4, 9) > .64) { bg = mixc(bg, 0x10202A, .7); ch = ' '; }  // a puddle
            else if ((actn <= 1 && by_wall && n > .58) || (actn == CISTERN && by_wall && n > .7)) {
              ch = hh % 3 ? '"' : ',';
              fg = mixc(0x3E5A2A, 0x5E7A36, (hh % 5) / 5.0);
            }
          }
          break;
        }
        case SHALLOW:
          // still water: calm blue, a faint ripple now and then
          bg = mixc(0x16304E, 0x1C3A5C, noise(x, y, 3, 9));
          ch = std::fmod(tt * .3 + (hh % 100) / 100.0, 1.0) < .15 ? '~' : ' ';
          fg = 0x4A74A8;
          break;
        case WATER:
          bg = mixc(0x0C1C38, 0x10244A, noise(x, y, 3, 9));
          ch = '~'; fg = 0x2E5490;
          break;
        case LAVA: {
          double a = tt * .8, d = ldist_[i];
          double v = .5 + .25 * std::sin(x * .23 + y * .41 + a) + .2 * std::sin(y * .17 - x * .11 - a) + .05 * std::sin(x * .9 + y * 1.3 + 2 * a);
          double nearf = std::min(1.0, (d - 1) / 3), farf = std::clamp((d - 3) / 6, 0.0, 1.0);
          bg = mixc(0x6A1806, 0xC8460C, .2 + .8 * nearf * (.6 + .4 * v));
          bg = mixc(bg, 0xFFB43C, farf * (v - .3) * 1.1);
          ch = ' '; fg = 0;
          int kk = int((int(tt * 3) + hh) % 22);
          if (d >= 3 && hh % 11 == 0 && kk < 5) { static const int BUB[] = {'.', 'o', 'O', '*', 0x00B0}; ch = BUB[kk]; fg = kk < 4 ? 0xFFE6A0 : 0x5A1404; }
          else if (d >= 2 && v > .82 && (x + y) % 3 == 0) { ch = '~'; fg = mixc(0xE0561A, 0xFFE6A0, farf); }
          else if (d == 1 && hh % 6 == 0) { ch = 0x00B7; fg = 0x8C2A0A; }
          bright = true;
          break;
        }
        case CRUST: case NECK: {
          // black crust; the thin ridges between plates a touch warmer, with the glow showing through cracks
          if (t == CRUST) bg = mixc(0x16100C, 0x2A1A12, noise(x, y, 5, 3));
          else bg = mixc(0x1C0E08, 0x2E140A, .5 + .3 * (glow + .1 * std::sin(x * .08 + y * .05)));
          ch = (x + y) % 2 == 0 ? '.' : ' '; fg = 0x5A3A30;
          if (t == CRUST && hh % 23 == 0) { ch = ','; fg = 0x3C1E12; }
          // glowing seams in the crust, breathing slowly
          if (t == CRUST && noise(x, y, 2.2, 21) > .72 && hh % 2) {
            ch = 0x00B7; fg = mixc(0x8C2A0A, 0xFF7020, .5 + .5 * std::sin(tt * 1.1 + x * .3)); bright = true;
          }
          if (t == NECK && hh % 7 == 0) { ch = '~'; fg = mixc(0x8C2A0A, 0xE0561A, glow); }
          break;
        }
        case BASALT: case RUNE: case DAIS: case THRONE: {
          // the island: big slabs of a colder, cut stone, warmed only at the edge by the lava
          bg = mixc(0x24262E, 0x30333D, (hash(x / 3, y / 3, 6) % 5) / 5.0);
          ch = ' ';
          if (x % 3 == 0 && y % 3 == 0) { ch = 0x00B7; fg = 0x4A4D5C; }
          bool shore = false;
          for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) if (tile(x + dx, y + dy) == LAVA) shore = true;
          if (t == BASALT && shore) { bg = mixc(0x16171C, 0x3A1A10, .3 + .2 * glow); ch = 0x25AA; fg = 0x3A3C48; }
          if (t == RUNE) {
            static const int RUNES[] = {0x3A8, 0x3A9, 0x2206, 0x2020, 0x398, 0x3A3};
            ch = RUNES[(x * 3 + y) % 6]; fg = mixc(0x4A1E10, 0xC04A16, .25 + .9 * glow); bright = true;
          }
          if (t == DAIS) { bg = mixc(0x383A46, 0x44465A, (hash(x / 3, y / 3, 6) % 3) / 3.0); ch = tile(x, y + 1) == DAIS ? ' ' : 0x25AC; fg = 0x5A5C6E; }
          if (t == THRONE) { bg = 0x383A46; ch = 0x2565; fg = 0x8A7AA0; }
          break;
        }
        case EDGE: {
          // the way on: arrows pointing off the map
          bg = mixc(0x1A0A06, 0x3A1408, .6);
          ch = x == 0 ? 0x2190 : x == W - 1 ? 0x2192 : y == 0 ? 0x2191 : 0x2193;
          fg = mixc(0xC8A060, 0xFFE6A0, .5 + .5 * std::sin(tt * 3));
          keep = true;
          break;
        }
        case TORCH: {
          int course = (x + (y % 2) * 2) / 4;
          bg = mixc(scale(act.wall, .22 + .08 * ((hash(course, y, 2) % 7) / 7.0)), 0x3A1A06, .6);
          int st = int(tt * 6 + hh % 7);
          fg = mixc(0xFFB040, 0xFF6020, nz(int(hh % 13) + st * 13) * .6);
          bright = true;
          break;
        }
        case CANDLE: {
          bg = floor_bg();
          int st = int(tt * 5 + hh % 7);
          fg = mixc(0xFFE6A0, 0xFF8030, nz(int(hh % 11) + st * 7) * .5);
          bright = true;
          break;
        }
        case TOMB: case CRACKED: case OPENTOMB: {
          bool up = tombish(tile(x, y - 1)), dn = tombish(tile(x, y + 1)), lf = tombish(tile(x - 1, y)), rt = tombish(tile(x + 1, y));
          fg = 0x8A8478; bg = 0x2E2B28;
          if (!up && !lf) ch = 0x250C;
          else if (!up && !rt) ch = 0x2510;
          else if (!dn && !lf) ch = 0x2514;
          else if (!dn && !rt) ch = 0x2518;
          else if (!up || !dn) ch = 0x2500;
          else if (!lf || !rt) ch = 0x2502;
          else if (t == OPENTOMB) { ch = ' '; bg = 0x080706; }
          else if (t == CRACKED || (tombish(tile(x - 2, y)) && tombish(tile(x + 2, y)))) { ch = 0x2020; fg = 0xA8A090; }
          else if (tile(x + 1, y) == CRACKED) { ch = '/'; fg = 0x5A544C; }
          else ch = ' ';
          if (shudder(x, y) && int(tt * 14 + x) % 2) fg = mixc(fg, 0xE8E0D0, .6);
          break;
        }
        case BIER: case CORPSE: case RESTLESS: {
          bg = 0x2A1A0C;
          if (t == BIER) { ch = bierish(tile(x - 1, y)) && bierish(tile(x + 1, y)) ? 0x2550 : 0x2564; fg = 0x8A6A3C; }
          else if (t == CORPSE) { ch = '%'; fg = 0x9A8C74; }
          else {
            // a twitch, now and then
            bool tw = std::fmod(tt * .45 + (hh % 100) / 100.0, 1.0) < .07;
            ch = '%'; fg = tw || (shudder(x, y) && int(tt * 14) % 2) ? 0xE8DCC0 : 0xB0A088;
          }
          break;
        }
        case ALTAR: {
          bool l = tile(x - 1, y) == ALTAR, r = tile(x + 1, y) == ALTAR;
          bool end3 = (l && !r && tile(x - 2, y) == ALTAR) || (r && !l && tile(x + 2, y) == ALTAR);
          ch = end3 ? 0x2584 : 0x2021; fg = end3 ? 0x8A8478 : 0xDCC88C; bg = 0x34302A;
          break;
        }
        case PEW: ch = 0x2261; fg = 0x6E5032; bg = floor_bg(); break;
        case PILLAR: ch = 0x25CB; fg = 0xB4ACA0; bg = 0x2C2926; break;
        case BONEWALL:
          ch = (x * 7 + y * 3) % 4 == 0 ? 0x3A9 : '%';
          fg = ch == '%' ? 0xA89C80 : 0xC8BCA0; bg = 0x241F1A;
          break;
        case BONES: ch = hh % 2 ? ',' : '%'; fg = 0xD2C4AA; bg = floor_bg(); break;
        case HEARTH: {
          int st = int(tt * 7 + hh % 7);
          static const int FL[] = {'^', 0x25B2, '*'};
          ch = FL[(st + x) % 3]; fg = FIREC[std::clamp(int(nz(int(hh % 31) + st) * 3.5), 0, 5)];
          bg = mixc(0x5A1804, 0xA04008, nz(int(hh % 7) + st * 7)); bright = true;
          break;
        }
        case BRAZIER: {
          int st = int(tt * 5 + hh % 7);
          fg = mixc(0xFF9030, 0xFFF0B0, nz(st)); bg = 0x5A1A08; bright = true;
          break;
        }
        case STONEWORK: bg = 0x2A1E18; fg = 0x5A4A40; break;
        case CHAIN: case WRECK: case SHIELD: case RAIL: case ASH:
          bg = mixc(0x16100C, 0x2A1A12, noise(x, y, 5, 3));
          if (t == ASH) bg = mixc(bg, 0x3A3632, .5);
          break;
        case ANVIL: case SLAG: case CART: case BANNER: case FALLEN: case STATUE: {
          bg = mixc(0x16100C, 0x2A1A12, noise(x, y, 5, 3));
          if (t == CART) bg = 0x2A1A0C;
          if (t == STATUE) {
            bg = 0x2A2622;
            if (tile(x - 1, y) != STATUE) ch = 0x263B;  // the head
          }
          if (t == FALLEN && (std::fmod(tt * .45 + (hh % 100) / 100.0, 1.0) < .05 || (shudder(x, y) && int(tt * 14) % 2))) fg = 0xE8DCC0;
          break;
        }
        case OBELISK: bg = 0x0A080C; break;
        case OBSIDIAN: {
          // glassy: now and then it catches the light
          bg = 0x0C0A12;
          if (std::fmod(tt * .2 + (hh % 100) / 100.0, 1.0) < .06) { fg = 0xC8D8F0; bright = true; }
          break;
        }
        case VENT: bg = 0x1A0E0A; break;
        case SLUDGE: case FLOATER: {
          // black sludge with a greasy sheen sliding over it, bubbles, floating ash, and now and then driftwood or a pale fish
          double a = tt * .8;
          double v = .5 + .5 * std::sin(x * .5 + y * .7 + a) * std::sin(x * .23 - y * .3 - a * .5);
          bg = mixc(0x0C120A, 0x1C2414, v);
          double sh = std::sin(x * .35 + y * .9 - a * 1.2);
          if (sh > .75) bg = mixc(bg, (x + y) % 2 ? 0x3A2E58 : 0x264A34, (sh - .75) * 3.2);
          ch = ' '; fg = 0;
          int kk = int((int(tt * 3) + hh) % 26);
          bool across = tile(x - 1, y) == SLUDGE && tile(x + 1, y) == SLUDGE, down = tile(x, y - 1) == SLUDGE && tile(x, y + 1) == SLUDGE;
          if (t == FLOATER) {
            ch = '%'; fg = (std::fmod(tt * .4 + (hh % 100) / 100.0, 1.0) < .05 || (shudder(x, y) && int(tt * 14) % 2)) ? 0xD8E0C0 : 0x7A8A6A;
          } else if (across && (x + int(tt * .5) + int(hash(y, 1, 9) % 40)) % 40 == 0) { ch = '='; fg = 0x6A5034; }
          else if (down && !across && (y + int(tt * .5) + int(hash(x, 1, 9) % 30)) % 30 == 0) { ch = 0x2551; fg = 0x6A5034; }
          else if (hash(x, y, 3) % 6 == 0 && kk < 4) { static const int BB[] = {'.', 'o', 'O', 0x00B0}; ch = BB[kk]; fg = kk < 3 ? 0x8AA040 : 0x3A4A20; }
          else if (std::fmod(tt * .35 + (hash(x, y, 7) % 1000) / 1000.0, 1.0) < .012) { ch = '-'; fg = 0xC8D0C0; }  // a blind fish
          else if (hash(x, y, 5) % 13 == 0) { ch = 0x2219; fg = 0x5A5A54; }
          // inside the rim of a waterwheel: its spokes, rocking back and forth where it is stuck
          for (int dy = -3; dy <= 3 && ch == ' '; ++dy)
            for (int dx = -4; dx <= 4; ++dx) {
              if (tile(x + dx, y + dy) != HUB) continue;
              double hx = x + dx + (tile(x + dx + 1, y + dy) == HUB ? .5 : 0), hy = y + dy;
              double ang = std::atan2((y - hy) / .68, x - hx), rock = .3 * std::sin(tt * .7);
              for (int k = 0; k < 4; ++k) {
                double sp = k * PI / 4 + rock, dd = std::remainder(ang - sp, PI);
                if (std::abs(dd) < .22) {
                  double deg = std::fmod(std::fmod(sp * 180 / PI, 180) + 180, 180);
                  ch = deg < 22 || deg >= 158 ? 0x2500 : deg < 68 ? '\\' : deg < 112 ? 0x2502 : '/';
                  fg = 0x6A5034;
                }
              }
              break;
            }
          break;
        }
        case MIRE: {
          bg = mixc(0x0E1416, 0x16201E, .5 + .5 * std::sin(x * .4 + y * .6 + tt * .8));
          if (tile(x, y - 1) == PILLAR) bg = mixc(bg, 0x3A403E, .45);  // a pillar's reflection
          ch = std::fmod(tt * .3 + (hh % 100) / 100.0, 1.0) < .08 ? '~' : ' ';
          fg = 0x3A4A48;
          break;
        }
        case GRATE: bg = 0x0E100E; ch = '#'; fg = 0x5A5A60; break;
        case CHASM: {
          // the floor falls away into the dark; its lip catches the light
          bg = 0x04050A; ch = ' ';
          if (tile(x, y - 1) != CHASM && !blocks(tile(x, y - 1))) { ch = 0x2580; fg = 0x3A4446; }
          break;
        }
        case FLOODGATE: {
          bg = 0x2A1A10; fg = mixc(0x8A4A28, 0xA0602E, nz(int(y)));
          ch = tile(x, y - 1) == FLOODGATE && tile(x, y + 1) == FLOODGATE ? (y % 2 ? 0x256C : 0x2551) : 0x2593;
          break;
        }
        case GEAR: bg = 0x1C1410; break;
        case WHEEL: {
          // the rim of the wheel follows the circle round its hub
          ch = 0x25A0; fg = 0x7A5A3A; bg = 0x2A1A0C;
          for (int dy = -4; dy <= 4; ++dy)
            for (int dx = -6; dx <= 6; ++dx)
              if (tile(x + dx, y + dy) == HUB) {
                double ang = std::atan2(-dy / .68, -dx) + PI / 2, deg = std::fmod(std::fmod(ang * 180 / PI, 180) + 180, 180);
                ch = deg < 22 || deg >= 158 ? 0x2500 : deg < 68 ? '\\' : deg < 112 ? 0x2502 : '/';
                dy = 9; break;
              }
          break;
        }
        case HUB: bg = 0x1A1A1C; break;
        case MOSSWALL: {
          int course = (x + (y % 2) * 2) / 4;
          bg = mixc(scale(act.wall, .22 + .08 * ((hash(course, y, 2) % 7) / 7.0)), 0x0E3A30, .5);
          ch = hh % 2 ? '"' : '*'; fg = mixc(0x5AC0A0, 0xA0FFE0, .5 + .5 * std::sin(tt * .7 + hh % 7)); bright = true;
          break;
        }
        case PIPE: case SPOUT: {
          int course = (x + (y % 2) * 2) / 4;
          bg = scale(act.wall, .22 + .08 * ((hash(course, y, 2) % 7) / 7.0));
          if (t == PIPE) ch = (tile(x - 1, y) == PIPE || tile(x + 1, y) == PIPE) ? 0x2550 : 0x2551;
          if (t == PIPE && hh % 9 == 0) ch = 0x256C;
          break;
        }
        case PUMP: bg = 0x1E1A18; fg = 0x4A4440; break;
        case LEVER: bg = floor_bg(); keep = true; break;
        case EGG: {
          double gi = .5 + .5 * std::sin(tt * 2 + hh % 9);
          ch = hh % 3 ? 'o' : 'O'; fg = mixc(0x9A9A80, 0xE0E8C0, gi); bg = mixc(0x1A1C14, 0x3A4430, gi);
          if (shudder(x, y) && int(tt * 14) % 2) fg = 0xFFFFFF;
          bright = true;
          break;
        }
        case TIDESTATUE: bg = 0x2A3032; break;
        case BASIN: {
          bool up = tile(x, y - 1) == BASIN, dn = tile(x, y + 1) == BASIN, lf = tile(x - 1, y) == BASIN, rt = tile(x + 1, y) == BASIN;
          bool inU = tile(x, y + 1) == SLUDGE || tile(x, y + 1) == FLOATER || tile(x, y + 1) == TIDESTATUE;
          bool inD = tile(x, y - 1) == SLUDGE || tile(x, y - 1) == FLOATER || tile(x, y - 1) == TIDESTATUE;
          fg = 0x6A7270; bg = 0x22282A;
          if (rt && dn && !up && !lf) ch = 0x2554;
          else if (lf && dn && !up && !rt) ch = 0x2557;
          else if (rt && up && !dn && !lf) ch = 0x255A;
          else if (lf && up && !dn && !rt) ch = 0x255D;
          else if ((lf || rt) && (inU || inD || !(up || dn))) ch = 0x2550;
          else if (up || dn) ch = 0x2551;
          else ch = 0x2550;
          break;
        }
        case COIN: bg = floor_bg(); keep = true; break;
        case SHAFT: {
          bg = mixc(0x3A1404, 0x8A3408, .5 + .3 * std::sin(tt * 1.3 + hh % 5));
          ch = 0x2591; fg = mixc(0xC85A14, 0xFFA040, .5 + .5 * std::sin(tt * 2 + hh % 7)); bright = true;
          break;
        }
        case LAMP: {
          bg = floor_bg();
          int st = int(tt * 5 + hh % 7);
          fg = mixc(0xFFC060, 0xFF8030, nz(int(hh % 11) + st * 7) * .5);
          bright = true;
          break;
        }
        case SLIME: bg = mixc(floor_bg(), 0x1A2416, .5); break;
        case DOOR: case OPEN: bg = floor_bg(); keep = true; break;
        case STAIRS: case UPSTAIRS: case CHEST: case OPENCHEST: case SHRINE: case USED: case FOUNTAIN:
          bg = floor_bg(); keep = true; break;
        default: bg = floor_bg(); break;
      }
      if (vis[i] == '1') {
        double l = L[i], w = warm[i];
        if (keep) l = std::max(l, 1.0);
        if (!bright) fg = mixc(mixc(scale(fg, l), 0x101828, std::max(0.0, 1 - l) * .35), scale(0xFF9A48, l), w * .35);
        bg = mixc(scale(bg, l), 0x5A2A0A, w * .5 * std::min(l, 1.2));
        if (double c = cold[i]; c > 0) {
          if (!bright) fg = mixc(fg, scale(0x5AE0B0, l), c * .35);
          bg = mixc(bg, 0x0E3A30, c * .45 * std::min(l, 1.2));
        }
      } else {
        // remembered, not seen: cold and dim
        fg = mixc(scale(fg, .35), 0x202430, .3);
        bg = scale(bg, .3);
      }
      put(X, Y, ch, fg, bg);
    }
}

// Particles: small squares of a font pixel or two, drawn over the rendered console. Each is worked out from the time
// alone, so nothing has to be kept from frame to frame.
void Client::draw_particles(SDL_Renderer* r, double ox, double oy, double cell) {
  if (screen_ != Screen::Game || !snap_.is_object() || (over_ != Over::None && over_ != Over::Target)) return;
  int fl = snap_["floor"];
  if (!maps_.count(fl)) return;
  if (snap_.value("town", false)) { draw_town_particles(r, ox, oy, cell); return; }
  const std::string& map = maps_[fl];
  const std::string& vis = snap_["vis"].get_ref<const std::string&>();
  double tt = now(), px = cell / 8;
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  auto open_at = [&](double x, double y) {
    int ix = int(std::floor(x)), iy = int(std::floor(y));
    if (ix < 0 || iy < 0 || ix >= W || iy >= H || vis[iy * W + ix] != '1') return false;
    return true;
  };
  auto dot = [&](double x, double y, int c, double a, double size = 1) {
    double X = x - camx_ + vx_, Y = y - camy_ + vy_;
    if (X < vx_ || Y < vy_ || X >= vx_ + vw_ || Y >= vy_ + vh_) return;
    SDL_SetRenderDrawColor(r, (c >> 16) & 255, (c >> 8) & 255, c & 255, Uint8(std::clamp(a, 0.0, 1.0) * 255));
    SDL_FRect rc{float(ox + X * cell), float(oy + Y * cell), float(px * size), float(px * size)};
    SDL_RenderFillRect(r, &rc);
  };
  for (auto& e : emits_) {
    double ex = e.x, ey = e.y;
    int sd = e.seed;
    switch (e.kind) {
      case 't': case 'f':
        // embers rising off the flame, and a little smoke
        for (int j = 0; j < 6; ++j) {
          double ph = std::fmod(tt * .55 + j / 6.0 + nz(sd + j) * .5, 1.0);
          if (ph > .85) continue;
          double x = ex + .5 + (nz(sd * 3 + j) - .5) * .7 + std::sin(tt * 2 + j) * .15, y = ey + .3 - ph * 2.2;
          dot(x, y, mixc(0xFFE070, 0x8A2A04, ph), 1 - ph * .6);
        }
        for (int j = 0; j < 4; ++j) {
          double ph = std::fmod(tt * .3 + j / 4.0 + sd * .07, 1.0);
          double x = ex + .5 + std::sin(tt * 1.3 + j) * .3 * ph + ph * .3, y = ey + .1 - ph * 2.6;
          dot(x, y, mixc(0x8A8480, 0x2A2A30, ph), .55 * (1 - ph), ph < .5 ? 1 : 2);
        }
        break;
      case 'c':
        for (int j = 0; j < 3; ++j) {
          double ph = std::fmod(tt * .25 + j / 3.0 + sd * .05, 1.0);
          double x = ex + .5 + std::sin(tt * 1.1 + j * 2) * .2 * ph, y = ey + .15 - ph * 1.8;
          dot(x, y, mixc(0xB0AAA0, 0x3A3A40, ph), .5 * (1 - ph));
        }
        break;
      case 'd':
        // dust drifting in the torchlight
        for (int j = 0; j < 7; ++j) {
          double ph = std::fmod(tt * .07 + nz(sd * 5 + j), 1.0);
          double x = ex + (nz(sd + j * 13) - .5) * 6 + std::sin(tt * .4 + j) * .6, y = ey + (nz(sd + j * 29) - .5) * 5 - ph * 1.5;
          if (!open_at(x, y) || blocks(tile_of(map[int(y) * W + int(x)]))) continue;
          dot(x, y, mixc(0x6A5A40, 0xE8D8B0, std::sin(ph * PI)), .8 * std::sin(ph * PI));
        }
        break;
      case 'w': {
        // a drop falls into the puddle, and one ring spreads
        double ph = std::fmod(tt * .33 + nz(sd), 1.0);
        if (ph < .25) {
          double k = ph / .25;
          dot(ex + .45, ey - .6 + k * 1.0, 0xA8C8F0, .8);
        } else if (ph < .6) {
          double k = (ph - .25) / .35, rr = .1 + k * .45;
          for (int j = 0; j < 10; ++j) {
            double a = j * PI / 5;
            dot(ex + .5 + std::cos(a) * rr * 1.2 - px / cell / 2, ey + .5 + std::sin(a) * rr * .7, 0x8AB0E0, (1 - k) * .7);
          }
        }
        break;
      }
      case 'l': {
        // now and then a bubble bursts and throws up a spark
        double ph = std::fmod(tt * .4 + nz(sd), 1.0);
        if (ph < .35) {
          double k = ph / .35;
          for (int j = 0; j < 3; ++j)
            dot(ex + .5 + (nz(sd + j) - .5) * k * 1.2, ey + .4 - k * 1.4 + k * k * .8, mixc(0xFFF0B0, 0xB0300A, k), 1 - k);
        }
        break;
      }
      case 'p':
        // pale spores drifting up and out from the glowing moss
        for (int j = 0; j < 5; ++j) {
          double ph = std::fmod(tt * .12 + j / 5.0 + nz(sd + j) * .4, 1.0);
          int fx = 0, fy = 0;
          for (P d4 : {P{1, 0}, P{-1, 0}, P{0, 1}, P{0, -1}}) if (!blocks(tile_of(map[(int(ey) + d4.y) * W + int(ex) + d4.x]))) { fx = d4.x; fy = d4.y; break; }
          double x = ex + .5 + fx * ph * 1.8 + std::sin(tt * 1.5 + j) * .3, y = ey + .5 + fy * ph * 1.8 - ph * 1.6 + (nz(sd * 3 + j) - .5) * 1.5;
          dot(x, y, mixc(0xB0FFE0, 0x2A5A4A, ph), .9 * (1 - ph));
        }
        break;
      case 'm':
        // low mist creeping over the sludge
        for (int j = 0; j < 3; ++j) {
          double ph = std::fmod(tt * .05 + j / 3.0 + nz(sd + j), 1.0);
          double x = ex - 1 + ph * 3 + std::sin(tt * .3 + j) * .4, y = ey + (nz(sd * 7 + j) - .3) + std::sin(tt * .5 + j) * .2;
          dot(x, y, mixc(0x4A5A5A, 0x8A9A98, std::sin(ph * PI) * .6), .5 * std::sin(ph * PI), j == 0 ? 2 : 1);
        }
        break;
      case 'r': {
        // a drop from the ceiling into a puddle, and one ring
        double ph = std::fmod(tt * .25 + nz(sd), 1.0);
        if (ph < .3) dot(ex + .45, ey - 2.5 + ph / .3 * 3, 0x9AB8D0, .8);
        else if (ph < .7) {
          double q = (ph - .3) / .4, rr = .15 + q * .6;
          for (int j = 0; j < 10; ++j) {
            double an = j * PI / 5;
            dot(ex + .5 + std::cos(an) * rr * 1.3, ey + .5 + std::sin(an) * rr * .7, 0x8AB0E0, (1 - q) * .7);
          }
        }
        break;
      }
      case 'z':
        // sludge pouring out of a broken pipe into the pool below, spattering where it lands
        for (int j = 0; j < 10; ++j) {
          double ph = std::fmod(tt * .9 + j / 10.0, 1.0);
          dot(ex + .3 + nz(sd + j) * .4, ey + .7 + ph * 1.2, mixc(0x5A6A3A, 0x1A2014, ph), .9, j % 3 ? 1 : 2);
        }
        for (int j = 0; j < 4; ++j) {
          double ph = std::fmod(tt * 1.3 + j / 4.0, 1.0);
          dot(ex + .5 + (j % 2 ? 1 : -1) * ph * .9, ey + 1.6 - std::sin(ph * PI) * .5, 0x6A7A44, 1 - ph);
        }
        break;
      case 'h':
        // ash and embers rising out of the shaft from the dead city
        for (int j = 0; j < 7; ++j) {
          double ph = std::fmod(tt * .22 + j / 7.0 + nz(sd + j) * .3, 1.0);
          double x = ex + .5 + (nz(sd * 5 + j) - .3) * 1.6 + std::sin(tt + j) * .4, y = ey + .8 - ph * 4;
          dot(x, y, j % 3 ? mixc(0x9A9488, 0x2A2826, ph) : mixc(0xFFB050, 0x6A1A04, ph), 1 - ph);
        }
        break;
      case 'u':
        // bubbles round a floating corpse
        for (int j = 0; j < 3; ++j) {
          double ph = std::fmod(tt * .5 + j / 3.0 + nz(sd + j), 1.0);
          double an = nz(sd * 3 + j) * 2 * PI, rr = .3 + ph * .9;
          dot(ex + .5 + std::cos(an) * rr * 1.3, ey + .5 + std::sin(an) * rr * .7, mixc(0xA0C050, 0x2A3418, ph), 1 - ph);
        }
        break;
      case 'q': {
        // a ripple in the sludge under the grate, following the one on it
        double ph = std::fmod(tt * .6, 1.0), rr = .3 + ph * 1.2;
        for (int j = 0; j < 12; ++j) {
          double an = j * PI / 6;
          dot(ex + .5 + std::cos(an) * rr * 1.4 - .6, ey + .5 + std::sin(an) * rr * .8 + .4, 0x5A7A4A, (1 - ph) * .6);
        }
        break;
      }
      case 'k':
        // sparks thrown off the hearth
        for (int j = 0; j < 8; ++j) {
          double ph = std::fmod(tt * .9 + j / 8.0 + nz(sd + j) * .3, 1.0);
          if (ph > .6) continue;
          double ang = -.3 - nz(sd * 2 + j) * 2.6, k = ph / .6;
          double x = ex + .5 + std::cos(ang) * ph * 3, y = ey + .4 + std::sin(ang) * ph * 3 + ph * ph * 3;
          dot(x, y, mixc(0xFFF4C0, 0xFF5010, k), 1 - k * .5);
        }
        break;
      case 'v': {
        // a burst of steam, then quiet
        double ph = std::fmod(tt * .22 + nz(sd), 1.0);
        if (ph < .5) {
          double k = ph / .5;
          for (int j = 0; j < 16; ++j) {
            double x = ex + .5 + (nz(sd + j) - .5) * k * 2.4 + std::sin(tt * 2 + j) * .2, y = ey + .3 - k * (2 + nz(sd * 3 + j) * 2.5);
            dot(x, y, mixc(0xF0F0F0, 0x50505A, k), (1 - k) * .9, j % 3 ? 1 : 2);
          }
        }
        break;
      }
      case 'g':
        // a thin wisp of smoke out of the ground
        for (int j = 0; j < 6; ++j) {
          double ph = std::fmod(tt * .18 + j / 6.0 + nz(sd) * .7, 1.0);
          double x = ex + .5 + std::sin(tt * .9 + j + sd) * .5 * ph + ph * .3, y = ey + .5 - ph * 2.6;
          dot(x, y, mixc(0x8A8078, 0x241E1C, ph), .7 * (1 - ph), ph < .4 ? 1 : 2);
        }
        break;
      case 'e': {
        // a red ember drifting in slow loops
        double rr = .8 + nz(sd + 5) * 1.6, sp = .35 + .2 * (sd % 3), a = tt * sp + sd;
        double x = ex + .5 + std::cos(a) * rr * 1.3 + std::sin(tt * .3 + sd) * .8;
        double y = ey + .5 + std::sin(a) * rr * .7 - std::sin(tt * .15 + sd) * .6;
        double tw = .5 + .5 * std::sin(tt * 1.6 + sd * 2);
        dot(x, y, mixc(0xC02810, 0xFF8040, tw), .65 + .35 * tw, tw > .8 ? 2 : 1);
        break;
      }
      case 'a':
        // flakes of ash, falling
        for (int j = 0; j < 4; ++j) {
          double ph = std::fmod(tt * .08 + j / 4.0 + nz(sd + j), 1.0);
          double x = ex + (nz(sd + j * 7) - .5) * 6 + std::sin(tt * .5 + j) * .6, y = ey - 2 + ph * 4;
          dot(x, y, mixc(0x9A9488, 0x3A3632, ph), .8 * std::sin(ph * PI));
        }
        break;
      case 's':
        // dust trickling from the crack in a lid
        for (int j = 0; j < 3; ++j) {
          double ph = std::fmod(tt * .4 + j / 3.0 + nz(sd + j), 1.0);
          dot(ex - .4 + nz(sd + j * 3) * .5, ey + .3 + ph * .6, 0xB8B0A0, .6 * (1 - ph));
        }
        break;
    }
  }
  // the dust of a tomb lid grinding open, or a cloud when it falls
  for (auto& f : fx_) {
    if (f.kw.value("k", "") != "dust") continue;
    double age = tt - f.start;
    int bx = f.kw.value("x", 0), by = f.kw.value("y", 0);
    if (!open_at(bx, by)) continue;
    bool fall = f.kw.value("fall", false), tomb = f.kw.value("tomb", true);
    if (!fall) {
      // puffs squeezed out along the seam of the lid, rising and spreading
      for (int j = 0; j < 30; ++j) {
        double ph = std::fmod(age * 1.3 + j / 30.0, 1.0);
        double sx = (nz(j) - .5) * (tomb ? 4.6 : 2.8);
        double x = bx + .5 + sx * (1 + ph * .4), y = by + (j % 2 ? -.4 : 1.4) - ph * 1.2 + (nz(j + 9) - .5) * .3;
        dot(x, y, mixc(0xE8E0D0, 0x4A4640, ph), 1 - ph * .9, j % 3 == 0 ? 2 : 1);
      }
    } else {
      double k = std::min(1.0, age / 1.4);
      for (int j = 0; j < 26; ++j) {
        double a = nz(j) * 2 * PI, rr = .4 + k * (1.5 + nz(j + 3) * 2);
        double x = bx + .5 + std::cos(a) * rr * 1.3, y = by + .5 + std::sin(a) * rr * .7 - k;
        dot(x, y, mixc(0xE0D8C8, 0x2A2824, k), 1 - k, j % 4 == 0 ? 2 : 1);
      }
    }
  }
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

// Render the console, the particles over it, and show it. Falls back to libtcod's own present when its renderer is
// not SDL's (there are then no particles).
void Client::present(const TCOD_ViewportOptions& vp) {
  TCOD_Context* c = ctx_.get_ptr().get();
  SDL_Renderer* r = c ? TCOD_context_get_sdl_renderer(c) : nullptr;
  if (!r || !c->c_accumulate_ || c->type != TCOD_RENDERER_SDL2) { ctx_.present(con_, vp); return; }
  SDL_SetRenderTarget(r, nullptr);
  SDL_SetRenderDrawColor(r, vp.clear_color.r, vp.clear_color.g, vp.clear_color.b, vp.clear_color.a);
  SDL_RenderClear(r);
  if (c->c_accumulate_(c, con_.get(), &vp) < 0) { ctx_.present(con_, vp); return; }
  auto* rd = static_cast<TCOD_RendererSDL2*>(c->contextdata_);
  auto& tr = rd->cursor_transform;
  if (tr.scale_x > 0) draw_particles(r, tr.offset_x, tr.offset_y, 1.0 / tr.scale_x);
  if (!shot_.empty()) {
    if (SDL_Surface* s = SDL_RenderReadPixels(r, nullptr)) {
      if (SDL_Surface* c32 = SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32)) {
        std::vector<unsigned char> px(size_t(c32->w) * c32->h * 4);
        for (int y = 0; y < c32->h; ++y)
          std::memcpy(px.data() + size_t(y) * c32->w * 4, static_cast<unsigned char*>(c32->pixels) + size_t(y) * c32->pitch, size_t(c32->w) * 4);
        lodepng_encode32_file(shot_.c_str(), px.data(), unsigned(c32->w), unsigned(c32->h));
        SDL_DestroySurface(c32);
      }
      SDL_DestroySurface(s);
    }
    shot_.clear();
  }
  SDL_RenderPresent(r);
}
