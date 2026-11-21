// The wide layout's frame, panel and log in the dwarven style (plan.md section 5): one bronze frame with rivets, a
// dark stone ground, three glass orbs for life, fury and mana, a sign in front of each line of the log, the place in
// the bar above the log, a red heartbeat at the map's edge when life runs low, and the suns in the headers burning.
#include <algorithm>
#include <cctype>
#include <cmath>

#include "client.hpp"
#include "data.hpp"
#include "palette.hpp"

namespace {
using namespace pal;
TCOD_ColorRGB rgb(int c) { return {uint8_t((c >> 16) & 255), uint8_t((c >> 8) & 255), uint8_t(c & 255)}; }
int mix(int a, int b, double f) {
  auto m = [&](int s) { int x = (a >> s) & 255, y = (b >> s) & 255; return std::clamp(int(x + (y - x) * f), 0, 255) << s; };
  return m(16) | m(8) | m(0);
}
int scl(int c, double f) {
  auto ch = [&](int v) { return std::clamp(int(v * f), 0, 255); };
  return (ch((c >> 16) & 255) << 16) | (ch((c >> 8) & 255) << 8) | ch(c & 255);
}
unsigned hsh(int x, int y, int s = 0) {
  unsigned v = unsigned(x) * 374761393u + unsigned(y) * 668265263u + unsigned(s) * 2246822519u;
  v = (v ^ (v >> 13)) * 1274126177u;
  return v ^ (v >> 16);
}
int ulen(const std::string& s) { int n = 0; for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n; return n; }
std::string upper(std::string s) { for (auto& c : s) c = char(std::toupper((unsigned char)c)); return s; }

// The quarter-block glyph for a 2x2 mask (bit 0 top left, 1 top right, 2 bottom left, 3 bottom right).
const int QUAD[16] = {' ', 0x2598, 0x259D, 0x2580, 0x2596, 0x258C, 0x259E, 0x259B, 0x2597, 0x259A, 0x2590, 0x259C, 0x2584, 0x2599, 0x259F, 0x2588};
int dist2(int a, int b) {
  int dr = ((a >> 16) & 255) - ((b >> 16) & 255), dg = ((a >> 8) & 255) - ((b >> 8) & 255), db = (a & 255) - (b & 255);
  return dr * dr + dg * dg + db * db;
}
// Four sub-pixel colours to one cell: the two most different colours, each pixel to the nearer, averaged.
void two_tone(const int q[4], int& ch, int& fg, int& bg) {
  int a = q[0], b = q[0], best = -1;
  for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) if (dist2(q[i], q[j]) > best) { best = dist2(q[i], q[j]); a = q[i]; b = q[j]; }
  int m = 0, sa[3] = {0, 0, 0}, sb[3] = {0, 0, 0}, na = 0, nb = 0;
  for (int i = 0; i < 4; ++i) {
    int* s = dist2(q[i], a) <= dist2(q[i], b) ? (m |= 1 << i, ++na, sa) : (++nb, sb);
    s[0] += (q[i] >> 16) & 255; s[1] += (q[i] >> 8) & 255; s[2] += q[i] & 255;
  }
  auto avg = [](int* s, int n) { return n ? ((s[0] / n) << 16) | ((s[1] / n) << 8) | (s[2] / n) : 0; };
  ch = QUAD[m]; fg = avg(sa, na); bg = nb ? avg(sb, nb) : fg;
}

// The sign in front of a line of the log, guessed from its words and colour: lore, a tip, danger, a death, loot.
std::pair<int, int> log_sign(const std::string& s, int col) {
  auto has = [&](const char* w) { return s.find(w) != std::string::npos; };
  if (s.rfind("[debug]", 0) == 0) return {' ', 0};
  if (col == 0x9CC89C) return {'?', 0x80D080};
  if (has(" dies.") || has("crumbles") || has("is slain") || has("The dark takes") || has(" falls.") || has("You die"))
    return {0x2020, 0xC8C8C8};
  if (s.rfind("You pick up", 0) == 0 || s.rfind("You find", 0) == 0 || s.rfind("You buy", 0) == 0 || has(" gold.") ||
      s.rfind("A healing potion", 0) == 0 || s.rfind("A mana potion", 0) == 0 || s.rfind("You open the chest", 0) == 0)
    return {0x2666, col};
  if (col == 0xFFD27F || col == 0xD2B48C || col == 0xE6D2A0) return {0x263C, GOLD};
  int r = (col >> 16) & 255, g = (col >> 8) & 255;
  if (col == 0xD0A0FF || col == 0xFFD060 || col == 0xC0A0E0 || (r >= 0xC0 && r - g >= 0x48)) return {'!', 0xFF6040};
  return {' ', 0};
}
}  // namespace

// The metal of the frame follows the zone; the rivets are the same metal, brighter.
int Client::frame_col() const {
  if (!snap_.is_object() || snap_.value("town", false) || snap_.value("rest", false)) return BRONZE;
  if (snap_.contains("boss") && snap_.value("zone", "") == "deep" && snap_.value("zfloor", 0) == 2) return 0x7890A8;
  static const int T[data::ZONES] = {0x8A8478, 0x7A6A5A, 0xA8703A, 0x6E7C94, 0x4E8A78, 0x8A8070, 0xB08A3A, 0x8A5A44, 0x5E5E6A, 0x7A6488};
  return T[std::clamp(snap_.value("act", 0), 0, data::ZONES - 1)];
}
int Client::rivet_col() const { return mix(frame_col(), 0xFFF0D8, .45); }

// A header in the frame: ══☼ TITLE ☼══ between two joints; the suns are kept so they can burn.
void Client::header(int x0, int x1, int y, const std::string& title, int left, int right) {
  int fc = frame_col();
  for (int x = x0; x <= x1; ++x) put(x, y, 0x2550, fc);
  if (left) put(x0, y, left, fc);
  if (right) put(x1, y, right, fc);
  if (title.empty()) return;
  int n = ulen(title) + 4, tx = (x0 + x1 + 1 - n) / 2;
  for (int i = tx - 1; i <= tx + n; ++i) put(i, y, ' ', fc);
  suns_.push_back({tx, y});
  suns_.push_back({tx + n - 1, y});
  put(tx, y, 0x263C, EMBER);
  text(tx + 2, y, title, GOLD);
  put(tx + n - 1, y, 0x263C, EMBER);
}

// A dark stone ground: a few ░ in near-black, the same every frame.
void Client::stone(int x0, int y0, int x1, int y1) {
  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x) {
      unsigned r = hsh(x, y) % 9;
      put(x, y, r < 3 ? 0x2591 : ' ', r ? STONE : 0x231C16, PANEL_BG);
    }
}

// A glass globe, 7 cells wide, filled to frac with a rippling surface (Avarenthia's orb, in quarter blocks).
void Client::orb(int x, int y, int val, int max, int liquid, const std::string& label, bool used) {
  const int w = 7, h = 7, pw = w * 2, ph = h * 2;
  double frac = used && max > 0 ? std::clamp(double(val) / max, 0.0, 1.0) : 0.0, t = now();
  double cx = (pw - 1) / 2.0, cy = (ph - 1) / 2.0, r = std::min(pw, ph) / 2.0 + .15;
  double level = ph - frac * (ph - 1.5) - .75;
  auto ins = [&](double px, double py) { double a = (px - cx) / r, b = (py - cy) / r; return a * a + b * b <= 1; };
  auto pix = [&](int px, int py) -> int {
    if (!ins(px, py)) return -1;
    double dx = (px - cx) / r, dy = (py - cy) / r, d = dx * dx + dy * dy;
    for (int a = -1; a <= 1; ++a) for (int b = -1; b <= 1; ++b)
      if (!ins(px + a, py + b)) return mix(0x46321A, 0xE6BA6E, std::clamp(.5 - .5 * (dx + dy) / 1.4, 0.0, 1.0));
    double shade = 1 - .55 * d;
    double wave = std::sin(px * .65 + t * 2.4) * .45 + std::sin(px * .35 - t * 1.6) * .3;
    int c;
    if (frac > 0 && py >= level + wave) {
      c = scl(mix(scl(liquid, 1.25), scl(liquid, .55), (py - level) / std::max(1.0, ph - level)), shade);
      if (py < level + wave + 1) c = mix(c, 0xFFF0DC, .35);
    } else c = scl(mix(0x1A141A, scl(liquid, .25), .35), shade);
    double hx = (px - (cx - r * .38)) / (r * .28), hy = (py - (cy - r * .45)) / (r * .22);
    if (hx * hx + hy * hy < 1) c = mix(c, 0xFFFFFF, .55 * (1 - (hx * hx + hy * hy)));
    return c;
  };
  for (int row = 0; row < h; ++row)
    for (int col = 0; col < w; ++col) {
      int q[4];
      bool any = false;
      for (int i = 0; i < 4; ++i) { q[i] = pix(col * 2 + i % 2, row * 2 + i / 2); if (q[i] >= 0) any = true; else q[i] = PANEL_BG; }
      if (!any) continue;
      int ch, fg, bg;
      two_tone(q, ch, fg, bg);
      put(x + col, y + row, ch, fg, bg);
    }
  if (used) orbs_.push_back({x, y, frac, liquid});
  text(x + (w - ulen(label)) / 2, y + h, label, used ? mix(liquid, 0xFFFFFF, .3) : DIM);
  std::string num = used ? std::to_string(val) + "/" + std::to_string(max) : "-";
  text(x + (w - ulen(num) + 1) / 2, y + h + 1, num, used ? ASH : DIM);
}

// The wide layout's frame round the panel and the log, with the place in the bar between map and log.
void Client::draw_frame_wide(int px, int ly) {
  int cw = con_.get_width(), chh = con_.get_height(), fc = frame_col(), rc = rivet_col();
  stone(px + 1, 1, cw - 1, chh - 1);
  stone(0, ly + 1, px, chh - 1);
  for (int y = 0; y < chh; ++y) { put(px, y, 0x2551, fc); put(cw - 1, y, 0x2551, fc); }
  header(px, cw - 1, 0, "HERO", 0x2554, 0x2557);
  std::string where;
  int frost = snap_.value("frost", 0);
  if (snap_.value("town", false)) where = "CHARNFEN";
  else if (snap_.value("rest", false)) where = "A WAYSTATION · DEPTH " + std::to_string(snap_.value("depth", 0));
  else where = upper(snap_.value("zname", std::string("the deep"))) + " · DEPTH " + std::to_string(snap_.value("depth", 0));
  if (frost > 0) where += " · FROST " + std::to_string(frost);
  header(0, px, ly, where, 0x2550, 0x2563);
  for (int x = 0; x < cw; ++x) put(x, chh - 1, 0x2550, fc);
  put(px, chh - 1, 0x2569, fc);
  put(cw - 1, chh - 1, 0x255D, fc);
  const int rv[5][2] = {{px, 0}, {cw - 1, 0}, {cw - 1, chh - 1}, {px, chh - 1}, {px, ly}};
  for (auto& p : rv) put(p[0], p[1], 0x25A0, rc);
}

void Client::draw_side(int x0, int y, int w, int h) {
  (void)y;
  auto& me = snap_["me"];
  int x = x0 + 2, right = x0 + w - 3, inner = right - x + 1;  // text from x to right
  // who you are
  text(x, 2, me["name"].get<std::string>(), GOLD);
  text(x, 3, me["clsname"].get<std::string>() + " · level " + std::to_string(me["lvl"].get<int>()), ASH);
  {
    int prev = me["prev"], next = me["next"], xp = me["xp"], n = inner - 3;
    int fill = next > prev ? std::clamp((xp - prev) * n / (next - prev), 0, n) : 0;
    text(x, 4, "XP", DIM);
    for (int i = 0; i < n; ++i) put(x + 3 + i, 4, 0x2584, i < fill ? 0xC09040 : 0x2E261C);
  }
  // the three orbs: a pool the hero does not use is an empty, dark globe
  bool fury = me.value("pool", "mana") == "fury";
  int hp = me["hp"].get<int>(), mhp = me["maxhp"], mn = me["mana"], mmn = me["maxmana"];
  orbs_.clear();
  orb(x, 6, hp, mhp, LIFE, "LIFE", true);
  orb(x + 7, 6, mn, mmn, FURY, "FURY", fury);
  orb(x + 14, 6, mn, mmn, MANA, "MANA", !fury);
  // what holds you: today's states, until the elements' stacks come
  {
    std::string st;
    auto add = [&](const char* k, const char* w2) { if (me.value(k, false)) st += std::string(st.empty() ? "" : " ") + w2; };
    add("poison", "poisoned"); add("numb", "numb"); add("shield", "shielded"); add("held", "held"); add("latched", "latched");
    add("bleeding", "bleeding"); add("branded", "branded"); add("sundered", "split"); add("dazed", "dazed");
    if (me.value("focus", 0) > 0) st += std::string(st.empty() ? "" : " ") + "focus " + std::string(me["focus"].get<int>(), '*');
    if (me.value("standfast", 0) > 0) st += std::string(st.empty() ? "" : " ") + "fast " + std::to_string(me["standfast"].get<int>());
    if (ulen(st) > inner) st = st.substr(0, inner);
    text(x, 16, st, 0x96E65A);
  }
  // skills: key, sign, name, cost in its pool's colour; a skill not yet come is dimmed with its level
  int r = 18;
  header(x0, x0 + w - 1, r++, "SKILLS", 0x2560, 0x2563);
  ++r;
  int pool_col = fury ? FURY_TEXT : MANA_TEXT;
  for (int i = 0; i < int(me["skills"].size()); ++i, ++r) {
    auto& sk = me["skills"][i];
    int lv = sk[4], cost = sk[1];
    put(x, r, '1' + i, KEY);
    if (me["lvl"].get<int>() < lv) { text(x + 4, r, sk[0].get<std::string>(), DIM); std::string a = "lvl " + std::to_string(lv); text(right + 1 - ulen(a), r, a, DIM); continue; }
    bool ok = mn >= cost;
    put(x + 2, r, 0xB7, ok ? ASH : DIM);
    text(x + 4, r, sk[0].get<std::string>(), ok ? ASH : DIM);
    std::string c = std::to_string(cost);
    text(right + 1 - ulen(c), r, c, ok ? pool_col : DIM);
  }
  ++r;
  put(x, r, 0xA1, 0xFF5040); text(x + 1, r, "×" + std::to_string(me["heal"].get<int>()), ASH); text(x + 4, r, "[q]", KEY);
  put(x + 10, r, 0xA1, fury ? FURY_TEXT : MANA_TEXT); text(x + 11, r, "×" + std::to_string(me["manap"].get<int>()), ASH); text(x + 14, r, "[w]", KEY);
  r += 2;
  // friends: one line each, no blank lines
  if (snap_["heroes"].size() > 1) {
    header(x0, x0 + w - 1, r++, "PARTY", 0x2560, 0x2563);
    for (auto& o : snap_["heroes"]) {
      if (o["pid"] == me["pid"]) continue;
      bool dead = o.value("dead", false), ready = std::find(ready_.begin(), ready_.end(), o["pid"].get<int>()) != ready_.end();
      put(x, r, '@', dead ? DIM : o["col"].get<int>());
      std::string n = o["name"].get<std::string>();
      if (ulen(n) > 9) n = n.substr(0, 9);
      text(x + 2, r, n, dead ? DIM : ASH);
      if (dead) text(x + 12, r, "fallen", DIM);
      else if (o.value("gone", false) || ready) text(x + 12, r, "at stairs", DIM);
      else {
        int bw = right - (x + 12) + 1, f = std::clamp(o["hp"].get<int>() * bw / std::max(1, o["max"].get<int>()), 0, bw);
        for (int i = 0; i < bw; ++i) put(x + 12 + i, r, 0x25AC, i < f ? 0xB03024 : 0x2E2018);
      }
      ++r;
    }
    ++r;
  }
  header(x0, x0 + w - 1, r++, "IN SIGHT", 0x2560, 0x2563);
  ++r;
  draw_in_sight(x, r, right, h - 2);
}

// Foes in sight, most urgent first: your target, then those winding up a heavy blow, then great foes, champions and
// uniques, then the rest nearest first. Unhurt ordinary foes of one kind share a line with a count; what does not fit
// is counted on the last line.
void Client::draw_in_sight(int x, int r, int right, int last) {
  struct Row { json e; int n = 1; };
  std::vector<Row> rows;
  int hx = snap_["me"]["x"], hy = snap_["me"]["y"];
  bool aiming = over_ == Over::Target;
  auto target = [&](const json& e) { return aiming && e["x"] == tx_ && e["y"] == ty_; };
  auto urgency = [&](const json& e) {
    if (target(e)) return 0;
    if (e.value("wind", false)) return 1;
    if (e.value("rank", "normal") != "normal") return 2;
    return 3;
  };
  auto tag = [&](const json& e) -> std::string {
    if (e.value("wind", false)) return "☼ heavy";
    if (e.value("spent", false)) return "spent";
    if (e.value("stun", false)) return "stunned";
    if (e.value("frozen", false)) return "frozen";
    if (e.value("pinned", false)) return "held";
    if (e.value("bleed", false)) return "bleeding";
    if (e.value("asleep", false)) return "asleep";
    if (snap_["me"]["cls"] == "cutthroat" && e.value("busy", false)) return "open";
    return "";
  };
  std::vector<json> fs;
  for (auto& e : snap_["ents"]) if (e.value("rank", "") != "npc") fs.push_back(e);
  std::stable_sort(fs.begin(), fs.end(), [&](const json& a, const json& b) {
    int ua = urgency(a), ub = urgency(b);
    if (ua != ub) return ua < ub;
    return cheb(a["x"], a["y"], hx, hy) < cheb(b["x"], b["y"], hx, hy);
  });
  for (auto& e : fs) {
    bool plain = e.value("rank", "") == "normal" && e["hp"] == e["max"] && tag(e).empty() && !target(e);
    if (plain) {
      auto it = std::find_if(rows.begin(), rows.end(), [&](const Row& w) {
        auto& o = w.e;
        return o["name"] == e["name"] && o["ch"] == e["ch"] && o.value("rank", "") == "normal" && o["hp"] == o["max"] && tag(o).empty() && !target(o);
      });
      if (it != rows.end()) { ++it->n; continue; }
    }
    rows.push_back({e, 1});
  }
  int shown = 0, n = int(rows.size());
  for (int i = 0; i < n; ++i) {
    bool fits = r + 1 <= last && (i == n - 1 || r + 3 <= last);
    if (!fits) break;
    auto& e = rows[i].e;
    if (target(e)) put(x - 1, r, 0x25BA, GOLD);
    put(x, r, e["ch"].get<int>(), e["col"].get<int>());
    std::string nm = e["name"].get<std::string>(), tg = tag(e);
    if (rows[i].n > 1) nm += " ×" + std::to_string(rows[i].n);
    int room = right - (x + 2) + 1 - (tg.empty() ? 0 : ulen(tg) + 1);
    if (ulen(nm) > room) nm = nm.substr(0, std::max(0, room));
    text(x + 2, r, nm, e.value("rank", "normal") == "normal" ? ASH : e["col"].get<int>());
    if (!tg.empty()) text(right + 1 - ulen(tg), r, tg, e.value("wind", false) ? 0xFFB070 : 0xC8B090);
    int bw = right - (x + 2) + 1, f = std::clamp(e["hp"].get<int>() * bw / std::max(1, e["max"].get<int>()), 0, bw);
    if (e["hp"].get<int>() > 0 && f == 0) f = 1;
    for (int k = 0; k < bw; ++k) put(x + 2 + k, r + 1, 0x2580, k < f ? 0xB03024 : 0x2E2018);
    r += 2;
    ++shown;
  }
  if (shown < n) {
    int more = 0;
    for (int i = shown; i < n; ++i) more += rows[i].n;
    text(x, r, "+" + std::to_string(more) + " more: [Tab] or [l]", DIM);
  }
}

// The log, newest at the bottom, each line with its sign; older lines fade.
void Client::draw_log(int x, int y, int w, int h) {
  struct L { std::string s; int col, sign, scol; };
  std::vector<L> lines;
  bool signs = layout_ == Layout::Wide;
  int tw = w - (signs ? 4 : 1);
  for (auto it = log_.rbegin(); it != log_.rend() && int(lines.size()) < h; ++it) {
    std::vector<std::string> parts;
    std::string cur, word;
    std::string s = it->first + " ";
    for (char c : s) {
      if (c == ' ') {
        if (ulen(cur) + ulen(word) + (cur.empty() ? 0 : 1) > tw && !cur.empty()) { parts.push_back(cur); cur.clear(); }
        cur += (cur.empty() ? "" : " ") + word;
        word.clear();
      } else word += c;
    }
    if (!cur.empty()) parts.push_back(cur);
    auto [sg, sc] = signs ? log_sign(it->first, it->second) : std::pair<int, int>{' ', 0};
    for (int p = int(parts.size()) - 1; p >= 0; --p) lines.push_back({parts[p], it->second, p == 0 ? sg : ' ', sc});
  }
  int n = std::min(h, int(lines.size()));
  for (int i = 0; i < n; ++i) {
    auto& l = lines[i];
    double f = i < 3 ? 1.0 : .55;
    int row = y + h - 1 - i;
    if (signs) {
      if (l.sign != ' ') put(x + 1, row, l.sign, scl(l.scol, f));
      text(x + 3, row, l.s, scl(l.col, f));
    } else text(x + 1, row, l.s, scl(l.col, f));
  }
}

// Low life: the map's edges pulse red like a heartbeat, two beats and a rest. Only a tint, nothing drawn over the map.
void Client::low_life_edge() {
  auto& me = snap_["me"];
  int hp = me["hp"].get<int>(), mhp = std::max(1, me["maxhp"].get<int>());
  if (me.value("dead", false) || hp * 10 > mhp * 3) return;
  double ph = std::fmod(now(), 1.2) / 1.2;
  double beat = std::exp(-std::pow((ph - .05) / .05, 2)) + .7 * std::exp(-std::pow((ph - .22) / .05, 2));
  double strength = .55 + .45 * beat, deep = 1.0 - double(hp) / mhp / .3;  // redder the lower it gets
  strength *= .6 + .4 * std::clamp(deep, 0.0, 1.0);
  for (int Y = vy_; Y < vy_ + vh_; ++Y)
    for (int X = vx_; X < vx_ + vw_; ++X) {
      double d = std::min({X - vx_, Y - vy_, vx_ + vw_ - 1 - X, vy_ + vh_ - 1 - Y}) + (hsh(X, Y, 5) % 100) / 100.0 * 1.2;
      double k = std::max(0.0, 1 - d / 5.0) * strength;
      if (k <= 0 || !con_.in_bounds({X, Y})) continue;
      auto& t = con_.at({X, Y});
      auto red = [&](TCOD_ColorRGBA& c) {
        int v = (c.r << 16) | (c.g << 8) | c.b;
        int lum = (c.r + c.g + c.b) / 3;
        int to = (std::min(255, lum / 2 + 110) << 16) | ((lum / 5 + 8) << 8) | (lum / 5 + 6);
        int m = mix(v, to, std::min(.8, k));
        c.r = uint8_t(m >> 16); c.g = uint8_t(m >> 8); c.b = uint8_t(m);
      };
      red(t.fg);
      red(t.bg);
    }
}

// The suns in the headers flicker like small flames.
void Client::burn_suns() {
  double t = now();
  for (size_t k = 0; k < suns_.size(); ++k) {
    auto [x, y] = suns_[k];
    if (!con_.in_bounds({x, y}) || con_.at({x, y}).ch != 0x263C) continue;
    double v = .5 + .25 * std::sin(t * 9 + k * 1.7) + .25 * std::sin(t * 23 + k * 3.1);
    int c = mix(0xC84614, 0xFFE678, std::clamp(v, 0.0, 1.0));
    con_.at({x, y}).fg = {uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c), 255};
  }
}

// Particles over the panel: light bubbles rising in each orb, and now and then a spark off a sun.
void Client::draw_ui_particles(SDL_Renderer* r, double ox, double oy, double cell) {
  if (layout_ != Layout::Wide || screen_ != Screen::Game || !snap_.is_object()) return;
  if (over_ != Over::None && over_ != Over::Target) return;
  double t = now(), px = cell / 8;
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  auto dot = [&](double x, double y, int c, double a) {  // x, y in font pixels of the console
    SDL_SetRenderDrawColor(r, (c >> 16) & 255, (c >> 8) & 255, c & 255, Uint8(std::clamp(a, 0.0, 1.0) * 255));
    SDL_FRect rc{float(ox + std::floor(x) * px), float(oy + std::floor(y) * px), float(px), float(px)};
    SDL_RenderFillRect(r, &rc);
  };
  for (size_t o = 0; o < orbs_.size(); ++o) {
    auto& ob = orbs_[o];
    double cx = ob.x * 8 + 28, cy = ob.y * 8 + 28, R = 22, surf = cy + R - ob.frac * 2 * R, bottom = cy + R - 3;
    if (surf >= bottom) continue;
    int light = mix(ob.col, 0xFFFFFF, .55);
    for (int k = 0; k < 9; ++k) {
      double ph = std::fmod(t * (.35 + .1 * (hsh(k, int(o), 1) % 4)) + (hsh(k * 7 + int(o), 7, 3) % 1000) / 1000.0, 1.0);
      double bx = cx - R * .7 + (hsh(k, int(o), 2) % 100) / 100.0 * R * 1.4 + 1.5 * std::sin(t * 4 + k);
      double by = bottom - ph * (bottom - surf - 1);
      if ((bx - cx) * (bx - cx) + (by - cy) * (by - cy) > (R - 2) * (R - 2)) continue;
      dot(bx, by, mix(light, ob.col, ph * .5), .9 - ph * .4);
    }
  }
  for (size_t k = 0; k < suns_.size(); ++k) {
    auto [x, y] = suns_[k];
    for (int j = 0; j < 2; ++j) {
      double ph = std::fmod(t * (.6 + .25 * j) + (hsh(int(k), j) % 100) / 100.0, 1.0);
      if (ph > .7) continue;  // most of the time a sun throws nothing
      double ex = x * 8 + 4 + (int(hsh(int(k), j, 3) % 7) - 3) + std::sin(t * 6 + j + k) * 2, ey = y * 8 + 2 - ph * 14;
      if (ey < 0) continue;
      dot(ex, ey, mix(0xFFDC82, 0x6E280A, ph / .7), 1 - ph / .7);
    }
  }
}
