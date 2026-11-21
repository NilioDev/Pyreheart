// The music lab plays in every mood and every state the game can put it in: no silence where there should be
// music, nothing not finite. (Classic has its own safety net: music_classic_test.)
#include <cmath>
#include <cstdio>
#include <vector>

#include "../src/music_lab.hpp"

int main() {
  using namespace music_lab;
  const int rate = 22050, secs = 12;
  int bad = 0, n = 0;
  std::vector<Scene> scenes(8);
  scenes[1].threat = 1; scenes[2].build = 0; scenes[3].build = 2; scenes[4].life = .1f; scenes[5].cold = 1;
  scenes[6].ghost = true; scenes[6].carrier = true; scenes[7].dead = true;
  for (int m = 0; m < MOODS; ++m)
    for (size_t k = 0; k < scenes.size(); ++k, ++n) {
      std::vector<float> s(size_t(rate) * secs * 2);
      render(Mood(m), k == 1, rate, secs, 99 + m * 7 + k, s.data(), scenes[k]);
      double sum = 0;
      bool finite = true;
      for (size_t i = 0; i < s.size() / 2; ++i) { if (!std::isfinite(s[i])) finite = false; sum += double(s[i]) * s[i]; }
      for (size_t i = s.size() / 2; i < s.size(); ++i) if (!std::isfinite(s[i])) finite = false;
      double rms = std::sqrt(sum / (s.size() / 2));
      if (!finite || rms < .004) { std::printf("lab mood %d scene %zu: %s (rms %.4f)\n", m, k, finite ? "silent" : "NOT FINITE", rms); ++bad; }
    }
  std::printf("music lab: %d of %d plays well\n", n - bad, n);
  return bad ? 1 : 0;
}
