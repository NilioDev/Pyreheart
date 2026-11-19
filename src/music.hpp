// Music that writes itself: slow dungeon synth, different every time. A drone holds the key, a choir holds the
// chords, a harp breaks them, a flute sings a phrase and answers it, drums keep a slow ritual; every few bars
// something changes. Each act has its own mood, and the Deep beats like a heart.
#pragma once
#include <atomic>
#include <cstdint>
#include <string>

namespace music {

// the first six are the old acts and keep their places; the zones after them came later
enum Mood { TITLE, CRYPT, CAVES, FORGE, DEEP, REST, CISTERN, CITY, VAULTS, BARRACKS, PRISON, SANCTUM, MOODS };

class Composer;

class Player {
 public:
  Player();
  ~Player();
  // opens the sound device and starts playing; false (and silence) if there is none
  bool open(int rate);
  void mood(Mood m, bool fight = false);  // a great foe awake makes it heavier
  void volume(float v);                   // 0 is off
  void renew();                           // a new piece in the same mood
  std::string describe() const;           // the key and tempo now playing
  float volume() const { return volume_; }

 private:
  Composer* c_ = nullptr;
  void* stream_ = nullptr;
  float volume_ = .7f;
};

// for previews and tests: renders seconds of a mood into interleaved stereo floats
void render(Mood m, bool fight, int rate, int seconds, uint64_t seed, float* out);

}  // namespace music
