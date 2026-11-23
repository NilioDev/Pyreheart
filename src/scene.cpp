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

// the newer air of the other zones, all on by default; one by one with PYREHEART_ZONEFX=c2,d2, off for none.
// c2 the Crypt's icy ghosts; d1 heat over the lava, d2 the Deep breathing; s1 the light
// the Cistern's water throws back, s2 bubbles of gas in the sludge; t2 gusts of wind in Charnfen, t3 its fire flaring;
// q1 the halls shuddering now and then (all zones but the Caves and the Deep, which have their own);
// v1 ash seeping under the Vaults' doors, v2 parchment flakes off the scroll drifts;
// b1 soot puffing up under the Barracks' boots, b2 flakes off the banners' tatters, b3 a war-drum far off;
// p1 a cell door rattling or chains clinking in the Prison; n1 ash welling out of the Sanctum's hole, n2 its incense haze;
// k1 the City's ash fall, k3 the ancestors weeping soot, k4 the air shafts' breath, k5 shadows at the shutters
// for the mock-ups: PYREHEART_FXPERIOD=4 makes the rare things (the Crypt's ghosts, the fire flaring in Charnfen) come every 4 s
double fx_period(double p) {
  static const char* e = SDL_getenv("PYREHEART_FXPERIOD");
  return e ? std::atof(e) : p;
}
bool zone_fx(const char* code) {
  static std::string v = SDL_getenv("PYREHEART_ZONEFX") ? SDL_getenv("PYREHEART_ZONEFX") : "all";
  return v == "all" || v.find(code) != std::string::npos;
}

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
// value noise at any point, for mist that drifts
double vnoise(double x, double y) {
  int x0 = int(std::floor(x)), y0 = int(std::floor(y));
  double fx = x - x0, fy = y - y0;
  auto v = [&](int i, int j) { return (hash(i, j, 77) % 1000) / 1000.0; };
  fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
  double a = v(x0, y0) + (v(x0 + 1, y0) - v(x0, y0)) * fx, b = v(x0, y0 + 1) + (v(x0 + 1, y0 + 1) - v(x0, y0 + 1)) * fx;
  return a + (b - a) * fy;
}
// the Crypt's new air, for the mock-ups: PYREHEART_CRYPTFX=1
// the Crypt's mist and particles; PYREHEART_CRYPTFX=off leaves them out
bool crypt_fx() {
  static bool on = !SDL_getenv("PYREHEART_CRYPTFX") || std::string(SDL_getenv("PYREHEART_CRYPTFX")) != "off";
  return on;
}
const int GLASS[] = {0xC83848, 0x3C5AD0, 0xD8A840, 0x6A3CB0};
// the Caves' air, all on by default; one by one with PYREHEART_CAVESFX=1,3 (1 mist over the stream, 2 heat over the
// fissures, 3 the mountain groans, 4 eyes in the dark, 5 wet rock by the stream, 6 draught); off leaves them all out
bool cave_fx(int n) {
  static std::string v = SDL_getenv("PYREHEART_CAVESFX") ? SDL_getenv("PYREHEART_CAVESFX") : "all";
  return v == "all" || v.find(char('0' + n)) != std::string::npos;
}
double flick(double t, int seed) { return .86 + .08 * std::sin(t * 4.1 + seed * 1.7) + .06 * std::sin(t * 9.7 + seed * 3.1); }
const int FIREC[] = {0xFFF4C0, 0xFFD050, 0xFFA030, 0xFF7020, 0xE04818, 0xA02810};
bool tombish(int t) { return t == TOMB || t == CRACKED || t == OPENTOMB; }
bool bierish(int t) { return t == BIER || t == CORPSE || t == RESTLESS; }
// now and then a draught comes down the Forge's halls: 0 most of the time, rising to 1 and falling again
// (PYREHEART_DRAUGHT=seconds sets how often, for recording)
const double DRAUGHT = SDL_getenv("PYREHEART_DRAUGHT") ? std::atof(SDL_getenv("PYREHEART_DRAUGHT")) : 13.0;
double draught(double tt) {
  double ph = std::fmod(tt / DRAUGHT, 1.0);
  return ph < .3 ? std::sin(ph / .3 * PI) : 0;
}
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
  bool forge = actn == 2 && !rest;
  if (forge && (int(hot_.size()) != W * H || ldist_floor_ != fl || hot_floor_ != fl)) {
    hot_floor_ = fl;
    hot_.assign(W * H, 0);
    for (int i = 0; i < W * H; ++i) {
      if (tile_of(map[i]) == LAVA || blocks(tile_of(map[i]))) continue;
      int x = i % W, y = i / W, best = 0;
      for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -2; dx <= 2; ++dx)
          if (tile(x + dx, y + dy) == LAVA) { int d = std::max(std::abs(dx), std::abs(dy)); if (!best || d < best) best = d; }
      hot_[i] = char(best);
    }
  }
  double gust = forge ? draught(tt) : 0;
  double glow = .35 + .2 * std::sin(tt * .8);
  bool cfx = actn == 0 && crypt_fx() && !rest;
  if (cfx && chill_floor_ != fl) {
    chill_floor_ = fl;
    // the grave chill spreads over open ground from open tombs and walls of bones, six steps at most
    chill_.assign(W * H, 0.f);
    std::vector<int> d(W * H, -1), q;
    for (int i = 0; i < W * H; ++i) {
      int t = tile_of(map[i]);
      if (t == OPENTOMB || t == BONEWALL || t == CRACKED) { d[i] = 0; q.push_back(i); }
    }
    for (size_t k = 0; k < q.size(); ++k) {
      int i = q[k];
      if (d[i] >= 6) continue;
      for (P d4 : {P{1, 0}, P{-1, 0}, P{0, 1}, P{0, -1}}) {
        int x = i % W + d4.x, y = i / W + d4.y;
        if (x < 0 || y < 0 || x >= W || y >= H || d[y * W + x] >= 0 || blocks(tile_of(map[y * W + x]))) continue;
        d[y * W + x] = d[i] + 1;
        q.push_back(y * W + x);
      }
    }
    for (int i = 0; i < W * H; ++i) if (d[i] > 0) chill_[i] = float(1 - (d[i] - 1) / 6.0);
    // three high windows in the wall behind each chapel altar, their light falling slantwise into the chapel
    beams_.clear();
    for (int i = 0; i < W * H; ++i) {
      int x = i % W, y = i / W;
      if (tile_of(map[i]) != ALTAR || tile(x - 1, y) == ALTAR || tile(x + 1, y) != ALTAR) continue;
      int a = tile(x + 2, y) == ALTAR ? 3 : 2, dir = tile(x, y - 1) == WALL ? 1 : tile(x, y + 1) == WALL ? -1 : 0;
      if (!dir) continue;
      int wy = y - dir, depth = 0;
      while (depth < 10 && !blocks(tile(x, y + dir * (depth + 1)))) ++depth;
      double cx = x + (a - 1) / 2.0;
      for (int k = -1; k <= 1; ++k) {
        int wx = int(std::lround(cx + k * 3 - (a == 2 && k ? .5 * k : 0)));
        if (tile(wx, wy) != WALL) continue;
        beams_.push_back({wx, wy, std::max(4, depth), GLASS[(k + 1 + x) % 4], dir});
      }
    }
  }
  bool cvx = actn == 1 && !rest;
  if (actn == CISTERN && !rest && cave_floor_ != fl) {
    // the Cistern: which floor and wall lies by the water, for the light it throws back
    cave_floor_ = fl;
    auto wet = [](int t) { return t == SLUDGE || t == FLOATER || t == MIRE || t == WATER || t == SHALLOW; };
    damp_.assign(W * H, 0.f); wetw_.assign(W * H, 0); draft_.assign(W * H, 0.f);
    for (int i = 0; i < W * H; ++i) {
      int x = i % W, y = i / W, t = tile_of(map[i]);
      if (wet(t)) continue;
      int best = 9;
      for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx) if (wet(tile(x + dx, y + dy))) best = std::min(best, std::max(std::abs(dx), std::abs(dy)));
      if (best <= 2) damp_[i] = float(1 - (best - 1) * .45);
    }
  }
  if (cvx && cave_floor_ != fl) {
    cave_floor_ = fl;
    wind_ = hash(fl, 5, 9) % 2 ? 1 : -1;
    auto spread = [&](auto src, int reach, std::vector<float>& out) {
      out.assign(W * H, 0.f);
      std::vector<int> d(W * H, -1), q;
      for (int i = 0; i < W * H; ++i) if (src(tile_of(map[i]))) { d[i] = 0; q.push_back(i); }
      for (size_t k = 0; k < q.size(); ++k) {
        int i = q[k];
        if (d[i] >= reach) continue;
        for (P d4 : {P{1, 0}, P{-1, 0}, P{0, 1}, P{0, -1}}) {
          int x = i % W + d4.x, y = i / W + d4.y;
          if (x < 0 || y < 0 || x >= W || y >= H || d[y * W + x] >= 0 || blocks(tile_of(map[y * W + x]))) continue;
          d[y * W + x] = d[i] + 1;
          q.push_back(y * W + x);
        }
      }
      for (int i = 0; i < W * H; ++i) if (d[i] >= 0) out[i] = float(1 - d[i] / double(reach + 1));
    };
    auto wet = [](int t) { return t == WATER || t == SHALLOW || t == PLANK; };
    spread(wet, 3, damp_);
    spread([](int t) { return t == BEAM || t == HEAP; }, 5, draft_);
    wetw_.assign(W * H, 0);
    for (int i = 0; i < W * H; ++i) {
      if (tile_of(map[i]) != WALL) continue;
      int x = i % W, y = i / W;
      for (int dy = -2; dy <= 2 && !wetw_[i]; ++dy)
        for (int dx = -2; dx <= 2; ++dx) if (wet(tile(x + dx, y + dy))) { wetw_[i] = 1; break; }
    }
  }
  // now and then the zone stirs as a whole: the mountain groans (Caves), a cold draught goes through the Crypt, the
  // Deep breathes, water roars behind the Cistern's walls
  double groan = 0, ev_age = 99;
  // everywhere else the halls shudder: the light flickers and dust, grit and stones come down
  static const char* SHUDDER[] = {"The crypt shudders.", nullptr, "The forge shudders.", nullptr, "The cistern shudders.",
                                  "The city shudders.", "The vaults shudder.", "The fortress shudders.", "The prison shudders.",
                                  "The temple shudders."};
  const char* ev_text = s.value("town", false) ? nullptr
                      : actn == 1 && cave_fx(3) ? "The mountain groans." : actn == 3 && zone_fx("d2") ? "The Deep breathes."
                      : actn >= 0 && actn < 10 && zone_fx("q1") ? SHUDDER[actn] : nullptr;
  if (!rest && ev_text) {
    static const char* every = SDL_getenv("PYREHEART_GROAN") ? SDL_getenv("PYREHEART_GROAN") : SDL_getenv("PYREHEART_EVENT");  // for the mock-ups: every n seconds
    if (groan_next_ < 0) groan_next_ = tt + (every ? 1.5 : 40 + nz(int(tt) % 997) * 50);
    if (tt >= groan_next_) {
      groan_t_ = tt;
      groan_next_ = tt + (every ? std::atof(every) : 45 + nz(int(tt * 7) % 997) * 60);
      log_.push_back({ev_text, 0xA0A0A0});
    }
    ev_age = tt - groan_t_;
    if (actn != 3 && ev_age < 2.6) groan = (1 - ev_age / 2.6) * (.5 + .5 * std::abs(std::sin(ev_age * 19)));
  }
  // the Barracks: far off, a war-drum booms twice that nobody beats; dust trickles down from the low ceilings
  if (!rest && actn == BARRACKS && zone_fx("b3") && !s.value("town", false)) {
    static const char* every = SDL_getenv("PYREHEART_DRUM");  // for the mock-ups: every n seconds
    if (drum_next_ < 0) drum_next_ = tt + (every ? 1.0 : 25 + nz(int(tt) % 991) * 30);
    if (tt >= drum_next_) {
      drum_t_ = tt;
      drum_next_ = tt + (every ? std::atof(every) : 35 + nz(int(tt * 5) % 991) * 45);
      log_.push_back({"A war-drum booms, far off.", 0xA0A0A0});
    }
  }
  // the Prison, empty for centuries: now and then a draught swings a cell door, or old chains clink in the dark
  if (!rest && actn == PRISON && zone_fx("p1") && !s.value("town", false)) {
    static const char* every = SDL_getenv("PYREHEART_RATTLE");  // for the mock-ups: every n seconds
    if (rattle_next_ < 0) rattle_next_ = tt + (every ? 1.0 : 20 + nz(int(tt) % 983) * 25);
    if (tt >= rattle_next_) {
      rattle_t_ = tt;
      rattle_door_ = !rattle_door_;
      rattle_next_ = tt + (every ? std::atof(every) : 30 + nz(int(tt * 3) % 983) * 40);
      log_.push_back({rattle_door_ ? "A cell door creaks on its hinges in a draught." : "Old chains clink somewhere in the dark.", 0xA0A0A0});
    }
  }
  double rattle = actn == PRISON && tt - rattle_t_ < 1.6 ? 1 - (tt - rattle_t_) / 1.6 : 0;
  double breath = actn == 3 && ev_age < 3.5 ? std::sin(ev_age / 3.5 * PI) : 0;  // the Deep: the lava swells and glows

  // how much of a window's light falls on a cell, and in what colour
  auto beam_at = [&](int x, int y, int& col) {
    double best = 0;
    for (auto& b : beams_) {
      int st = (y - b.wy) * b.dir;
      if (st <= 0 || st > b.len) continue;
      double cx = b.wx + st * .45, dd = std::abs(x - cx);
      if (dd > 1.1) continue;
      double k = (1 - dd / 1.1) * (1 - .45 * st / double(b.len));
      if (k > best) { best = k; col = b.col; }
    }
    return best;
  };

  // the light: a dim base, pools round every flame, and each hero's own light
  static const bool whole_view = SDL_getenv("PYREHEART_WHOLE") != nullptr;  // a screenshot of the whole map
  double base = rest ? .9 : whole_view ? .8 : actn == VAULTS ? .46 : .6;  // the Vaults are pitch dark: only your own light
  std::vector<float> L(W * H, float(base)), warm(W * H, 0.f), cold(W * H, 0.f);
  std::vector<P> heaps;  // the Caves' fallen shafts, with grey daylight on them
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
        if (forge && vis[i] == '1' && hash(x, y, 35) % 9 == 0) emits_.push_back({float(x), float(y), 'K', sd});  // sparks off the channel
      } else if (t == SHALLOW && vis[i] == '1' && hash(x, y, 5) % 19 == 0) {
        emits_.push_back({float(x), float(y), 'w', sd});
      } else if (t == CRACKED && vis[i] == '1') {
        emits_.push_back({float(x), float(y), 's', sd});
      } else if (cfx && t == ALTAR && tile(x - 1, y) != ALTAR && tile(x + 1, y) == ALTAR) {
        float mid = x + (tile(x + 2, y) == ALTAR ? 1.f : .5f);
        if (vis[i] == '1') emits_.push_back({mid, float(y), 'i', sd});  // incense from the middle of the altar
      } else if (cfx && t == RESTLESS && vis[i] == '1') {
        emits_.push_back({float(x), float(y), 'b', sd});  // will-o'-wisps round a corpse that will not lie still
      } else if (cfx && (t == BIER || t == CORPSE) && vis[i] == '1' && hash(x, y, 18) % 3 == 0) {
        emits_.push_back({float(x), float(y), 'j', sd});  // the fumes of the embalmers' jars
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
        if (actn != 1) pool(x, y, 2.5, .85, .5, true);
      } else if (t == HEAP && actn == 1) {
        // grey daylight down the fallen shaft, onto the heap and round it
        pool(x + .5, y + .5, 3.8, 1.05, 0);
        heaps.push_back({x, y});
        if (vis[i] == '1' && tile(x, y - 1) != HEAP) emits_.push_back({float(x), float(y), 'G', sd});
      } else if (t == CAMPFIRE) {
        pool(x, y, 6 * f, 1.2 * f, 1.);
        if (vis[i] == '1') { emits_.push_back({float(x), float(y), 'k', sd}); emits_.push_back({float(x), float(y), 'c', sd}); }
      } else if (t == STALAG && vis[i] == '1' && hash(x, y, 27) % 2) {
        emits_.push_back({float(x), float(y), 'r', sd});
      } else if (t == CHASM && actn == 1 && vis[i] == '1' && hash(x, y, 28) % 7 == 0 && !blocks(tile(x - 1, y))) {
        emits_.push_back({float(x), float(y), 'P', sd});
      } else if (t == ORE && vis[i] == '1') {
        emits_.push_back({float(x), float(y), 'o', sd});
      } else if (t == BEAM && actn == 1 && vis[i] == '1') {
        emits_.push_back({float(x), float(y), 'x', sd});
      } else if (actn == 1 && t == FLOOR && vis[i] == '1' && hash(x, y, 23) % 83 == 0) {
        emits_.push_back({float(x), float(y), 'r', sd});
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
        if (actn == CISTERN && t == SLUDGE) {
          // in the great basins: grey ash floating on the sludge with the slow current, sour vapour curling off it
          bool basin = false;
          for (int dy = -4; dy <= 4 && !basin; ++dy) for (int dx = -4; dx <= 4; ++dx) if (tile(x + dx, y + dy) == BASIN) { basin = true; break; }
          if (basin && hash(x, y, 41) % 4 == 0) emits_.push_back({float(x), float(y), 'Q', sd});
          if (basin && hash(x, y, 42) % 11 == 0) emits_.push_back({float(x), float(y), 'W', sd});
        }
      } else if (actn == CISTERN && (t == FLOOR || t == GRATE) && vis[i] == '1') {
        if (hash(x, y, 16) % 47 == 0) emits_.push_back({float(x), float(y), 'r', sd});
      } else if (t == EMBERS) {
        // the old embers: a little dull red light, a thin smoke, brighter while the draught blows
        if (hash(x, y, 31) % 3) pool(x, y, 2.4 + 1.5 * gust, base + .3 + .3 * gust, .7);
        if (vis[i] == '1' && hash(x, y, 32) % 3 == 0) emits_.push_back({float(x), float(y), 'g', sd});
      } else if (actn == VAULTS && (t == VAULTDOOR || (t == DOOR && hash(x, y, 39) % 2 == 0))) {
        if (vis[i] == '1' && zone_fx("v1")) emits_.push_back({float(x), float(y), 'V', sd});  // ash seeping in under the door
      } else if (actn == VAULTS && t == SCROLLDRIFT && vis[i] == '1' && zone_fx("v2")) {
        for (auto& hh2 : s["heroes"])
          if (cheb(x, y, hh2["x"].get<int>(), hh2["y"].get<int>()) <= 4) { emits_.push_back({float(x), float(y), 'N', sd}); break; }
      } else if (actn == VAULTS && t == BINDRUNE) {
        // a rune of binding: the curse still holds, and the rune breathes a cold violet
        double g = .5 + .5 * std::sin(tt * .9 + x);
        pool(x, y, 1.8, base + .12 * g, .5, true);
      } else if (actn == SANCTUM && t == CHASM && vis[i] == '1') {
        pool(x, y, 2.2, base + .12, .8);  // a little red light up out of the hole
        if (hash(x, y, 35) % 3 == 0 && zone_fx("n1")) emits_.push_back({float(x), float(y), 'S', sd});  // the wound still bleeds ash
      } else if (actn == SANCTUM && t == CENSER && vis[i] == '1') {
        emits_.push_back({float(x), float(y), 'g', sd});  // bone-dust smoking out of the censer
      } else if (actn == SANCTUM && t == FLOOR && vis[i] == '1' && hash(x, y, 33) % 71 == 0) {
        emits_.push_back({float(x), float(y), 'd', sd});
      } else if (actn == PRISON && t == MAIDEN && vis[i] == '1') {
        emits_.push_back({float(x), float(y), 'J', sd});  // dark drops out of its seams
      } else if (actn == PRISON && t == BRAZIER && vis[i] == '1') {
        emits_.push_back({float(x), float(y), 'g', sd});  // smoke hanging low over the irons
      } else if (actn == PRISON && t == FLOOR && vis[i] == '1' && hash(x, y, 33) % 71 == 0) {
        emits_.push_back({float(x), float(y), 'd', sd});  // dust in the torchlight
      } else if (actn == PRISON && t == SHALLOW && vis[i] == '1' && hash(x, y, 34) % 5 == 0) {
        emits_.push_back({float(x), float(y), 'J', sd + 7777});  // water dripping into the flooded cells
      } else if (actn == BARRACKS && t == HANGING && vis[i] == '1' && zone_fx("b2")) {
        emits_.push_back({float(x), float(y), 'F', sd});  // black flakes coming off a banner's tatters
      } else if (actn == BARRACKS && t == FLOOR && vis[i] == '1' && hash(x, y, 33) % 71 == 0) {
        emits_.push_back({float(x), float(y), 'd', sd});  // soot hanging in the air
      } else if (actn == VAULTS && t == FLOOR && vis[i] == '1' && hash(x, y, 33) % 61 == 0) {
        emits_.push_back({float(x), float(y), 'd', sd});  // dust hanging in the dark
      } else if (actn == CITY && t == POST) {
        // the ember left in the lantern's cage: a small red light, now and then a little brighter
        double fl2 = .5 + .5 * std::sin(tt * .7 + sd) * std::sin(tt * .23 + sd * 3);
        pool(x, y, 1.8 + 1.2 * fl2, base + .2 + .25 * fl2, .9);
        if (vis[i] == '1') emits_.push_back({float(x), float(y), 'c', sd});  // a thin smoke, like the other zones' lamps
      } else if (actn == CITY && t == ANCESTOR && vis[i] == '1' && zone_fx("k3")) {
        emits_.push_back({float(x), float(y), 'T', sd});  // the ancestors weep soot
      } else if (actn == CITY && t == GRATE && vis[i] == '1' && zone_fx("k4")) {
        if (hash(x, y, 36) % 2 == 0) emits_.push_back({float(x), float(y), 'Y', sd});  // a sour breath out of the air shafts
      } else if (forge && t == FLOOR && vis[i] == '1') {
        // the Forge's air: dust turning in the light, ash and soot coming down
        if (hash(x, y, 33) % 61 == 0) emits_.push_back({float(x), float(y), 'd', sd});
        if (hash(x, y, 34) % 67 == 0) emits_.push_back({float(x), float(y), 'A', sd});
      } else if ((t == CRUST || t == NECK) && vis[i] == '1') {
        // smoke seeping out of the crust, red embers drifting in loops, ash falling
        if (hash(x, y, 12) % 37 == 0) emits_.push_back({float(x), float(y), 'g', sd});
        if (hash(x, y, 13) % 29 == 0) emits_.push_back({float(x), float(y), 'e', sd});
        if (hash(x, y, 14) % 61 == 0) emits_.push_back({float(x), float(y), 'a', sd});
      }
    }
  if (cvx)
    for (int y = y0; y < y1; ++y)
      for (int x = x0; x < x1; ++x) {
        int i = y * W + x, t = tile(x, y), sd = int(hash(x, y) % 97);
        if (vis[i] != '1') continue;
        if (cave_fx(2) && t == FISSURE) {
          pool(x, y, 1.8 * flick(tt * .7, sd), .95, .9);
          emits_.push_back({float(x), float(y), 'H', sd});
        }
        if (cave_fx(6) && !draft_.empty() && draft_[i] > 0 && !blocks(t) && hash(x, y, 31) % 3 == 0)
          emits_.push_back({float(x), float(y), 'D', sd});
      }
  if (!rest && (actn == 3 || actn == CISTERN))
    for (int y = y0; y < y1; ++y)
      for (int x = x0; x < x1; ++x) {
        int i = y * W + x, t = tile(x, y), sd = int(hash(x, y) % 97);
        if (vis[i] != '1') continue;
        if (actn == 3 && zone_fx("d1") && t == LAVA && ldist_[i] <= 2 && hash(x, y, 33) % 3 == 0) emits_.push_back({float(x), float(y), 'H', sd});
        if (actn == 3 && breath > .3 && t == LAVA && hash(x, y, 34) % 17 == 0) emits_.push_back({float(x), float(y), 'E', sd});
        if (actn == CISTERN && zone_fx("s2") && (t == SLUDGE || t == MIRE) && hash(x, y, 35) % 9 == 0) emits_.push_back({float(x), float(y), 'B', sd});
      }
  if (s.contains("ground"))
    for (auto& g : s["ground"])
      if (g[2] == 0) {
        int gx = g[0], gy = g[1];
        pool(gx, gy, 3 * flick(tt, gx * 7 + gy), 1.1, .9);
        if (vis[gy * W + gx] == '1') emits_.push_back({float(gx), float(gy), 'f', gx * 7 + gy});
      }
  if (s.contains("embers"))
    for (auto& e : s["embers"]) {
      int ex = e[0], ey = e[1];
      pool(ex, ey, 3 * flick(tt, ex * 11 + ey), 1.1, .9);
      if (vis[ey * W + ex] == '1') emits_.push_back({float(ex), float(ey), 'f', ex * 11 + ey});
    }
  for (auto& h : s["heroes"])
    if (!h.value("gone", false) && !h.value("dead", false)) {
      int hx = h["x"].get<int>(), hy = h["y"].get<int>();
      pool(hx, hy, 6, 1.25, 0);
      if (h.value("carry", 0) > 0) {  // the ember in their hands lights the way
        pool(hx, hy, 9 * flick(tt, hx * 5 + hy), 1.2, .9);
        emits_.push_back({float(hx), float(hy), 'f', hx * 5 + hy});
      }
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

  // the Crypt's ghosts: now and then a pale, icy shape drifts through the halls and the walls, leaving frost behind it
  ghost_a_ = 0;
  std::vector<float> frost;
  std::vector<int> under;
  if (actn == 0 && zone_fx("c2") && !rest && !s["heroes"].empty()) {
    // a ghost is rare: one chance in three every 25 s, never two cycles running (about one every two minutes; it used to be every 20 s);
    // with PYREHEART_FXPERIOD set (the mock-ups) one comes every cycle
    double period = fx_period(25), cyc = std::floor(tt / period), age = tt - cyc * period, dur = std::min(8.0, period * .85);
    static const bool every = SDL_getenv("PYREHEART_FXPERIOD") != nullptr;
    auto comes = [](int c) { return nz(c * 613 + 5) < .33; };
    bool here = every || (comes(int(cyc)) && !comes(int(cyc) - 1));
    if (here && age < dur) {
      int seed = int(cyc) * 613, hx = s["heroes"][0]["x"], hy = s["heroes"][0]["y"];
      static int start_cyc = -1;
      static double sx = 0, sy = 0, ex = 0, ey = 0;
      if (start_cyc != int(cyc)) {
        start_cyc = int(cyc);
        double side = nz(seed) < .5 ? -1 : 1;
        sx = hx + side * 7; sy = hy + (nz(seed + 1) - .5) * 6; ex = hx - side * 7; ey = hy + (nz(seed + 2) - .5) * 6;
      }
      auto at = [&](double a, double& x, double& y) { double q = a / dur; x = sx + (ex - sx) * q; y = sy + (ey - sy) * q + std::sin(q * 5 + seed) * .7; };
      double k = age / dur;
      at(age, ghost_x_, ghost_y_);
      ghost_dx_ = ex > sx ? 1 : -1;
      ghost_a_ = std::clamp(std::min(k / .15, (1 - k) / .15), 0.0, 1.0);
      frost.assign(W * H, 0.f);
      under.assign(W * H, 0);
      // its cold glow round it, like the ghosts of old films
      for (int dy = -3; dy <= 3; ++dy)
        for (int dx = -4; dx <= 4; ++dx) {
          int ix = int(std::floor(ghost_x_ + .5)) + dx, iy = int(std::floor(ghost_y_ + .5)) + dy;
          if (ix < 0 || iy < 0 || ix >= W || iy >= H) continue;
          double d = std::hypot(dx / 1.3, double(dy));
          if (d < 3.2) frost[iy * W + ix] = std::max(frost[iy * W + ix], float((1 - d / 3.2) * .9 * ghost_a_));
        }
      for (double back = 0; back <= 3 && back <= age; back += .2) {
        double fx2, fy2;
        at(age - back, fx2, fy2);
        for (int dy = -1; dy <= 0; ++dy) {
          int ix = int(std::floor(fx2 + .5)), iy = int(std::floor(fy2 + .5)) + dy;
          if (ix < 0 || iy < 0 || ix >= W || iy >= H) continue;
          frost[iy * W + ix] = std::max(frost[iy * W + ix], float((1 - back / 3) * ghost_a_));
        }
      }
      static int said = -1;
      if (said != int(cyc) && k > .3) {
        said = int(cyc);
        if (nz(seed + 3) < .5) log_.push_back({"The air turns to ice as something passes.", 0x8AA8D0});
      }
    }
  }

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
          // every zone has its own wall: its own signs and colours, one cell at a time (walls are one cell thick);
          // the laid blocks in courses belong to the Caves (and the village)
          double r = (hash(x, y, 11) % 1000) / 1000.0;
          // the marks (niches, hooks, rivets, veins) sit on the face of the wall, never deep in the rock
          bool face = false;
          for (P d4 : {P{1, 0}, P{-1, 0}, P{0, 1}, P{0, -1}}) if (!blocks(tile(x + d4.x, y + d4.y))) face = true;
          if (!face) r = 1;
          auto shade = [&](int lo, int hi, int k = 1) { return mixc(lo, hi, .6 * noise(x, y, 2.5, k) + .4 * ((hash(x, y, k + 11) % 1000) / 1000.0)); };
          if (rest || actn == 1) {
            int course = (x + (y % 2) * 2) / 4;
            bg = scale(col, .22 + .08 * ((hash(course, y, 2) % 7) / 7.0));
            if ((x + (y % 2) * 2) % 4) { ch = 0x2593; fg = scale(col, .4 + .1 * noise(x, y, 3, 4)); }
            else { ch = 0x2502; fg = scale(col, .16); }
            if (hh % 23 == 0) { ch = '/'; fg = scale(col, .18); }  // a crack
          } else if (actn == 0) {  // the Crypt: pale bone stone and old plaster, skull niches, cut crosses
            bg = 0x1C1A18;
            if (r < .08) { ch = 0x3A9; fg = 0xC8BCA0; bg = 0x100E0C; }
            else if (r < .13) { ch = 0x253C; fg = 0x968C78; }
            else if (r < .30) { ch = 0x2592; fg = shade(0x544E46, 0x787064); }
            else { ch = 0x2591; fg = shade(0x645E54, 0x968E7E); }
          } else if (actn == 2) {  // the Forge: fire brick, black with soot here, still hot there, iron ties
            bg = 0x1E0E0A;
            if (r < .06) { ch = '#'; fg = mixc(0xC85A1E, 0xFF8A30, .5 + .5 * std::sin(tt * 1.3 + hh % 7)); bg = 0x401408; bright = true; }
            else if (r < .22) { ch = '#'; fg = shade(0x281A16, 0x3C261E); bg = 0x120A08; }
            else if (r < .30) { ch = 0x2261; fg = 0x5A3C32; }
            else { ch = '#'; fg = shade(0x642C1C, 0x9C482C); }
          } else if (actn == DEEP) {  // the Deep: basalt columns with glints of glass
            bg = 0x100E1A;
            if (r < .07) { ch = 0x2219; fg = 0xC8B4FF; }
            else { ch = r < .45 ? 0x258C : 0x2590; fg = shade(0x322A50, 0x5C5088); }
          } else if (actn == CISTERN) {  // the Cistern: wet stone, slime, water seeping down
            bg = 0x101818;
            if (r < .08) { ch = 0xA6; fg = 0x4A7C76; }
            else if (r < .30) { ch = 0x2592; fg = shade(0x28462E, 0x40643E); }
            else { ch = 0x2592; fg = shade(0x2C4240, 0x506A62); }
          } else if (actn == CITY) {  // the City: dressed ashlar, carved courses, rosettes
            bg = 0x282622;
            if (r < .10) { ch = 0x2580; fg = shade(0x6E685E, 0x8C8476); bg = 0x3C3832; }
            else if (r < .16) { ch = 0xA4; fg = 0x96825A; bg = 0x464038; }
            else { ch = 0x2588; fg = shade(0x56524A, 0x746E64); }
          } else if (actn == VAULTS) {  // the Vaults: dark banded stone, bronze plates, rivets
            bg = 0x120F0C;
            if (r < .12) { ch = 0x25D8; fg = 0x826436; bg = 0x281E10; }
            else if (r < .20) { ch = 0x2022; fg = 0xAA823C; bg = 0x1E1810; }
            else { ch = 0x2580; fg = shade(0x423624, 0x645030); }
          } else if (actn == BARRACKS) {  // the Barracks: fieldstone, empty weapon hooks
            bg = 0x1E1814;
            if (r < .06) { ch = 0xAC; fg = 0x96969C; }
            else if (r < .30) { ch = 0x2593; fg = shade(0x503E32, 0x705A4C); }
            else { ch = 0x2584; fg = shade(0x5A463A, 0x846A5A); }
          } else if (actn == PRISON) {  // the Prison: slate, chisel scars, shackle rings
            bg = 0x0E0E12;
            if (r < .06) { ch = 0xB0; fg = 0x786E64; }
            else if (r < .14) { ch = '/'; fg = 0x646470; bg = 0x282A34; }
            else { ch = 0x2588; fg = shade(0x282A34, 0x444654); }
          } else {  // the Sanctum: polished black stone, gold veins, gold inlay
            bg = 0x0A080C;
            if (r < .07) { ch = 0x2593; fg = 0x9C783A; }
            else if (r < .11) { ch = 0x2666; fg = 0xBE9646; bg = 0x1A141C; }
            else { ch = 0x2593; fg = shade(0x28202E, 0x483C50); }
          }
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
            else if (actn == PRISON && tile(x, y - 1) == GALLOWS && hh % 2) {
              double sw = std::sin(tt * .8 + x);  // a rope, swinging a little
              ch = sw < -.6 ? '/' : sw > .6 ? '\\' : 0xA6; fg = 0x8A7A5A;
            }
            else if (actn == BARRACKS && tile(x, y - 1) == HANGING && blocks(tile(x, y - 2))) {
              // the tatters of the banner above, swinging in a draught
              double sw = std::sin(tt * 1.3 + x * .7) + .4 * std::sin(tt * 2.9 + x);
              ch = sw < -.5 ? '/' : sw > .5 ? '\\' : 0xA6; fg = 0x5A1E12;
            }
            else if ((actn <= 1 && by_wall && n > .58) || (actn == CISTERN && by_wall && n > .7)) {
              ch = hh % 3 ? '"' : ',';
              fg = mixc(0x3E5A2A, 0x5E7A36, (hh % 5) / 5.0);
            }
          }
          break;
        }
        case SHALLOW: case WATER:
          if (actn == 1) {
            // the Caves' stream runs: ripples travel along it, white where it rushes past the planks
            double ph = y * .9 + x * .25 - tt * 2.6, k = .5 + .5 * std::sin(ph);
            bool past = tile(x, y - 1) == PLANK || tile(x - 1, y) == PLANK || tile(x + 1, y) == PLANK || tile(x, y + 1) == PLANK;
            if (t == SHALLOW) {
              bg = mixc(0x1A2C3C, 0x284054, k); fg = mixc(0x5A82AA, 0xAAC8DC, k);
              ch = k > .6 ? 0x2248 : '.';
            } else {
              bg = mixc(0x0A1428, 0x142846, k); fg = mixc(0x28508C, 0x6EA0D2, k);
              ch = std::sin(ph * 1.7 + x) > -.2 ? '~' : '-';
            }
            if (past && std::sin(ph * 2.3) > .1) { ch = std::sin(ph * 3) > 0 ? '~' : 0xB0; fg = mixc(0xB4D2E6, 0xF0F8FF, k); }
            break;
          }
          if (t == WATER) {
            bg = mixc(0x0C1C38, 0x10244A, noise(x, y, 3, 9));
            ch = '~'; fg = 0x2E5490;
            break;
          }
          // still water: calm blue, a faint ripple now and then
          bg = mixc(0x16304E, 0x1C3A5C, noise(x, y, 3, 9));
          ch = std::fmod(tt * .3 + (hh % 100) / 100.0, 1.0) < .15 ? '~' : ' ';
          fg = 0x4A74A8;
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
          if (t == DAIS && actn == CISTERN) {
            // a raised slab of stone in the Sluice-Keeper's reservoir: dry ground above the flood, a lip along its edge
            bg = mixc(0x323A36, 0x3C4642, (hash(x / 2, y, 6) % 3) / 3.0);
            ch = tile(x, y + 1) != DAIS ? 0x2584 : (x + y) % 3 == 0 ? 0xB7 : ' ';
            fg = tile(x, y + 1) != DAIS ? 0x78807A : 0x5A625E;
          }
          if (t == DAIS && actn == SANCTUM) {
            // the platform where the Pyreheart lay: polished black, inlaid with gold in a lattice, the light running over it
            bg = mixc(0x16121C, 0x221C2A, (hash(x / 2, y / 2, 6) % 3) / 3.0);
            // only a faint gold point where the lines of the lattice cross, so the floor stays calm to fight on
            bool inlay = x % 6 == 0 && y % 4 == 3;
            ch = inlay ? 0xB7 : ' ';
            fg = inlay ? 0x6A5428 : 0x2A2430;
            double sweep = std::fmod(tt * .1 + (hash(x / 8, 0, 38) % 100) / 400.0, 1.0) * 60 - 15;
            if (inlay && std::abs(x * .8 + y - sweep) < 1.2) { fg = 0xFFE6A0; bright = true; }
          }
          if (t == THRONE) { bg = 0x383A46; ch = 0x2565; fg = 0x8A7AA0; }
          if (t == THRONE && actn == CITY) { bg = 0x3A2A14; fg = 0xB48A48; }
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
          if (actn == SANCTUM) {
            // the great altar where the Pyreheart burned: black with soot, cracked through, and in its middle the
            // hollow where the crystal sat, cold now, with frost at its rim
            int nl = 0, nr = 0;
            while (tile(x - nl - 1, y) == ALTAR) ++nl;
            while (tile(x + nr + 1, y) == ALTAR) ++nr;
            bg = mixc(0x141012, 0x1E1A1A, (hh % 3) / 3.0);
            if (nl == nr) { ch = 0x25D8; fg = 0x6A7A90; bg = 0x0C0E14; if (std::fmod(tt * .13, 1.0) < .05) { fg = 0xB4C8E0; bright = true; } }
            else if (nl == nr + 1) { ch = '/'; fg = 0x0A0808; }
            else if (!l || !r) { ch = 0x2584; fg = 0x4A4440; }
            else { ch = 0x2021; fg = 0x6A5A3C; }
            break;
          }
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
          if (t == RAIL) {
            auto railish = [&](int a, int b) { int u = tile(a, b); return u == RAIL || u == TURNTABLE || u == CART; };
            if (!railish(x - 1, y) && !railish(x + 1, y) && (railish(x, y - 1) || railish(x, y + 1))) ch = 0x2551;
            if (actn == 1) bg = floor_bg();
          }
          if (t == ASH) bg = mixc(bg, 0x3A3632, .5);
          break;
        case ANVIL: case SLAG: case CART: case BANNER: case FALLEN: case STATUE: {
          bg = mixc(0x16100C, 0x2A1A12, noise(x, y, 5, 3));
          if (t == CART) bg = 0x2A1A0C;
          if (t == STATUE) {
            bg = 0x2A2622;
            if (tile(x - 1, y) != STATUE) ch = 0x263B;  // the head
          }
          if (t == BANNER && actn == BARRACKS) fg = 0xA03A20;  // the Iron-Vanguard's, on its pole
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
          // the surface moves in slow, loose patches that drift and change shape, never in lines
          double v = .6 * vnoise(x * .25 + tt * .05, y * .4 + std::sin(tt * .1) * .5) + .4 * vnoise(x * .6 - tt * .08, y * .9 + 7);
          bg = mixc(0x0C120A, 0x1C2414, v);
          double sh = vnoise(x * .3 - tt * .06 + 20, y * .5 + tt * .03);
          if (sh > .7) bg = mixc(bg, mixc(0x24382C, 0x2C3040, vnoise(x * .2 + 40, y * .2)), (sh - .7) * 2.2);  // the greasy sheen, faint
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
          bg = mixc(0x0E1416, 0x16201E, vnoise(x * .35 + tt * .06, y * .5 - tt * .04));
          if (tile(x, y - 1) == PILLAR) bg = mixc(bg, 0x3A403E, .45);  // a pillar's reflection
          ch = std::fmod(tt * .3 + (hh % 100) / 100.0, 1.0) < .08 ? '~' : ' ';
          fg = 0x3A4A48;
          break;
        }
        case GRATE:
          bg = 0x0E100E; ch = '#'; fg = 0x5A5A60;
          break;
        case CHASM: {
          // the floor falls away into the dark; its lip catches the light
          bg = 0x04050A; ch = ' ';
          if (actn == 1) {
            // the Caves' chasm: the rock face still shows near the edge, then only dark
            int near = 0;
            for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) near += tile(x + dx, y + dy) != CHASM && tile(x + dx, y + dy) != PLANK;
            if (near) { bg = mixc(0x06060A, 0x2A2018, std::min(1.0, near / 4.0)); if ((x + y) % 2) { ch = 0xB7; fg = 0x4A3C30; } }
            break;
          }
          if (actn == SANCTUM) {
            // the hole dug behind the altar: jagged at the edge, and far down a dull red glow, the Deep below breathing
            int near = 0;
            for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) near += tile(x + dx, y + dy) != CHASM;
            double g = .5 + .5 * std::sin(tt * .7 + x * .3);
            bg = near ? 0x0C0A0E : mixc(0x1A0604, 0x3A0E06, g * (1 - near / 8.0));
            if (near && hh % 2) { ch = 0x25B2; fg = 0x2A2630; }
            break;
          }
          if (tile(x, y - 1) != CHASM && !blocks(tile(x, y - 1))) { ch = 0x2580; fg = 0x3A4446; }
          break;
        }
        case SOOTDEAD: bg = floor_bg(); if (hh % 3 == 0) ch = 0x3A9; break;
        case CENSER:
          bg = floor_bg();
          if (std::fmod(tt * .2 + (hh % 100) / 100.0, 1.0) < .04) { fg = 0xFFE6A0; bright = true; }  // the brass catches the light
          break;
        case RELIQUARY:
          bg = 0x2A2010;
          if (std::fmod(tt * .2 + (hh % 100) / 100.0, 1.0) < .03) { fg = 0xFFF0B0; bright = true; }
          break;
        case NICHE: bg = 0x08080A; break;
        case ASHPRAYER:
          // the High Prelate's last prayer, written in ash along the steps
          bg = y % 2 ? 0x2E2C28 : 0x24221F;
          ch = hh % 4 == 0 ? '-' : hh % 4 == 1 ? 0x2248 : '~';
          fg = 0x9A948A;
          break;
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
          if (actn == 1) {
            // the Webmother's egg sacs: pale, swelling and slackening, now and then something moves inside
            double k = std::pow(std::max(0.0, std::sin(tt * 1.6 + hh % 13)), 4);
            ch = k > .5 ? 'O' : 'o'; fg = mixc(0xB8A880, 0xF4E6B8, k); bg = mixc(floor_bg(), 0x3A3020, .3 + .4 * k);
            break;
          }
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
        case COIN:
          bg = floor_bg(); keep = true;
          if ((actn == CITY || actn == VAULTS || actn == SANCTUM) && std::fmod(tt * .22 + (hh % 100) / 100.0, 1.0) < .035) { fg = 0xFFF0B0; bright = true; }  // it catches the light
          break;
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
        case DOOR: case OPEN:
          bg = floor_bg(); keep = true;
          // a cell door the draught swings
          if (t == DOOR && rattle > 0 && rattle_door_ && (tile(x - 1, y) == BARS || tile(x, y - 1) == BARS) && hash(x, y, int(rattle_t_)) % 4 == 0 && int(tt * 18) % 2)
            { fg = 0xE0D0B8; bright = true; }
          break;
        case STAIRS: case UPSTAIRS: case CHEST: case OPENCHEST: case SHRINE: case USED: case FOUNTAIN:
          bg = floor_bg(); keep = true; break;
        case ORE: {
          // copper and gold in the rock, catching the light now and then
          bg = scale(act.wall, .24);
          bool gold = hash(x, y, 24) % 3 == 0;
          ch = hash(x, y, 25) % 2 ? 0x2666 : 0x2022;
          fg = gold ? 0xE8C050 : 0xCE703A;
          break;
        }
        case TIMBER: {
          // the beam of a timber frame, laid in the rock across the tunnel
          bg = scale(act.wall, .22);
          bool side = !blocks(tile(x - 1, y)) || !blocks(tile(x + 1, y)) || tile(x - 1, y) == PROP || tile(x + 1, y) == PROP;
          ch = side ? 0x2551 : 0x2550;
          break;
        }
        case HEAP:
          if (actn == SANCTUM) { ch = 0x2584; fg = 0x8C847A; bg = 0x2C2926; break; }  // the stump of a broken pillar
          bg = mixc(floor_bg(), 0x2A2420, .6); ch = hash(x, y, 26) % 3 ? 0x25B2 : '%'; break;
        case BEAM: case PICK: case TURNTABLE: case COCOON:
          bg = floor_bg();
          if (t == PICK && actn == SANCTUM) {
            // the sacred pickaxe, gold wire round its haft, dropped at the lip of the hole it dug
            bg = 0x16121C; fg = 0xC8A050;
            if (std::fmod(tt * .25, 1.0) < .06) { fg = 0xFFE6A0; bright = true; }
          }
          break;
        case PLANK: {
          // planks over the water or the dark, swaying a little
          bool water = tile(x, y - 1) == WATER || tile(x, y + 1) == WATER || tile(x - 1, y) == WATER || tile(x + 1, y) == WATER ||
                       tile(x, y - 1) == SHALLOW || tile(x, y + 1) == SHALLOW;
          bg = water ? 0x0E1A30 : 0x06060A;
          fg = mixc(0x78542E, 0xA07446, .5 + .5 * std::sin(tt * 1.5 + x * .6));
          break;
        }
        case STALAG: bg = floor_bg(); fg = mixc(0x968C7C, 0xC8BEAA, (hh % 7) / 7.0); break;
        case MIDDEN: {
          bg = mixc(floor_bg(), 0x2A2018, .6);
          ch = hh % 4 == 0 ? '&' : hh % 3 == 0 ? ',' : '%';
          fg = mixc(0x786450, 0xBEB096, (hh % 9) / 9.0);
          if ((int(tt * 3) + hh) % 40 == 0) fg = 0xE6DCC8;  // the heap stirs
          break;
        }
        case BURROW: {
          bg = 0x0A0806;
          // a rat pokes its nose out now and then and ducks back
          if (std::fmod(tt * .4 + (hh % 100) / 100.0, 1.0) < .2) { ch = 'r'; fg = 0xAA826E; }
          break;
        }
        case CAMPFIRE: {
          static const int FL[] = {0x25B2, '^', 0x25B2, '*'};
          ch = FL[(int(tt * 9) + x) % 4]; fg = mixc(0xFF7814, 0xFFE682, .5 + .5 * std::sin(tt * 13));
          bg = 0x5A1E06; bright = true;
          break;
        }
        case CRYSTAL: {
          // crystals in the rock, amber and white, each glinting now and then on its own
          bg = scale(act.wall, .22);
          ch = hh % 3 ? 0x2666 : '*';
          int base = hh % 2 ? 0xF0AA46 : 0xECE8DC;
          double ph = std::fmod(tt * .45 + (hh % 1000) / 1000.0, 1.0), k = ph < .15 ? std::sin(ph / .15 * PI) : 0;
          fg = mixc(scale(base, .65), 0xFFFFF0, k); bg = mixc(bg, 0x5A4628, k);
          if (k > .3) bright = true;
          break;
        }
        case TOTEM: case HIDE: case STUB: bg = floor_bg(); break;
        // ---- the Forge, left in haste
        case EMBERS: {
          // old embers in the ash, breathing slowly; some all but out; the draught wakes them a little
          double k = (hh % 3 ? .65 + .35 * ((hh / 3) % 3) / 2.0 : .2) * (1 + .6 * gust);
          double g = std::min(1.0, (.5 + .5 * std::sin(tt * .9 + hh % 7)) * k);
          ch = hh % 3 ? 0x2219 : ',';
          fg = mixc(0x4A4440, 0xD05A1C, g); bg = mixc(0x161210, 0x4A1606, g); bright = true;
          break;
        }
        case BELLOWS: ch = tile(x, y + 1) == BELLOWS ? 0x2584 : 0x2580; bg = 0x161210; break;
        case SLAGPIT: ch = hh % 3 ? 0x2591 : 0x2592; fg = mixc(0x4A4440, 0x625A54, (hh % 5) / 5.0); bg = 0x1A1714; break;
        case HOOK:
          if (actn != PRISON) { bg = floor_bg(); break; }
          { // a hook on its chain, swinging; harder while the chains clink
            double sw = std::sin(tt * (1.1 + 3 * rattle * !rattle_door_) + x) * (1 + rattle * !rattle_door_);
            ch = sw < -.5 ? '/' : sw > .5 ? '\\' : 0xA6; bg = floor_bg();
          }
          break;
        case LAWS: bg = 0x22222A; break;
        case KEYS: bg = 0x1A1A20; if (rattle > 0 && !rattle_door_ && int(tt * 16 + x) % 2) { fg = 0xE0C880; bright = true; } break;
        case JUDGESEAT: case TORTURERACK: case BLOCK: bg = floor_bg(); break;
        case MAIDEN: bg = 0x1E1614; break;
        case GALLOWS: {
          bool l = tile(x - 1, y) == GALLOWS, r = tile(x + 1, y) == GALLOWS;
          ch = l && r ? 0x2500 : r ? 0x250C : l ? 0x2510 : 0x393; bg = floor_bg();
          break;
        }
        case SLUNG: ch = (x + y) % 2 ? '~' : '-'; fg = (x + y) % 2 ? 0x9A9AA4 : 0x7A7A84; bg = floor_bg(); break;
        case STACK: {
          bool edge = false;
          for (P d4 : {P{1, 0}, P{-1, 0}, P{0, 1}, P{0, -1}}) if (tile(x + d4.x, y + d4.y) != STACK && tile(x + d4.x, y + d4.y) != EMBERS) edge = true;
          ch = edge ? 0x2593 : 0x2588; fg = mixc(0x2A2420, 0x3E3630, (hh % 4) / 4.0); bg = 0x0E0A08;
          break;
        }
        case SPILL: {
          // metal that ran from the tipped crucible and set; where it lies thickest it is still warm
          bool thick = false;
          for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx) if (tile(x + dx, y + dy) == CRUCIBLE) thick = true;
          ch = hh % 2 ? 0x2248 : '~';
          if (thick) {
            double g = .45 * (.5 + .5 * std::sin(tt * .9 + hh % 7)) * (1 + .5 * gust);
            fg = mixc(0x6A4A3A, 0xC0501A, g); bg = mixc(0x241C18, 0x4A1606, g); bright = true;
          } else { fg = 0x8A7A70; bg = 0x2A2420; }
          break;
        }
        case MOULD: case CAST: {
          // the shape of a blade cut in the floor: a point, the blade, a cross-guard and the grip
          auto mo = [&](int a, int b) { int u = tile(a, b); return u == MOULD || u == CAST || u == SPILL; };
          bool lf = mo(x - 1, y), rt = mo(x + 1, y), upm = mo(x, y - 1), dn = mo(x, y + 1);
          bool under_guard = upm && mo(x - 1, y - 1) && mo(x + 1, y - 1);
          if (lf && rt) ch = (upm || dn) ? 0x253C : 0x2500;
          else if (under_guard) ch = 0x2502;
          else if (!upm) ch = 0x2206;
          else ch = 0x2551;
          if (t == MOULD) { fg = 0x4A3A30; bg = 0x120C0A; } else { fg = 0x7A6E68; bg = 0x1E1816; }
          break;
        }
        case TROUGH: fg = 0x4A3A30; bg = 0x120C0A; break;
        case RIM: {
          auto q = [&](int a, int b) { return tile(a, b) == QUENCH; };
          if (q(x - 1, y + 1) || q(x, y + 1) || q(x + 1, y + 1)) ch = 0x2584;
          else if (q(x - 1, y - 1) || q(x, y - 1) || q(x + 1, y - 1)) ch = 0x2580;
          else if (q(x + 1, y)) ch = 0x2590;
          else ch = 0x258C;
          bg = 0x1E1612;
          break;
        }
        case QUENCH: {
          // still water under a skin of ash: no ripple, only a faint sheen and the flakes on it
          ch = hh % 4 == 0 ? 0x00B7 : ' '; fg = 0x5A5650;
          bg = mixc(0x06090C, 0x0E161E, .4 + .2 * ((hh % 3) / 3.0));
          break;
        }
        case HUSK: ch = hh % 5 == 0 ? 'o' : hh % 3 ? 'G' : 'g'; bg = 0x1E1C1C; break;
        case BENCH: bg = 0x1E1C1C; break;
        case PLATES: case BOLTS: bg = 0x1A1816; break;
        case SCRAP: {
          static const int SC[] = {'&', '%', 0xA5, '/', '\\', 0xAC, 0x2310, '=', 0x2534, 0x252C, 0xF7, '+'};
          ch = SC[hh % 12];
          double rust = (hash(x, y, 36) % 4) / 4.0;
          fg = mixc(0xA0A0AA, 0xB0603A, rust); bg = mixc(0x16100C, 0x241A14, rust);
          break;
        }
        case COALHEAP: fg = mixc(0x3A3430, 0x5A5048, (hh % 3) / 3.0); bg = 0x16120E; break;
        case BONEHEAP: ch = hh % 3 ? '%' : ','; fg = mixc(0xC8BCA0, 0xE8DCC4, (hh % 4) / 4.0); bg = 0x241F1A; break;
        case HOIST: bg = 0x1A120E; break;
        case WORKPIECE: bg = 0x221A16; break;
        case HWALL: {
          // the houses' walls: dressed stone in courses, lighter than the cavern's rock
          int course = (x + (y % 2) * 2) / 3;
          bg = mixc(0x2C2824, 0x36322C, (hash(course, y, 2) % 5) / 5.0);
          ch = (x + (y % 2) * 2) % 3 ? 0x2593 : 0x2502;
          fg = ch == 0x2502 ? 0x1E1A16 : mixc(0x5E5650, 0x6E6660, noise(x, y, 3, 25));
          break;
        }
        case DRIFT: {
          double n = noise(x, y, 3, 21);
          bg = mixc(floor_bg(), 0x3A3834, .3 + .25 * n);
          ch = n > .55 ? 0x2591 : 0xB7; fg = mixc(0x4E4A46, 0x7A7670, (hh % 4) / 4.0);
          break;
        }
        case MARBLE: {
          bg = mixc(0x34322E, 0x403E3A, (hash(x / 3, y / 2, 9) % 5) / 5.0);
          ch = (x % 3 == 0 || y % 2 == 0) ? 0xB7 : ' '; fg = 0x6A6660;
          break;
        }
        case MOSAIC: {
          // little tiles in blue and bone, a pattern of diamonds under the ash
          int dx = std::abs((x % 6) - 3), dy = std::abs((y % 4) - 2);
          bool blue = dx + dy * 1.5 <= 2.5;
          bg = blue ? mixc(0x1A2434, 0x24304A, (hh % 3) / 3.0) : mixc(0x2E2C2A, 0x383430, (hh % 3) / 3.0);
          ch = ' ';
          if (hh % 9 == 0) { ch = 0x2591; fg = 0x5A5650; }  // ash on it
          break;
        }
        case DRYBATH: bg = mixc(0x1E1C1A, 0x2A2826, noise(x, y, 4, 22)); fg = 0x5A5650; ch = (hh % 3) ? 0x2591 : ' '; break;
        case STEP: bg = y % 2 ? 0x2E2C28 : 0x24221F; fg = y % 2 ? 0x8A867E : 0x6A6660; ch = 0x2550; break;
        case GOLDBED: {
          // the golden river ran here: a dry bed a man's height deep, gilt at the bottom under the ash
          bg = mixc(0x1A140A, 0x2A2010, noise(x, y, 4, 23));
          ch = hh % 3 ? 0xB7 : 0x2591; fg = 0x5A4A24;
          if (std::fmod(tt * .15 + (hh % 100) / 100.0, 1.0) < .03) { fg = 0xC8A050; bright = true; }
          break;
        }
        case BRIDGE: bg = 0x3A3834; fg = 0xB4B0A8; ch = 0x2550; break;
        case AWNING: bg = floor_bg(); fg = hash(x / 2, y, 37) % 2 ? 0x6E2A20 : 0x2A4A3A; ch = 0x25AC; break;
        case REDWALL: {
          bg = mixc(0x3A1410, 0x4A1A14, (hash((x + (y % 2) * 2) / 4, y, 2) % 5) / 5.0);
          fg = mixc(0x8A3026, 0xA0382C, noise(x, y, 3, 24));
          ch = (x + (y % 2) * 2) % 4 ? 0x2593 : 0x2502;
          if (ch == 0x2502) fg = 0x2A0E0A;
          break;
        }
        case REDCOL: bg = 0x2A1410; fg = 0xB43C2C; break;
        case LION: case VAT: case KEG: case LECTERN: case SIGN: case NOTICE: case SCROLL: bg = floor_bg(); break;
        case BALUSTER: {
          bool v = (tile(x, y - 1) == BALUSTER || tile(x, y + 1) == BALUSTER), hz = (tile(x - 1, y) == BALUSTER || tile(x + 1, y) == BALUSTER);
          ch = v && hz ? 0x253C : v ? 0x2502 : 0x2500;
          bg = 0x2E2C28;
          break;
        }
        case SCREEN: case SUNDISC: case SUNHEART: case GOLDCRUST: {
          // gold: dull and sagging, but now and then the light runs over it
          bg = t == GOLDCRUST ? mixc(floor_bg(), 0x3A2C14, .6) : t == SUNHEART ? 0x5A4620 : 0x46361A;
          double sweep = std::fmod(tt * .12 + (hash(x / 6, y / 4, 38) % 100) / 300.0, 1.0) * 40 - 10;
          double d = std::abs(x + y * .5 - (x / 20) * 20 - sweep);
          if (d < 1.5) { fg = mixc(fg, 0xFFECAA, .7 * (1 - d / 1.5)); bright = true; }
          break;
        }
        case SHUTTER: {
          // shutters, some of them hanging; now and then a shadow seems to pass behind one
          bg = 0x1E1814; fg = 0x6A4E34;
          if (zone_fx("k5")) {
            double ph = std::fmod(tt / 23.0 + (hh % 1000) / 1000.0, 1.0);
            if (ph < .05) { double k = std::sin(ph / .05 * PI); fg = mixc(fg, 0x141010, k); bg = mixc(bg, 0x080606, k); }
          }
          break;
        }
        case POST: {
          // like the torches, candles and lamps elsewhere, the sign itself is the light: here only a dull ember, breathing
          bg = floor_bg();
          double fl2 = .5 + .5 * std::sin(tt * .7 + hh % 97) * std::sin(tt * .23 + (hh % 97) * 3);
          fg = mixc(0x7A2408, 0xE0682A, fl2);
          bright = true;
          break;
        }
        case ANCESTOR: bg = 0x2A2622; break;
        case DEADHEAP: bg = 0x34322E; break;
        case GOLDHAND: case COINHEAP: case MOUND: case INGOTS: case CROWN: case GEM: {
          // gold and stones: dull in the dark, now and then catching the light
          bg = t == GOLDHAND ? 0x3C2C10 : t == GEM || t == CROWN ? floor_bg() : 0x2A2010;
          if (t == GEM) { static const int GC[] = {0x7A98B0, 0xA05050, 0x50A070, 0xA89850}; fg = GC[hh % 4]; }
          if (t == MOUND) {
            // the mound heaves now and then: something under it
            double ph = std::fmod(tt / 7.0, 1.0);
            if (ph < .07) { fg = mixc(fg, 0xFFE090, std::sin(ph / .07 * PI)); bright = true; }
            if (hh % 4 == 0) fg = 0xA8A8B0;  // silver
          }
          if (std::fmod(tt * .2 + (hh % 100) / 100.0, 1.0) < .03) { fg = t == GEM ? mixc(fg, 0xFFFFFF, .7) : 0xFFF0B0; bright = true; }
          break;
        }
        case CHESTWRECK: case FALLSHELF: case TABLET: case SCALES: case LEDGER: bg = floor_bg(); break;
        case CASE: bg = 0x1C1E22; break;
        case SCROLLDRIFT: bg = mixc(floor_bg(), 0x2A2622, .5); break;
        case MELTBAR: bg = floor_bg(); ch = hh % 2 ? 0x2518 : 0x2510; break;
        case TAPESTRY: bg = 0x180A0A; ch = hh % 4 == 0 ? 0x2591 : 0x2592; break;
        case BINDRUNE: {
          static const int RN[] = {0x3A3, 0x3A6, 0x3B4, 0x398};
          double g = .5 + .5 * std::sin(tt * .9 + x);
          bg = 0x14121A; ch = RN[(x + y) % 4]; fg = mixc(0x2A1E3A, 0x7A5AA0, g); bright = true;
          break;
        }
        case INSCRIPTION: bg = 0x28221A; break;
        // ---- the Barracks, as the garrison left them
        case BUNK: case TABLE: case RACK: case DUMMY: {
          bg = floor_bg();
          if (actn != BARRACKS) break;
          if (t == BUNK && hh % 5 == 0) { ch = '_'; fg = 0x5A4430; }  // fallen through
          else if (t == BUNK && hh % 5 == 1) { ch = '%'; fg = 0xC8BCA0; bg = 0x34281E; }  // the sleeper's bones still in it
          else if (t == TABLE && hh % 6 == 0) { ch = '/'; fg = 0x6A5038; }  // smashed
          else if (t == RACK && hh % 3 == 0) { ch = 0x2567; fg = 0x7A7A84; }  // overturned
          else if (t == DUMMY && hh % 2) fg = 0x4A3A2C;  // burned black
          if ((t == RACK) && std::fmod(tt * .2 + (hh % 100) / 100.0, 1.0) < .025) { fg = 0xE0E0E8; bright = true; }  // steel not quite rusted
          break;
        }
        case SPEAR: case BANDAGE:
          bg = floor_bg();
          if (t == SPEAR && hh % 2) ch = '\\';
          if (t == SPEAR && std::fmod(tt * .2 + (hh % 100) / 100.0, 1.0) < .02) { fg = 0xE0E0E8; bright = true; }
          break;
        case SHIELDHEAP: bg = 0x221C16; break;
        case DRUM: bg = 0x281A12; break;
        case ARMOUR: bg = floor_bg(); break;
        case TARGET: bg = 0x2A2018; break;
        case PASSMAP: ch = (x + y) % 2 ? '^' : 0x2248; bg = 0x3C3020; break;
        case HANGING: bg = 0x1E100C; ch = 0xB6; break;
        default: bg = floor_bg(); break;
      }
      if (forge && hot_.size() == size_t(W * H) && hot_[i] && !blocks(t)) {
        // the air wavers over the running metal: the floor beside it comes and goes, reddened
        double wv = std::sin(tt * 5 + x * 1.3 + y * 2.1) * std::sin(tt * 2.3 - x * .7);
        if (t == FLOOR) ch = wv > .35 ? 0x2219 : wv < -.35 ? ' ' : ch;
        bg = mixc(bg, 0x5A1E0A, (hot_[i] == 1 ? .18 : .08) * (1 + wv));
        fg = mixc(fg, 0xC85A28, .3);
      }
      if (vis[i] == '1') {
        double l = L[i] * (1 - .3 * groan), w = warm[i];
        if (keep) l = std::max(l, 1.0);
        if (!bright) fg = mixc(mixc(scale(fg, l), 0x101828, std::max(0.0, 1 - l) * .35), scale(0xFF9A48, l), w * .35);
        bg = mixc(scale(bg, l), 0x5A2A0A, w * .5 * std::min(l, 1.2));
        if (double c = cold[i]; c > 0) {
          if (!bright) fg = mixc(fg, scale(0x5AE0B0, l), c * .35);
          bg = mixc(bg, 0x0E3A30, c * .45 * std::min(l, 1.2));
        }
        if (!heaps.empty()) {
          // daylight from far up the fallen shaft: grey and cold on the heap and round it
          double k = 0;
          for (P hp : heaps) k = std::max(k, 1 - std::hypot(x - hp.x, (y - hp.y) * 1.25) / 3.6);
          if (k > 0) {
            k *= .85 + .15 * std::sin(tt * .7);
            bg = mixc(bg, 0x5A6274, k * .5);
            if (!bright) fg = mixc(fg, 0xC8CEDA, k * .3);
          }
        }
        if (cvx && !damp_.empty()) {
          // cold mist lying low over the stream, drifting slowly downstream, thinning round a hero
          if (cave_fx(1) && damp_[i] > 0 && (!blocks(t) || t == WATER)) {
            double n = .6 * vnoise(x * .16 - tt * .1 * wind_, y * .35 + tt * .03) + .4 * vnoise(x * .4 - tt * .22 * wind_, y * .7);
            double m = std::clamp((n - .28) / .42, 0.0, 1.0) * damp_[i];
            for (auto& h : s["heroes"]) {
              double hd = std::hypot(x - h["x"].get<int>(), (y - h["y"].get<int>()) * 1.3);
              if (hd < 2.2) m *= hd / 2.2;
            }
            bg = mixc(bg, scale(0x6A7C92, .35 + .5 * std::min(l, 1.2)), m * .55);
            if (!bright) fg = mixc(fg, 0x8A9CB4, m * .3);
          }
          // the rock by the stream is wet: darker, bluer, and here and there it catches the light
          if (cave_fx(5) && t == WALL && wetw_[i]) {
            bg = mixc(bg, 0x0C1622, .45);
            fg = mixc(fg, scale(0x36506A, std::min(l, 1.2)), .4);
            double ph = std::fmod(tt * .21 + (hh % 1000) / 1000.0, 1.0);
            if (hh % 3 == 0 && ph < .2) {
              double k = std::sin(ph / .2 * PI) * std::min(1.0, l + .2);
              fg = mixc(fg, 0xE6F4FF, k); bg = mixc(bg, 0x2A4460, k * .7);
            }
          }
        }
        if (breath > 0) {
          // the Deep breathes: the lava swells brighter and its glow reaches further
          double k = breath * std::min(1.0, w * 1.6 + (t == LAVA ? .8 : 0));
          bg = mixc(bg, t == LAVA ? 0xFF7A20 : 0x7A2408, k * (t == LAVA ? .22 : .16));
          if (!bright) fg = mixc(fg, 0xFFB060, k * .12);
        }
        if (actn == CISTERN && !damp_.empty()) {
          // the water throws the light back: pale wavering bands on the stone beside it
          if (zone_fx("s1") && damp_[i] > 0) {
            double v = std::sin(x * .6 + y * .35 + tt * .55) * std::sin(x * .25 - y * .8 - tt * .4);
            double band = std::clamp((v - .3) / .6, 0.0, 1.0) * damp_[i] * std::min(1.0, l + .2);
            bg = mixc(bg, 0x4A8A72, band * .3);
            if (!bright) fg = mixc(fg, 0xA8E0C8, band * .15);
          }
        }
        if (cfx) {
          // the grave chill: low cold mist, in drifting banks, thinning round a hero who walks through it
          if (float ch2 = chill_[i]; ch2 > 0 && !blocks(t)) {
            double n = .6 * vnoise(x * .18 - tt * .12, y * .3 + tt * .05) + .4 * vnoise(x * .4 + tt * .2, y * .6 - tt * .08);
            double m = std::clamp((n - .25) / .45, 0.0, 1.0) * ch2;
            for (auto& h : s["heroes"]) {
              double hd = std::hypot(x - h["x"].get<int>(), (y - h["y"].get<int>()) * 1.3);
              if (hd < 2.2) m *= hd / 2.2;
            }
            bg = mixc(bg, scale(0x5A6A84, .4 + .5 * std::min(l, 1.2)), m * .5);
            if (!bright) fg = mixc(fg, 0x7A8AA4, m * .3);
          }
          // coloured light from the chapel windows, and the windows themselves
          int gc = 0;
          if (double k = beam_at(x, y, gc); k > 0 && !blocks(t)) {
            bg = mixc(bg, scale(gc, .55), k * .55);
            if (!bright) fg = mixc(fg, gc, k * .35);
          }
          for (auto& b : beams_)
            if (b.wx == x && b.wy == y) { ch = 0x2592; fg = b.col; bg = scale(b.col, .35); }
        }
      } else {
        // remembered, not seen: cold and dim
        fg = mixc(scale(fg, .35), 0x202430, .3);
        bg = scale(bg, .3);
      }
      if (!frost.empty() && frost[i] > 0) {
        // the ghost's cold glow, and frost where it went by, fading
        bg = mixc(bg, 0x3A5A7A, frost[i] * .5);
        if (!bright) fg = mixc(fg, 0xB8D8F8, frost[i] * .4);
      }
      if (!under.empty()) under[i] = bg;
      put(X, Y, ch, fg, bg);
    }
  if (ghost_a_ > 0) {
    // the ghost itself: someone long dead, an @ like any of the living, half see-through, glowing a cold blue-white
    int x = int(std::floor(ghost_x_ + .5)), y = int(std::floor(ghost_y_ + .5));
    if (x >= x0 && y >= y0 && x < x1 && y < y1) {
      int b = under[y * W + x];
      double a = ghost_a_ * (.8 + .2 * std::sin(tt * 3));
      put(x - camx_ + vx_, y - camy_ + vy_, '@', mixc(b, 0xE6F4FF, a * .85), mixc(b, 0x3A5A80, a * .55));
    }
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
  // a unique or a legendary on the floor glints: a few motes of its own colour turn slowly around it
  if (snap_.contains("items"))
    for (auto& it : snap_["items"]) {
      int col = it.value("col", 0);
      bool leg = col == data::rarity_col("legendary");
      if (!leg && col != data::rarity_col("unique")) continue;
      double ix = it["x"].get<int>(), iy = it["y"].get<int>();
      if (!open_at(ix + .5, iy + .5)) continue;
      int n = leg ? 4 : 3;
      for (int j = 0; j < n; ++j) {
        double ph = std::fmod(tt * (leg ? .5 : .35) + j / double(n) + nz(int(ix * 31 + iy * 17)) * .3, 1.0);
        double a = ph * 2 * PI, rr = .55 + .15 * std::sin(tt * 1.7 + j);
        double tw = .5 + .5 * std::sin(tt * 4 + j * 2.1);
        dot(ix + .5 + std::cos(a) * rr, iy + .5 + std::sin(a) * rr * .8, mixc(col, 0xFFFFFF, .35 * tw), .35 + .5 * tw);
      }
      double sp = std::fmod(tt * .6 + ix * .13, 2.0);  // now and then a star flashes on it
      if (sp < .12) dot(ix + .45, iy + .2, 0xFFFFFF, 1 - sp / .12, leg ? 2 : 1);
    }
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
      case 'K':
        // sparks lifting off the running metal, drifting up and away as they cool
        for (int j = 0; j < 2; ++j) {
          double ph = std::fmod(tt * .32 + j / 2.0 + nz(sd + j * 5), 1.0);
          double x = ex + .5 + (nz(sd * 3 + j) - .5) * .8 + std::sin(tt * 1.7 + j + sd) * .35 * ph, y = ey + .4 - ph * 3.2;
          dot(x, y, ph < .3 ? mixc(0xFFE6A0, 0xFFB050, ph / .3) : mixc(0xF07828, 0x7A2008, (ph - .3) / .7), 1 - ph * .5);
        }
        break;
      case 'A':
        // grey ash and black soot coming down from the dark above, catching fire over the metal
        for (int j = 0; j < 3; ++j) {
          double ph = std::fmod(tt * .1 + j / 3.0 + nz(sd + j), 1.0);
          double x = ex + (nz(sd + j * 7) - .5) * 6 + std::sin(tt * .5 + j + sd) * .7, y = ey - 3 + ph * 6;
          if (!open_at(x, y)) continue;
          int under = tile_of(map[int(y) * W + int(x)]);
          int c = under == LAVA ? 0xFFB050 : (sd + j) % 3 ? 0x8A8480 : 0x2A2420;
          dot(x, y, c, .85 * std::sin(ph * PI));
        }
        break;
      case 'a':
        // flakes of ash, falling
        for (int j = 0; j < 4; ++j) {
          double ph = std::fmod(tt * .08 + j / 4.0 + nz(sd + j), 1.0);
          double x = ex + (nz(sd + j * 7) - .5) * 6 + std::sin(tt * .5 + j) * .6, y = ey - 2 + ph * 4;
          dot(x, y, mixc(0x9A9488, 0x3A3632, ph), .8 * std::sin(ph * PI));
        }
        break;
      case 'i':
        // incense: a thin, slow ribbon of grey-lilac smoke out of the censer on the altar, curling as it climbs
        for (int j = 0; j < 22; ++j) {
          double ph = std::fmod(tt * .09 + j / 22.0, 1.0);
          double x = ex + .5 + std::sin(tt * .7 + ph * 7 + sd) * ph * 1.1 + ph * .4, y = ey + .2 - ph * 5.5;
          if (!open_at(x, y)) continue;
          dot(x, y, mixc(0xC8B8D8, 0x3A3448, ph), .6 * (1 - ph) * std::min(1.0, ph * 8), ph < .35 ? 1 : 2);
        }
        break;
      case 'b':
        // will-o'-wisps: three pale lights circling a corpse that will not lie still, flickering out and back
        for (int j = 0; j < 3; ++j) {
          double a = tt * (.6 + .15 * j) + j * 2.1 + sd, rr = .9 + .3 * std::sin(tt * .5 + j);
          double x = ex + .5 + std::cos(a) * rr * 1.4, y = ey + .4 + std::sin(a) * rr * .6 - .3 * std::sin(tt * 1.3 + j);
          double fl = .5 + .5 * std::sin(tt * 5.3 + j * 3 + sd) * std::sin(tt * 2.1 + j);
          if (fl < .15) continue;
          dot(x - px / cell, y - px / cell, 0x6AA8FF, .4 * fl, 3);
          dot(x, y, mixc(0x9AC8FF, 0xF0F8FF, fl), fl, 2);
          for (int k = 1; k < 4; ++k) {
            double b2 = a - k * .18;
            dot(ex + .5 + std::cos(b2) * rr * 1.4, ey + .4 + std::sin(b2) * rr * .6 - .3 * std::sin(tt * 1.3 + j), 0x6A98E0, .55 * fl * (1 - k / 4.0));
          }
        }
        break;
      case 'T':
        // soot running from a statue's eyes, a black drop now and then
        for (int j = 0; j < 2; ++j) {
          double ph = std::fmod(tt * .18 + j * .5 + nz(sd + j), 1.0);
          double x = ex + .5 + (j ? .16 : -.16), y = ey + .4 + ph * ph * 1.1;
          dot(x, ey + .42, 0x141210, .9);
          dot(x, y, 0x1A1816, ph < .85 ? 1 : (1 - ph) / .15, 1);
        }
        break;
      case 'Y':
        // a sour yellow-grey breath out of an air shaft's grate, drifting up and thinning
        for (int j = 0; j < 12; ++j) {
          double ph = std::fmod(tt * .1 + j / 12.0 + nz(sd + j) * .4, 1.0);
          double x = ex + .5 + (nz(sd * 3 + j) - .5) * 1.4 + std::sin(tt * .6 + j + sd) * .5 * ph, y = ey + .5 - ph * 3.4;
          if (!open_at(x, y)) continue;
          dot(x, y, mixc(0xB0A870, 0x2E2C22, ph), .7 * (1 - ph) * std::min(1.0, ph * 6), ph < .35 ? 1 : 2);
        }
        break;
      case 'V':
        // fine grey ash seeping in under a door or through its keyhole, creeping out over the floor both ways
        for (int j = 0; j < 12; ++j) {
          double ph = std::fmod(tt * .12 + j / 12.0 + nz(sd + j) * .3, 1.0);
          double side = j % 2 ? 1 : -1;
          double x = ex + .5 + (nz(sd * 3 + j) - .5) * 1.2 + (nz(sd + j * 5) - .5) * ph * 1.6, y = ey + .5 + side * (.4 + ph * 1.3);
          if (!open_at(x, y) || blocks(tile_of(map[int(y) * W + int(x)]))) continue;
          dot(x, y, mixc(0xA8A094, 0x3A3630, ph), .7 * (1 - ph));
        }
        break;
      case 'S':
        // ash welling up out of the hole behind the altar and spilling over its lip
        for (int j = 0; j < 6; ++j) {
          double ph = std::fmod(tt * .22 + j / 6.0 + nz(sd + j) * .5, 1.0);
          double x = ex + nz(sd * 3 + j) + std::sin(tt * 1.3 + j + sd) * .4 * ph, y = ey + .8 - ph * 2.4 + ph * ph * 1.2;
          dot(x, y, mixc(0xA8A094, 0x2A2826, ph), .8 * (1 - ph), ph < .5 ? 1 : 2);
        }
        break;
      case 'J': {
        // a drop falling from a seam (an iron maiden's: dark red) or from the ceiling into the water, and its splash
        bool water = sd >= 7777;
        double per = 2.2 + nz(sd) * 2, ph = std::fmod(tt / per + nz(sd + 3), 1.0);
        double x = ex + .3 + nz(sd + 1) * .4;
        if (ph < .3) dot(x, ey + (water ? -1.2 + ph / .3 * 1.6 : .8 + ph / .3 * .6), water ? 0x6A7A8A : 0x7A1410, .9);
        else if (ph < .42) { double k = (ph - .3) / .12; for (int d = -1; d <= 1; d += 2) dot(x + d * k * .35, ey + (water ? .45 : 1.4) - k * .1, water ? 0x8A9AAA : 0x4A0C0A, .7 * (1 - k)); }
        break;
      }
      case 'F':
        // black flakes of banner cloth coming off the tatters, drifting down and settling
        for (int j = 0; j < 3; ++j) {
          double ph = std::fmod(tt * .16 + j / 3.0 + nz(sd + j) * .5, 1.0);
          double x = ex + .5 + (nz(sd * 3 + j) - .5) * .8 + std::sin(tt * 2 + j + sd) * .25, y = ey + 1.3 + ph * 2.2;
          if (!open_at(x, y)) continue;
          dot(x, y, j % 3 ? 0x141010 : 0x5A1E12, .9 * std::min(1.0, (1 - ph) * 3));
        }
        break;
      case 'N':
        // black flakes of what was parchment, stirred up off the drift as you pass, sinking again
        for (int j = 0; j < 4; ++j) {
          double ph = std::fmod(tt * .35 + j / 4.0 + nz(sd + j), 1.0);
          double x = ex + .5 + (nz(sd * 3 + j) - .5) * .9 + std::sin(tt * 3 + j + sd) * .25, y = ey + .7 - std::sin(ph * PI) * 1.1;
          dot(x, y, j % 3 ? 0x141210 : 0x4A4440, .9 * std::sin(ph * PI));
        }
        break;
      case 'j':
        // embalming fumes: a sickly green breath seeping off the bier and creeping low
        for (int j = 0; j < 14; ++j) {
          double ph = std::fmod(tt * .13 + j / 14.0 + nz(sd + j) * .3, 1.0);
          double x = ex + .5 + (nz(sd * 3 + j) - .5) * 1.6 + std::sin(tt * .8 + j) * .4 + ph * 1.2, y = ey + .3 - ph * 1.6;
          if (!open_at(x, y)) continue;
          dot(x, y, mixc(0xB0E880, 0x2A3A1A, ph), .8 * (1 - ph), ph < .3 ? 1 : 2);
        }
        break;
      case 'G':
        // grit trickling down the fallen shaft onto the heap, and dust turning in the daylight
        for (int j = 0; j < 12; ++j) {
          double ph = std::fmod(tt * .35 + nz(sd + j), 1.0);
          double x = ex - 1 + nz(sd * 3 + j * 7) * 3 + std::sin(tt + j) * .1, y = ey - 3 + ph * 3.5;
          if (!open_at(x, y)) continue;
          dot(x, y, j % 3 ? 0xB4A896 : 0x786C5E, .8 * (1 - ph * .5), j % 4 ? 1 : 2);
        }
        for (int j = 0; j < 14; ++j) {
          double ph = std::fmod(tt * .05 + nz(sd * 7 + j), 1.0);
          double x = ex + .5 + (nz(sd + j * 11) - .5) * 6 + std::sin(tt * .4 + j) * .3, y = ey + .5 + (nz(sd + j * 5) - .5) * 4 - ph;
          if (!open_at(x, y)) continue;
          dot(x, y, 0xDCE0EA, .55 * std::sin(ph * PI));
        }
        break;
      case 'o': {
        // a glint off the ore
        double ph = std::fmod(tt * .35 + nz(sd * 13), 1.0);
        if (ph < .18) {
          double k = std::sin(ph / .18 * PI), cx = ex + .3 + nz(sd) * .4, cy = ey + .3 + nz(sd + 1) * .4;
          dot(cx, cy, 0xFFF8D2, k);
          for (P d4 : {P{1, 0}, P{-1, 0}, P{0, 1}, P{0, -1}}) dot(cx + d4.x * .14, cy + d4.y * .14, 0xFFF8D2, k * .7);
        }
        break;
      }
      case 'x': {
        // the broken frame: grit trickling where the ceiling sags, and now and then a puff of dust
        for (int j = 0; j < 6; ++j) {
          double ph = std::fmod(tt * .5 + j / 6.0, 1.0);
          dot(ex + .1 + nz(sd + j) * .8, ey - .8 + ph * 1.6, 0xAAA08C, .7 * (1 - ph));
        }
        double ph = std::fmod(tt * .17 + nz(sd), 1.0);
        if (ph < .2) {
          double k = ph / .2;
          for (int j = 0; j < 14; ++j) {
            double a = nz(j * 3 + sd) * PI;
            dot(ex + .5 + std::cos(a) * k * 1.6, ey + .4 + std::sin(a) * k * .6 + k * .5, 0xBEB4A0, (1 - k) * .8);
          }
        }
        break;
      }
      case 'P': {
        // now and then a pebble rolls over the lip and drops away into the dark
        double ph = std::fmod(tt * .3 + nz(sd), 1.0);
        if (ph < .5) dot(ex + .3 + nz(sd + 1) * .4, ey - .3 + ph * 3, 0x968C78, 1 - ph * 2);
        break;
      }
      case 'H': {
        // the air over a glowing fissure shivers with heat, and now and then a spark goes up
        for (int j = 0; j < 7; ++j) {
          double ph = std::fmod(tt * .5 + j / 7.0 + nz(sd + j) * .3, 1.0);
          double x = ex + .2 + nz(sd * 3 + j) * .6 + std::sin(tt * 9 + j * 2.1 + ph * 6) * (.1 + ph * .2), y = ey + .4 - ph * 1.9;
          if (!open_at(x, y)) continue;
          dot(x, y, mixc(0xFFC060, 0x8A3010, ph), .75 * (1 - ph * .8), ph < .4 ? 1 : 2);
        }
        if (sd % 2 == 0) {
          double ph = std::fmod(tt * .2 + nz(sd * 3), 1.0);
          if (ph < .22) {
            double k = ph / .22;
            double x = ex + .5 + std::sin(k * 7 + sd) * .3, y = ey + .4 - k * 3.4;
            if (open_at(x, y)) dot(x, y, mixc(0xFFF0A0, 0xFF4810, k), 1 - k * .7);
          }
        }
        break;
      }
      case 'Q': {
        // grey ash floating on the sludge, carried slowly along by the current, a flake turning over now and then
        for (int j = 0; j < 2; ++j) {
          double ph = std::fmod(tt * .05 + nz(sd * 5 + j), 1.0);
          double x = ex - 1 + ph * 3, y = ey + .25 + nz(sd + j * 3) * .5 + std::sin(tt * .6 + j + sd) * .06;
          if (int(x) < 0 || int(y) < 0 || int(x) >= W || int(y) >= H || tile_of(map[int(y) * W + int(x)]) != SLUDGE) continue;
          bool turn = std::fmod(tt * .7 + nz(sd + j) * 5, 4.0) < .3;
          dot(x, y, turn ? 0xB4B0A6 : 0x8A867E, std::sin(ph * PI) * .9);
        }
        break;
      }
      case 'W': {
        // sour vapour curling up off a basin
        for (int j = 0; j < 3; ++j) {
          double ph = std::fmod(tt * .12 + nz(sd * 3 + j), 1.0);
          double x = ex + .5 + (nz(sd + j) - .5) * .8 + std::sin(ph * 6 + j) * .3, y = ey + .6 - ph * 1.6;
          dot(x, y, mixc(0x5A6E46, 0x1E261A, ph), .55 * (1 - ph));
        }
        break;
      }
      case 'B': {
        // a bubble of foul gas swells in the sludge and bursts, a little yellow-green puff rising from it
        double ph = std::fmod(tt * .23 + nz(sd * 7), 1.0);
        if (ph >= .3 && ph < .42) {  // and a few drops of sludge thrown up as it bursts
          double k = (ph - .3) / .12;
          for (int j = 0; j < 4; ++j) dot(ex + .5 + (j - 1.5) * .22 * (.4 + k), ey + .5 - std::sin(k * PI) * .35, 0x6A7A5A, .8);
        }
        if (ph < .3) {
          double k = ph / .3, rr = .1 + k * .3;
          for (int j = 0; j < 8; ++j) {
            double a = j * PI / 4;
            dot(ex + .5 + std::cos(a) * rr * 1.2, ey + .55 + std::sin(a) * rr * .8, 0x6A7A4A, .7);
          }
        } else if (ph < .7) {
          double k = (ph - .3) / .4;
          for (int j = 0; j < 9; ++j) {
            double x = ex + .5 + (nz(sd + j) - .5) * (.4 + k * 1.4) + std::sin(tt * 2 + j) * .1, y = ey + .4 - k * (.6 + nz(sd * 3 + j) * 1.2);
            dot(x, y, mixc(0xB8C86A, 0x3A4A2A, k), .6 * (1 - k), j % 3 ? 1 : 2);
          }
        }
        break;
      }
      case 'E': {
        // the Deep breathes: sparks thrown up off the lava
        for (int j = 0; j < 4; ++j) {
          double ph = std::fmod(tt * .8 + j / 4.0 + nz(sd + j), 1.0);
          double x = ex + .5 + (nz(sd * 3 + j) - .5) * 1.6 + std::sin(tt * 3 + j) * ph * .5, y = ey + .3 - ph * (2.5 + nz(sd + j * 7) * 2);
          dot(x, y, mixc(0xFFF0A0, 0xC03008, ph), 1 - ph * .8, j ? 1 : 2);
        }
        break;
      }
      case 'D':
        // a draught through the tunnel: dust blown along, all one way
        for (int j = 0; j < 3; ++j) {
          double ph = std::fmod(tt * .45 + j / 3.0 + nz(sd * 5 + j), 1.0);
          double x = ex + .5 + wind_ * (ph * 3.5 - 1.75), y = ey + .2 + nz(sd + j * 7) * .6 + std::sin(tt * 3 + j + sd) * .12;
          if (!open_at(x, y) || blocks(tile_of(map[int(y) * W + int(x)]))) continue;
          dot(x, y, j ? 0xB8AC98 : 0xDED4C0, .75 * std::sin(ph * PI));
          dot(x - wind_ * .14, y, 0xA89C88, .4 * std::sin(ph * PI));
          dot(x - wind_ * .28, y, 0xA89C88, .15 * std::sin(ph * PI));  // a short streak behind it
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
  if (static const char* demo = SDL_getenv("PYREHEART_FXDEMO"); demo && !snap_["heroes"].empty()) {
    // for recording: now and then a soul or a quake beside the hero ("soul", "quake" or both)
    static double last = -9;
    static int n = 0;
    if (tt - last > 2.6) {
      last = tt;
      std::string d = demo;
      bool soul = d == "soul" || (d != "quake" && n % 2 == 0);
      int hx = snap_["heroes"][0]["x"], hy = snap_["heroes"][0]["y"];
      fx_.push_back({json{{"k", soul ? "soul" : "quake"}, {"x", hx + 3}, {"y", hy - 2}, {"dur", 2.2}}, tt});
      ++n;
    }
  }
  if (snap_.value("act", 0) == 1 && !snap_["heroes"].empty() && !snap_.value("rest", false)) {
    // now and then a few bats flit through the light, from one side of it to the other
    double period = 7.5, cyc = std::floor(tt / period), ph = tt / period - cyc;
    if (ph < .3) {
      int seed = int(cyc) * 7919;
      int hx = snap_["heroes"][0]["x"], hy = snap_["heroes"][0]["y"];
      double a = nz(seed) * 2 * PI;
      double sx = hx + .5 + std::cos(a) * 8, sy = hy + .5 + std::sin(a) * 5, mx = hx + .5 + (nz(seed + 1) - .5) * 4, my = hy + .5 + (nz(seed + 2) - .5) * 3;
      double ex2 = hx + .5 - std::cos(a) * 8, ey2 = hy + .5 - std::sin(a) * 5;
      for (int j = 0; j < 5; ++j) {
        double k = std::clamp(ph / .3 - j * .04, 0.0, 1.0), u = 1 - k;
        double x = u * u * sx + 2 * u * k * mx + k * k * ex2 + std::sin(tt * 30 + j * 2) * .25;
        double y = u * u * sy + 2 * u * k * my + k * k * ey2 + std::sin(tt * 23 + j) * .3 + (j - 2) * .25;
        if (!open_at(x, y)) continue;
        bool wing = std::sin(tt * 40 + j * 3) > 0;
        dot(x, y, 0x141012, 1);
        dot(x - .14, y - (wing ? .1 : -.05), 0x2A2224, 1);
        dot(x + .14, y - (wing ? .1 : -.05), 0x2A2224, 1);
      }
    }
  }
  if (snap_.value("act", 0) == 2 && !snap_["heroes"].empty() && !snap_.value("rest", false)) {
    // the draught: ash and grit skim across the floor through the hero's light, all one way, then it dies down
    double g = draught(tt), cyc = std::floor(tt / DRAUGHT);
    if (g > .05) {
      int seed = int(cyc) * 4447, dir = nz(seed) < .5 ? 1 : -1;
      int hx = snap_["heroes"][0]["x"], hy = snap_["heroes"][0]["y"];
      double ph = std::fmod(tt / DRAUGHT, 1.0) / .3;
      for (int j = 0; j < 110; ++j) {
        double st = nz(seed + j * 3) * .6, k = (ph - st) / .4;
        if (k < 0 || k > 1) continue;
        double x = hx + .5 + dir * (-12 + k * 24), y = hy + .5 + (nz(seed + j * 7) - .5) * 12 + std::sin(k * 6 + j) * .35;
        if (!open_at(x, y) || blocks(tile_of(map[int(y) * W + int(x)]))) continue;
        double a = std::min(1.0, std::sin(k * PI) * 2) * std::min(1.0, g * 2);
        int c = j % 3 ? 0x9A928A : 0xC8BEB0;
        dot(x, y, c, .9 * a);
        dot(x - dir * .15, y, c, .5 * a);  // a short streak behind it
      }
    }
  }
  if (snap_.contains("ents"))
    for (auto& e : snap_["ents"]) {
      // a heap of gold that is not one: when it twitches, a coin slides off it
      if (!e.value("hidden", false)) continue;
      double ph = std::fmod(tt * .37 + e["id"].get<int>() * .61, 6.0);
      if (ph > .6) continue;
      double k = ph / .6, mx = e["x"].get<int>(), my = e["y"].get<int>();
      if (!open_at(mx, my)) continue;
      dot(mx + .5 + k * .9, my + .55 + k * k * .35, 0xFFD24A, 1 - k * .5, 2);
    }
  if (int ap = snap_.value("act", 0); ap != 1 && ap != 3 && !snap_["heroes"].empty() && !snap_.value("rest", false)) {
    if (double age = tt - groan_t_; zone_fx("q1") && age < 2.6) {
      // the halls shudder: dust and grit come down off every ceiling and cornice, a stone or a piece of carving among them
      for (int j = 0; j < 1800; ++j) {
        double st = nz(j * 5 + int(groan_t_)) * 1.2, ag = age - st;
        if (ag < 0 || ag > 1.2) continue;
        double x = camx_ + nz(j * 3 + 1) * vw_, y = camy_ + nz(j * 11 + 2) * vh_ - 1.5 + ag * ag * 4;
        if (!open_at(x, y)) continue;
        bool grit = j % 4 == 0, stone = j % 23 == 0;
        dot(x, y, stone ? 0x8A847A : grit ? 0xA8A296 : 0xD0CAC0, (1 - ag / 1.2) * (grit ? 1 : .75), stone ? 3 : grit ? 2 : 1);
      }
      // where the stones land on the paving, a little burst of dust
      for (int j = 0; j < 1800; j += 23) {
        double st = nz(j * 5 + int(groan_t_)) * 1.2, ag = age - st - 1.2;
        if (ag < 0 || ag > .6) continue;
        double bx = camx_ + nz(j * 3 + 1) * vw_, by = camy_ + nz(j * 11 + 2) * vh_ - 1.5 + 4 * 1.44;
        if (!open_at(bx, by)) continue;
        for (int k = 0; k < 8; ++k) {
          double a = PI + nz(j + k * 7) * PI, rr = ag / .6 * (.4 + nz(k) * .8);
          dot(bx + std::cos(a) * rr * 1.4, by + std::sin(a) * rr * .5, 0xB0AAA0, .7 * (1 - ag / .6));
        }
      }
    }
  }
  if (snap_.value("act", 0) == SANCTUM && zone_fx("n2") && !snap_["heroes"].empty() && !snap_.value("rest", false)) {
    // the haze of incense and bone-dust that never cleared: pale motes drifting slowly through the halls
    for (int j = 0; j < 120; ++j) {
      double v = .15 + nz(j * 5) * .25, span = vw_ + 4, ph = std::fmod(tt * v / span + nz(j * 3), 1.0);
      double x = camx_ - 2 + ph * span, y = camy_ + nz(j * 7) * vh_ + std::sin(tt * .4 + j) * .6;
      if (!open_at(x, y)) continue;
      dot(x, y, mixc(0x3A3642, 0x7A7484, .5 + .5 * std::sin(tt * .7 + j)), .55 * std::min(1.0, std::min(ph, 1 - ph) * 10));
    }
  }
  if (snap_.value("act", 0) == BARRACKS && !snap_["heroes"].empty() && !snap_.value("rest", false)) {
    if (double age = tt - drum_t_; zone_fx("b3") && age < 3.2) {
      // the war-drum: on each of its two beats dust trickles down from the cracks in the low ceilings
      for (int beat = 0; beat < 2; ++beat) {
        double ag = age - beat * .55;
        if (ag < 0 || ag > 1.6) continue;
        for (int j = 0; j < 90; ++j) {
          int sdj = j * 7 + beat * 131 + int(drum_t_);
          double st = nz(sdj) * .4, a2 = ag - st;
          if (a2 < 0 || a2 > 1.0) continue;
          double x = camx_ + nz(sdj + 1) * vw_, y0 = camy_ + nz(sdj + 2) * vh_;
          for (int k = 0; k < 3; ++k) {
            double y = y0 + a2 * a2 * 3.5 - k * .2;
            if (!open_at(x, y)) continue;
            dot(x, y, mixc(0xA8A094, 0x4A4440, a2), (1 - a2) * .85);
          }
        }
      }
    }
    if (zone_fx("b1")) {
      // soot puffs up under every boot that takes a step, the dead's and yours
      std::map<int, int> now_at;
      auto stepped = [&](int id, int x, int y) {
        now_at[id] = y * W + x;
        auto it = ent_cell_.find(id);
        if (it != ent_cell_.end() && it->second != y * W + x) puffs_.push_back({double(x), double(y), tt});
      };
      int hid = -1;
      for (auto& hh2 : snap_["heroes"]) stepped(hid--, hh2["x"].get<int>(), hh2["y"].get<int>());
      if (snap_.contains("ents")) for (auto& e : snap_["ents"]) if (e.contains("id")) stepped(e["id"].get<int>(), e["x"].get<int>(), e["y"].get<int>());
      ent_cell_ = std::move(now_at);
      while (!puffs_.empty() && tt - puffs_.front()[2] > .9) puffs_.pop_front();
      while (puffs_.size() > 80) puffs_.pop_front();
      for (auto& pf : puffs_) {
        double k = (tt - pf[2]) / .9;
        if (!open_at(pf[0] + .5, pf[1] + .5)) continue;
        for (int j = 0; j < 9; ++j) {
          double a = PI + nz(j * 7 + int(pf[2] * 10)) * PI, rr = k * (.4 + nz(j) * .8);
          dot(pf[0] + .5 + std::cos(a) * rr * 1.4, pf[1] + .9 + std::sin(a) * rr * .5 - k * .2, mixc(0x8A8278, 0x2A2622, k), .7 * (1 - k), j % 4 ? 1 : 2);
        }
      }
    }
  }
  if (snap_.value("act", 0) == CITY && !snap_["heroes"].empty() && !snap_.value("rest", false)) {
    auto tl = [&](int x, int y) { return x < 0 || y < 0 || x >= W || y >= H ? int(WALL) : tile_of(map[y * W + x]); };
    if (zone_fx("k1"))
      // the ash still comes down out of the dark like a slow grey snow
      for (int j = 0; j < 170; ++j) {
        double v = .45 + nz(j * 5) * .5, span = vh_ + 4, ph = std::fmod(tt * v / span + nz(j * 3), 1.0);
        double x = camx_ + nz(j * 7) * vw_ + std::sin(tt * .5 + j) * .7, y = camy_ - 2 + ph * span;
        if (!open_at(x, y)) continue;
        int c = j % 4 == 0 ? 0x3A3632 : mixc(0x6A6660, 0xB0AAA2, nz(j * 11));
        dot(x, y, c, .75 * std::min(1.0, std::min(ph, 1 - ph) * 12), j % 9 == 0 ? 2 : 1);
      }
    // ash puffs up round the feet of whoever wades through a drift
    if (zone_fx("k1")) {
      int hx = snap_["heroes"][0]["x"], hy = snap_["heroes"][0]["y"];
      if (hx != step_x_ || hy != step_y_) {
        step_x_ = hx; step_y_ = hy;
        if (tl(hx, hy) == DRIFT) step_t_ = tt;
      }
      double age = tt - step_t_;
      if (age < 1.1 && tl(hx, hy) == DRIFT) {
        double k = age / 1.1;
        for (int j = 0; j < 22; ++j) {
          double a = PI + nz(j * 7 + int(step_t_ * 10)) * PI, rr = k * (.6 + nz(j) * 1.2);
          dot(hx + .5 + std::cos(a) * rr * 1.5, hy + .85 + std::sin(a) * rr * .7 - k * .3, mixc(0xB0AAA2, 0x4A4642, k), .8 * (1 - k), j % 4 ? 1 : 2);
        }
      }
    }
  }
  if (snap_.value("act", 0) == 1 && !snap_["heroes"].empty() && !snap_.value("rest", false)) {
    if (double age = tt - groan_t_; cave_fx(3) && age < 2.6) {
      // the mountain groans: grit and dust come down all over the view, a stone or two among them
      for (int j = 0; j < 700; ++j) {
        double st = nz(j * 5 + int(groan_t_)) * 1.2, ag = age - st;
        if (ag < 0 || ag > 1.2) continue;
        double x = camx_ + nz(j * 3 + 1) * vw_, y = camy_ + nz(j * 11 + 2) * vh_ - 1.5 + ag * ag * 4;
        if (!open_at(x, y)) continue;
        bool grit = j % 5 == 0, stone = j % 37 == 0;
        dot(x, y, stone ? 0x6A6052 : grit ? 0x9A8E7A : 0xD8CEBC, (1 - ag / 1.2) * (grit ? 1 : .8), stone ? 3 : grit ? 2 : 1);
      }
    }
    if (cave_fx(4)) {
      // eyes in the dark at the edge of the light, watching; they are gone when you come near
      int hx = snap_["heroes"][0]["x"], hy = snap_["heroes"][0]["y"];
      double period = 9;
      int cyc = int(std::floor(tt / period));
      double ph = tt / period - cyc;
      for (int e = 0; e < 2; ++e) {
        int seed = cyc * 7919 + e * 131;
        if (nz(seed + 5) < .35 || eyes_gone_ == cyc * 2 + e) continue;
        // a spot out of sight, seven to ten steps off, on open ground, chosen when they open
        static int cyc_at[2] = {-1, -1};
        static P spot[2];
        if (cyc_at[e] != cyc) { cyc_at[e] = cyc; spot[e] = {-1, -1}; }
        P& at = spot[e];
        for (int k = 0; k < 40 && at.x < 0 && ph > .07 && ph < .1; ++k) {
          double a = nz(seed + k * 3) * 2 * PI, rr = 7 + nz(seed + k * 3 + 1) * 3;
          int x = hx + int(std::lround(std::cos(a) * rr)), y = hy + int(std::lround(std::sin(a) * rr * .7));
          if (x < 1 || y < 1 || x >= W - 1 || y >= H - 1 || blocks(tile_of(map[y * W + x])) || vis[y * W + x] == '1') continue;
          at = {x, y};
        }
        if (at.x < 0) continue;
        double d = std::hypot(at.x - hx, (at.y - hy) * 1.3);
        if (d < 5.5 || vis[at.y * W + at.x] == '1') { eyes_gone_ = cyc * 2 + e; continue; }
        double on = std::min({1.0, (ph - .08) / .08, (.85 - ph) / .08});
        if (on <= 0) continue;
        double blink = std::fmod(tt * .7 + nz(seed + 9), 1.0) < .06 ? 0 : 1;
        bool rat = nz(seed + 7) < .5;
        int c = rat ? 0xE04828 : 0xE8D070;
        double x = at.x + .3 + std::sin(tt * .5 + seed) * .08, y = at.y + .45;
        for (double ox2 : {0.0, rat ? .25 : .4}) {
          dot(x + ox2 - px / cell, y - px / cell, c, on * blink * .22, 4);
          dot(x + ox2, y, c, on * blink, 2);
        }
      }
    }
  }
  int actp = snap_.value("act", 0);
  if (actp == 0 && !snap_["heroes"].empty() && !snap_.value("rest", false)) {
    auto tl = [&](int x, int y) { return x < 0 || y < 0 || x >= W || y >= H ? int(WALL) : tile_of(map[y * W + x]); };
    if (actp == 0 && zone_fx("c2") && ghost_a_ > 0) {
      // a soft cold halo round it, brightest at the middle, breathing slowly
      double pulse = .8 + .2 * std::sin(tt * 2.2);
      for (int r2 = 4; r2 >= 1; --r2) {
        double rr = r2 * .45 * pulse;
        dot(ghost_x_ + .5 - rr, ghost_y_ + .5 - rr, 0x9AC8FF, ghost_a_ * .1, rr * 16);
      }
      // the ghost breathes frost: pale glittering motes round it and falling behind it
      for (int j = 0; j < 18; ++j) {
        double ph = std::fmod(tt * .6 + nz(j * 7), 1.0);
        double x = ghost_x_ + .5 + (nz(j * 3) - .5) * 1.6 - ghost_dx_ * ph * 1.5, y = ghost_y_ - .2 + (nz(j * 5) - .5) * 1.8 + ph * .8;
        double tw = std::sin(ph * PI) * (.5 + .5 * std::sin(tt * 9 + j * 2));
        dot(x, y, j % 3 ? 0xC8E0FF : 0xFFFFFF, ghost_a_ * tw * .8);
      }
    }
  }
  if (snap_.value("act", 0) == 0 && crypt_fx() && !snap_.value("rest", false)) {
    auto tl = [&](int x, int y) { return x < 0 || y < 0 || x >= W || y >= H ? int(WALL) : tile_of(map[y * W + x]); };
    // the grave chill as wisps: slow pale motes drifting low over the misty ground, pushed aside by a hero
    if (!chill_.empty()) {
      int x0 = std::max(0, camx_), y0 = std::max(0, camy_), x1 = std::min(W, camx_ + vw_), y1 = std::min(H, camy_ + vh_);
      for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
          float c = chill_[y * W + x];
          if (c <= 0 || vis[y * W + x] != '1' || hash(x, y, 21) % 3) continue;
          int sd = x * 131 + y * 17;
          for (int j = 0; j < 2; ++j) {
            double ph = std::fmod(tt * .06 + nz(sd + j), 1.0);
            double wx = x + ph * 2.5 - .8 + std::sin(tt * .4 + sd + j) * .3, wy = y + .5 + std::sin(tt * .3 + j + sd) * .4;
            for (auto& h : snap_["heroes"]) {
              double dx = wx - h["x"].get<int>() - .5, dy = (wy - h["y"].get<int>() - .5) * 1.3, d = std::hypot(dx, dy);
              if (d < 2 && d > .01) { wx += dx / d * (2 - d) * .6; wy += dy / d * (2 - d) * .4; }
            }
            if (!open_at(wx, wy) || blocks(tl(int(wx), int(wy)))) continue;
            dot(wx, wy, 0xA8B8D0, .35 * c * std::sin(ph * PI), j ? 1 : 2);
          }
        }
    }
    // dust turning slowly in the coloured window light
    for (auto& b : beams_)
      for (int j = 0; j < 14; ++j) {
        double ph = std::fmod(tt * .05 + nz(b.wx * 7 + j), 1.0), d = nz(b.wx * 13 + j * 5) * b.len;
        double st = 1 + d + std::sin(tt * .3 + j) * .3 - ph * .8, y = b.wy + .5 + b.dir * st, x = b.wx + .5 + st * .45 + (nz(j * 3 + b.wx) - .5) * 1.6;
        if (!open_at(x, y)) continue;
        dot(x, y, mixc(b.col, 0xFFFFFF, .6), .7 * std::sin(ph * PI));
      }
    for (auto& h : snap_["heroes"]) {
      if (h.value("gone", false) || h.value("dead", false)) continue;
      int hx = h["x"], hy = h["y"], sd = h.value("pid", 1);
      // breath in the cold of the tombs
      double ph = std::fmod(tt / 2.6 + sd * .37, 1.0);
      if (ph < .55) {
        double k = ph / .55;
        for (int j = 0; j < 6; ++j) {
          double x = hx + .75 + k * (.8 + nz(sd * 9 + j) * .8) + (nz(j) - .5) * k * .6, y = hy + .25 - k * (.4 + nz(j * 3) * .5);
          dot(x, y, 0xC8D0DC, .45 * (1 - k), k > .5 ? 2 : 1);
        }
      }
      // bone dust: a step onto old bones kicks up a little grey puff
      if (sd == pid_ || snap_["heroes"].size() == 1) {
        if (hx != step_x_ || hy != step_y_) {
          step_x_ = hx; step_y_ = hy;
          if (tl(hx, hy) == BONES) step_t_ = tt;
        }
        double age = tt - step_t_;
        if (age < .8) {
          double k = age / .8;
          for (int j = 0; j < 16; ++j) {
            double a = PI + nz(j * 7 + int(step_t_ * 10)) * PI, rr = k * (.5 + nz(j) * .9);
            dot(hx + .5 + std::cos(a) * rr * 1.4, hy + .85 + std::sin(a) * rr * .6 + k * k * .3, mixc(0xD8D0B8, 0x5A5448, k), .8 * (1 - k), j % 4 ? 1 : 2);
          }
        }
      }
    }
  }
  for (auto& f : fx_) {
    std::string k = f.kw.value("k", "");
    if (k != "soul" && k != "quake") continue;
    if (k == "soul" && !(snap_.value("act", 0) == 0 && crypt_fx())) continue;
    if (k == "quake" && !crypt_fx()) continue;
    double age = tt - f.start;
    int bx = f.kw.value("x", 0), by = f.kw.value("y", 0);
    if (k == "soul") {
      // what held the dead up lets go: a pale wisp rising, with a fading trail
      if (age > 2.2 || !open_at(bx, by)) continue;
      double q = age / 2.2;
      for (int j = 0; j < 16; ++j) {
        double qq = std::max(0.0, q - j * .02);
        double x = bx + .5 + std::sin(qq * 9 + bx) * .35 * (1 + qq), y = by + .3 - qq * 3.4;
        double a = (1 - q * q) * (1 - j / 16.0) * std::min(1.0, age * 4);
        if (j == 0) {
          dot(x - 2 * px / cell, y - 2 * px / cell, 0x6A8AD0, a * .25, 5);
          dot(x - px / cell, y - px / cell, 0xA8C0F0, a * .5, 3);
        }
        dot(x, y, j == 0 ? 0xFFFFFF : mixc(0xC8D8FF, 0x506080, j / 16.0), a, j < 3 ? 2 : 1);
      }
    } else {
      // the vault shakes: dust and grit fall from the ceiling all round, sparks fly where the blow lands
      if (age > 1.8) continue;
      for (int j = 0; j < 90; ++j) {
        double st = nz(j * 5 + bx) * .6, ag = age - st;
        if (ag < 0 || ag > 1.1) continue;
        double x = bx + .5 + (nz(j * 3 + by) - .5) * 14, y0 = by + (nz(j * 11) - .5) * 9 - 1.2, y = y0 + ag * ag * 3.2;
        if (!open_at(x, y)) continue;
        bool grit = j % 5 == 0;
        dot(x, y, grit ? 0xA89C88 : 0xD8D0C0, (1 - ag / 1.1) * (grit ? 1 : .75), grit ? 2 : 1);
      }
      if (age < .35) {
        double q = age / .35;
        for (int j = 0; j < 28; ++j) {
          double a = nz(j * 13 + bx) * 2 * PI, rr = q * (1 + nz(j) * 2);
          dot(bx + .5 + std::cos(a) * rr * 1.4, by + .5 + std::sin(a) * rr * .7 + q * q * .5, mixc(0xFFF0C0, 0xFF5010, q), 1 - q * .7, j % 5 ? 1 : 2);
        }
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
  if (tr.scale_x > 0) { draw_particles(r, tr.offset_x, tr.offset_y, 1.0 / tr.scale_x); draw_ui_particles(r, tr.offset_x, tr.offset_y, 1.0 / tr.scale_x); }
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
