// Windows as objects (plan.md section 5): bronze for everything of your hero (the main menu, the Hero screen, the
// options, the handbook), leather for the inventory, a wooden stall for the trader, stone for what is written in the
// world. Keys are drawn as gold keycaps, element names in their colour.
#include <algorithm>
#include <cmath>

#include "client.hpp"
#include "data.hpp"
#include "elements.hpp"
#include "handbook.hpp"
#include "palette.hpp"

namespace {
using namespace pal;
int mix(int a, int b, double f) {
  auto m = [&](int s) { int x = (a >> s) & 255, y = (b >> s) & 255; return std::clamp(int(x + (y - x) * f), 0, 255) << s; };
  return m(16) | m(8) | m(0);
}
std::string clip(const std::string& s, int n) { return int(s.size()) <= n ? s : s.substr(0, std::max(0, n - 1)) + "\u2026"; }
int ulen(const std::string& s) { int n = 0; for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n; return n; }
const int LEATHER = 0x7A4E2C, LEATHER_BG = 0x1C130C, STITCH = 0xC8A070, WOOD = 0x8A6438, WOOD_BG = 0x18110B;
const int STONE_EDGE = 0x8A8478, STONE_BG = 0x2A2826, CAP_BG = 0xE0B050, CAP_FG = 0x1A1208;

struct MenuItem { const char* name; const char* key; const char* sub; };
const MenuItem MENU[] = {
    {"Back to the dive", "Esc", "the fire still burns"}, {"Hero", "c", "level, stats, resistances, perks"},
    {"Inventory", "i", "what you wear and carry"},       {"Handbook", "?", "how everything works"},
    {"Options", "o", "music, sound, numbers"},           {nullptr, nullptr, nullptr},
    {"Debug and testing", "F12", "tests, bug reports, tools"}, {"Save and quit", "q", "your dive waits for you"},
};
const int MENU_N = int(std::size(MENU));
}  // namespace

// Everything already drawn sinks back, so a window stands out over the game.
void Client::dim_all(double f) {
  for (int y = 0; y < con_.get_height(); ++y)
    for (int x = 0; x < con_.get_width(); ++x) {
      auto& t = con_.at({x, y});
      t.fg = {uint8_t(t.fg.r * f), uint8_t(t.fg.g * f), uint8_t(t.fg.b * f), 255};
      t.bg = {uint8_t(t.bg.r * f), uint8_t(t.bg.g * f), uint8_t(t.bg.b * f), 255};
    }
}

void Client::window(int x, int y, int w, int h, const std::string& title, Mat m) {
  int bg = m == Mat::Leather ? LEATHER_BG : m == Mat::Wood ? WOOD_BG : m == Mat::Stone ? STONE_BG : PAPER;
  win_bg_ = bg;
  for (int j = y; j < y + h; ++j) for (int i = x; i < x + w; ++i) put(i, j, ' ', INK, bg);
  int eh = 0x2550, ev = 0x2551, ec = BRONZE, corner = 0x25A0, cc = RIVET;
  if (m == Mat::Leather) { eh = ev = 0xB7; ec = STITCH; cc = 0x8A5A30; }
  if (m == Mat::Wood) { ec = WOOD; corner = 0x256C; cc = 0x5A3E22; }
  if (m == Mat::Stone) { eh = 0x2580; ev = 0x2588; ec = STONE_EDGE; corner = 0x2584; cc = 0xB0A898; }
  for (int i = x; i < x + w; ++i) { put(i, y, eh, ec, bg); put(i, y + h - 1, m == Mat::Stone ? 0x2584 : eh, ec, bg); }
  for (int j = y; j < y + h; ++j) { put(x, j, ev, ec, bg); put(x + w - 1, j, ev, ec, bg); }
  for (auto [cx, cy] : {std::pair<int, int>{x, y}, {x + w - 1, y}, {x, y + h - 1}, {x + w - 1, y + h - 1}}) put(cx, cy, corner, cc, bg);
  if (m == Mat::Leather) {  // leather strips inside the sides, a brass buckle at the foot
    for (int j = y + 1; j < y + h - 1; ++j) { put(x + 1, j, 0x258C, LEATHER, bg); put(x + w - 2, j, 0x2590, LEATHER, bg); }
    text(x + w / 2 - 1, y + h - 1, "▄█▄", 0xD8B040, bg);
  }
  if (m == Mat::Wood) {  // a striped awning over the stall
    for (int i = x - 1; i <= x + w; ++i) {
      int c = ((i - x + 1) / 3) % 2 ? 0xB03024 : 0xE0D0B0;
      put(i, y - 2, 0x2584, c, 0);
      put(i, y - 1, (i - x + 1) % 3 == 1 ? 0x25BC : 0x2580, c, 0);
    }
  }
  if (title.empty()) return;
  std::string t = title;
  for (auto& c : t) c = char(std::toupper((unsigned char)c));
  if (m == Mat::Bronze) {
    int n = ulen(t) + 4, tx = x + (w - n) / 2;
    for (int i = tx - 1; i <= tx + n; ++i) put(i, y, ' ', ec, bg);
    put(tx, y, 0x263C, EMBER, bg);
    text(tx + 2, y, t, GOLD, bg);
    put(tx + n - 1, y, 0x263C, EMBER, bg);
  } else text(x + (w - ulen(t) - 2) / 2, y, " " + t + " ", GOLD, bg);
}

void Client::keycap(int x, int y, const std::string& k) { text(x, y, " " + k + " ", CAP_FG, CAP_BG); }

// Text where [k] is a gold keycap and {Ember} (or a sign's name) is drawn in its colour. Returns the width used.
int Client::rich(int x, int y, const std::string& s, int col, int bg) {
  static const std::vector<std::pair<std::string, std::pair<std::string, int>>> WORDS = {
      {"Ember", {"Ember", 0xFF8A30}}, {"Rime", {"Rime", 0xA8D8FF}}, {"Ash", {"Ash", 0xA8A8B8}}, {"Rot", {"Rot", 0x9CC850}},
      {"Blood", {"Blood", 0xD8403A}}, {"sun", {"☼", GOLD}}, {"?", {"?", 0x80D080}}, {"!", {"!", 0xFF6040}},
      {"dagger", {"†", 0xC8C8C8}}, {"loot", {"♦", 0x6090FF}}};
  int cx = x;
  for (size_t i = 0; i < s.size();) {
    if (s[i] == '[') {
      size_t j = s.find(']', i);
      if (j != std::string::npos) {
        std::string k = s.substr(i + 1, j - i - 1);
        keycap(cx, y, k);
        cx += ulen(k) + 2;
        i = j + 1;
        continue;
      }
    }
    if (s[i] == '{') {
      size_t j = s.find('}', i);
      if (j != std::string::npos) {
        std::string k = s.substr(i + 1, j - i - 1);
        for (auto& [w, v] : WORDS) if (w == k) { text(cx, y, v.first, v.second, bg); cx += ulen(v.first); }
        i = j + 1;
        continue;
      }
    }
    size_t j = i + 1;
    while (j < s.size() && (s[j] & 0xC0) == 0x80) ++j;
    text(cx, y, s.substr(i, j - i), col, bg);
    ++cx;
    i = j;
  }
  return cx - x;
}
// The same, wrapped to w. Returns the lines used.
int Client::rich_wrap(int x, int y, int w, const std::string& s, int col, int bg, bool draw) {
  auto vis = [](const std::string& wd) {  // a word's width as drawn
    int n = 0;
    for (size_t i = 0; i < wd.size(); ++i) {
      if (wd[i] == '[') { size_t j = wd.find(']', i); if (j != std::string::npos) { n += ulen(wd.substr(i + 1, j - i - 1)) + 2; i = j; continue; } }
      if (wd[i] == '{') { size_t j = wd.find('}', i); if (j != std::string::npos) { std::string k = wd.substr(i + 1, j - i - 1); n += k == "sun" || k == "dagger" || k == "loot" ? 1 : ulen(k); i = j; continue; } }
      if ((wd[i] & 0xC0) != 0x80) ++n;
    }
    return n;
  };
  std::vector<std::string> words;
  std::string cur;
  for (char c : s) { if (c == ' ') { words.push_back(cur); cur.clear(); } else cur += c; }
  words.push_back(cur);
  std::vector<std::string> lines(1);
  int col_at = 0;
  for (auto& wd : words) {
    int l = vis(wd);
    if (col_at > 0 && col_at + 1 + l > w) { lines.push_back(""); col_at = 0; }
    if (col_at > 0) { lines.back() += ' '; ++col_at; }
    lines.back() += wd;
    col_at += l;
  }
  if (draw) for (size_t i = 0; i < lines.size(); ++i) rich(x, y + int(i), lines[i], col, bg);
  return int(lines.size());
}

// ------------------------------------------------------------------ the main menu
void Client::draw_menu() {
  int cw = con_.get_width(), chh = con_.get_height();
  int w = std::min(cw, 40), h = std::min(chh, 30);
  int x = (cw - w) / 2, y = (chh - h) / 2;
  window(x, y, w, h, "Pyreheart");
  int r = y + 2;
  for (int i = 0; i < MENU_N; ++i) {
    auto& it = MENU[i];
    if (!it.name) { r += 1; continue; }
    bool on = i == menu_sel_;
    int bg = on ? SEL : PAPER;
    for (int k = x + 1; k < x + w - 1; ++k) { put(k, r, ' ', INK, bg); put(k, r + 1, ' ', INK, bg); }
    if (on) put(x + 2, r, 0x25BA, GOLD, bg);
    text(x + 4, r, it.name, on ? GOLD : ASH, bg);
    keycap(x + w - 3 - ulen(it.key) - 2, r, it.key);
    text(x + 4, r + 1, it.sub, DIM, bg);
    buttons_.push_back({x + 1, r, w - 2, 2, "", "menu:" + std::to_string(i)});
    r += 3;
  }
  rich(x + 2, y + h - 2, "[↑↓] choose  [Enter] open", INK, PAPER);
  std::string v = std::string(" v") + PYREHEART_BUILD + " ";
  text(x + w - 2 - ulen(v), y + h - 1, v, 0x7A6A58, PAPER);
}

void Client::menu_do(int i) {
  if (i < 0 || i >= MENU_N || !MENU[i].name) return;
  std::string k = MENU[i].key;
  if (k == "Esc") over_ = Over::None;
  else if (k == "c") over_ = Over::Hero;
  else if (k == "i") { over_ = Over::Pack; sel_ = 0; }
  else if (k == "?") open_book();
  else if (k == "o") { over_ = Over::Options; opt_sel_ = 0; }
  else if (k == "F12") open_debug();
  else if (k == "q") { link_.reset(); host_.reset(); screen_ = Screen::Title; over_ = Over::None; menu_ = 0; snap_ = json(); }
}

void Client::menu_key(SDL_Keycode k) {
  auto step = [&](int d) { do menu_sel_ = (menu_sel_ + d + MENU_N) % MENU_N; while (!MENU[menu_sel_].name); };
  if (k == SDLK_ESCAPE) over_ = Over::None;
  else if (k == SDLK_UP || k == SDLK_KP_8) step(-1);
  else if (k == SDLK_DOWN || k == SDLK_KP_2) step(1);
  else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) menu_do(menu_sel_);
  else if (k == SDLK_C) menu_do(1);
  else if (k == SDLK_I) menu_do(2);
  else if (k == SDLK_SLASH || k == SDLK_QUESTION || k == SDLK_F1) menu_do(3);
  else if (k == SDLK_O) menu_do(4);
  else if (k == SDLK_D || k == SDLK_F12) menu_do(6);
  else if (k == SDLK_Q) menu_do(7);
}

// ------------------------------------------------------------------ the Hero screen
void Client::draw_hero() {
  auto& me = snap_["me"];
  int cw = con_.get_width(), chh = con_.get_height();
  int w = std::min(cw, 74), h = std::min(chh, 32);
  int x = (cw - w) / 2, y = (chh - h) / 2, half = (w - 1) / 2;
  window(x, y, w, h, me["name"].get<std::string>());
  for (int j = y + 1; j < y + h - 1; ++j) put(x + half, j, 0x2502, 0x5A4028, PAPER);
  put(x + half, y + h - 1, 0x2567, BRONZE, PAPER);
  int L = x + 2, R = x + half + 2, W2 = half - 3;
  auto head = [&](int hx, int hy, const std::string& t) { text(hx, hy, "══ " + t + " ══", BRONZE, PAPER); text(hx + 3, hy, t, GOLD, PAPER); };
  auto bar = [&](int bx, int by, int n, double f, int col) { for (int i = 0; i < n; ++i) put(bx + i, by, 0x2584, i < std::lround(n * f) ? col : 0x3A2E22, PAPER); };
  auto right_val = [&](int ry, const std::string& name, const std::string& v, int nc, int vc) { text(R, ry, name, nc, PAPER); text(R + W2 - ulen(v), ry, v, vc, PAPER); };
  // left: who you are and how you grow: the title, the level, the three ways of fighting, the weapon, the element
  int r = y + 2;
  text(L, r++, me["clsname"].get<std::string>(), me["col"].get<int>(), PAPER);
  if (int c = me.value("combo", -1); c >= 0) r += rich_wrap(L, r, W2, std::string("Talent: ") + data::TALENTS[c / 5][c % 5] + ".", 0xB89868, PAPER);
  int prev = me["prev"], next = me["next"], xp = me["xp"];
  text(L, r, "Level " + std::to_string(me["lvl"].get<int>()), ASH, PAPER);
  text(L + 10, r, "XP", DIM, PAPER);
  bar(L + 13, r++, W2 - 13, next > prev ? double(xp - prev) / (next - prev) : 0, 0xC09040);
  int frost = snap_.value("frost", 0);
  ++r;
  if (int sc = me.value("scars", 0)) {
    text(L, r++, "Scarred " + std::to_string(sc) + (sc == 1 ? " time" : " times") + ": " + std::to_string(std::min(70, 10 * sc)) + "% less life", 0xC08070, PAPER);
    ++r;
  }
  text(L, r++, (frost ? "Frost " + std::to_string(frost) : std::string("Normal")) + ", depth " + std::to_string(snap_.value("depth", 0)) + ", " +
                   std::to_string(me.value("kills", 0)) + " slain", DIM, PAPER);
  ++r;
  head(L, r++, "Styles");
  for (auto& st : me.value("styles", json::array())) {
    int rank = st[1], sx = st[2], sp = st[3], sn = st[4], nxt = st[5], col = st[6];
    text(L, r, st[0].get<std::string>(), rank > 0 ? col : DIM, PAPER);
    text(L + 7, r, std::to_string(rank), rank > 0 ? ASH : DIM, PAPER);
    bar(L + 10, r, W2 - 20, sn > sp ? double(sx - sp) / (sn - sp) : 0, rank > 0 ? col : 0x5A5048);
    if (nxt > 0) { std::string ns = "skill " + std::to_string(nxt); text(L + W2 - ulen(ns), r, ns, 0x7A7068, PAPER); }
    r += 2;
  }
  if (me["weapon"].is_object()) {
    auto& wp = me["weapon"];
    ++r;
    head(L, r++, "Weapon");
    int m = wp.value("mastery", 0), wx = wp.value("xp", 0), wp0 = wp.value("prev", 0), wn = wp.value("next", 0);
    text(L, r, wp["name"].get<std::string>(), ASH, PAPER);
    std::string ms = "mastery " + std::to_string(m) + "/5";
    text(L + W2 - ulen(ms), r++, ms, INK, PAPER);
    if (wn > wp0) bar(L, r++, W2, double(wx - wp0) / (wn - wp0), 0xC0A060);
    ++r;
    r += rich_wrap(L, r, W2, wp["trait"].get<std::string>(), DIM, PAPER);
  }
  int el = me.value("el", -1);
  if (el >= 0) {
    ++r;
    head(L, r++, "Element");
    for (int e : {el, me.value("el2", -1)}) {
      if (e < 0) continue;
      auto& er = me["eranks"][e];
      int rk = er[0], ex = er[1], e0 = er[2], e1 = er[3];
      text(L, r, elem::NAME[e], elem::COL[e], PAPER);
      text(L + 7, r, std::to_string(rk), ASH, PAPER);
      bar(L + 10, r++, W2 - 10, e1 > e0 ? double(ex - e0) / (e1 - e0) : 0, elem::COL[e]);
    }
  }
  // right: what you can take
  r = y + 2;
  right_val(r++, "Life", std::to_string(me["maxhp"].get<int>()), 0xE06050, ASH);
  right_val(r++, "Fury", std::to_string(me.value("maxfury", 10)), FURY_TEXT, ASH);
  right_val(r++, "Mana", std::to_string(me["maxmana"].get<int>()), MANA_TEXT, ASH);
  ++r;
  right_val(r++, "Damage", me["dmg"].get<std::string>(), INK, ASH);
  right_val(r++, "Armour", std::to_string(me["armour"].get<int>()), INK, ASH);
  right_val(r++, "Accuracy", std::to_string(me.value("acc", 0)) + "%", INK, ASH);
  right_val(r++, "Critical", std::to_string(me.value("crit", 0)) + "%", INK, ASH);
  ++r;
  head(R, r++, "Resist");
  for (int i = 0; i < elem::N; ++i) {
    int v = me.contains("eres") && me["eres"].size() > size_t(i) ? me["eres"][i].get<int>() : 0;
    put(R, r, 0x25A0, elem::COL[i], PAPER);
    text(R + 2, r, elem::NAME[i], elem::COL[i], PAPER);
    std::string vs = std::to_string(v) + "%";
    text(R + W2 - ulen(vs), r++, vs, v < 0 ? 0xE05040 : ASH, PAPER);
  }
  if (frost) text(R, r++, "Frost takes " + std::to_string(10 * std::min(frost, 5)) + "% from each", 0x8AB8E0, PAPER);
  ++r;
  head(R, r++, "Perks");
  if (me["perks"].empty()) text(R, r++, "none yet", DIM, PAPER);
  for (auto& p : me["perks"]) {
    if (r >= y + h - 3) break;
    int rank = p[1];
    text(R, r, p[0].get<std::string>(), ASH, PAPER);
    if (rank > 1) text(R + W2 - 2, r, "x" + std::to_string(rank), INK, PAPER);
    ++r;
  }
  if (me.value("known", json::array()).size() > 4 && r < y + h - 4) {
    ++r;
    head(R, r++, "Skills known");
    std::string all;
    for (auto& k : me["known"]) all += (all.empty() ? "" : ", ") + k["name"].get<std::string>();
    r += rich_wrap(R, r, W2, all, DIM, PAPER);
  }
  rich(x + w - 13, y + h - 2, "[Esc] back", INK, PAPER);
}

// ------------------------------------------------------------------ options
void Client::apply_volume() { music_.volume(music_on_ ? music_vol_ / 10.0f : 0); }

void Client::draw_options() {
  int cw = con_.get_width(), chh = con_.get_height();
  int w = std::min(cw, 46), h = 19;
  int x = (cw - w) / 2, y = (chh - h) / 2;
  window(x, y, w, h, "Options");
  struct Row { const char* name; std::string val; int level; const char* note; };
  Row rows[] = {{"Music volume", "", music_on_ ? music_vol_ : 0, music_on_ ? "" : "music is off on the title screen"},
                {"Sound volume", "", sound_vol_, "no sounds yet; they come with the lab"},
                {"Floating numbers", numbers_on_ ? "on" : "off", -1, "the harm dealt, rising over the map"},
                {"Music", music_lab_ ? "lab" : "classic", -1, music_lab_ ? "the music lab: new ideas to test" : "the music as it always was"}};
  int r = y + 2;
  for (int i = 0; i < 4; ++i) {
    bool on = i == opt_sel_;
    int bg = on ? SEL : PAPER;
    for (int k = x + 1; k < x + w - 1; ++k) { put(k, r, ' ', INK, bg); put(k, r + 1, ' ', INK, bg); }
    if (on) put(x + 2, r, 0x25BA, GOLD, bg);
    text(x + 4, r, rows[i].name, on ? GOLD : ASH, bg);
    int vx = x + 24;
    if (rows[i].level >= 0) {
      put(vx, r, 0x25C4, on ? KEY : DIM, bg);
      for (int k = 0; k < 10; ++k) put(vx + 2 + k, r, 0x25AC, k < rows[i].level ? EMBER : 0x3A2E22, bg);
      put(vx + 13, r, 0x25BA, on ? KEY : DIM, bg);
    } else text(vx + 2, r, rows[i].val, ASH, bg);
    text(x + 4, r + 1, rows[i].note, DIM, bg);
    buttons_.push_back({x + 1, r, w - 2, 2, "", "opt:" + std::to_string(i)});
    r += 3;
  }
  rich(x + 2, y + h - 2, "[↑↓] choose  [◄►] change  [Esc] back", INK, PAPER);
}

void Client::options_key(SDL_Keycode k) {
  int d = k == SDLK_LEFT || k == SDLK_KP_4 ? -1 : k == SDLK_RIGHT || k == SDLK_KP_6 || k == SDLK_RETURN || k == SDLK_KP_ENTER ? 1 : 0;
  if (k == SDLK_ESCAPE) { over_ = snap_.is_object() && screen_ == Screen::Game ? Over::Menu : Over::None; return; }
  if (k == SDLK_UP || k == SDLK_KP_8) { opt_sel_ = (opt_sel_ + 3) % 4; return; }
  if (k == SDLK_DOWN || k == SDLK_KP_2) { opt_sel_ = (opt_sel_ + 1) % 4; return; }
  if (!d) return;
  if (opt_sel_ == 0) {
    if (!music_on_ && d > 0) { music_on_ = true; music_vol_ = std::max(1, music_vol_); }
    else music_vol_ = std::clamp(music_vol_ + d, 0, 10);
    apply_volume();
  } else if (opt_sel_ == 1) sound_vol_ = std::clamp(sound_vol_ + d, 0, 10);
  else if (opt_sel_ == 2) numbers_on_ = !numbers_on_;
  else { music_lab_ = !music_lab_; music_.use_lab(music_lab_); }
  save_settings();
}

// ------------------------------------------------------------------ the Delver's Handbook
// It opens at the page that fits: a tip just given, a fight, a camp, Charnfen; else the page read last.
void Client::open_book() {
  int ch = book_last_;
  book_why_.clear();
  bool in_game = screen_ == Screen::Game && snap_.is_object();
  if (in_game && tip_ch_ >= 0 && now() - tip_t_ < 30) { ch = tip_ch_; book_why_ = "for the tip you were just given."; }
  else if (in_game && !foes().empty()) { ch = book::index_of("fighting"); book_why_ = "you are in a fight."; }
  else if (in_game && snap_.value("town", false)) { ch = book::index_of("first"); book_why_ = "you are in Charnfen."; }
  else if (in_game && snap_.value("camp", false)) { ch = book::index_of("rest"); book_why_ = "you are at a camp."; }
  else if (!in_game && book_last_ == 0) book_why_ = "it is the beginning.";
  book_ch_ = ch;
  book_scroll_ = 0;
  over_ = Over::Book;
}

void Client::draw_book() {
  int cw = con_.get_width(), chh = con_.get_height();
  int w = std::min(cw - 4, 90), h = std::min(chh - 4, 44);
  int x = (cw - w) / 2, y = (chh - h) / 2, toc = std::min(22, w / 3);
  window(x, y, w, h, "The Delver's Handbook");
  for (int j = y + 1; j < y + h - 1; ++j) put(x + toc, j, 0x2502, 0x5A4028, PAPER);
  put(x + toc, y, 0x2564, BRONZE, PAPER);
  put(x + toc, y + h - 1, 0x2567, BRONZE, PAPER);
  auto& C = book::chapters();
  int r = y + 2;
  for (size_t i = 0; i < C.size(); ++i) {
    bool on = int(i) == book_ch_;
    if (on) for (int k = x + 1; k < x + toc; ++k) put(k, r, ' ', INK, SEL);
    if (on) put(x + 2, r, 0x25BA, GOLD, SEL);
    text(x + 4, r, C[i].title, on ? GOLD : INK, on ? SEL : PAPER);
    buttons_.push_back({x + 1, r, toc - 1, 1, "", "book:" + std::to_string(i)});
    ++r;
    if (on)
      for (auto& b : C[i].blocks)
        if (b.kind == 'h') { text(x + 6, r++, b.text, 0x7A7068, PAPER); }
  }
  if (!book_why_.empty()) {
    rich_wrap(x + 2, y + h - 4, toc - 3, "Opened here because " + book_why_, DIM, PAPER);
  }
  // the page
  auto& ch = C[std::clamp(book_ch_, 0, int(C.size()) - 1)];
  int px = x + toc + 3, pw = w - toc - 6, top = y + 2, bottom = y + h - 3;
  std::string t = ch.title;
  for (auto& c : t) c = char(std::toupper((unsigned char)c));
  // lay the page out first, so it can scroll
  struct Line { int kind; std::string s; };
  std::vector<std::pair<char, std::string>> blocks;
  for (auto& b : ch.blocks) blocks.push_back({b.kind, b.text});
  if (ch.key == "keys") for (auto& [k, v] : book::KEYS) blocks.push_back({'k', k + "\t" + v});
  if (ch.key == "elements" && screen_ == Screen::Game && snap_.is_object() && snap_.contains("zel")) {
    auto& z = snap_["zel"];
    auto nm = [&](int i) { return "{" + std::string(elem::NAME[z[i].get<int>()]) + "}"; };
    blocks.push_back({'h', "Here, in " + snap_.value("zname", std::string("this zone"))});
    blocks.push_back({'p', "The foes here strike with " + nm(0) + " and " + nm(1) + ". They resist " + nm(2) + " and fear " + nm(3) + "."});
  }
  int total = 2;
  for (auto& [k, s] : blocks) total += k == 'h' ? 1 : k == 'k' ? 1 : k == 'p' ? rich_wrap(0, 0, pw, s, INK, PAPER, false) + 1 : 1;
  int room = bottom - top - 1;
  book_scroll_ = std::clamp(book_scroll_, 0, std::max(0, total - room));
  int yy = top - book_scroll_;
  auto vis = [&](int row) { return row >= top && row < bottom; };
  if (vis(yy)) text(px, yy, t, GOLD, PAPER);
  if (vis(yy + 1)) for (int i = 0; i < ulen(t); ++i) put(px + i, yy + 1, 0x2500, 0x806030, PAPER);
  yy += 3;
  for (auto& [k, s] : blocks) {
    if (k == 'h') { if (vis(yy)) text(px, yy, s, GOLD, PAPER); ++yy; }
    else if (k == 'k') {
      auto tab = s.find('\t');
      if (vis(yy)) { rich(px, yy, s.substr(0, tab), INK, PAPER); text(px + 30, yy, s.substr(tab + 1), INK, PAPER); }
      ++yy;
    } else if (k == 'p') {
      int n = rich_wrap(0, 0, pw, s, INK, PAPER, false);
      bool all = vis(yy) && vis(yy + n - 1);
      if (all) rich_wrap(px, yy, pw, s, INK, PAPER);
      yy += n + 1;
    } else ++yy;
  }
  if (total > room) text(x + w - 4, bottom - 1, book_scroll_ + room < total ? "▼" : " ", DIM, PAPER);
  std::string foot = "[↑↓] chapter  [PgUp] [PgDn] scroll  [Esc] close";
  rich(x + toc + 3, y + h - 2, foot, INK, PAPER);
}

void Client::book_key(SDL_Keycode k) {
  int n = int(book::chapters().size());
  if (k == SDLK_ESCAPE || k == SDLK_SLASH || k == SDLK_QUESTION || k == SDLK_F1) {
    book_last_ = book_ch_;
    over_ = Over::None;
    return;
  }
  if (k == SDLK_UP || k == SDLK_KP_8) { book_ch_ = (book_ch_ + n - 1) % n; book_scroll_ = 0; book_why_.clear(); }
  else if (k == SDLK_DOWN || k == SDLK_KP_2) { book_ch_ = (book_ch_ + 1) % n; book_scroll_ = 0; book_why_.clear(); }
  else if (k == SDLK_PAGEDOWN || k == SDLK_RIGHT || k == SDLK_SPACE) book_scroll_ += 8;
  else if (k == SDLK_PAGEUP || k == SDLK_LEFT) book_scroll_ = std::max(0, book_scroll_ - 8);
}

// A tip in the log points the handbook at its page for a while.
void Client::note_tip(const std::string& s) {
  static const std::vector<std::pair<const char*, const char*>> MAP = {
      {"pack", "gear"}, {"▲", "gear"}, {"wear", "gear"}, {"stairs", "moving"}, {"Walk", "moving"}, {"shield", "fighting"},
      {"foe", "fighting"}, {"perk", "hero"}, {"vault", "gear"}, {"Spark", "frost"}, {"great foe", "foes"}};
  for (auto& [w, c] : MAP)
    if (s.find(w) != std::string::npos) { tip_ch_ = book::index_of(c); tip_t_ = now(); return; }
}

// ------------------------------------------------------------------ the coals at the great fire

// Five coals side by side, each a glowing heap with what its element does; the one you take burns in your hits.
void Client::draw_coal_window() {
  int cw = con_.get_width(), chh = con_.get_height();
  int col_w = 18, w = std::min(cw, col_w * elem::N + 5), h = std::min(chh, 27);
  int x = (cw - w) / 2, y = (chh - h) / 2;
  window(x, y, w, h, "A coal from the great fire");
  text(x + 3, y + 2, "Take one coal down with you. Your hits lay its element on the foe.", INK, PAPER);
  double t = now();
  coal_sel_ = std::clamp(coal_sel_, 0, elem::N - 1);
  for (int e = 0; e < elem::N; ++e) {
    int cx = x + 3 + e * col_w, mid = cx + col_w / 2 - 1;
    bool on = e == coal_sel_;
    int c = elem::COL[e], bg = on ? mix(PAPER, c, .12) : PAPER;
    for (int j = y + 4; j < y + h - 4; ++j) for (int i = cx; i < cx + col_w - 1; ++i) put(i, j, ' ', INK, bg);
    if (on) { for (int i = cx; i < cx + col_w - 1; ++i) { put(i, y + 4, 0x2580, mix(bg, c, .6), PAPER); put(i, y + h - 5, 0x2584, mix(bg, c, .6), PAPER); } }
    // the heap: a glowing core under a flicker
    double f = .7 + .3 * std::sin(t * (on ? 5.0 : 2.4) + e * 1.3);
    int core = mix(0x201810, c, f), rim = mix(0x201810, c, f * .55);
    put(mid, y + 6, std::fmod(t * 3 + e, 1.0) < .5 ? 0x2219 : 0xB7, mix(bg, c, f * (on ? 1 : .5)), bg);
    text(mid - 1, y + 7, "▄█▄", core, bg);
    text(mid - 2, y + 8, "▀▀▀▀▀", rim, bg);
    put(mid, y + 7, 0x25AA, 0xFFF0D0, core);
    std::string name = elem::NAME[e], role = elem::ROLE[e];
    text(cx + (col_w - 1 - ulen(name)) / 2, y + 10, name, on ? c : mix(c, PAPER, .3), bg);
    text(cx + (col_w - 1 - ulen(role)) / 2, y + 11, role, on ? ASH : DIM, bg);
    std::string lays = std::string(elem::PLAYS[e][0]) + (elem::PLAYS[e][1][0] ? std::string(" ") + elem::PLAYS[e][1] : "");
    int rr = y + 13;
    text(cx + 1, rr++, "each stack:", on ? DIM : 0x5A5048, bg);
    rr += rich_wrap(cx + 1, rr, col_w - 3, lays, on ? INK : DIM, bg) + 1;
    text(cx + 1, rr++, "set off:", on ? DIM : 0x5A5048, bg);
    rich_wrap(cx + 1, rr, col_w - 3, elem::PLAYS[e][2], on ? GOLD : DIM, bg);
    buttons_.push_back({cx, y + 4, col_w - 1, h - 7, "", "coal:" + std::to_string(e)});
  }
  text(x + 3, y + h - 3, "■ a hit lays a stack   ☼ a heavy hit sets them all off", DIM, PAPER);
  rich(x + 3, y + h - 2, "[◄►] coal  [Enter] take it  [?] more in the handbook  [Esc] not yet", INK, PAPER);
}

void Client::coal_key(SDL_Keycode k) {
  if (k == SDLK_ESCAPE) { over_ = Over::None; return; }
  if (k == SDLK_LEFT || k == SDLK_KP_4 || k == SDLK_A) coal_sel_ = (coal_sel_ + elem::N - 1) % elem::N;
  else if (k == SDLK_RIGHT || k == SDLK_KP_6 || k == SDLK_D) coal_sel_ = (coal_sel_ + 1) % elem::N;
  else if (k >= SDLK_1 && k < SDLK_1 + elem::N) coal_sel_ = int(k - SDLK_1);
  else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) { send({{"t", "coal"}, {"e", coal_sel_}}); over_ = Over::None; }
  else if (k == SDLK_SLASH || k == SDLK_QUESTION || k == SDLK_F1) { open_book(); book_ch_ = book::index_of("elements"); book_scroll_ = 0; }
}

// ------------------------------------------------------------------ the gatekeeper's rack in Charnfen

// Twelve first weapons in three columns; below, what the chosen one does. A first weapon, not a class.
void Client::draw_rack_window() {
  int cw = con_.get_width(), chh = con_.get_height();
  int w = std::min(cw, 76), h = std::min(chh, 34);
  int x = (cw - w) / 2, y = (chh - h) / 2;
  window(x, y, w, h, "The gatekeeper's rack");
  rich_wrap(x + 3, y + 2, w - 6, "\"One each,\" says the old gatekeeper. \"A first weapon, not a calling. What you do with it is what you become.\"", INK, PAPER);
  int n = int(data::WEAPONS.size());
  rack_sel_ = std::clamp(rack_sel_, 0, n - 1);
  int col_w = (w - 6) / 3;
  for (int s = 0; s < 3; ++s) {
    int cx = x + 3 + s * col_w;
    const auto& st = data::STYLES[s];
    std::string head = st.name;
    for (auto& c : head) c = char(std::toupper((unsigned char)c));
    text(cx, y + 5, "══ " + head + " ══", BRONZE, PAPER);
    text(cx + 3, y + 5, head, st.col, PAPER);
    text(cx, y + 6, st.res == "fury" ? "skills use fury" : "skills use mana", DIM, PAPER);
    int r = y + 8;
    for (int i = 0; i < n; ++i) {
      const auto& wd = data::WEAPONS[i];
      if (wd.style != st.key) continue;
      bool on = i == rack_sel_;
      int bg = on ? SEL : PAPER;
      for (int k = cx - 1; k < cx + col_w - 2; ++k) put(k, r, ' ', INK, bg);
      std::string g = wd.key == "bow" || wd.key == "crossbow" ? "}" : wd.key == "staff" || wd.key == "wand" ? "/" : "|";
      text(cx, r, g, on ? GOLD : 0xB0A080, bg);
      text(cx + 2, r, wd.name, on ? GOLD : INK, bg);
      buttons_.push_back({cx - 1, r, col_w - 1, 1, "", "rack:" + std::to_string(i)});
      r += 2;
    }
  }
  // the chosen weapon
  const auto& wd = data::WEAPONS[rack_sel_];
  int r = y + 17;
  for (int i = x + 2; i < x + w - 2; ++i) put(i, r - 1, 0x2500, 0x5A4028, PAPER);
  const BaseDef* base = nullptr;
  for (auto& b : data::BASES) if (b.key == wd.start) base = &b;
  text(x + 3, r, base ? base->name : wd.name, GOLD, PAPER);
  std::string dmg = base ? "damage " + std::to_string(base->lo) + "-" + std::to_string(base->hi) : "";
  text(x + w - 3 - ulen(dmg), r, dmg, ASH, PAPER);
  r += 2;
  r += rich_wrap(x + 3, r, w - 6, wd.trait, INK, PAPER);
  ++r;
  std::string attack = wd.key == "chisel" ? "walk into a foe to strike; [f] carves a rune" : wd.reach > 0 ? "[f] reaches " + std::to_string(wd.reach) + "; walking into a foe strikes too" : "walk into a foe to strike it";
  ++r;
  rich(x + 3, r++, "Attack: " + attack, INK, PAPER);
  ++r;
  int sign = elem::skill_sign(wd.sig);
  const SkillDef* sk = nullptr;
  for (auto& k : data::SKILLS) if (k.key == wd.sig) sk = &k;
  if (sk) {
    int sx = x + 3 + rich(x + 3, r, "Skill [1]", INK, PAPER) + 1;
    put(sx, r, sign, sign == 0xB7 ? ASH : GOLD, PAPER);
    text(sx + 2, r, sk->name, GOLD, PAPER);
    r += 2;
    rich_wrap(x + 5, r, w - 8, std::string(sk->about) + (sign == 0x263C ? ", and sets off your stacks" : sign == 0x25A0 ? ", laying a stack with each hit" : ""), DIM, PAPER);
  }
  rich(x + 3, y + h - 2, "[◄►] style  [↑↓] weapon  [Enter] take it  [Esc] leave", INK, PAPER);
}

void Client::rack_key(SDL_Keycode k) {
  int n = int(data::WEAPONS.size());
  auto style_of = [&](int i) { return data::WEAPONS[i].style; };
  auto in_style = [&](const std::string& st) { std::vector<int> v; for (int i = 0; i < n; ++i) if (style_of(i) == st) v.push_back(i); return v; };
  if (k == SDLK_ESCAPE) { over_ = Over::None; return; }
  int s = 0;
  for (int i = 0; i < 3; ++i) if (data::STYLES[i].key == style_of(rack_sel_)) s = i;
  auto col = in_style(data::STYLES[s].key);
  int pos = int(std::find(col.begin(), col.end(), rack_sel_) - col.begin());
  if (k == SDLK_UP || k == SDLK_KP_8) rack_sel_ = col[(pos + int(col.size()) - 1) % col.size()];
  else if (k == SDLK_DOWN || k == SDLK_KP_2) rack_sel_ = col[(pos + 1) % col.size()];
  else if (k == SDLK_LEFT || k == SDLK_RIGHT || k == SDLK_KP_4 || k == SDLK_KP_6) {
    int ns = (s + (k == SDLK_LEFT || k == SDLK_KP_4 ? 2 : 1)) % 3;
    auto other = in_style(data::STYLES[ns].key);
    rack_sel_ = other[std::min(pos, int(other.size()) - 1)];
  } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    send({{"t", "rack"}, {"w", data::WEAPONS[rack_sel_].key}});
    over_ = Over::None;
  }
}

// ------------------------------------------------------------------ a camp fire

// The rows to pick from: the keys [2]-[5], then the known skills not on a key, then a second coal while there is none.
int Client::fire_rows() {
  auto& me = snap_["me"];
  int known = int(me.value("known", json::array()).size());
  bool coal = me.value("el", -1) >= 0 && me.value("el2", -1) < 0;
  return 4 + std::max(0, known - 4) + (coal ? 1 : 0);
}

// THE FIRE (plan.md section 5, mock 22): left the keys, right the skills known but not on a key; [Enter] marks a key and
// then swaps it with the skill picked; [◄►] sets a key to the second coal; a path waits at the foot of the window.
void Client::draw_fire_window() {
  auto& me = snap_["me"];
  int cw = con_.get_width(), chh = con_.get_height();
  int w = std::min(cw, 80), h = std::min(chh, 34);
  int x = (cw - w) / 2, y = (chh - h) / 2;
  window(x, y, w, h, "The fire");
  int W2 = (w - 8) / 2, L = x + 3, R = x + 5 + W2;
  for (int j = y + 2; j < y + 17; ++j) put(x + 3 + W2 + 1, j, 0x2502, 0x5A4028, PAPER);
  auto head = [&](int cx, int r, const std::string& t) { text(cx, r, "══ " + t + " ══", BRONZE, PAPER); text(cx + 3, r, t, GOLD, PAPER); };
  const json known = me.value("known", json::array());
  int el = me.value("el", -1), el2 = me.value("el2", -1);
  json slotel = me.value("slotel", json::array({0, 0, 0, 0, 0}));
  int rows = fire_rows();
  fire_sel_ = std::clamp(fire_sel_, 0, rows - 1);
  auto row_bg = [&](int i) { return i == fire_sel_ ? SEL : PAPER; };
  auto skill_line = [&](int cx, int r, int wdt, const json& k, int i, const std::string& key) {
    int bg = row_bg(i);
    for (int c = cx - 1; c < cx + wdt; ++c) put(c, r, ' ', INK, bg);
    int sx = cx;
    if (!key.empty()) { keycap(sx, r, key); sx += ulen(key) + 3; }
    int sign = k.value("sign", 0xB7);
    put(sx, r, sign, sign == 0xB7 ? ASH : GOLD, bg);
    bool marked = i == fire_mark_;
    text(sx + 2, r, k["name"].get<std::string>(), marked ? EMBER : i == fire_sel_ ? GOLD : INK, bg);
    buttons_.push_back({cx - 1, r, wdt + 1, 1, "", "fire:" + std::to_string(i)});
  };
  // left: the keys
  int r = y + 2;
  head(L, r, "On your keys");
  r += 2;
  const json skills = me.value("skills", json::array());
  if (!skills.empty()) {
    keycap(L, r, "1");
    text(L + 4, r, skills[0][0].get<std::string>(), ASH, PAPER);
    if (me["weapon"].is_object()) { std::string wn = me["weapon"]["name"]; text(L + W2 - ulen(wn), r, wn, DIM, PAPER); }
  }
  r += 2;
  for (int i = 0; i < 4; ++i, r += 2) {
    if (i >= int(known.size())) { keycap(L, r, std::to_string(i + 2)); text(L + 4, r, "empty", DIM, PAPER); continue; }
    skill_line(L, r, W2, known[i], i, std::to_string(i + 2));
    if (el2 >= 0) {  // which coal this key lays
      int e = slotel[i + 1].get<int>() ? el2 : el;
      std::string n = std::string("◄") + elem::NAME[e] + "►";
      text(L + W2 - ulen(n), r, n, elem::COL[e], row_bg(i));
    }
  }
  if (el2 >= 0) rich(L, r++, "[◄►] which coal a key lays", DIM, PAPER);
  // right: known, not on keys
  r = y + 2;
  head(R, r, "Known, not on keys");
  r += 2;
  if (known.size() <= 4) text(R, r, "nothing waits here yet", DIM, PAPER);
  for (size_t j = 4; j < known.size(); ++j, r += 2) {
    skill_line(R, r, W2, known[j], int(j), "");
    int st = known[j].value("style", 0), rk = known[j].value("rank", 0);
    std::string from = std::string(data::STYLES[st].name) + " " + std::to_string(rk);
    text(R + W2 - ulen(from), r, from, data::STYLES[st].col, row_bg(int(j)));
  }
  // the second coal, while there is none
  int coal_row = rows - 1;
  r = y + 15;
  if (el >= 0 && el2 < 0) {
    if (fire_coal_ == el || fire_coal_ < 0 || fire_coal_ >= elem::N) fire_coal_ = (el + 1) % elem::N;
    int bg = row_bg(coal_row);
    for (int c = R - 1; c < R + W2; ++c) put(c, r, ' ', INK, bg);
    text(R, r, "A second coal:", INK, bg);
    std::string n = std::string("◄") + elem::NAME[fire_coal_] + "►";
    text(R + W2 - ulen(n), r, n, elem::COL[fire_coal_], bg);
    buttons_.push_back({R - 1, r, W2 + 1, 1, "", "fire:" + std::to_string(coal_row)});
  }
  // what the marked row would do, in a grey line
  int sep = y + 18;
  for (int i = x + 2; i < x + w - 2; ++i) put(i, sep - 1, 0x2500, 0x5A4028, PAPER);
  std::string note;
  if (el >= 0 && el2 < 0 && fire_sel_ == coal_row) note = std::string("[Enter] take the ") + elem::NAME[fire_coal_] + " coal: keys set to it lay " + elem::NAME[fire_coal_] + ". It cannot be undone.";
  else if (fire_mark_ >= 0) note = "Now pick the skill to put on [" + std::to_string(fire_mark_ + 2) + "] and press [Enter].";
  else if (fire_sel_ < int(known.size())) note = known[fire_sel_]["name"].get<std::string>() + ": " + known[fire_sel_]["about"].get<std::string>() + ".";
  rich_wrap(L, sep, w - 6, note, DIM, PAPER);
  // a path waiting
  r = sep + 3;
  json path = me.value("path", json::array({-1, -1, -1}));
  json styles = me.value("styles", json::array());
  int due = -1;
  for (int s = 0; s < 3 && s < int(styles.size()); ++s)
    if (styles[s][1].get<int>() >= data::PATH_RANK && path[s].get<int>() < 0) { due = s; break; }
  if (due >= 0) {
    rich(L, r++, data::STYLES[due].name + std::string(" rank ") + std::to_string(data::PATH_RANK) + ": choose a path", data::STYLES[due].col, PAPER);
    ++r;
    for (int c = 0; c < 2; ++c) {
      const auto& p = data::PATHS[due][c];
      keycap(L, r, c ? "b" : "a");
      text(L + 4, r, p.name, GOLD, PAPER);
      r += rich_wrap(L + 17, r, w - 22, p.about, INK, PAPER);
      buttons_.push_back({L, r - 1, 3, 1, "", "key:" + std::to_string(c ? int(SDLK_B) : int(SDLK_A))});
      ++r;
    }
  } else {
    std::string taken;
    for (int s = 0; s < 3; ++s) if (path[s].get<int>() >= 0) taken += (taken.empty() ? "" : ", ") + std::string(data::PATHS[s][path[s].get<int>()].name);
    text(L, r, taken.empty() ? "A path opens when a style reaches rank " + std::to_string(data::PATH_RANK) + "." : "Your paths: " + taken + ".", DIM, PAPER);
  }
  rich(L, y + h - 2, "[↑↓] pick  [Enter] swap  [◄►] coal  [a][b] path  [Esc] leave", INK, PAPER);
}

void Client::fire_key(SDL_Keycode k) {
  auto& me = snap_["me"];
  int rows = fire_rows(), known = int(me.value("known", json::array()).size());
  int el = me.value("el", -1), el2 = me.value("el2", -1);
  bool coal = el >= 0 && el2 < 0;
  int coal_row = rows - 1;
  if (k == SDLK_ESCAPE) { if (fire_mark_ >= 0) fire_mark_ = -1; else over_ = Over::None; return; }
  if (k == SDLK_UP || k == SDLK_KP_8) fire_sel_ = (fire_sel_ + rows - 1) % rows;
  else if (k == SDLK_DOWN || k == SDLK_KP_2) fire_sel_ = (fire_sel_ + 1) % rows;
  else if (k == SDLK_LEFT || k == SDLK_RIGHT || k == SDLK_KP_4 || k == SDLK_KP_6) {
    if (coal && fire_sel_ == coal_row) {
      int d = k == SDLK_LEFT || k == SDLK_KP_4 ? elem::N - 1 : 1;
      do fire_coal_ = (fire_coal_ + d) % elem::N; while (fire_coal_ == el);
    } else if (fire_sel_ < 4 && fire_sel_ < known && el2 >= 0) send({{"t", "slotel"}, {"k", fire_sel_ + 2}});
  } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    if (coal && fire_sel_ == coal_row) { send({{"t", "coal"}, {"e", fire_coal_}}); return; }
    if (fire_sel_ >= known) return;
    if (fire_mark_ < 0) {
      if (fire_sel_ < 4) fire_mark_ = fire_sel_;
    } else {
      if (fire_sel_ != fire_mark_) send({{"t", "slot"}, {"k", fire_mark_ + 2}, {"i", fire_sel_}});
      fire_mark_ = -1;
    }
  } else if (k == SDLK_A || k == SDLK_B) {
    json path = me.value("path", json::array({-1, -1, -1})), styles = me.value("styles", json::array());
    for (int s = 0; s < 3 && s < int(styles.size()); ++s)
      if (styles[s][1].get<int>() >= data::PATH_RANK && path[s].get<int>() < 0) { send({{"t", "path"}, {"s", s}, {"c", k == SDLK_B ? 1 : 0}}); break; }
  }
}

// ------------------------------------------------------------------ the graves of Charnfen

// Every grave in Charnfen stands for a fallen hero, the best nearest the great fire: the stones in turn are a solo
// hero's and a co-op hero's, each list by how far it got (Frost level, then depth). A stone with no one is old.
json Client::grave_for(int x, int y) {
  int fl = snap_.value("floor", -1);
  if (!maps_.count(fl) || x < 0 || y < 0 || x >= W || y >= H) return nullptr;
  const std::string& map = maps_[fl];
  if (tile_of(map[y * W + x]) != data::GRAVE) return nullptr;
  std::vector<std::pair<int, int>> stones;  // (distance to the fire, cell)
  for (int i = 0; i < W * H && i < int(map.size()); ++i)
    if (tile_of(map[i]) == data::GRAVE) stones.push_back({std::abs(i % W - 60) + 2 * std::abs(i / W - 53), i});
  std::sort(stones.begin(), stones.end());
  int at = -1;
  for (size_t k = 0; k < stones.size(); ++k) if (stones[k].second == y * W + x) at = int(k);
  std::vector<json> solo, coop;
  for (auto& g : graves_) (g.value("coop", false) ? coop : solo).push_back(g);
  auto better = [](const json& a, const json& b) {
    return a.value("frost", 0) != b.value("frost", 0) ? a.value("frost", 0) > b.value("frost", 0) : a.value("depth", 0) > b.value("depth", 0);
  };
  std::stable_sort(solo.begin(), solo.end(), better);
  std::stable_sort(coop.begin(), coop.end(), better);
  auto& list = at % 2 ? coop : solo;
  size_t n = size_t(at / 2);
  json g = n < list.size() ? list[n] : json::object();
  g["i"] = at;
  g["place"] = int(n) + 1;
  g["list"] = at % 2 ? "co-op" : "solo";
  return g;
}

// The graves of Charnfen (mock 23): two lists, alone and together, ranked by the deepest point reached; the chosen
// grave's stone below. Walking into a stone opens it at that hero.
std::vector<json> Client::grave_list(bool coop) const {
  std::vector<json> l;
  for (auto& g : graves_) if (g.value("coop", false) == coop) l.push_back(g);
  std::stable_sort(l.begin(), l.end(), [](const json& a, const json& b) {
    return a.value("frost", 0) != b.value("frost", 0) ? a.value("frost", 0) > b.value("frost", 0) : a.value("depth", 0) > b.value("depth", 0);
  });
  return l;
}

void Client::graves_key(SDL_Keycode k) {
  int n = int(grave_list(graves_tab_ == 1).size());
  if (k == SDLK_ESCAPE) { over_ = Over::None; return; }
  if (k == SDLK_TAB || k == SDLK_LEFT || k == SDLK_RIGHT) { graves_tab_ ^= 1; grave_sel_ = 0; return; }
  if (n && (k == SDLK_UP || k == SDLK_KP_8)) grave_sel_ = (std::max(0, grave_sel_) + n - 1) % n;
  if (n && (k == SDLK_DOWN || k == SDLK_KP_2)) grave_sel_ = (std::max(0, grave_sel_) + 1) % n;
}

void Client::draw_grave() {
  // grave_at_ (set when a stone is walked into) picks the list and the hero, once
  if (grave_at_ >= 0) {
    int fl = snap_.value("floor", -1);
    grave_sel_ = -1;
    if (maps_.count(fl)) {
      const std::string& map = maps_[fl];
      for (int i = 0; i < W * H && i < int(map.size()); ++i)
        if (tile_of(map[i]) == data::GRAVE) {
          json c = grave_for(i % W, i / W);
          if (c.value("i", -2) != grave_at_) continue;
          graves_tab_ = c.value("list", std::string("solo")) == "co-op" ? 1 : 0;
          if (c.contains("name")) grave_sel_ = c.value("place", 1) - 1;
          break;
        }
    }
    grave_at_ = -1;
  }
  auto list = grave_list(graves_tab_ == 1);
  int cw = con_.get_width(), chh = con_.get_height();
  int w = std::min(cw, 86), h = std::min(chh, 46);
  int x = (cw - w) / 2, y = (chh - h) / 2;
  window(x, y, w, h, "THE GRAVES OF CHARNFEN", Mat::Stone);
  int sy = y + h - 20, sw = 46, sx = x + (w - sw) / 2;  // the stone
  int bg = win_bg_;
  // tabs
  const char* tabs[] = {" Alone ", " Together "};
  for (int t = 0, tx = x + 3; t < 2; ++t) {
    bool on = t == graves_tab_;
    if (on) for (int k = 0; k < ulen(tabs[t]); ++k) put(tx + k, y + 3, ' ', INK, GOLD);
    text(tx, y + 2, tabs[t], on ? 0x201810 : 0x8A8478, on ? GOLD : bg);
    buttons_.push_back({tx, y + 2, ulen(tabs[t]), 1, "", std::string("gtab:") + char('0' + t)});
    tx += ulen(tabs[t]) + 2;
  }
  int cN = x + 3, cName = x + 7, cTitle = x + 22, cDeep = x + 38, cFell = x + 60;
  int r = y + 4;
  text(cN, r, "#", GOLD, bg); text(cName, r, "Name", GOLD, bg); text(cTitle, r, "Title", GOLD, bg);
  text(cDeep, r, "Deepest", GOLD, bg); text(cFell, r, "Fell", GOLD, bg);
  r += 2;
  int rows = std::max(2, (sy - 3 - r) / 2), first = std::clamp(grave_sel_ - rows / 2, 0, std::max(0, int(list.size()) - rows));
  auto el_col = [](const json& g) { int e = g.value("el", -1); return e >= 0 && e < elem::N ? elem::COL[e] : 0xB0A890; };
  auto deep = [](const json& g) {
    int f = g.value("frost", 0);
    std::string z = g.value("zone", std::string(""));
    if (z.rfind("The ", 0) == 0) z = z.substr(4);
    return (f ? "Frost " + std::to_string(f) : std::string("Normal")) + " - " + z;
  };
  if (list.empty()) text(cName, r, graves_tab_ ? "No hero who went down with friends lies here yet." : "No hero lies here yet.", 0x8A8478, bg);
  for (int i = first; i < int(list.size()) && i < first + rows; ++i, r += 2) {
    auto& g = list[i];
    bool on = i == grave_sel_;
    int rb = on ? 0x3A3630 : bg;
    if (on) for (int k = x + 1; k < x + w - 1; ++k) { put(k, r, ' ', INK, rb); put(k, r + 1, ' ', INK, rb); }
    if (on) put(x + 2, r, 0x25BA, GOLD, rb);
    text(cN, r, std::to_string(i + 1), 0xC8C0B0, rb);
    text(cName, r, g.value("name", std::string("?")).substr(0, 14), 0xE8E0D0, rb);
    text(cTitle, r, g.value("title", std::string("-")).substr(0, 15), el_col(g), rb);
    text(cDeep, r, deep(g).substr(0, 21), 0x90C8FF, rb);
    text(cFell, r, g.value("killer", std::string("the dark")).substr(0, size_t(std::max(4, x + w - 2 - cFell))), 0x8A8478, rb);
    buttons_.push_back({x + 1, r, w - 2, 2, "", "grave:" + std::to_string(i)});
  }
  // the stone of the chosen hero
  for (int k = x + 1; k < x + w - 1; ++k) put(k, sy - 2, 0x2500, 0x5A5650, bg);
  for (int j = sy; j < sy + 16; ++j)
    for (int k = sx; k < sx + sw; ++k) put(k, j, ' ', INK, 0x4A4640);
  for (int k = sx - 2; k < sx + sw + 2; ++k) put(k, sy + 16, 0x2580, 0x3A3026, bg);
  int sr = sy + 1;
  auto line = [&](const std::string& t, int col) { text(sx + (sw - ulen(t)) / 2, sr, t, col, 0x4A4640); sr += 2; };
  if (grave_sel_ < 0 || grave_sel_ >= int(list.size())) {
    sr += 4;
    line("An old stone. The name is worn away.", 0x9A9488);
    line("Snow lies on it.", 0x7A7468);
  } else {
    auto& g = list[grave_sel_];
    line("┼", 0xC8C0B0);
    std::string name = g.value("name", std::string("?"));
    for (auto& c : name) c = char(std::toupper(static_cast<unsigned char>(c)));
    line(name, 0xF0E8D8);
    line(g.value("title", std::string("")) + " - level " + std::to_string(g.value("lvl", 1)), el_col(g));
    std::string kit = g.value("weapon", std::string(""));
    int e1 = g.value("el", -1), e2 = g.value("el2", -1);
    if (e1 >= 0 && e1 < elem::N) kit += std::string(kit.empty() ? "" : " - ") + elem::NAME[e1] + (e2 >= 0 && e2 < elem::N ? std::string(" and ") + elem::NAME[e2] : "");
    if (!kit.empty()) line(kit, 0xC8C0B0);
    int f = g.value("frost", 0);
    line("reached " + (f ? "Frost " + std::to_string(f) : std::string("Normal")) + ", " + g.value("zone", std::string("")), 0xC8C0B0);
    line("felled by " + g.value("killer", std::string("the dark")), 0xC8C0B0);
    int rk = g.value("rekindled", 0);
    line(std::to_string(g.value("kills", 0)) + " foes went before" + (rk ? ", rekindled " + std::to_string(rk) + (rk == 1 ? " time" : " times") : std::string()), 0xA8A090);
  }
  rich(x + 3, y + h - 2, "[↑↓] grave   [Tab] alone / together   [Esc] leave", INK, bg);
}

// ------------------------------------------------------------------ the helpers (plan.md section 6)

// Walk into a helper and their window opens: a line in their voice, and their keys. Ysolde heals and sells the potions,
// Brann turns a resistance on worn gear into another element, old Ebb shows the way taken and opens the way on.
void Client::draw_talk() {
  if (!snap_.contains("talk")) return;
  auto& t = snap_["talk"];
  auto& me = snap_["me"];
  std::string who = t.value("who", "");
  int cw = con_.get_width(), chh = con_.get_height();
  bool guide = who == "guide", smith = who == "smith";
  int w = std::min(cw, guide ? 72 : 62);
  int rows = smith ? std::max({1, int(t.value("gear", json::array()).size()), int(t.value("temper", json::array()).size())}) : 0;
  int h = std::min(chh, guide ? (t.contains("ways") ? 34 : 10) : smith ? 15 + rows : 14);
  int x = (cw - w) / 2, y = (chh - h) / 2;
  const char* title = who == "healer" ? "Ysolde, the healer" : smith ? "Brann, the smith" : "Old Ebb";
  int col = who == "healer" ? 0xAADCAA : smith ? 0xC8966E : 0x9696AA;
  window(x, y, w, h, title);
  int r = y + 2;
  r += rich_wrap(x + 3, r, w - 6, "\"" + t.value("line", std::string()) + "\"", col, PAPER) + 1;
  int gold = me.value("gold", 0);
  if (who == "healer") {
    bool healed = t.value("healed", false);
    int price = t.value("potion", 0);
    rich(x + 3, r, "[h]", INK, PAPER); text(x + 7, r, "heal me", healed ? DIM : ASH, PAPER);
    text(x + 30, r, healed ? "once a camp" : "free", DIM, PAPER);
    buttons_.push_back({x + 2, r++, w - 4, 1, "", "key:" + std::to_string(SDLK_H)});
    rich(x + 3, r, "[p]", INK, PAPER); text(x + 7, r, "healing potion (" + std::to_string(me.value("heal", 0)) + ")", 0xFF7070, PAPER);
    text(x + 30, r, std::to_string(price) + "g", 0xFFD700, PAPER);
    buttons_.push_back({x + 2, r++, w - 4, 1, "", "key:" + std::to_string(SDLK_P)});
    rich(x + 3, r, "[m]", INK, PAPER); text(x + 7, r, "mana potion (" + std::to_string(me.value("manap", 0)) + ")", 0x7090FF, PAPER);
    text(x + 30, r, std::to_string(price) + "g", 0xFFD700, PAPER);
    buttons_.push_back({x + 2, r++, w - 4, 1, "", "key:" + std::to_string(SDLK_M)});
    ++r;
    if (me.value("scars", 0) > 0) text(x + 3, r, "Scars she cannot mend: " + std::to_string(me.value("scars", 0)) + ".", DIM, PAPER);
  } else if (smith && smith_temper_) {
    auto tem = t.value("temper", json::array());
    int n = int(tem.size()), cap = t.value("cap", 1);
    text(x + 3, r++, "TEMPER  (up to +" + std::to_string(cap) + " at this Frost)", GOLD, PAPER);
    if (!n) rich_wrap(x + 3, r, w - 6, "You wear nothing I could put my hammer to.", DIM, PAPER);
    else {
      talk_sel_ = std::clamp(talk_sel_, 0, n - 1);
      for (int i = 0; i < n; ++i, ++r) {
        auto& g = tem[i];
        bool on = i == talk_sel_, full = g.value("plus", 0) >= cap;
        int bg = on ? SEL : PAPER;
        for (int k = x + 2; k < x + w - 2; ++k) put(k, r, ' ', INK, bg);
        text(x + 3, r, on ? ">" : " ", GOLD, bg);
        text(x + 5, r, clip(g.value("name", std::string()), w - 22), data::rarity_col(g.value("rarity", std::string("normal"))), bg);
        std::string pr = full ? "as far as it goes" : std::to_string(g.value("price", 0)) + "g";
        text(x + w - 3 - ulen(pr), r, pr, full ? DIM : 0xFFD700, bg);
        buttons_.push_back({x + 2, r, w - 4, 1, "", "talksel:" + std::to_string(i)});
      }
      ++r;
      rich(x + 3, r++, "Your gold: " + std::to_string(gold) + ".", ASH, PAPER);
    }
    rich(x + 3, y + h - 2, "[↑↓] gear  [Enter] temper  [Tab] retune  [Esc] leave", INK, PAPER);
    return;
  } else if (smith) {
    text(x + 3, r++, "RETUNE", GOLD, PAPER);
    auto gear = t.value("gear", json::array());
    int n = int(gear.size());
    if (!n) {
      rich_wrap(x + 3, r, w - 6, "Nothing you wear turns an element. Bring me gear with a resistance on it.", DIM, PAPER);
    } else {
      talk_sel_ = std::clamp(talk_sel_, 0, n - 1);
      for (int i = 0; i < n; ++i, ++r) {
        auto& g = gear[i];
        bool on = i == talk_sel_;
        int bg = on ? SEL : PAPER;
        for (int k = x + 2; k < x + w - 2; ++k) put(k, r, ' ', INK, bg);
        int e = g.value("el", 0);
        text(x + 3, r, on ? ">" : " ", GOLD, bg);
        text(x + 5, r, clip(g.value("name", std::string()), w - 34), data::rarity_col(g.value("rarity", std::string("normal"))), bg);
        std::string from = "+" + std::to_string(g.value("v", 0)) + "% " + elem::NAME[e];
        int fx = x + w - 27;
        text(fx, r, from, elem::COL[e], bg);
        if (on) {
          int to = talk_el_ < 0 || talk_el_ == e ? (e + 1) % elem::N : talk_el_;
          talk_el_ = to;
          text(fx + ulen(from) + 1, r, "► " + std::string(elem::NAME[to]), elem::COL[to], bg);
        }
        buttons_.push_back({x + 2, r, w - 4, 1, "", "talksel:" + std::to_string(i)});
      }
      ++r;
      rich(x + 3, r++, "Costs " + std::to_string(t.value("retune", 0)) + " gold. Your gold: " + std::to_string(gold) + ".", ASH, PAPER);
    }
    rich(x + 3, y + h - 2, "[↑↓] gear  [◄►] element  [Enter] retune  [Tab] temper  [Esc] leave", INK, PAPER);
    return;
  } else if (guide) {
    if (!t.contains("ways")) {
      rich(x + 3, y + h - 2, "[Esc] leave", INK, PAPER);
      return;
    }
    auto& ways = t["ways"];
    std::vector<int> can;
    for (auto& wy : ways) can.push_back(wy.value("z", 0));
    text(x + 3, r++, "THE WAY DOWN", GOLD, PAPER);
    r += route_chart(x + 3, r + 1, w - 6, PAPER, t.value("open", false) ? std::vector<int>{} : can) + 2;
    for (int k = x + 2; k < x + w - 2; ++k) put(k, r, 0x2500, 0x5A4028, PAPER);
    r += 2;
    bool open = t.value("open", false);
    int n = int(ways.size());
    talk_sel_ = std::clamp(talk_sel_, 0, std::max(0, n - 1));
    for (int i = 0; i < n && !open; ++i) {
      auto& wy = ways[i];
      bool on = i == talk_sel_;
      int bg = on ? SEL : PAPER;
      int top = r;
      for (int k = x + 2; k < x + w - 2; ++k) put(k, r, ' ', INK, bg);
      rich(x + 3, r, "[" + std::to_string(i + 1) + "]", INK, bg);
      text(x + 7, r++, wy.value("name", std::string()), on ? GOLD : ASH, bg);
      r += rich_wrap(x + 7, r, w - 10, wy.value("text", std::string()), on ? INK : DIM, PAPER) + 1;
      buttons_.push_back({x + 2, top, w - 4, r - top - 1, "", "key:" + std::to_string(SDLK_1 + i)});
    }
    if (open) rich_wrap(x + 3, r, w - 6, "The way on is open. Ebb has written it in his book.", DIM, PAPER);
    rich(x + 3, y + h - 2, open ? "[Esc] leave" : "[↑↓] choose  [Enter] open the way  [Esc] leave", INK, PAPER);
    return;
  }
  rich(x + 3, y + h - 2, "[Esc] leave", INK, PAPER);
}

void Client::talk_key(SDL_Keycode k) {
  auto close = [&] { send({{"t", "talk"}, {"k", "close"}}); over_ = Over::None; };
  if (k == SDLK_ESCAPE) { close(); return; }
  if (!snap_.contains("talk")) return;
  auto& t = snap_["talk"];
  std::string who = t.value("who", "");
  bool up = k == SDLK_UP || k == SDLK_KP_8, down = k == SDLK_DOWN || k == SDLK_KP_2;
  bool enter = k == SDLK_RETURN || k == SDLK_KP_ENTER;
  if (who == "healer") {
    if (k == SDLK_H) send({{"t", "talk"}, {"k", "heal"}});
    else if (k == SDLK_P) send({{"t", "talk"}, {"k", "potion"}, {"p", "heal"}});
    else if (k == SDLK_M) send({{"t", "talk"}, {"k", "potion"}, {"p", "mana"}});
  } else if (who == "smith") {
    if (k == SDLK_TAB) { smith_temper_ = !smith_temper_; talk_sel_ = 0; talk_el_ = -1; return; }
    if (smith_temper_) {
      auto tem = t.value("temper", json::array());
      int n = int(tem.size());
      if (!n) return;
      if (up || down) { talk_sel_ = (talk_sel_ + (up ? n - 1 : 1)) % n; return; }
      if (enter) send({{"t", "talk"}, {"k", "temper"}, {"id", tem[std::clamp(talk_sel_, 0, n - 1)]["id"]}});
      return;
    }
    auto gear = t.value("gear", json::array());
    int n = int(gear.size());
    if (!n) return;
    if (up || down) { talk_sel_ = (talk_sel_ + (up ? n - 1 : 1)) % n; talk_el_ = -1; return; }
    int e = gear[std::clamp(talk_sel_, 0, n - 1)].value("el", 0);
    if (k == SDLK_LEFT || k == SDLK_RIGHT || k == SDLK_KP_4 || k == SDLK_KP_6) {
      int d = k == SDLK_LEFT || k == SDLK_KP_4 ? elem::N - 1 : 1;
      int to = talk_el_ < 0 ? e : talk_el_;
      do to = (to + d) % elem::N; while (to == e);
      talk_el_ = to;
    } else if (enter && talk_el_ >= 0 && talk_el_ != e) {
      auto& g = gear[std::clamp(talk_sel_, 0, n - 1)];
      send({{"t", "talk"}, {"k", "retune"}, {"id", g["id"]}, {"from", g["from"]}, {"e", talk_el_}});
      talk_el_ = -1;
    }
  } else if (who == "guide" && t.contains("ways") && !t.value("open", false)) {
    auto& ways = t["ways"];
    int n = int(ways.size());
    if (!n) return;
    if (up || down) { talk_sel_ = (talk_sel_ + (up ? n - 1 : 1)) % n; return; }
    int pick = k >= SDLK_1 && k < SDLK_1 + n ? int(k - SDLK_1) : enter ? talk_sel_ : -1;
    if (pick >= 0) send({{"t", "talk"}, {"k", "way"}, {"z", ways[pick]["z"]}});
  }
}
