// engine_link.h - the ONLY interface between the native UI and the C++ audio engine.
//
// The Android implementation (engine_link.cpp) is adapted from the repo's own
// verified host, android/app/src/main/cpp/bridge.cpp: an Oboe low-latency stream
// whose audio callback drains a lock-free queue of mdaw::Cmd (built with
// mdaw::parseLine + mdaw::prepare from text lines) and renders via
// mdaw::Engine::render(L, R, n). The host-test implementation
// (engine_link_stub.cpp) simulates the same interface on a desktop machine.
//
// Command vocabulary: commands are the engine's text lines (one per line), the
// same protocol js/engine/nativesink.js uses against bridge.cpp. The exact
// spellings live in native/engine/commands.h of the repo and MUST be re-checked
// against cmds.h (ui namespace) before a device release - see NOTES.md.
#pragma once
#include <string>
#include <vector>

namespace mdnative {

struct Snapshot {
  double posTicks = 0;    // mdaw::Engine::tickPos()
  bool playing = false;   // mdaw::Engine::playing
  bool countIn = false;   // mdaw::Engine::cin > 0
  int cpuPermille = 0;    // audio block cost, per mille of budget (bridge snap[3])
  long xruns = 0;         // Oboe stream xrun count
  long badBlocks = 0;     // blocks the host had to silence (NaN/inf guard)
  float meterMasterL = 0; // peak meters of the first strips, 0..1 (from fillMeters)
  float meterMasterR = 0;
  double pdcOutFrames = 0;
};

struct Stats {
  double blocks = 0, meanPct = 0, p50Pct = 0, p99Pct = 0, maxPct = 0;
  double xruns = 0, badBlocks = 0, burst = 0, buffer = 0, sampleRate = 0, latencyMs = -1;
};

class EngineLink {
 public:
  virtual ~EngineLink() = default;
  // Start the engine at the device's native sample rate.
  // Returns false if the audio stream could not be opened.
  // portedInst/portedFx list the engine type ids the C++ port supports
  // (mdaw::instPorted / mdaw::devPorted) - the UI greys out everything else.
  virtual bool start(int sampleRate, std::vector<int>* portedInst, std::vector<int>* portedFx) = 0;
  // Send newline-separated engine text commands; returns error lines ("" = ok).
  virtual std::string cmd(const std::string& text) = 0;
  // Open/close the output stream (foreground/background).
  virtual bool setActive(bool on) = 0;
  virtual bool poll(Snapshot* out) = 0;
  virtual void stats(Stats* out, bool reset) = 0;
};

// Implemented by engine_link.cpp (Android) and engine_link_stub.cpp (host test).
EngineLink* createEngineLink();
void destroyEngineLink(EngineLink*);

}  // namespace mdnative
