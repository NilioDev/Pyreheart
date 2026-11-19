// Renders each mood of the music to a WAV file, to listen to without playing: music_preview <dir> [seconds]
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "music.hpp"

static void wav(const std::string& path, const std::vector<float>& s, int rate) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return;
  auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
  auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
  uint32_t bytes = uint32_t(s.size() * 2);
  std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
  u32(16); u16(1); u16(2); u32(rate); u32(rate * 4); u16(4); u16(16);
  std::fwrite("data", 1, 4, f); u32(bytes);
  for (float x : s) { int16_t v = int16_t(std::lround(std::fmax(-1, std::fmin(1, x)) * 32000)); std::fwrite(&v, 2, 1, f); }
  std::fclose(f);
}

int main(int argc, char** argv) {
  std::string dir = argc > 1 ? argv[1] : ".";
  int secs = argc > 2 ? std::atoi(argv[2]) : 60, rate = 44100;
  const char* names[] = {"title", "crypt", "caves", "forge", "deep", "rest", "cistern", "city", "vaults", "barracks", "prison", "sanctum"};
  for (int m = 0; m < music::MOODS; ++m)
    for (int fight = 0; fight < (m != music::TITLE && m != music::REST ? 2 : 1); ++fight) {
      std::vector<float> s(size_t(rate) * secs * 2);
      music::render(music::Mood(m), fight, rate, secs, 1234 + m, s.data());
      double sum = 0, peak = 0;
      bool bad = false;
      for (float x : s) { if (!std::isfinite(x)) bad = true; sum += x * x; peak = std::fmax(peak, std::fabs(x)); }
      std::printf("%-6s %s rms %.3f peak %.3f%s\n", names[m], fight ? "fight" : "     ", std::sqrt(sum / s.size()), peak, bad ? " NOT FINITE" : "");
      wav(dir + "/" + names[m] + (fight ? "-fight" : "") + ".wav", s, rate);
    }
}
