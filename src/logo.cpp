// The title screen's picture: PYREHEART in stone letters that glow from below like coals, a line under them,
// the mouth of the old Hearthwarden mine with the fire burning inside, smoke and embers.
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "client.hpp"

namespace {
// 8 by 7 letters; P and T hang two rows below the line, like the tails in the painted design
const std::vector<std::string> GLYPH_P = {"#######.", ".##...##", ".##...##", ".######.", ".##.....", ".##.....", ".##.....", ".##.....", "..#....."};
const std::vector<std::string> GLYPH_Y = {"###..###", ".##..##.", "..####..", "...##...", "...##...", "...##...", "..####.."};
const std::vector<std::string> GLYPH_R = {"#######.", ".##...##", ".##...##", ".######.", ".##.##..", ".##..##.", "###...##"};
const std::vector<std::string> GLYPH_E = {"########", ".##....#", ".##.#...", ".####...", ".##.#...", ".##....#", "########"};
const std::vector<std::string> GLYPH_H = {"###..###", ".##..##.", ".##..##.", ".######.", ".##..##.", ".##..##.", "###..###"};
const std::vector<std::string> GLYPH_A = {"...##...", "..####..", ".##..##.", ".##..##.", ".######.", ".##..##.", "###..###"};
const std::vector<std::string> GLYPH_T = {"########", "#..##..#", "...##...", "...##...", "...##...", "...##...", "...##...", "...##...", "...#...."};

const std::vector<std::string>& glyph(char c) {
  switch (c) {
    case 'P': return GLYPH_P;
    case 'Y': return GLYPH_Y;
    case 'R': return GLYPH_R;
    case 'E': return GLYPH_E;
    case 'H': return GLYPH_H;
    case 'A': return GLYPH_A;
    default: return GLYPH_T;
  }
}

// the mine, each row centred: = beam, | post, B timber, r R rock, f fire, . cobbles lit by the fire
const std::vector<std::string> MINE = {
    "rRRRRRRRRRRRr",
    "rrR=================Rrr",
    "rRRBB|BB=========BB|BBRRr",
    "rRRRBB|B   fffff   B|BBRRRr",
    "rRRRrBB|B  fffffff  B|BBrRRRr",
    "rRRrRRBB|B fffffffff B|BBRRrRRr",
    "rRRRRrRBB|B fffffffff B|BBRrRRRRr",
    "rRrRRRRRBB|B fffffffff B|BBRRRRRrRr",
    "rr  ..  BB|B...........B|BB  ..  rr",
    ".   .   ...........   .   .",
};
constexpr int TALL = 11;  // the letters are drawn 11 rows high from 7: square cells would make them squat

uint32_t hash3(int a, int b, int c) {
  uint32_t h = uint32_t(a) * 374761393u + uint32_t(b) * 668265263u + uint32_t(c) * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}
double noise(int a, int b, int c) { return (hash3(a, b, c) & 0xFFFF) / 65535.0; }

int rgb(double r, double g, double b) {
  auto c = [](double v) { return std::clamp(int(v), 0, 255); };
  return (c(r) << 16) | (c(g) << 8) | c(b);
}
int lerp(int a, int b, double f) {
  f = std::clamp(f, 0.0, 1.0);
  auto m = [&](int s) { int x = (a >> s) & 255, y = (b >> s) & 255; return int(x + (y - x) * f) << s; };
  return m(16) | m(8) | m(0);
}
// heat 0..1: black, deep red, orange, yellow, almost white
int fire_col(double h) {
  h = std::clamp(h, 0.0, 1.0);
  if (h < .3) return lerp(0x000000, 0x701000, h / .3);
  if (h < .6) return lerp(0x701000, 0xE85010, (h - .3) / .3);
  if (h < .85) return lerp(0xE85010, 0xFFB030, (h - .6) / .25);
  return lerp(0xFFB030, 0xFFF0B0, (h - .85) / .15);
}
}  // namespace

// the picture with its top row at y; returns the first free row under it
int Client::draw_logo(int y0) {
  int cw = con_.get_width(), ch = con_.get_height();
  double t = now();
  int tick = int(t * 8);

  // words: one line on a wide screen, PYRE over HEART on a narrow one
  std::vector<std::pair<std::string, int>> lines;  // word, gap between letters
  // on a narrow screen the letters lose their middle column: 7 wide, so HEART fits in 40
  if (cw >= 84) lines = {{"PYREHEART", 1}};
  else lines = {{"PYRE", 1}, {"HEART", 1}};
  const int gw = cw >= 84 ? 8 : 7;

  std::vector<char> letter(size_t(cw * ch), 0);
  struct Cell { int x, y, row, rows; };
  std::vector<Cell> cells;
  int y = y0;
  int line_y = 0, line_x0 = 0, line_x1 = 0;
  for (size_t li = 0; li < lines.size(); ++li) {
    auto& [word, gap] = lines[li];
    int w = int(word.size()) * gw + (int(word.size()) - 1) * gap;
    int x0 = (cw - w) / 2;
    for (size_t i = 0; i < word.size(); ++i) {
      auto& g = glyph(word[i]);
      // the tails only on the last line, where they can hang past the rule
      int tails = li + 1 == lines.size() ? int(g.size()) - 7 : 0;
      int rows = TALL + tails * 2;
      for (int r = 0; r < rows; ++r)
        for (int c = 0; c < gw; ++c) {
          // top, middle and foot bars two rows thick, so the letters stand heavy
          static const int ROWS[TALL] = {0, 0, 1, 2, 2, 3, 3, 4, 5, 6, 6};
          int sr = r < TALL ? ROWS[r] : 7 + (r - TALL) / 2;
          int sc = gw == 8 || c < 4 ? c : c + 1;
          if (g[sr][sc] == '#') {
            int x = x0 + int(i) * (gw + gap) + c, yy = y + r;
            if (x < 0 || x >= cw || yy < 0 || yy >= ch) continue;
            letter[yy * cw + x] = 1;
            cells.push_back({x, yy, r, TALL});
          }
        }
    }
    if (li + 1 == lines.size()) { line_y = y + TALL; line_x0 = std::max(1, x0 - 4); line_x1 = std::min(cw - 2, x0 + w + 3); }
    y += TALL + 2;
  }

  // smoke on both sides of the mine, drifting
  int mine_w = 0;
  for (auto& m : MINE) mine_w = std::max(mine_w, int(m.size()));
  int mine_x = (cw - mine_w) / 2, mine_y = line_y + 1;
  for (int yy = mine_y - 1; yy < mine_y + int(MINE.size()); ++yy)
    for (int x = 0; x < cw; ++x) {
      double dx = std::abs(x + .5 - cw / 2.0) - mine_w / 2.0 + 3;
      if (dx < 0 || dx > 18) continue;
      double s = std::sin(x * .45 + t * .9 + yy * .8) + std::sin(x * .17 - t * .6 + yy * 1.3) + std::sin(yy * .9 + t * 1.3);
      double fade = (1 - dx / 18) * (1 - double(yy - mine_y) / MINE.size() * .3);
      if (s * fade > .55) put(x, yy, s * fade > 1.2 ? 0x2592 : 0x2591, rgb(70 * fade + 25, 66 * fade + 22, 64 * fade + 22));
    }

  // the rule under the words, with a glowing stone in the middle
  if (line_y < ch) {
    for (int x = line_x0; x <= line_x1; ++x) put(x, line_y, 0x2550, 0x8C8278);
    put(line_x0, line_y, '<', 0x8C8278);
    put(line_x1, line_y, '>', 0x8C8278);
    int mid = cw / 2;
    put(mid - 1, line_y, 0x2569, 0x8C8278);
    put(mid + 1, line_y, 0x2569, 0x8C8278);
    put(mid, line_y, 0x2666, lerp(0xE85010, 0xFFC040, .5 + .5 * std::sin(t * 3)));
  }

  // the mine
  for (int r = 0; r < int(MINE.size()); ++r)
    for (int c0 = 0; c0 < int(MINE[r].size()); ++c0) {
      int c = c0 + (mine_w - int(MINE[r].size())) / 2;
      int x = mine_x + c, yy = mine_y + r;
      if (x < 0 || x >= cw || yy >= ch) continue;
      char k = MINE[r][c0];
      double light = 1 - std::abs(c - mine_w / 2.0) / (mine_w / 2.0);  // the fire lights what is near it
      double flick = .85 + .15 * std::sin(t * 7 + c);
      switch (k) {
        case '=': put(x, yy, 0x2550, lerp(0x5A3C22, 0xC07030, light * .6 * flick), 0x140A04); break;
        case '|': put(x, yy, 0x2551, lerp(0x5A3C22, 0xD08038, light * .8 * flick), 0x0A0502); break;
        case 'B': put(x, yy, 0x2588, lerp(0x3A2614, 0x8A5426, light * .7 * flick)); break;
        case 'r': case 'R': {
          int base = lerp(k == 'r' ? 0x2A2622 : 0x403A34, 0x58504A, noise(x, yy, 5) * .5);
          put(x, yy, 0x2588, lerp(base, 0x803810, light * light * .5 * flick));
          break;
        }
        case 'f': {
          // hotter low down and in the middle, licking upward
          double depth = double(r - 3) / 4.0, mid = 1 - std::abs(c - (mine_w - 1) / 2.0) / 5.5;
          double h = .25 + .45 * depth + .35 * mid + .35 * (noise(c, r, tick) - .5) + .1 * std::sin(t * 9 + c * 1.7 + r);
          int col = fire_col(h);
          int glyphs[] = {' ', 0x2591, 0x2592, 0x2593};
          int g = glyphs[hash3(c, r, tick / 2) % 4];
          put(x, yy, g == ' ' ? ' ' : g, fire_col(h + .25), col);
          break;
        }
        case '.': put(x, yy, noise(c, r, 7) < .5 ? 0x00B7 : 0x2219, lerp(0x3A2A20, 0xE07020, light * flick)); break;
        default: break;
      }
    }

  // the letters: grey stone above, red-hot below, with cracks, glowing round their edges
  for (auto& c : cells) {
    double f = (c.row - TALL * .3) / (TALL * .75);  // 0 at the top third, 1 at the foot, more on the tails
    double flick = .1 * std::sin(t * 2.3 + c.x * .3) + .08 * (noise(c.x, c.y, tick / 3) - .5);
    int stone = lerp(0x9A928A, 0x5A524C, noise(c.x, c.y, 1) * .6);
    int col = lerp(stone, fire_col(.68 + .3 * f + flick), std::clamp(f * 1.2 + flick, 0.0, 1.0));
    if (noise(c.x, c.y, 2) < .1) col = lerp(col, 0x000000, .45);  // cracks in the stone
    put(c.x, c.y, 0x2588, col);
  }
  for (auto& c : cells)
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        int x = c.x + dx, yy = c.y + dy;
        if (x < 0 || x >= cw || yy < 0 || yy >= ch || letter[yy * cw + x]) continue;
        double f = std::clamp((c.row - 1.0) / (TALL - 1.0), 0.0, 1.0) * (.75 + .25 * std::sin(t * 2 + c.x * .2));
        auto& tile = con_.at({x, yy});
        int glow = rgb(70 * f + 8, 18 * f + 2, 0);
        if (((tile.bg.r << 16) | (tile.bg.g << 8) | tile.bg.b) < glow) tile.bg = {uint8_t(glow >> 16), uint8_t((glow >> 8) & 255), 0, 255};
      }

  // embers: rising from the fire and drifting up past the letters
  for (int i = 0; i < 46; ++i) {
    double life = 3 + 3 * noise(i, 0, 9), speed = 2.5 + 3 * noise(i, 1, 9);
    double age = std::fmod(t + life * noise(i, 2, 9), life);
    bool from_fire = i % 3 == 0;
    double sx = from_fire ? cw / 2.0 - 4 + 8 * noise(i, 3, 9) : (noise(i, 4, 9) < .5 ? cw * .25 : cw * .72) + (noise(i, 3, 9) - .5) * cw * .3;
    double sy = from_fire ? mine_y + 3 : line_y - 2;
    int x = int(sx + std::sin(age * 1.7 + i) * 1.5), yy = int(sy - age * speed);
    if (x < 0 || x >= cw || yy < 0 || yy >= ch || letter[yy * cw + x]) continue;
    double fade = 1 - age / life;
    put(x, yy, fade > .6 ? 0x2219 : fade > .3 ? 0x00B7 : '.', fire_col(.4 + .5 * fade));
  }
  return mine_y + int(MINE.size()) + 1;
}
