// host/main.cpp - smoke test for the platform-neutral UI core + stub engine.
// Simulates real touches at widget centres (geometry taken from the App itself)
// and checks that the right engine commands are emitted and state changes stick.
#include <cmath>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>
#include "engine_link.h"
#include "engine_link_stub.h"
#include "ui_core.h"

using namespace mdnative;

static int failures = 0;
#define CHECK(cond, msg)                                  \
  do {                                                    \
    if (cond) { std::printf("  ok: %s\n", msg); }         \
    else { std::printf("  FAIL: %s\n", msg); failures++; } \
  } while (0)

static bool logHas(const std::string& prefix) {
  for (auto& l : stubCommandLog())
    if (l.rfind(prefix, 0) == 0) return true;
  return false;
}
static void tap(App& app, Rect r) {
  app.onTouch(r.cx(), r.cy(), 0);
  app.onTouch(r.cx(), r.cy(), 1);
}

int main() {
  EngineLink* eng = createEngineLink();
  std::vector<int> inst, fx;
  if (!eng->start(48000, &inst, &fx)) { std::printf("FAIL: stub start\n"); return 1; }
  eng->setActive(true);

  App app;
  app.setEngine(eng);
  std::vector<DrawCmd> list;
  app.draw(&list);
  CHECK(list.size() > 60, "draw list populated (rects+labels)");
  CHECK(app.view() == View::Drums, "starts on DRUMS view");
  CHECK(!app.stepOn(0, 0), "step (kick,1) initially off");

  // toggle a drum step on (first edit triggers the one-time engine setup)
  tap(app, app.stepRect(0, 0));
  CHECK(app.stepOn(0, 0), "tap toggles kick step 1 on");
  CHECK(logHas("trackInst 0 3"), "setup: drum track = DrumSampler (3)");
  CHECK(logHas("trackInst 1 1"), "setup: synth track = MiniSynth (1)");
  CHECK(logHas("trackNotes 0 [5 0 120 36 0.90 0] 1"), "drum pattern sent as trackNotes (kick 36 @ tick 0)");

  // tempo +
  tap(app, app.bpmPlusRect());
  CHECK(std::fabs(app.bpm() - 121.0) < 0.01, "BPM+ -> 121");
  CHECK(logHas("tempo 121"), "tempo command sent");

  // play: stub playhead must advance and wrap one bar
  tap(app, app.playRect());
  CHECK(app.localPlaying(), "play pressed");
  CHECK(logHas("play"), "play command sent");
  Snapshot snap;
  double prev = -1; bool advanced = false, wrapped = false;
  for (int i = 0; i < 110; i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    eng->poll(&snap);
    app.onSnapshot(snap);
    if (prev >= 0 && snap.posTicks > prev) advanced = true;
    if (prev > 1000 && snap.posTicks < prev) wrapped = true;
    prev = snap.posTicks;
  }
  CHECK(snap.playing, "snapshot reports playing");
  CHECK(advanced, "playhead advances with time");
  CHECK(wrapped, "playhead loops the bar");
  app.draw(&list);  // playhead highlight path exercised

  // keys view: add a note
  tap(app, app.tabRect(View::Keys));
  CHECK(app.view() == View::Keys, "tab switches to KEYS");
  app.draw(&list);
  tap(app, app.noteRect(0, 4));
  CHECK(app.noteOn(0, 4), "tap adds note at row 0 step 5");
  CHECK(logHas("trackNotes 1 [5 960 240 71 0.80 0] 1"), "note pattern sent as trackNotes (pitch 71 @ tick 960)");

  // mixer: drag master fader up
  tap(app, app.tabRect(View::Mixer));
  app.draw(&list);
  Rect fr = app.faderRect(App::kChannels);  // master
  float before = app.fader(App::kChannels);
  app.onTouch(fr.cx(), fr.y + fr.h * 0.9f, 0);
  app.onTouch(fr.cx(), fr.y + fr.h * 0.02f, 2);
  app.onTouch(fr.cx(), fr.y + fr.h * 0.02f, 1);
  CHECK(app.fader(App::kChannels) > before, "fader drag raises master gain");
  CHECK(logHas("trackVol 64"), "master volume sent as trackVol 64 (MASTER strip)");

  // stop
  tap(app, app.playRect());
  CHECK(!app.localPlaying(), "stop pressed");
  CHECK(logHas("stop"), "stop command sent");

  eng->setActive(false);
  destroyEngineLink(eng);
  std::printf(failures ? "HOST TEST: %d FAILURES\n" : "HOST TEST: ALL PASS\n", failures);
  return failures ? 1 : 0;
}
