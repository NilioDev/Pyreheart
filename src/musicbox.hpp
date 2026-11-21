// The music the game plays: the frozen classic generator (src/music.cpp) or the lab (src/music_lab.cpp), never both
// at once. The two share no code (plan.md section 4); this only picks which one has the sound device.
#pragma once
#include <memory>
#include <string>

#include "music.hpp"
#include "music_lab.hpp"

class MusicBox {
 public:
  void open(int rate) { rate_ = rate; start(); }
  bool lab() const { return lab_; }
  void use_lab(bool on) {
    if (on == lab_) return;
    lab_ = on;
    if (rate_) start();
  }
  // lab_mood: a mood only the lab has (Charnfen's theme), or -1 for the lab to follow the classic one
  void mood(music::Mood m, bool fight = false, int lab_mood = -1) {
    mood_ = m; fight_ = fight; lab_mood_ = lab_mood;
    if (c_) c_->mood(m, fight);
    if (l_) l_->mood(music_lab::Mood(lab_mood >= 0 ? lab_mood : int(m)), fight);
  }
  void volume(float v) { vol_ = v; if (c_) c_->volume(v); if (l_) l_->volume(v); }
  void renew() { if (c_) c_->renew(); if (l_) l_->renew(); }
  std::string describe() const { return c_ ? c_->describe() : l_ ? l_->describe() : ""; }
  music_lab::Player* lab_player() { return l_.get(); }  // the lab's own hooks (threat, build-up, ...)

 private:
  void start() {
    c_.reset(); l_.reset();  // the one playing lets go of the sound device first
    if (lab_) { l_ = std::make_unique<music_lab::Player>(); l_->open(rate_); }
    else { c_ = std::make_unique<music::Player>(); c_->open(rate_); }
    volume(vol_);
    mood(mood_, fight_, lab_mood_);
  }
  bool lab_ = false, fight_ = false;
  int rate_ = 0, lab_mood_ = -1;
  float vol_ = .7f;
  music::Mood mood_ = music::TITLE;
  std::unique_ptr<music::Player> c_;
  std::unique_ptr<music_lab::Player> l_;
};
