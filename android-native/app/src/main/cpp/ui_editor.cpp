// ui_editor.cpp - the detail panel frame (tabs Device / Clip / Keys) and the
// clip editors (web map section 2b): piano roll with velocity lane, drum
// step editor, and the audio clip editor.
#include "ui_core.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "ui_widgets.h"

namespace mdnative {
namespace {
std::string is(int v) { return std::to_string(v); }
const char* kNoteNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
std::string noteName(int p) {
  return std::string(kNoteNames[((p % 12) + 12) % 12]) + is(p / 12 - 1);
}
bool isBlack(int p) {
  int pc = ((p % 12) + 12) % 12;
  return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}
}  // namespace

// ------------------------------------------------------------- detail frame
void App::drawDetail(std::vector<DrawCmd>* o) {
  using namespace ui;
  Rect D = detailR_;
  rect(o, D, PANEL);
  rect(o, {D.x, D.y, D.w, 2}, LINE);
  auto add = [&](Rect r, int id, int a = 0, int b = 0, double v = 0) { hits_.push_back({r, id, a, b, v}); };
  const char* tn[3] = {"DEVICE", "CLIP", "KEYS"};
  DetailTab tabs[3] = {DetailTab::Device, DetailTab::Clip, DetailTab::Keys};
  for (int i = 0; i < 3; i++) {
    Rect r = {D.x + 6 + i * 116.0f, D.y + 3, 110, 24};
    buttonS(o, r, tn[i], tab_ == tabs[i]);
    add(r, 8001 + i);
  }
  Track* t = selTrackPtr();
  std::string ctx = t ? t->name : "MASTER";
  if (tab_ == DetailTab::Clip && selClipPtr()) ctx += "  /  " + selClipPtr()->name;
  textClip(o, D.x + 370, D.y + 8, 2.2f, DIM, ctx, 640);
  Rect mx = {kDesignW - 118, D.y + 3, 52, 24}, sh = {kDesignW - 60, D.y + 3, 52, 24};
  buttonS(o, mx, detailMax_ ? "MIN" : "MAX", false);
  buttonS(o, sh, detailOpen_ ? "SHUT" : "OPEN", false);
  add(mx, 8005);
  add(sh, 8004);
  detailContentR_ = {D.x, D.y + 30, D.w, D.h - 30};
  if (!detailOpen_) return;
  switch (tab_) {
    case DetailTab::Device: drawDevice(o); break;
    case DetailTab::Clip: drawEditor(o); break;
    case DetailTab::Keys: drawKeys(o); break;
  }
}

bool App::touchDetail(const Hit& h, int, float, float) {
  switch (h.id) {
    case 8001: tab_ = DetailTab::Device; detailOpen_ = true; return true;
    case 8002:
      tab_ = DetailTab::Clip;
      detailOpen_ = true;
      if (selClip_ < 0) {
        Track* t = selTrackPtr();
        if (t && !t->clips.empty()) selClip_ = 0;
      }
      return true;
    case 8003: tab_ = DetailTab::Keys; detailOpen_ = true; return true;
    case 8004: detailOpen_ = !detailOpen_; return true;
    case 8005: detailMax_ = !detailMax_; detailOpen_ = true; return true;
    default: return false;
  }
}

// ------------------------------------------------------------- clip editor
void App::drawEditor(std::vector<DrawCmd>* o) {
  using namespace ui;
  Rect C = detailContentR_;
  auto add = [&](Rect r, int id, int a = 0, int b = 0, double v = 0) { hits_.push_back({r, id, a, b, v}); };
  Track* t = selTrackPtr();
  if (selTarget_ != 0 || !t) {
    text(o, C.x + 16, C.y + 16, 2.4f, DIM, "CLIP EDITOR: SELECT A TRACK IN THE ARRANGEMENT");
    return;
  }
  Clip* c = selClipPtr();
  if (!c) {
    text(o, C.x + 16, C.y + 16, 2.4f, DIM, "CLIP EDITOR: SELECT A CLIP (OR +CLIP ON THE TRACK HEADER)");
    return;
  }
  // toolbar
  Rect tb = {C.x, C.y, C.w, 34};
  rect(o, tb, PANEL2);
  Rect nm = {C.x + 8, C.y + 5, 220, 24};
  rect(o, nm, CELLOFF);
  textClip(o, nm.x + 6, nm.y + 5, 2.2f, TXT, c->name, nm.w - 12);
  add(nm, 3006);
  if (t->kind == kDrum) {
    Rect sm = {C.x + 236, C.y + 5, 92, 24};
    buttonS(o, sm, edMode_ == 0 ? "STEP" : "NOTES", false);
    add(sm, 3001);
  }
  Rect pl = {C.x + 336, C.y + 5, 150, 24};
  buttonS(o, pl, "PAT " + is((int)std::round(c->patternLen / barTicks())) + " BAR", false);
  add(pl, 3002);
  Rect zm = {C.x + 494, C.y + 5, 40, 24}, zp = {C.x + 538, C.y + 5, 40, 24};
  buttonS(o, zm, "Z-", false);
  buttonS(o, zp, "Z+", false);
  add(zm, 3003);
  add(zp, 3004);
  Rect cmn = {C.x + 1080, C.y + 5, 90, 24};
  buttonS(o, cmn, "CLIP...", false);
  add(cmn, 3008);
  Rect del2 = {C.x + 1180, C.y + 5, 90, 24};
  buttonS(o, del2, "DEL CLIP", false);
  add(del2, 3009);
  if (t->kind != kAudio) {
    Rect del = {C.x + 586, C.y + 5, 110, 24};
    buttonS(o, del, "DEL NOTE", selNote_ >= 0);
    add(del, 3005);
    text(o, C.x + 710, C.y + 9, 2, DIM, is((int)c->notes.size()) + " NOTES - TAP GRID = ADD, DRAG NOTE = MOVE, EDGE = LENGTH");
  } else {
    text(o, C.x + 710, C.y + 9, 2, DIM, "AUDIO CLIP (SILENT: NO SAMPLE DATA CHANNEL - SEE HELP)");
  }
  Rect body = {C.x, C.y + 34, C.w, C.h - 34};
  if (t->kind == kAudio) { drawAudioEditor(o, body); return; }
  if (t->kind == kDrum && edMode_ == 0) { drawStepEditor(o, body); return; }
  drawPianoRoll(o, body);
}

void App::drawPianoRoll(std::vector<DrawCmd>* o, Rect body) {
  using namespace ui;
  auto add = [&](Rect r, int id, int a = 0, int b = 0, double v = 0) { hits_.push_back({r, id, a, b, v}); };
  Track* t = selTrackPtr();
  Clip* c = selClipPtr();
  if (!t || !c) return;
  float velH = 54;
  edKeysR_ = {body.x, body.y, 58, body.h - velH};
  edGridR_ = {body.x + 58, body.y, body.w - 58, body.h - velH};
  edVelR_ = {body.x + 58, body.y + body.h - velH, body.w - 58, velH};
  Rect G = edGridR_;
  rect(o, G, 0x1E1E24FF);
  add(G, 3000);
  add(edKeysR_, 3007);
  // horizontal pitch rows
  int rows = (int)(G.h / edRowH_) + 1;
  for (int i = 0; i < rows; i++) {
    int pitch = edTopPitch_ - i;
    if (pitch < 0) break;
    float y = G.y + i * edRowH_;
    if (isBlack(pitch)) rect(o, {G.x, y, G.w, edRowH_}, 0x232329FF);
    if (pitch % 12 == 0) rect(o, {G.x, y + edRowH_ - 1, G.w, 1}, PANEL3);
    Rect kr = {body.x, y, 58, edRowH_ - 1};
    rect(o, kr, isBlack(pitch) ? BLACKKEY : WHITEKEY);
    if (pitch % 12 == 0 || edRowH_ >= 18) text(o, kr.x + 4, y + 4, 1.8f, isBlack(pitch) ? DIM : DARK, noteName(pitch));
  }
  // vertical beat lines + pattern end
  double beat = barTicks() / (proj_.sigNum > 0 ? proj_.sigNum : 4);
  for (double tk = 0; tk <= c->patternLen; tk += beat) {
    float x = G.x + (float)((tk - edTick0_) * edPpt_);
    if (x < G.x || x > G.x + G.w) continue;
    rect(o, {x, G.y, 1, G.h}, std::fmod(tk, barTicks()) == 0 ? PANEL3 : 0x2C2C34FF);
  }
  float endX = G.x + (float)((c->patternLen - edTick0_) * edPpt_);
  if (endX >= G.x && endX <= G.x + G.w) rect(o, {endX, G.y, 2, G.h}, ACC);
  rect(o, edVelR_, 0x22222AFF);
  // notes
  for (size_t i = 0; i < c->notes.size(); i++) {
    const Note& n = c->notes[i];
    int row = edTopPitch_ - n.pitch;
    if (row < -1 || row > rows) continue;
    float x = G.x + (float)((n.start - edTick0_) * edPpt_);
    float w = std::max(5.0f, (float)(n.len * edPpt_));
    float y = G.y + row * edRowH_ + 2;
    if (x + w < G.x || x > G.x + G.w) continue;
    Rect nr = {std::max(x, G.x), y, std::min(x + w, G.x + G.w) - std::max(x, G.x), edRowH_ - 4};
    uint32_t col = t->kind == kDrum ? ACC : ACC2;
    rect(o, nr, col);
    if ((int)i == selNote_) frame(o, nr, TXT);
    add(nr, 3010, (int)i, 0, 0);
    Rect er = {nr.x + nr.w - 9, nr.y, 9, nr.h};
    add(er, 3011, (int)i, 0, 0);
    // velocity bar
    float vx = G.x + (float)((n.start - edTick0_) * edPpt_);
    if (vx >= edVelR_.x && vx < edVelR_.x + edVelR_.w) {
      float bh = edVelR_.h * n.vel;
      rect(o, {vx, edVelR_.y + edVelR_.h - bh, 5, bh}, YEL);
      add({vx - 3, edVelR_.y, 11, edVelR_.h}, 3020, (int)i, 0, 0);
    }
  }
  // velocity lane separator + playhead (pattern-local)
  rect(o, {edVelR_.x, edVelR_.y, edVelR_.w, 1}, LINE);
  text(o, body.x + 2, edVelR_.y + 6, 1.8f, DIM, "VEL");
  double local = std::fmod(snap_.posTicks - c->start, c->patternLen > 0 ? c->patternLen : 3840);
  if (snap_.playing && selClip_ >= 0 && snap_.posTicks >= c->start && snap_.posTicks < c->start + c->len) {
    float px = G.x + (float)((local - edTick0_) * edPpt_);
    if (px >= G.x && px <= G.x + G.w) rect(o, {px, G.y, 2, G.h + velH}, 0xFFFFFFAA);
  }
}

void App::drawStepEditor(std::vector<DrawCmd>* o, Rect body) {
  using namespace ui;
  auto add = [&](Rect r, int id, int a = 0, int b = 0, double v = 0) { hits_.push_back({r, id, a, b, v}); };
  Track* t = selTrackPtr();
  Clip* c = selClipPtr();
  if (!t || !c) return;
  edGridR_ = {body.x + 92, body.y, body.w - 92, body.h};
  Rect G = edGridR_;
  rect(o, G, 0x1E1E24FF);
  add(G, 3000);
  int steps = (int)std::max(1.0, std::round(c->patternLen / 240.0));
  float rowH = G.h / 6;
  float stepW = (float)(240.0 * edPpt_);
  for (int p = 0; p < 6; p++) {
    float y = G.y + p * rowH;
    Rect lr = {body.x, y, 88, rowH - 2};
    rect(o, lr, PANEL2);
    textClip(o, lr.x + 6, y + rowH / 2 - 6, 2, TXT, defs::kDrumPads[p].name, 80);
    add(lr, 3031, p, 0, 0);  // tap label = preview pad
    for (int s = 0; s < steps; s++) {
      float x = G.x + (float)((s * 240.0 - edTick0_) * edPpt_);
      if (x + stepW < G.x || x > G.x + G.w) continue;
      bool on = false;
      for (const Note& n : c->notes)
        if (n.pitch == defs::kDrumPads[p].pitch && (int)std::round(n.start / 240.0) == s) { on = true; break; }
      Rect cell = {x + 1, y + 1, stepW - 2, rowH - 3};
      rect(o, cell, on ? ACC : (s % 4 == 0 ? CELL : CELLOFF));
      add(cell, 3030, p, s, 0);
    }
  }
  // step playhead
  if (snap_.playing && snap_.posTicks >= c->start && snap_.posTicks < c->start + c->len) {
    double local = std::fmod(snap_.posTicks - c->start, c->patternLen);
    float px = G.x + (float)((local - edTick0_) * edPpt_);
    if (px >= G.x && px <= G.x + G.w) rect(o, {px, G.y, 2, G.h}, 0xFFFFFFAA);
  }
}

void App::drawAudioEditor(std::vector<DrawCmd>* o, Rect body) {
  using namespace ui;
  auto add = [&](Rect r, int id, int a = 0, int b = 0, double v = 0) { hits_.push_back({r, id, a, b, v}); };
  Clip* c = selClipPtr();
  if (!c) return;
  Rect W = {body.x + 8, body.y + 6, body.w - 16, body.h * 0.52f};
  rect(o, W, 0x1E1E24FF);
  frame(o, W, LINE);
  unsigned seed = 2166136261u;
  for (char ch : c->sampleName) seed = (seed ^ (unsigned char)ch) * 16777619u;
  int nb = (int)(W.w / 5);
  for (int i = 0; i < nb; i++) {
    seed = seed * 1664525u + 1013904223u;
    float v = ((seed >> 9) & 0x3FF) / 1023.0f;
    float bh = W.h * 0.44f * v + 2;
    rect(o, {W.x + 2 + i * 5.0f, W.cy() - bh, 3, bh * 2}, ACC2);
  }
  text(o, W.x + 8, W.y + 6, 2.2f, DIM, c->sampleName.empty() ? "(NO SAMPLE)" : c->sampleName);
  // sliders: offset / gain / fade in / fade out
  struct S { const char* label; double v; int id; };
  S ss[4] = {{"OFFSET", c->offset / 8, 10}, {"GAIN", c->gain / 2, 11}, {"FADE IN", c->fadeIn / 1920, 12}, {"FADE OUT", c->fadeOut / 1920, 13}};
  for (int i = 0; i < 4; i++) {
    float y = W.y + W.h + 14 + i * 30.0f;
    text(o, body.x + 10, y + 3, 2.2f, DIM, ss[i].label);
    Rect sr = {body.x + 130, y, 420, 18};
    hslider(o, sr, (float)std::min(1.0, std::max(0.0, ss[i].v)));
    add(sr, 3060 + i, ss[i].id, 0, 0);
  }
  Rect sp = {body.x + 600, W.y + W.h + 14, 190, 40}, sb = {body.x + 802, W.y + W.h + 14, 190, 40}, mb = {body.x + 1004, W.y + W.h + 14, 190, 40};
  buttonS(o, sp, "SAMPLE...", false);
  buttonS(o, sb, "SPLIT AT PLAYHEAD", false);
  buttonS(o, mb, c->muted ? "UNMUTE" : "MUTE", c->muted);
  add(sp, 3064);
  add(sb, 3065);
  add(mb, 3066);
  text(o, body.x + 600, W.y + W.h + 66, 2, FAINT, "WAVEFORM IS A PLACEHOLDER: SAMPLE BYTES");
  text(o, body.x + 600, W.y + W.h + 88, 2, FAINT, "CANNOT CROSS THE TEXT PROTOCOL (SEE HELP).");
}

bool App::touchEditor(const Hit& h, int action, float x, float y) {
  (void)action;
  Track* t = selTrackPtr();
  Clip* c = selClipPtr();
  switch (h.id) {
    case 3000: {  // grid background: scroll (tap = add note in piano roll)
      drag_ = Drag{};
      drag_.what = kDragScrollEd;
      drag_.x0 = x; drag_.y0 = y;
      drag_.t0 = edTick0_; drag_.v0 = edTopPitch_;
      drag_.c = (t && (t->kind == kMidi || (t->kind == kDrum && edMode_ == 1))) ? 1 : 0;
      return true;
    }
    case 3001: edMode_ = edMode_ == 0 ? 1 : 0; return true;
    case 3002: {
      if (!c) return true;
      std::vector<MenuItem> it;
      const int bars[6] = {1, 2, 4, 8, 16, 32};
      for (int b : bars) it.push_back({is(b) + " BAR", b, c->patternLen == b * barTicks()});
      openMenu("PATTERN LENGTH", kMenuPatLen, it);
      return true;
    }
    case 3003: edPpt_ = std::max(0.02, edPpt_ / 1.4); return true;
    case 3004: edPpt_ = std::min(1.2, edPpt_ * 1.4); return true;
    case 3005:
      if (c && selNote_ >= 0 && selNote_ < (int)c->notes.size()) {
        checkpoint();
        c->notes.erase(c->notes.begin() + selNote_);
        selNote_ = -1;
        syncNotes(selTrack_);
      }
      return true;
    case 3006:
      if (c) openKeyboard(kKbClipName, c->name, "CLIP NAME");
      return true;
    case 3008: {
      if (!c) return true;
      std::vector<MenuItem> it = {{"DUPLICATE CLIP", 1}, {c->muted ? "UNMUTE CLIP" : "MUTE CLIP", 2},
                                  {"DELETE CLIP", 3}, {"RENAME CLIP", 4}, {"PATTERN LENGTH...", 5},
                                  {"SPLIT AT PLAYHEAD", 6}, {"CLEAR NOTES", 7}};
      openMenu("CLIP: " + c->name, kMenuClip, it);
      return true;
    }
    case 3009:
      if (c && t) {
        checkpoint();
        t->clips.erase(t->clips.begin() + selClip_);
        selClip_ = -1;
        syncNotes(selTrack_);
      }
      return true;
    case 3007: {  // editor key column: preview
      if (!t || selTarget_ != 0) return true;
      int row = (int)((y - edKeysR_.y) / edRowH_);
      int pitch = edTopPitch_ - row;
      if (pitch < 0 || pitch > 127) return true;
      previewPitch_ = pitch;
      send("preview " + fmtNum(selTrack_) + " " + fmtNum(pitch) + " 0.9 1\n");
      return true;
    }
    case 3010: case 3011: {  // note drag / resize
      if (!c || h.a < 0 || h.a >= (int)c->notes.size()) return true;
      checkpoint();
      selNote_ = h.a;
      drag_ = Drag{};
      drag_.what = kDragNote;
      drag_.a = selTrack_; drag_.c = h.a;
      drag_.x0 = x; drag_.y0 = y;
      drag_.t0 = c->notes[h.a].start;
      drag_.v0 = c->notes[h.a].len;
      drag_.v1 = c->notes[h.a].pitch;
      dragClipMode_ = h.id == 3011 ? 1 : 0;
      return true;
    }
    case 3020: {  // velocity bar
      if (!c || h.a < 0 || h.a >= (int)c->notes.size()) return true;
      checkpoint();
      selNote_ = h.a;
      drag_ = Drag{};
      drag_.what = kDragVel;
      drag_.a = selTrack_; drag_.c = h.a;
      drag_.v0 = edVelR_.y; drag_.w0 = edVelR_.h;
      float v = 1.0f - (y - edVelR_.y) / edVelR_.h;
      c->notes[h.a].vel = std::min(1.0f, std::max(0.05f, v));
      return true;
    }
    case 3030: {  // step cell toggle + paint drag
      if (!c || h.a < 0 || h.a >= 6) return true;
      checkpoint();
      int pitch = defs::kDrumPads[h.a].pitch;
      double st = h.b * 240.0;
      int found = -1;
      for (size_t i = 0; i < c->notes.size(); i++)
        if (c->notes[i].pitch == pitch && (int)std::round(c->notes[i].start / 240.0) == h.b) { found = (int)i; break; }
      if (found >= 0) c->notes.erase(c->notes.begin() + found);
      else c->notes.push_back({st, 120, pitch, 0.95f, 0});
      syncNotes(selTrack_);
      drag_ = Drag{};
      drag_.what = kDragPaint;
      drag_.a = found >= 0 ? 0 : 1;  // paint value
      drag_.b = h.b;                 // last painted step (avoid re-toggle)
      drag_.c = h.a;
      return true;
    }
    case 3031:  // pad label: preview the pad sound
      if (selTarget_ == 0) {
        previewPitch_ = defs::kDrumPads[h.a].pitch;
        send("preview " + fmtNum(selTrack_) + " " + fmtNum(previewPitch_) + " 0.95 1\n");
      }
      return true;
    case 3060: case 3061: case 3062: case 3063: {  // audio sliders
      drag_ = Drag{};
      drag_.what = kDragSlider;
      drag_.a = h.a;
      drag_.v0 = h.r.x; drag_.w0 = h.r.w; drag_.x0 = x; drag_.y0 = y;
      applySlider(h.a, (x - h.r.x) / h.r.w, false);
      return true;
    }
    case 3064: {  // sample picker
      std::vector<MenuItem> it;
      int n = 0;
      for (const char* const* p = defs::kSampleFiles; *p; p++, n++) it.push_back({*p, n, c && c->sampleId == n});
      if (it.empty()) it.push_back({"(NO SAMPLES IN REPO)", 0, false, true});
      openMenu("SAMPLE (NAME ONLY - SILENT)", kMenuSamplePick, it);
      return true;
    }
    case 3065: {  // split at playhead
      if (!c || !t) return true;
      double pos = snap_.posTicks;
      if (pos <= c->start + 30 || pos >= c->start + c->len - 30) { setStatusLine("SPLIT: PLAYHEAD OUTSIDE CLIP"); return true; }
      checkpoint();
      Clip nc = *c;
      nc.start = pos;
      nc.len = c->start + c->len - pos;
      nc.offset = c->offset + (pos - c->start) / (barTicks() / (proj_.sigNum > 0 ? proj_.sigNum : 4)) * (60.0 / proj_.bpm);
      c->len = pos - c->start;
      t->clips.insert(t->clips.begin() + selClip_ + 1, nc);
      syncNotes(selTrack_);
      return true;
    }
    case 3066:
      if (c) { checkpoint(); c->muted = !c->muted; syncNotes(selTrack_); }
      return true;
    default: return false;
  }
}

void App::editorTapAdd(float x, float y) {
  Track* t = selTrackPtr();
  Clip* c = selClipPtr();
  if (!t || !c || t->kind == kAudio) return;
  if (!edGridR_.contains(x, y)) return;
  int row = (int)((y - edGridR_.y) / edRowH_);
  int pitch = edTopPitch_ - row;
  double tick = edTick0_ + (x - edGridR_.x) / edPpt_;
  double snap = snapTicks();
  if (snap > 1) tick = std::round(tick / snap) * snap;
  if (pitch < 0 || pitch > 127 || tick < 0 || tick >= c->patternLen) return;
  checkpoint();
  Note n;
  n.start = tick;
  n.len = edLastLen_;
  n.pitch = pitch;
  n.vel = 0.9f;
  c->notes.push_back(n);
  selNote_ = (int)c->notes.size() - 1;
  syncNotes(selTrack_);
}

void App::editorPaint(float x, float y) {
  Track* t = selTrackPtr();
  Clip* c = selClipPtr();
  if (!t || !c || t->kind != kDrum || edMode_ != 0) return;
  if (!edGridR_.contains(x, y)) return;
  int pads = 6;
  float rowH = edGridR_.h / pads;
  int p = (int)((y - edGridR_.y) / rowH);
  int s = (int)std::floor((edTick0_ + (x - edGridR_.x) / edPpt_) / 240.0);
  if (p < 0 || p >= pads || s < 0 || s * 240.0 >= c->patternLen) return;
  int pitch = defs::kDrumPads[p].pitch;
  int found = -1;
  for (size_t i = 0; i < c->notes.size(); i++)
    if (c->notes[i].pitch == pitch && (int)std::round(c->notes[i].start / 240.0) == s) { found = (int)i; break; }
  bool changed = false;
  if (drag_.a == 1 && found < 0) { c->notes.push_back({s * 240.0, 120, pitch, 0.95f, 0}); changed = true; }
  if (drag_.a == 0 && found >= 0) { c->notes.erase(c->notes.begin() + found); changed = true; }
  if (changed) { syncNotes(selTrack_); drag_.b = s; drag_.c = p; }
}

}  // namespace mdnative
