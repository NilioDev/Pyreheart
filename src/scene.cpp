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
  auto& act = ACTS[std::clamp(s["act"].get<int>(), 0, 3)];
  int actn = s["act"].get<int>();
  bool rest = s.value("rest", false);
  double tt = now();
  emits_.clear();
  auto tile = [&](int x, int y) { return x < 0 || y < 0 || x >= W || y >= H ? int(WALL) : map[y * W + x] - '0'; };
  int x0 = std::max(0, camx_), y0 = std::max(0, camy_), x1 = std::min(W, camx_ + vw_), y1 = std::min(H, camy_ + vh_);

  // the light: a dim base, pools round every flame, and each hero's own light
  double base = rest ? .9 : .6;
  std::vector<float> L(W * H, float(base)), warm(W * H, 0.f);
  auto pool = [&](double cx, double cy, double r, double a, double w) {
    int ax = std::max(x0, int(cx - r - 1)), bx = std::min(x1 - 1, int(cx + r + 1));
    int ay = std::max(y0, int(cy - r - 1)), by = std::min(y1 - 1, int(cy + r + 1));
    for (int y = ay; y <= by; ++y)
      for (int x = ax; x <= bx; ++x) {
        double d = std::hypot(x - cx, y - cy);
        if (d > r) continue;
        double k = 1 - d / (r + 1), sm = k * k * (3 - 2 * k);
        int i = y * W + x;
        L[i] = std::max(L[i], float(base + (a - base) * sm));
        warm[i] = std::max(warm[i], float(w * k));
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
        pool(x, y, 2.2, .95, .9);
      } else if (t == SHALLOW && vis[i] == '1' && hash(x, y, 5) % 19 == 0) {
        emits_.push_back({float(x), float(y), 'w', sd});
      } else if (t == CRACKED && vis[i] == '1') {
        emits_.push_back({float(x), float(y), 's', sd});
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
    if (!h.value("gone", false) && !h.value("dead", false)) pool(h["x"].get<int>(), h["y"].get<int>(), 6, 1.25, 0);

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
            if (actn <= 1 && by_wall && n > .58) {
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
          double p = .5 + .5 * std::sin(tt * 1.3 + (hh % 100) * .2);
          bg = mixc(0x6A1806, 0xA03A0A, p); fg = mixc(0xFF7020, 0xFFC050, p); bright = true;
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
          if (!open_at(x, y) || blocks(map[int(y) * W + int(x)] - '0')) continue;
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
