// ui_topbar.cpp - top bar (web map section 1), the project menu, the Tempo
// window, the generic menu modal, the on-screen keyboard, Log/Help modals,
// and App::menuAction / keyboardDone (all menu behaviour lives here).
#include "ui_core.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include "ui_widgets.h"

namespace mdnative {
namespace {
std::string f1(double v) { char b[32]; std::snprintf(b, sizeof b, "%.1f", v); return b; }
std::string is(int v) { return std::to_string(v); }
}  // namespace

// ---------------------------------------------------------------- top bar
void App::drawTopbar(std::vector<DrawCmd>* o) {
  using namespace ui;
  rect(o, {0, 0, kDesignW, 56}, PANEL);
  rect(o, {0, 54, kDesignW, 2}, LINE);
  auto add = [&](Rect r, int id, int a = 0, int b = 0, double v = 0) { hits_.push_back({r, id, a, b, v}); };
  // left group
  Rect brw = {8, 8, 46, 40};
  buttonS(o, brw, "BRW", browserOpen_);
  add(brw, 1001);
  Rect bpmR = {60, 8, 96, 40};
  rect(o, bpmR, PANEL2);
  textC(o, {bpmR.x, bpmR.y + 2, bpmR.w, 22}, 3, TXT, f1(proj_.bpm));
  textC(o, {bpmR.x, bpmR.y + 24, bpmR.w, 14}, 1.8f, DIM, "BPM");
  add(bpmR, 1002);
  Rect sigR = {162, 8, 62, 40};
  rect(o, sigR, PANEL2);
  textC(o, sigR, 3, TXT, is(proj_.sigNum) + "/" + is(proj_.sigDen));
  add(sigR, 1003);
  Rect un = {230, 8, 42, 40}, re = {276, 8, 42, 40};
  buttonS(o, un, "UND", false);
  buttonS(o, re, "RED", false);
  add(un, 1004);
  add(re, 1005);
  // centre group: position, transport, loop
  double bar = barTicks();
  double pos = snap_.posTicks;
  long barN = bar > 0 ? (long)(pos / bar) : 0;
  double inBar = bar > 0 ? std::fmod(pos, bar) : 0;
  double beat = bar / (proj_.sigNum > 0 ? proj_.sigNum : 4);
  long beatN = beat > 0 ? (long)(inBar / beat) : 0;
  long tickN = (long)std::fmod(inBar, beat > 0 ? beat : 1);
  Rect posR = {452, 8, 128, 40};
  rect(o, posR, PANEL2);
  textC(o, posR, 3, TXT, is((int)barN + 1) + "." + is((int)beatN + 1) + "." + is((int)tickN));
  add(posR, 1006);
  Rect rw = {586, 8, 42, 40}, pl = {632, 8, 56, 40}, rc = {692, 8, 42, 40}, mt = {738, 8, 42, 40};
  buttonS(o, rw, "|<", false);
  buttonS(o, pl, wantPlay_ ? "STOP" : "PLAY", wantPlay_);
  buttonS(o, rc, "REC", recArm_);
  buttonS(o, mt, "MET", proj_.metro);
  add(rw, 1007);
  add(pl, 1008);
  add(rc, 1009);
  add(mt, 1010);
  Rect ls = {786, 8, 66, 40}, lt = {856, 8, 46, 40}, ll = {906, 8, 66, 40};
  rect(o, ls, PANEL2);
  textC(o, ls, 2.5f, DIM, is((int)(proj_.loopStart / bar) + 1));
  buttonS(o, lt, "LOP", proj_.loopOn);
  rect(o, ll, PANEL2);
  textC(o, ll, 2.5f, DIM, is((int)std::round(proj_.loopLen / bar)) + " BAR");
  add(ls, 1011);
  add(lt, 1012);
  add(ll, 1013);
  // right group
  text(o, 984, 14, 2, DIM, "CPU " + is(snap_.cpuPermille / 10) + "%");
  text(o, 984, 32, 2, DIM, "FPS " + is((int)fps_));
  Rect va = {1056, 8, 66, 40}, vm = {1126, 8, 66, 40}, mn = {1198, 8, 74, 40};
  buttonS(o, va, "ARR", view_ == View::Arrange);
  buttonS(o, vm, "MIX", view_ == View::Mixer);
  buttonS(o, mn, "MENU", false);
  add(va, 1014);
  add(vm, 1015);
  add(mn, 1016);
  // status strip (bottom overlay)
  rect(o, {0, 700, kDesignW, 20}, PANEL);
  text(o, 8, 705, 2, ACC, proj_.name);
  textClip(o, 300, 705, 2, DIM, status_, 970);
}

bool App::touchTopbar(const Hit& h, int, float x, float y) {
  switch (h.id) {
    case 1001:
      browserOpen_ = !browserOpen_;
      if (browserOpen_) { FileOp ls; ls.kind = 2; fileOps_.push_back(ls); }
      return true;
    case 1002: {  // BPM: tap = tempo window, drag = scrub (0.1/px like web)
      drag_ = Drag{};
      drag_.what = kDragBpm;
      drag_.y0 = y; drag_.x0 = x; drag_.v0 = proj_.bpm;
      drag_.a = 1;  // a=1: a tap (no move) opens the tempo window on release
      return true;
    }
    case 1003: modal_ = Modal::Tempo; return true;
    case 1004: undo(); return true;
    case 1005: redo(); return true;
    case 1006: send("seek 0\n"); return true;
    case 1007: send("seek 0\n"); return true;
    case 1008: {
      wantPlay_ = !wantPlay_;
      if (wantPlay_) {
        ensureSetup();
        if (proj_.countIn > 0 && !snap_.playing)
          send("playCountIn " + fmtNum(proj_.countIn * proj_.sigNum) + "\n");
        else
          send("play\n");
        setStatusLine("PLAY");
      } else {
        send("stop\n");
        setStatusLine("STOP");
      }
      return true;
    }
    case 1009:
      recArm_ = !recArm_;
      setStatusLine(recArm_ ? "REC ARMED (KEYS/PADS -> CLIP)" : "REC OFF");
      return true;
    case 1010: {  // metronome: tap toggles; the hit also offers the menu via drag down
      proj_.metro = !proj_.metro;
      send(std::string("setMetro ") + (proj_.metro ? "1\n" : "0\n"));
      return true;
    }
    case 1011: {  // loop start: tap = set to current bar
      checkpoint();
      double bar = barTicks();
      proj_.loopStart = std::floor(snap_.posTicks / bar) * bar;
      send(buildLoop(proj_));
      return true;
    }
    case 1012:
      checkpoint();
      proj_.loopOn = !proj_.loopOn;
      send(buildLoop(proj_));
      return true;
    case 1013: {
      std::vector<MenuItem> it;
      const int bars[6] = {1, 2, 4, 8, 16, 32};
      for (int b : bars) it.push_back({"LOOP " + is(b) + " BAR", b, proj_.loopLen == b * barTicks()});
      it.push_back({"LOOP TO SELECTED CLIP", 90, false});
      it.push_back({"COUNT-IN / METRO MENU", 91, false});
      openMenu("LOOP", kMenuLoopLen, it);
      return true;
    }
    case 1014: view_ = View::Arrange; return true;
    case 1015: view_ = View::Mixer; return true;
    case 1016: {
      std::vector<MenuItem> it = {
          {"NEW PROJECT", 1}, {"SAVE (QUICK)", 2}, {"SAVE AS...", 3},
          {"OPEN PROJECT...", 4}, {"RENAME PROJECT...", 5}, {"TEMPO...", 6},
          {"SETTINGS...", 7}, {"LOG", 8}, {"HELP / LIMITS", 9},
          {"EXPORT AUDIO (NOT WIRED)", 10, false, true},
          {"IMPORT (NOT WIRED)", 11, false, true}};
      openMenu("PROJECT: " + proj_.name, kMenuProject, it);
      return true;
    }
    default: return false;
  }
  (void)x;
}

// ------------------------------------------------------------- menu modal
void App::openMenu(const std::string& title, int ctx, std::vector<MenuItem> items) {
  openMenuX(title, ctx, items, 0, 0);
}
void App::openMenuX(const std::string& title, int ctx, std::vector<MenuItem> items, int a, int b) {
  modal_ = Modal::Menu;
  menuTitle_ = title;
  menuCtx_ = ctx;
  menuA_ = a;
  menuB_ = b;
  menuItems_ = items;
  menuScroll_ = 0;
}

void App::drawModal(std::vector<DrawCmd>* o) {
  using namespace ui;
  // backdrop hit (closes) - added FIRST so later hits win
  hits_.push_back({{0, 0, kDesignW, kDesignH}, 9000, 0, 0, 0});
  rect(o, {0, 0, kDesignW, kDesignH}, 0x000000A0);
  if (modal_ == Modal::Menu) {
    float w = 560, rowH = 46;
    float hgt = std::min(560.0f, 64 + (float)menuItems_.size() * rowH);
    Rect box = {(kDesignW - w) / 2, (kDesignH - hgt) / 2, w, hgt};
    rect(o, box, PANEL);
    frame(o, box, LINE);
    text(o, box.x + 16, box.y + 14, 3, TXT, menuTitle_);
    Rect closeR = {box.x + box.w - 52, box.y + 8, 44, 36};
    buttonS(o, closeR, "X", false);
    hits_.push_back({closeR, 9001, 0, 0, 0});
    float listY = box.y + 56, listH = box.h - 64;
    // scroll drag surface + page buttons (long lists: presets etc.)
    hits_.push_back({{box.x, listY, box.w, listH}, 9005, 0, 0, 0});
    if ((float)menuItems_.size() * rowH > listH) {
      Rect up = {box.x + box.w - 52, listY + 4, 44, 44}, dn = {box.x + box.w - 52, listY + listH - 48, 44, 44};
      buttonS(o, up, "^", false);
      buttonS(o, dn, "v", false);
      hits_.push_back({up, 9006, 0, 0, 0});
      hits_.push_back({dn, 9007, 0, 0, 0});
    }
    float maxScroll = std::max(0.0f, (float)menuItems_.size() * rowH - listH);
    if (menuScroll_ > maxScroll) menuScroll_ = maxScroll;
    for (size_t i = 0; i < menuItems_.size(); i++) {
      float y = listY + i * rowH - menuScroll_;
      if (y + rowH < listY || y > listY + listH) continue;
      Rect r = {box.x + 8, y, box.w - 16, rowH - 6};
      const MenuItem& mi = menuItems_[i];
      rect(o, r, mi.disabled ? CELLOFF : PANEL2);
      if (mi.checked) rect(o, {r.x + 6, r.y + 8, 10, r.h - 16}, ACC2);
      textClip(o, r.x + 26, r.y + 10, 2.8f, mi.disabled ? FAINT : TXT, mi.label, r.w - 40);
      if (!mi.disabled) hits_.push_back({r, 9100, (int)i, mi.id, 0});
    }
  } else if (modal_ == Modal::Tempo) {
    drawTempo(o);
  } else if (modal_ == Modal::Keyboard) {
    drawKeyboard(o);
  } else if (modal_ == Modal::Settings) {
    drawSettings(o);
  } else if (modal_ == Modal::EnvEdit) {
    drawEnvEditor(o);
  } else if (modal_ == Modal::Log) {
    Rect box = {240, 90, 800, 540};
    rect(o, box, PANEL);
    frame(o, box, LINE);
    text(o, box.x + 16, box.y + 14, 3, TXT, "LOG");
    Rect closeR = {box.x + box.w - 52, box.y + 8, 44, 36};
    buttonS(o, closeR, "X", false);
    hits_.push_back({closeR, 9001, 0, 0, 0});
    float y = box.y + 60;
    int start = log_.size() > 22 ? (int)log_.size() - 22 : 0;
    for (size_t i = start; i < log_.size() && y < box.y + box.h - 24; i++, y += 22)
      textClip(o, box.x + 16, y, 2.2f, DIM, log_[i], box.w - 32);
  } else if (modal_ == Modal::Help) {
    Rect box = {190, 70, 900, 580};
    rect(o, box, PANEL);
    frame(o, box, LINE);
    text(o, box.x + 16, box.y + 14, 3, TXT, "HELP / LIMITS");
    Rect closeR = {box.x + box.w - 52, box.y + 8, 44, 36};
    buttonS(o, closeR, "X", false);
    hits_.push_back({closeR, 9001, 0, 0, 0});
    static const char* lines[] = {
        "GESTURES: DRAG = SCROLL / MOVE, EDGE OF CLIP = TRIM,",
        "LONG LIST ROWS SCROLL BY DRAGGING THE LIST.",
        "KNOBS: DRAG VERTICALLY. FADERS: RELATIVE DRAG.",
        "",
        "WIRED TO THE ENGINE (TEXT PROTOCOL): TRANSPORT, TEMPO,",
        "SIGNATURE, LOOP, METRO, TRACK NOTES/CLIPS, CC-LESS",
        "AUTOMATION LANES, INST/DEV PARAMS, MIXER, PREVIEW,",
        "BEND / MOD WHEEL, INSERT/REMOVE DEVICES.",
        "",
        "KNOWN LIMITS OF THIS BUILD:",
        "- SAMPLE AUDIO CANNOT BE UPLOADED: THE ENGINE TAKES",
        "  SAMPLE BYTES VIA A BINARY CHANNEL THE TEXT LINK",
        "  DOES NOT HAVE. AUDIO CLIPS ARE EDITABLE BUT SILENT;",
        "  SAMPLE/SOUNDFONT BROWSERS LIST REPO FILES ONLY.",
        "- MIXER METERS: ONLY THE MASTER METER IS IN THE",
        "  SNAPSHOT; STRIP METERS SHOW NO SIGNAL.",
        "- PROJECT FILES USE THE NATIVE TEXT FORMAT (MDAWN1),",
        "  NOT THE WEB .MDAW JSON; DEMOS LIST ONLY.",
        "- NO MIC RECORDING (NO INPUT STREAM IN ENGINE LINK);",
        "  REC CAPTURES KEYS/PADS MIDI INTO CLIPS.",
        "- EXPORT AUDIO / IMPORT: NOT WIRED (SEE MENU).",
    };
    float y = box.y + 58;
    for (const char* l : lines) { text(o, box.x + 16, y, 2.2f, DIM, l); y += 24; }
  }
}

bool App::touchModal(const Hit& h, int action, float x, float y) {
  if (modal_ == Modal::Settings && h.id >= 9300) return touchSettings(h, action, x, y);
  switch (h.id) {
    case 9000:  // backdrop
      if (modal_ == Modal::Menu || modal_ == Modal::Log || modal_ == Modal::Help ||
          modal_ == Modal::Tempo || modal_ == Modal::Settings || modal_ == Modal::EnvEdit)
        modal_ = Modal::None;
      return true;
    case 9001: modal_ = Modal::None; return true;
    case 9005:  // menu scroll surface
      drag_ = Drag{};
      drag_.what = kDragScrollMenu;
      drag_.y0 = y; drag_.x0 = x; drag_.v0 = menuScroll_;
      return true;
    case 9006: menuScroll_ = std::max(0.0f, menuScroll_ - 46 * 8); return true;
    case 9007: menuScroll_ += 46 * 8; return true;
    case 9100: {  // menu item (a = index)
      if (h.a < 0 || h.a >= (int)menuItems_.size()) return true;
      MenuItem mi = menuItems_[h.a];
      int ctx = menuCtx_;
      modal_ = Modal::None;
      menuAction(ctx, mi.id);
      return true;
    }
    default:
      if (modal_ == Modal::Tempo) return touchTempo(h, action, x, y);
      if (modal_ == Modal::Keyboard) return touchKeyboard(h, action, x, y);
      if (modal_ == Modal::EnvEdit) return touchEnvEditor(h, action, x, y);
      return false;
  }
}

// ------------------------------------------------------------- tempo window
void App::drawTempo(std::vector<DrawCmd>* o) {
  using namespace ui;
  Rect box = {340, 130, 600, 460};
  rect(o, box, PANEL);
  frame(o, box, LINE);
  text(o, box.x + 16, box.y + 14, 3, TXT, "TEMPO");
  Rect closeR = {box.x + box.w - 52, box.y + 8, 44, 36};
  buttonS(o, closeR, "X", false);
  hits_.push_back({closeR, 9001, 0, 0, 0});
  textC(o, {box.x + 150, box.y + 60, 300, 60}, 6, ACC, f1(proj_.bpm));
  struct B { Rect r; const char* s; int id; };
  B bs[] = {
      {{box.x + 30, box.y + 60, 90, 56}, "-1", 9401}, {{box.x + 480, box.y + 60, 90, 56}, "+1", 9402},
      {{box.x + 30, box.y + 126, 90, 44}, "-0.1", 9403}, {{box.x + 480, box.y + 126, 90, 44}, "+0.1", 9404},
      {{box.x + 150, box.y + 126, 140, 44}, "TAP", 9405},
  };
  for (auto& b : bs) { buttonS(o, b.r, b.s, false); hits_.push_back({b.r, b.id, 0, 0, 0}); }
  // signature steppers
  text(o, box.x + 30, box.y + 196, 2.5f, DIM, "SIGNATURE");
  Rect nM = {box.x + 180, box.y + 188, 52, 44}, nV = {box.x + 236, box.y + 188, 60, 44}, nP = {box.x + 300, box.y + 188, 52, 44};
  buttonS(o, nM, "-", false); rect(o, nV, PANEL2); textC(o, nV, 3, TXT, is(proj_.sigNum)); buttonS(o, nP, "+", false);
  hits_.push_back({nM, 9411, 0, 0, 0}); hits_.push_back({nP, 9412, 0, 0, 0});
  textC(o, {box.x + 356, box.y + 188, 20, 44}, 3, DIM, "/");
  Rect dM = {box.x + 380, box.y + 188, 52, 44}, dV = {box.x + 436, box.y + 188, 60, 44}, dP = {box.x + 500, box.y + 188, 52, 44};
  buttonS(o, dM, "-", false); rect(o, dV, PANEL2); textC(o, dV, 3, TXT, is(proj_.sigDen)); buttonS(o, dP, "+", false);
  hits_.push_back({dM, 9413, 0, 0, 0}); hits_.push_back({dP, 9414, 0, 0, 0});
  // count-in
  text(o, box.x + 30, box.y + 256, 2.5f, DIM, "COUNT-IN (BAR)");
  const int ci[4] = {0, 1, 2, 4};
  for (int i = 0; i < 4; i++) {
    Rect r = {box.x + 180 + i * 96.0f, box.y + 248, 88, 44};
    buttonS(o, r, is(ci[i]), proj_.countIn == ci[i]);
    hits_.push_back({r, 9420, ci[i], 0, 0});
  }
  // metronome + follow
  Rect mR = {box.x + 30, box.y + 316, 170, 48}, fR = {box.x + 216, box.y + 316, 170, 48};
  buttonS(o, mR, "METRO", proj_.metro);
  buttonS(o, fR, "FOLLOW", follow_);
  hits_.push_back({mR, 9430, 0, 0, 0});
  hits_.push_back({fR, 9431, 0, 0, 0});
  text(o, box.x + 30, box.y + 386, 2.2f, DIM, "METRO VOL");
  Rect sl = {box.x + 180, box.y + 384, 300, 22};
  hslider(o, sl, (float)proj_.metroVol);
  hits_.push_back({sl, 9440, 0, 0, 0});
  text(o, box.x + 30, box.y + 424, 2.2f, DIM, "POSITION " + is((int)(snap_.posTicks / barTicks()) + 1) + " BAR");
}

bool App::touchTempo(const Hit& h, int, float x, float y) {
  auto setBpm = [&](double d) {
    checkpoint();
    proj_.bpm = std::min(240.0, std::max(40.0, proj_.bpm + d));
    send("tempo " + fmtNum(proj_.bpm) + "\n");
  };
  switch (h.id) {
    case 9401: setBpm(-1); return true;
    case 9402: setBpm(1); return true;
    case 9403: setBpm(-0.1); return true;
    case 9404: setBpm(0.1); return true;
    case 9405: {  // tap tempo (average of last taps, kept in drag_ scratch)
      using namespace std::chrono;
      static steady_clock::time_point lastTap;
      static int taps = 0;
      static double acc = 0;
      auto now = steady_clock::now();
      if (taps > 0) {
        double iv = duration<double>(now - lastTap).count();
        if (iv > 2.0) { taps = 0; acc = 0; }
        else { acc += iv; taps++; }
      }
      lastTap = now;
      if (taps >= 2) {
        checkpoint();
        proj_.bpm = std::min(240.0, std::max(40.0, 60.0 / (acc / taps)));
        send("tempo " + fmtNum(proj_.bpm) + "\n");
      }
      return true;
    }
    case 9411: case 9412: {
      checkpoint();
      proj_.sigNum = std::min(16, std::max(1, proj_.sigNum + (h.id == 9412 ? 1 : -1)));
      send(buildSig(proj_));
      send(buildLoop(proj_));
      return true;
    }
    case 9413: case 9414: {
      checkpoint();
      const int dens[5] = {2, 4, 8, 16, 32};
      int cur = 0;
      for (int i = 0; i < 5; i++) if (dens[i] == proj_.sigDen) cur = i;
      cur = std::min(4, std::max(0, cur + (h.id == 9414 ? 1 : -1)));
      proj_.sigDen = dens[cur];
      send(buildSig(proj_));
      send(buildLoop(proj_));
      return true;
    }
    case 9420: proj_.countIn = h.a; dirty_ = true; return true;
    case 9430:
      proj_.metro = !proj_.metro;
      send(std::string("setMetro ") + (proj_.metro ? "1\n" : "0\n"));
      return true;
    case 9431: follow_ = !follow_; return true;
    case 9440: {
      drag_ = Drag{};
      drag_.what = kDragSlider; drag_.a = 1;
      drag_.v0 = h.r.x; drag_.w0 = h.r.w; drag_.x0 = x; drag_.y0 = y;
      applySlider(1, (x - h.r.x) / h.r.w, false);
      return true;
    }
    default: return false;
  }
}

// ------------------------------------------------------------- keyboard
void App::openKeyboard(int target, const std::string& initial, const std::string& title) {
  modal_ = Modal::Keyboard;
  kbTarget_ = target;
  kbText_ = initial;
  kbTitle_ = title;
}
void App::drawKeyboard(std::vector<DrawCmd>* o) {
  using namespace ui;
  Rect box = {240, 120, 800, 470};
  rect(o, box, PANEL);
  frame(o, box, LINE);
  text(o, box.x + 16, box.y + 14, 3, TXT, kbTitle_);
  Rect closeR = {box.x + box.w - 52, box.y + 8, 44, 36};
  buttonS(o, closeR, "X", false);
  hits_.push_back({closeR, 9001, 0, 0, 0});
  Rect field = {box.x + 16, box.y + 54, box.w - 32, 52};
  rect(o, field, CELLOFF);
  textClip(o, field.x + 10, field.y + 14, 3.5f, TXT, kbText_ + "_", field.w - 20);
  static const char* rows[4] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM-_"};
  float kw = 68, kh = 62, gap = 8;
  for (int r = 0; r < 4; r++) {
    std::string row = rows[r];
    float total = row.size() * (kw + gap) - gap;
    float x0 = box.x + (box.w - total) / 2;
    float y = box.y + 124 + r * (kh + gap);
    for (size_t i = 0; i < row.size(); i++) {
      Rect kr = {x0 + i * (kw + gap), y, kw, kh};
      char ch[2] = {row[i], 0};
      buttonS(o, kr, ch, false);
      hits_.push_back({kr, 9200, (int)row[i], 0, 0});
    }
  }
  float y = box.y + 124 + 4 * (kh + gap);
  Rect sp = {box.x + 90, y, 330, kh}, del = {box.x + 432, y, 120, kh}, ok = {box.x + 564, y, 120, kh};
  buttonS(o, sp, "SPACE", false);
  buttonS(o, del, "DEL", false);
  buttonS(o, ok, "OK", true);
  hits_.push_back({sp, 9201, 0, 0, 0});
  hits_.push_back({del, 9202, 0, 0, 0});
  hits_.push_back({ok, 9203, 0, 0, 0});
}
bool App::touchKeyboard(const Hit& h, int, float, float) {
  switch (h.id) {
    case 9200:
      if (kbText_.size() < 40) kbText_ += (char)h.a;
      return true;
    case 9201: kbText_ += ' '; return true;
    case 9202: if (!kbText_.empty()) kbText_.pop_back(); return true;
    case 9203: {
      int t = kbTarget_;
      std::string v = kbText_;
      modal_ = Modal::None;
      keyboardDone(t, v);
      return true;
    }
    default: return false;
  }
}
// ------------------------------------------------------------- menu actions
void App::menuAction(int ctx, int id) {
  static const uint32_t kCols[8] = {0xFF8C2EFF, 0x3ECF6EFF, 0xFFD94DFF, 0xFF6E9EFF,
                                    0xFFFF6EFF, 0x6EFFD9FF, 0xFFCF6EFF, 0x9E6EFFFF};
  switch (ctx) {
    case kMenuProject:
      switch (id) {
        case 1:  // New
          undo_.push_back(serializeProject(proj_));
          applyLoaded(makeDefaultProject(), "NEW PROJECT");
          break;
        case 2: saveToSlot(proj_.name, false); break;
        case 3: openKeyboard(kKbSaveAs, proj_.name, "SAVE AS"); break;
        case 4: {  // Open: slots then files
          std::vector<MenuItem> it;
          for (size_t i = 0; i < memSlots_.size(); i++) it.push_back({memSlots_[i].first, (int)i, false});
          for (size_t i = 0; i < fileList_.size(); i++) {
            bool dup = false;
            for (auto& s : memSlots_) if (s.first == fileList_[i]) dup = true;
            if (!dup) it.push_back({fileList_[i], 1000 + (int)i, false});
          }
          if (it.empty()) it.push_back({"(NOTHING SAVED YET)", 0, false, true});
          openMenu("OPEN PROJECT", kMenuBrowserProj, it);
          break;
        }
        case 5: openKeyboard(kKbProjName, proj_.name, "PROJECT NAME"); break;
        case 6: modal_ = Modal::Tempo; break;
        case 7: modal_ = Modal::Settings; break;
        case 8: modal_ = Modal::Log; break;
        case 9: modal_ = Modal::Help; break;
        default: break;
      }
      break;
    case kMenuBrowserProj:
      if (id >= 1000) { if (id - 1000 < (int)fileList_.size()) loadByName(fileList_[id - 1000], false); }
      else if (id >= 0 && id < (int)memSlots_.size()) loadByName(memSlots_[id].first, false);
      break;
    case kMenuTrack: {
      bool isRet = menuA_ >= 1000;
      int idx = isRet ? menuA_ - 1000 : menuA_;
      std::vector<Track>& vec = isRet ? proj_.returns : proj_.tracks;
      if (idx < 0 || idx >= (int)vec.size()) break;
      Track& t = vec[idx];
      int strip = isRet ? stripOfReturn(idx) : idx;
      switch (id) {
        case 1: openKeyboard(kKbTrackName, t.name, "TRACK NAME"); break;
        case 2: {
          checkpoint();
          Track copy = t;
          copy.name += " 2";
          vec.insert(vec.begin() + idx + 1, copy);
          if (isRet) syncReturn(idx + 1); else syncTrack(idx + 1);
          break;
        }
        case 3: {
          checkpoint();
          sendRaw("resetTrack " + fmtNum(strip) + "\n");
          vec.erase(vec.begin() + idx);
          // strips above the removed one shift down: full resync
          for (size_t i = idx; i < vec.size(); i++) isRet ? syncReturn((int)i) : syncTrack((int)i);
          if (selTrack_ >= (int)vec.size()) selTrack_ = std::max(0, (int)vec.size() - 1);
          selClip_ = -1;
          break;
        }
        case 4: t.folded = !t.folded; dirty_ = true; break;
        case 5: {  // add clip
          if (isRet || t.kind == kAuto) break;
          checkpoint();
          double at = 0;
          for (const Clip& c : t.clips) at = std::max(at, c.start + c.len);
          Clip c;
          c.start = at; c.len = barTicks(); c.patternLen = barTicks();
          c.name = t.kind == kAudio ? "Audio" : "Pattern";
          if (t.kind == kAudio && defs::kSampleFileCount > 0) { c.sampleId = 0; c.sampleName = defs::kSampleFiles[0]; }
          t.clips.push_back(c);
          selClip_ = (int)t.clips.size() - 1;
          syncNotes(idx);
          tab_ = DetailTab::Clip;
          detailOpen_ = true;
          break;
        }
        case 6: {  // change instrument
          std::vector<MenuItem> it;
          for (const defs::UnitDef& d : defs::kInstruments)
            it.push_back({d.name, d.type, t.instType == d.type});
          openMenu("INSTRUMENT", kMenuInstPick, it);
          break;
        }
        case 7: checkpoint(); t.clips.clear(); selClip_ = -1; syncNotes(idx); break;
        case 8: {
          checkpoint();
          int ci = 0;
          for (int i = 0; i < 8; i++) if (kCols[i] == t.color) ci = i;
          t.color = kCols[(ci + 1) % 8];
          dirty_ = true;
          break;
        }
        case 9: tab_ = DetailTab::Device; detailOpen_ = true; break;
        default: break;
      }
      break;
    }
    case kMenuClip: {
      Track* t = selTrackPtr();
      Clip* c = selClipPtr();
      if (!t || !c || selTarget_ != 0) break;
      switch (id) {
        case 1: {
          checkpoint();
          Clip copy = *c;
          copy.start = c->start + c->len;
          copy.name += " 2";
          t->clips.insert(t->clips.begin() + selClip_ + 1, copy);
          selClip_++;
          syncNotes(selTrack_);
          break;
        }
        case 2: checkpoint(); c->muted = !c->muted; syncNotes(selTrack_); break;
        case 3:
          checkpoint();
          t->clips.erase(t->clips.begin() + selClip_);
          selClip_ = -1;
          syncNotes(selTrack_);
          break;
        case 4: openKeyboard(kKbClipName, c->name, "CLIP NAME"); break;
        case 5: {
          std::vector<MenuItem> it;
          const int bars[6] = {1, 2, 4, 8, 16, 32};
          for (int b : bars) it.push_back({is(b) + " BAR", b, c->patternLen == b * barTicks()});
          openMenu("PATTERN LENGTH", kMenuPatLen, it);
          break;
        }
        case 6: {  // split at playhead (pattern clips: split the pattern too)
          double pos = snap_.posTicks;
          if (pos <= c->start + 30 || pos >= c->start + c->len - 30) { setStatusLine("SPLIT: PLAYHEAD OUTSIDE CLIP"); break; }
          checkpoint();
          Clip nc = *c;
          nc.start = pos;
          nc.len = c->start + c->len - pos;
          if (t->kind != kAudio) {
            double local = std::fmod(pos - c->start, c->patternLen);
            nc.offset = local;
            nc.name += " B";
          }
          c->len = pos - c->start;
          t->clips.insert(t->clips.begin() + selClip_ + 1, nc);
          syncNotes(selTrack_);
          break;
        }
        case 7: checkpoint(); c->notes.clear(); selNote_ = -1; syncNotes(selTrack_); break;
        default: break;
      }
      break;
    }
    case kMenuAddTrack: {
      checkpoint();
      if (id == 10) {
        Track r;
        r.kind = kReturn;
        r.name = "FX " + std::string(1, (char)('A' + (int)proj_.returns.size()));
        r.color = kCols[proj_.returns.size() % 8];
        proj_.returns.push_back(r);
        syncReturn((int)proj_.returns.size() - 1);
        selTarget_ = 1;
        selTrack_ = (int)proj_.returns.size() - 1;
        break;
      }
      Track t;
      t.kind = id == 1 ? kMidi : id == 2 ? kDrum : id == 3 ? kAudio : kAuto;
      t.instType = t.kind == kDrum ? 3 : 1;
      t.name = t.kind == kDrum ? "Drums" : t.kind == kAudio ? "Audio" : t.kind == kAuto ? "Auto" : "Keys";
      t.color = kCols[proj_.tracks.size() % 8];
      ensureParams(&t);
      if (t.kind != kAuto) {
        Clip c;
        c.len = barTicks();
        c.patternLen = barTicks();
        c.name = "Pattern 1";
        if (t.kind == kAudio && defs::kSampleFileCount > 0) { c.sampleId = 0; c.sampleName = defs::kSampleFiles[0]; c.name = "Audio"; }
        t.clips.push_back(c);
      }
      proj_.tracks.push_back(t);
      selTarget_ = 0;
      selTrack_ = (int)proj_.tracks.size() - 1;
      selClip_ = t.clips.empty() ? -1 : 0;
      syncTrack(selTrack_);
      break;
    }
    case kMenuStrip: {  // automation lane for strip menuA_
      static const int kKindOf[5] = {0, 1, 2, 3, 7};
      if (id < 1 || id > 5) break;
      int kind = kKindOf[id - 1];
      int strip = menuA_;
      int found = -1;
      for (size_t i = 0; i < proj_.autoLanes.size(); i++)
        if (proj_.autoLanes[i].track == strip && proj_.autoLanes[i].kind == kind) found = (int)i;
      if (found < 0) {
        checkpoint();
        AutoLane l;
        l.kind = kind;
        l.track = strip;
        double cur = 0.85;
        const Track* t = nullptr;
        if (strip == kMasterStrip) cur = proj_.masterVol;
        else if (strip >= 65 && strip - 65 < (int)proj_.returns.size()) t = &proj_.returns[strip - 65];
        else if (strip >= 0 && strip < (int)proj_.tracks.size()) t = &proj_.tracks[strip];
        if (t) cur = kind == 0 ? t->vol : kind == 1 ? t->pan : kind == 2 ? t->send[0] : kind == 3 ? t->send[1] : t->width;
        l.pts = {{0, cur}, {16 * barTicks(), cur}};
        proj_.autoLanes.push_back(l);
        found = (int)proj_.autoLanes.size() - 1;
        send(buildAutomation(proj_));
      }
      envLane_ = found;
      envSelPt_ = -1;
      modal_ = Modal::EnvEdit;
      break;
    }
    case kMenuDevCard: {
      Track* t = selTrackPtr();
      if (!t || menuA_ < 0 || menuA_ >= (int)t->devices.size()) break;
      int strip = selStrip();
      int old = (int)t->devices.size();
      switch (id) {
        case 1: checkpoint(); t->devices.erase(t->devices.begin() + menuA_); send(buildTrackDevices(strip, *t, old)); break;
        case 2: devClip_ = t->devices[menuA_]; devClipOk_ = true; setStatusLine("DEVICE COPIED"); break;
        case 3:
          if (devClipOk_) { checkpoint(); t->devices[menuA_] = devClip_; send(buildTrackDevices(strip, *t, old)); }
          break;
        case 5:
          if (menuA_ > 0) { checkpoint(); std::swap(t->devices[menuA_], t->devices[menuA_ - 1]); send(buildTrackDevices(strip, *t, old)); }
          break;
        case 6:
          if (menuA_ + 1 < (int)t->devices.size()) { checkpoint(); std::swap(t->devices[menuA_], t->devices[menuA_ + 1]); send(buildTrackDevices(strip, *t, old)); }
          break;
        default: break;
      }
      break;
    }
    case kMenuFxPick: {
      if (menuA_ == -1) {  // group stage -> type stage
        if (id < 0 || id >= (int)(sizeof(defs::kFxGroups) / sizeof(defs::FxGroup))) break;
        std::vector<MenuItem> it;
        for (int ty : defs::kFxGroups[id].types) {
          const defs::UnitDef* d = devDef(ty);
          if (d) it.push_back({d->name, ty, false});
        }
        openMenuX("ADD FX", kMenuFxPick, it, id, 0);
        break;
      }
      Track* t = selTrackPtr();
      if (!t) break;
      checkpoint();
      FxDevice dv;
      dv.type = id;
      ensureParams(&dv);
      int pos = std::min(devPickFor_, (int)t->devices.size());
      int old = (int)t->devices.size();
      t->devices.insert(t->devices.begin() + pos, dv);
      send(buildTrackDevices(selStrip(), *t, old));
      setStatusLine("FX ADDED: " + std::string(devDef(id) ? devDef(id)->name : "?"));
      break;
    }
    case kMenuInstPick: {
      Track* t = selTrackPtr();
      if (!t || selTarget_ != 0) break;
      checkpoint();
      t->instType = id;
      t->instParams = defaultParams(instDef(id));
      send(buildTrackInst(selTrack_, *t));
      setStatusLine("INSTRUMENT: " + std::string(instDef(id) ? instDef(id)->name : "?"));
      break;
    }
    case kMenuLoopLen:
      checkpoint();
      if (id == 90) {
        Clip* c = selClipPtr();
        if (c) { proj_.loopStart = c->start; proj_.loopLen = c->len; proj_.loopOn = true; }
      } else if (id == 91) {
        std::vector<MenuItem> it = {{"METRO ON/OFF", 1, proj_.metro}, {"COUNT-IN 0 BAR", 2, proj_.countIn == 0},
                                    {"COUNT-IN 1 BAR", 3, proj_.countIn == 1}, {"COUNT-IN 2 BAR", 4, proj_.countIn == 2},
                                    {"COUNT-IN 4 BAR", 5, proj_.countIn == 4}};
        openMenu("METRO / COUNT-IN", kMenuMetro, it);
        return;
      } else {
        proj_.loopLen = id * barTicks();
        proj_.loopOn = true;
      }
      send(buildLoop(proj_));
      break;
    case kMenuMetro:
      if (id == 1) {
        proj_.metro = !proj_.metro;
        send(std::string("setMetro ") + (proj_.metro ? "1\n" : "0\n"));
      } else if (id >= 2 && id <= 5) {
        proj_.countIn = id == 2 ? 0 : id == 3 ? 1 : id == 4 ? 2 : 4;
        dirty_ = true;
      }
      break;
    case kMenuSnap: snapMode_ = id; break;
    case kMenuPatLen: {
      Clip* c = selClipPtr();
      if (c) {
        checkpoint();
        c->patternLen = id * barTicks();
        syncNotes(selTrack_);
      }
      break;
    }
    case kMenuAudioTo: {
      int strip = menuA_;
      if (strip >= 0 && strip < (int)proj_.tracks.size()) {
        checkpoint();
        proj_.tracks[strip].out = id == 100 ? -1 : id;
        send("trackOut " + fmtNum(strip) + " " + fmtNum(proj_.tracks[strip].out) + "\n");
      }
      break;
    }
    case kMenuPresetPick: {
      Track* t = selTrackPtr();
      if (t && id >= 0 && id < (int)menuPresets_.size()) {
        checkpoint();
        applyPresetToTrack(t, defs::kPresets[menuPresets_[id]]);
        send(buildTrackInst(selTrack_, *t));
        setStatusLine("PRESET: " + std::string(defs::kPresets[menuPresets_[id]].name));
      }
      break;
    }
    case kMenuSamplePick: {
      Clip* c = selClipPtr();
      if (c && id >= 0 && id < defs::kSampleFileCount) {
        checkpoint();
        c->sampleId = id;
        c->sampleName = defs::kSampleFiles[id];
        syncNotes(selTrack_);
      }
      break;
    }
    default: break;
  }
}

void App::keyboardDone(int target, const std::string& text) {
  std::string v = text;
  while (!v.empty() && v.back() == ' ') v.pop_back();
  switch (target) {
    case kKbProjName:
      checkpoint();
      proj_.name = v.empty() ? "Untitled" : v;
      dirty_ = true;
      setStatusLine("PROJECT NAME: " + proj_.name);
      break;
    case kKbSaveAs:
      if (!v.empty()) saveToSlot(v, false);
      break;
    case kKbSearch: broSearch_ = v; broScroll_[browserTab_] = 0; break;
    case kKbTrackName: {
      Track* t = selTrackPtr();
      if (t && !v.empty()) { checkpoint(); t->name = v; dirty_ = true; }
      break;
    }
    case kKbClipName: {
      Clip* c = selClipPtr();
      if (c && !v.empty()) { checkpoint(); c->name = v; dirty_ = true; }
      break;
    }
    default: break;
  }
}

}  // namespace mdnative
