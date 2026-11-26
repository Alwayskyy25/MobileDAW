// ui_core.cpp - App core: engine synchronisation, hit/drag dispatch,
// menus & modals, undo/redo, and the file bridge to the shell.
// Screen-specific drawing/touch live in ui_topbar/arrange/editor/mixer/
// device/keys/browser .cpp - all are App member functions.
#include "ui_core.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include "ui_widgets.h"

namespace mdnative {
namespace {
std::string trimErr(const std::string& e) {
  std::string s = e.substr(0, e.find('\n'));
  return s.size() > 64 ? s.substr(0, 64) : s;
}
double clampd(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }
}  // namespace

// ------------------------------------------------------------------ status
void App::pushLog(const std::string& s) {
  log_.push_back(s);
  if (log_.size() > 60) log_.erase(log_.begin(), log_.begin() + (log_.size() - 60));
}
void App::setStatusLine(const std::string& s) {
  status_ = s;
  pushLog(s);
}
void App::setStatus(const std::string& s) { setStatusLine(s); }
void App::setEngine(EngineLink* e) { eng_ = e; }

// ------------------------------------------------------------------ engine
void App::sendRaw(const std::string& text) {
  if (!eng_) {
    if (!text.empty()) setStatusLine("NO ENGINE (command dropped)");
    return;
  }
  std::string err = eng_->cmd(text);
  if (!err.empty()) setStatusLine("ENGINE: " + trimErr(err));
}
void App::send(const std::string& text) {
  ensureSetup();
  sendRaw(text);
}
void App::ensureSetup() {
  if (setupDone_ || !eng_) return;
  setupDone_ = true;
  sendRaw(buildTransport(proj_, false));
  sendRaw(buildSig(proj_));
  sendRaw(buildLoop(proj_));
  sendRaw(std::string("setMetro ") + (proj_.metro ? "1\n" : "0\n"));
  sendRaw(buildSyncAll(proj_));
  setStatusLine("ENGINE READY");
}
void App::syncMixerStrip(int strip) {
  if (strip == kMasterStrip) { send(buildMasterMixer(proj_)); return; }
  if (strip >= 65) {
    int i = strip - 65;
    if (i >= 0 && i < (int)proj_.returns.size()) send(buildStripMixer(strip, proj_.returns[i]));
    return;
  }
  if (strip >= 0 && strip < (int)proj_.tracks.size()) send(buildStripMixer(strip, proj_.tracks[strip]));
}
void App::syncNotes(int i) {
  if (i < 0 || i >= (int)proj_.tracks.size()) return;
  send(buildTrackNotes(i, proj_.tracks[i]));
  send(buildTrackClips(i, proj_.tracks[i]));
}
void App::syncTrack(int i) {
  if (i < 0 || i >= (int)proj_.tracks.size()) return;
  const Track& t = proj_.tracks[i];
  send(buildTrackInst(i, t));
  send(buildTrackDevices(i, t, 16));
  send(buildStripMixer(i, t));
  send(buildTrackNotes(i, t));
  send(buildTrackClips(i, t));
}
void App::syncReturn(int i) {
  if (i < 0 || i >= (int)proj_.returns.size()) return;
  int st = stripOfReturn(i);
  send(buildTrackDevices(st, proj_.returns[i], 16));
  send(buildStripMixer(st, proj_.returns[i]));
}
void App::syncAll() {
  ensureSetup();
  sendRaw(buildTransport(proj_, false));
  sendRaw(buildSig(proj_));
  sendRaw(buildLoop(proj_));
  sendRaw(std::string("setMetro ") + (proj_.metro ? "1\n" : "0\n"));
  sendRaw(buildSyncAll(proj_));
}

// ------------------------------------------------------------------ undo
void App::checkpoint() {
  undo_.push_back(serializeProject(proj_));
  if (undo_.size() > 60) undo_.erase(undo_.begin());
  redo_.clear();
  dirty_ = true;
}
void App::applyLoaded(Project p, const std::string& what) {
  int oldT = (int)proj_.tracks.size(), oldR = (int)proj_.returns.size();
  sendRaw(buildResetStrips(std::max(oldT, (int)p.tracks.size()), std::max(oldR, (int)p.returns.size())));
  proj_ = p;
  selTarget_ = 0;
  selTrack_ = proj_.tracks.empty() ? 0 : std::min(selTrack_, (int)proj_.tracks.size() - 1);
  selClip_ = -1;
  selNote_ = -1;
  setupDone_ = false;
  ensureSetup();
  setStatusLine(what + ": " + proj_.name);
}
void App::undo() {
  if (undo_.empty()) { setStatusLine("NOTHING TO UNDO"); return; }
  redo_.push_back(serializeProject(proj_));
  std::string s = undo_.back();
  undo_.pop_back();
  Project p;
  std::string err;
  if (parseProject(s, &p, &err)) applyLoaded(p, "UNDO");
}
void App::redo() {
  if (redo_.empty()) { setStatusLine("NOTHING TO REDO"); return; }
  undo_.push_back(serializeProject(proj_));
  std::string s = redo_.back();
  redo_.pop_back();
  Project p;
  std::string err;
  if (parseProject(s, &p, &err)) applyLoaded(p, "REDO");
}

// ------------------------------------------------------------------ helpers
double App::snapTicks() const {
  double beat = barTicks() / (proj_.sigNum > 0 ? proj_.sigNum : 4);
  switch (snapMode_) {
    case 0: return 1;
    case 1: return barTicks();
    case 2: return beat;
    case 3: return beat / 2;
    case 4: return beat / 4;
    case 5: return beat / 8;
    default: return beat;
  }
}
const Hit* App::hitAt(float x, float y) const {
  for (size_t i = hits_.size(); i-- > 0;)
    if (hits_[i].r.contains(x, y)) return &hits_[i];
  return nullptr;
}
Track* App::selTrackPtr() {
  if (selTarget_ == 1) {
    if (selTrack_ >= 0 && selTrack_ < (int)proj_.returns.size()) return &proj_.returns[selTrack_];
    return nullptr;
  }
  if (selTarget_ == 0 && selTrack_ >= 0 && selTrack_ < (int)proj_.tracks.size())
    return &proj_.tracks[selTrack_];
  return nullptr;
}
int App::selStrip() const {
  if (selTarget_ == 2) return kMasterStrip;
  if (selTarget_ == 1) return stripOfReturn(selTrack_);
  return selTrack_;
}
Clip* App::selClipPtr() {
  Track* t = selTrackPtr();
  if (!t || selClip_ < 0 || selClip_ >= (int)t->clips.size()) return nullptr;
  return &t->clips[selClip_];
}

// ------------------------------------------------------------------ files
bool App::takeFileOp(FileOp* out) {
  if (fileOps_.empty()) return false;
  *out = fileOps_.front();
  fileOps_.erase(fileOps_.begin());
  return true;
}
void App::saveToSlot(const std::string& name, bool silent) {
  std::string data = serializeProject(proj_);
  bool found = false;
  for (auto& s : memSlots_)
    if (s.first == name) { s.second = data; found = true; }
  if (!found) memSlots_.push_back({name, data});
  FileOp op;
  op.kind = 0; op.name = name; op.data = data; op.silent = silent;
  fileOps_.push_back(op);
  dirty_ = false;
  if (!silent) setStatusLine("SAVED: " + name);
}
void App::loadByName(const std::string& name, bool silent) {
  for (auto& s : memSlots_)
    if (s.first == name) {
      Project p;
      std::string err;
      if (parseProject(s.second, &p, &err)) { checkpoint(); applyLoaded(p, "LOADED"); }
      else setStatusLine("LOAD FAILED: " + err);
      return;
    }
  FileOp op;
  op.kind = 1; op.name = name; op.silent = silent;
  fileOps_.push_back(op);
}
void App::deliverLoad(const std::string& name, bool ok, const std::string& data, bool silent) {
  if (!ok) {
    if (!silent) setStatusLine("LOAD FAILED: " + name);
    return;
  }
  bool found = false;
  for (auto& s : memSlots_)
    if (s.first == name) { s.second = data; found = true; }
  if (!found) memSlots_.push_back({name, data});
  if (silent && dirty_) return;  // autosave restore only into a fresh session
  Project p;
  std::string err;
  if (parseProject(data, &p, &err)) { applyLoaded(p, "LOADED"); }
  else if (!silent) setStatusLine("LOAD FAILED: " + err);
}
void App::deliverFileList(const std::vector<std::string>& names) { fileList_ = names; }
void App::requestAutoLoad() {
  if (autoLoadAsked_) return;
  autoLoadAsked_ = true;
  FileOp op;
  op.kind = 1; op.name = "autosave"; op.silent = true;
  fileOps_.push_back(op);
  FileOp ls;
  ls.kind = 2;
  fileOps_.push_back(ls);
}

// ------------------------------------------------------------------ draw
void App::draw(std::vector<DrawCmd>* o) {
  o->clear();
  hits_.clear();
  frame_++;
  // FPS (cheap: frames between draws, smoothed)
  static auto lastT = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  double dt = std::chrono::duration<double>(now - lastT).count();
  lastT = now;
  if (dt > 0) fps_ = fps_ * 0.9f + (float)(1.0 / dt) * 0.1f;
  posAtDraw_ = snap_.posTicks;
  ui::rect(o, {0, 0, kDesignW, kDesignH}, ui::BG);
  drawTopbar(o);
  bodyR_ = {0, 56, kDesignW, kDesignH - 56};
  if (view_ == View::Mixer) {
    arrR_ = bodyR_;
    detailR_ = {0, 0, 0, 0};
    drawMixer(o);
  } else {
    float dh = detailOpen_ ? (detailMax_ ? 452.0f : 268.0f) : 30.0f;
    arrR_ = {0, 56, kDesignW, kDesignH - 56 - dh};
    detailR_ = {0, kDesignH - dh, kDesignW, dh};
    drawArrange(o);
    drawDetail(o);
  }
  if (browserOpen_) drawBrowser(o);
  if (modal_ != Modal::None) drawModal(o);
  // debounced autosave to the shell (and the in-memory slot)
  if (dirty_ && frame_ - autoSaveAt_ > 900) {
    autoSaveAt_ = (int)frame_;
    saveToSlot("autosave", true);
  }
}

// ------------------------------------------------------------------ touch
void App::onTouch(float lx, float ly, int action) { onTouchP(lx, ly, action, 0); }

void App::onTouchP(float lx, float ly, int action, int pid) {
  // Keys/pads area is multi-touch and bypasses the hit list - but never
  // while a modal or the browser is up (their controls live in that area).
  if (modal_ == Modal::None && !browserOpen_ && tab_ == DetailTab::Keys && detailOpen_ && view_ == View::Arrange &&
      keysArea_.contains(lx, ly)) {
    keysPointer(pid, lx, ly, action);
    return;
  }
  if (pid != 0) return;
  if (action == 2) {  // move
    if (drag_.what != kDragNone) dragMove(lx, ly);
    return;
  }
  if (action == 1) {  // up
    if (previewPitch_ >= 0 && selTarget_ == 0) {
      send("preview " + fmtNum(selTrack_) + " " + fmtNum(previewPitch_) + " 0 0\n");
      previewPitch_ = -1;
    }
    if (drag_.what != kDragNone) { dragEnd(lx, ly); drag_.what = kDragNone; }
    return;
  }
  if (action != 0) return;
  const Hit* h = hitAt(lx, ly);
  if (!h) return;
  Hit hit = *h;
  if (modal_ != Modal::None && hit.id < 9000) return;  // modal captures all
  int range = hit.id / 1000;
  bool ok = false;
  switch (range) {
    case 1: ok = touchTopbar(hit, action, lx, ly); break;
    case 2: ok = touchArrange(hit, action, lx, ly); break;
    case 3: ok = touchEditor(hit, action, lx, ly); break;
    case 4: ok = touchMixer(hit, action, lx, ly); break;
    case 5: ok = touchDevice(hit, action, lx, ly); break;
    case 6: ok = touchKeys(hit, action, lx, ly); break;
    case 7: ok = touchBrowser(hit, action, lx, ly); break;
    case 8: ok = touchDetail(hit, action, lx, ly); break;
    case 9: ok = touchModal(hit, action, lx, ly); break;
    default: break;
  }
  (void)ok;
}

// dragMove / dragEnd: the big switch over DragWhat. Modules configure drag_
// on touch-down (checkpoint already taken by the module where relevant).
void App::dragMove(float x, float y) {
  drag_.moved = true;
  float dy = drag_.y0 - y;  // up = positive
  switch (drag_.what) {
    case kDragBpm: {
      double nb = clampd(drag_.v0 + dy * 0.1, 40, 240);
      if (std::fabs(nb - proj_.bpm) > 0.001) {
        proj_.bpm = nb;
        send("tempo " + fmtNum(nb) + "\n");
      }
      break;
    }
    case kDragKnob: {
      double nv = clampd(drag_.v0 + dy / 160.0, 0, 1);
      applyKnob(drag_.a, drag_.b, drag_.c, nv, false);
      break;
    }
    case kDragFader: {
      // relative drag (web mixer): full track height = full range
      double range = 1.25;
      double nv = clampd(drag_.t0 + (drag_.y0 - y) / (drag_.w0 > 0 ? drag_.w0 : 300) * range, 0, range);
      applyFader(drag_.a, nv, false);
      break;
    }
    case kDragClip: {
      Track* t = drag_.a >= 0 && drag_.a < (int)proj_.tracks.size() ? &proj_.tracks[drag_.a] : nullptr;
      if (!t || drag_.b < 0 || drag_.b >= (int)t->clips.size()) break;
      Clip& c = t->clips[drag_.b];
      double dt = (x - drag_.x0) / arrPpt_;
      double snap = snapTicks();
      if (dragClipMode_ == 0) {
        double ns = drag_.t0 + dt;
        if (snap > 1) ns = std::round(ns / snap) * snap;
        c.start = std::max(0.0, ns);
      } else if (dragClipMode_ == 1) {
        double ns = drag_.t0 + dt;
        if (snap > 1) ns = std::round(ns / snap) * snap;
        ns = clampd(ns, 0, drag_.t0 + drag_.v0 - snap > 0 ? drag_.t0 + drag_.v0 - 60 : drag_.t0);
        double delta = ns - drag_.t0;
        c.start = ns;
        c.len = drag_.v0 - delta;
        if (t->kind != kAudio) c.offset = drag_.v1 + delta;
      } else {
        double nl = drag_.v0 + dt;
        if (snap > 1) nl = std::round(nl / snap) * snap;
        c.len = std::max(60.0, nl);
      }
      break;
    }
    case kDragNote: {
      Clip* c = selClipPtr();
      if (!c || drag_.c < 0 || drag_.c >= (int)c->notes.size()) break;
      Note& n = c->notes[drag_.c];
      if (dragClipMode_ == 1) {  // resize right edge
        double nl = drag_.v0 + (x - drag_.x0) / edPpt_;
        double snap = snapTicks();
        if (snap > 1) nl = std::round(nl / snap) * snap;
        n.len = clampd(nl, 30, c->patternLen);
      } else {
        double ns = drag_.t0 + (x - drag_.x0) / edPpt_;
        double snap = snapTicks();
        if (snap > 1) ns = std::round(ns / snap) * snap;
        n.start = clampd(ns, 0, c->patternLen - 30);
        int dp = (int)std::round((drag_.y0 - y) / edRowH_);
        n.pitch = (int)clampd(drag_.v1 + dp, 0, 127);
      }
      selNote_ = drag_.c;
      break;
    }
    case kDragVel: {
      Clip* c = selClipPtr();
      if (!c || drag_.c < 0 || drag_.c >= (int)c->notes.size()) break;
      float v = 1.0f - (y - (float)drag_.v0) / (float)(drag_.w0 > 0 ? drag_.w0 : 60);
      c->notes[drag_.c].vel = (float)clampd(v, 0.05, 1);
      break;
    }
    case kDragScrollArr: {
      arrTick0_ = std::max(0.0, drag_.t0 - (x - drag_.x0) / arrPpt_);
      arrY0_ = (float)std::max(0.0, drag_.v0 - (y - drag_.y0));
      break;
    }
    case kDragScrollEd: {
      edTick0_ = std::max(0.0, drag_.t0 - (x - drag_.x0) / edPpt_);
      edTopPitch_ = (int)clampd(drag_.v0 + std::round((y - drag_.y0) / edRowH_), 24, 127);
      break;
    }
    case kDragScrollMix: mixX0_ = (float)std::max(0.0, drag_.v0 - (x - drag_.x0)); break;
    case kDragScrollBro: broScroll_[drag_.a] = (float)std::max(0.0, drag_.v0 - (y - drag_.y0)); break;
    case kDragScrollMenu: menuScroll_ = (float)std::max(0.0, drag_.v0 - (y - drag_.y0)); break;
    case kDragScrollDev: devScroll_ = (float)std::max(0.0, drag_.v0 - (y - drag_.y0)); break;
    case kDragHeadVol: {  // absolute mini-slider in a track header (a=strip)
      float frac = (x - (float)drag_.v0) / (float)(drag_.w0 > 0 ? drag_.w0 : 100);
      applyFader(drag_.a, clampd(frac, 0, 1) * 1.25, false);
      break;
    }
    case kDragHeadPan: {  // a=strip; value -1..1 from x
      float frac = (x - (float)drag_.v0) / (float)(drag_.w0 > 0 ? drag_.w0 : 100);
      applyKnob(4, drag_.a, 0, clampd(frac, 0, 1), false);
      break;
    }
    case kDragPaint: editorPaint(x, y); break;
    case kDragEnvPt: {
      if (envLane_ < 0 || envLane_ >= (int)proj_.autoLanes.size()) break;
      AutoLane& l = proj_.autoLanes[envLane_];
      if (drag_.b < 0 || drag_.b >= (int)l.pts.size()) break;
      double nt = drag_.t0 + (x - drag_.x0) / edPpt_;
      double lo = drag_.b > 0 ? l.pts[drag_.b - 1].first + 1 : 0;
      double hi = drag_.b + 1 < (int)l.pts.size() ? l.pts[drag_.b + 1].first - 1 : 1e9;
      l.pts[drag_.b].first = clampd(nt, lo, hi);
      double vlo, vhi;
      envRange(l, &vlo, &vhi);
      float frac = 1.0f - (y - (float)drag_.v0) / (float)(drag_.w0 > 0 ? drag_.w0 : 200);
      l.pts[drag_.b].second = vlo + clampd(frac, 0, 1) * (vhi - vlo);
      envSelPt_ = drag_.b;
      break;
    }
    case kDragWheel: {
      float v = (float)clampd(drag_.v0 + dy / 120.0, -1, 1);
      if (drag_.a == 0) {
        bendV_ = v;
        Track* t = selTrackPtr();
        if (t && selTarget_ == 0) send("bend " + fmtNum(selTrack_) + " " + fmtNum(v) + "\n");
      } else {
        modV_ = (v + 1) / 2;
        if (selTarget_ == 0) send("modWheel " + fmtNum(selTrack_) + " " + fmtNum(modV_) + "\n");
      }
      break;
    }
    case kDragSeek: {
      double tick = arrTick0_ + (x - headW_) / arrPpt_;
      if (tick < 0) tick = 0;
      send("seek " + fmtNum(tick) + "\n");
      break;
    }
    case kDragHeadW: headW_ = (float)clampd(drag_.v0 + (x - drag_.x0), 120, 460); break;
    case kDragSlider: applySlider(drag_.a, clampd((x - (float)drag_.v0) / (drag_.w0 > 0 ? drag_.w0 : 200), 0, 1), false); break;
    default: break;
  }
}

void App::dragEnd(float x, float y) {
  (void)x; (void)y;
  switch (drag_.what) {
    case kDragKnob: applyKnob(drag_.a, drag_.b, drag_.c, 0, true); break;
    case kDragFader: applyFader(drag_.a, 0, true); break;
    case kDragClip: syncNotes(drag_.a); dirty_ = true; break;
    case kDragNote: case kDragVel: syncNotes(selTrack_); dirty_ = true; break;
    case kDragEnvPt: send(buildAutomation(proj_)); dirty_ = true; break;
    case kDragScrollEd:
      if (drag_.c == 1 && !drag_.moved) editorTapAdd(drag_.x0, drag_.y0);
      break;
    case kDragScrollArr:
      if (drag_.c == 1 && !drag_.moved) {  // plain tap on a lane/header = select
        int r = arrRowAt(drag_.y0);
        if (r >= 0) {
          if (r >= (int)proj_.tracks.size()) { selTarget_ = 1; selTrack_ = r - (int)proj_.tracks.size(); }
          else { selTarget_ = 0; selTrack_ = r; }
          selClip_ = -1;
        }
      }
      break;
    case kDragWheel:
      if (drag_.a == 0) {
        bendV_ = 0;
        if (selTarget_ == 0) send("bend " + fmtNum(selTrack_) + " 0\n");
      }
      break;
    case kDragSlider: applySlider(drag_.a, -1, true); break;
    case kDragBpm:
      dirty_ = true;
      if (!drag_.moved) modal_ = Modal::Tempo;  // plain tap on BPM = tempo window (web)
      break;
    default: break;
  }
}

// ------------------------------------------------------------------ appliers
// Knob kinds: 0 instParam (b=param), 1 devParam (b=dev, c=param),
// 2 devMix (b=dev), 3 devGain dB (b=dev), 4 stripPan (b=strip),
// 5 stripWidth (b=strip), 6 stripSend (b=strip, c=send idx).
// drag_.v0 holds the START value in parameter units; `norm01` is the new
// normalized position. For inst/dev params the conversion uses the def's
// curve (ui::valOf). Pan/width/send are linear in their own ranges.
void App::applyKnob(int kind, int b, int c, double norm01, bool final) {
  if (final) { dirty_ = true; return; }
  Track* t = selTrackPtr();
  int strip = selStrip();
  switch (kind) {
    case 0: {
      if (!t) return;
      ensureParams(t);
      const defs::UnitDef* d = instDef(t->instType);
      if (!d || b < 0 || b >= (int)d->params.size() || b >= (int)t->instParams.size()) return;
      double v = ui::valOf(d->params[b], norm01);
      t->instParams[b] = v;
      if (selTarget_ == 0) send("instParam " + fmtNum(selTrack_) + " " + fmtNum(b) + " " + fmtNum(v) + "\n");
      break;
    }
    case 1: {
      if (!t || b < 0 || b >= (int)t->devices.size()) return;
      FxDevice& dv = t->devices[b];
      ensureParams(&dv);
      const defs::UnitDef* d = devDef(dv.type);
      if (!d || c < 0 || c >= (int)d->params.size() || c >= (int)dv.params.size()) return;
      double v = ui::valOf(d->params[c], norm01);
      dv.params[c] = v;
      send("devParam " + fmtNum(strip) + " " + fmtNum(b) + " " + fmtNum(c) + " " + fmtNum(v) + "\n");
      break;
    }
    case 2: {
      if (!t || b < 0 || b >= (int)t->devices.size()) return;
      t->devices[b].mix = norm01;
      send("devParam " + fmtNum(strip) + " " + fmtNum(b) + " 1000 " + fmtNum(norm01) + "\n");
      break;
    }
    case 3: {
      if (!t || b < 0 || b >= (int)t->devices.size()) return;
      double db = -36 + norm01 * 48;
      t->devices[b].gainDb = db;
      send("devParam " + fmtNum(strip) + " " + fmtNum(b) + " 1001 " + fmtNum(std::pow(10.0, db / 20.0)) + "\n");
      break;
    }
    case 4: case 5: case 6: {
      Track* st = nullptr;
      if (b == kMasterStrip) return;
      if (b >= 65) { int i = b - 65; if (i < (int)proj_.returns.size()) st = &proj_.returns[i]; }
      else if (b >= 0 && b < (int)proj_.tracks.size()) st = &proj_.tracks[b];
      if (!st) return;
      if (kind == 4) { st->pan = -1 + norm01 * 2; send("trackPan " + fmtNum(b) + " " + fmtNum(st->pan) + "\n"); }
      if (kind == 5) { st->width = norm01 * 2; send("trackWidth " + fmtNum(b) + " " + fmtNum(st->width) + "\n"); }
      if (kind == 6) { st->send[c] = norm01 * 1.5; send("trackSend " + fmtNum(b) + " " + fmtNum(c) + " " + fmtNum(st->send[c]) + "\n"); }
      break;
    }
    default: break;
  }
}

void App::applyFader(int strip, double vol, bool final) {
  if (final) { dirty_ = true; return; }
  if (strip == kMasterStrip) {
    proj_.masterVol = vol;
    send("trackVol 64 " + fmtNum(vol) + "\n");
    return;
  }
  Track* t = nullptr;
  if (strip >= 65) { int i = strip - 65; if (i < (int)proj_.returns.size()) t = &proj_.returns[i]; }
  else if (strip >= 0 && strip < (int)proj_.tracks.size()) t = &proj_.tracks[strip];
  if (!t) return;
  t->vol = vol;
  send("trackVol " + fmtNum(strip) + " " + fmtNum(vol) + "\n");
}

// Generic sliders (settings + editor velocity default etc.):
// 1 = metronome volume, 2 = master volume (as fader), 3 = arrange row height.
void App::applySlider(int id, double norm01, bool final) {
  if (norm01 < 0) {  // finalize-only call from dragEnd (value already applied)
    if (final && id >= 10 && id <= 13) syncNotes(selTrack_);
    if (final) dirty_ = true;
    return;
  }
  switch (id) {
    case 1: proj_.metroVol = norm01; send("setMetroVol " + fmtNum(norm01) + "\n"); break;
    case 2: applyFader(kMasterStrip, norm01 * 1.25, false); break;
    case 3: rowH_ = 44 + (float)norm01 * 56; break;
    // audio clip editor sliders (10 offset, 11 gain, 12 fadeIn, 13 fadeOut)
    case 10: case 11: case 12: case 13: {
      Clip* c = selClipPtr();
      if (!c) break;
      if (id == 10) c->offset = norm01 * 8;
      if (id == 11) c->gain = norm01 * 2;
      if (id == 12) c->fadeIn = norm01 * 1920;
      if (id == 13) c->fadeOut = norm01 * 1920;
      if (final) syncNotes(selTrack_);
      break;
    }
    default: break;
  }
}

void App::envRange(const AutoLane& l, double* lo, double* hi) const {
  switch (l.kind) {
    case 0: *lo = 0; *hi = 1.5; break;
    case 1: *lo = -1; *hi = 1; break;
    case 2: case 3: *lo = 0; *hi = 1.5; break;
    case 7: *lo = 0; *hi = 2; break;
    case 4: {
      const Track* t = (l.track >= 65) ? (l.track - 65 < (int)proj_.returns.size() ? &proj_.returns[l.track - 65] : nullptr)
                     : (l.track >= 0 && l.track < (int)proj_.tracks.size() ? &proj_.tracks[l.track] : nullptr);
      const defs::UnitDef* d = nullptr;
      if (t && l.dev >= 0 && l.dev < (int)t->devices.size()) d = devDef(t->devices[l.dev].type);
      if (d && l.param >= 0 && l.param < (int)d->params.size()) { *lo = d->params[l.param].min; *hi = d->params[l.param].max; }
      else { *lo = 0; *hi = 1; }
      break;
    }
    case 5: {
      const Track* t = (l.track >= 0 && l.track < (int)proj_.tracks.size()) ? &proj_.tracks[l.track] : nullptr;
      const defs::UnitDef* d = t ? instDef(t->instType) : nullptr;
      if (d && l.param >= 0 && l.param < (int)d->params.size()) { *lo = d->params[l.param].min; *hi = d->params[l.param].max; }
      else { *lo = 0; *hi = 1; }
      break;
    }
    default: *lo = 0; *hi = 1; break;
  }
}

}  // namespace mdnative
