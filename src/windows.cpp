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
  // left: who you are and how you grow
  int r = y + 2;
  text(L, r++, me["clsname"].get<std::string>(), me["col"].get<int>(), PAPER);
  r += rich_wrap(L, r, W2, me.value("rule", std::string("")), INK, PAPER);
  ++r;
  int prev = me["prev"], next = me["next"], xp = me["xp"];
  text(L, r, "Level " + std::to_string(me["lvl"].get<int>()), ASH, PAPER);
  text(L + 10, r, "XP", DIM, PAPER);
  bar(L + 13, r++, W2 - 13, next > prev ? double(xp - prev) / (next - prev) : 0, 0xC09040);
  int frost = snap_.value("frost", 0);
  text(L, r++, (frost ? "Frost " + std::to_string(frost) : std::string("Normal")) + ", depth " + std::to_string(snap_.value("depth", 0)), DIM, PAPER);
  text(L, r++, std::to_string(me.value("kills", 0)) + " foes slain", DIM, PAPER);
  ++r;
  head(L, r++, "Perks");
  if (me["perks"].empty()) text(L, r++, "none yet", DIM, PAPER);
  for (auto& p : me["perks"]) {
    if (r >= y + h - 3) break;
    std::string n = p[0].get<std::string>();
    int rank = p[1];
    text(L, r, n, ASH, PAPER);
    if (rank > 1) text(L + W2 - 2, r, "x" + std::to_string(rank), INK, PAPER);
    ++r;
  }
  // right: what you can take
  r = y + 2;
  bool fury = me.value("pool", "mana") == "fury";
  right_val(r++, "Life", std::to_string(me["maxhp"].get<int>()), 0xE06050, ASH);
  right_val(r++, fury ? "Fury" : "Mana", std::to_string(me["maxmana"].get<int>()), fury ? FURY_TEXT : MANA_TEXT, ASH);
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
  if (frost) text(R, r++, "Frost takes " + std::to_string(10 * frost) + "% from each", 0x8AB8E0, PAPER);
  int el = me.value("el", -1);
  if (el >= 0) {
    ++r;
    head(R, r++, "Your coal");
    text(R, r, elem::NAME[el], elem::COL[el], PAPER);
    int el2 = me.value("el2", -1);
    if (el2 >= 0) { text(R + ulen(elem::NAME[el]), r, " and ", INK, PAPER); text(R + ulen(elem::NAME[el]) + 5, r, elem::NAME[el2], elem::COL[el2], PAPER); }
    ++r;
  }
  rich(x + w - 13, y + h - 2, "[Esc] back", INK, PAPER);
}

// ------------------------------------------------------------------ options
void Client::apply_volume() { music_.volume(music_on_ ? music_vol_ / 10.0f : 0); }

void Client::draw_options() {
  int cw = con_.get_width(), chh = con_.get_height();
  int w = std::min(cw, 46), h = 16;
  int x = (cw - w) / 2, y = (chh - h) / 2;
  window(x, y, w, h, "Options");
  struct Row { const char* name; std::string val; int level; const char* note; };
  Row rows[] = {{"Music volume", "", music_on_ ? music_vol_ : 0, music_on_ ? "" : "music is off on the title screen"},
                {"Sound volume", "", sound_vol_, "no sounds yet; they come with the lab"},
                {"Floating numbers", numbers_on_ ? "on" : "off", -1, "the harm dealt, rising over the map"}};
  int r = y + 2;
  for (int i = 0; i < 3; ++i) {
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
  if (k == SDLK_UP || k == SDLK_KP_8) { opt_sel_ = (opt_sel_ + 2) % 3; return; }
  if (k == SDLK_DOWN || k == SDLK_KP_2) { opt_sel_ = (opt_sel_ + 1) % 3; return; }
  if (!d) return;
  if (opt_sel_ == 0) {
    if (!music_on_ && d > 0) { music_on_ = true; music_vol_ = std::max(1, music_vol_); }
    else music_vol_ = std::clamp(music_vol_ + d, 0, 10);
    apply_volume();
  } else if (opt_sel_ == 1) sound_vol_ = std::clamp(sound_vol_ + d, 0, 10);
  else numbers_on_ = !numbers_on_;
  save_settings();
}

// ------------------------------------------------------------------ the Delver's Handbook
// It opens at the page that fits: a tip just given, a fight, the fire of a waystation, Charnfen; else the page read last.
void Client::open_book() {
  int ch = book_last_;
  book_why_.clear();
  bool in_game = screen_ == Screen::Game && snap_.is_object();
  if (in_game && tip_ch_ >= 0 && now() - tip_t_ < 30) { ch = tip_ch_; book_why_ = "for the tip you were just given."; }
  else if (in_game && !foes().empty()) { ch = book::index_of("fighting"); book_why_ = "you are in a fight."; }
  else if (in_game && snap_.value("town", false)) { ch = book::index_of("first"); book_why_ = "you are in Charnfen."; }
  else if (in_game && snap_.value("rest", false)) { ch = book::index_of("rest"); book_why_ = "you are at a waystation."; }
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
