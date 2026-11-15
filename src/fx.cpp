#include "fx.hpp"

#include <algorithm>
#include <cmath>

namespace fx {
namespace {

const double STEP = .02, BOLT_MAX = .12, BURST_FRAME = .05, ROUND_STEP = .03, FRONT = .04, ROUND_DUR = .22;
const double SWING_TICK = .045, SWING_LIT = .12, SPIN_TICK = .03, CRIT_TIME = .26, BLOOD_TIME = .3, CLOUD_DUR = .22;
const Rgb STEEL_DIM{150, 152, 160}, GOLD_CRIT{255, 214, 90}, BLOOD_RED{200, 30, 30}, DUST{150, 140, 130};
const Rgb WHITE{255, 255, 255}, HOT{255, 250, 220}, DARK{14, 12, 12};
// the signs, as code points of the tile set
const int MID = 0x00B7, BUL = 0x2219, DOT = 0x2022, SHADE = 0x2591;

int clamp255(double v) { return v < 0 ? 0 : v > 255 ? 255 : int(v); }

int line_char(int dx, int dy) {
  if (dx == 0) return '|';
  if (dy == 0) return '-';
  return (dx > 0) == (dy > 0) ? '\\' : '/';
}
int sgn(int v) { return (v > 0) - (v < 0); }
double bolt_time(size_t n) { return std::min(BOLT_MAX, STEP * std::max<size_t>(1, n)); }
Rgb fade(Rgb c, double k) { return scale(c, .45 + .55 * std::clamp(k, 0.0, 1.0)); }

std::vector<std::pair<int, int>> pts(const nlohmann::json& a) {
  std::vector<std::pair<int, int>> v;
  if (a.is_array())
    for (auto& p : a) v.push_back({p[0].get<int>(), p[1].get<int>()});
  return v;
}
std::vector<int> signs_of(const std::string& s) {  // UTF-8 to code points
  std::vector<int> out;
  for (size_t i = 0; i < s.size();) {
    unsigned char c = s[i];
    int cp, n;
    if (c < 0x80) { cp = c; n = 1; }
    else if ((c >> 5) == 6) { cp = c & 0x1F; n = 2; }
    else if ((c >> 4) == 14) { cp = c & 0x0F; n = 3; }
    else { cp = c & 0x07; n = 4; }
    for (int k = 1; k < n && i + k < s.size(); ++k) cp = (cp << 6) | (s[i + k] & 0x3F);
    out.push_back(cp);
    i += n;
  }
  return out;
}

// An area of radius r drawn so it looks round: a front of signs runs out by real distance, the ground behind it
// glows and dims, the corners are drawn softer, and a faint cloud beyond the sides rounds it off.
std::vector<Cell> round(int x, int y, int r, Rgb col, double age, const std::vector<int>& signs, bool hollow,
                        int sign_override, bool glow, bool cloud, double dur, bool sound) {
  std::vector<Cell> out;
  for (int dy = -r; dy <= r; ++dy)
    for (int dx = -r; dx <= r; ++dx) {
      if (hollow && dx == 0 && dy == 0) continue;
      double d = std::sqrt(double(dx * dx + dy * dy));
      double q = age - d * ROUND_STEP;
      if (q < 0 || q >= dur) continue;
      bool soft = d > r + .5;
      int st = int(q / FRONT);
      if (st < int(signs.size())) {
        Rgb c = st == 0 ? mix(col, HOT, .5) : fade(col, 1 - st * .3);
        int ch = signs[st];
        if (st == 0 && sound) ch = std::abs(dx) >= std::abs(dy) ? (dx < 0 ? '(' : ')') : '-';
        else if (st == 0 && sign_override) ch = sign_override;
        out.push_back({x + dx, y + dy, ch, soft ? scale(c, .72) : c});
      } else if (glow) {
        double k = (q - FRONT * signs.size()) / std::max(.01, dur - FRONT * signs.size());
        out.push_back({x + dx, y + dy, SHADE, mix(col, DARK, std::min(.95, .3 + .55 * k + (soft ? .15 : 0)))});
      }
    }
  if (cloud && r >= 1) {
    double a = age - ROUND_STEP * r;
    if (a >= 0 && a < CLOUD_DUR) {
      double k = a / CLOUD_DUR;
      Rgb base = mix(col, DUST, .5);
      double rr = r * 1.415 + .7;
      for (int dy = -r - 2; dy <= r + 2; ++dy)
        for (int dx = -r - 2; dx <= r + 2; ++dx) {
          if (std::max(std::abs(dx), std::abs(dy)) <= r || dx * dx + dy * dy > rr * rr) continue;
          if (k > .6 && (((x + dx) * 73856093) ^ ((y + dy) * 19349663)) % 5 == 0) continue;
          out.push_back({x + dx, y + dy, k < .55 ? SHADE : MID, mix(base, DARK, .35 + .55 * k)});
        }
    }
  }
  return out;
}

}  // namespace

Rgb rgb(int c) { return {(c >> 16) & 255, (c >> 8) & 255, c & 255}; }
Rgb mix(Rgb a, Rgb b, double f) {
  return {clamp255(a.r + (b.r - a.r) * f), clamp255(a.g + (b.g - a.g) * f), clamp255(a.b + (b.b - a.b) * f)};
}
Rgb scale(Rgb c, double f) { return {clamp255(c.r * f), clamp255(c.g * f), clamp255(c.b * f)}; }

double life(const nlohmann::json& kw) {
  std::string kind = kw.value("k", "");
  double d = kw.value("delay", 0.0);
  int r = kw.value("r", 1);
  if (kind == "bolt") return d + bolt_time(kw["cells"].size()) + .06;
  if ((kind == "burst" || kind == "wave") && r >= 1) {
    double g = kind == "burst" && kw.value("gather", false) ? BURST_FRAME : 0;
    return d + g + std::max(ROUND_STEP * r * 1.415 + kw.value("dur", ROUND_DUR), ROUND_STEP * r + CLOUD_DUR);
  }
  if (kind == "burst") return d + BURST_FRAME * (r + 3);
  if (kind == "swing") return d + SWING_TICK * 3 + SWING_LIT;
  if (kind == "spin") return d + SPIN_TICK * 8 + SWING_LIT;
  if (kind == "crit") return d + CRIT_TIME;
  if (kind == "blood") return d + BLOOD_TIME;
  return d + kw.value("dur", .4);
}

std::vector<Cell> cells(const nlohmann::json& kw, double age) {
  std::string kind = kw.value("k", "");
  age -= kw.value("delay", 0.0);
  std::vector<Cell> out;
  if (age < 0) return out;
  Rgb col = rgb(kw.value("col", 0xFFFFFF));
  int x = kw.value("x", 0), y = kw.value("y", 0), r = kw.value("r", 1);
  if (kind == "bolt") {
    auto cs = pts(kw["cells"]);
    if (cs.empty()) return out;
    int n = int(cs.size());
    int head = int(age / bolt_time(n) * n);
    std::string chs = kw.value("ch", "*");
    int ch = chs.empty() ? '*' : chs[0];
    if (ch == '-' || ch == '|' || ch == '/' || ch == '\\') {
      auto src = kw.contains("src") ? std::make_pair(kw["src"][0].get<int>(), kw["src"][1].get<int>()) : cs.front();
      ch = line_char(sgn(cs.back().first - src.first), sgn(cs.back().second - src.second));
    }
    for (int k = 0; k < 4; ++k) {
      int i = head - k;
      if (i >= 0 && i < n)
        out.push_back({cs[i].first, cs[i].second, k == 0 ? ch : k == 1 ? MID : BUL, k == 0 ? mix(col, {255, 255, 240}, .35) : fade(col, 1 - k * .28)});
    }
    return out;
  }
  if (kind == "burst") {
    bool gather = kw.value("gather", false);
    if (gather) {
      if (age < BURST_FRAME) {
        int rr = r + 1;
        for (int dy = -rr; dy <= rr; ++dy)
          for (int dx = -rr; dx <= rr; ++dx)
            if (std::max(std::abs(dx), std::abs(dy)) == rr) out.push_back({x + dx, y + dy, MID, fade(col, .55)});
        return out;
      }
      age -= BURST_FRAME;
    }
    return round(x, y, r, col, age, signs_of(kw.value("signs", "*+·")), kw.value("hollow", false), 0, true, true,
                 kw.value("dur", ROUND_DUR), false);
  }
  if (kind == "wave") {
    std::string sign = kw.value("sign", "*");
    bool quiet = sign == "sound" || sign == "~" || sign == "!";
    std::vector<int> signs = sign == "*" ? signs_of("*+·") : std::vector<int>{sign == "sound" ? '(' : signs_of(sign)[0], MID};
    return round(x, y, r, col, age, signs, true, 0, !quiet, !quiet, kw.value("dur", ROUND_DUR), sign == "sound");
  }
  if (kind == "rise") {  // small crosses of light rising off whoever is healed
    double dur = kw.value("dur", .4);
    int dxs[3] = {0, -1, 1};
    for (int j = 0; j < 3; ++j) {
      double k = (age - j * .07) / (dur - .14);
      if (k >= 0 && k < 1) out.push_back({x + dxs[j], y - 1 - int(k * 2), '+', fade(col, 1 - k * .7)});
    }
    return out;
  }
  if (kind == "line") {  // a stroke: the whole line at once, white to its colour
    auto cs = pts(kw["cells"]);
    double k = age / kw.value("dur", .4);
    if (k >= 1 || cs.empty()) return out;
    int ch = line_char(sgn(cs.back().first - cs.front().first), sgn(cs.back().second - cs.front().second));
    Rgb c = mix(WHITE, col, std::min(1.0, k * 1.6));
    for (auto [cx, cy] : cs) out.push_back({cx, cy, ch, k < .6 ? c : fade(c, 1 - (k - .6) / .4)});
    return out;
  }
  if (kind == "inward") {  // a ring of motes drawn in to one spot, then gone
    double k = age / kw.value("dur", .4);
    if (k >= 1) return out;
    int rr = std::max(0, int(std::lround(3 * (1 - k))));
    if (rr == 0) return {{x, y, DOT, mix(col, WHITE, .3)}};
    for (int dy = -rr; dy <= rr; ++dy)
      for (int dx = -rr; dx <= rr; ++dx)
        if (std::max(std::abs(dx), std::abs(dy)) == rr && (dx + dy) % 2 == 0) out.push_back({x + dx, y + dy, BUL, fade(col, .5 + k / 2)});
    return out;
  }
  if (kind == "swing") {  // \ | / along the side of the foe, white fading to steel
    auto src = std::make_pair(kw["src"][0].get<int>(), kw["src"][1].get<int>());
    static const int dirs[8][2] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
    for (auto [tx, ty] : pts(kw["cells"])) {
      int dx = sgn(tx - src.first), dy = sgn(ty - src.second);
      int i = -1;
      for (int k = 0; k < 8; ++k) if (dirs[k][0] == dx && dirs[k][1] == dy) i = k;
      if (i < 0) continue;
      int far = std::max(std::abs(tx - src.first), std::abs(ty - src.second));
      int n = 0;
      for (int k = -1; k <= 1; ++k, ++n) {
        int ex = dirs[(i + k + 8) % 8][0], ey = dirs[(i + k + 8) % 8][1];
        double a = age - n * SWING_TICK;
        if (a >= 0 && a < SWING_LIT) out.push_back({src.first + ex * far, src.second + ey * far, line_char(ex, ey), mix(WHITE, STEEL_DIM, a / SWING_LIT)});
      }
    }
    return out;
  }
  if (kind == "spin") {  // the blade once round you
    static const int dirs[8][2] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
    for (int i = 0; i < 8; ++i) {
      double a = age - i * SPIN_TICK;
      if (a >= 0 && a < SWING_LIT) out.push_back({x + dirs[i][0], y + dirs[i][1], line_char(dirs[i][0], dirs[i][1]), mix(WHITE, STEEL_DIM, a / SWING_LIT)});
    }
    return out;
  }
  if (kind == "crit") {  // a white star that turns gold and fades
    double k = age / CRIT_TIME;
    if (k < 1) out.push_back({x, y, '*', k < .6 ? mix(WHITE, GOLD_CRIT, std::min(1.0, k * 2)) : fade(GOLD_CRIT, 1 - (k - .6) / .4)});
    return out;
  }
  if (kind == "blood") {  // a red * on the cut, then drops fly back and darken
    int sx = kw.contains("src") ? kw["src"][0].get<int>() : x - 1, sy = kw.contains("src") ? kw["src"][1].get<int>() : y;
    int dx = sgn(x - sx), dy = sgn(y - sy);
    double k = age / BLOOD_TIME;
    if (k >= 1) return out;
    if (k < .3) out.push_back({x, y, '*', BLOOD_RED});
    int offs[3][2] = {{dx, dy}, {dx - dy, dy + dx}, {dx + dy, dy - dx}};
    for (int j = 0; j < 3; ++j) {
      int ox = std::clamp(offs[j][0], -1, 1), oy = std::clamp(offs[j][1], -1, 1);
      int n = 1 + int(k * 1.8);
      if ((ox || oy) && k > .1) out.push_back({x + ox * n, y + oy * n, j == 0 ? ',' : MID, fade(BLOOD_RED, 1 - k)});
    }
    return out;
  }
  return out;
}

}  // namespace fx
