// The debug and testing menu ([F12] or [`], or [d] in the game menu): a checklist of what still needs testing, a
// bug report that writes down everything needed to find the same map again (build, seed, zone, map, where the hero
// stood) with a screenshot, and a few tools to get around quickly. Reports go to bugs.txt in the game's own folder,
// and each one is copied to the clipboard as well, so it can be pasted straight into a message.
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "client.hpp"
#include "data.hpp"

namespace {
const int GREY = 0xA0A0A0, DIM = 0x606060, WHITE = 0xE8E8E8, GOLD = 0xFFD27F, KEY = 0xFFC860;
const int OK_COL = 0x70D070, BUG_COL = 0xFF6050, INFO = 0x80C0FF;

std::string pref_dir() {
  char* p = SDL_GetPrefPath("NilioDev", "Pyreheart");
  std::string s = p ? std::string(p) : "";
  SDL_free(p);
  return s;
}

const char* const TOOLS[][2] = {
    {"heal", "Heal fully (and wake from death)"},
    {"god", "Harm off / on (nothing hurts you)"},
    {"reveal", "Show the whole map"},
    {"level", "Gain a level"},
    {"gold", "Get 500 gold"},
    {"next", "Go on to the next map"},
    {"boss", "Jump to this zone's boss map"},
    {"music", "Music: classic / lab"},
    {"copy", "Copy all bug reports to the clipboard"},
    {"folder", "Open the folder with the reports and screenshots"},
    {"clear", "Start a new bug list (the old one goes to bugs-sent.txt)"},
};
const int NTOOLS = int(std::size(TOOLS));
}  // namespace

// What there is to test: the whole game first, then each class and each zone.
std::vector<std::string> Client::test_items() const {
  std::vector<std::string> t = {
      "Start: intro crawl, title screen, name and class",
      "Charnfen: walk around, graveyard dead, gate guards, trader",
      "Controls: keyboard (move, look, shoot, skills, pack)",
      "Controls: touch on the phone (tap, hold to walk, buttons)",
      "Pack: equip, drop, compare marks on items",
      "Shop: buy, sell, potions",
      "Perks: choosing a perk on level up",
      "Waystation: rest, route choice to the next zone",
      "Music: each zone its own, boss music, music off",
      "Music lab: the new music ideas (switch in Options or here)",
      "Save and continue a dive later",
      "Death and the end screen",
      "The Pyre: Sparks, gifts, Frost levels",
      "Co-op: host and join a game (2-4 players)",
      "Co-op: trading loot between players",
      "Victory: the Ashen King and the ending text",
  };
  for (auto& w : data::WEAPONS) t.push_back("Weapon " + w.name + ": trait, skill, balance, feel");
  for (int i = 0; i < data::ZONES; ++i) {
    const std::string& z = data::ACTS[i].name;
    t.push_back(z + ": maps look right, room to walk and fight");
    t.push_back(z + ": themed rooms, light and particles");
    t.push_back(z + ": tremor and other atmosphere");
    t.push_back(z + ": foes and their difficulty");
    t.push_back(z + ": the boss fight");
  }
  t.push_back("Cistern: the reservoir arena floods during the boss fight");
  t.push_back("Prison: dead prisoners rise from the cells");
  t.push_back("Sanctum: kneeling altar guards wake near the altar");
  t.push_back("Deep: the crust plates and the two-map maze");
  return t;
}

void Client::load_tests() {
  tests_.clear();
  std::ifstream f(pref_dir() + "testlist.txt");
  std::string line;
  while (std::getline(f, line)) {
    auto tab = line.find('\t');
    if (tab == std::string::npos) continue;
    std::string st = line.substr(0, tab), item = line.substr(tab + 1);
    if (st == "#next") { bug_next_ = std::max(1, std::atoi(item.c_str())); continue; }
    tests_[item] = st == "ok" ? 1 : st == "bug" ? 2 : 0;
  }
  tests_loaded_ = true;
}

void Client::save_tests() {
  std::ofstream f(pref_dir() + "testlist.txt");
  f << "#next\t" << bug_next_ << "\n";
  for (auto& it : test_items()) {
    int s = tests_.count(it) ? tests_[it] : 0;
    if (s) f << (s == 1 ? "ok" : "bug") << "\t" << it << "\n";
  }
}

// The view as it is is saved first (without the menu over it), then the menu opens on the next frame.
void Client::open_debug() {
  if (!tests_loaded_) load_tests();
  over_ = Over::None;
  shot_ = pref_dir() + "last-view.png";
  dbg_pending_ = true;
  dbg_msg_.clear();
}

void Client::close_debug() {
  if (typing_) SDL_StopTextInput(ctx_.get_sdl_window());
  typing_ = false;
  over_ = Over::None;
}

void Client::debug_tab(int t) {
  dbg_tab_ = (t + 3) % 3;
  sel_ = 0;
  bool type = dbg_tab_ == 1;
  if (type && !typing_) SDL_StartTextInput(ctx_.get_sdl_window());
  if (!type && typing_) SDL_StopTextInput(ctx_.get_sdl_window());
  typing_ = type;
}

// Everything needed to come back to the same spot: the line under "Replay" starts the game there.
std::string Client::bug_context() const {
  if (!snap_.is_object()) return "Not in a dive.\n";
  const json& s = snap_;
  const json& me = s["me"];
  std::ostringstream o;
  bool town = s.value("town", false), rest = s.value("rest", false);
  int zf = s.value("zfloor", 0), zfs = s.value("zfloors", 3);
  std::string zone = s.value("zname", "?");
  o << "Build: " << PYREHEART_BUILD << "\n";
  if (town) o << "Where: Charnfen (the surface)";
  else if (rest) o << "Where: the waystation after " << zone << " map " << zf + 1;
  else o << "Where: " << zone << ", map " << zf + 1 << " of " << zfs << (zf == zfs - 1 ? " (boss map)" : "");
  o << ", depth " << s.value("depth", 0) << ", turn " << s.value("turn", 0) << "\n";
  int hx = me.value("x", 0), hy = me.value("y", 0);
  std::string tile = "?";
  auto m = maps_.find(s.value("floor", -1));
  if (m != maps_.end() && hy * W + hx < int(m->second.size())) tile = data::TILES[tile_of(m->second[hy * W + hx])].name;
  o << "Hero: " << me.value("clsname", "?") << " level " << me.value("lvl", 1) << " at " << hx << "," << hy << " on " << tile
    << ", HP " << me.value("hp", 0) << "/" << me.value("maxhp", 0) << ", Frost " << s.value("frost", 0)
    << (s.value("god", false) ? ", harm off" : "") << ", " << s["heroes"].size() << " player(s)\n";
  o << "Replay: PYREHEART_SEED=" << s.value("seed", "?");
  if (!town) {
    o << " PYREHEART_ZONE=" << s.value("zone", "?") << " PYREHEART_ZFLOOR=" << zf;
    if (rest) o << " PYREHEART_REST=1";
  }
  o << "\n";
  if (!log_.empty()) {
    o << "Last messages:\n";
    size_t from = log_.size() > 6 ? log_.size() - 6 : 0;
    for (size_t i = from; i < log_.size(); ++i) o << "  " << log_[i].first << "\n";
  }
  return o.str();
}

void Client::save_bug() {
  std::string dir = pref_dir();
  int n = bug_next_++;
  std::time_t now = std::time(nullptr);
  char when[32];
  std::strftime(when, sizeof when, "%Y-%m-%d %H:%M", std::localtime(&now));
  std::ostringstream o;
  o << "=== Pyreheart bug #" << n << " (" << when << ") ===\n";
  if (!note_test_.empty()) o << "Test: " << note_test_ << "\n";
  o << bug_context();
  std::error_code ec;
  std::string png = "bug-" + std::to_string(n) + ".png";
  std::filesystem::copy_file(dir + "last-view.png", dir + png, std::filesystem::copy_options::overwrite_existing, ec);
  if (!ec) o << "Screenshot: " << png << "\n";
  o << "Note: " << note_ << "\n\n";
  std::ofstream(dir + "bugs.txt", std::ios::app) << o.str();
  SDL_SetClipboardText(o.str().c_str());
  save_tests();
  note_.clear();
  note_test_.clear();
  dbg_msg_ = "Bug #" + std::to_string(n) + " is saved and copied to the clipboard: paste it into your message to Claude" +
             (ec ? "." : ", with " + png + " from the bug folder.");
}

void Client::debug_tool(int i) {
  std::string w = TOOLS[i][0];
  std::string dir = pref_dir();
  if (w == "music") {
    music_lab_ = !music_lab_;
    music_.use_lab(music_lab_);
    save_settings();
    dbg_msg_ = std::string("The music is now ") + (music_lab_ ? "the lab." : "classic.");
  } else if (w == "copy") {
    std::ifstream f(dir + "bugs.txt");
    std::stringstream b;
    b << f.rdbuf();
    std::string all = b.str();
    if (all.empty()) { dbg_msg_ = "There are no bug reports yet."; return; }
    SDL_SetClipboardText(all.c_str());
    dbg_msg_ = "All bug reports are copied to the clipboard.";
  } else if (w == "folder") {
    std::string url = "file://" + dir;
    if (!SDL_OpenURL(url.c_str())) dbg_msg_ = "The folder: " + dir;
    else dbg_msg_ = "Opened " + dir;
  } else if (w == "clear") {
    std::ifstream f(dir + "bugs.txt");
    std::stringstream b;
    b << f.rdbuf();
    f.close();
    if (b.str().empty()) { dbg_msg_ = "The bug list is already empty."; return; }
    std::ofstream(dir + "bugs-sent.txt", std::ios::app) << b.str();
    std::ofstream(dir + "bugs.txt", std::ios::trunc);
    dbg_msg_ = "A new bug list is started; the old reports are kept in bugs-sent.txt.";
  } else {
    send({{"t", "debug"}, {"w", w}});
    if (w == "next" || w == "boss") close_debug();
    else dbg_msg_ = std::string("Done: ") + TOOLS[i][1] + ".";
  }
}

void Client::debug_key(SDL_Keycode k) {
  bool enter = k == SDLK_RETURN || k == SDLK_KP_ENTER;
  if (k == SDLK_ESCAPE) { close_debug(); return; }
  if (k == SDLK_TAB || k == SDLK_F12) { debug_tab(dbg_tab_ + 1); return; }
  if (k == SDLK_F2 || k == SDLK_F3 || k == SDLK_F4) { debug_tab(int(k - SDLK_F2)); return; }
  if (dbg_tab_ == 1) {  // typing the note
    if (k == SDLK_BACKSPACE && !note_.empty()) {
      do note_.pop_back(); while (!note_.empty() && (note_.back() & 0xC0) == 0x80);
    } else if (enter) {
      if (note_.empty() && note_test_.empty()) dbg_msg_ = "Write what went wrong first.";
      else save_bug();
    }
    return;
  }
  if (!typing_ && (k == SDLK_1 || k == SDLK_2 || k == SDLK_3)) { debug_tab(int(k - SDLK_1)); return; }
  int dy = (k == SDLK_UP || k == SDLK_KP_8) ? -1 : (k == SDLK_DOWN || k == SDLK_KP_2) ? 1 : 0;
  int pg = (k == SDLK_PAGEUP || k == SDLK_KP_9) ? -10 : (k == SDLK_PAGEDOWN || k == SDLK_KP_3) ? 10 : 0;
  if (dbg_tab_ == 0) {
    auto items = test_items();
    int n = int(items.size());
    if (dy || pg) { sel_ = std::clamp(sel_ + dy + pg, 0, n - 1); return; }
    sel_ = std::clamp(sel_, 0, n - 1);
    int& st = tests_[items[sel_]];
    if (enter || k == SDLK_SPACE) {
      st = (st + 1) % 3;
      save_tests();
      if (st == 2) { note_test_ = items[sel_]; debug_tab(1); dbg_msg_ = "Marked as a bug. Write what went wrong, then press Enter."; }
    } else if (k == SDLK_B) {
      st = 2;
      save_tests();
      note_test_ = items[sel_];
      debug_tab(1);
      dbg_skip_ = true;  // the b itself comes as text too
      dbg_msg_ = "Write what went wrong, then press Enter.";
    } else if (k == SDLK_U) { st = 0; save_tests(); }
    return;
  }
  if (dy) { sel_ = (sel_ + dy + NTOOLS) % NTOOLS; return; }
  if (enter || k == SDLK_SPACE) debug_tool(std::clamp(sel_, 0, NTOOLS - 1));
}

void Client::draw_debug() {
  int cw = con_.get_width(), chh = con_.get_height();
  int w = std::min(cw - 2, 84), h = std::min(chh - 2, 46);
  int x = (cw - w) / 2, y = (chh - h) / 2;
  box(x, y, w, h, "Debug and testing");
  text(x + w - 4, y, "[x]", KEY);
  buttons_.push_back({x + w - 4, y, 3, 1, "", "close"});
  static const char* const TABS[] = {"Tests", "Report a bug", "Tools"};
  int tx = x + 2;
  for (int i = 0; i < 3; ++i) {
    std::string l = "[" + std::to_string(i + 1) + "] " + TABS[i];
    text(tx, y + 2, l, i == dbg_tab_ ? GOLD : GREY, i == dbg_tab_ ? 0x302818 : -1);
    buttons_.push_back({tx, y + 2, int(l.size()), 1, "", "key:" + std::to_string(SDLK_F2 + i)});
    tx += int(l.size()) + 3;
  }
  int top = y + 4, bottom = y + h - 4;  // the rows in between, then a message line and the keys
  if (!dbg_msg_.empty()) wrap(x + 2, y + h - 4, w - 4, dbg_msg_, INFO, 2);

  if (dbg_tab_ == 0) {
    auto items = test_items();
    int n = int(items.size()), ok = 0, bug = 0;
    for (auto& it : items) { auto f = tests_.find(it); if (f != tests_.end()) { ok += f->second == 1; bug += f->second == 2; } }
    text(x + 2, top, std::to_string(ok) + " ok, " + std::to_string(bug) + " with a bug, " + std::to_string(n - ok - bug) + " still to test", GREY);
    int rows = bottom - top - 2;
    sel_ = std::clamp(sel_, 0, n - 1);
    if (sel_ < dbg_scroll_) dbg_scroll_ = sel_;
    if (sel_ >= dbg_scroll_ + rows) dbg_scroll_ = sel_ - rows + 1;
    dbg_scroll_ = std::clamp(dbg_scroll_, 0, std::max(0, n - rows));
    for (int r = 0; r < rows && dbg_scroll_ + r < n; ++r) {
      int i = dbg_scroll_ + r, ry = top + 2 + r;
      auto f = tests_.find(items[i]);
      int st = f == tests_.end() ? 0 : f->second;
      bool on = i == sel_;
      if (on) for (int c = x + 1; c < x + w - 1; ++c) put(c, ry, ' ', WHITE, 0x282830);
      text(x + 2, ry, st == 1 ? "[ok] " : st == 2 ? "[bug]" : "[   ]", st == 1 ? OK_COL : st == 2 ? BUG_COL : DIM, on ? 0x282830 : -1);
      std::string l = items[i];
      if (int(l.size()) > w - 11) l = l.substr(0, w - 12) + "~";
      text(x + 8, ry, l, on ? WHITE : st == 1 ? GREY : WHITE, on ? 0x282830 : -1);
      buttons_.push_back({x + 1, ry, w - 2, 1, "", "sel:" + std::to_string(i)});
    }
    if (dbg_scroll_ > 0) text(x + w - 3, top + 2, "^", DIM);
    if (dbg_scroll_ + rows < n) text(x + w - 3, top + 1 + rows, "v", DIM);
    text(x + 2, y + h - 2, "[Enter] untested > ok > bug  [b] report a bug about it  [Tab] next  [Esc] close", DIM);
  } else if (dbg_tab_ == 1) {
    int r = top;
    if (!note_test_.empty()) r += wrap(x + 2, r, w - 4, "About the test: " + note_test_, GOLD, 2) + 1;
    text(x + 2, r++, "This is saved with the report:", GREY);
    std::istringstream ctx(bug_context());
    std::string line;
    while (std::getline(ctx, line) && r < top + 16) r += wrap(x + 4, r, w - 6, line, DIM, 2);
    r += 1;
    text(x + 2, r++, "What went wrong? (type it here)", WHITE);
    int nb = std::max(3, bottom - r - 3);
    for (int j = 0; j < nb; ++j) for (int c = x + 2; c < x + w - 2; ++c) put(c, r + j, ' ', WHITE, 0x1C1C24);
    bool blink = int(now() * 2) % 2 == 0;
    wrap(x + 3, r, w - 6, note_ + (blink ? "_" : " "), WHITE, nb);
    buttons_.push_back({x + 2, r, w - 4, nb, "", "type"});
    r += nb + 1;
    std::string save = "[Enter] save the bug and copy it";
    text(x + 2, r, save, KEY);
    buttons_.push_back({x + 2, r, int(save.size()), 1, "", "key:" + std::to_string(SDLK_RETURN)});
    text(x + 2, y + h - 2, "Saved in bugs.txt in: " + pref_dir(), DIM);
  } else {
    for (int i = 0; i < NTOOLS; ++i) {
      int ry = top + i * 2;
      bool on = i == sel_;
      if (on) for (int c = x + 1; c < x + w - 1; ++c) put(c, ry, ' ', WHITE, 0x282830);
      text(x + 2, ry, TOOLS[i][1], on ? WHITE : GREY, on ? 0x282830 : -1);
      buttons_.push_back({x + 1, ry, w - 2, 1, "", "sel:" + std::to_string(i)});
    }
    if (snap_.is_object()) text(x + 2, top + NTOOLS * 2 + 1, std::string("Harm: ") + (snap_.value("god", false) ? "off" : "on") + "   Seed: " + snap_.value("seed", "?"), DIM);
    wrap(x + 2, top + NTOOLS * 2 + 3, w - 4, "Reports and screenshots: " + pref_dir(), DIM, 2);
    text(x + 2, y + h - 2, "[Enter] use  [Tab] next  [Esc] close", DIM);
  }
}
