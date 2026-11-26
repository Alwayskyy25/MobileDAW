// ui_arrange.cpp - Arrange view (web map section 2a): track headers, clip
// lanes, ruler with loop region + playhead, snap/zoom/follow toolbar, and
// the automation envelope editor modal (opened from the mixer).
#include "ui_core.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "ui_widgets.h"

namespace mdnative {
namespace {
std::string is(int v) { return std::to_string(v); }
const char* kKindName[5] = {"MIDI", "DRUM", "AUDIO", "AUTO", "RET"};
uint32_t trackColor(const Track& t) { return t.color; }
}  // namespace

// Layout helpers shared with touch: rows are tracks then returns.
int App::arrRowCount() const { return (int)proj_.tracks.size() + (int)proj_.returns.size(); }
float App::arrRowY(int row) const {
  float y = arrR_.y + 32;
  for (int i = 0; i < row; i++) {
    bool folded = false;
    if (i < (int)proj_.tracks.size()) folded = proj_.tracks[i].folded;
    y += folded ? 30.0f : rowH_;
  }
  return y - arrY0_;
}
float App::arrRowH(int row) const {
  if (row < (int)proj_.tracks.size() && proj_.tracks[row].folded) return 30;
  return rowH_;
}
int App::arrRowAt(float y) const {
  for (int r = 0; r < arrRowCount(); r++) {
    float ry = arrRowY(r);
    if (y >= ry && y < ry + arrRowH(r)) return r;
  }
  return -1;
}

void App::drawArrange(std::vector<DrawCmd>* o) {
  using namespace ui;
  Rect A = arrR_;
  rect(o, A, BG);
  // follow the playhead while playing (web Follow)
  if (follow_ && snap_.playing) {
    double px = headW_ + (snap_.posTicks - arrTick0_) * arrPpt_;
    if (px > kDesignW - 60 || px < headW_) arrTick0_ = std::max(0.0, snap_.posTicks - barTicks());
  }
  // ---- toolbar strip
  Rect tb = {A.x, A.y, A.w, 30};
  rect(o, tb, PANEL);
  auto add = [&](Rect r, int id, int a = 0, int b = 0, double v = 0) { hits_.push_back({r, id, a, b, v}); };
  static const char* kSnapNames[6] = {"SNAP OFF", "SNAP BAR", "SNAP BEAT", "SNAP 1/2", "SNAP 1/4", "SNAP 1/8"};
  Rect sn = {A.x + 4, A.y + 3, 118, 24};
  buttonS(o, sn, kSnapNames[snapMode_], false);
  add(sn, 2040);
  Rect fo = {A.x + 126, A.y + 3, 92, 24};
  buttonS(o, fo, "FOLLOW", follow_);
  add(fo, 2041);
  Rect zm = {A.x + 222, A.y + 3, 40, 24}, zp = {A.x + 266, A.y + 3, 40, 24};
  buttonS(o, zm, "Z-", false);
  buttonS(o, zp, "Z+", false);
  add(zm, 2042);
  add(zp, 2043);
  Rect at = {A.x + 310, A.y + 3, 118, 24};
  buttonS(o, at, "+ TRACK", false);
  add(at, 2044);
  text(o, A.x + 440, A.y + 8, 2, DIM, proj_.name + "  -  " + is((int)proj_.tracks.size()) + " TRACKS");
  textR(o, kDesignW - 8, A.y + 8, 2, DIM, status_);
  // ---- lanes background (scroll drag)
  Rect lanes = {headW_, A.y + 32, kDesignW - headW_, A.h - 32 - 26};
  hits_.push_back({lanes, 2000, 0, 0, 0});
  Rect headBg = {0, A.y + 32, headW_, A.h - 32 - 26};
  hits_.push_back({headBg, 2000, 0, 0, 0});
  // bar grid lines
  double bar = barTicks();
  if (bar > 0) {
    long firstBar = (long)std::floor(arrTick0_ / bar);
    for (long b = firstBar;; b++) {
      float x = headW_ + (float)((b * bar - arrTick0_) * arrPpt_);
      if (x > kDesignW) break;
      if (x >= headW_) rect(o, {x, A.y + 32, 1, A.h - 58}, 0x2A2A32FF);
    }
  }
  // ---- rows
  int rows = arrRowCount();
  for (int r = 0; r < rows; r++) {
    float y = arrRowY(r);
    float h = arrRowH(r);
    if (y + h < A.y + 32 || y > A.y + A.h - 26) continue;
    bool isRet = r >= (int)proj_.tracks.size();
    const Track& t = isRet ? proj_.returns[r - proj_.tracks.size()] : proj_.tracks[r];
    int strip = isRet ? stripOfReturn(r - (int)proj_.tracks.size()) : r;
    bool sel = (selTarget_ == (isRet ? 1 : 0)) && selTrack_ == (isRet ? r - (int)proj_.tracks.size() : r);
    // header
    Rect hr = {0, y, headW_, h};
    rect(o, hr, sel ? PANEL3 : PANEL);
    rect(o, {0, y, 8, h}, trackColor(t));
    add(hr, 2001, isRet ? 1000 + (r - (int)proj_.tracks.size()) : r, 0, 0);
    textClip(o, 14, y + 4, 2.4f, TXT, t.name, headW_ - 90);
    textR(o, headW_ - 6, y + 4, 2, DIM, kKindName[t.kind]);
    if (h > 34) {
      // vol + pan mini sliders
      Rect vs = {14, y + 24, 84, 10}, ps = {106, y + 24, 84, 10};
      rect(o, vs, CELLOFF);
      rect(o, {vs.x, vs.y, vs.w * (float)std::min(1.0, t.vol / 1.25), vs.h}, ACC2);
      rect(o, ps, CELLOFF);
      rect(o, {ps.cx() - 1, ps.y, 2, ps.h}, LINE);
      rect(o, {ps.x + ps.w * (float)((t.pan + 1) / 2) - 2, ps.y, 4, ps.h}, ACC);
      add(vs, 2010, strip, 0, 0);
      add(ps, 2011, strip, 0, 0);
      // M / S / +CLIP / menu
      Rect mB = {14, y + 40, 30, 20}, sB = {48, y + 40, 30, 20}, cB = {82, y + 40, 62, 20}, nB = {148, y + 40, 44, 20};
      buttonS(o, mB, "M", t.mute);
      buttonS(o, sB, "S", t.solo);
      buttonS(o, cB, "+CLIP", false);
      buttonS(o, nB, "...", false);
      add(mB, 2012, isRet ? 1000 + (r - (int)proj_.tracks.size()) : r, 0, 0);
      add(sB, 2013, isRet ? 1000 + (r - (int)proj_.tracks.size()) : r, 0, 0);
      add(cB, 2014, isRet ? 1000 + (r - (int)proj_.tracks.size()) : r, 0, 0);
      add(nB, 2015, isRet ? 1000 + (r - (int)proj_.tracks.size()) : r, 0, 0);
    }
    // lane line
    rect(o, {headW_, y + h - 1, kDesignW - headW_, 1}, 0x2A2A32FF);
    if (t.kind == kAuto) {
      text(o, headW_ + 10, y + h / 2 - 6, 2, FAINT, "AUTOMATION TRACK - EDIT VIA MIXER LONG-PRESS");
      continue;
    }
    if (isRet) {
      text(o, headW_ + 10, y + h / 2 - 6, 2, FAINT, "FX RETURN - EDIT CHAIN IN DEVICE TAB");
      continue;
    }
    // clips
    for (size_t ci = 0; ci < t.clips.size(); ci++) {
      const Clip& c = t.clips[ci];
      float x0 = headW_ + (float)((c.start - arrTick0_) * arrPpt_);
      float x1 = headW_ + (float)((c.start + c.len - arrTick0_) * arrPpt_);
      if (x1 < headW_ || x0 > kDesignW) continue;
      Rect cr = {std::max(x0, headW_), y + 4, std::min(x1, kDesignW) - std::max(x0, headW_), h - 8};
      bool csel = sel && selClip_ == (int)ci;
      uint32_t col = c.muted ? 0x4A4A55FF : trackColor(t);
      rect(o, cr, col);
      if (csel) frame(o, cr, TXT);
      textClip(o, cr.x + 5, cr.y + 3, 2, DARK, c.name, cr.w - 10);
      // content preview
      if (t.kind == kAudio) {
        // placeholder waveform: deterministic bars from the sample name
        unsigned seed = 2166136261u;
        for (char ch : c.sampleName) seed = (seed ^ (unsigned char)ch) * 16777619u;
        int nb = (int)(cr.w / 7);
        for (int i = 0; i < nb; i++) {
          seed = seed * 1664525u + 1013904223u;
          float v = ((seed >> 9) & 0x3FF) / 1023.0f * 0.45f + 0.08f;
          float bh = (cr.h - 18) * v;
          rect(o, {cr.x + 3 + i * 7.0f, cr.y + cr.h - 6 - bh, 4, bh}, DARK);
        }
      } else {
        // mini note preview (first pattern cycle)
        for (const Note& n : c.notes) {
          float nx = cr.x + (float)(n.start / (c.patternLen > 0 ? c.patternLen : 3840) * cr.w);
          float ny = cr.y + cr.h - 8 - (float)((n.pitch % 24) / 24.0f) * (cr.h - 16);
          rect(o, {nx, ny, std::max(3.0f, (float)(n.len / (c.patternLen > 0 ? c.patternLen : 3840) * cr.w)), 3}, DARK);
        }
      }
      add(cr, 2020, r, (int)ci, 0);
      Rect el = {cr.x, cr.y, 10, cr.h}, er = {cr.x + cr.w - 10, cr.y, 10, cr.h};
      add(el, 2021, r, (int)ci, 0);
      add(er, 2022, r, (int)ci, 0);
    }
  }
  // ---- ruler (bottom)
  Rect ru = {0, A.y + A.h - 26, kDesignW, 26};
  rect(o, ru, PANEL);
  add(ru, 2030);
  if (bar > 0) {
    long firstBar = (long)std::floor(arrTick0_ / bar);
    for (long b = firstBar;; b++) {
      float x = headW_ + (float)((b * bar - arrTick0_) * arrPpt_);
      if (x > kDesignW) break;
      if (x >= headW_) text(o, x + 3, ru.y + 6, 2, DIM, is((int)b + 1));
    }
  }
  // loop region on the ruler
  float lx0 = headW_ + (float)((proj_.loopStart - arrTick0_) * arrPpt_);
  float lx1 = headW_ + (float)((proj_.loopStart + proj_.loopLen - arrTick0_) * arrPpt_);
  if (proj_.loopOn && lx1 > headW_ && lx0 < kDesignW)
    rect(o, {std::max(lx0, headW_), ru.y, std::min(lx1, kDesignW) - std::max(lx0, headW_), 4}, ACC);
  // playhead
  float px = headW_ + (float)((snap_.posTicks - arrTick0_) * arrPpt_);
  if (px >= headW_ && px <= kDesignW) rect(o, {px, A.y + 32, 2, A.h - 58}, 0xFFFFFFFF);
}

bool App::touchArrange(const Hit& h, int action, float x, float y) {
  (void)action;
  auto trackOf = [&](int a, bool* isRet) -> Track* {
    if (a >= 1000) {
      *isRet = true;
      int i = a - 1000;
      return i < (int)proj_.returns.size() ? &proj_.returns[i] : nullptr;
    }
    *isRet = false;
    return a < (int)proj_.tracks.size() ? &proj_.tracks[a] : nullptr;
  };
  switch (h.id) {
    case 2000: {  // scroll background
      drag_ = Drag{};
      drag_.what = kDragScrollArr;
      drag_.x0 = x; drag_.y0 = y;
      drag_.t0 = arrTick0_; drag_.v0 = arrY0_;
      drag_.a = (int)x; drag_.b = (int)y;  // tap candidate (select track by row)
      drag_.c = 1;
      return true;
    }
    case 2001: {  // header select
      bool isRet;
      Track* t = trackOf(h.a, &isRet);
      if (!t) return true;
      selTarget_ = isRet ? 1 : 0;
      selTrack_ = isRet ? h.a - 1000 : h.a;
      selClip_ = -1;
      return true;
    }
    case 2010: {  // header vol (h.a = engine strip)
      drag_ = Drag{};
      drag_.what = kDragHeadVol;
      drag_.a = h.a;
      drag_.v0 = h.r.x; drag_.w0 = h.r.w; drag_.x0 = x; drag_.y0 = y;
      float frac = (x - h.r.x) / h.r.w;
      applyFader(h.a, std::min(1.0f, std::max(0.0f, frac)) * 1.25, false);
      return true;
    }
    case 2011: {  // header pan
      drag_ = Drag{};
      drag_.what = kDragHeadPan;
      drag_.a = h.a;
      drag_.v0 = h.r.x; drag_.w0 = h.r.w; drag_.x0 = x; drag_.y0 = y;
      float frac = (x - h.r.x) / h.r.w;
      applyKnob(4, h.a, 0, std::min(1.0f, std::max(0.0f, frac)), false);
      return true;
    }
    case 2012: case 2013: {  // M / S
      bool isRet;
      Track* t = trackOf(h.a, &isRet);
      if (!t) return true;
      checkpoint();
      int strip = isRet ? stripOfReturn(h.a - 1000) : h.a;
      if (h.id == 2012) { t->mute = !t->mute; send("trackMute " + fmtNum(strip) + (t->mute ? " 1\n" : " 0\n")); }
      else { t->solo = !t->solo; send("trackSolo " + fmtNum(strip) + (t->solo ? " 1\n" : " 0\n")); }
      selTarget_ = isRet ? 1 : 0;
      selTrack_ = isRet ? h.a - 1000 : h.a;
      return true;
    }
    case 2014: {  // +CLIP
      bool isRet;
      Track* t = trackOf(h.a, &isRet);
      if (!t || isRet || t->kind == kAuto) return true;
      checkpoint();
      double at = 0;
      for (const Clip& c : t->clips) at = std::max(at, c.start + c.len);
      Clip c;
      c.start = at;
      c.len = barTicks();
      c.patternLen = barTicks();
      c.name = t->kind == kAudio ? "Audio" : "Pattern";
      if (t->kind == kAudio) { c.sampleId = 0; c.sampleName = defs::kSampleFiles[0] ? defs::kSampleFiles[0] : ""; }
      t->clips.push_back(c);
      selTarget_ = 0; selTrack_ = h.a; selClip_ = (int)t->clips.size() - 1;
      syncNotes(h.a);
      tab_ = DetailTab::Clip;
      detailOpen_ = true;
      return true;
    }
    case 2015: {  // header "..." menu
      bool isRet;
      Track* t = trackOf(h.a, &isRet);
      if (!t) return true;
      selTarget_ = isRet ? 1 : 0;
      selTrack_ = isRet ? h.a - 1000 : h.a;
      std::vector<MenuItem> it = {
          {"RENAME", 1}, {"DUPLICATE TRACK", 2}, {"DELETE TRACK", 3},
          {t->folded ? "UNFOLD" : "FOLD", 4}, {"ADD CLIP", 5},
          {"CHANGE INSTRUMENT...", 6}, {"CLEAR CLIPS", 7}, {"NEXT COLOR", 8}};
      if (isRet) it = {{"RENAME", 1}, {"DELETE RETURN", 3}, {"EDIT FX CHAIN (DEVICE TAB)", 9}};
      openMenuX("TRACK: " + t->name, kMenuTrack, it, h.a, 0);
      return true;
    }
    case 2020: case 2021: case 2022: {  // clip body / edges
      Track* t = h.a < (int)proj_.tracks.size() ? &proj_.tracks[h.a] : nullptr;
      if (!t || h.b < 0 || h.b >= (int)t->clips.size()) return true;
      selTarget_ = 0; selTrack_ = h.a; selClip_ = h.b;
      tab_ = DetailTab::Clip;
      detailOpen_ = true;
      checkpoint();
      drag_ = Drag{};
      drag_.what = kDragClip;
      drag_.a = h.a; drag_.b = h.b;
      drag_.x0 = x; drag_.y0 = y;
      drag_.t0 = t->clips[h.b].start;
      drag_.v0 = t->clips[h.b].len;
      drag_.v1 = t->clips[h.b].offset;
      dragClipMode_ = h.id == 2020 ? 0 : h.id == 2021 ? 1 : 2;
      drag_.c = (int)h.id;  // tap detection: no-move tap on body opens menu
      return true;
    }
    case 2030: {  // ruler seek
      drag_ = Drag{};
      drag_.what = kDragSeek;
      drag_.x0 = x; drag_.y0 = y;
      double tick = arrTick0_ + (x - headW_) / arrPpt_;
      send("seek " + fmtNum(std::max(0.0, tick)) + "\n");
      return true;
    }
    case 2040: {
      std::vector<MenuItem> it;
      const char* nm[6] = {"OFF", "BAR", "BEAT", "1/2", "1/4", "1/8"};
      for (int i = 0; i < 6; i++) it.push_back({std::string("SNAP ") + nm[i], i, snapMode_ == i});
      openMenu("SNAP", kMenuSnap, it);
      return true;
    }
    case 2041: follow_ = !follow_; return true;
    case 2042: arrPpt_ = std::max(0.008, arrPpt_ / 1.4); return true;
    case 2043: arrPpt_ = std::min(0.6, arrPpt_ * 1.4); return true;
    case 2044: {
      std::vector<MenuItem> it = {{"MIDI TRACK", 1}, {"DRUM TRACK", 2}, {"AUDIO TRACK", 3},
                                  {"AUTOMATION TRACK", 4}, {"FX RETURN TRACK", 10}};
      openMenu("ADD TRACK", kMenuAddTrack, it);
      return true;
    }
    default: return false;
  }
}

// ------------------------------------------------------- envelope editor
void App::drawEnvEditor(std::vector<DrawCmd>* o) {
  using namespace ui;
  if (envLane_ < 0 || envLane_ >= (int)proj_.autoLanes.size()) { modal_ = Modal::None; return; }
  AutoLane& l = proj_.autoLanes[envLane_];
  Rect box = {140, 90, 1000, 540};
  rect(o, box, PANEL);
  frame(o, box, LINE);
  static const char* kNames[8] = {"VOLUME", "PAN", "SEND A", "SEND B", "DEVICE PARAM", "INST PARAM", "?", "WIDTH"};
  text(o, box.x + 16, box.y + 14, 3, TXT, std::string("AUTOMATION: ") + kNames[l.kind & 7] + "  STRIP " + is(l.track));
  Rect closeR = {box.x + box.w - 52, box.y + 8, 44, 36};
  buttonS(o, closeR, "X", false);
  hits_.push_back({closeR, 9001, 0, 0, 0});
  Rect g = {box.x + 20, box.y + 56, box.w - 40, box.h - 130};
  rect(o, g, CELLOFF);
  hits_.push_back({g, 9501, 0, 0, 0});
  double vlo, vhi;
  envRange(l, &vlo, &vhi);
  double span = 16 * barTicks();
  edPpt_ = g.w / span;
  // grid lines per bar
  for (int b = 0; b <= 16; b++) {
    float x = g.x + (float)(b * barTicks() * edPpt_);
    rect(o, {x, g.y, 1, g.h}, 0x33333CFF);
  }
  // polyline as stepped segments
  for (size_t i = 0; i < l.pts.size(); i++) {
    float px = g.x + (float)(l.pts[i].first * edPpt_);
    float py = g.y + g.h * (1 - (float)((l.pts[i].second - vlo) / (vhi - vlo)));
    if (i > 0) {
      float qx = g.x + (float)(l.pts[i - 1].first * edPpt_);
      float qy = g.y + g.h * (1 - (float)((l.pts[i - 1].second - vlo) / (vhi - vlo)));
      int steps = std::max(1, (int)std::fabs(px - qx) / 6);
      for (int s = 0; s <= steps; s++) {
        float t = s / (float)steps;
        rect(o, {qx + (px - qx) * t - 1, qy + (py - qy) * t - 1, 3, 3}, ACC);
      }
    }
    Rect pr = {px - 9, py - 9, 18, 18};
    rect(o, pr, i == (size_t)envSelPt_ ? TXT : ACC2);
    hits_.push_back({pr, 9500, (int)i, 0, 0});
  }
  // playhead
  float px = g.x + (float)(snap_.posTicks * edPpt_);
  if (px >= g.x && px <= g.x + g.w) rect(o, {px, g.y, 2, g.h}, 0xFFFFFF88);
  Rect del = {box.x + 20, box.y + box.h - 60, 170, 44}, clr = {box.x + 202, box.y + box.h - 60, 170, 44};
  buttonS(o, del, "DELETE POINT", false);
  buttonS(o, clr, "CLEAR LANE", false);
  hits_.push_back({del, 9502, 0, 0, 0});
  hits_.push_back({clr, 9503, 0, 0, 0});
  if (envSelPt_ >= 0 && envSelPt_ < (int)l.pts.size())
    text(o, box.x + 400, box.y + box.h - 48, 2.4f, DIM,
         "PT " + is(envSelPt_ + 1) + "  TICK " + is((int)l.pts[envSelPt_].first) + "  VAL " + fmtNum(l.pts[envSelPt_].second));
  text(o, box.x + 20, box.y + box.h - 88, 2, FAINT, "TAP GRAPH = ADD POINT - DRAG POINT = MOVE");
}

bool App::touchEnvEditor(const Hit& h, int, float x, float y) {
  if (envLane_ < 0 || envLane_ >= (int)proj_.autoLanes.size()) return true;
  AutoLane& l = proj_.autoLanes[envLane_];
  switch (h.id) {
    case 9500: {  // point drag
      checkpoint();
      drag_ = Drag{};
      drag_.what = kDragEnvPt;
      drag_.b = h.a;
      drag_.x0 = x; drag_.y0 = y;
      drag_.t0 = l.pts[h.a].first;
      drag_.v0 = h.r.y - 130;   // graph top approx (recomputed below)
      // graph rect: box {140,90,1000,540} -> g.y = 146, g.h = 410
      drag_.v0 = 146; drag_.w0 = 410;
      envSelPt_ = h.a;
      return true;
    }
    case 9501: {  // add point
      checkpoint();
      double tick = (x - 160) / edPpt_;
      double snap = snapTicks();
      if (snap > 1) tick = std::round(tick / snap) * snap;
      double vlo, vhi;
      envRange(l, &vlo, &vhi);
      double val = vlo + (1 - (y - 146) / 410.0) * (vhi - vlo);
      l.pts.push_back({std::max(0.0, tick), val});
      std::stable_sort(l.pts.begin(), l.pts.end(),
                       [](const std::pair<double, double>& a, const std::pair<double, double>& b) { return a.first < b.first; });
      send(buildAutomation(proj_));
      dirty_ = true;
      return true;
    }
    case 9502:
      if (envSelPt_ >= 0 && envSelPt_ < (int)l.pts.size() && l.pts.size() > 1) {
        checkpoint();
        l.pts.erase(l.pts.begin() + envSelPt_);
        envSelPt_ = -1;
        send(buildAutomation(proj_));
        dirty_ = true;
      }
      return true;
    case 9503:
      checkpoint();
      l.pts.clear();
      l.pts.push_back({0, l.kind == 1 ? 0 : (l.kind == 7 ? 1 : 0.85)});
      envSelPt_ = -1;
      send(buildAutomation(proj_));
      dirty_ = true;
      return true;
    default: return false;
  }
}

}  // namespace mdnative
