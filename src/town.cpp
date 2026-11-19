// Charnfen at night: the last village, drawn by hand (ref/charnfen.txt). Everything is dark but living flame, each
// source flickering on its own; the great fire on the square throws sparks and smoke, chimneys smoke, snow falls and
// melts before it reaches the fire.
#include <algorithm>
#include <cmath>

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
  for (int k2 = 0; k2 < W * H / 40; ++k2) {
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
