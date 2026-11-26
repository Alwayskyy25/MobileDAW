// engine_link_stub.cpp - host (desktop) simulation of EngineLink for tests.
// Understands just enough of the command protocol to move a playhead:
// "play", "stop", "tempo <bpm>". Everything is logged (stubCommandLog()).
// NOT compiled into the Android app.
#include "engine_link.h"
#include <chrono>
#include <cmath>
#include <cstdio>
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
    if (inst) { inst->clear(); for (int t = 1; t <= 38; t++) inst->push_back(t); }
    if (fx) {
      fx->clear();
      for (int t = 0; t <= 52; t++) fx->push_back(t);
      for (int t = 103; t <= 151; t++) fx->push_back(t);  // FLM range (approx)
    }
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
      else if (line.rfind("seek ", 0) == 0) pos_ = std::atof(line.c_str() + 5);
      else if (line.rfind("loop ", 0) == 0) {
        double on, a, b;
        if (std::sscanf(line.c_str() + 5, "%lf %lf %lf", &on, &a, &b) == 3) { loopOn_ = on != 0; loopA_ = a; loopB_ = b; }
      }
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
      if (loopOn_ && loopB_ > loopA_ && pos_ >= loopB_) pos_ = loopA_ + std::fmod(pos_ - loopA_, loopB_ - loopA_);
      else if (pos_ >= 3840.0 * 64) pos_ = 0;  // safety wrap
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
  bool loopOn_ = false;
  double loopA_ = 0, loopB_ = 3840;
  std::chrono::steady_clock::time_point last_ = std::chrono::steady_clock::now();
};
}  // namespace

EngineLink* createEngineLink() { return new Stub(); }
void destroyEngineLink(EngineLink* e) { delete e; }

}  // namespace mdnative
