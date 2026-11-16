// The music: a small synthesizer and a composer that never plays the same piece twice.
// Everything is made here, sample by sample: no recordings.
#include "music.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

namespace music {
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
  int drum_kind;         // 0 none, 1 slow tom, 2 anvil and tom, 3 heartbeat, 4 drips
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
};

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

  void render(float* out, int frames) {
    for (int i = 0; i < frames; ++i) {
      // changing mood: fade out, begin anew, fade in
      int w = want_.load(std::memory_order_relaxed);
      bool wf = want_fight_.load(std::memory_order_relaxed);
      if (w != mood_ || wf != fight_) fading_ = true;
      if (fading_) {
        fade_ = std::max(0.f, fade_ - 1 / (1.5f * rate_));
        if (fade_ == 0) { begin(Mood(w), wf); fading_ = false; }
      } else fade_ = std::min(1.f, fade_ + 1 / (3.f * rate_));

      if (--step_left_ <= 0) { step(); step_left_ = step_len_; }
      float l, r;
      voice(l, r);
      float v = volume_.load(std::memory_order_relaxed) * fade_;
      out[2 * i] = std::tanh(l * v);
      out[2 * i + 1] = std::tanh(r * v);
    }
  }

 private:
  float rate_;
  Rand r_;
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
  std::array<Drum, 4> drums_;
  int next_drum_ = 0;
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
    float bpm = m_->bpm * (fight ? 1.15f : 1.f) * (.95f + .1f * r_.uni());
    step_len_ = int(rate_ * 60 / bpm / 2);  // eighth notes
    step_ = 0; bar_ = 0; step_left_ = 0;
    room_.feedback = .8f + .08f * m_->room;
    new_progression();
    new_arpeggio();
    new_melody();
    use_pad_ = true; use_harp_ = m_->harp > 0; use_flute_ = r_.chance(.6f); use_drums_ = m_->drum_kind != 0 || fight;
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
    if (s == 0 && step_ > 0) {
      ++bar_;
      // every eight bars something changes, so it never stands still
      if (bar_ % 8 == 0) {
        if (r_.chance(.5f)) new_progression();
        if (r_.chance(.4f)) new_melody();
        if (r_.chance(.3f)) new_arpeggio();
        if (r_.chance(.35f)) use_flute_ = !use_flute_;
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
        for (int d : ch)
          if (auto* p = quietest()) {
            p->f.set(m_->pad_cutoff * (fight_ ? 1.5f : 1.f), .8f, rate_);
            p->start(mtof(float(note(d, 1))), 1.6f, 2.8f);
          }
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
          if (n.at == pos) { flute_.play(mtof(float(note(n.deg, 2)))); flute_left_ = n.len; }
    }

    // bells: a low tolling in the crypt, water dripping in the caves
    if (m_->bells > 0) {
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

    // the drums
    if (use_drums_) {
      auto hit = [&](float f0, float f1, float decay, float noise, float g) {
        drums_[next_drum_].start(f0, f1, decay, noise, g * std::max(m_->drums, fight_ ? .7f : 0.f));
        next_drum_ = (next_drum_ + 1) % int(drums_.size());
      };
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
        default: break;
      }
    }
    ++step_;
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

    float pad = 0;
    for (auto& p : pad_) pad += p.tick(rate, 0);
    pad *= m_->pad;
    float harp = 0;
    for (auto& h : harp_) harp += h.tick();
    float fl = flute_.tick(rate, r_) * m_->flute;
    float bell = 0;
    for (auto& b : bells_) bell += b.tick(rate);
    float drum = 0;
    for (auto& d : drums_) drum += d.tick(rate, r_);
    drum *= .5f;

    float dry = drone + pad + harp + fl + bell + drum;
    float wl, wr;
    room_.tick(pad + harp * 1.2f + fl * 1.3f + bell * 1.5f + drum * .6f + drone * .3f, wl, wr);
    float wet = m_->room;
    outl = dry * .8f + wl * wet + harp * .1f;
    outr = dry * .8f + wr * wet - harp * .1f;
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

void Player::volume(float v) {
  volume_ = std::clamp(v, 0.f, 1.f);
  if (c_) c_->volume_ = volume_;
}

void render(Mood m, bool fight, int rate, int seconds, uint64_t seed, float* out) {
  Composer c(float(rate), seed);
  c.want_ = m;
  c.want_fight_ = fight;
  c.render(out, rate * seconds);
}

}  // namespace music
