// engine_link_stub.cpp - host (desktop) simulation of EngineLink for tests.
// Understands just enough of the command protocol to move a playhead:
// "play", "stop", "tempo <bpm>". Everything is logged (stubCommandLog()).
// NOT compiled into the Android app.
#include "engine_link.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include "cmds.h"
#include "engine_link_stub.h"

namespace mdnative {

std::vector<std::string>& stubCommandLog() {
  static std::vector<std::string> log;
  return log;
}

namespace {
class Stub : public EngineLink {
 public:
  bool start(int sr, std::vector<int>* inst, std::vector<int>* fx) override {
    sr_ = sr;
    if (inst) *inst = {0, 1};        // pretend Mini Synth + SuperSaw are ported
    if (fx) *fx = {};
    last_ = std::chrono::steady_clock::now();
    return true;
  }
  std::string cmd(const std::string& text) override {
    size_t p = 0;
    while (p < text.size()) {
      size_t e = text.find('\n', p);
      if (e == std::string::npos) e = text.size();
      std::string line = text.substr(p, e - p);
      p = e + 1;
      if (line.empty()) continue;
      stubCommandLog().push_back(line);
      if (line == cmds::kPlay) playing_ = true;
      else if (line == cmds::kStop) { playing_ = false; pos_ = 0; }
      else if (line.rfind(cmds::kTempo, 0) == 0) bpm_ = std::atof(line.c_str() + 6);
    }
    return "";
  }
  bool setActive(bool on) override { active_ = on; last_ = std::chrono::steady_clock::now(); return true; }
  bool poll(Snapshot* out) override {
    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - last_).count();
    last_ = now;
    if (playing_ && active_) {
      // ticks per beat = steps-per-beat(4) * ticks-per-step
      pos_ += dt * (bpm_ / 60.0) * 4 * cmds::kTicksPerStep;
      double bar = cmds::kStepsPerBar * cmds::kTicksPerStep;
      if (pos_ >= bar) pos_ -= bar;  // v1 pattern loops one bar
    }
    out->posTicks = pos_;
    out->playing = playing_;
    out->cpuPermille = playing_ ? 42 : 3;
    double ph = pos_ / cmds::kTicksPerStep;
    out->meterMasterL = playing_ ? 0.4f + 0.3f * (float)std::fabs(std::sin(ph)) : 0;
    out->meterMasterR = out->meterMasterL * 0.9f;
    return true;
  }
  void stats(Stats* o, bool) override { *o = Stats(); o->sampleRate = sr_; o->latencyMs = 10; }

 private:
  int sr_ = 48000;
  bool playing_ = false, active_ = false;
  double bpm_ = 120, pos_ = 0;
  std::chrono::steady_clock::time_point last_ = std::chrono::steady_clock::now();
};
}  // namespace

EngineLink* createEngineLink() { return new Stub(); }
void destroyEngineLink(EngineLink* e) { delete e; }

}  // namespace mdnative
