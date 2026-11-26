// ui_mixer.cpp - Mixer view (web map section 3), Ableton-style strips for
// every track + returns + Master: title, Audio To, sends, pan/width knobs,
// dB field, meter + relative fader, Activator/Solo/Arm, AUTO (automation).
// NOTE: only the master meter exists in the engine snapshot - other strips
// draw an empty meter frame (documented limit).
#include "ui_core.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "ui_widgets.h"

namespace mdnative {
namespace {
std::string f1(double v) { char b[32]; std::snprintf(b, sizeof b, "%.1f", v); return b; }
std::string is(int v) { return std::to_string(v); }
}  // namespace

void App::drawMixer(std::vector<DrawCmd>* o) {
  using namespace ui;
  Rect B = arrR_;  // full body in mixer view
  rect(o, B, BG);
  auto add = [&](Rect r, int id, int a = 0, int b = 0, double v = 0) { hits_.push_back({r, id, a, b, v}); };
  add(B, 4000);
  struct Strip {
    int strip; const Track* t; bool master; int target; int idx;
  };
  std::vector<Strip> strips;
  for (size_t i = 0; i < proj_.tracks.size(); i++) strips.push_back({(int)i, &proj_.tracks[i], false, 0, (int)i});
  for (size_t i = 0; i < proj_.returns.size(); i++) strips.push_back({stripOfReturn((int)i), &proj_.returns[i], false, 1, (int)i});
  strips.push_back({kMasterStrip, nullptr, true, 2, 0});
  const float SW = 150, GAP = 8;
  float totalW = strips.size() * (SW + GAP);
  if (mixX0_ > std::max(0.0f, totalW - B.w)) mixX0_ = std::max(0.0f, totalW - B.w);
  text(o, B.x + 10, B.y + 6, 2.2f, DIM, "MIXER - DRAG BACKGROUND TO SCROLL - AUTO OPENS AUTOMATION MENU");
  for (size_t si = 0; si < strips.size(); si++) {
    const Strip& s = strips[si];
    float x = B.x + 6 + si * (SW + GAP) - mixX0_;
    if (x + SW < B.x || x > B.x + B.w) continue;
    float y = B.y + 30;
    float H = B.h - 38;
    bool sel = selStrip() == s.strip;
    Rect sr = {x, y, SW, H};
    rect(o, sr, sel ? PANEL3 : PANEL);
    // title bar
    uint32_t col = s.master ? 0xFFD94DFF : s.t->color;
    rect(o, {x, y, SW, 30}, col);
    textClip(o, x + 6, y + 7, 2.2f, DARK, s.master ? "MASTER" : s.t->name, SW - 12);
    add({x, y, SW, 30}, 4001, s.target, s.idx, 0);
    float cy = y + 34;
    if (!s.master) {
      // Audio To
      Rect at = {x + 6, cy, SW - 12, 26};
      buttonS(o, at, s.t->out < 0 ? "TO: MASTER" : "TO: FX " + is(s.t->out + 1), false);
      add(at, 4012, s.strip, 0, 0);
      cy += 32;
      // sends A/B
      text(o, x + 8, cy + 2, 1.8f, DIM, "SEND A");
      text(o, x + 80, cy + 2, 1.8f, DIM, "SEND B");
      Rect ka = {x + 8, cy + 16, 56, 44}, kb = {x + 80, cy + 16, 56, 44};
      knob(o, ka, "", (float)(s.t->send[0] / 1.5));
      knob(o, kb, "", (float)(s.t->send[1] / 1.5));
      add(ka, 4010, 6, s.strip, 0);
      add(kb, 4010, 6, s.strip, 1);
      cy += 66;
      // pan / width
      text(o, x + 8, cy + 2, 1.8f, DIM, "PAN");
      text(o, x + 80, cy + 2, 1.8f, DIM, "WIDTH");
      Rect kp = {x + 8, cy + 16, 56, 44}, kw = {x + 80, cy + 16, 56, 44};
      knob(o, kp, "", (float)((s.t->pan + 1) / 2), true);
      knob(o, kw, "", (float)(s.t->width / 2));
      add(kp, 4010, 4, s.strip, 0);
      add(kw, 4010, 5, s.strip, 0);
      cy += 66;
    } else {
      text(o, x + 8, cy + 4, 1.8f, DIM, "LIMITER");
      text(o, x + 8, cy + 22, 2.2f, TXT, proj_.masterLimit ? "ON" : "OFF");
      cy += 32 + 66 + 66;
    }
    // dB field
    double vol = s.master ? proj_.masterVol : s.t->vol;
    Rect db = {x + 6, cy, SW - 12, 26};
    rect(o, db, CELLOFF);
    textC(o, db, 2.4f, TXT, f1(ui::linToDb(vol)) + " DB");
    cy += 32;
    // meter + fader
    float fH = y + H - 44 - cy;
    Rect fr = {x + 22, cy, 52, fH};
    fader(o, fr, (float)(vol / 1.25), s.master ? ACC : ACC2);
    add(fr, 4020, s.strip, 0, 0);
    Rect mr = {x + 96, cy, 16, fH};
    rect(o, mr, CELLOFF);
    if (s.master) {
      float mh = fH * std::min(1.0f, snap_.meterMasterL);
      rect(o, {mr.x, cy + fH - mh, 7, mh}, ACC2);
      float mh2 = fH * std::min(1.0f, snap_.meterMasterR);
      rect(o, {mr.x + 9, cy + fH - mh2, 7, mh2}, ACC2);
    }
    // bottom buttons: ACT / SOLO / ARM / AUTO
    float by = y + H - 38;
    if (!s.master) {
      Rect a1 = {x + 4, by, 34, 32}, a2 = {x + 41, by, 34, 32}, a3 = {x + 78, by, 34, 32}, a4 = {x + 115, by, 32, 32};
      buttonS(o, a1, "ACT", !s.t->mute);
      buttonS(o, a2, "SOL", s.t->solo);
      buttonS(o, a3, "ARM", s.t->arm);
      buttonS(o, a4, "AUT", false);
      add(a1, 4030, s.strip, 0, 0);
      add(a2, 4031, s.strip, 0, 0);
      add(a3, 4032, s.strip, 0, 0);
      add(a4, 4033, s.strip, 0, 0);
    } else {
      Rect a4 = {x + 78, by, 69, 32};
      buttonS(o, a4, "AUTO", false);
      add(a4, 4033, s.strip, 0, 0);
    }
  }
}

bool App::touchMixer(const Hit& h, int, float x, float y) {
  switch (h.id) {
    case 4000:
      drag_ = Drag{};
      drag_.what = kDragScrollMix;
      drag_.x0 = x; drag_.y0 = y; drag_.v0 = mixX0_;
      return true;
    case 4001:  // title: select (a=target, b=idx)
      selTarget_ = h.a;
      selTrack_ = h.b;
      selClip_ = -1;
      return true;
    case 4010: {  // knob: a=kind (4 pan,5 width,6 send), b=strip, v=send idx
      drag_ = Drag{};
      drag_.what = kDragKnob;
      drag_.a = h.a; drag_.b = h.b; drag_.c = (int)h.v;
      drag_.x0 = x; drag_.y0 = y;
      Track* t = nullptr;
      if (h.b >= 65) { int i = h.b - 65; if (i < (int)proj_.returns.size()) t = &proj_.returns[i]; }
      else if (h.b >= 0 && h.b < (int)proj_.tracks.size()) t = &proj_.tracks[h.b];
      if (!t) return true;
      checkpoint();
      if (h.a == 4) drag_.v0 = (t->pan + 1) / 2;
      else if (h.a == 5) drag_.v0 = t->width / 2;
      else drag_.v0 = t->send[(int)h.v] / 1.5;
      return true;
    }
    case 4012: {  // Audio To menu (h.a = strip, tracks only)
      std::vector<MenuItem> it;
      it.push_back({"MASTER", 100, false});
      for (size_t i = 0; i < proj_.returns.size(); i++)
        it.push_back({"FX RETURN " + std::to_string(i + 1), (int)i, false});
      openMenuX("AUDIO TO", kMenuAudioTo, it, h.a, 0);
      return true;
    }
    case 4020: {  // fader (relative drag)
      drag_ = Drag{};
      drag_.what = kDragFader;
      drag_.a = h.a;
      drag_.x0 = x; drag_.y0 = y;
      drag_.w0 = h.r.h;
      double v = 0;
      if (h.a == kMasterStrip) v = proj_.masterVol;
      else if (h.a >= 65) { int i = h.a - 65; if (i < (int)proj_.returns.size()) v = proj_.returns[i].vol; }
      else if (h.a >= 0 && h.a < (int)proj_.tracks.size()) v = proj_.tracks[h.a].vol;
      drag_.t0 = v;
      checkpoint();
      return true;
    }
    case 4030: case 4031: case 4032: {
      Track* t = nullptr;
      if (h.a >= 65) { int i = h.a - 65; if (i < (int)proj_.returns.size()) t = &proj_.returns[i]; }
      else if (h.a >= 0 && h.a < (int)proj_.tracks.size()) t = &proj_.tracks[h.a];
      else if (h.a == kMasterStrip) return true;
      if (!t) return true;
      checkpoint();
      if (h.id == 4030) { t->mute = !t->mute; send("trackMute " + fmtNum(h.a) + (t->mute ? " 1\n" : " 0\n")); }
      if (h.id == 4031) { t->solo = !t->solo; send("trackSolo " + fmtNum(h.a) + (t->solo ? " 1\n" : " 0\n")); }
      if (h.id == 4032) { t->arm = !t->arm; }
      return true;
    }
    case 4033: {  // automation menu for this strip
      std::vector<MenuItem> it = {{"AUTOMATE VOLUME", 1}, {"AUTOMATE PAN", 2},
                                  {"AUTOMATE SEND A", 3}, {"AUTOMATE SEND B", 4},
                                  {"AUTOMATE WIDTH", 5}};
      // mark existing lanes (item id -> lane kind: 1..5 -> 0,1,2,3,7)
      static const int kKindOf[5] = {0, 1, 2, 3, 7};
      for (auto& mi : it)
        for (const AutoLane& l : proj_.autoLanes)
          if (l.track == h.a && l.kind == kKindOf[mi.id - 1]) mi.checked = true;
      openMenuX("AUTOMATION: STRIP " + is(h.a), kMenuStrip, it, h.a, 0);
      return true;
    }
    default: return false;
  }
}

}  // namespace mdnative
