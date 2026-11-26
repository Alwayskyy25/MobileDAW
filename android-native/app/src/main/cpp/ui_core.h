// ui_core.h - platform-neutral UI of MobileDAW Native v1.
//
// No Android/GL includes here: the UI produces a DrawList (coloured rects +
// text runs in a fixed 1280x720 logical space) and consumes touch points in
// the same space. native_main.cpp scales real pixels to logical space and
// hands the DrawList to the GLES renderer. This file compiles and is tested
// on a desktop host (see ../../host/).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "engine_link.h"

namespace mdnative {

struct Rect {
  float x = 0, y = 0, w = 0, h = 0;
  bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
  float cx() const { return x + w / 2; }
  float cy() const { return y + h / 2; }
};

struct DrawCmd {
  enum Kind { FillRect, Text } kind = FillRect;
  Rect r;
  uint32_t color = 0xFFFFFFFFu;  // RGBA
  std::string text;              // Text only
  float textScale = 3;           // logical px per font pixel (font is 3x5)
};

enum class View { Drums, Keys, Mixer };

class App {
 public:
  static constexpr float kDesignW = 1280, kDesignH = 720;
  static constexpr int kRows = 4;          // drum rows: KICK SNARE HAT PERC
  static constexpr int kPitches = 12;      // piano roll rows (one octave)
  static constexpr int kSteps = 16;
  static constexpr int kChannels = 4;      // mixer channels (+ master)

  void setEngine(EngineLink* e) { eng_ = e; }
  // Shell-level status override (e.g. engine/audio failed at launch but the
  // app stays open) - drawn on the same status line as engine command errors.
  void setStatus(const std::string& s) { status_ = s; }
  void onSnapshot(const Snapshot& s) { snap_ = s; }
  void onTouch(float lx, float ly, int action);  // action: 0=down 1=up 2=move
  void draw(std::vector<DrawCmd>* out);

  // State (also used by the host smoke test)
  bool playing() const { return snap_.playing; }
  bool localPlaying() const { return wantPlay_; }
  double bpm() const { return bpm_; }
  bool stepOn(int row, int step) const { return drums_[row][step]; }
  bool noteOn(int pitchRow, int step) const { return notes_[pitchRow][step]; }
  float fader(int ch) const { return faders_[ch]; }
  View view() const { return view_; }
  const std::string& status() const { return status_; }

  // Widget geometry of the last draw() (logical coords) - for tests & input.
  Rect playRect() const { return playR_; }
  Rect bpmPlusRect() const { return bpmPlusR_; }
  Rect tabRect(View v) const;
  Rect stepRect(int row, int step) const;
  Rect noteRect(int pitchRow, int step) const;
  Rect faderRect(int ch) const;

 private:
  void send(const std::string& line);
  void rawSend(const std::string& line);
  void ensureSetup();
  void sendDrumPattern();
  void sendNotePattern();
  void togglePlay();
  int currentStep() const;

  EngineLink* eng_ = nullptr;
  Snapshot snap_;
  View view_ = View::Drums;
  bool wantPlay_ = false;
  bool setupDone_ = false;
  bool metro_ = false, loop_ = true;
  double bpm_ = 120.0;
  bool drums_[kRows][kSteps] = {};
  bool notes_[kPitches][kSteps] = {};
  float faders_[kChannels + 1] = {0.8f, 0.8f, 0.8f, 0.8f, 0.9f};  // + master
  int dragFader_ = -1;
  std::string status_ = "MOBILEDAW NATIVE V1";
  // last-frame geometry
  Rect playR_, bpmMinusR_, bpmPlusR_, metroR_, loopR_;
  Rect tabR_[3];
  Rect gridR_;   // step grid area (drums & keys share x layout)
  Rect faderR_[kChannels + 1];
};

}  // namespace mdnative
