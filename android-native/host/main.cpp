// host/main.cpp - desktop smoke + behaviour tests for the full native UI
// (ui_core + all screen modules + ui_model) against the stub engine.
// Every check prints PASS/FAIL; exit code = number of failures (capped).
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "engine_link_stub.h"
#include "ui_core.h"
#include "ui_model.h"

using namespace mdnative;

static int fails = 0, total = 0;
static void check(bool ok, const std::string& what) {
  total++;
  std::printf("%s %s\n", ok ? "PASS" : "FAIL", what.c_str());
  if (!ok) fails++;
}
static bool logHas(const std::string& sub) {
  for (const std::string& l : stubCommandLog())
    if (l.find(sub) != std::string::npos) return true;
  return false;
}
static void logClear() { stubCommandLog().clear(); }

static std::vector<DrawCmd> dc;
static const Hit* findHit(App& app, int id, int a = -999, int b = -999) {
  const Hit* found = nullptr;
  for (const Hit& h : app.hits())
    if (h.id == id && (a == -999 || h.a == a) && (b == -999 || h.b == b)) found = &h;
  return found;
}
static bool tap(App& app, int id, int a = -999, int b = -999) {
  app.draw(&dc);
  const Hit* h = findHit(app, id, a, b);
  if (!h) return false;
  float x = h->r.cx(), y = h->r.cy();
  app.onTouch(x, y, 0);
  app.onTouch(x, y, 1);
  app.draw(&dc);
  return true;
}
static bool tapAt(App& app, float x, float y) {
  app.onTouch(x, y, 0);
  app.onTouch(x, y, 1);
  app.draw(&dc);
  return true;
}
static void drag(App& app, float x0, float y0, float x1, float y1) {
  app.onTouch(x0, y0, 0);
  app.onTouch(x1, y1, 2);
  app.onTouch(x1, y1, 1);
  app.draw(&dc);
}

int main() {
  // ---------------------------------------------------------- model (pure)
  check(instDef(1) && std::string(instDef(1)->name).find("Mini Synth") == 0, "model: instDef(1) = Mini Synth*");
  check(instDef(1) && instDef(1)->params.size() == 7, "model: Mini Synth has 7 params");
  check(instDef(999) == nullptr, "model: instDef(999) = nullptr");
  check(devDef(0) != nullptr && devDef(52) != nullptr, "model: devDef range exists");
  {
    Project p = makeDefaultProject();
    check(p.tracks.size() == 2 && p.returns.size() == 1, "model: default project 2 tracks + 1 return");
    check(buildSig(p) == "setSig 960 4\n", "model: setSig 4/4");
    p.sigNum = 3; p.sigDen = 4;
    check(buildSig(p) == "setSig 960 3\n", "model: setSig 3/4");
    p.sigNum = 6; p.sigDen = 8;
    check(buildSig(p) == "setSig 480 6\n", "model: setSig 6/8");
    p.sigNum = 4; p.sigDen = 4;
    Clip c;
    c.start = 3840; c.len = 7680; c.patternLen = 3840;
    c.notes = {{0, 240, 60, 0.9f, 0}, {3600, 480, 62, 0.8f, 0}};
    std::vector<Note> f = flattenClipNotes(c);
    check(f.size() == 4, "model: flatten loops pattern over 2 cycles");
    check(f.size() == 4 && f[0].start == 3840 && f[1].start == 7440 && f[1].len == 240,
          "model: flatten cuts note at cycle end");
    check(f.size() == 4 && f[2].start == 7680 && f[3].start == 11280, "model: flatten cycle 2 positions");
    Track t;
    t.kind = kMidi;
    t.clips = {c};
    std::string tn = buildTrackNotes(5, t);
    check(tn.find("trackNotes 5 [24 ") == 0 && tn.size() > 10 && tn.compare(tn.size() - 6, 6, "] 4 6\n") == 0,
          "model: trackNotes stride 6 format");
    FxDevice dv;
    dv.type = 4;
    ensureParams(&dv);
    Track t2;
    t2.devices = {dv};
    std::string bd = buildTrackDevices(0, t2, 1);
    check(bd.find("removeDevice 0 0") == 0 && bd.find("insertDevice 0 0 4") != std::string::npos,
          "model: buildTrackDevices remove+insert");
    check(bd.find(" 1000 ") != std::string::npos && bd.find(" 1001 ") != std::string::npos,
          "model: buildTrackDevices sends wet mix/gain");
    Project p2 = makeDefaultProject();
    AutoLane l;
    l.kind = 0; l.track = 0; l.dev = -1; l.param = 0;
    l.pts = {{0, 0.85}, {3840, 0.5}};
    p2.autoLanes = {l};
    check(buildAutomation(p2) == "automation [9 0 0 -1 0 2 0 0.85 3840 0.5] 9\n", "model: automation flat format");
    Project p3 = makeDefaultProject();
    p3.bpm = 133.5;
    p3.name = "My Song";
    p3.tracks[0].clips[0].notes = {{0, 240, 36, 0.95f, 0}, {480, 240, 38, 0.9f, 0}};
    p3.tracks[1].instType = 5;
    std::string ser = serializeProject(p3);
    check(ser.rfind("MDAWN1", 0) == 0, "model: serialize header");
    Project back;
    std::string err;
    check(parseProject(ser, &back, &err), "model: parse round trip ok");
    check(back.bpm == 133.5 && back.name == "My Song" && back.tracks.size() == 2 &&
              back.tracks[0].clips[0].notes.size() == 2 && back.tracks[1].instType == 5,
          "model: round trip preserves fields");
    Project junk;
    check(!parseProject("hello world", &junk, &err), "model: parse rejects garbage");
  }

  // ---------------------------------------------------------- app session
  App app;
  EngineLink* eng = createEngineLink();
  std::vector<int> pi, pf;
  eng->start(48000, &pi, &pf);
  check(pi.size() >= 38 && pf.size() >= 53, "stub: ported lists cover inst/fx");
  app.setEngine(eng);
  app.onSnapshot(Snapshot());
  app.draw(&dc);
  check(!dc.empty() && !app.hits().empty(), "app: first draw produces drawlist + hits");

  logClear();
  check(tap(app, 1008), "app: PLAY hit exists");
  check(app.wantPlay(), "app: wantPlay after PLAY tap");
  check(logHas("trackInst 0 3") && logHas("trackInst 1 1"), "app: setup sends instruments");
  check(logHas("tempo 120") && logHas("loop 1 0 3840") && logHas("play"), "app: setup tempo/loop + play");
  check(logHas("setSig 960 4"), "app: setup sends signature");
  tap(app, 1008);  // stop
  check(!app.wantPlay() && logHas("stop"), "app: STOP tap");

  // BPM drag (+100 px up = +10 BPM)
  app.draw(&dc);
  {
    const Hit* h = findHit(app, 1002);
    check(h != nullptr, "app: BPM hit exists");
    drag(app, h->r.cx(), h->r.cy(), h->r.cx(), h->r.cy() - 100);
    check(std::fabs(app.project().bpm - 130) < 0.01, "app: BPM drag 120 -> 130");
  }
  // Tempo window +1
  check(tap(app, 1003), "app: open tempo window");
  check(app.modal() == Modal::Tempo, "app: tempo modal open");
  tap(app, 9402);
  check(std::fabs(app.project().bpm - 131) < 0.01, "app: tempo +1 -> 131");
  tap(app, 9001);
  check(app.modal() == Modal::None, "app: tempo modal closed");

  // Add a drum track via +TRACK menu
  check(tap(app, 2044), "app: +TRACK hit");
  check(app.modal() == Modal::Menu, "app: add-track menu open");
  check(tap(app, 9100, -999, 2), "app: menu item DRUM TRACK");
  check(app.project().tracks.size() == 3 && app.project().tracks[2].kind == kDrum, "app: drum track added");

  // Piano roll: select Keys clip (track 1), maximise detail, tap-add a note
  check(tap(app, 2020, 1, 0), "app: select Keys clip");
  check(app.detailTab() == DetailTab::Clip && app.selClip() == 0, "app: clip editor open on Keys clip");
  tap(app, 8005);  // MAX detail
  app.draw(&dc);
  {
    const Hit* g = findHit(app, 3000);
    check(g != nullptr, "app: piano grid hit exists");
    // pitch 60 = row 12 (top pitch 72, rowH 22); tick 960 at ppt 0.11
    float x = g->r.x + 960 * 0.11f + 2;
    float y = g->r.y + 12 * 22 + 5;
    tapAt(app, x, y);
    const Clip& c = app.project().tracks[1].clips[0];
    check(c.notes.size() == 1 && c.notes[0].pitch == 60 && c.notes[0].start == 960,
          "app: piano tap-add note C4 @960");
    check(logHas("trackNotes 1 ["), "app: note add syncs trackNotes");
  }
  tap(app, 3005);  // DEL NOTE
  check(app.project().tracks[1].clips[0].notes.empty(), "app: DEL NOTE removes selected note");

  // Step editor: select drum clip (track 0), toggle step 0 of Kick
  check(tap(app, 2020, 0, 0), "app: select drum clip");
  app.draw(&dc);
  check(findHit(app, 3030, 0, 0) != nullptr, "app: step cell hit exists");
  tap(app, 3030, 0, 0);
  {
    const Clip& c = app.project().tracks[0].clips[0];
    check(c.notes.size() == 1 && c.notes[0].pitch == 36, "app: step toggle adds kick note");
  }
  app.undo();
  check(app.project().tracks[0].clips[0].notes.empty(), "app: undo removes step note");
  app.redo();
  check(app.project().tracks[0].clips[0].notes.size() == 1, "app: redo restores step note");

  // Mixer: fader + pan knob + automation
  check(tap(app, 1015), "app: switch to mixer");
  check(app.view() == View::Mixer, "app: mixer view");
  app.draw(&dc);
  {
    const Hit* f = findHit(app, 4020, 0);
    check(f != nullptr, "app: mixer fader hit exists");
    double before = app.project().tracks[0].vol;
    drag(app, f->r.cx(), f->r.cy(), f->r.cx(), f->r.cy() - 60);
    check(app.project().tracks[0].vol > before && logHas("trackVol 0"), "app: fader drag raises volume");
    const Hit* k = findHit(app, 4010, 4, 0);
    check(k != nullptr, "app: mixer pan knob hit exists");
    drag(app, k->r.cx(), k->r.cy(), k->r.cx(), k->r.cy() - 40);
    check(app.project().tracks[0].pan > 0 && logHas("trackPan 0"), "app: pan knob drag");
  }
  logClear();
  check(tap(app, 4033, 0), "app: AUTO button");
  check(tap(app, 9100, -999, 1), "app: automation menu VOLUME");
  check(app.modal() == Modal::EnvEdit, "app: envelope editor open");
  check(app.project().autoLanes.size() == 1, "app: automation lane created");
  app.draw(&dc);
  {
    const Hit* g = findHit(app, 9501);
    check(g != nullptr, "app: env graph hit exists");
    tapAt(app, g->r.cx(), g->r.cy());
    check(app.project().autoLanes[0].pts.size() == 3, "app: env tap adds point");
    check(logHas("automation"), "app: automation sync sent");
  }
  tap(app, 9001);
  check(tap(app, 1014), "app: back to arrange");

  // Device: change instrument + add FX
  check(tap(app, 8001), "app: device tab");
  logClear();
  check(tap(app, 5001), "app: CHANGE instrument");
  check(tap(app, 9100, -999, 2), "app: pick SuperSaw");
  check(app.project().tracks[0].instType == 2 && logHas("trackInst 0 2"), "app: instrument changed to SuperSaw");
  check(tap(app, 5030), "app: ADD FX groups");
  check(tap(app, 9100, -999, 0), "app: FX group 0");
  app.draw(&dc);
  {
    const Hit* first = findHit(app, 9100);
    check(first != nullptr, "app: FX type list shown");
    int type = first ? first->b : -1;
    tap(app, 9100, -999, type);
    check(app.project().tracks[0].devices.size() == 1 && logHas("insertDevice 0 0 " + std::to_string(type)),
          "app: FX inserted into chain");
  }
  app.draw(&dc);
  {
    const Hit* k = findHit(app, 5011, 0, 0);
    check(k != nullptr, "app: device param knob exists");
    double before = app.project().tracks[0].devices[0].params[0];
    if (k) drag(app, k->r.cx(), k->r.cy(), k->r.cx(), k->r.cy() + 120);  // long drag: crosses stepped params too
    check(k && app.project().tracks[0].devices[0].params[0] != before && logHas("devParam 0 0 0"), "app: devParam knob drag");
  }

  // Browser: apply GMS to the selected track
  logClear();
  check(tap(app, 1001), "app: open browser");
  check(app.browserOpen(), "app: browser open");
  check(tap(app, 7010, 30), "app: browser instrument DW row");
  check(app.project().tracks[0].instType == 30 && logHas("trackInst 0 30"), "app: browser applies instrument");
  tap(app, 7008);  // close browser
  check(!app.browserOpen(), "app: browser closed");

  // Keys: preview via pointer API on track 1
  tap(app, 2020, 1, 0);  // select Keys clip/track again
  tap(app, 8003);        // Keys tab
  app.draw(&dc);
  {
    Rect area;
    int whites;
    app.keysGeom(&area, &whites);
    float x = area.x + area.w * 0.25f, y = area.y + area.h * 0.8f;
    int pitch = app.keysPitchAt(x, y);
    check(pitch > 0, "app: keysPitchAt finds a pitch");
    logClear();
    app.onTouchP(x, y, 0, 3);
    check(logHas("preview 1 " + std::to_string(pitch)), "app: keys pointer previews note");
    app.onTouchP(x, y, 1, 3);
    check(logHas("preview 1 " + std::to_string(pitch) + " 0 0"), "app: keys pointer releases note");
  }

  // Save As "TEST" via project menu + keyboard; change BPM; reload
  check(tap(app, 1016), "app: project menu");
  check(tap(app, 9100, -999, 3), "app: SAVE AS menu item");
  check(app.modal() == Modal::Keyboard, "app: keyboard open");
  for (int i = 0; i < 20; i++) tap(app, 9202);  // BKSP: clear the prefilled name
  for (char ch : std::string("TEST")) {
    app.draw(&dc);
    const Hit* k = findHit(app, 9200, (int)ch);
    if (!k) { check(false, "app: keyboard key exists"); break; }
    app.onTouch(k->r.cx(), k->r.cy(), 0);
    app.onTouch(k->r.cx(), k->r.cy(), 1);
  }
  tap(app, 9203);  // OK
  check(app.modal() == Modal::None, "app: keyboard closed after save");
  {
    App::FileOp op;
    bool hasSave = false;
    while (app.takeFileOp(&op)) if (op.kind == 0 && op.name == "TEST") hasSave = true;
    check(hasSave, "app: save produced a file op for the shell");
  }
  double savedBpm = app.project().bpm;
  app.draw(&dc);
  {
    const Hit* h = findHit(app, 1002);
    drag(app, h->r.cx(), h->r.cy(), h->r.cx(), h->r.cy() + 100);  // unsaved change: -10 BPM
  }
  check(std::fabs(app.project().bpm - (savedBpm - 10)) < 0.01, "app: BPM changed after save");
  check(tap(app, 1001), "app: open browser for load");
  tap(app, 7005);  // PROJ tab
  app.draw(&dc);
  check(tap(app, 7023, 0), "app: load slot row");
  check(std::fabs(app.project().bpm - savedBpm) < 0.01, "app: load restores saved BPM");

  destroyEngineLink(eng);
  std::printf("----\n%d/%d checks passed\n", total - fails, total);
  return fails > 120 ? 120 : fails;
}
