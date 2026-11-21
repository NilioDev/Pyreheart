// The safety net under the classic music (plan.md section 4): every classic mood, and its fight variant, is rendered
// with a fixed seed and compared with a fingerprint taken when classic was frozen. Any change to src/music.cpp that
// changes what it plays fails here. The fingerprint is the loudness of each half second, kept to 1%,
// so the small float differences between compilers and machines pass but a different note, instrument or mix does not.
//   music_classic_test            compare with tests/music_classic_ref.hpp
//   music_classic_test --write    print a new reference (only when Niels has agreed to change classic)
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../src/music.hpp"
#include "music_classic_ref.hpp"

constexpr int RATE = 22050, SECS = 16, BLOCKS = SECS * 2;

static std::vector<float> fingerprint(int m, bool fight) {
  std::vector<float> s(size_t(RATE) * SECS * 2);
  music::render(music::Mood(m), fight, RATE, SECS, 777 + m, s.data());
  std::vector<float> fp(BLOCKS);
  size_t per = s.size() / BLOCKS;
  for (int b = 0; b < BLOCKS; ++b) {
    double sum = 0;
    for (size_t i = 0; i < per; ++i) { float x = s[b * per + i]; sum += double(x) * x; }
    fp[b] = float(std::sqrt(sum / per));
  }
  return fp;
}

static bool has_fight(int m) { return m != music::TITLE && m != music::REST; }

int main(int argc, char** argv) {
  bool write = argc > 1 && !std::strcmp(argv[1], "--write");
  if (write) std::printf("// written by music_classic_test --write: the fingerprint of the frozen classic music\n#pragma once\nstatic const float CLASSIC_REF[][%d] = {\n", BLOCKS);
  int row = 0, bad = 0;
  for (int m = 0; m < music::MOODS; ++m)
    for (int fight = 0; fight < (has_fight(m) ? 2 : 1); ++fight, ++row) {
      auto fp = fingerprint(m, fight);
      if (write) {
        std::printf("    {");
        for (int b = 0; b < BLOCKS; ++b) std::printf("%s%.6ff", b ? ", " : "", fp[b]);
        std::printf("},  // mood %d%s\n", m, fight ? " fight" : "");
        continue;
      }
      if (row >= int(sizeof(CLASSIC_REF) / sizeof(CLASSIC_REF[0]))) { std::printf("mood %d%s: no reference\n", m, fight ? " fight" : ""); ++bad; continue; }
      for (int b = 0; b < BLOCKS; ++b) {
        float want = CLASSIC_REF[row][b], got = fp[b];
        if (!std::isfinite(got) || std::fabs(got - want) > 5e-4f + .01f * want) {
          std::printf("classic music changed: mood %d%s, at %.1f s: loudness %.6f, was %.6f\n", m, fight ? " fight" : "", b * .5, got, want);
          ++bad;
          break;
        }
      }
    }
  if (write) { std::printf("};\n"); return 0; }
  std::printf("classic music: %s (%d moods and fight variants)\n", bad ? "CHANGED" : "unchanged", row);
  return bad ? 1 : 0;
}
