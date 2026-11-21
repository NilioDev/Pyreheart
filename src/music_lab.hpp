// Music that writes itself: slow dungeon synth, different every time. A drone holds the key, a choir holds the
// chords, a harp breaks them, a flute sings a phrase and answers it, drums keep a slow ritual; every few bars
// something changes. Each act has its own mood, and the Deep beats like a heart.
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace music_lab {

// the first six are the old acts and keep their places; the zones after them came later
enum Mood { TITLE, CRYPT, CAVES, FORGE, DEEP, REST, CISTERN, CITY, VAULTS, BARRACKS, PRISON, SANCTUM, CHARNFEN, MOODS };

// short tunes over the music, in the key that is playing
enum Jingle { J_BOSS, J_SPARK, J_LOOT, J_VICTORY };

// what is going on in the game, for the lab's music to follow (plan.md section 4)
struct Scene {
  float threat = 0;   // 0 quiet .. 1 foes all around and awake
  int build = 1;      // the map of the zone: 0 the first, 1 the second, 2 the boss map
  float life = 1;     // the hero's life, 0..1
  float cold = 0;     // deeper down, or a higher Frost level: 0..1
  bool dead = false;  // the dive is over: the music freezes and sinks, then only the bell
  bool ghost = false; // co-op, fallen: the music far away, muffled
  bool carrier = false;  // co-op, bearing a friend's ember: a tense heartbeat
};

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
  void scene(const Scene& s);             // the lab: what the music follows
  void jingle(Jingle j);
  float volume() const { return volume_; }

 private:
  Composer* c_ = nullptr;
  void* stream_ = nullptr;
  float volume_ = .7f;
};

// for previews and tests: renders seconds of a mood into interleaved stereo floats
void render(Mood m, bool fight, int rate, int seconds, uint64_t seed, float* out, const Scene& scene = Scene{});
// the same, with the scene changing as it plays (every tenth of a second); jingles: bits of the jingles to play now
void render_script(Mood m, bool fight, int rate, int seconds, uint64_t seed, float* out, const std::function<Scene(float t, int& jingles)>& at);

}  // namespace music_lab
