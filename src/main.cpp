// Pyreheart: opens the window, loads the font and runs the client.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstdlib>
#include <libtcod.hpp>
#include <string>

#include "client.hpp"
#include "glyphs.hpp"

static TCOD_Tileset* load_font() {
  // through SDL, so the same code reads the font from the APK's assets on Android
  size_t size = 0;
  void* buf = SDL_LoadFile("assets/glyphs.png", &size);
  if (!buf) {
    std::string base = SDL_GetBasePath() ? SDL_GetBasePath() : "";
    buf = SDL_LoadFile((base + "assets/glyphs.png").c_str(), &size);
  }
  if (!buf) buf = SDL_LoadFile("glyphs.png", &size);  // Android: the APK's assets folder
  if (!buf) return nullptr;
  TCOD_Tileset* ts = TCOD_tileset_load_mem(size, static_cast<unsigned char*>(buf), glyphs::COLUMNS, glyphs::ROWS,
                                           int(glyphs::CHARMAP.size()), glyphs::CHARMAP.data());
  SDL_free(buf);
  return ts;
}

// For testing and screenshots: PYREHEART_KEYS plays keys (one char each, '~' Enter, '^' Escape, '_' a pause),
// then PYREHEART_SHOT saves the screen to that file and quits.
static SDL_Keycode script_key(char c) {
  switch (c) {
    case '~': return SDLK_RETURN;
    case '^': return SDLK_ESCAPE;
    case 'U': return SDLK_UP;
    case 'D': return SDLK_DOWN;
    case 'L': return SDLK_LEFT;
    case 'R': return SDLK_RIGHT;
    case 'W': return 0;  // a mouse wheel turn: the watch crown
    default: return SDL_Keycode(c);
  }
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  TCOD_Tileset* ts = load_font();
  if (!ts) { std::fprintf(stderr, "Cannot read assets/glyphs.png: %s\n", SDL_GetError()); return 1; }
  auto tileset = tcod::TilesetPtr(ts);

  const char* lay = SDL_getenv("PYREHEART_LAYOUT");
  std::string l = lay ? lay : "";
  TCOD_ContextParams p{};
  p.tcod_version = TCOD_COMPILEDVERSION;
  p.tileset = tileset.get();
  p.vsync = 1;
  p.sdl_window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
  p.window_title = "Pyreheart";
#ifdef __ANDROID__
  p.sdl_window_flags |= SDL_WINDOW_FULLSCREEN;
#endif
  if (l == "watch") { p.pixel_width = 456; p.pixel_height = 456; }
  else if (l == "tall") { p.pixel_width = 480; p.pixel_height = 960; }
  else { p.columns = 100; p.rows = 50; }
  auto ctx = tcod::Context(p);

  Client client(ctx);
  const char* keys = SDL_getenv("PYREHEART_KEYS");
  const char* shot = SDL_getenv("PYREHEART_SHOT");
  std::string script = keys ? keys : "";
  size_t at = 0;
  int frames = 0;
  while (client.running) {
    client.frame();
    if (shot || keys) {
      if (++frames % 4 == 0 && at < script.size()) {
        char c = script[at++];
        SDL_Event ev{};
        if (c == '_') {
        } else if (c == 'W') {
          ev.type = SDL_EVENT_MOUSE_WHEEL; ev.wheel.y = -1; client.event(ev);
        } else {
          ev.type = SDL_EVENT_KEY_DOWN; ev.key.key = script_key(c); client.event(ev);
          if (c >= 32 && c < 127 && c != '~' && c != '^' && !(c >= 'A' && c <= 'Z')) {
            SDL_Event t{}; char s[2] = {c, 0}; t.type = SDL_EVENT_TEXT_INPUT; t.text.text = s; client.event(t);
          }
        }
      }
      if (at >= script.size() && shot && frames > int(script.size()) * 4 + 20) {
        // PYREHEART_SHOTS=n with a %d in the name saves n frames in a row, for an animation
        const char* many = SDL_getenv("PYREHEART_SHOTS");
        int n = many ? std::atoi(many) : 1;
        static int saved = 0;
        char name[512];
        std::snprintf(name, sizeof name, shot, saved);
        ctx.save_screenshot(name);
        if (++saved >= n) break;
      }
    }
    SDL_Event ev;
    if (SDL_WaitEventTimeout(&ev, 30)) {
      do client.event(ev); while (client.running && SDL_PollEvent(&ev));
    }
  }
  return 0;
}
