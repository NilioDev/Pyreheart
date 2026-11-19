// The player's side: screens, drawing and input. It only ever knows what the host sends it (see view.cpp).
#pragma once
#include <SDL3/SDL.h>

#include <chrono>
#include <deque>
#include <libtcod.hpp>
#include <memory>

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
  enum class Screen { Intro, Title, Name, Class, Join, Finding, Listen, Game };
  enum class Over { None, Pack, Perk, Shop, Help, Target, Menu, Ring };
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
  int ldist_floor_ = -1;
  std::string ldist_map_;
  void draw_particles(SDL_Renderer* r, double ox, double oy, double cell);
  void present(const TCOD_ViewportOptions& vp);
  void draw_side(int x, int y, int w, int h);
  void draw_status_tall(int y);
  void draw_log(int x, int y, int w, int h);
  void draw_buttons();
  void draw_pack();
  int draw_changes(int x, int y, int w, const json& it);
  void draw_perk();
  void draw_shop();
  void draw_help();
  void draw_menu();
  void draw_ring();
  void draw_end();
  void put(int x, int y, int ch, int fg, int bg = -1);
  void text(int x, int y, const std::string& s, int fg, int bg = -1);
  int wrap(int x, int y, int w, const std::string& s, int fg, int max_lines = 99);
  void box(int x, int y, int w, int h, const std::string& title);
  void bar(int x, int y, int w, int val, int max, int col, const std::string& label);
  bool in_circle(int x, int y) const;
};
