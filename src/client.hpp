// The player's side: screens, drawing and input. It only ever knows what the host sends it (see view.cpp).
#pragma once
#include <SDL3/SDL.h>

#include <chrono>
#include <deque>
#include <libtcod.hpp>
#include <map>
#include <array>
#include <memory>
#include <set>

#include "music.hpp"
#include "net.hpp"

enum class Layout { Wide, Tall, Watch };

class Client {
 public:
  explicit Client(tcod::Context& ctx);
  bool running = true;
  void event(SDL_Event& ev);
  void frame();
  void remember_window();  // the window's size and fullscreen, for the next start
  void shot(const std::string& path) { shot_ = path; frame(); }  // the screen as shown, particles too, as a PNG

 private:
  enum class Screen { Intro, Title, Name, Class, Join, Finding, Listen, Pyre, Game };
  enum class Over { None, Pack, Perk, Shop, Book, Target, Menu, Ring, Epigraph, Debug, Hero, Options, Coal };
  tcod::Context& ctx_;
  tcod::Console con_;
  Layout layout_ = Layout::Wide;
  Screen screen_ = Screen::Intro;
  double intro_start_ = 0;
  Over over_ = Over::None;
  int menu_ = 0, sel_ = 0, cls_ = 0;
  bool hosting_ = false, open_ = false, music_on_ = true, fullscreen_ = false;
  int win_w_ = 0, win_h_ = 0;
  bool listen_fight_ = false;
  music::Player music_;
  void update_music();
  std::string name_ = "Hero", addr_, message_;
  std::vector<std::string> found_;

  std::unique_ptr<Host> host_;
  std::shared_ptr<Link> link_;
  int pid_ = 0;
  json snap_;
  std::map<int, std::string> maps_;
  std::map<int, std::vector<char>> seen_;
  std::deque<std::pair<std::string, int>> log_;
  struct Fx { json kw; double start; };
  std::vector<Fx> fx_;
  bool numbers_on_ = true;  // floating combat numbers
  bool waiting_ = false, pending_join_ = false;
  int sent_turn_ = -1;
  std::vector<int> ready_;

  // aiming and looking
  int tx_ = 0, ty_ = 0;
  std::string aim_;  // "shoot", "skill1", "skill2", "look"
  // touch
  bool held_ = false;
  double held_since_ = 0, last_step_ = 0;
  int hold_x_ = 0, hold_y_ = 0;
  int ring_ = 0;
  // screen geometry of the map view
  int vx_ = 0, vy_ = 0, vw_ = 0, vh_ = 0, camx_ = 0, camy_ = 0;
  struct Button { int x, y, w, h; std::string label, action; };
  std::vector<Button> buttons_;

  double now() const;
  void pick_layout();
  void load_settings();
  void save_settings();

  void start(bool host, bool open, bool resume = false);
  std::vector<std::string> title_items() const;
  int title_index(const std::string& item) const;
  void join(const std::string& address);
  void poll();
  void apply(const json& snap);
  void send(const json& act);
  void act(const std::string& what);
  void key(SDL_Keycode k, SDL_Keymod mod);
  void click(int x, int y, bool down);
  void begin_aim(const std::string& what);
  void cycle_target(int dir);
  std::vector<json> foes() const;

  void draw_title();
  void draw_intro();
  int draw_logo(int y);  // logo.cpp
  int menu_top() const;
  void draw_game();
  void draw_map();
  void draw_tiles();  // scene.cpp: stone, light and the dressing of the rooms
  struct Emit { float x, y; char kind; int seed; };
  std::vector<Emit> emits_;  // where particles rise this frame
  std::string shot_;
  std::vector<int> ldist_;  // each lava cell's distance from land, for the current map
  int hot_floor_ = -1;
  std::vector<char> hot_;  // in the Forge: how near the running metal each cell lies (1, 2), for the heat shimmer
  int ldist_floor_ = -1;
  // the Crypt's air (mock-ups, PYREHEART_CRYPTFX=1): how near each cell is to an open grave or the ossuary, for the
  // grave chill; the coloured light of the chapel windows; where a hero last stepped, for bone dust
  int chill_floor_ = -1;
  std::vector<float> chill_;
  struct Beam { int wx, wy, len, col, dir; };  // dir: +1 the light falls down the screen, -1 up
  std::vector<Beam> beams_;
  int step_x_ = -1, step_y_ = -1;
  double step_t_ = -9;
  std::string ldist_map_;
  // the Caves' air: mist over the stream, the wet rock beside it, the draught in the propped tunnels and down the
  // fallen shafts, the mountain groaning now and then, eyes at the edge of the light
  int cave_floor_ = -1, wind_ = 1, eyes_gone_ = -1;
  std::vector<float> damp_, draft_;
  std::vector<char> wetw_;
  double groan_t_ = -99, groan_next_ = -1;
  double drum_t_ = -99, drum_next_ = -1;
  double rattle_t_ = -99, rattle_next_ = -1;  // the Prison: something rattles a cell door, or chains clink
  bool rattle_door_ = true;  // the Barracks: a war-drum booming far off, now and then
  std::map<int, int> ent_cell_;  // the Barracks: where each foe stood, so soot puffs up when it takes a step
  std::deque<std::array<double, 3>> puffs_;  // and those puffs: x, y, when
  double ghost_x_ = 0, ghost_y_ = 0, ghost_dx_ = 1, ghost_a_ = 0;  // the Crypt's ghost, if one is passing
  void draw_particles(SDL_Renderer* r, double ox, double oy, double cell);
  void present(const TCOD_ViewportOptions& vp);
  void draw_side(int x, int y, int w, int h);
  // the dwarven frame, orbs and suns of the wide layout (hud.cpp)
  struct OrbAt { int x, y; double frac; int col; };
  std::vector<OrbAt> orbs_;
  std::vector<std::pair<int, int>> suns_;
  int frame_col() const;
  int rivet_col() const;
  void header(int x0, int x1, int y, const std::string& title, int left, int right);
  void stone(int x0, int y0, int x1, int y1);
  void orb(int x, int y, int val, int max, int liquid, const std::string& label, bool used);
  void draw_frame_wide(int px, int ly);
  void draw_in_sight(int x, int r, int right, int last);
  void stacks_row(int x, int y, const json& st, const std::set<int>& warn);
  void draw_coals();
  int coal_sel_ = 0;
  void draw_coal_window();
  void coal_key(SDL_Keycode k);
  void low_life_edge();
  void burn_suns();
  void draw_ui_particles(SDL_Renderer* r, double ox, double oy, double cell);
  void draw_status_tall(int y);
  void draw_log(int x, int y, int w, int h);
  void draw_buttons();
  void draw_pack();
  int draw_changes(int x, int y, int w, const json& it);
  void draw_perk();
  void draw_shop();
  void draw_menu();
  // windows as objects (windows.cpp): the main menu, the Hero screen, the options and the handbook
  enum class Mat { Bronze, Leather, Wood, Stone };
  int win_bg_ = 0x1A140F, menu_sel_ = 0, opt_sel_ = 0, music_vol_ = 7, sound_vol_ = 7;
  int book_ch_ = 0, book_scroll_ = 0, book_last_ = 0, tip_ch_ = -1;
  double tip_t_ = -99;
  std::string book_why_;
  void dim_all(double f);
  void window(int x, int y, int w, int h, const std::string& title, Mat m = Mat::Bronze);
  void keycap(int x, int y, const std::string& k);
  int rich(int x, int y, const std::string& s, int col, int bg);
  int rich_wrap(int x, int y, int w, const std::string& s, int col, int bg, bool draw = true);
  void menu_do(int i);
  void menu_key(SDL_Keycode k);
  void draw_hero();
  void apply_volume();
  void draw_options();
  void options_key(SDL_Keycode k);
  void open_book();
  void draw_book();
  void book_key(SDL_Keycode k);
  void note_tip(const std::string& s);
  // the debug and testing menu (debug.cpp)
  int dbg_tab_ = 0, dbg_scroll_ = 0, bug_next_ = 1;
  bool typing_ = false, dbg_skip_ = false, dbg_pending_ = false, tests_loaded_ = false;
  std::string note_, note_test_, dbg_msg_;
  std::map<std::string, int> tests_;  // 1 ok, 2 a bug
  std::vector<std::string> test_items() const;
  void load_tests();
  void save_tests();
  void open_debug();
  void close_debug();
  void debug_tab(int t);
  void debug_key(SDL_Keycode k);
  void debug_tool(int i);
  std::string bug_context() const;
  void save_bug();
  void draw_debug();
  void draw_ring();
  void draw_end();
  // a zone's book fragment, shown once when it is first entered; each zone keeps a bag so all five come before one again
  struct Epi { std::string title, text, source; int col; };
  std::deque<Epi> epis_;
  double epi_start_ = 0;
  int epi_floor_ = -1;
  std::map<std::string, std::vector<int>> bags_;
  // the Pyre: Sparks earned (all of them, spent or not), the gifts bought with them, the Frost levels opened and chosen,
  // and the zone bosses already paid out for, as "zone@frost"
  int sparks_ = 0, frost_max_ = 0, frost_ = 0, pyre_sel_ = 0;
  std::map<std::string, int> talents_;
  std::set<std::string> claimed_;
  void claim_sparks(const json& s);
  void draw_pyre();
  void draw_town();  // Charnfen (town.cpp)
  void draw_town_particles(SDL_Renderer* r, double ox, double oy, double cell);
  P town_fire_{-99, -99};
  void pyre_key(SDL_Keycode k);
  void queue_epigraph(const std::string& zone);
  void draw_epigraph();
  void draw_route();
  void put(int x, int y, int ch, int fg, int bg = -1);
  void text(int x, int y, const std::string& s, int fg, int bg = -1);
  int wrap(int x, int y, int w, const std::string& s, int fg, int max_lines = 99);
  void box(int x, int y, int w, int h, const std::string& title);
  void bar(int x, int y, int w, int val, int max, int col, const std::string& label);
  bool in_circle(int x, int y) const;
};
