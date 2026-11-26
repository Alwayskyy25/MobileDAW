// ui_keys.cpp - Keys tab of the detail panel (web map section 2b pane
// "keys"): multi-touch piano keyboard with pitch-bend / mod wheels for
// melodic tracks, drum pads for drum tracks. Notes go to the engine with
// the `preview` command; with REC armed + transport playing, played notes
// are also captured into the selected track's clip at the playhead.
#include "ui_core.h"
#include <algorithm>
#include <cmath>
#include "ui_widgets.h"

namespace mdnative {
namespace {
std::string is(int v) { return std::to_string(v); }
// White-key layout: 14 white keys starting at C of keysOctave_.
// Returns pitch for a white-key index.
int whitePitch(int base, int wi) {
  static const int kSemi[7] = {0, 2, 4, 5, 7, 9, 11};
  return base + (wi / 7) * 12 + kSemi[wi % 7];
}
}  // namespace

// Geometry shared by draw + pointer handling.
void App::keysGeom(Rect* area, int* whites) const {
  *area = {detailContentR_.x + 120, detailContentR_.y + 8, detailContentR_.w - 128, detailContentR_.h - 16};
  *whites = 14;
}
int App::keysPitchAt(float x, float y) const {
  Rect area;
  int whites;
  keysGeom(&area, &whites);
  if (!area.contains(x, y)) return -1;
  const Track* t = (selTarget_ == 0 && selTrack_ >= 0 && selTrack_ < (int)proj_.tracks.size())
                       ? &proj_.tracks[selTrack_] : nullptr;
  if (t && t->kind == kDrum) return -1;
  int base = keysOctave_ * 12 + 12;  // C of the base octave (C4 = 60)
  float ww = area.w / whites;
  // black keys first (upper 55% of the area)
  if (y < area.y + area.h * 0.55f) {
    for (int wi = 0; wi < whites; wi++) {
      int wp = whitePitch(base, wi);
      int pc = wp % 12;
      bool hasBlack = pc == 0 || pc == 2 || pc == 5 || pc == 7 || pc == 9;  // C D F G A
      if (!hasBlack) continue;
      float bx = area.x + (wi + 1) * ww - ww * 0.3f;
      if (x >= bx && x < bx + ww * 0.6f) return wp + 1;
    }
  }
  int wi = (int)((x - area.x) / ww);
  if (wi < 0 || wi >= whites) return -1;
  return whitePitch(base, wi);
}
int App::padAt(float x, float y) const {
  Rect area;
  int whites;
  keysGeom(&area, &whites);
  if (!area.contains(x, y)) return -1;
  int col = (int)((x - area.x) / (area.w / 3));
  int row = (int)((y - area.y) / (area.h / 2));
  if (col < 0 || col > 2 || row < 0 || row > 1) return -1;
  return row * 3 + col;
}

void App::drawKeys(std::vector<DrawCmd>* o) {
  using namespace ui;
  Rect C = detailContentR_;
  auto add = [&](Rect r, int id, int a = 0, int b = 0, double v = 0) { hits_.push_back({r, id, a, b, v}); };
  Track* t = selTrackPtr();
  if (selTarget_ != 0 || !t) {
    text(o, C.x + 16, C.y + 16, 2.4f, DIM, "KEYS: SELECT A TRACK");
    keysArea_ = {0, 0, 0, 0};
    return;
  }
  // wheels column (bend / mod) + octave buttons under it
  Rect bw = {C.x + 8, C.y + 8, 48, C.h - 108}, mw = {C.x + 62, C.y + 8, 48, C.h - 108};
  rect(o, bw, PANEL2);
  rect(o, mw, PANEL2);
  float by = bw.y + bw.h * (1 - (bendV_ + 1) / 2);
  rect(o, {bw.x + 4, by - 4, bw.w - 8, 8}, ACC);
  float my = mw.y + mw.h * (1 - modV_);
  rect(o, {mw.x + 4, my - 4, mw.w - 8, 8}, ACC2);
  text(o, bw.x + 6, bw.y + bw.h + 4, 1.8f, DIM, "BEND");
  text(o, mw.x + 8, mw.y + mw.h + 4, 1.8f, DIM, "MOD");
  add(bw, 6001);
  add(mw, 6002);
  Rect omB = {C.x + 8, C.y + C.h - 40, 48, 32}, opB = {C.x + 62, C.y + C.h - 40, 48, 32};
  buttonS(o, omB, "OCT-", false);
  buttonS(o, opB, "OCT+", false);
  add(omB, 6003);
  add(opB, 6004);
  Rect area;
  int whites;
  keysGeom(&area, &whites);
  keysArea_ = area;
  if (t->kind == kDrum) {
    // drum pads: 3 cols x 2 rows
    for (int i = 0; i < 6; i++) {
      float pw = area.w / 3, ph = area.h / 2;
      Rect pr = {area.x + (i % 3) * pw + 3, area.y + (i / 3) * ph + 3, pw - 6, ph - 6};
      bool heldNow = false;
      for (const HeldKey& hk : held_) if (hk.pitch == defs::kDrumPads[i].pitch) heldNow = true;
      rect(o, pr, heldNow ? ACC : PANEL3);
      textC(o, pr, 3, heldNow ? DARK : TXT, defs::kDrumPads[i].name);
    }
    text(o, area.x + 4, area.y - 2, 2, DIM, recArm_ ? "PADS - REC ARMED" : "PADS");
    return;
  }
  // piano keyboard
  int base = keysOctave_ * 12 + 12;
  float ww = area.w / whites;
  for (int wi = 0; wi < whites; wi++) {
    int p = whitePitch(base, wi);
    Rect kr = {area.x + wi * ww + 1, area.y, ww - 2, area.h};
    bool heldNow = false;
    for (const HeldKey& hk : held_) if (hk.pitch == p) heldNow = true;
    rect(o, kr, heldNow ? ACC : WHITEKEY);
    if (p % 12 == 0) {
      static const char* kN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
      text(o, kr.x + 4, kr.y + kr.h - 18, 2, DARK, std::string(kN[p % 12]) + is(p / 12 - 1));
    }
  }
  for (int wi = 0; wi < whites; wi++) {
    int wp = whitePitch(base, wi);
    int pc = wp % 12;
    bool hasBlack = pc == 0 || pc == 2 || pc == 5 || pc == 7 || pc == 9;
    if (!hasBlack) continue;
    float bx = area.x + (wi + 1) * ww - ww * 0.3f;
    Rect kr = {bx, area.y, ww * 0.6f, area.h * 0.55f};
    bool heldNow = false;
    for (const HeldKey& hk : held_) if (hk.pitch == wp + 1) heldNow = true;
    rect(o, kr, heldNow ? ACC : BLACKKEY);
  }
  text(o, area.x + 4, area.y + area.h - 20, 2, DARK, "OCT " + is(keysOctave_) + (recArm_ ? "  -  REC ARMED" : ""));
}

bool App::touchKeys(const Hit& h, int, float x, float y) {
  switch (h.id) {
    case 6001: case 6002: {
      drag_ = Drag{};
      drag_.what = kDragWheel;
      drag_.a = h.id == 6001 ? 0 : 1;
      drag_.x0 = x; drag_.y0 = y;
      drag_.v0 = h.id == 6001 ? bendV_ : modV_ * 2 - 1;
      return true;
    }
    case 6003: keysOctave_ = std::max(1, keysOctave_ - 1); return true;
    case 6004: keysOctave_ = std::min(7, keysOctave_ + 1); return true;
    default: return false;
  }
}

void App::keysPointer(int pid, float x, float y, int action) {
  Track* t = selTrackPtr();
  if (!t || selTarget_ != 0) return;
  if (action == 0) {
    int pitch = -1;
    if (t->kind == kDrum) {
      int pad = padAt(x, y);
      if (pad >= 0) pitch = defs::kDrumPads[pad].pitch;
    } else {
      pitch = keysPitchAt(x, y);
    }
    if (pitch < 0) return;
    held_.push_back({pid, pitch, snap_.posTicks});
    send("preview " + fmtNum(selTrack_) + " " + fmtNum(pitch) + " 0.95 1\n");
  } else if (action == 1) {
    for (size_t i = 0; i < held_.size(); i++) {
      if (held_[i].pid != pid) continue;
      HeldKey hk = held_[i];
      held_.erase(held_.begin() + i);
      send("preview " + fmtNum(selTrack_) + " " + fmtNum(hk.pitch) + " 0 0\n");
      // REC capture: write the played note into the clip at the playhead.
      if (recArm_ && snap_.playing && (t->kind == kMidi || t->kind == kDrum)) {
        double t0 = hk.startTick, t1 = snap_.posTicks;
        if (t1 <= t0) t1 = t0 + 120;
        Clip* target = nullptr;
        int ci = -1;
        for (size_t k = 0; k < t->clips.size(); k++)
          if (t0 >= t->clips[k].start && t0 < t->clips[k].start + t->clips[k].len) { target = &t->clips[k]; ci = (int)k; break; }
        if (!target) {
          checkpoint();
          Clip nc;
          double bar = barTicks();
          nc.start = std::floor(t0 / bar) * bar;
          nc.len = bar;
          nc.patternLen = bar;
          nc.name = "Rec";
          t->clips.push_back(nc);
          ci = (int)t->clips.size() - 1;
          target = &t->clips[ci];
        } else {
          checkpoint();
        }
        double local = std::fmod(t0 - target->start, target->patternLen);
        Note n;
        n.start = local;
        n.len = std::min(t1 - t0, target->patternLen - local);
        if (n.len < 30) n.len = 30;
        n.pitch = hk.pitch;
        n.vel = 0.95f;
        target->notes.push_back(n);
        selClip_ = ci;
        syncNotes(selTrack_);
        setStatusLine("REC: NOTE WRITTEN TO CLIP");
      }
      return;
    }
  }
}

}  // namespace mdnative
