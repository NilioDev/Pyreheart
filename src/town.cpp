// Charnfen at night: the last village, drawn by hand (ref/charnfen.txt). Everything is dark but living flame, each
// source flickering on its own; the great fire on the square throws sparks and smoke, chimneys smoke, snow falls and
// melts before it reaches the fire.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include <SDL3/SDL.h>

#include "client.hpp"
#include "data.hpp"

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
// value noise, smooth, for the mist
double vnoise(double x, double y) {
  auto h = [](int a, int b) { unsigned v = unsigned(a) * 73856093u ^ unsigned(b) * 19349663u; v ^= v >> 13; v *= 0x5bd1e995u; v ^= v >> 15; return (v % 10000) / 10000.0; };
  int x0 = int(std::floor(x)), y0 = int(std::floor(y));
  double fx = x - x0, fy = y - y0;
  fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
  double a = h(x0, y0) + (h(x0 + 1, y0) - h(x0, y0)) * fx, b = h(x0, y0 + 1) + (h(x0 + 1, y0 + 1) - h(x0, y0 + 1)) * fx;
  return a + (b - a) * fy;
}
// for the mock-ups: PYREHEART_TOWNFX=mist (banks of ground mist) or all (mist and finer particles)
int town_fx() {
  static int m = [] { const char* e = std::getenv("PYREHEART_TOWNFX"); return !e ? 0 : std::string(e) == "all" ? 2 : 1; }();
  return m;
}
}  // namespace

void Client::draw_town() {
  auto& s = snap_;
  int fl = s["floor"];
  const std::string& map = maps_[fl];
  const std::string& vis = s["vis"].get_ref<const std::string&>();
  auto& seen = seen_[fl];
  auto& act = data::ACTS[s["act"].get<int>()];
  int hx = s["me"]["x"], hy = s["me"]["y"];
  auto sx = [&](int x) { return x - camx_ + vx_; };
  auto sy = [&](int y) { return y - camy_ + vy_; };
  auto T = [&](int i) { return tile_of(map[i]); };
  std::vector<float> light(W * H, .26f), warm(W * H, 0.f);
  std::vector<P> fires, smokes;
  int fcx = -99, fcy = -99;
  double tt = now();
  auto flick = [&](int seed) { return float(.86 + .08 * std::sin(tt * 4.1 + seed * 1.7) + .06 * std::sin(tt * 9.7 + seed * 3.1)); };
  auto pool = [&](int cx, int cy, float r, float a, float w) {
    int R = int(r) + 1;
    for (int y = std::max(0, cy - R); y <= std::min(H - 1, cy + R); ++y)
      for (int x = std::max(0, cx - R); x <= std::min(W - 1, cx + R); ++x) {
        float d = std::sqrt(float((x - cx) * (x - cx) + (y - cy) * (y - cy)));
        if (d > r) continue;
        float k = 1 - d / (r + 1);
        int i = y * W + x;
        light[i] = std::max(light[i], .26f + (a - .26f) * k * k * (3 - 2 * k));
        warm[i] = std::max(warm[i], w * k);
      }
  };
  pool(hx, hy, 5, .8f, 0);
  int sx_ = 0, sy_ = 0;
  for (int i = 0; i < W * H; ++i) {
    int t = T(i), x = i % W, y = i / W;
    if (t == data::FIRE) { fires.push_back({x, y}); sx_ += x; sy_ += y; }
    else if (t == data::CHIMNEY) smokes.push_back({x, y});
    else if (t == data::TORCH) { float f = flick(i); pool(x, y, 4.5f * f, 1.05f * f, .8f); }
    else if (t == data::CANDLE) { float f = flick(i); pool(x, y, 3.5f * f, 1.f * f, .6f); }
    else if (t == data::STAIRS) pool(x, y, 3, .7f * flick(i), 1.f);  // the warmth of the sleeping god, from below
    else if (t == data::LIT) pool(x, y, 3, .75f * flick(i), .7f);
    else if (t == data::CRACK || t == data::EMBER) pool(x, y, 2, .6f * flick(i), .9f);
    else if (t == data::ALTAR) pool(x, y, 3, .8f * flick(i), .4f);
    else if (t == data::WINDOW) pool(x, y, 1, .5f, .2f);
  }
  if (!fires.empty()) {
    fcx = sx_ / int(fires.size()); fcy = sy_ / int(fires.size());
    float f = flick(7) * .6f + flick(19) * .4f;
    pool(fcx, fcy, 12.f * f, 1.25f * f, 1.f);
  }
  town_fire_ = {fcx, fcy};
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x) {
      int X = sx(x), Y = sy(y);
      if (X < vx_ || Y < vy_ || X >= vx_ + vw_ || Y >= vy_ + vh_) continue;
      int i = y * W + x;
      if (!seen[i]) continue;
      int t = T(i);
      auto& td = data::TILES[t];
      int col = td.col >= 0 ? td.col : t == data::WALL ? act.wall : act.floor;
      int ch = td.ch;
      if (t == data::SNOW) {
        unsigned h = unsigned(i) * 2654435761u >> 24;  // drifts: mostly bare snow, here and there a mound
        ch = h < 150 ? ' ' : h < 235 ? '.' : h < 250 ? ',' : '\'';
        if (std::hypot((x - fcx) / 1.6, double(y - fcy)) < 6) { ch = h < 120 ? '.' : ','; col = 0x6A5442; }  // melted round the fire
      }
      int bg = 0;
      if (t == data::WALL) ch = '#';
      float l = light[i], w = warm[i];
      int fg = mixc(mixc(scale(col, l), 0x101828, std::max(0.f, 1 - l) * .35), scale(0xFF9A48, l), w * .35);
      if (t == data::WALL) bg = mixc(scale(col, .22 * l), 0x3A1A08, w * .5);
      else if (w > .05f) bg = scale(0x5A2A0A, w * std::min(l, 1.2f) * .75f);
      (void)vis;
      if (town_fx() && t != data::WALL && !data::blocks(t)) {
        // ground mist: slow banks drifting with the wind, thick among the graves, burnt away round the fire;
        // where the firelight falls on it, it glows warm
        double fd = std::hypot((x - fcx) / 1.6, double(y - fcy));
        double keep = std::clamp((fd - 7) / 9, 0.0, 1.0);
        double n = .55 * vnoise(x * .09 - tt * .22, y * .2 + tt * .03) + .3 * vnoise(x * .21 - tt * .4 + 50, y * .4 - tt * .05) +
                   .15 * vnoise(x * .5 - tt * .7, y * .9 + 20);
        double m = std::clamp((n - .38) / .35, 0.0, 1.0) * keep;
        if (m > 0) {
          int fog = mixc(0x46506A, 0x8A5A34, std::min(1.0, w * 1.4));
          fog = scale(fog, .55 + .6 * std::min(1.f, l));
          bg = mixc(bg, fog, m * .62);
          fg = mixc(fg, fog, m * .38);
          if (t == data::SNOW && ch == ' ' && m > .7) { ch = 0x2591; fg = mixc(bg, 0x8A94A8, .25); }  // the thickest wisps
        }
      }
      put(X, Y, ch, fg, bg);
    }
  // the fire, sparks and smoke, torches and candles, snow
  auto inside = [&](int x, int y) { int X = sx(x), Y = sy(y); return x >= 0 && y >= 0 && x < W && y < H && X >= vx_ && Y >= vy_ && X < vx_ + vw_ && Y < vy_ + vh_; };
  auto noise = [](int k) { unsigned h = unsigned(k) * 2654435761u; h ^= h >> 13; h *= 0x5bd1e995u; return (h ^ (h >> 15)) % 10000 / 10000.0; };
  static const int FIREC[] = {0xFFF4C0, 0xFFD050, 0xFFA030, 0xFF7020, 0xE04818, 0xA02810};
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x) {
      int i = y * W + x, t2 = T(i);
      if (!inside(x, y)) continue;
      if (t2 == data::FIRE) {
        // the core is hot and white, the edge red; every cell dances on its own beat
        double core = std::hypot((x - fcx) / 1.6, double(y - fcy));
        int step = int(tt * 4.5 + noise(i) * 6);
        int c = std::clamp(int(core * 2.2) + int(noise(i * 31 + step) * 2.4), 0, 5);
        int ch = (step + i) % 3 == 0 ? '^' : (step + i) % 3 == 1 ? 0x25B2 : '*';
        put(sx(x), sy(y), ch, FIREC[c], mixc(0x5A1804, 0xA04008, float(noise(i * 7 + step))));
      } else if (t2 == data::TORCH || t2 == data::CANDLE) {
        int step = int(tt * 3.5 + noise(i) * 5);
        int fg = mixc(t2 == data::TORCH ? 0xFFB040 : 0xFFE6A0, 0xFF6020, float(noise(i + step * 13)) * .6f);
        put(sx(x), sy(y), data::TILES[t2].ch, fg, scale(0x3A1A06, .6 + .4 * noise(step + i)));
        if (t2 == data::TORCH && inside(x, y - 1) && !data::blocks(T(i - W)) && noise(step * 3 + i) < .45)
          put(sx(x), sy(y - 1), noise(step + i * 5) < .5 ? '\'' : 0x00B7, 0xFF9030);  // a lick of flame above
      } else if (t2 == data::LIT) {
        double f = .75 + .25 * std::sin(tt * 5 + x * 1.7 + y);
        put(sx(x), sy(y), data::TILES[t2].ch, scale(0xFFB040, f), scale(0x4A2808, f));
      } else if (t2 == data::CRACK || t2 == data::EMBER) {
        double ph = std::fmod(tt * .7 + (x * 13 + y * 7) % 10 * .1, 1.0);
        int up = int(ph * 4), ex = x + (((x + y) & 1) ? (up > 2) : -(up > 2)), ey = y - 1 - up;
        if (inside(ex, ey) && !data::blocks(T(ey * W + ex))) put(sx(ex), sy(ey), ph < .5 ? 0x2022 : 0x00B7, scale(0xFF8030, 1 - ph * .8));
      }
    }
  // smoke: from the great fire and the chimneys, rising and leaning with the wind
  auto smoke = [&](P o, int k, int n, double rise) {
    for (int j = 0; j < n; ++j) {
      double period = 4 + noise(k * 17 + j) * 3, ph = std::fmod(tt / period + noise(k * 5 + j * 11), 1.0);
      int x = o.x + int(std::round(ph * ph * 7 + std::sin(tt * .9 + j) * ph * 1.5 + (noise(j + k) - .5) * 2));
      int y = o.y - 1 - int(ph * rise);
      if (!inside(x, y)) continue;
      put(sx(x), sy(y), ph < .35 ? 0x2592 : 0x2591, mixc(0x8A8478, 0x2A2A30, float(ph)));
    }
  };
  int k = 0;
  for (auto& c : smokes) smoke(c, ++k, 5, 9);
  if (fcx >= 0) {
    smoke({fcx, fcy - 1}, 99, 10, 13);
    if (town_fx() < 2)
    // sparks: thrown up from the fire, drifting, cooling from yellow to red to nothing
    for (int j = 0; j < 26; ++j) {
      double period = .9 + noise(j * 3) * 1.4, ph = std::fmod(tt / period + noise(j * 7), 1.0);
      P o = fires[size_t(noise(j * 13 + int(tt / period)) * fires.size()) % fires.size()];
      int x = o.x + int(std::round(std::sin(tt * 2.3 + j) * ph * 2 + (noise(j) - .5) * ph * 4));
      int y = o.y - 1 - int(ph * (4 + noise(j * 5) * 6));
      if (!inside(x, y) || data::blocks(T(y * W + x))) continue;
      put(sx(x), sy(y), ph < .3 ? '*' : ph < .7 ? 0x00B7 : '.', FIREC[std::min(5, 1 + int(ph * 5))]);
    }
  }
  // snow: about one flake in forty cells, falling and drifting; it melts before it reaches the fire
  for (int k2 = 0; k2 < (town_fx() == 2 ? 0 : W * H / 40); ++k2) {
    unsigned hsh = k2 * 2654435761u;
    double speed = 1.5 + (hsh >> 8) % 100 / 60.0;
    int x = int(hsh % W + std::sin(tt * .8 + k2) * 1.5 + tt * .6) % W;
    int y = int((hsh >> 16) % H + tt * speed) % H;
    if (x < 0 || !inside(x, y)) continue;
    if (std::hypot((x - fcx) / 1.6, double(y - fcy)) < 7) continue;
    int t2 = T(y * W + x);
    if (t2 == data::FLAGS || (data::blocks(t2) && t2 != data::GRAVE && t2 != data::CROSS && t2 != data::TREE)) continue;
    put(sx(x), sy(y), (k2 % 7) ? 0x00B7 : '*', (k2 % 7) ? 0xB8C0D4 : 0x7A8498);
  }
}

// Finer than a cell: snow and ash in three layers, sparks from the great fire, the breath of the heroes in the cold
void Client::draw_town_particles(SDL_Renderer* r, double ox, double oy, double cell) {
  if (town_fx() < 2) return;
  const std::string& map = maps_[snap_["floor"].get<int>()];
  double tt = now(), px = cell / 8;
  int fcx = town_fire_.x, fcy = town_fire_.y;
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  auto dot = [&](double x, double y, int c, double a, double size = 1) {
    double X = x - camx_ + vx_, Y = y - camy_ + vy_;
    if (X < vx_ || Y < vy_ || X >= vx_ + vw_ || Y >= vy_ + vh_) return;
    SDL_SetRenderDrawColor(r, (c >> 16) & 255, (c >> 8) & 255, c & 255, Uint8(std::clamp(a, 0.0, 1.0) * 255));
    SDL_FRect rc{float(ox + X * cell), float(oy + Y * cell), float(px * size), float(px * size)};
    SDL_RenderFillRect(r, &rc);
  };
  auto nz = [](int k) { unsigned h = unsigned(k) * 2654435761u; h ^= h >> 13; h *= 0x5bd1e995u; return (h ^ (h >> 15)) % 10000 / 10000.0; };
  auto open = [&](double x, double y) {
    int ix = int(std::floor(x)), iy = int(std::floor(y));
    if (ix < 0 || iy < 0 || ix >= W || iy >= H) return false;
    int t = tile_of(map[iy * W + ix]);
    return t != data::WALL && t != data::CHIMNEY && t != data::WINDOW && t != data::LIT;
  };
  // snow in three layers, falling slowly and swaying as it goes: the near flakes soft and star-shaped, the middle ones
  // small, the far ones fine and dim; now and then a grey flake of ash among the far ones
  const double u = 1.0 / 8;  // one pixel of the font, in cells
  for (int layer = 0; layer < 3; ++layer) {
    int n = layer == 0 ? 110 : layer == 1 ? 280 : 380;
    double sp = layer == 0 ? 1.1 : layer == 1 ? .75 : .45;
    for (int k = 0; k < n; ++k) {
      int id = layer * 1000 + k;
      bool ash = layer == 2 && k % 12 == 0;
      double fall = sp * (.8 + nz(id * 3) * .45);
      double sway = std::sin(tt * (.5 + nz(id * 5) * .5) + k * 1.3) * (.4 + layer * .15 + nz(id * 9) * .4);
      double x = camx_ + std::fmod(nz(id) * 140 + tt * .25 * sp, 140.0) - 10 + sway;
      double y = camy_ + std::fmod(nz(id * 7) * 80 + tt * fall, 80.0) - 10;
      double fd = std::hypot((x - fcx) / 1.6, y - fcy);
      if (fd < 9 || !open(x, y)) continue;  // melted in the heat, or behind a wall
      double a = std::clamp((fd - 9) / 3, 0.0, 1.0);
      if (ash) { dot(x, y, 0x8A8478, .5 * a); continue; }
      if (layer == 0) {  // a soft flake: a bright heart and four faint points
        dot(x - u, y - u, 0xDCE4F2, .35 * a, 5);  // a soft glow round it
        dot(x, y, 0xF8FAFF, .95 * a, 3);
        dot(x - 2 * u, y + u, 0xE0E8F4, .55 * a);
        dot(x + 4 * u, y + u, 0xE0E8F4, .55 * a);
        dot(x + u, y - 2 * u, 0xE0E8F4, .55 * a);
        dot(x + u, y + 4 * u, 0xE0E8F4, .55 * a);
      } else if (layer == 1) dot(x, y, 0xE8EEF8, .85 * a, 2);
      else dot(x, y, 0xA8B2C8, .5 * a);
    }
  }
  if (fcx >= 0) {
    // sparks: thrown up out of the fire, wobbling on the heat, cooling from white to gold to red to nothing
    for (int j = 0; j < 60; ++j) {
      double period = 1.2 + nz(j * 3) * 1.8, ph = std::fmod(tt / period + nz(j * 7), 1.0);
      double x = fcx + .5 + (nz(j * 11 + int(tt / period)) - .5) * 3 + std::sin(tt * 3 + j) * ph * 1.2 + ph * ph * 2;
      double y = fcy + .3 - ph * (5 + nz(j * 5) * 7);
      int c = ph < .2 ? 0xFFF4C0 : ph < .5 ? 0xFFC040 : ph < .8 ? 0xFF6A20 : 0xA02810;
      dot(x, y, c, 1 - ph * .8, j % 9 == 0 && ph < .3 ? 2 : 1);
    }
  }
  // breath: every two seconds or so each hero breathes out a little cloud that drifts off and thins
  for (auto& h : snap_["heroes"]) {
    if (h.value("gone", false) || h.value("dead", false)) continue;
    int hx = h["x"], hy = h["y"], sd = h.value("pid", 1);
    double ph = std::fmod(tt / 2.4 + sd * .37, 1.0);
    if (ph > .6) continue;
    double k = ph / .6;
    for (int j = 0; j < 7; ++j) {
      double x = hx + .75 + k * (1 + nz(sd * 9 + j) * .9) + (nz(j) - .5) * k * .7, y = hy + .25 - k * (.5 + nz(j * 3) * .6);
      dot(x, y, 0xD8DCE4, .55 * (1 - k), k > .5 ? 2 : 1);
    }
  }
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}
