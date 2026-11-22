// The music LAB: a copy of the classic music (src/music.cpp) to try new ideas in, sharing no code with it
// (plan.md section 4). What is new here: the Pyreheart and Ashen King motifs, threat that grows, the build-up over
// a zone's maps, Charnfen's own theme, jingles in the key, colder music deeper down, near death, dying, the
// waystation's fire, the ghost and the ember bearer, and planned silences.
// Everything is made here, sample by sample: no recordings.
#include "music_lab.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <numbers>
#include <random>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace music_lab {
namespace {

constexpr float TAU = 2 * std::numbers::pi_v<float>;

float mtof(float m) { return 440.f * std::pow(2.f, (m - 69.f) / 12.f); }

// ------------------------------------------------------------------ sound makers
struct Rand {
  std::mt19937_64 g;
  float uni() { return float(g() >> 40) / float(1 << 24); }          // 0..1
  float bi() { return uni() * 2 - 1; }                                 // -1..1
  int below(int n) { return n <= 1 ? 0 : int(g() % uint64_t(n)); }
  bool chance(float p) { return uni() < p; }
};

// a saw without the harsh fizz of a plain one
float blep(float t, float dt) {
  if (t < dt) { t /= dt; return t + t - t * t - 1; }
  if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
  return 0;
}
struct Saw {
  float ph = 0;
  float tick(float f, float rate) {
    float dt = f / rate;
    ph += dt;
    if (ph >= 1) ph -= 1;
    return 2 * ph - 1 - blep(ph, dt);
  }
};

// a resonant low-pass filter (state variable, trapezoid)
struct Filter {
  float s1 = 0, s2 = 0, a1 = 1, a2 = 0, a3 = 0;
  void set(float cutoff, float q, float rate) {
    float g = std::tan(std::numbers::pi_v<float> * std::min(cutoff, rate * .45f) / rate), k = 1 / q;
    a1 = 1 / (1 + g * (g + k)); a2 = g * a1; a3 = g * a2;
  }
  float tick(float x) {
    float v3 = x - s2, v1 = a1 * s1 + a2 * v3, v2 = s2 + a2 * s1 + a3 * v3;
    s1 = 2 * v1 - s1; s2 = 2 * v2 - s2;
    return v2;
  }
};

// a slow swell: rises in attack seconds, falls in release seconds once let go
struct Swell {
  float level = 0, attack = 1, release = 2;
  bool on = false;
  float tick(float rate) {
    if (on) level = std::min(1.f, level + 1 / (attack * rate));
    else level *= std::exp(-4.6f / (release * rate));  // to 1% in release seconds
    return level;
  }
  bool idle() const { return !on && level < 1e-4f; }
};

// the choir: each note two saws a little apart, soft and slow
struct PadVoice {
  Saw a, b, c;
  Filter f;
  Swell env;
  float freq = 0;
  void start(float fr, float att, float rel) { freq = fr; env.attack = att; env.release = rel; env.on = true; }
  float tick(float rate, float cutoff_mod) {
    if (env.idle()) return 0;
    float e = env.tick(rate);
    float x = a.tick(freq * 1.0015f, rate) + b.tick(freq * .9985f, rate) + .5f * c.tick(freq * .5f, rate);
    (void)cutoff_mod;
    return f.tick(x) * e * .16f;
  }
};

// the harp: a plucked string (a short delay line fed with noise that rings down)
struct Pluck {
  std::vector<float> line;
  float delay = 100, damp = .996f, gain = 0, bright = .5f;
  int at = 0;
  float last = 0;
  bool live = false;
  void start(float fr, float rate, Rand& r, float g, float br, float ring) {
    if (line.empty()) line.assign(size_t(rate / 30) + 2, 0);
    delay = std::min(float(line.size() - 2), rate / fr);
    // the pick: noise, softened, so the string sounds plucked by a finger, not a pick
    float lp = 0;
    for (size_t i = 0; i < line.size(); ++i) { lp += br * (r.bi() - lp); line[i] = lp; }
    gain = g;
    damp = ring;
    live = true;
    last = 0;
  }
  float tick() {
    if (!live) return 0;
    int n = int(line.size());
    float rd = float(at) - delay;
    while (rd < 0) rd += n;
    int i0 = int(rd), i1 = (i0 + 1) % n;
    float fr = rd - i0, y = line[i0] * (1 - fr) + line[i1] * fr;
    float out = .5f * (y + last) * damp;  // averaging two samples: the string loses its highs as it rings
    last = y;
    line[at] = out;
    at = (at + 1) % n;
    gain *= .99999f;
    if (std::fabs(out) < 1e-5f && gain < 1e-3f) live = false;
    return out * gain;
  }
};

// the flute: almost a sine, with breath and a late vibrato
struct Flute {
  float ph = 0, freq = 0, target = 0, age = 0, breath_lp = 0;
  Swell env;
  float tick(float rate, Rand& r) {
    if (env.idle()) return 0;
    float e = env.tick(rate);
    freq += (target - freq) * (1 - std::exp(-1 / (.04f * rate)));  // a short glide between notes
    age += 1 / rate;
    float vib = 1 + .004f * std::min(1.f, std::max(0.f, age - .35f) * 2) * std::sin(TAU * 5.2f * age);
    ph += freq * vib / rate;
    if (ph >= 1) ph -= 1;
    breath_lp += .03f * (r.bi() - breath_lp);
    float x = std::sin(TAU * ph) + .18f * std::sin(2 * TAU * ph) + .06f * std::sin(3 * TAU * ph);
    return (x + breath_lp * .5f) * e * .17f;
  }
  void play(float fr) {
    if (env.level < .05f) freq = fr;
    target = fr; age = 0; env.attack = .09f; env.release = .35f; env.on = true;
  }
  void stop() { env.on = false; }
};

// bells and anvils: two sines, one bending the other, fading
struct Bell {
  float t = 1e9f, freq = 0, ratio = 1.4f, index = 3, decay = 1, amp = 0;
  float tick(float rate) {
    if (t > 8 / decay) return 0;
    t += 1 / rate;
    float m = std::sin(TAU * freq * ratio * t) * index * std::exp(-t * decay * 1.5f);
    return std::sin(TAU * freq * t + m) * std::exp(-t * decay) * amp;
  }
  void start(float f, float r, float i, float d, float a) { t = 0; freq = f; ratio = r; index = i; decay = d; amp = a; }
};

// drums: a skin struck, its pitch falling, with a little noise
struct Drum {
  float t = 1e9f, f0 = 120, f1 = 50, decay = 5, amp = 0, ph = 0, noise = 0, nlp = 0;
  float tick(float rate, Rand& r) {
    if (t > 10 / decay) return 0;
    t += 1 / rate;
    float f = f1 + (f0 - f1) * std::exp(-t * 20);
    ph += f / rate;
    if (ph >= 1) ph -= 1;
    nlp += .25f * (r.bi() - nlp);
    return (std::sin(TAU * ph) + nlp * noise * std::exp(-t * 40)) * std::exp(-t * decay) * amp;
  }
  void start(float a, float b, float d, float n, float g) { t = 0; f0 = a; f1 = b; decay = d; noise = n; amp = g; ph = 0; }
};

// a big stone room: combs and all-passes (the old Schroeder way)
struct Room {
  struct Line { std::vector<float> b; int i = 0; float lp = 0; };
  std::array<Line, 6> comb_l, comb_r;
  std::array<Line, 3> ap_l, ap_r;
  float feedback = .86f, damp = .3f;
  void init(float rate) {
    const int comb[6] = {1116, 1188, 1277, 1356, 1422, 1491}, ap[3] = {556, 441, 341};
    float s = rate / 44100.f;
    for (int k = 0; k < 6; ++k) { comb_l[k].b.assign(size_t(comb[k] * s), 0); comb_r[k].b.assign(size_t((comb[k] + 23) * s), 0); }
    for (int k = 0; k < 3; ++k) { ap_l[k].b.assign(size_t(ap[k] * s), 0); ap_r[k].b.assign(size_t((ap[k] + 23) * s), 0); }
  }
  static float comb_tick(Line& l, float x, float fb, float d) {
    float y = l.b[l.i];
    l.lp = y * (1 - d) + l.lp * d;
    l.b[l.i] = x + l.lp * fb;
    if (++l.i >= int(l.b.size())) l.i = 0;
    return y;
  }
  static float ap_tick(Line& l, float x) {
    float y = l.b[l.i];
    l.b[l.i] = x + y * .5f;
    if (++l.i >= int(l.b.size())) l.i = 0;
    return y - x;
  }
  void tick(float in, float& outl, float& outr) {
    float l = 0, r = 0;
    for (int k = 0; k < 6; ++k) { l += comb_tick(comb_l[k], in, feedback, damp); r += comb_tick(comb_r[k], in, feedback, damp); }
    for (int k = 0; k < 3; ++k) { l = ap_tick(ap_l[k], l); r = ap_tick(ap_r[k], r); }
    outl = l * .12f; outr = r * .12f;
  }
};

// ------------------------------------------------------------------ the moods
struct MoodDef {
  int scale[7];
  float bpm;
  int roots[4];          // keys to choose from (MIDI note of the key's low root)
  float drone, pad, harp, flute, bells, drums, room, pad_cutoff;
  bool tritone;          // the drone's second note: a fifth, or the devil's interval
  int drum_kind;         // 0 none, 1 slow tom, 2 anvil and tom, 3 heartbeat, 4 drips; the later zones:
                         // 5 cistern, 6 city, 7 vaults, 8 barracks, 9 prison, 10 sanctum (see zone_sounds)
  std::vector<std::vector<int>> progressions;
};

const MoodDef MOODS_DEF[MOODS] = {
    // title: the minor of old ballads, a harp and a flute in a big hall
    {{0, 2, 3, 5, 7, 8, 10}, 58, {50, 52, 45, 47}, .5f, .7f, .8f, .8f, .1f, .35f, .9f, 1100, false, 1,
     {{0, 5, 3, 4}, {0, 3, 5, 4}, {0, 6, 5, 4}, {0, 5, 6, 4}, {0, 3, 0, 4}}},
    // the crypt: a choir, a low bell, a slow drum
    {{0, 2, 3, 5, 7, 8, 10}, 52, {50, 48, 45, 46}, .7f, .9f, .35f, .6f, .35f, .5f, 1.f, 800, false, 1,
     {{0, 5, 3, 4}, {0, 3, 6, 4}, {0, 5, 0, 6}, {0, 3, 0, 5}}},
    // the caves: the half-step of phrygian, water dripping somewhere, hardly a drum
    {{0, 1, 3, 5, 7, 8, 10}, 48, {45, 47, 43, 44}, .8f, .55f, .3f, .4f, .8f, .2f, 1.f, 650, false, 4,
     {{0, 1, 0, 6}, {0, 1, 3, 0}, {0, 6, 5, 1}, {0, 3, 1, 0}}},
    // the forge: harmonic minor, iron on iron
    {{0, 2, 3, 5, 7, 8, 11}, 64, {45, 47, 43, 48}, .7f, .7f, .2f, .45f, .1f, .8f, .8f, 1000, false, 2,
     {{0, 5, 3, 4}, {0, 3, 4, 0}, {0, 5, 4, 4}, {0, 1, 4, 0}}},
    // the deep: the drone splits on the tritone, and the stolen heart beats
    {{0, 1, 4, 5, 7, 8, 10}, 42, {40, 41, 43, 38}, 1.f, .6f, .15f, .35f, .3f, .9f, 1.f, 600, true, 3,
     {{0, 1, 0, 6}, {0, 1, 5, 1}, {0, 6, 1, 0}}},
    // the waystation: dorian, warmer, a harp to rest by
    {{0, 2, 3, 5, 7, 9, 10}, 66, {50, 52, 55, 48}, .35f, .55f, 1.f, .7f, 0, 0, .7f, 1400, false, 0,
     {{0, 3, 0, 6}, {0, 6, 3, 0}, {0, 2, 3, 6}}},
    // the cistern: still black water in a vast hall; slow drops fall far, a broken wheel turns in threes
    {{0, 2, 3, 5, 7, 8, 10}, 44, {43, 45, 41, 46}, .75f, .7f, .25f, .45f, 0, .35f, 1.f, 700, false, 5,
     {{0, 5, 6, 0}, {0, 3, 5, 0}, {0, 6, 3, 4}, {0, 5, 3, 6}}},
    // the city: the dorian of a proud people, a slow procession through empty streets, a far great bell
    {{0, 2, 3, 5, 7, 9, 10}, 54, {48, 50, 46, 45}, .42f, .68f, .55f, .7f, 0, .45f, 1.f, 1300, false, 6,
     {{0, 3, 6, 4}, {0, 6, 3, 0}, {0, 4, 3, 6}, {0, 2, 3, 4}}},
    // the vaults: a strange minor behind locked doors, a mechanism ticking, gold glinting in the dark
    {{0, 2, 3, 6, 7, 8, 11}, 60, {45, 47, 44, 46}, .7f, .8f, .9f, .55f, 0, .3f, .6f, 900, false, 7,
     {{0, 3, 4, 0}, {0, 5, 4, 0}, {0, 1, 0, 4}, {0, 3, 1, 4}}},
    // the barracks: the minor of old soldiers, a march drum that never sounded the retreat, a fife
    {{0, 2, 3, 5, 7, 8, 10}, 74, {45, 43, 47, 48}, .6f, .75f, 0, .65f, 0, .8f, .75f, 1000, false, 8,
     {{0, 6, 5, 4}, {0, 5, 6, 4}, {0, 3, 4, 4}, {0, 6, 0, 4}}},
    // the prison: locrian and the devil's interval, chains dragged over stone, a heavy door far away
    {{0, 1, 3, 5, 6, 8, 10}, 40, {42, 43, 41, 44}, .9f, .55f, .15f, .3f, 0, .6f, .95f, 550, true, 9,
     {{0, 1, 0, 4}, {0, 4, 1, 0}, {0, 6, 1, 0}, {0, 1, 4, 1}}},
    // the sanctum: a choir in black stone around an empty altar, temple bells, no drum but the bells
    {{0, 2, 3, 5, 7, 8, 10}, 44, {50, 48, 52, 47}, .48f, .75f, .3f, .55f, 0, .5f, 1.f, 1500, false, 10,
     {{0, 5, 3, 4}, {0, 3, 5, 6}, {0, 6, 5, 0}, {0, 5, 1, 0}}},
    // Charnfen (lab): cold and lonely, wind over the roofs, the small village bell; home
    {{0, 2, 3, 5, 7, 8, 10}, 50, {50, 52, 48, 47}, .3f, .5f, .6f, .75f, 0, 0, .85f, 900, false, 11,
     {{0, 5, 3, 6}, {0, 3, 5, 0}, {0, 6, 3, 4}}},
};

// the Pyreheart motif: five notes that come back in every zone (degrees of the scale, and where they fall in two
// bars of eighths); the Ashen King's answers it with the half step and the fall
struct Motif { int at[5], deg[5], len[5]; };
const Motif PYRE = {{0, 4, 6, 8, 12}, {0, 4, 3, 2, 4}, {4, 2, 2, 4, 4}};
const Motif KING = {{0, 3, 6, 10, 12}, {0, 1, 0, -2, -3}, {3, 3, 4, 2, 4}};

// arpeggio patterns over one bar of eight steps: 0..2 the chord's notes, 3 the root an octave up, -1 rest
const std::vector<std::array<int, 8>> ARPS = {
    {0, 1, 2, 3, 2, 1, -1, -1}, {0, -1, 1, -1, 2, -1, 3, -1}, {0, 2, 1, 3, -1, -1, 2, -1},
    {0, -1, -1, 2, -1, -1, 3, -1}, {0, 1, 2, -1, 3, 2, 1, -1}, {2, 1, 0, -1, -1, -1, -1, -1},
};

}  // namespace

// ------------------------------------------------------------------ the composer
class Composer {
 public:
  explicit Composer(float rate, uint64_t seed) : rate_(rate) {
    r_.g.seed(seed);
    room_.init(rate);
    begin(TITLE, false);
  }
  std::atomic<int> want_{TITLE};
  std::atomic<bool> want_fight_{false};
  std::atomic<float> volume_{.7f};
  std::atomic<int> renew_{0};          // raised to ask for a new piece in the same mood
  std::atomic<int> playing_root_{0}, playing_bpm_{0}, playing_mood_{TITLE};
  int renewed_ = 0;
  // the lab: what the game says is going on
  std::atomic<float> want_threat_{0}, want_life_{1}, want_cold_{0};
  std::atomic<int> want_build_{1};
  std::atomic<bool> want_dead_{false}, want_ghost_{false}, want_carrier_{false};
  std::atomic<int> jingles_{0};  // a bit for each jingle asked for

  void render(float* out, int frames) {
    for (int i = 0; i < frames; ++i) {
      // changing mood: fade out, begin anew, fade in. A great foe waking does not change the piece: it only
      // raises the threat (lab)
      int w = want_.load(std::memory_order_relaxed);
      fight_ = want_fight_.load(std::memory_order_relaxed);
      int rn = renew_.load(std::memory_order_relaxed);
      if (w != mood_ || rn != renewed_) { fading_ = true; renewed_ = rn; }
      if (fading_) {
        fade_ = std::max(0.f, fade_ - 1 / (1.5f * rate_));
        if (fade_ == 0) { begin(Mood(w), fight_); fading_ = false; }
      } else fade_ = std::min(1.f, fade_ + 1 / (3.f * rate_));
      follow();

      // dying: the tape slows and sinks to nothing; the piece is only made as fast as it is heard
      float l, r;
      if (tape_ < 1) {
        tape_pos_ += tape_;
        while (tape_pos_ >= 1) { tape_pos_ -= 1; prev_l_ = cur_l_; prev_r_ = cur_r_; make(cur_l_, cur_r_); }
        l = prev_l_ + (cur_l_ - prev_l_) * tape_pos_;
        r = prev_r_ + (cur_r_ - prev_r_) * tape_pos_;
        l *= tape_; r *= tape_;
      } else { make(l, r); prev_l_ = cur_l_ = l; prev_r_ = cur_r_ = r; }
      // after it: only the bell of Charnfen, far away
      float bell = 0;
      if (dead_ && tape_ <= 0) {
        if (--toll_left_ <= 0) { toll_left_ = int(4.5f * rate_); toll_.start(mtof(float(root_ - 12 + 24)), 1.41f, 2.2f, .35f, .2f); }
        bell = toll_.tick(rate_);
      }
      l += bell; r += bell;
      float v = volume_.load(std::memory_order_relaxed) * fade_;
      out[2 * i] = std::tanh(l * v);
      out[2 * i + 1] = std::tanh(r * v);
    }
  }

 private:
  // one sample of the piece: the eighth note's events, then every voice
  void make(float& l, float& r) {
    if (--step_left_ <= 0) { step(); step_left_ = step_len_; }
    voice(l, r);
  }
  // follows the game smoothly, sample by sample, so nothing switches hard
  void follow() {
    float k = 1 / rate_;
    float th = want_threat_.load(std::memory_order_relaxed);
    if (fight_) th = std::max(th, .85f);
    threat_ += (th - threat_) * k / (th > threat_ ? 1.f : 6.f);  // it comes in quickly, and ebbs away slowly
    float life = want_life_.load(std::memory_order_relaxed);
    float danger = std::clamp((.4f - life) / .2f, 0.f, 1.f);  // from 40% life, full at 20%
    danger_ += (danger - danger_) * k / (danger > danger_ ? .8f : 2.5f);
    cold_ += (want_cold_.load(std::memory_order_relaxed) - cold_) * k / 8;
    ghost_amt_ += ((want_ghost_.load(std::memory_order_relaxed) ? 1.f : 0.f) - ghost_amt_) * k / 2;
    carrier_ = want_carrier_.load(std::memory_order_relaxed);
    bool dead = want_dead_.load(std::memory_order_relaxed);
    if (dead && !dead_) { dead_ = true; toll_left_ = int(1.5f * rate_); }
    if (!dead && dead_) { dead_ = false; tape_ = 1; begin(Mood(mood_), fight_); }
    if (dead_ && tape_ > 0) tape_ = std::max(0.f, tape_ - k / 3.5f * (.4f + tape_));  // slower and slower, lower and lower
    // the planned silence: fade out over two bars, back in after it
    if (silence_left_ > 0) {
      --silence_left_;
      if (threat_ > .3f) {  // something happened in the silence: the threat breaks in with one hard blow
        silence_left_ = 0;
        drums_[next_drum_].start(62, 30, 2.f, .9f, 1.5f);
        next_drum_ = (next_drum_ + 1) % int(drums_.size());
      }
    }
    float bar = step_len_ * 8.f;
    sil_amt_ += ((silence_left_ > 0 ? 0.f : 1.f) - sil_amt_) / (silence_left_ > 0 ? 2 * bar : bar);
    if (sil_since_ < 1e9f) sil_since_ += k;
  }

  float rate_;
  Rand r_;
  // the lab's state
  float threat_ = 0, danger_ = 0, cold_ = 0, ghost_amt_ = 0, sil_amt_ = 1, sil_since_ = 0;
  bool carrier_ = false, dead_ = false, king_ = false;
  int build_ = 1, base_len_ = 1, silence_left_ = 0, toll_left_ = 0;
  float tape_ = 1, tape_pos_ = 0, prev_l_ = 0, prev_r_ = 0, cur_l_ = 0, cur_r_ = 0;
  Bell toll_, far_;
  Filter ghost_l_, ghost_r_;
  float muff_l_ = 0, muff_r_ = 0;
  float crackle_ = 0, crack_lp_ = 0, rumble_ = 0, wind_ = 0, wind_lp_ = 0, wind_lp2_ = 0;
  // a jingle: its notes, each on an eighth from now
  struct JNote { int at; int kind; int deg; int oct; float amp; };  // kind 0 harp, 1 bell, 2 flute
  std::vector<JNote> jq_;
  const MoodDef* m_ = &MOODS_DEF[TITLE];
  int mood_ = TITLE;
  bool fight_ = false, fading_ = false;
  float fade_ = 0;
  int root_ = 50, step_len_ = 1, step_left_ = 0, step_ = 0, bar_ = 0;
  std::vector<int> prog_;
  std::array<int, 8> arp_{};
  struct Note { int at, deg, len; };
  std::vector<Note> call_, answer_;
  bool use_pad_ = true, use_harp_ = true, use_flute_ = true, use_drums_ = true;
  float lfo_ = 0;
  int flute_left_ = 0;

  Saw drone_a_, drone_b_, drone_c_;
  Filter drone_f_;
  std::array<PadVoice, 8> pad_;
  std::array<Pluck, 6> harp_;
  int next_harp_ = 0;
  Flute flute_;
  std::array<Bell, 4> bells_;
  int next_bell_ = 0;
  std::array<Bell, 8> zbells_;  // the later zones' own bells, chains and coins
  int next_zbell_ = 0, rattle_ = 0, wheel_ = 0;
  std::array<Drum, 4> drums_;
  int next_drum_ = 0, far_left_ = 0;
  Room room_;

  int note(int deg, int octave) const {
    int o = deg >= 0 ? deg / 7 : -((6 - deg) / 7);
    int d = deg - o * 7;
    return root_ + 12 * (octave + o) + m_->scale[d];
  }
  std::array<int, 3> chord(int deg) const { return {deg, deg + 2, deg + 4}; }

  void begin(Mood m, bool fight) {
    mood_ = m;
    fight_ = fight;
    m_ = &MOODS_DEF[m];
    root_ = m_->roots[r_.below(4)];
    float bpm = m_->bpm * (1 - .08f * want_cold_.load(std::memory_order_relaxed)) * (.95f + .1f * r_.uni());  // colder, slower
    step_len_ = base_len_ = int(rate_ * 60 / bpm / 2);  // eighth notes; the threat quickens them (lab)
    ghost_l_.set(520, .7f, rate_); ghost_r_.set(520, .7f, rate_);
    build_ = want_build_.load(std::memory_order_relaxed);
    step_ = 0; bar_ = 0; step_left_ = 0;
    playing_root_ = root_; playing_bpm_ = int(bpm + .5f); playing_mood_ = m;
    room_.feedback = .8f + .08f * m_->room;
    new_progression();
    new_arpeggio();
    new_melody();
    use_pad_ = true; use_harp_ = m_->harp > 0; use_flute_ = r_.chance(.6f); use_drums_ = true;
    for (auto& p : pad_) p.env.on = false;
    flute_.stop();
  }
  void new_progression() {
    auto& pool = m_->progressions;
    prog_ = pool[r_.below(int(pool.size()))];
  }
  void new_arpeggio() { arp_ = ARPS[r_.below(int(ARPS.size()))]; }
  // a phrase over two bars, and its answer: the answer keeps the call's rhythm and comes home to the key note
  void new_melody() {
    call_.clear(); answer_.clear();
    // now and then the Pyreheart motif instead (often on the title, always at a victory); a great foe brings the
    // Ashen King's. The key and the chords stay as they are, and the flute bends a note of it now and then
    float want = mood_ == TITLE || mood_ == CHARNFEN ? .55f : .3f;
    king_ = fight_ && r_.chance(.6f);
    if (king_ || r_.chance(want)) {
      const Motif& mo = king_ ? KING : PYRE;
      for (int i = 0; i < 5; ++i) call_.push_back({mo.at[i], mo.deg[i], mo.len[i]});
      if (r_.chance(.5f)) { auto& n = call_[1 + r_.below(3)]; n.deg += r_.chance(.5f) ? 1 : -1; }
      answer_ = call_;
      answer_.back().deg = 0;
      return;
    }
    king_ = false;
    static const int lens[] = {2, 2, 3, 4, 4, 6};
    int at = r_.below(2) * 2, deg = r_.chance(.5f) ? 4 : 2;
    while (at < 14) {
      int len = std::min(lens[r_.below(6)], 16 - at);
      call_.push_back({at, deg, len});
      at += len + (r_.chance(.25f) ? 2 : 0);
      deg += r_.below(5) - 2;
      deg = std::clamp(deg, -2, 7);
    }
    for (size_t i = 0; i < call_.size(); ++i) {
      Note n = call_[i];
      if (i + 1 == call_.size()) n.deg = 0;
      else if (r_.chance(.5f)) n.deg = std::clamp(n.deg + r_.below(3) - 1, -2, 7);
      answer_.push_back(n);
    }
  }

  // every eighth note: what plays now
  void step() {
    int s = step_ % 8;
    if (s == 0) {  // at the bar line (lab): the build-up, the threat's tempo, a planned silence
      build_ = want_build_.load(std::memory_order_relaxed);
      step_len_ = int(base_len_ / (1 + .25f * threat_));
      if (bar_ % 8 == 7 && silence_left_ == 0 && sil_since_ > 120 && threat_ < .08f && danger_ < .1f && !dead_ &&
          mood_ != TITLE && r_.chance(.4f)) {
        silence_left_ = int(15 * rate_);
        sil_since_ = 0;
        far_left_ = int(5 * rate_);
      }
    }
    if (int j = jingles_.exchange(0)) queue_jingles(j);
    for (size_t i = 0; i < jq_.size();) {
      if (jq_[i].at-- > 0) { ++i; continue; }
      play_jnote(jq_[i]);
      jq_.erase(jq_.begin() + long(i));
    }
    if (s == 0 && step_ > 0) {
      ++bar_;
      // every eight bars something changes, so it never stands still
      if (bar_ % 8 == 0) {
        if (r_.chance(.5f)) new_progression();
        if (r_.chance(.4f)) new_melody();
        if (r_.chance(.3f)) new_arpeggio();
        if (r_.chance(.35f)) use_flute_ = !use_flute_;
        if (build_ == 0 && use_flute_ && r_.chance(.4f)) use_flute_ = false;  // the first map: calmer
        if (m_->harp > 0 && r_.chance(.2f)) use_harp_ = !use_harp_;
        if (r_.chance(.15f)) use_pad_ = !use_pad_;
        if (!use_pad_ && !use_harp_) use_pad_ = true;
      }
    }
    int deg = prog_[bar_ % prog_.size()];
    auto ch = chord(deg);

    // the choir: a new chord at each bar
    if (s == 0) {
      for (auto& p : pad_) p.env.on = false;
      if (use_pad_ && m_->pad > 0) {
        // each note goes to the quietest free voice, so the last chord can fade out in peace
        auto quietest = [&]() -> PadVoice* {
          PadVoice* best = nullptr;
          for (auto& p : pad_) if (!p.env.on && (!best || p.env.level < best->env.level)) best = &p;
          return best;
        };
        float cut = m_->pad_cutoff * (1 + .6f * threat_) * (1 - .4f * cold_);  // brighter as foes close in, duller in the cold
        for (int d : ch)
          if (auto* p = quietest()) {
            p->f.set(cut, .8f, rate_);
            p->start(mtof(float(note(d, 1))), 1.6f, 2.8f);
          }
        if (build_ >= 2)  // the boss map: the choir goes down an octave too
          if (auto* p = quietest()) { p->f.set(cut * .7f, .8f, rate_); p->start(mtof(float(note(deg, 0))), 1.6f, 2.8f); }
        // the deep: a half-step rubs against the chord
        if (m_->tritone && r_.chance(.4f))
          if (auto* p = quietest()) { p->f.set(m_->pad_cutoff, .8f, rate_); p->start(mtof(float(note(deg + 1, 1))), 3, 3); }
      }
    }

    // the harp breaks the chord
    if (use_harp_ && m_->harp > 0) {
      int a = arp_[s];
      if (a >= 0 && r_.chance(.75f + .25f * m_->harp)) {
        int n = a == 3 ? note(deg, 3) : note(ch[a], 2);
        harp_[next_harp_].start(mtof(float(n)), rate_, r_, .55f * m_->harp, .35f, .9985f);
        next_harp_ = (next_harp_ + 1) % int(harp_.size());
      }
    }

    // the flute: call over bars 0-1, answer over bars 4-5 of each eight
    if (flute_left_ > 0 && --flute_left_ == 0) flute_.stop();
    if (use_flute_ && m_->flute > 0) {
      int ph = bar_ % 8, pos = (ph % 4) * 8 + s;
      const std::vector<Note>* part = ph < 2 ? &call_ : (ph >= 4 && ph < 6) ? &answer_ : nullptr;
      if (part)
        for (auto& n : *part)
          if (n.at == pos) { flute_.play(mtof(float(note(n.deg, king_ ? 1 : 2)))); flute_left_ = n.len; }
    }

    // bells: a low tolling in the crypt, water dripping in the caves
    if (m_->bells > 0 && m_->drum_kind < 5) {
      if (m_->drum_kind == 4) {
        if (r_.chance(.12f * m_->bells)) {
          int n = note(r_.below(7), 4);
          bells_[next_bell_].start(mtof(float(n)), 3.5f, 1.2f, 3.5f, .12f);
          next_bell_ = (next_bell_ + 1) % int(bells_.size());
        }
      } else if (s == 0 && bar_ % 4 == 0 && r_.chance(m_->bells * 1.5f)) {
        bells_[next_bell_].start(mtof(float(note(0, 1))), 1.41f, 2.5f, .6f, .16f);
        next_bell_ = (next_bell_ + 1) % int(bells_.size());
      }
    }

    // icy glints, deeper down and higher on the Frost ladder
    if (cold_ > .05f && r_.chance(.035f * cold_)) {
      bells_[next_bell_].start(mtof(float(note(r_.below(7), 5))), 3.5f, .7f, 1.4f, .035f);
      next_bell_ = (next_bell_ + 1) % int(bells_.size());
    }
    // the drums
    if (use_drums_) {
      float bg = build_ == 0 ? .65f : build_ == 2 ? 1.2f : 1.f;
      auto hit = [&](float f0, float f1, float decay, float noise, float g) {
        drums_[next_drum_].start(f0, f1, decay, noise, g * std::max(m_->drums, .7f * threat_) * bg * (1 + .5f * threat_));
        next_drum_ = (next_drum_ + 1) % int(drums_.size());
      };
      auto beat = [&](float f0, float f1, float decay, float noise, float g) {  // a heartbeat, whatever the mood's drums
        drums_[next_drum_].start(f0, f1, decay, noise, g);
        next_drum_ = (next_drum_ + 1) % int(drums_.size());
      };
      // the threat brings drums in, step by step, and they ebb away again
      if (threat_ > .15f && (s == 0 || s == 4)) hit(95, 42, 4, .5f, .5f + .6f * threat_);   // a war drum on every beat
      if (threat_ > .45f && (s == 2 || s == 6)) hit(150, 70, 7, .6f, .45f * threat_);
      if (threat_ > .75f && (s == 3 || s == 7) && r_.chance(.6f)) hit(190, 90, 10, .8f, .3f * threat_);
      // near death a heart underneath, an echo of the Deep; the ember bearer's beats fast and tense
      if (danger_ > .05f) { if (s == 0 || s == 4) beat(110, 52, 5, .6f, 2.6f * danger_); if (s == 1 || s == 5) beat(100, 48, 6, .6f, 1.8f * danger_); }  // pitched up so small speakers carry it
      if (carrier_ && (s == 0 || s == 1 || s == 4 || s == 5)) beat(s % 4 ? 66 : 72, 36, 6, .3f, s % 4 ? .35f : .5f);
      int kind = m_->drum_kind;
      if (fight_ && kind == 0) kind = 1;
      switch (kind) {
        case 1:  // slow ritual: on the first beat every other bar, more in a fight
          if (s == 0 && (bar_ % 2 == 0 || fight_)) hit(110, 48, 3.5f, .6f, .9f);
          if (fight_ && (s == 4 || s == 6)) hit(140, 70, 6, .5f, .5f);
          break;
        case 2:  // the forge: the hammer falls on one and three, a drum under it
          if (s == 0 || s == 4) {
            bells_[next_bell_].start(r_.chance(.5f) ? 640.f : 710.f, 2.76f, 4.f, 3.5f, .13f);
            next_bell_ = (next_bell_ + 1) % int(bells_.size());
          }
          if (s == 0) hit(100, 45, 3, .7f, .9f);
          if (fight_ && s == 6) hit(120, 55, 5, .6f, .6f);
          break;
        case 3:  // the heart under the world: lub-dub, faster when he is awake
          if (s == 0) hit(70, 38, 5, .3f, 1.1f);
          if (s == 1) hit(65, 36, 6, .3f, .75f);
          if (fight_ && s == 4) { hit(70, 38, 5, .3f, 1.f); }
          if (fight_ && s == 5) { hit(65, 36, 6, .3f, .7f); }
          break;
        case 4:  // the caves: a far drum now and then
          if (s == 0 && r_.chance(.25f)) hit(90, 42, 2.5f, .4f, .6f);
          break;
        default: zone_sounds(s, hit); break;
      }
    }
    ++step_;
  }

  void zbell(float f, float ratio, float index, float decay, float amp) {
    zbells_[next_zbell_].start(f, ratio, index, decay, amp);
    next_zbell_ = (next_zbell_ + 1) % int(zbells_.size());
  }
  // the drums and noises of the later zones
  template <class Hit>
  void zone_sounds(int s, Hit& hit) {
    switch (m_->drum_kind) {
      case 5:  // the cistern: a drop into deep water, far off; a broken wheel knocking in threes
        if (r_.chance(.07f)) zbell(mtof(float(note(r_.below(5), 3))), 2.f, .8f, 1.6f, .1f);
        if (r_.chance(.03f)) zbell(mtof(float(note(0, 0))), .5f, 2.f, 2.5f, .1f);  // the sludge gives a bubble
        if (++wheel_ % 3 == 0 && (wheel_ % 24 < 12 || fight_)) hit(85, 40, 3.f, .5f, .55f);
        if (fight_ && s == 0) hit(105, 45, 3.5f, .6f, .9f);
        break;
      case 6:  // the city: a slow procession on one and three, a great bell far away every other bar
        if (s == 0) hit(95, 44, 2.8f, .5f, .8f);
        if (s == 4) hit(85, 42, 3.2f, .4f, .5f);
        if (s == 0 && bar_ % 2 == 0) zbell(mtof(float(note(0, 0))), 1.41f, 2.2f, .45f, .13f);
        if (fight_ && (s == 6 || s == 7)) hit(130, 60, 6, .6f, .45f);
        break;
      case 7:  // the vaults: the lock ticks, and now and then gold glints
        zbell(s % 2 ? 2600.f : 3100.f, 1.f, .2f, 60, s % 4 == 0 ? .05f : .025f);
        if (r_.chance(.06f)) zbell(mtof(float(note(r_.below(7), 4))), 3.01f, 1.5f, 2.2f, .08f);
        if (s == 0 && bar_ % 2 == 0) hit(70, 40, 2.5f, .3f, .7f);  // a heavy door
        if (fight_ && (s == 0 || s == 3 || s == 6)) hit(110, 50, 4, .5f, .7f);
        break;
      case 8: {  // the barracks: a march, the big drum on one and five, the snare between, rolls into the bar
        if (s == 0 || s == 4) hit(100, 46, 3.5f, .6f, .9f);
        bool roll = bar_ % 4 == 3 && s >= 5;
        if (s == 2 || s == 6 || roll || (fight_ && (s == 3 || s == 7)))
          hit(230, 190, 14, 2.5f, roll ? .3f + .1f * (s - 5) : .35f);
        break;
      }
      case 9:  // the prison: chains dragged in bursts, a heavy door now and then
        if (rattle_ == 0 && r_.chance(fight_ ? .1f : .04f)) rattle_ = 3 + r_.below(4);
        if (rattle_ > 0) { --rattle_; zbell(900 + 500 * r_.uni(), 3.71f, 4.f, 9, .06f + .03f * r_.uni()); }
        if (s == 0 && bar_ % 4 == 0) hit(60, 32, 2.f, .5f, 1.f);
        if (fight_ && s == 0) hit(75, 36, 3, .4f, .8f);
        if (fight_ && s == 3) hit(75, 36, 3, .4f, .6f);
        break;
      case 10:  // the sanctum: the temple bell on the bar, a small bell answers it
        if (s == 0) zbell(mtof(float(note(0, 1))), 1.41f, 2.5f, .5f, bar_ % 2 ? .08f : .14f);
        if (s == 4 && r_.chance(.5f)) zbell(mtof(float(note(r_.chance(.5f) ? 4 : 2, 3))), 2.76f, 1.2f, 1.2f, .06f);
        if (fight_ && (s == 0 || s == 4)) hit(80, 40, 2.5f, .5f, .9f);
        break;
      case 11:  // Charnfen (lab): the small village bell, three strokes now and then
        if (bar_ % 8 == 2 && (s == 0 || s == 3 || s == 6)) zbell(mtof(float(note(0, 3))), 2.76f, 1.4f, 1.1f, .07f);
        break;
      default: break;
    }
  }

  // ------------------------------------------------------------------ jingles (lab)
  void queue_jingles(int bits) {
    int at = 0;
    auto add = [&](int kind, int deg, int oct, float amp, int gap) { jq_.push_back({at, kind, deg, oct, amp}); at += gap; };
    int up = r_.chance(.5f) ? 1 : 2;  // a little different each time
    if (bits & (1 << J_BOSS)) {  // a great foe slain: the motif climbs on the harp, a bell under it
      for (int d : {0, 2, 4, 7}) add(0, d, 2, .7f, up);
      add(1, 0, 1, .22f, 0);
      at += 4;
    }
    if (bits & (1 << J_SPARK)) {  // a spark: three high glints
      for (int d : {4, 7, r_.chance(.5f) ? 9 : 11}) add(1, d, 4, .07f, 1);
      at += 2;
    }
    if (bits & (1 << J_LOOT)) {  // a rare find: two glints
      add(1, 0, 4, .06f, up);
      add(1, r_.chance(.5f) ? 4 : 2, 4, .06f, 0);
    }
    if (bits & (1 << J_VICTORY)) {  // the motif, grandly, on the flute, with the bells
      at = 0;
      for (int i = 0; i < 5; ++i) {
        jq_.push_back({PYRE.at[i], 2, PYRE.deg[i], 2, 1});
        jq_.push_back({PYRE.at[i], 0, PYRE.deg[i], 1, .6f});
      }
      jq_.push_back({0, 1, 0, 1, .25f});
      jq_.push_back({12, 1, 4, 2, .2f});
      use_flute_ = false;  // the flute is the motif's now
    }
  }
  void play_jnote(const JNote& n) {
    float f = mtof(float(note(n.deg, n.oct)));
    if (n.kind == 0) { harp_[next_harp_].start(f, rate_, r_, .55f * n.amp, .5f, .9988f); next_harp_ = (next_harp_ + 1) % int(harp_.size()); }
    else if (n.kind == 1) { bells_[next_bell_].start(f, n.oct >= 4 ? 3.01f : 1.41f, n.oct >= 4 ? 1.2f : 2.5f, n.oct >= 4 ? 1.8f : .5f, n.amp); next_bell_ = (next_bell_ + 1) % int(bells_.size()); }
    else { flute_.play(f); flute_left_ = 3; }
  }

  void voice(float& outl, float& outr) {
    float rate = rate_;
    // the drone: the key note low, with its fifth (or tritone), breathing slowly
    lfo_ += 1 / rate;
    float droot = mtof(float(root_ - 12));
    float second = droot * (m_->tritone ? 1.41421f : 1.5f);
    if ((step_ & 63) == 0) drone_f_.set(260 + 120 * std::sin(TAU * .05f * lfo_), .9f, rate);
    float drone = (drone_a_.tick(droot, rate) + .7f * drone_b_.tick(droot * 1.003f, rate) + .5f * drone_c_.tick(second, rate));
    drone = drone_f_.tick(drone) * (.8f + .2f * std::sin(TAU * .11f * lfo_)) * .22f * m_->drone;
    // the threat makes the drone throb on the eighths, and swell
    float throb = 1 - .55f * threat_ * (float(step_left_) / float(std::max(1, step_len_)));
    drone *= throb * (1 + .6f * threat_);

    // the lab's layers: near death the music thins out; in a planned silence all but the drone rests
    float thin = 1 - .85f * danger_, sil = sil_amt_;
    float pad = 0;
    for (auto& p : pad_) pad += p.tick(rate, 0);
    pad *= m_->pad * thin * sil * (1 + .4f * threat_);
    float harp = 0;
    for (auto& h : harp_) harp += h.tick();
    harp *= (1 - .9f * danger_) * sil;
    float fl = flute_.tick(rate, r_) * m_->flute * (1 - .95f * danger_) * sil;
    float bell = 0;
    for (auto& b : bells_) bell += b.tick(rate);
    float zb = 0;
    for (auto& b : zbells_) zb += b.tick(rate);
    bell += zb * sil;
    float drum = 0;
    for (auto& d : drums_) drum += d.tick(rate, r_);
    drum *= .5f;
    // a far sound in the silence
    if (far_left_ > 0 && --far_left_ == 0) far_.start(mtof(float(root_ - 12)), .5f, 2.f, .35f, .12f);
    bell += far_.tick(rate);

    // the waystation's fire crackles under the harp
    float fire = 0;
    if (mood_ == REST) {
      if (r_.uni() < 9 / rate) crackle_ = .5f + .5f * r_.uni();
      crackle_ *= .992f;
      crack_lp_ += .5f * (r_.bi() - crack_lp_);
      rumble_ += .004f * (r_.bi() - rumble_);
      fire = crack_lp_ * crackle_ * .09f + rumble_ * .35f;
    }
    // wind over Charnfen's roofs: noise through a slow, wandering filter
    if (mood_ == CHARNFEN || (mood_ == TITLE && cold_ > .5f)) {
      wind_ += 1 / rate;
      float gust = .5f + .5f * std::sin(TAU * .07f * wind_) * std::sin(TAU * .023f * wind_ + 1);
      float a = .004f + .02f * gust;
      wind_lp_ += a * (r_.bi() - wind_lp_);
      wind_lp2_ += a * (wind_lp_ - wind_lp2_);
      fire += wind_lp2_ * (1.2f + 1.5f * gust);
    }

    float dry = drone + pad + harp + fl + bell + drum + fire;
    float wl, wr;
    room_.tick(pad + harp * 1.2f + fl * 1.3f + bell * 1.5f + drum * .6f + drone * .3f, wl, wr);
    float wet = m_->room * (1 + .6f * ghost_amt_);
    outl = dry * .8f + wl * wet + harp * .1f;
    outr = dry * .8f + wr * wet - harp * .1f;
    // near death the world goes muffled, as if the ears ring; the heart stays clear underneath
    if (danger_ > .001f) {
      float c = 1 - .93f * danger_;
      muff_l_ += c * (outl - muff_l_); muff_r_ += c * (outr - muff_r_);
      outl = muff_l_ + drum * .9f * danger_; outr = muff_r_ + drum * .9f * danger_;
    } else { muff_l_ = outl; muff_r_ = outr; }
    // a ghost hears it all from far away, through stone
    if (ghost_amt_ > .001f) {
      float ml = ghost_l_.tick(outl), mr = ghost_r_.tick(outr), g = ghost_amt_;
      outl = (outl * (1 - g) + ml * g) * (1 - .4f * g);
      outr = (outr * (1 - g) + mr * g) * (1 - .4f * g);
    }
  }
};

// ------------------------------------------------------------------ the device
static void SDLCALL feed(void* user, SDL_AudioStream* stream, int additional, int) {
  auto* c = static_cast<Composer*>(user);
  static thread_local std::vector<float> buf;
  int frames = additional / int(2 * sizeof(float));
  if (frames <= 0) return;
  buf.resize(size_t(frames) * 2);
  c->render(buf.data(), frames);
  SDL_PutAudioStreamData(stream, buf.data(), frames * int(2 * sizeof(float)));
}

Player::Player() = default;
Player::~Player() {
  if (stream_) SDL_DestroyAudioStream(static_cast<SDL_AudioStream*>(stream_));
  delete c_;
}

bool Player::open(int rate) {
  if (stream_) return true;
  if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
  uint64_t seed = uint64_t(std::chrono::steady_clock::now().time_since_epoch().count());
  c_ = new Composer(float(rate), seed);
  c_->volume_ = volume_;
  SDL_AudioSpec spec{SDL_AUDIO_F32, 2, rate};
  auto* s = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed, c_);
  if (!s) return false;
  stream_ = s;
  SDL_ResumeAudioStreamDevice(s);
  return true;
}

void Player::mood(Mood m, bool fight) {
  if (!c_) return;
  c_->want_ = m;
  c_->want_fight_ = fight;
}

void Player::renew() {
  if (c_) ++c_->renew_;
}

std::string Player::describe() const {
  if (!c_) return "";
  static const char* names[] = {"C", "C sharp", "D", "E flat", "E", "F", "F sharp", "G", "A flat", "A", "B flat", "B"};
  static const char* modes[MOODS] = {"minor", "minor", "phrygian", "harmonic minor", "phrygian dominant", "dorian",
                                     "minor", "dorian", "hungarian minor", "minor", "locrian", "minor", "minor"};
  int root = c_->playing_root_, mood = std::clamp(int(c_->playing_mood_), 0, MOODS - 1);
  return std::string(names[root % 12]) + " " + modes[mood] + ", " + std::to_string(int(c_->playing_bpm_)) + " beats a minute";
}

void Player::scene(const Scene& s) {
  if (!c_) return;
  c_->want_threat_ = std::clamp(s.threat, 0.f, 1.f);
  c_->want_build_ = std::clamp(s.build, 0, 2);
  c_->want_life_ = std::clamp(s.life, 0.f, 1.f);
  c_->want_cold_ = std::clamp(s.cold, 0.f, 1.f);
  c_->want_dead_ = s.dead;
  c_->want_ghost_ = s.ghost;
  c_->want_carrier_ = s.carrier;
}

void Player::jingle(Jingle j) {
  if (c_) c_->jingles_ |= 1 << j;
}

void Player::volume(float v) {
  volume_ = std::clamp(v, 0.f, 1.f);
  if (c_) c_->volume_ = volume_;
}

void render(Mood m, bool fight, int rate, int seconds, uint64_t seed, float* out, const Scene& s) {
  Composer c(float(rate), seed);
  c.want_ = m;
  c.want_fight_ = fight;
  c.want_threat_ = s.threat; c.want_build_ = s.build; c.want_life_ = s.life; c.want_cold_ = s.cold;
  c.want_ghost_ = s.ghost; c.want_carrier_ = s.carrier;
  c.render(out, rate * seconds / 2);
  c.want_dead_ = s.dead;  // dying is heard from halfway
  c.render(out + size_t(rate * seconds / 2) * 2, rate * seconds - rate * seconds / 2);
}

void render_script(Mood m, bool fight, int rate, int seconds, uint64_t seed, float* out, const std::function<Scene(float, int&)>& at) {
  Composer c(float(rate), seed);
  c.want_ = m;
  c.want_fight_ = fight;
  int chunk = rate / 10, total = rate * seconds;
  for (int done = 0; done < total; done += chunk) {
    int j = 0;
    Scene s = at(float(done) / rate, j);
    c.want_threat_ = s.threat; c.want_build_ = s.build; c.want_life_ = s.life; c.want_cold_ = s.cold;
    c.want_ghost_ = s.ghost; c.want_carrier_ = s.carrier; c.want_dead_ = s.dead;
    if (j) c.jingles_ |= j;
    c.render(out + size_t(done) * 2, std::min(chunk, total - done));
  }
}

}  // namespace music_lab
