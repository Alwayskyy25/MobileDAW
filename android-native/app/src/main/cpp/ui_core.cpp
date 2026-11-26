// ui_core.cpp - see ui_core.h. Immediate-mode: draw() rebuilds the DrawList and
// remembers widget rects; onTouch() hit-tests against the last frame's rects.
#include "ui_core.h"
#include <cmath>
#include <cstdio>
#include "cmds.h"

namespace mdnative {
namespace {
// Palette (dark, like the web app #1b1b1c family)
constexpr uint32_t BG = 0x1B1B1EFF, PANEL = 0x242428FF, PANEL2 = 0x2E2E34FF,
                   LINE = 0x3A3A42FF, TXT = 0xEEEEF2FF, DIM = 0x9A9AA6FF,
                   ACC = 0xFF8C2EFF, ACC2 = 0x3ECF6EFF, RED = 0xFF4444FF,
                   CELL = 0x33333CFF, CELLOFF = 0x26262CFF, WHITEKEY = 0xD8D8DEFF,
                   BLACKKEY = 0x17171BFF;

void rect(std::vector<DrawCmd>* o, Rect r, uint32_t c) {
  DrawCmd d; d.kind = DrawCmd::FillRect; d.r = r; d.color = c; o->push_back(d);
}
void text(std::vector<DrawCmd>* o, float x, float y, float scale, uint32_t c, const std::string& s) {
  DrawCmd d; d.kind = DrawCmd::Text; d.r = {x, y, 0, 0}; d.color = c; d.text = s; d.textScale = scale;
  o->push_back(d);
}
std::string f1(double v) { char b[32]; std::snprintf(b, sizeof b, "%.1f", v); return b; }
std::string f2(double v) { char b[32]; std::snprintf(b, sizeof b, "%.2f", v); return b; }
std::string i2(int v) { char b[32]; std::snprintf(b, sizeof b, "%d", v); return b; }
}  // namespace

void App::rawSend(const std::string& line) {
  std::string err = eng_->cmd(line + "\n");
  if (!err.empty()) status_ = "ENGINE: " + err.substr(0, 60);
}

// One-time engine setup, sent before the first user command: the two v1
// tracks get their instruments (commands.cpp TRACKINST; units.cpp:
// 3 = DrumSampler, 1 = MiniSynth), empty patterns, the song tempo, the
// 1-bar loop and the initial mixer volumes.
void App::ensureSetup() {
  if (setupDone_ || !eng_) return;
  setupDone_ = true;
  rawSend(std::string(cmds::kTrackInst) + " " + i2(cmds::kDrumTrack) + " " + i2(cmds::kInstDrumSampler));
  rawSend(std::string(cmds::kTrackInst) + " " + i2(cmds::kSynthTrack) + " " + i2(cmds::kInstMiniSynth));
  sendDrumPattern();
  sendNotePattern();
  rawSend(std::string(cmds::kTempo) + " " + f1(bpm_));
  rawSend(std::string(cmds::kLoop) + (loop_ ? " 1 0 " : " 0 0 ") + i2(kSteps * cmds::kTicksPerStep));
  for (int c = 0; c <= kChannels; c++)
    rawSend(std::string(cmds::kTrackVol) + " " + i2(c < kChannels ? c : cmds::kMasterStrip) + " " + f2(faders_[c]));
}

void App::send(const std::string& line) {
  if (!eng_) { status_ = "NO ENGINE: " + line; return; }
  ensureSetup();
  rawSend(line);
}

// The engine has no per-step toggle command: a pattern edit resends the
// track's whole note list (TRACKNOTES, stride 5: start, len, pitch, vel, pan;
// ticks, PPQ 960 - see prepNotes in commands.cpp). Drum steps are notes on
// the DrumSampler track with GM pad pitches (golden scenario inst-drums).
void App::sendDrumPattern() {
  std::string flat; int n = 0;
  for (int r = 0; r < kRows; r++)
    for (int s = 0; s < kSteps; s++)
      if (drums_[r][s]) {
        flat += " " + i2(s * cmds::kTicksPerStep) + " " + i2(cmds::kTicksPerStep / 2) + " " +
                i2(cmds::kDrumPitch[r]) + " 0.90 0";
        n++;
      }
  rawSend(std::string(cmds::kTrackNotes) + " " + i2(cmds::kDrumTrack) + " [" + i2(n * 5) + flat + "] " + i2(n));
}

void App::sendNotePattern() {
  std::string flat; int n = 0;
  for (int p = 0; p < kPitches; p++)
    for (int s = 0; s < kSteps; s++)
      if (notes_[p][s]) {
        flat += " " + i2(s * cmds::kTicksPerStep) + " " + i2(cmds::kTicksPerStep) + " " +
                i2(71 - p) + " 0.80 0";
        n++;
      }
  rawSend(std::string(cmds::kTrackNotes) + " " + i2(cmds::kSynthTrack) + " [" + i2(n * 5) + flat + "] " + i2(n));
}

int App::currentStep() const {
  if (snap_.posTicks < 0) return -1;
  return (int)(snap_.posTicks / cmds::kTicksPerStep) % kSteps;
}

void App::togglePlay() {
  wantPlay_ = !wantPlay_;
  send(wantPlay_ ? cmds::kPlay : cmds::kStop);
  status_ = wantPlay_ ? "PLAY" : "STOP";
}

Rect App::tabRect(View v) const { return tabR_[static_cast<int>(v)]; }

Rect App::stepRect(int row, int step) const {
  float cw = gridR_.w / kSteps, ch = gridR_.h / kRows;
  return {gridR_.x + step * cw + 2, gridR_.y + row * ch + 2, cw - 4, ch - 4};
}
Rect App::noteRect(int pr, int step) const {
  float cw = gridR_.w / kSteps, ch = gridR_.h / kPitches;
  return {gridR_.x + step * cw + 2, gridR_.y + pr * ch + 2, cw - 4, ch - 4};
}
Rect App::faderRect(int ch) const { return faderR_[ch]; }

void App::onTouch(float lx, float ly, int action) {
  if (action == 0) {  // down
    if (playR_.contains(lx, ly)) { togglePlay(); return; }
    if (bpmPlusR_.contains(lx, ly)) { bpm_ = std::fmin(240.0, bpm_ + 1); send(std::string(cmds::kTempo) + " " + f1(bpm_)); return; }
    if (bpmMinusR_.contains(lx, ly)) { bpm_ = std::fmax(40.0, bpm_ - 1); send(std::string(cmds::kTempo) + " " + f1(bpm_)); return; }
    if (metroR_.contains(lx, ly)) { metro_ = !metro_; send(std::string(cmds::kMetro) + (metro_ ? " 1" : " 0")); return; }
    if (loopR_.contains(lx, ly)) { loop_ = !loop_; send(std::string(cmds::kLoop) + (loop_ ? " 1 0 " : " 0 0 ") + std::to_string(kSteps * cmds::kTicksPerStep)); return; }
    for (int i = 0; i < 3; i++)
      if (tabR_[i].contains(lx, ly)) { view_ = static_cast<View>(i); return; }
    if (view_ == View::Drums) {
      for (int r = 0; r < kRows; r++)
        for (int s = 0; s < kSteps; s++)
          if (stepRect(r, s).contains(lx, ly)) {
            drums_[r][s] = !drums_[r][s];
            if (eng_) { ensureSetup(); sendDrumPattern(); }
            return;
          }
    } else if (view_ == View::Keys) {
      for (int p = 0; p < kPitches; p++)
        for (int s = 0; s < kSteps; s++)
          if (noteRect(p, s).contains(lx, ly)) {
            notes_[p][s] = !notes_[p][s];
            if (eng_) { ensureSetup(); sendNotePattern(); }  // row 0 = B4 (71)
            return;
          }
    } else {  // Mixer
      for (int c = 0; c <= kChannels; c++)
        if (faderR_[c].contains(lx, ly)) { dragFader_ = c; break; }
    }
  }
  if (action == 2 && dragFader_ >= 0) {  // move
    Rect t = faderR_[dragFader_];
    float v = 1.0f - (ly - t.y) / t.h;
    faders_[dragFader_] = v < 0 ? 0 : v > 1 ? 1 : v;
    send(std::string(cmds::kTrackVol) + " " + i2(dragFader_ < kChannels ? dragFader_ : cmds::kMasterStrip) + " " + f2(faders_[dragFader_]));
  }
  if (action == 1) dragFader_ = -1;  // up
}

void App::draw(std::vector<DrawCmd>* o) {
  o->clear();
  rect(o, {0, 0, kDesignW, kDesignH}, BG);
  // ---- top transport bar
  rect(o, {0, 0, kDesignW, 92}, PANEL);
  playR_ = {16, 16, 120, 60};
  rect(o, playR_, wantPlay_ ? RED : ACC2);
  text(o, playR_.x + 22, playR_.y + 20, 4, 0x101014FF, wantPlay_ ? "STOP" : "PLAY");
  bpmMinusR_ = {160, 16, 52, 60}; bpmPlusR_ = {292, 16, 52, 60};
  rect(o, bpmMinusR_, PANEL2); rect(o, bpmPlusR_, PANEL2);
  text(o, bpmMinusR_.x + 18, bpmMinusR_.y + 20, 4, TXT, "-");
  text(o, bpmPlusR_.x + 16, bpmPlusR_.y + 20, 4, TXT, "+");
  text(o, 222, 22, 4, TXT, i2((int)bpm_));
  text(o, 222, 52, 2, DIM, "BPM");
  metroR_ = {368, 16, 108, 60}; loopR_ = {488, 16, 108, 60};
  rect(o, metroR_, metro_ ? ACC : PANEL2); rect(o, loopR_, loop_ ? ACC : PANEL2);
  text(o, metroR_.x + 14, metroR_.y + 22, 3, TXT, "METRO");
  text(o, loopR_.x + 22, loopR_.y + 22, 3, TXT, "LOOP");
  // position + cpu + meters
  int cs = currentStep();
  text(o, 640, 22, 4, TXT, "STEP " + i2(cs < 0 ? 0 : cs + 1) + "/16");
  text(o, 640, 54, 2, DIM, "CPU " + i2(snap_.cpuPermille / 10) + "% X" + i2((int)snap_.xruns));
  rect(o, {900, 24, 300, 16}, PANEL2); rect(o, {900, 48, 300, 16}, PANEL2);
  rect(o, {900, 24, 300 * std::fmin(1.0f, snap_.meterMasterL), 16}, ACC2);
  rect(o, {900, 48, 300 * std::fmin(1.0f, snap_.meterMasterR), 16}, ACC2);
  // ---- view tabs
  const char* names[3] = {"DRUMS", "KEYS", "MIXER"};
  for (int i = 0; i < 3; i++) {
    tabR_[i] = {16 + i * 180.0f, 108, 164, 52};
    rect(o, tabR_[i], view_ == static_cast<View>(i) ? ACC : PANEL2);
    text(o, tabR_[i].x + 30, tabR_[i].y + 16, 3, TXT, names[i]);
  }
  text(o, 580, 122, 3, DIM, status_);
  // ---- body
  gridR_ = {140, 190, 1120, 470};
  if (view_ == View::Drums) {
    const char* rows[4] = {"KICK", "SNARE", "HAT", "PERC"};
    for (int r = 0; r < kRows; r++) {
      text(o, 24, gridR_.y + r * (gridR_.h / kRows) + 44, 3, DIM, rows[r]);
      for (int s = 0; s < kSteps; s++) {
        Rect c = stepRect(r, s);
        uint32_t col = drums_[r][s] ? ACC : (s % 4 == 0 ? CELL : CELLOFF);
        if (s == cs && wantPlay_) col = drums_[r][s] ? 0xFFD9A0FF : 0x555560FF;
        rect(o, c, col);
      }
    }
  } else if (view_ == View::Keys) {
    for (int p = 0; p < kPitches; p++) {
      int pc = (71 - p) % 12; bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
      rect(o, {16, gridR_.y + p * (gridR_.h / kPitches) + 2, 108, gridR_.h / kPitches - 4}, black ? BLACKKEY : WHITEKEY);
      for (int s = 0; s < kSteps; s++) {
        Rect c = noteRect(p, s);
        uint32_t col = notes_[p][s] ? ACC2 : (black ? 0x1E1E24FF : CELLOFF);
        if (s == cs && wantPlay_ && notes_[p][s]) col = 0xBFF5D2FF;
        rect(o, c, col);
      }
    }
  } else {  // Mixer
    const char* chn[5] = {"DRUM", "SYNTH", "CH3", "CH4", "MAST"};
    for (int c = 0; c <= kChannels; c++) {
      faderR_[c] = {120 + c * 230.0f, 200, 110, 420};
      rect(o, faderR_[c], PANEL);
      float v = faders_[c];
      rect(o, {faderR_[c].x, faderR_[c].y + faderR_[c].h * (1 - v), faderR_[c].w, faderR_[c].h * v}, c == kChannels ? ACC : ACC2);
      text(o, faderR_[c].x + 14, 640, 3, TXT, chn[c]);
      text(o, faderR_[c].x + 24, 176, 3, DIM, i2((int)(v * 100)));
    }
  }
  // footer hint
  text(o, 16, 688, 2, LINE, "MOBILEDAW NATIVE - C++ UI - ENGINE: MDAW C++/OBOE");
}

}  // namespace mdnative
