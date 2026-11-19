#include "client.hpp"

#include <numbers>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <random>
#include <cctype>

#include "data.hpp"
#include "fx.hpp"
#include "lore.hpp"

namespace {
// the places to listen to, in the order of the map, and their music
struct ListenPlace { const char* name; music::Mood mood; bool foe; };
const ListenPlace LISTEN[] = {
    {"Charnfen", music::TITLE, false},       {"The Crypt", music::CRYPT, true},   {"The Caves", music::CAVES, true},
    {"The Cistern", music::CISTERN, true},   {"The Forge", music::FORGE, true},   {"The City", music::CITY, true},
    {"The Vaults", music::VAULTS, true},     {"The Barracks", music::BARRACKS, true}, {"The Prison", music::PRISON, true},
    {"The Sanctum", music::SANCTUM, true},   {"The Deep", music::DEEP, true},     {"A waystation", music::REST, false},
};
const int LISTEN_N = int(std::size(LISTEN)), LISTEN_ROWS = (LISTEN_N + 1) / 2;
bool listen_foe(int i) { return LISTEN[std::clamp(i, 0, LISTEN_N - 1)].foe; }

const int WIDE_W = 100, WIDE_H = 50;
const int VIEW_W = 76, VIEW_H = 42;  // the map's window in the wide layout
const int GREY = 0xA0A0A0, DIM = 0x606060, WHITE = 0xE8E8E8, GOLD = 0xFFD27F, KEY = 0xFFC860, RED = 0xD04040, BLUE = 0x4070E0;
const int PANEL = 0x101014;

TCOD_ColorRGB c3(int c) { return {uint8_t((c >> 16) & 255), uint8_t((c >> 8) & 255), uint8_t(c & 255)}; }
int scale(int c, double f) {
  auto ch = [&](int v) { return std::clamp(int(v * f), 0, 255); };
  return (ch((c >> 16) & 255) << 16) | (ch((c >> 8) & 255) << 8) | ch(c & 255);
}
int mixc(int a, int b, double f) {
  auto m = [&](int s) { int x = (a >> s) & 255, y = (b >> s) & 255; return std::clamp(int(x + (y - x) * f), 0, 255) << s; };
  return m(16) | m(8) | m(0);
}
int sgn(int v) { return (v > 0) - (v < 0); }

// key name like [q], drawn as a gold keycap in the help and tips
const std::vector<std::pair<std::string, std::string>> HELP = {
    {"", "Pyreheart. Go down sixteen floors under Charnfen, past the crypt, the caves and the forge, and tear the Pyreheart out of the Ashen King."},
    {"", ""},
    {"[arrows] [numpad]", "walk; walk into a foe to strike it. Diagonals: numpad, or [Home] [PgUp] [End] [PgDn]."},
    {"[.] [numpad 5]", "wait a moment."},
    {"[1] [2] [3] [4]", "your skills: two at the start, the third at level 3, the fourth at level 6. A skill that needs a target lets you aim: [Tab] to the next foe, [Enter] to let go."},
    {"[f]", "shoot with a bow."},
    {"[q] [w]", "drink a healing or a mana potion."},
    {"[i]", "your pack: [Enter] wear or use, [d] put down (anyone may take it), [t] give to the hero next to you."},
    {"[p]", "choose a perk when you have gone up a level."},
    {"[>]", "go down the stairs. With friends, the floor changes when all of you stand on the stairs."},
    {"[l]", "look at what you see."},
    {"[Esc]", "close a window; the game menu."},
    {"[F11]", "full screen and back; the window remembers its size for the next time."},
    {"", ""},
    {"", "Blue foes are champions, gold ones are uniques: stronger, and they carry better things."},
    {"", "Every fourth floor a great foe waits by the way down. After it comes a waystation with a fountain and a trader."},
    {"", "The dead wake again at the next waystation, if their friends get there."},
    {"", "Things you find are yours alone; what you put down, anyone may take."},
    {"", "In your pack and at the trader, a green \u25b2 is better than what you wear, a red \u25bc worse, and ~ a trade."},
    {"", ""},
    {"", "Great foes show their heavy blows before they land, as a count on the ground. Red: step off it. Purple: a spell; stun the foe or hurt it hard to break it. Gold: the only safe ground, so stand in it."},
    {"", "After its greatest blow a foe is spent for a moment and takes more harm. It fights harder at two thirds and at one third of its life."},
};
}  // namespace

Client::Client(tcod::Context& ctx) : ctx_(ctx), con_(WIDE_W, WIDE_H) {
  intro_start_ = now();
  load_settings();
  pick_layout();
  // the watch has less to spare: half the samples
  music_.volume(music_on_ ? .7f : 0);
  music_.open(layout_ == Layout::Watch ? 22050 : 44100);
}

double Client::now() const {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void Client::pick_layout() {
  int pw = 0, ph = 0;
  SDL_GetWindowSizeInPixels(ctx_.get_sdl_window(), &pw, &ph);
  if (pw <= 0 || ph <= 0) { pw = 800; ph = 500; }
  const char* force = SDL_getenv("PYREHEART_LAYOUT");
  std::string f = force ? force : "";
  if (f == "watch" || (f.empty() && std::min(pw, ph) < 700 && std::abs(pw - ph) < 80)) {
    layout_ = Layout::Watch;
    con_ = tcod::Console(19, 19);
  } else if (f == "tall" || (f.empty() && ph > pw)) {
    layout_ = Layout::Tall;
    int cols = 40;
    int rows = std::max(30, ph * cols / std::max(1, pw));
    con_ = tcod::Console(cols, rows);
  } else {
    layout_ = Layout::Wide;
    con_ = SDL_getenv("PYREHEART_WHOLE") ? tcod::Console(W, H) : tcod::Console(WIDE_W, WIDE_H);
  }
}

static std::string pref_file() {
  char* p = SDL_GetPrefPath("NilioDev", "Pyreheart");
  std::string s = p ? std::string(p) + "settings.txt" : "settings.txt";
  SDL_free(p);
  return s;
}
void Client::load_settings() {
  std::ifstream f(pref_file());
  std::string line;
  while (std::getline(f, line)) {
    if (line.rfind("name=", 0) == 0) name_ = line.substr(5);
    if (line.rfind("addr=", 0) == 0) addr_ = line.substr(5);
    if (line.rfind("class=", 0) == 0) cls_ = std::clamp(std::atoi(line.c_str() + 6), 0, int(data::CLASSES.size()) - 1);
    if (line.rfind("music=", 0) == 0) music_on_ = line[6] != '0';
    if (line.rfind("window=", 0) == 0) std::sscanf(line.c_str(), "window=%dx%d", &win_w_, &win_h_);
    if (line.rfind("fullscreen=", 0) == 0) fullscreen_ = line[11] == '1';
    if (line.rfind("bag_", 0) == 0 && line.find('=') != std::string::npos) {
      auto eq = line.find('=');
      auto& bag = bags_[line.substr(4, eq - 4)];
      for (size_t i = eq + 1; i < line.size();) {
        size_t j = line.find(',', i);
        if (j == std::string::npos) j = line.size();
        if (j > i) bag.push_back(std::atoi(line.substr(i, j - i).c_str()));
        i = j + 1;
      }
    }
  }
}
void Client::remember_window() {
  SDL_Window* w = ctx_.get_sdl_window();
  if (!w || layout_ != Layout::Wide) return;
  fullscreen_ = (SDL_GetWindowFlags(w) & SDL_WINDOW_FULLSCREEN) != 0;
  if (!fullscreen_) SDL_GetWindowSize(w, &win_w_, &win_h_);
  save_settings();
}

void Client::save_settings() {
  std::ofstream f(pref_file());
  f << "name=" << name_ << "\naddr=" << addr_ << "\nclass=" << cls_ << "\nmusic=" << (music_on_ ? 1 : 0) << "\n";
  if (layout_ == Layout::Wide) {
    if (win_w_ > 0) f << "window=" << win_w_ << "x" << win_h_ << "\n";
    f << "fullscreen=" << (fullscreen_ ? 1 : 0) << "\n";
  }
  for (auto& [k, bag] : bags_) {
    f << "bag_" << k << "=";
    for (size_t i = 0; i < bag.size(); ++i) f << (i ? "," : "") << bag[i];
    f << "\n";
  }
}

void Client::queue_epigraph(const std::string& zone) {
  auto it = lore::EPIGRAPHS.find(zone);
  if (it == lore::EPIGRAPHS.end() || it->second.empty()) return;
  int n = int(it->second.size());
  auto& bag = bags_[zone];  // the ones already shown since the bag was last shaken
  std::vector<int> left;
  for (int i = 0; i < n; ++i) if (std::find(bag.begin(), bag.end(), i) == bag.end()) left.push_back(i);
  if (left.empty()) {  // all have been read: shake the bag, but never the same one twice running
    int last = bag.empty() ? -1 : bag.back();
    bag.clear();
    for (int i = 0; i < n; ++i) if (i != last) left.push_back(i);
  }
  static std::mt19937 pick(uint32_t(std::chrono::steady_clock::now().time_since_epoch().count()));
  int k = left[pick() % left.size()];
  bag.push_back(k);
  save_settings();
  std::string title = "CHARNFEN";
  int col = 0xD2B48C;
  for (int i = 0; i < data::ZONES; ++i)
    if (data::ACTS[i].key == zone) {
      title = data::ACTS[i].name;
      for (auto& c : title) c = char(std::toupper(static_cast<unsigned char>(c)));
      col = data::ACTS[i].wall;
    }
  epis_.push_back({title, it->second[k].text, it->second[k].source, col});
  if (over_ != Over::Epigraph) { over_ = Over::Epigraph; epi_start_ = now(); }
}

static std::string save_file() {
  char* p = SDL_GetPrefPath("NilioDev", "Pyreheart");
  std::string s = p ? std::string(p) + "dive.jsonl" : "dive.jsonl";
  SDL_free(p);
  return s;
}

std::vector<std::string> Client::title_items() const {
  std::vector<std::string> items;
  if (saved_dive(save_file())) items.push_back("Go on with the dive");
  for (auto s : {"Dive alone", "Host a game for friends", "Join a game", "How to play", "Listen to the music"}) items.push_back(s);
  items.push_back(music_on_ ? "Music: on" : "Music: off");
  items.push_back("Quit");
  return items;
}

int Client::title_index(const std::string& item) const {
  auto items = title_items();
  for (int i = 0; i < int(items.size()); ++i) if (items[i] == item) return i;
  return 0;
}

// ------------------------------------------------------------------ the link
void Client::start(bool host, bool open, bool resume) {
  hosting_ = host;
  open_ = open;
  // a dive alone is saved after every move, so it goes on after the app is closed or killed
  host_ = std::make_unique<Host>(uint64_t(std::chrono::steady_clock::now().time_since_epoch().count()), open,
                                 open ? "" : save_file(), resume);
  link_ = host_->local();
  link_->send({{"hello", {{"name", name_}, {"cls", data::CLASSES[cls_].key}, {"ver", NET_VERSION}}}});
  screen_ = Screen::Game;
  maps_.clear(); seen_.clear(); log_.clear(); fx_.clear(); snap_ = json(); pid_ = 0;
  if (open) {
    std::string a = host_->address();
    log_.push_back({"Your friends can join you at " + (a.empty() ? std::string("this computer's address") : a) + ", port 7777.", GOLD});
    log_.push_back({"Over the internet, port 7777 (TCP and UDP) must be opened to this computer on the router.", GREY});
  }
}

void Client::join(const std::string& address) {
  std::string host = address;
  int port = PORT;
  auto c = host.rfind(':');
  if (c != std::string::npos && host.find(':') == c) { port = std::atoi(host.c_str() + c + 1); host = host.substr(0, c); }
  std::string err;
  link_ = connect_to(host, port, err);
  if (!link_) { message_ = err; screen_ = Screen::Title; return; }
  hosting_ = false;
  host_.reset();
  link_->send({{"hello", {{"name", name_}, {"cls", data::CLASSES[cls_].key}, {"ver", NET_VERSION}}}});
  screen_ = Screen::Game;
  maps_.clear(); seen_.clear(); log_.clear(); fx_.clear(); snap_ = json(); pid_ = 0;
}

void Client::poll() {
  if (!link_) return;
  while (auto m = link_->poll()) {
    if (m->contains("welcome")) pid_ = (*m)["welcome"];
    else if (m->contains("snap")) apply((*m)["snap"]);
    else if (m->contains("ready")) ready_ = (*m)["ready"].get<std::vector<int>>();
    else if (m->contains("error")) { message_ = (*m)["error"]; link_.reset(); host_.reset(); screen_ = Screen::Title; return; }
  }
  if (link_ && link_->closed() && screen_ == Screen::Game) {
    message_ = "The host has gone.";
    link_.reset(); host_.reset();
    screen_ = Screen::Title;
  }
}

void Client::apply(const json& s) {
  int fl = s["floor"];
  if (s.contains("map")) maps_[fl] = s["map"];
  auto& seen = seen_[fl];
  if (seen.empty()) seen.assign(W * H, 0);
  const std::string& vis = s["vis"].get_ref<const std::string&>();
  for (int i = 0; i < W * H; ++i) if (vis[i] == '1') seen[i] = 1;
  if (s.contains("seen")) {
    const std::string& sn = s["seen"].get_ref<const std::string&>();
    for (int i = 0; i < W * H && i < int(sn.size()); ++i) if (sn[i] == '1') seen[i] = 1;
  }
  for (auto& l : s["log"]) log_.push_back({l[0], l[1]});
  while (log_.size() > 300) log_.pop_front();
  double t = now();
  for (auto& f : s["fx"]) fx_.push_back({f, t});
  if (s["turn"].get<int>() != sent_turn_) { waiting_ = false; ready_.clear(); }
  bool was_shopping = snap_.is_object() && snap_["me"].value("shopping", false);
  if (fl != epi_floor_ && !s.value("rest", false) && layout_ != Layout::Watch && !SDL_getenv("PYREHEART_WHOLE")) {
    // the first map of a zone opens with one of its book fragments; the dive opens with Charnfen's
    bool first = epi_floor_ < 0;
    epi_floor_ = fl;
    int act = s.value("act", 0), zf = s.value("zfloor", 0);
    bool taken_up = first && s["turn"].get<int>() > 0;  // a dive taken up again: no fragment
    if (!taken_up) {
      if (s.value("depth", 1) == 1) {
        queue_epigraph("charnfen");
        if (data::ACTS[act].floors == 1) queue_epigraph(data::ACTS[act].key);
      } else if (zf == 0 || (act == 0 && zf == 1)) queue_epigraph(data::ACTS[act].key);
    }
  }
  snap_ = s;
  bool shopping = s["me"].value("shopping", false);
  if (shopping && !was_shopping) { over_ = Over::Shop; sel_ = 0; }
  if (!shopping && over_ == Over::Shop) over_ = Over::None;
  if (s["me"].value("dead", false) && over_ == Over::Target) over_ = Over::None;
  if (layout_ == Layout::Watch && !s["fx"].empty()) {
    for (auto& f : s["fx"])  // a short buzz when you are hurt
      if (f.value("k", "") == "blood" && f.value("x", -1) == s["me"].value("x", 0) && f.value("y", -1) == s["me"].value("y", 0)) {
        // vibration is asked of the system by the Android side; desktop has none
        SDL_Event e{};
        e.type = SDL_EVENT_USER;
        SDL_PushEvent(&e);
        break;
      }
  }
}

void Client::send(const json& a) {
  if (!link_ || !snap_.is_object()) return;
  std::string t = a.value("t", "");
  bool free = t == "perk" || t == "buy" || t == "buyp" || t == "sell" || t == "close";
  if (!free) {
    if (waiting_) return;
    waiting_ = true;
    sent_turn_ = snap_["turn"];
  }
  link_->send({{"act", a}});
}

std::vector<json> Client::foes() const {
  std::vector<json> out;
  if (!snap_.is_object()) return out;
  int x = snap_["me"]["x"], y = snap_["me"]["y"];
  for (auto& e : snap_["ents"]) if (e.value("rank", "") != "npc") out.push_back(e);
  std::sort(out.begin(), out.end(), [&](const json& a, const json& b) {
    return cheb(a["x"], a["y"], x, y) < cheb(b["x"], b["y"], x, y);
  });
  return out;
}

void Client::begin_aim(const std::string& what) {
  auto fs = foes();
  if (what != "look" && fs.empty()) { log_.push_back({"Nothing to aim at.", GREY}); return; }
  aim_ = what;
  over_ = Over::Target;
  if (!fs.empty()) { tx_ = fs[0]["x"]; ty_ = fs[0]["y"]; }
  else { tx_ = snap_["me"]["x"]; ty_ = snap_["me"]["y"]; }
}

void Client::cycle_target(int dir) {
  auto fs = foes();
  if (fs.empty()) return;
  int i = 0;
  for (size_t k = 0; k < fs.size(); ++k) if (fs[k]["x"] == tx_ && fs[k]["y"] == ty_) i = int(k);
  i = (i + dir + int(fs.size())) % int(fs.size());
  tx_ = fs[i]["x"]; ty_ = fs[i]["y"];
}

// One action by name: the same for a key, a button on the phone and the ring on the watch.
void Client::act(const std::string& what) {
  if (!snap_.is_object()) return;
  auto& me = snap_["me"];
  if (what == "wait") send({{"t", "wait"}});
  else if (what == "heal") send({{"t", "quaff"}, {"k", "heal"}});
  else if (what == "mana") send({{"t", "quaff"}, {"k", "mana"}});
  else if (what == "down") send({{"t", "down"}});
  else if (what == "pack") { over_ = Over::Pack; sel_ = 0; }
  else if (what == "perk") { if (!me["choice"].is_null()) { over_ = Over::Perk; sel_ = 0; } else log_.push_back({"No perk to choose now.", GREY}); }
  else if (what == "help") over_ = Over::Help;
  else if (what == "look") begin_aim("look");
  else if (what == "shoot") { if (me.value("bow", false)) begin_aim("shoot"); else log_.push_back({"You have no bow.", GREY}); }
  else if (what.rfind("skill", 0) == 0) {
    int n = what.back() - '0';
    if (n < 1 || n > int(me["skills"].size())) return;
    auto& sk = me["skills"][n - 1];
    if (me["lvl"].get<int>() < sk[4].get<int>()) { log_.push_back({sk[0].get<std::string>() + " comes to you at level " + std::to_string(sk[4].get<int>()) + ".", GREY}); return; }
    bool fury = me.value("pool", "mana") == "fury";
    if (me["mana"].get<int>() < sk[1].get<int>()) { log_.push_back({std::string(fury ? "Not enough fury" : "Not enough mana") + " for " + sk[0].get<std::string>() + ".", 0x7090FF}); return; }
    if (sk[2].get<bool>()) begin_aim(what);
    else send({{"t", "skill"}, {"n", n}});
  } else if (what == "menu") over_ = Over::Menu;
}

// ------------------------------------------------------------------ input
void Client::event(SDL_Event& ev) {
  if (ev.type == SDL_EVENT_QUIT) { running = false; return; }
  if (ev.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) { pick_layout(); return; }
  if (ev.type == SDL_EVENT_KEY_DOWN) {
    // [F11] or [Alt+Enter]: full screen and back, anywhere
    bool alt_enter = (ev.key.key == SDLK_RETURN || ev.key.key == SDLK_KP_ENTER) && (ev.key.mod & SDL_KMOD_ALT);
    if (ev.key.key == SDLK_F11 || alt_enter) {
      SDL_Window* w = ctx_.get_sdl_window();
      if (w) SDL_SetWindowFullscreen(w, (SDL_GetWindowFlags(w) & SDL_WINDOW_FULLSCREEN) == 0);
      return;
    }
    key(ev.key.key, ev.key.mod);
    return;
  }
  if (ev.type == SDL_EVENT_TEXT_INPUT) {
    std::string t = ev.text.text;
    if (screen_ == Screen::Name && name_.size() < 16) name_ += t;
    if (screen_ == Screen::Join && addr_.size() < 60) addr_ += t;
    return;
  }
  if (ev.type == SDL_EVENT_MOUSE_WHEEL) {  // the watch crown, or a mouse wheel
    int d = ev.wheel.y > 0 ? -1 : ev.wheel.y < 0 ? 1 : 0;
    if (!d) return;
    if (screen_ == Screen::Intro) { screen_ = Screen::Title; menu_ = 0; return; }
    if (over_ == Over::Ring) ring_ = (ring_ + d + 11) % 11;
    else if (over_ == Over::Target) cycle_target(d);
    else if (over_ == Over::Pack || over_ == Over::Shop || over_ == Over::Perk) sel_ = std::max(0, sel_ + d);
    else if (screen_ != Screen::Game) menu_ = std::max(0, menu_ + d);
    else if (layout_ == Layout::Watch) { over_ = Over::Ring; ring_ = 0; }
    return;
  }
  if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev.type == SDL_EVENT_MOUSE_BUTTON_UP) {
    SDL_Event e = ev;
    ctx_.convert_event_coordinates(e);
    click(int(e.button.x), int(e.button.y), ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
    return;
  }
  if (ev.type == SDL_EVENT_MOUSE_MOTION && held_) {
    SDL_Event e = ev;
    ctx_.convert_event_coordinates(e);
    hold_x_ = int(e.motion.x); hold_y_ = int(e.motion.y);
  }
}

void Client::key(SDL_Keycode k, SDL_Keymod mod) {
  if (k == SDLK_AC_BACK) k = SDLK_ESCAPE;  // the back button or swipe on a phone or watch
  if (screen_ == Screen::Intro) { screen_ = Screen::Title; menu_ = 0; return; }
  bool shift = mod & SDL_KMOD_SHIFT;
  if (screen_ == Screen::Name || screen_ == Screen::Join) {
    std::string& s = screen_ == Screen::Name ? name_ : addr_;
    if (k == SDLK_BACKSPACE && !s.empty()) s.pop_back();
    else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
      SDL_StopTextInput(ctx_.get_sdl_window());
      if (screen_ == Screen::Name) { if (name_.empty()) name_ = "Hero"; screen_ = Screen::Class; }
      else { save_settings(); join(addr_); }
    } else if (k == SDLK_ESCAPE) { SDL_StopTextInput(ctx_.get_sdl_window()); screen_ = Screen::Title; }
    return;
  }
  if (screen_ == Screen::Listen) {
    if (k == SDLK_F) { listen_fight_ = !listen_fight_; return; }
    if (k == SDLK_N) { music_.renew(); return; }
    if (layout_ != Layout::Watch && (k == SDLK_LEFT || k == SDLK_RIGHT || k == SDLK_KP_4 || k == SDLK_KP_6)) {
      menu_ = (menu_ + LISTEN_ROWS) % (2 * LISTEN_ROWS);  // to the other column
      if (menu_ >= LISTEN_N) menu_ -= LISTEN_ROWS;
      return;
    }
    if (k == SDLK_ESCAPE) { screen_ = Screen::Title; menu_ = title_index("Listen to the music"); return; }
  }
  if (screen_ == Screen::Title || screen_ == Screen::Class || screen_ == Screen::Finding || screen_ == Screen::Listen) {
    int n = screen_ == Screen::Title ? int(title_items().size()) : screen_ == Screen::Class ? int(data::CLASSES.size()) : screen_ == Screen::Listen ? LISTEN_N : int(found_.size()) + 1;
    if (k == SDLK_UP || k == SDLK_KP_8) menu_ = (menu_ + n - 1) % n;
    else if (k == SDLK_DOWN || k == SDLK_KP_2) menu_ = (menu_ + 1) % n;
    else if (k == SDLK_ESCAPE) { if (screen_ == Screen::Title) running = false; else { screen_ = Screen::Title; menu_ = 0; } }
    else if (screen_ == Screen::Listen && (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE)) music_.renew();
    else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) click(-1000, menu_, true);
    return;
  }
  if (!snap_.is_object()) { if (k == SDLK_ESCAPE) over_ = Over::Menu; return; }
  std::string state = snap_.value("state", "play");
  if (state != "play") {
    if (k == SDLK_RETURN || k == SDLK_ESCAPE || k == SDLK_KP_ENTER) { link_.reset(); host_.reset(); screen_ = Screen::Title; menu_ = 0; snap_ = json(); }
    return;
  }
  auto& me = snap_["me"];
  int dx = 0, dy = 0;
  switch (k) {
    case SDLK_UP: case SDLK_KP_8: dy = -1; break;
    case SDLK_DOWN: case SDLK_KP_2: dy = 1; break;
    case SDLK_LEFT: case SDLK_KP_4: dx = -1; break;
    case SDLK_RIGHT: case SDLK_KP_6: dx = 1; break;
    case SDLK_HOME: case SDLK_KP_7: dx = -1; dy = -1; break;
    case SDLK_PAGEUP: case SDLK_KP_9: dx = 1; dy = -1; break;
    case SDLK_END: case SDLK_KP_1: dx = -1; dy = 1; break;
    case SDLK_PAGEDOWN: case SDLK_KP_3: dx = 1; dy = 1; break;
    default: break;
  }
  bool enter = k == SDLK_RETURN || k == SDLK_KP_ENTER;
  switch (over_) {
    case Over::Epigraph:
      // the first key shows the whole text at once, the next closes it
      if (!epis_.empty()) {
        double all = (std::count(epis_.front().text.begin(), epis_.front().text.end(), ' ') + 2) / 12.0;
        if (now() - epi_start_ < all) { epi_start_ = now() - all; return; }
      }
      if (!epis_.empty()) epis_.pop_front();
      if (epis_.empty()) over_ = Over::None; else epi_start_ = now();
      return;
    case Over::Help:
      over_ = Over::None;
      return;
    case Over::Menu:
      if (k == SDLK_ESCAPE) over_ = Over::None;
      else if (k == SDLK_M) { music_on_ = !music_on_; music_.volume(music_on_ ? .7f : 0); save_settings(); }
      else if (k == SDLK_Q || enter) { link_.reset(); host_.reset(); screen_ = Screen::Title; over_ = Over::None; menu_ = 0; snap_ = json(); }
      return;
    case Over::Target: {
      if (k == SDLK_ESCAPE) { over_ = Over::None; return; }
      if (k == SDLK_TAB) { cycle_target(shift ? -1 : 1); return; }
      if (dx || dy) { tx_ = std::clamp(tx_ + dx, 0, W - 1); ty_ = std::clamp(ty_ + dy, 0, H - 1); return; }
      bool fire = enter || k == SDLK_F || (aim_.rfind("skill", 0) == 0 && k == SDLK_1 + (aim_.back() - '1'));
      if (fire && aim_ != "look") {
        over_ = Over::None;
        if (aim_ == "shoot") send({{"t", "shoot"}, {"x", tx_}, {"y", ty_}});
        else send({{"t", "skill"}, {"n", aim_.back() - '0'}, {"x", tx_}, {"y", ty_}});
      } else if (enter || k == SDLK_L) over_ = Over::None;
      return;
    }
    case Over::Perk: {
      auto& ch = me["choice"];
      if (ch.is_null()) { over_ = Over::None; return; }
      int n = int(ch.size());
      if (k == SDLK_ESCAPE) over_ = Over::None;
      else if (dy) sel_ = (sel_ + dy + n) % n;
      else if (k >= SDLK_1 && k < SDLK_1 + n) { send({{"t", "perk"}, {"k", ch[k - SDLK_1][0]}}); over_ = Over::None; }
      else if (enter) { send({{"t", "perk"}, {"k", ch[std::clamp(sel_, 0, n - 1)][0]}}); over_ = Over::None; }
      return;
    }
    case Over::Pack: case Over::Shop: {
      auto& pack = me["pack"];
      int np = int(pack.size());
      int ns = over_ == Over::Shop && snap_.contains("shop") ? int(snap_["shop"]["items"].size()) : 0;
      int n = np + ns;
      if (k == SDLK_ESCAPE || k == SDLK_I) { if (over_ == Over::Shop) send({{"t", "close"}}); over_ = Over::None; return; }
      if (dy && n) { sel_ = (sel_ + dy + n) % n; return; }
      if (over_ == Over::Shop && k == SDLK_H) { send({{"t", "buyp"}, {"k", "heal"}}); return; }
      if (over_ == Over::Shop && k == SDLK_M) { send({{"t", "buyp"}, {"k", "mana"}}); return; }
      if (!n) return;
      sel_ = std::clamp(sel_, 0, n - 1);
      if (sel_ < ns) { if (enter) send({{"t", "buy"}, {"i", sel_}}); return; }
      auto& it = pack[sel_ - ns];
      if (over_ == Over::Shop && (k == SDLK_S || enter)) { send({{"t", "sell"}, {"id", it["id"]}}); return; }
      if (enter) send({{"t", "equip"}, {"id", it["id"]}});
      else if (k == SDLK_D) send({{"t", "drop"}, {"id", it["id"]}});
      else if (k == SDLK_T) send({{"t", "give"}, {"id", it["id"]}});
      return;
    }
    case Over::Ring:
      if (k == SDLK_ESCAPE) over_ = Over::None;
      return;
    case Over::None: break;
  }
  if (k == SDLK_ESCAPE) { over_ = Over::Menu; return; }
  if (me.value("dead", false) || me.value("gone", false)) return;
  if (dx || dy) { send({{"t", "move"}, {"dx", dx}, {"dy", dy}}); return; }
  if ((k == SDLK_PERIOD && shift) || k == SDLK_GREATER) { act("down"); return; }
  if ((k == SDLK_SLASH && shift) || k == SDLK_QUESTION || k == SDLK_F1) { act("help"); return; }
  switch (k) {
    case SDLK_PERIOD: case SDLK_KP_5: case SDLK_SPACE: case SDLK_KP_PERIOD: act("wait"); break;
    case SDLK_1: act("skill1"); break;
    case SDLK_2: act("skill2"); break;
    case SDLK_3: act("skill3"); break;
    case SDLK_4: act("skill4"); break;
    case SDLK_F: act("shoot"); break;
    case SDLK_Q: act("heal"); break;
    case SDLK_W: act("mana"); break;
    case SDLK_I: act("pack"); break;
    case SDLK_P: act("perk"); break;
    case SDLK_L: act("look"); break;
    case SDLK_RETURN: case SDLK_KP_ENTER: {
      int t = maps_.count(snap_["floor"]) ? maps_[snap_["floor"]][me["y"].get<int>() * W + me["x"].get<int>()] - TILE0 : -1;
      if (t == data::STAIRS || t == data::EDGE) act("down");
      break;
    }
    default: break;
  }
}

// A tap or a click. x == -1000 means "the menu line y was chosen" (keyboard on the menus).
void Client::click(int x, int y, bool down) {
  if (!down) { held_ = false; return; }
  if (screen_ == Screen::Intro) { screen_ = Screen::Title; menu_ = 0; return; }
  if (screen_ != Screen::Game) {
    int choice = x == -1000 ? y : -1;
    if (x != -1000) {  // which line was tapped
      int top = menu_top();
      int step = layout_ == Layout::Watch ? 2 : 2;
      choice = (y - top) / step;
      if ((y - top) < 0) choice = -1;
      else if (layout_ == Layout::Watch) choice += std::max(0, menu_ - 2);
      // the music's places stand in two columns: a tap on the right one counts from its first line
      else if (screen_ == Screen::Listen) choice = choice < LISTEN_ROWS ? choice + (x >= con_.get_width() / 2 ? LISTEN_ROWS : 0) : choice + LISTEN_ROWS;
    }
    if (screen_ == Screen::Title) {
      auto items = title_items();
      if (choice < 0 || choice >= int(items.size())) return;
      menu_ = choice;
      message_.clear();
      const std::string& it = items[choice];
      if (it == "Go on with the dive") start(true, false, true);
      else if (it == "Dive alone" || it == "Host a game for friends") { hosting_ = it != "Dive alone"; screen_ = Screen::Name; SDL_StartTextInput(ctx_.get_sdl_window()); }
      else if (it == "Join a game") { found_ = discover(700); screen_ = Screen::Finding; menu_ = 0; }
      else if (it == "How to play") over_ = Over::Help, screen_ = Screen::Title;
      else if (it == "Listen to the music") { screen_ = Screen::Listen; menu_ = 0; }
      else if (it.rfind("Music:", 0) == 0) { music_on_ = !music_on_; music_.volume(music_on_ ? .7f : 0); save_settings(); }
      else running = false;
    } else if (screen_ == Screen::Listen) {
      // a tap on a place plays it; a tap on the one playing writes a new piece; below the list, back
      if (choice >= 0 && choice < LISTEN_N) { if (choice == menu_) music_.renew(); menu_ = choice; }
      else if (choice == LISTEN_N + 1) listen_fight_ = !listen_fight_;  // the line "[f] a great foe awake"
      else if (choice > LISTEN_N + 1 || choice < 0) { screen_ = Screen::Title; menu_ = title_index("Listen to the music"); }
    } else if (screen_ == Screen::Class) {
      if (choice < 0 || choice >= int(data::CLASSES.size())) return;
      cls_ = choice;
      save_settings();
      if (pending_join_) { pending_join_ = false; join(addr_); }
      else start(true, hosting_);
      over_ = Over::None;
    } else if (screen_ == Screen::Finding) {
      if (choice < 0 || choice > int(found_.size())) return;
      if (choice < int(found_.size())) {
        addr_ = found_[choice].substr(found_[choice].find('|') + 1);
        pending_join_ = true;
        screen_ = Screen::Name;
        SDL_StartTextInput(ctx_.get_sdl_window());
      } else { pending_join_ = true; screen_ = Screen::Join; SDL_StartTextInput(ctx_.get_sdl_window()); }
      menu_ = 0;
    } else if (screen_ == Screen::Name) {
      SDL_StartTextInput(ctx_.get_sdl_window());
    }
    return;
  }
  if (!snap_.is_object()) return;
  if (snap_.value("state", "play") != "play") { link_.reset(); host_.reset(); screen_ = Screen::Title; menu_ = 0; snap_ = json(); return; }
  for (auto& b : buttons_)
    if (x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h) {
      if (b.action.rfind("sel:", 0) == 0) { sel_ = std::atoi(b.action.c_str() + 4); key(SDLK_RETURN, SDL_KMOD_NONE); }
      else if (b.action.rfind("key:", 0) == 0) key(SDL_Keycode(std::atoi(b.action.c_str() + 4)), SDL_KMOD_NONE);
      else if (b.action == "close") key(SDLK_ESCAPE, SDL_KMOD_NONE);
      else { over_ = Over::None; act(b.action); }
      return;
    }
  if (over_ == Over::Help || over_ == Over::Menu) { over_ = Over::None; return; }
  if (over_ == Over::Epigraph) { key(SDLK_SPACE, SDL_KMOD_NONE); return; }
  if (over_ == Over::Pack || over_ == Over::Shop || over_ == Over::Perk) return;
  // a tap on the map
  int mx = x - vx_ + camx_, my = y - vy_ + camy_;
  if (x < vx_ || y < vy_ || x >= vx_ + vw_ || y >= vy_ + vh_) return;
  auto& me = snap_["me"];
  int hx = me["x"], hy = me["y"];
  if (over_ == Over::Target) {
    if (mx == tx_ && my == ty_) key(SDLK_RETURN, SDL_KMOD_NONE);
    else { tx_ = mx; ty_ = my; }
    return;
  }
  if (over_ == Over::Ring) { over_ = Over::None; return; }
  if (mx == hx && my == hy) {
    if (layout_ == Layout::Watch) { over_ = Over::Ring; ring_ = 0; }
    else act("wait");
    return;
  }
  if (layout_ == Layout::Watch) {  // eight slices of the dial round the hero
    double ang = std::atan2(double(my - hy), double(mx - hx));
    int s = int(std::lround(ang / (std::numbers::pi / 4))) & 7;
    static const int D[8][2] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
    send({{"t", "move"}, {"dx", D[s][0]}, {"dy", D[s][1]}});
  } else {
    send({{"t", "move"}, {"dx", sgn(mx - hx)}, {"dy", sgn(my - hy)}});
  }
  held_ = true;
  held_since_ = last_step_ = now();
  hold_x_ = x; hold_y_ = y;
}

// ------------------------------------------------------------------ drawing helpers
void Client::put(int x, int y, int ch, int fg, int bg) {
  if (!con_.in_bounds({x, y})) return;
  auto& t = con_.at({x, y});
  t.ch = ch;
  t.fg = {uint8_t((fg >> 16) & 255), uint8_t((fg >> 8) & 255), uint8_t(fg & 255), 255};
  if (bg >= 0) t.bg = {uint8_t((bg >> 16) & 255), uint8_t((bg >> 8) & 255), uint8_t(bg & 255), 255};
}
// The one-glance mark in front of an item: is it better than what I wear?
static std::pair<std::string, int> mark(const json& it) {
  if (!it.contains("cmp")) return {" ", 0};
  int v = it["cmp"]["v"];
  switch (v) {
    case 2: return {"▲", 0x50D050};
    case 1: return {"▲", 0xB0C050};
    case -1: return {"▼", 0xC09050};
    case -2: return {"▼", 0xD05050};
    case -9: return {"×", 0x806060};
    default: return {it["cmp"]["d"].empty() ? "=" : "~", 0x909090};
  }
}

static std::string clip(const std::string& s, int n) { return int(s.size()) <= n ? s : s.substr(0, std::max(0, n - 1)) + "\u2026"; }
static std::string verdict_text(const json& c) {
  static const char* SAY[] = {"worse than what you wear", "mostly worse than what you wear", "some better, some worse",
                              "mostly better than what you wear", "better than what you wear"};
  int v = c["v"];
  if (v == -9) return "not for you";
  if (v == 0 && c["d"].empty()) return "no different from what you wear";
  return SAY[v + 2];
}

void Client::text(int x, int y, const std::string& s, int fg, int bg) {
  if (y < 0 || y >= con_.get_height()) return;
  tcod::print(con_, {x, y}, s, c3(fg), bg >= 0 ? std::optional<TCOD_ColorRGB>(c3(bg)) : std::nullopt);
}
// Text with [keys] drawn as gold keycaps, wrapped to w. Returns the lines used.
int Client::wrap(int x, int y, int w, const std::string& s, int fg, int max_lines) {
  std::vector<std::string> words;
  std::string cur;
  for (char c : s) { if (c == ' ') { words.push_back(cur); cur.clear(); } else cur += c; }
  words.push_back(cur);
  int line = 0, col = 0;
  auto len = [](const std::string& w2) { int n = 0; for (unsigned char c : w2) if ((c & 0xC0) != 0x80) ++n; return n; };
  for (auto& wd : words) {
    int l = len(wd);
    if (col > 0 && col + 1 + l > w) { ++line; col = 0; if (line >= max_lines) return line; }
    if (col > 0) ++col;
    bool k = !wd.empty() && wd[0] == '[';
    text(x + col, y + line, wd, k ? KEY : fg);
    col += l;
  }
  return line + 1;
}
void Client::box(int x, int y, int w, int h, const std::string& title) {
  for (int j = y; j < y + h; ++j) for (int i = x; i < x + w; ++i) put(i, j, ' ', WHITE, PANEL);
  for (int i = x; i < x + w; ++i) { put(i, y, 0x2500, DIM, PANEL); put(i, y + h - 1, 0x2500, DIM, PANEL); }
  for (int j = y; j < y + h; ++j) { put(x, j, 0x2502, DIM, PANEL); put(x + w - 1, j, 0x2502, DIM, PANEL); }
  put(x, y, 0x250C, DIM, PANEL); put(x + w - 1, y, 0x2510, DIM, PANEL);
  put(x, y + h - 1, 0x2514, DIM, PANEL); put(x + w - 1, y + h - 1, 0x2518, DIM, PANEL);
  if (!title.empty()) text(x + 2, y, " " + title + " ", GOLD, PANEL);
}
void Client::bar(int x, int y, int w, int val, int max, int col, const std::string& label) {
  int fill = max > 0 ? std::clamp(val * w / max, 0, w) : 0;
  for (int i = 0; i < w; ++i) put(x + i, y, ' ', WHITE, i < fill ? scale(col, .55) : scale(col, .15));
  text(x + 1, y, label, WHITE);
}
bool Client::in_circle(int x, int y) const {
  double c = (con_.get_width() - 1) / 2.0, r = con_.get_width() / 2.0;
  return (x - c) * (x - c) + (y - c) * (y - c) <= r * r;
}

// ------------------------------------------------------------------ the frame
// the music follows where you are: the title, an act, a waystation; a great foe awake makes it heavier
void Client::update_music() {
  if (screen_ == Screen::Listen) {
    music_.mood(LISTEN[std::clamp(menu_, 0, LISTEN_N - 1)].mood, listen_fight_ && listen_foe(menu_));
    music_.volume(.7f);  // to listen, the music plays even when it is off for the game
    return;
  }
  music_.volume(music_on_ ? .7f : 0);
  if (screen_ != Screen::Game || !snap_.is_object()) { music_.mood(music::TITLE); return; }
  if (snap_.value("rest", false)) { music_.mood(music::REST); return; }
  // each zone its own mood, in the order of ACTS
  namespace m = music;
  static const m::Mood zone[] = {m::CRYPT, m::CAVES, m::FORGE, m::DEEP, m::CISTERN, m::CITY, m::VAULTS, m::BARRACKS, m::PRISON, m::SANCTUM};
  static_assert(sizeof zone / sizeof zone[0] == data::ZONES);
  music_.mood(zone[std::clamp(snap_.value("act", 0), 0, data::ZONES - 1)], snap_.contains("boss"));
}

void Client::frame() {
  poll();
  update_music();
  if (held_ && screen_ == Screen::Game && over_ == Over::None && snap_.is_object()) {
    // holding a finger down keeps walking, until a foe comes into sight
    double t = now();
    if (t - held_since_ > .35 && t - last_step_ > .14 && !waiting_) {
      bool foe = false;
      for (auto& e : snap_["ents"]) if (e.value("rank", "") != "npc") foe = true;
      if (foe) held_ = false;
      else { last_step_ = t; click(hold_x_, hold_y_, true); held_since_ = t - 1; }
    }
  }
  con_.clear({0x20, {200, 200, 200, 255}, {0, 0, 0, 255}});
  buttons_.clear();
  if (screen_ == Screen::Game) draw_game();
  else draw_title();
  if (over_ == Over::Help) draw_help();
  TCOD_ViewportOptions vp{};
  vp.tcod_version = TCOD_COMPILEDVERSION;
  vp.keep_aspect = true;
  vp.integer_scaling = layout_ == Layout::Wide;
  vp.clear_color = {0, 0, 0, 255};
  vp.align_x = .5f; vp.align_y = .5f;
  present(vp);
}

int Client::menu_top() const {
  if (layout_ == Layout::Watch) return 6;
  if (layout_ == Layout::Tall) return 47;
  return 30;
}

// before the title: the story crawls up the whole screen, then the title comes. Any key or tap skips it.
void Client::draw_intro() {
  int cw = con_.get_width(), chh = con_.get_height();
  bool watch = layout_ == Layout::Watch;
  int w = watch ? 13 : std::min(cw - 4, 64);
  std::vector<std::pair<std::string, int>> lines;  // text, colour
  lines.push_back({"PYREHEART", 0xFF8C28});
  lines.push_back({"", 0});
  for (auto& para : lore::INTRO) {
    if (para.empty()) { lines.push_back({"", 0}); continue; }
    std::string line, word;
    for (size_t i = 0; i <= para.size(); ++i) {
      if (i == para.size() || para[i] == ' ') {
        if (!line.empty() && int(line.size() + 1 + word.size()) > w) { lines.push_back({line, 0xC8B48C}); line.clear(); }
        line += (line.empty() ? "" : " ") + word;
        word.clear();
      } else word += para[i];
    }
    if (!line.empty()) lines.push_back({line, 0xC8B48C});
  }
  // a blank row between lines, so the text breathes; it rises from under the screen to past its top
  const int gap = watch ? 1 : 2;
  double speed = watch ? 1.6 : 1.4;  // rows a second
  int pos = int((now() - intro_start_) * speed);
  int total = int(lines.size()) * gap + chh;
  if (pos >= total) { screen_ = Screen::Title; menu_ = 0; return; }
  for (int i = 0; i < int(lines.size()); ++i) {
    int y = chh - pos + i * gap;
    if (y < 0 || y >= chh || lines[i].first.empty()) continue;
    double edge = std::clamp(std::min(y + 1, chh - y) / (chh * .18), 0.0, 1.0);  // fade in at the foot, out at the top
    const std::string& t = lines[i].first;
    int x = (cw - int(t.size())) / 2;
    if (i == 0) {  // the name, letter by letter in fire colours
      for (int k = 0; k < int(t.size()); ++k)
        put(x + k, y, t[k], mixc(0x000000, mixc(0xC8B8A8, 0xFF7020, .5 + .5 * std::sin(now() * 2 + k * .7)), edge));
    } else text(x, y, t, mixc(0x000000, lines[i].second, edge));
  }
  if (!watch) {
    std::string hint = layout_ == Layout::Tall ? "tap to skip" : "[any key] skip";
    text(cw - int(hint.size()) - 2, chh - 1, hint, 0x504840);
  }
  if (watch) for (int y = 0; y < chh; ++y) for (int x = 0; x < cw; ++x) if (!in_circle(x, y)) put(x, y, ' ', 0, 0);
}

void Client::draw_title() {
  if (screen_ == Screen::Intro) { draw_intro(); return; }
  int cw = con_.get_width();
  int top = menu_top();
  auto centre = [&](int y, const std::string& s, int col) {
    int n = 0; for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
    text(std::max(0, (cw - n) / 2), y, s, col);
  };
  if (layout_ != Layout::Watch) {
    draw_logo(layout_ == Layout::Wide ? 2 : 2);
  } else {
    // the watch: the word in fire colours, embers rising behind it
    double t = now();
    const std::string word = "PYREHEART";
    for (int i = 0; i < int(word.size()); ++i) {
      double f = .5 + .5 * std::sin(t * 2 + i * .7);
      put(5 + i, 3, word[i], mixc(0xC8B8A8, 0xFF7020, f));
    }
    for (int i = 0; i < 6; ++i) {
      double age = std::fmod(t * (1 + i * .13) + i * .37, 2.0);
      put(4 + (i * 5) % 11, 3 - int(age * 1.2) + (i % 2), 0x00B7, mixc(0xFFB030, 0x501000, age / 2));
    }
  }
  std::vector<std::string> items;
  std::string title;
  if (screen_ == Screen::Title) items = title_items();
  else if (screen_ == Screen::Listen) {
    title = "The music writes itself: a new piece every time.";
    for (auto& p : LISTEN) items.push_back(p.name);
  }
  else if (screen_ == Screen::Class) {
    title = "Who are you, " + name_ + "?";
    for (auto& c : data::CLASSES) items.push_back(c.name);
  } else if (screen_ == Screen::Finding) {
    title = found_.empty() ? "No game found on this network." : "Games on this network:";
    for (auto& f : found_) items.push_back(f.substr(0, f.find('|')) + "  " + f.substr(f.find('|') + 1));
    items.push_back("Type an address...");
  } else if (screen_ == Screen::Name) {
    title = "Your name:";
    centre(top, name_ + "_", WHITE);
    centre(top + 2, "[Enter]", KEY);
  } else if (screen_ == Screen::Join) {
    title = "Address of the host (as they see it on their screen):";
    centre(top, addr_ + "_", WHITE);
    centre(top + 2, "[Enter]", KEY);
  }
  if (!title.empty()) centre(top - 2, title, GOLD);
  // the watch has room for five lines: they scroll with the choice
  int first = layout_ == Layout::Watch ? std::max(0, menu_ - 2) : 0;
  int last = layout_ == Layout::Watch ? std::min(int(items.size()), first + 5) : int(items.size());
  for (int i = first; i < last; ++i) {
    bool on = i == menu_;
    if (screen_ == Screen::Listen && layout_ != Layout::Watch) {  // two columns, the map's order down each
      std::string s = (on ? "> " : "  ") + items[i] + (on ? " <" : "  ");
      int cx = cw / 2 + (i < LISTEN_ROWS ? -1 : 1) * std::min(13, cw / 4);
      text(cx - int(s.size()) / 2, top + (i % LISTEN_ROWS) * 2, s, on ? WHITE : GREY);
      continue;
    }
    centre(top + (i - first) * 2, (on ? "> " : "  ") + items[i] + (on ? " <" : "  "), on ? WHITE : GREY);
  }
  if (screen_ == Screen::Listen && layout_ != Layout::Watch) {
    int y = top + 2 * LISTEN_ROWS + 1;
    centre(y, "Now: " + music_.describe(), 0xB4A078);
    if (listen_foe(menu_))
      centre(y + 2, std::string("[f] a great foe awake: ") + (listen_fight_ ? "yes" : "no"), listen_fight_ ? 0xFF8060 : GREY);
    centre(y + 4, "[Enter] a new piece    [Esc] back", 0x808080);
  }
  if (screen_ == Screen::Class && layout_ != Layout::Watch) {
    auto& c = data::CLASSES[std::clamp(menu_, 0, int(data::CLASSES.size()) - 1)];
    int w = std::min(cw - 4, 60);
    wrap((cw - w) / 2, top + 2 * int(data::CLASSES.size()) + 1, w, c.about, 0xB4A078);
  }
  if (!message_.empty()) centre(con_.get_height() - 3, message_, 0xFF8080);
}

void Client::draw_game() {
  if (!snap_.is_object()) {
    text(2, 2, "Waiting for the host...", GREY);
    return;
  }
  int cw = con_.get_width(), chh = con_.get_height();
  if (layout_ == Layout::Wide && SDL_getenv("PYREHEART_WHOLE")) {
    vx_ = 0; vy_ = 0; vw_ = cw; vh_ = chh;  // the whole map, nothing else (for screenshots)
    draw_map();
  } else if (layout_ == Layout::Wide) {
    vx_ = 0; vy_ = 0; vw_ = VIEW_W; vh_ = VIEW_H;
    draw_map();
    draw_side(VIEW_W, 0, cw - VIEW_W, chh);
    draw_log(0, VIEW_H, VIEW_W, chh - VIEW_H);
  } else if (layout_ == Layout::Tall) {
    vx_ = 0; vy_ = 2; vw_ = cw; vh_ = chh - 2 - 5 - 6;
    draw_status_tall(0);
    draw_map();
    draw_log(0, vy_ + vh_, cw, 5);
    draw_buttons();
  } else {
    vx_ = 0; vy_ = 0; vw_ = cw; vh_ = chh;
    draw_map();
    auto& me = snap_["me"];
    // life and mana as short bars along the top of the dial, the newest line along the bottom
    int hp = me["hp"], mhp = me["maxhp"], mn = me["mana"], mmn = me["maxmana"];
    for (int i = 0; i < 7 && over_ != Over::Ring; ++i) {
      put(2 + i + 4, 1, i < hp * 7 / std::max(1, mhp) ? 0x2588 : 0x2591, RED);
      put(2 + i + 4, 2, i < mn * 7 / std::max(1, mmn) ? 0x2588 : 0x2591, BLUE);
    }
    if (!log_.empty() && over_ != Over::Ring) {
      // the newest line slides past, like a ticker, in the part of the dial that is wide enough
      std::string l = log_.back().first;
      const int span = 11;
      if (int(l.size()) > span) {
        std::string loop = l + "   ";
        int off = int(now() * 6) % int(loop.size());
        loop += loop;
        l = loop.substr(off, span);
      }
      text((cw - int(l.size())) / 2, chh - 3, l, log_.back().second);
    }
    for (int y = 0; y < chh; ++y) for (int x = 0; x < cw; ++x) if (!in_circle(x, y)) put(x, y, ' ', 0, 0);
  }
  if (snap_.contains("boss") && layout_ != Layout::Watch) {
    auto& b = snap_["boss"];
    int w = std::min(40, vw_ - 4);
    int bx = vx_ + (vw_ - w) / 2;
    bar(bx, vy_, w, b["hp"], b["max"], 0xC03030, b["name"].get<std::string>());
    // the two places where the fight turns
    for (int k = 1; k <= 2; ++k) {
      int X = bx + w * k / 3;
      if (!con_.in_bounds({X, vy_})) continue;
      auto& t = con_.at({X, vy_});
      if (t.ch == ' ' || t.ch == 0x2588 || t.ch == 0x2591) { t.ch = 0x2502; t.fg = {255, 220, 150, 255}; }
      else t.bg = {150, 110, 40, 255};
    }
    std::string doing = b.value("doing", "");
    if (!doing.empty()) {
      int col = doing.rfind("red", 0) == 0 ? 0xFF7060 : doing.rfind("purple", 0) == 0 ? 0xD0A0FF : doing.rfind("gold", 0) == 0 ? 0xFFD060 : 0xE0D080;
      text(bx + (w - int(doing.size())) / 2, vy_ + 1, doing, col, 0);
    }
  }
  auto& me = snap_["me"];
  if (over_ == Over::None && !me["choice"].is_null() && layout_ != Layout::Watch) text(vx_ + 1, vy_ + vh_ - 1, "[p] a new perk", KEY);
  if (waiting_ && snap_["heroes"].size() > 1) text(vx_ + 1, vy_ + (snap_.contains("boss") ? 2 : 0), "waiting for the others...", GREY);
  switch (over_) {
    case Over::Pack: draw_pack(); break;
    case Over::Shop: draw_shop(); break;
    case Over::Perk: draw_perk(); break;
    case Over::Menu: draw_menu(); break;
    case Over::Ring: draw_ring(); break;
    case Over::Epigraph: draw_epigraph(); break;
    default: break;
  }
  if (over_ == Over::None && snap_.value("rest", false) && layout_ != Layout::Watch) draw_route();
  if (snap_.value("state", "play") != "play") draw_end();
}

// A zone's book fragment in a dark box in the middle: the zone's name in its colour, the text flowing in word by word
// as the opening crawl does, and the source dimmed at the foot.
void Client::draw_epigraph() {
  if (epis_.empty()) { over_ = Over::None; return; }
  const Epi& e = epis_.front();
  int w = std::min(vw_ - 4, 56), inner = w - 6;
  // the words, wrapped
  std::vector<std::string> lines;
  {
    std::string line, word;
    for (size_t i = 0; i <= e.text.size(); ++i) {
      if (i == e.text.size() || e.text[i] == ' ') {
        if (!line.empty() && int(line.size() + 1 + word.size()) > inner) { lines.push_back(line); line.clear(); }
        line += (line.empty() ? "" : " ") + word;
        word.clear();
      } else word += e.text[i];
    }
    if (!line.empty()) lines.push_back(line);
  }
  std::vector<std::string> src;
  {
    std::string line, word, s2 = "- " + e.source;
    for (size_t i = 0; i <= s2.size(); ++i) {
      if (i == s2.size() || s2[i] == ' ') {
        if (!line.empty() && int(line.size() + 1 + word.size()) > inner) { src.push_back(line); line.clear(); }
        line += (line.empty() ? "" : " ") + word;
        word.clear();
      } else word += s2[i];
    }
    if (!line.empty()) src.push_back(line);
  }
  int h = int(lines.size() + src.size()) + 7;
  int x0 = vx_ + (vw_ - w) / 2, y0 = vy_ + std::max(0, (vh_ - h) / 2);
  for (int y = y0; y < y0 + h; ++y)
    for (int x = x0; x < x0 + w; ++x) put(x, y, ' ', 0, 0x0C0A0A);
  for (int x = x0; x < x0 + w; ++x) { put(x, y0, 0x2500, 0x5A4A3A, 0x0C0A0A); put(x, y0 + h - 1, 0x2500, 0x5A4A3A, 0x0C0A0A); }
  for (int y = y0; y < y0 + h; ++y) { put(x0, y, 0x2502, 0x5A4A3A, 0x0C0A0A); put(x0 + w - 1, y, 0x2502, 0x5A4A3A, 0x0C0A0A); }
  put(x0, y0, 0x250C, 0x5A4A3A, 0x0C0A0A); put(x0 + w - 1, y0, 0x2510, 0x5A4A3A, 0x0C0A0A);
  put(x0, y0 + h - 1, 0x2514, 0x5A4A3A, 0x0C0A0A); put(x0 + w - 1, y0 + h - 1, 0x2518, 0x5A4A3A, 0x0C0A0A);
  // the zone's colour, made bright enough to read
  int r = (e.col >> 16) & 255, g = (e.col >> 8) & 255, b = e.col & 255, mx = std::max({r, g, b, 1});
  double k = 230.0 / mx;
  int tc = (std::min(255, int(r * k)) << 16) | (std::min(255, int(g * k)) << 8) | std::min(255, int(b * k));
  text(x0 + (w - int(e.title.size())) / 2, y0 + 2, e.title, tc, 0x0C0A0A);
  // word by word: about twelve words a second
  int words = int((now() - epi_start_) * 12);
  int y = y0 + 4;
  for (auto& l : lines) {
    std::string shown;
    for (size_t i = 0; i < l.size() && words > 0; ++i) {
      shown += l[i];
      if (l[i] == ' ') --words;
    }
    if (words > 0) --words;
    text(x0 + 3, y++, shown, 0xC8B48C, 0x0C0A0A);
  }
  ++y;
  bool done = words > 0;
  if (done)
    for (auto& l : src) { text(x0 + w - 3 - int(l.size()), y++, l, 0x7A6A5A, 0x0C0A0A); }
  if (done) text(x0 + w - 3 - 7, y0 + h - 1, " [key] ", 0x5A4A3A, 0x0C0A0A);
}

// In a waystation, a chart of the zones: the way taken so far in gold, the zones the stairs here lead to in white.
void Client::draw_route() {
  static const std::vector<std::vector<int>> rows = {{0}, {1, 4}, {2, 5, 6}, {7, 8}, {9}, {3}};
  std::vector<int> route;
  if (snap_.contains("route")) for (auto& z : snap_["route"]) route.push_back(z.get<int>());
  std::vector<int> next;
  if (snap_.contains("exits")) for (auto& e : snap_["exits"]) next.push_back(e[2].get<int>());
  int w = std::min(vw_ - 2, 50), h = int(rows.size()) * 2 + 3;
  int x0 = vx_ + (vw_ - w) / 2, y0 = vy_ + std::max(1, vh_ / 2 - 4 - h - 1);
  box(x0, y0, w, h, "The way down");
  for (size_t j = 0; j < rows.size(); ++j) {
    int n = int(rows[j].size());
    for (int i = 0; i < n; ++i) {
      int z = rows[j][i];
      std::string name = data::ACTS[z].name.substr(4);  // without "The "
      bool been = std::find(route.begin(), route.end(), z) != route.end();
      bool open = std::find(next.begin(), next.end(), z) != next.end();
      int col = been ? GOLD : open ? 0xF0F0F0 : 0x5A5A5A;
      int cx = x0 + w * (2 * i + 1) / (2 * n);
      int y = y0 + 2 + int(j) * 2;
      if (open) name = "> " + name + " <";
      text(cx - int(name.size()) / 2, y, name, col);
    }
  }
}

void Client::draw_map() {
  auto& s = snap_;
  int fl = s["floor"];
  const std::string& map = maps_[fl];
  if (map.size() != size_t(W * H)) return;
  auto& seen = seen_[fl];
  int hx = s["me"]["x"], hy = s["me"]["y"];
  int mw = s.value("mw", W), mh = s.value("mh", H);  // the part of the map in use
  camx_ = std::clamp(hx - vw_ / 2, 0, std::max(0, mw - vw_));
  camy_ = std::clamp(hy - vh_ / 2, 0, std::max(0, mh - vh_));
  if (vw_ >= mw) camx_ = -(vw_ - mw) / 2;
  if (vh_ >= mh) camy_ = -(vh_ - mh) / 2;
  auto sx = [&](int x) { return x - camx_ + vx_; };
  auto sy = [&](int y) { return y - camy_ + vy_; };
  draw_tiles();
  for (auto& w : s["writ"]) { int i = w; put(sx(i % W), sy(i / W), '?', 0xE6D2A0); }
  // runes and burning or hallowed ground
  if (s.contains("ground"))
    for (auto& g : s["ground"]) {
      int X = sx(g[0]), Y = sy(g[1]);
      if (X < vx_ || Y < vy_ || X >= vx_ + vw_ || Y >= vy_ + vh_) continue;
      bool flick = std::fmod(now() * 3 + g[0].get<int>() * .37, 1.0) < .5;
      int k = g[2];
      bool slow = std::fmod(now() * .8 + g[0].get<int>() * .21 + g[1].get<int>() * .13, 1.0) < .5;
      if (k == 0 || k == 7) put(X, Y, '^', flick ? 0xFF8C28 : 0xFFC850, 0x3C1406);       // embers, burning grease
      else if (k == 1) put(X, Y, 0x00B7, 0xFFE68C, 0x3C3414);                              // hallowed
      else if (k == 2) put(X, Y, slow ? 0x2591 : 0x2592, 0x7AB048, 0x14280C);              // gas
      else if (k == 3) put(X, Y, '~', slow ? 0xD8DC48 : 0xB8C030, 0x2C2E08);               // acid
      else if (k == 4) put(X, Y, '~', 0x403428, 0x0E0B08);                                 // grease
      else if (k == 5) put(X, Y, '~', flick ? 0xFF8C28 : 0xE06018, 0x4A1606);              // slag
      else if (k == 6) put(X, Y, ',', 0x3C2C1C, 0x0C0906);                                 // tar
      else if (k == 8) put(X, Y, slow ? 0x2592 : 0x2593, 0x9A968C, 0x2E2C28);              // smoke
    }
  if (s.contains("runes"))
    for (auto& g : s["runes"]) {
      int X = sx(g[0]), Y = sy(g[1]);
      if (X < vx_ || Y < vy_ || X >= vx_ + vw_ || Y >= vy_ + vh_) continue;
      put(X, Y, 0x263C, g[2] == snap_["me"]["pid"] ? 0x50E6E6 : 0x308C8C, 0x0A2828);
    }
  // blows being wound up: the count on the ground, tinted by what to do
  if (s.contains("marks"))
    for (auto& mk : s["marks"]) {
      int k = mk["k"], n = mk["n"];
      static const int BG[] = {0x5A1410, 0x3C1650, 0x5A4610}, FG[] = {0xFF6040, 0xC080FF, 0xFFD040};
      bool blink = std::fmod(now(), .8) < .4;
      auto& c = mk["c"];
      for (size_t i = 0; i + 1 < c.size(); i += 2) {
        int X = sx(c[i]), Y = sy(c[i + 1]);
        if (X < vx_ || Y < vy_ || X >= vx_ + vw_ || Y >= vy_ + vh_) continue;
        put(X, Y, '0' + std::min(n, 9), n == 1 && blink ? 0xFFFFFF : FG[k], n == 1 && blink ? scale(BG[k], 1.6) : BG[k]);
      }
    }
  for (auto& it : s["items"]) {
    std::string ch = it["ch"];
    put(sx(it["x"]), sy(it["y"]), (unsigned char)ch[0], it["col"]);
  }
  for (auto& e : s["ents"]) {
    int col = e["col"];
    if (e.value("frozen", false)) col = 0x9CD2FF;
    int X = sx(e["x"]), Y = sy(e["y"]);
    if (e.value("chill", false))  // the cold around it, as a blue tint on the floor
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
          if ((dx || dy) && con_.in_bounds({X + dx, Y + dy})) {
            auto& t = con_.at({X + dx, Y + dy});
            t.bg = {uint8_t(t.bg.r * .6 + 20), uint8_t(t.bg.g * .6 + 40), uint8_t(t.bg.b * .6 + 80), 255};
          }
    int bg = -1;
    if (e.value("braced", false)) bg = 0x6A5A3A;  // its guard up
    if (e.value("hidden", false) && std::fmod(now() * .37 + e["id"].get<int>() * .61, 6.0) < .12) col = 0xB89400;  // a twitch
    put(X, Y, e["ch"].get<int>(), col, bg);
    if (e.value("listen", false) && con_.in_bounds({X, Y - 1})) put(X, Y - 1, '?', 0xE0E0A0);
  }
  for (auto& h : s["heroes"]) {
    if (h.value("gone", false)) continue;
    bool dead = h.value("dead", false);
    put(sx(h["x"]), sy(h["y"]), dead ? '%' : '@', dead ? 0x803030 : h["col"].get<int>());
  }
  // effects
  double t = now();
  fx_.erase(std::remove_if(fx_.begin(), fx_.end(), [&](const Fx& f) { return t - f.start > fx::life(f.kw); }), fx_.end());
  for (auto& f : fx_)
    for (auto& c : fx::cells(f.kw, t - f.start)) {
      int X = sx(c.x), Y = sy(c.y);
      if (X < vx_ || Y < vy_ || X >= vx_ + vw_ || Y >= vy_ + vh_) continue;
      if (c.x < 0 || c.y < 0 || c.x >= W || c.y >= H || !seen[c.y * W + c.x] || (map[c.y * W + c.x] - TILE0) == data::WALL) continue;
      put(X, Y, c.ch, (c.col.r << 16) | (c.col.g << 8) | c.col.b);
    }
  if (over_ == Over::Target) {
    int X = sx(tx_), Y = sy(ty_);
    if (con_.in_bounds({X, Y})) {
      auto& tile = con_.at({X, Y});
      tile.bg = {120, 100, 30, 255};
    }
    // what is under the cursor
    std::string what;
    std::string about;
    for (auto& e : s["ents"])
      if (e["x"] == tx_ && e["y"] == ty_) {
        what = e["name"].get<std::string>();
        if (e.contains("max") && e["max"].get<int>() > 1) what += " (" + std::to_string(e["hp"].get<int>()) + "/" + std::to_string(e["max"].get<int>()) + ")";
        about = e.value("about", "");
      }
    for (auto& it : s["items"])
      if (what.empty() && it["x"] == tx_ && it["y"] == ty_) {
        what = it["name"];
        if (it.contains("cmp")) {
          about = verdict_text(it["cmp"]);
          for (auto& d : it["cmp"]["d"]) {
            double by = d[1];
            char buf[48];
            snprintf(buf, sizeof buf, std::abs(by - std::round(by)) < .05 ? ", %+.0f %s" : ", %+.1f %s", by, d[0].get<std::string>().c_str());
            about += buf;
          }
        }
      }
    if (what.empty() && seen[ty_ * W + tx_]) what = data::TILES[map[ty_ * W + tx_] - TILE0].name;
    if (snap_.contains("exits"))
      for (auto& e : snap_["exits"])
        if (e[0].get<int>() == tx_ && e[1].get<int>() == ty_) what = "stairs down to " + data::ACTS[e[2].get<int>()].name;
    int ly = vy_ + vh_ - 2;
    if (layout_ != Layout::Watch) {
      text(vx_ + 1, ly, what, WHITE, 0);
      if (!about.empty()) text(vx_ + 1, ly + 1, about, GREY, 0);
      std::string hint = aim_ == "look" ? "[Esc]" : "[Tab] [Enter] [Esc]";
      text(vx_ + vw_ - int(hint.size()) - 1, vy_, hint, KEY, 0);
    }
  }
}

void Client::draw_side(int x, int y, int w, int h) {
  auto& me = snap_["me"];
  int r = y;
  text(x + 1, r++, me["name"].get<std::string>(), me["col"].get<int>());
  text(x + 1, r++, "level " + std::to_string(me["lvl"].get<int>()) + " " + me["clsname"].get<std::string>(), GREY);
  ++r;
  bar(x + 1, r++, w - 2, me["hp"], me["maxhp"], RED, "Life " + std::to_string(me["hp"].get<int>()) + "/" + std::to_string(me["maxhp"].get<int>()));
  {
    bool fu = me.value("pool", "mana") == "fury";
    bar(x + 1, r++, w - 2, me["mana"], me["maxmana"], fu ? 0xC05020 : BLUE, std::string(fu ? "Fury " : "Mana ") + std::to_string(me["mana"].get<int>()) + "/" + std::to_string(me["maxmana"].get<int>()));
  }
  int prev = me["prev"], next = me["next"], xp = me["xp"];
  bar(x + 1, r++, w - 2, xp - prev, next - prev, 0xB09040, "XP");
  std::string st;
  if (me.value("poison", false)) st += "poisoned ";
  if (me.value("numb", false)) st += "numb ";
  if (me.value("shield", false)) st += "shielded ";
  if (me.value("held", false)) st += "held ";
  if (me.value("latched", false)) st += "latched ";
  if (me.value("bleeding", false)) st += "bleeding ";
  if (me.value("branded", false)) st += "branded ";
  if (me.value("sundered", false)) st += "split armour ";
  if (me.value("dazed", false)) st += "dazed ";
  if (!st.empty()) text(x + 1, r, st, 0x96E65A);
  ++r;
  std::string where = snap_.value("rest", false) ? "Waystation" : "Depth " + std::to_string(snap_["depth"].get<int>());
  text(x + 1, r++, where, GOLD);
  text(x + 1, r++, data::ACTS[snap_["act"].get<int>()].name, GREY);
  text(x + 1, r++, std::to_string(me["gold"].get<int>()) + " gold", 0xFFD700);
  ++r;
  text(x + 1, r, "[q]", KEY); text(x + 5, r++, "heal x" + std::to_string(me["heal"].get<int>()), 0xFF7070);
  bool fury = me.value("pool", "mana") == "fury";
  text(x + 1, r, "[w]", KEY); text(x + 5, r++, std::string(fury ? "fury" : "mana") + " x" + std::to_string(me["manap"].get<int>()), fury ? 0xFF9050 : 0x7090FF);
  for (int i = 0; i < int(me["skills"].size()); ++i) {
    auto& sk = me["skills"][i];
    int lv = sk[4];
    if (me["lvl"].get<int>() < lv) { text(x + 1, r, "[" + std::to_string(i + 1) + "]", DIM); text(x + 5, r++, "at level " + std::to_string(lv), DIM); continue; }
    bool ok = me["mana"].get<int>() >= sk[1].get<int>();
    text(x + 1, r, "[" + std::to_string(i + 1) + "]", KEY);
    text(x + 5, r++, sk[0].get<std::string>() + " " + std::to_string(sk[1].get<int>()), ok ? WHITE : DIM);
  }
  if (me.value("focus", 0) > 0) text(x + 1, r++, "focus " + std::string(me["focus"].get<int>(), '*'), 0xB4E68C);
  if (me.value("standfast", 0) > 0) text(x + 1, r++, "standing fast " + std::to_string(me["standfast"].get<int>()), 0xE6C878);
  text(x + 1, r++, "damage " + me["dmg"].get<std::string>(), GREY);
  text(x + 1, r++, "armour " + std::to_string(me["armour"].get<int>()), GREY);
  ++r;
  if (snap_["heroes"].size() > 1) {
    text(x + 1, r++, "PARTY", GOLD);
    for (auto& o : snap_["heroes"]) {
      if (o["pid"] == me["pid"]) continue;
      std::string n = o["name"].get<std::string>();
      if (o.value("dead", false)) n += " (fallen)";
      else if (o.value("gone", false)) n += " (at stairs)";
      bool ready = std::find(ready_.begin(), ready_.end(), o["pid"].get<int>()) != ready_.end();
      text(x + 1, r, ready ? "+" : " ", 0x80E080);
      text(x + 2, r++, n, o["col"].get<int>());
      if (!o.value("dead", false)) bar(x + 2, r++, w - 3, o["hp"], o["max"], RED, "");
    }
    ++r;
  }
  text(x + 1, r++, "IN SIGHT", GOLD);
  for (auto& e : foes()) {
    if (r >= h - 1) break;
    std::string n = e["name"].get<std::string>();
    if (int(n.size()) > w - 4) n = n.substr(0, w - 4);
    put(x + 1, r, e["ch"].get<int>(), e["col"].get<int>());
    text(x + 3, r++, n, e["rank"] == "normal" ? GREY : e["col"].get<int>());
    // what an opening-seeker or anyone else wants to know at a glance
    std::string tag = e.value("spent", false) ? "spent" : e.value("stun", false) ? "stunned" : e.value("frozen", false) ? "frozen"
                    : e.value("pinned", false) ? "held" : e.value("bleed", false) ? "bleeding" : "";
    if (snap_["me"]["cls"] == "cutthroat" && e.value("busy", false)) tag = tag.empty() ? "open" : tag + ", open";
    bar(x + 3, r, w - 4, e["hp"], e["max"], 0x903030, tag);
    ++r;
  }
}

void Client::draw_status_tall(int y) {
  auto& me = snap_["me"];
  int cw = con_.get_width();
  bar(0, y, cw / 2 - 1, me["hp"], me["maxhp"], RED, std::to_string(me["hp"].get<int>()) + "/" + std::to_string(me["maxhp"].get<int>()));
  bar(cw / 2, y, cw - cw / 2, me["mana"], me["maxmana"], me.value("pool", "mana") == "fury" ? 0xC05020 : BLUE, std::to_string(me["mana"].get<int>()) + "/" + std::to_string(me["maxmana"].get<int>()));
  std::string where = snap_.value("rest", false) ? "Waystation" : "Depth " + std::to_string(snap_["depth"].get<int>());
  text(0, y + 1, where + "  lvl " + std::to_string(me["lvl"].get<int>()) + "  " + std::to_string(me["gold"].get<int>()) + "g", GOLD);
}

void Client::draw_log(int x, int y, int w, int h) {
  // newest at the bottom, wrapped
  std::vector<std::pair<std::string, int>> lines;
  for (auto it = log_.rbegin(); it != log_.rend() && int(lines.size()) < h; ++it) {
    std::vector<std::string> parts;
    std::string cur, word;
    std::string s = it->first + " ";
    for (char c : s) {
      if (c == ' ') {
        if (int(cur.size() + word.size()) + (cur.empty() ? 0 : 1) > w - 1 && !cur.empty()) { parts.push_back(cur); cur.clear(); }
        cur += (cur.empty() ? "" : " ") + word;
        word.clear();
      } else word += c;
    }
    if (!cur.empty()) parts.push_back(cur);
    for (auto p = parts.rbegin(); p != parts.rend(); ++p) lines.push_back({*p, it->second});
  }
  int n = std::min(h, int(lines.size()));
  for (int i = 0; i < n; ++i) {
    auto& [s, c] = lines[i];
    int age = i;
    text(x + 1, y + h - 1 - i, s, age < 3 ? c : scale(c, .6));
  }
}

void Client::draw_buttons() {
  int cw = con_.get_width(), chh = con_.get_height();
  auto& me = snap_["me"];
  std::vector<std::pair<std::string, std::string>> b;
  if (me.value("bow", false)) b.push_back({"Shoot", "shoot"});
  for (int i = 0; i < int(me["skills"].size()); ++i)
    if (me["lvl"].get<int>() >= me["skills"][i][4].get<int>()) b.push_back({std::to_string(i + 1) + " ", "skill" + std::to_string(i + 1)});
  for (auto x : std::vector<std::pair<std::string, std::string>>{{"Heal", "heal"}, {me.value("pool", "mana") == "fury" ? "Fury" : "Mana", "mana"},
                                                                 {"Wait", "wait"}, {"Bag", "pack"}, {"> ", "down"}, {"Perk", "perk"},
                                                                 {"Look", "look"}, {"?", "help"}})
    b.push_back(x);
  int per = 6, bw = cw / per, bh = 3;
  int top = chh - 6;
  for (size_t i = 0; i < b.size() && i < 12; ++i) {
    int x = int(i % per) * bw, y = top + int(i / per) * bh;
    for (int j = 0; j < bh; ++j) for (int k = 0; k < bw - 1; ++k) put(x + k, y + j, ' ', WHITE, 0x202028);
    text(x + (bw - 1 - int(b[i].first.size())) / 2, y + 1, b[i].first, KEY, 0x202028);
    buttons_.push_back({x, y, bw - 1, bh, b[i].first, b[i].second});
  }
}

// Every number that would move if this were worn, green up and red down. Returns the lines used.
int Client::draw_changes(int x, int y, int w, const json& it) {
  if (!it.contains("cmp")) return 0;
  auto& c = it["cmp"];
  int v = c["v"], r = y;
  auto [m, col] = mark(it);
  if (v == -9) return 0;
  text(x, r, m, col);
  text(x + 2, r++, verdict_text(c), col);
  for (auto& d : c["d"]) {
    double by = d[1];
    char buf[32];
    if (std::abs(by - std::round(by)) < .05) snprintf(buf, sizeof buf, "%+d", int(std::round(by)));
    else snprintf(buf, sizeof buf, "%+.1f", by);
    int dc = by > 0 ? 0x60D060 : 0xD06060;
    text(x, r, by > 0 ? "▲" : "▼", dc);
    text(x + 2, r, buf, dc);
    text(x + 8, r, d[0].get<std::string>(), GREY);
    if (++r >= y + 12) break;
  }
  (void)w;
  return r - y;
}

void Client::draw_pack() {
  auto& me = snap_["me"];
  int cw = con_.get_width(), chh = con_.get_height();
  int w = std::min(cw, 74), h = std::min(chh, 34);
  int x = (cw - w) / 2, y = (chh - h) / 2;
  box(x, y, w, h, "Your things");
  int r = y + 1;
  for (size_t i = 0; i < data::SLOTS.size(); ++i) {
    auto& it = me["equip"][i];
    text(x + 2, r, data::SLOTS[i], DIM);
    if (!it.is_null()) text(x + 10, r, it["short"].get<std::string>(), data::rarity_col(it["rarity"]));
    ++r;
  }
  ++r;
  auto& pack = me["pack"];
  text(x + 2, r++, "Pack (" + std::to_string(pack.size()) + "/" + std::to_string(data::PACK_MAX) + ")", GOLD);
  int list_top = r;
  sel_ = pack.empty() ? 0 : std::clamp(sel_, 0, int(pack.size()) - 1);
  for (size_t i = 0; i < pack.size() && r < y + h - 2; ++i) {
    auto& it = pack[i];
    bool on = int(i) == sel_;
    int col = data::rarity_col(it["rarity"]);
    if (!it.value("use", true)) col = scale(col, .5);
    auto [m, mc] = mark(it);
    text(x + 2, r, on ? ">" : " ", WHITE);
    text(x + 3, r, m, mc);
    text(x + 5, r, clip(it["short"].get<std::string>(), w / 2 - 6), col, on ? 0x303040 : PANEL);
    buttons_.push_back({x + 2, r, w / 2 - 2, 1, "", "sel:" + std::to_string(i)});
    ++r;
  }
  if (!pack.empty()) {
    auto& it = pack[sel_];
    int dx = x + w / 2 + 1, dy = list_top;
    text(dx, dy++, it["name"].get<std::string>(), data::rarity_col(it["rarity"]));
    for (auto& l : it["lines"]) dy += wrap(dx, dy, w / 2 - 3, l.get<std::string>(), GREY);
    // what it would replace
    for (size_t i = 0; i < data::SLOTS.size(); ++i)
      if (data::SLOTS[i] == it["slot"].get<std::string>() && !me["equip"][i].is_null()) {
        ++dy;
        text(dx, dy++, "now: " + me["equip"][i]["short"].get<std::string>(), DIM);
      }
    ++dy;
    dy += draw_changes(dx, dy, w / 2 - 3, it);
    if (!it.value("use", true)) text(dx, ++dy, "Not for a " + me["clsname"].get<std::string>() + ".", 0xC06060);
  }
  if (layout_ == Layout::Wide) wrap(x + 2, y + h - 2, w - 4, "[Enter] wear  [d] put down  [t] give  [Esc] close", GREY);
  else {
    int bw = w / 3;
    std::vector<std::pair<std::string, int>> b = {{"Wear", SDLK_RETURN}, {"Drop", SDLK_D}, {"Give", SDLK_T}};
    for (int i = 0; i < 3; ++i) {
      text(x + i * bw + 2, y + h - 2, "[" + b[i].first + "]", KEY);
      buttons_.push_back({x + i * bw, y + h - 2, bw, 1, "", "key:" + std::to_string(b[i].second)});
    }
    buttons_.push_back({x + w - 3, y, 3, 1, "", "close"});
    text(x + w - 3, y, "[x]", KEY);
  }
}

void Client::draw_shop() {
  auto& me = snap_["me"];
  if (!snap_.contains("shop")) return;
  auto& shop = snap_["shop"];
  int cw = con_.get_width(), chh = con_.get_height();
  int w = std::min(cw, 74), h = std::min(chh, 34);
  int x = (cw - w) / 2, y = (chh - h) / 2;
  box(x, y, w, h, "The trader");
  int r = y + 1;
  text(x + 2, r++, std::to_string(me["gold"].get<int>()) + " gold", 0xFFD700);
  text(x + 2, r, "[h]", KEY); text(x + 6, r++, "healing potion, " + std::to_string(shop["potion"].get<int>()) + " gold (you have " + std::to_string(me["heal"].get<int>()) + ")", 0xFF7070);
  text(x + 2, r, "[m]", KEY); text(x + 6, r++, "mana potion, " + std::to_string(shop["potion"].get<int>()) + " gold (you have " + std::to_string(me["manap"].get<int>()) + ")", 0x7090FF);
  buttons_.push_back({x + 2, r - 2, w - 4, 1, "", "key:" + std::to_string(SDLK_H)});
  buttons_.push_back({x + 2, r - 1, w - 4, 1, "", "key:" + std::to_string(SDLK_M)});
  ++r;
  auto& items = shop["items"];
  auto& pack = me["pack"];
  int n = int(items.size() + pack.size());
  sel_ = n ? std::clamp(sel_, 0, n - 1) : 0;
  text(x + 2, r++, "For sale", GOLD);
  int i = 0;
  json shown;
  for (auto& it : items) {
    bool on = i == sel_;
    auto [m, mc] = mark(it);
    text(x + 2, r, on ? ">" : " ", WHITE);
    text(x + 3, r, m, mc);
    text(x + 5, r, clip(it["short"].get<std::string>(), w / 2 - 12) + "  " + std::to_string(it["price"].get<int>()) + "g", data::rarity_col(it["rarity"]), on ? 0x303040 : PANEL);
    buttons_.push_back({x + 2, r, w / 2, 1, "", "sel:" + std::to_string(i)});
    if (on) shown = it;
    ++r; ++i;
  }
  ++r;
  text(x + 2, r++, "Yours to sell", GOLD);
  for (size_t k = 0; k < pack.size() && r < y + h - 2; ++k) {
    auto& it = pack[k];
    bool on = i == sel_;
    int price = 1;
    for (auto& s : shop["sell"]) if (s[0] == it["id"]) price = s[1];
    auto [m, mc] = mark(it);
    text(x + 2, r, on ? ">" : " ", WHITE);
    text(x + 3, r, m, mc);
    text(x + 5, r, clip(it["short"].get<std::string>(), w / 2 - 12) + "  " + std::to_string(price) + "g", data::rarity_col(it["rarity"]), on ? 0x303040 : PANEL);
    buttons_.push_back({x + 2, r, w / 2, 1, "", "sel:" + std::to_string(i)});
    if (on) shown = it;
    ++r; ++i;
  }
  if (shown.is_object()) {
    int dy = y + 5;
    text(x + w / 2 + 1, dy++, shown["name"].get<std::string>(), data::rarity_col(shown["rarity"]));
    for (auto& l : shown["lines"]) dy += wrap(x + w / 2 + 1, dy, w / 2 - 3, l.get<std::string>(), GREY);
    ++dy;
    draw_changes(x + w / 2 + 1, dy, w / 2 - 3, shown);
  }
  wrap(x + 2, y + h - 2, w - 4, layout_ == Layout::Wide ? "[Enter] buy or sell  [Esc] leave" : "tap twice to buy or sell", GREY);
  if (layout_ != Layout::Wide) { buttons_.push_back({x + w - 3, y, 3, 1, "", "close"}); text(x + w - 3, y, "[x]", KEY); }
}

void Client::draw_perk() {
  auto& ch = snap_["me"]["choice"];
  if (ch.is_null()) { over_ = Over::None; return; }
  int cw = con_.get_width(), chh = con_.get_height();
  int w = std::min(cw, 60), h = 4 + int(ch.size()) * 4;
  int x = (cw - w) / 2, y = (chh - h) / 2;
  box(x, y, w, h, "Choose a perk");
  for (size_t i = 0; i < ch.size(); ++i) {
    int r = y + 2 + int(i) * 4;
    bool on = int(i) == sel_;
    text(x + 2, r, "[" + std::to_string(i + 1) + "]", KEY);
    text(x + 6, r, ch[i][1].get<std::string>(), on ? WHITE : GREY, on ? 0x303040 : PANEL);
    wrap(x + 6, r + 1, w - 8, ch[i][2].get<std::string>(), 0xB4A078);
    buttons_.push_back({x + 1, r, w - 2, 3, "", "key:" + std::to_string(SDLK_1 + int(i))});
  }
}

void Client::draw_menu() {
  int cw = con_.get_width(), chh = con_.get_height();
  int w = std::min(cw, 40), h = 9;
  int x = (cw - w) / 2, y = (chh - h) / 2;
  box(x, y, w, h, "Pyreheart");
  text(x + 2, y + 2, "[Esc] back to the dive", WHITE);
  text(x + 2, y + 4, std::string("[m] music: ") + (music_on_ ? "on" : "off"), WHITE);
  text(x + 2, y + 6, "[q] leave the dive", WHITE);
  buttons_.push_back({x, y + 2, w, 1, "", "close"});
  buttons_.push_back({x, y + 4, w, 1, "", "key:" + std::to_string(SDLK_M)});
  buttons_.push_back({x, y + 6, w, 1, "", "key:" + std::to_string(SDLK_Q)});
}

void Client::draw_help() {
  int cw = con_.get_width(), chh = con_.get_height();
  int w = std::min(cw, 80), h = std::min(chh, 40);
  int x = (cw - w) / 2, y = (chh - h) / 2;
  box(x, y, w, h, "How to play");
  int r = y + 2;
  for (auto& [k, t] : HELP) {
    if (r >= y + h - 1) break;
    if (k.empty()) { r += t.empty() ? 1 : wrap(x + 2, r, w - 4, t, 0xC8C8C8); continue; }
    wrap(x + 2, r, 22, k, KEY);
    r += std::max(1, wrap(x + 24, r, w - 26, t, 0xC8C8C8));
  }
}

void Client::draw_ring() {
  // the watch: actions round the edge of the dial; the crown turns it, a tap on one does it
  static const std::vector<std::pair<std::string, std::string>> R = {
      {"1 ", "skill1"}, {"2 ", "skill2"}, {"3 ", "skill3"}, {"4 ", "skill4"}, {"Hl", "heal"}, {"Mn", "mana"}, {"..", "wait"},
      {"> ", "down"}, {"Bg", "pack"}, {"Pk", "perk"}, {"Sh", "shoot"}};
  int cw = con_.get_width();
  // the dial goes dark behind the ring, so the actions stand out
  for (int y = 0; y < con_.get_height(); ++y)
    for (int x = 0; x < cw; ++x) {
      auto& t = con_.at({x, y});
      bool wall = t.ch != ' ' && t.ch != 0x00B7;
      t.ch = wall ? t.ch : ' ';
      t.fg = {uint8_t(t.fg.r / 6), uint8_t(t.fg.g / 6), uint8_t(t.fg.b / 6), 255};
      t.bg = {0, 0, 0, 255};
    }
  double c = (cw - 1) / 2.0, rad = cw / 2.0 - 2.5;
  for (size_t i = 0; i < R.size(); ++i) {
    double a = -std::numbers::pi / 2 + i * 2 * std::numbers::pi / R.size();
    int x = int(std::lround(c + std::cos(a) * rad)) - 1, y = int(std::lround(c + std::sin(a) * rad));
    bool on = int(i) == ring_;
    text(x, y, R[i].first, on ? 0x000000 : KEY, on ? KEY : 0x202028);
    buttons_.push_back({x - 1, y - 1, 4, 3, "", R[i].second});
  }
  // the middle names what the crown points at; a tap there does it
  auto& me = snap_["me"];
  std::string name;
  const std::string& a = R[ring_].second;
  if (a.rfind("skill", 0) == 0) {
    auto& sk = me["skills"][a.back() - '1'];
    name = me["lvl"].get<int>() >= sk[4].get<int>() ? sk[0].get<std::string>() : "Lvl " + std::to_string(sk[4].get<int>());
  } else if (a == "heal") name = "Heal x" + std::to_string(me["heal"].get<int>());
  else if (a == "mana") name = "Mana x" + std::to_string(me["manap"].get<int>());
  else if (a == "wait") name = "Wait";
  else if (a == "down") name = "Go down";
  else if (a == "pack") name = "Pack";
  else if (a == "perk") name = me["choice"].is_null() ? "Perks" : "New perk!";
  else name = "Shoot";
  if (int(name.size()) > 9) name = name.substr(0, 9);
  int w = int(name.size());
  for (int x = int(c) - 5; x <= int(c) + 5; ++x) for (int y = int(c) - 1; y <= int(c) + 1; ++y) put(x, y, ' ', 0, 0x181820);
  text(int(c) - w / 2, int(c), name, WHITE, 0x181820);
  buttons_.push_back({int(c) - 5, int(c) - 1, 11, 3, "", a});
}

void Client::draw_end() {
  auto& me = snap_["me"];
  int cw = con_.get_width(), chh = con_.get_height();
  bool won = snap_["state"] == "won";
  int w = std::min(cw, 60), h = std::min(chh, 14);
  int x = (cw - w) / 2, y = (chh - h) / 2;
  box(x, y, w, h, won ? "The fire is yours" : "The dive is over");
  int r = y + 2;
  if (won) for (auto& l : lore::VICTORY) r += wrap(x + 2, r, w - 4, l, GOLD);
  else r += wrap(x + 2, r, w - 4, "Charnfen waits in vain.", 0xFF8080);
  ++r;
  text(x + 2, r++, me["name"].get<std::string>() + ", level " + std::to_string(me["lvl"].get<int>()) + " " + me["clsname"].get<std::string>(), WHITE);
  text(x + 2, r++, "depth " + std::to_string(snap_["depth"].get<int>()) + ", " + std::to_string(me["kills"].get<int>()) + " foes slain, " +
                       std::to_string(snap_["turn"].get<int>()) + " turns", GREY);
  text(x + 2, y + h - 2, "[Enter]", KEY);
}
