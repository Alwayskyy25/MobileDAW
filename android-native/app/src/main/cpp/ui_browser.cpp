// ui_browser.cpp - the Browser drawer (web map section 4) with its five
// tabs (Instruments / Audio FX / Samples / Presets / Projects) and the
// Settings modal (web map section 6, the subset that maps to real engine
// commands + local UI prefs).
#include "ui_core.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include "ui_widgets.h"

namespace mdnative {
namespace {
std::string is(int v) { return std::to_string(v); }
bool match(const std::string& hay, const std::string& needle) {
  if (needle.empty()) return true;
  std::string h = hay, n = needle;
  for (char& c : h) c = (char)std::tolower((unsigned char)c);
  for (char& c : n) c = (char)std::tolower((unsigned char)c);
  return h.find(n) != std::string::npos;
}
}  // namespace

void App::drawBrowser(std::vector<DrawCmd>* o) {
  using namespace ui;
  Rect B = {0, 56, 452, kDesignH - 56};
  rect(o, B, PANEL);
  rect(o, {B.x + B.w - 2, B.y, 2, B.h}, LINE);
  auto add = [&](Rect r, int id, int a = 0, int b = 0, double v = 0) { hits_.push_back({r, id, a, b, v}); };
  const char* tn[5] = {"INST", "FX", "SAMP", "PRE", "PROJ"};
  for (int i = 0; i < 5; i++) {
    Rect r = {B.x + 6 + i * 72.0f, B.y + 6, 68, 30};
    buttonS(o, r, tn[i], browserTab_ == i);
    add(r, 7001 + i);
  }
  Rect pin = {B.x + B.w - 84, B.y + 6, 36, 30}, cls = {B.x + B.w - 44, B.y + 6, 36, 30};
  buttonS(o, pin, "PIN", browserPin_);
  buttonS(o, cls, "X", false);
  add(pin, 7007);
  add(cls, 7008);
  Rect sf = {B.x + 6, B.y + 42, B.w - 12, 30};
  rect(o, sf, CELLOFF);
  textClip(o, sf.x + 8, sf.y + 7, 2.2f, broSearch_.empty() ? FAINT : TXT,
           broSearch_.empty() ? "SEARCH..." : broSearch_, sf.w - 16);
  add(sf, 7006);
  Rect list = {B.x, B.y + 78, B.w, B.h - 78};
  add(list, 7000, browserTab_);
  float y = list.y + 4 - broScroll_[browserTab_];
  const float rowH = 40;
  auto row = [&](const std::string& label, int id, int a, bool dim = false, bool checked = false) {
    if (y + rowH > list.y && y < list.y + list.h) {
      Rect r = {B.x + 6, y, B.w - 12, rowH - 5};
      rect(o, r, PANEL2);
      if (checked) rect(o, {r.x + 4, r.y + 6, 8, r.h - 12}, ACC2);
      textClip(o, r.x + 20, r.y + 9, 2.3f, dim ? FAINT : TXT, label, r.w - 60);
      if (id >= 0) add(r, id, a);
    }
    y += rowH;
  };
  auto header = [&](const std::string& label) {
    if (y + 24 > list.y && y < list.y + list.h) text(o, B.x + 10, y + 4, 2, ACC, label);
    y += 26;
  };
  Track* t = selTrackPtr();
  switch (browserTab_) {
    case 0: {  // Instruments
      for (int ty : defs::kInstList) {
        const defs::UnitDef* d = instDef(ty);
        if (!d || !match(d->name, broSearch_)) continue;
        row(d->name, 7010, ty, false, t && selTarget_ == 0 && t->instType == ty);
        // small "+" (new track) hit on the right edge of the row
        if (y - rowH + 4 > list.y && y - rowH < list.y + list.h) {
          Rect pr = {B.x + B.w - 52, y - rowH + 2, 40, 30};
          buttonS(o, pr, "+", false);
          add(pr, 7011, ty);
        }
      }
      break;
    }
    case 1: {  // FX groups
      int gi = 0;
      for (const defs::FxGroup& g : defs::kFxGroups) {
        bool any = false;
        for (int ty : g.types) {
          const defs::UnitDef* d = devDef(ty);
          if (d && match(d->name, broSearch_)) any = true;
        }
        if (any) {
          row(std::string(fxGroup_ == gi ? "v " : "> ") + g.name, 7012, gi);
          if (fxGroup_ == gi)
            for (int ty : g.types) {
              const defs::UnitDef* d = devDef(ty);
              if (!d || !match(d->name, broSearch_)) continue;
              row("    " + std::string(d->name), 7013, ty);
            }
        }
        gi++;
      }
      break;
    }
    case 2: {  // Samples / SoundFonts (names only)
      header("SAMPLES (NAMES ONLY - NO AUDIO DATA)");
      for (int i = 0; i < defs::kSampleFileCount; i++)
        if (match(defs::kSampleFiles[i], broSearch_)) row(defs::kSampleFiles[i], 7014, i);
      header("SOUNDFONTS (CANNOT LOAD - NO DATA CHANNEL)");
      for (int i = 0; i < defs::kSoundFontFileCount; i++) row(defs::kSoundFontFiles[i], -1, 0, true);
      break;
    }
    case 3: {  // Presets for the selected track's instrument (or all)
      int want = (t && selTarget_ == 0) ? t->instType : -1;
      int n = (int)(sizeof(defs::kPresets) / sizeof(defs::PresetDef));
      int lastType = -999;
      for (int i = 0; i < n; i++) {
        const defs::PresetDef& p = defs::kPresets[i];
        if (want >= 0 && p.type != want) continue;
        if (!match(p.name, broSearch_)) continue;
        if (want < 0 && p.type != lastType) {
          const defs::UnitDef* d = instDef(p.type);
          header(d ? d->name : "TYPE " + is(p.type));
          lastType = p.type;
        }
        row((p.cat[0] ? std::string(p.cat) + ": " : "") + p.name, 7015, i);
      }
      break;
    }
    case 4: {  // Projects
      row("NEW PROJECT", 7020, 0);
      row("QUICK SAVE", 7021, 0);
      row("SAVE AS...", 7022, 0);
      header("SAVED (THIS SESSION + DEVICE FILES)");
      for (size_t i = 0; i < memSlots_.size(); i++) row(memSlots_[i].first, 7023, (int)i);
      for (size_t i = 0; i < fileList_.size(); i++) {
        bool dup = false;
        for (auto& s : memSlots_) if (s.first == fileList_[i]) dup = true;
        if (!dup) row(fileList_[i], 7024, (int)i);
      }
      row("RESTORE AUTOSAVE", 7026, 0);
      header("DEMOS (WEB FORMAT - CANNOT LOAD)");
      for (int i = 0; i < defs::kDemoProjectCount; i++) row(defs::kDemoProjects[i], 7025, i, true);
      break;
    }
  }
  float maxScroll = std::max(0.0f, (y + broScroll_[browserTab_]) - (list.y + list.h) + 10);
  if (broScroll_[browserTab_] > maxScroll) broScroll_[browserTab_] = maxScroll;
}

bool App::touchBrowser(const Hit& h, int, float x, float y) {
  Track* t = selTrackPtr();
  switch (h.id) {
    case 7000:
      drag_ = Drag{};
      drag_.what = kDragScrollBro;
      drag_.a = h.a; drag_.x0 = x; drag_.y0 = y; drag_.v0 = broScroll_[h.a];
      return true;
    case 7001: case 7002: case 7003: case 7004: case 7005:
      browserTab_ = h.id - 7001;
      return true;
    case 7006: openKeyboard(kKbSearch, broSearch_, "SEARCH"); return true;
    case 7007: browserPin_ = !browserPin_; return true;
    case 7008: browserOpen_ = false; return true;
    case 7010: {  // instrument -> selected track
      if (!t || selTarget_ != 0) { setStatusLine("SELECT A TRACK FIRST"); return true; }
      checkpoint();
      t->instType = h.a;
      t->instParams = defaultParams(instDef(h.a));
      send(buildTrackInst(selTrack_, *t));
      setStatusLine("INSTRUMENT: " + std::string(instDef(h.a) ? instDef(h.a)->name : "?"));
      if (!browserPin_) browserOpen_ = false;
      return true;
    }
    case 7011: {  // instrument -> new track
      checkpoint();
      Track nt;
      nt.kind = h.a == 3 ? kDrum : kMidi;
      nt.name = instDef(h.a) ? instDef(h.a)->name : "Track";
      nt.instType = h.a;
      ensureParams(&nt);
      static const uint32_t kCols[6] = {0xFF8C2EFF, 0x3ECF6EFF, 0xFFD94DFF, 0xFF6E9EFF, 0xFFFF6EFF, 0x6EFFD9FF};
      nt.color = kCols[proj_.tracks.size() % 6];
      proj_.tracks.push_back(nt);
      selTarget_ = 0;
      selTrack_ = (int)proj_.tracks.size() - 1;
      selClip_ = -1;
      syncTrack(selTrack_);
      if (!browserPin_) browserOpen_ = false;
      return true;
    }
    case 7012: fxGroup_ = fxGroup_ == h.a ? -1 : h.a; return true;
    case 7013: {  // FX -> end of selected chain
      if (!t || selTarget_ == 2) { setStatusLine("SELECT A TRACK/RETURN FIRST"); return true; }
      checkpoint();
      FxDevice dv;
      dv.type = h.a;
      ensureParams(&dv);
      int pos = (int)t->devices.size();
      t->devices.push_back(dv);
      send(buildTrackDevices(selStrip(), *t, pos));
      setStatusLine("FX ADDED: " + std::string(devDef(h.a) ? devDef(h.a)->name : "?"));
      if (!browserPin_) browserOpen_ = false;
      tab_ = DetailTab::Device;
      detailOpen_ = true;
      return true;
    }
    case 7014: {  // sample -> audio clip on the selected audio track
      if (!t || selTarget_ != 0 || t->kind != kAudio) {
        setStatusLine("SAMPLES GO ON AUDIO TRACKS (NAMES ONLY - SILENT)");
        return true;
      }
      checkpoint();
      Clip* c = selClipPtr();
      if (c) {
        c->sampleId = h.a;
        c->sampleName = defs::kSampleFiles[h.a];
      } else {
        Clip nc;
        nc.start = std::floor(snap_.posTicks / barTicks()) * barTicks();
        nc.len = barTicks();
        nc.patternLen = barTicks();
        nc.sampleId = h.a;
        nc.sampleName = defs::kSampleFiles[h.a];
        nc.name = nc.sampleName;
        t->clips.push_back(nc);
        selClip_ = (int)t->clips.size() - 1;
      }
      syncNotes(selTrack_);
      if (!browserPin_) browserOpen_ = false;
      return true;
    }
    case 7015: {  // preset -> selected track (if the type matches)
      const defs::PresetDef& p = defs::kPresets[h.a];
      if (!t || selTarget_ != 0 || t->instType != p.type) {
        setStatusLine("PRESET IS FOR ANOTHER INSTRUMENT");
        return true;
      }
      checkpoint();
      applyPresetToTrack(t, p);
      send(buildTrackInst(selTrack_, *t));
      setStatusLine("PRESET: " + std::string(p.name));
      if (!browserPin_) browserOpen_ = false;
      return true;
    }
    case 7020: menuAction(kMenuProject, 1); return true;
    case 7021: saveToSlot(proj_.name, false); return true;
    case 7022: openKeyboard(kKbSaveAs, proj_.name, "SAVE AS"); return true;
    case 7023:
      if (h.a >= 0 && h.a < (int)memSlots_.size()) { loadByName(memSlots_[h.a].first, false); browserOpen_ = false; }
      return true;
    case 7024:
      if (h.a >= 0 && h.a < (int)fileList_.size()) { loadByName(fileList_[h.a], false); browserOpen_ = false; }
      return true;
    case 7025: setStatusLine("DEMO IS WEB .MDAW JSON - NOT LOADABLE IN NATIVE BUILD"); return true;
    case 7026: loadByName("autosave", false); return true;
    default: return false;
  }
}

// Apply a factory preset: reset the instrument params to defaults, then
// apply the preset's sparse overrides (defs::PresetDef::vals "i=v;i=v").
void App::applyPresetToTrack(Track* t, const defs::PresetDef& p) {
  t->instParams = defaultParams(instDef(t->instType));
  std::string vals = p.vals;
  size_t at = 0;
  while (at < vals.size()) {
    size_t sc = vals.find(';', at);
    std::string kv = vals.substr(at, sc == std::string::npos ? sc : sc - at);
    size_t eq = kv.find('=');
    if (eq != std::string::npos) {
      int idx = std::atoi(kv.substr(0, eq).c_str());
      double v = std::atof(kv.substr(eq + 1).c_str());
      if (idx >= 0 && idx < (int)t->instParams.size()) t->instParams[idx] = v;
    }
    if (sc == std::string::npos) break;
    at = sc + 1;
  }
}

// ------------------------------------------------------------- settings
void App::drawSettings(std::vector<DrawCmd>* o) {
  using namespace ui;
  Rect box = {290, 110, 700, 500};
  rect(o, box, PANEL);
  frame(o, box, LINE);
  text(o, box.x + 16, box.y + 14, 3, TXT, "SETTINGS");
  Rect closeR = {box.x + box.w - 52, box.y + 8, 44, 36};
  buttonS(o, closeR, "X", false);
  hits_.push_back({closeR, 9001, 0, 0, 0});
  auto add = [&](Rect r, int id, int a = 0) { hits_.push_back({r, id, a, 0, 0}); };
  float y = box.y + 60;
  auto toggleRow = [&](const char* label, bool on, int id) {
    text(o, box.x + 16, y + 8, 2.4f, DIM, label);
    Rect r = {box.x + box.w - 150, y, 130, 38};
    buttonS(o, r, on ? "ON" : "OFF", on);
    add(r, id);
    y += 50;
  };
  toggleRow("MASTER LIMITER", proj_.masterLimit, 9301);
  toggleRow("PLUGIN DELAY COMP (PDC)", proj_.pdc, 9302);
  toggleRow("METRONOME", proj_.metro, 9308);
  toggleRow("FOLLOW PLAYHEAD", follow_, 9309);
  // sliders
  auto sliderRow = [&](const char* label, float v, int id) {
    text(o, box.x + 16, y + 8, 2.4f, DIM, label);
    Rect r = {box.x + box.w - 330, y + 6, 300, 24};
    hslider(o, r, v);
    add(r, id);
    y += 50;
  };
  sliderRow("METRO VOLUME", (float)proj_.metroVol, 9303);
  sliderRow("MASTER VOLUME", (float)(proj_.masterVol / 1.25), 9304);
  sliderRow("TRACK ROW HEIGHT", (rowH_ - 44) / 56, 9307);
  // count-in
  text(o, box.x + 16, y + 8, 2.4f, DIM, "COUNT-IN (BAR)");
  const int ci[4] = {0, 1, 2, 4};
  for (int i = 0; i < 4; i++) {
    Rect r = {box.x + box.w - 330 + i * 82.0f, y, 74, 38};
    buttonS(o, r, is(ci[i]), proj_.countIn == ci[i]);
    add(r, 9305, ci[i]);
  }
  y += 50;
  Rect lg = {box.x + 16, y, 150, 40}, hp = {box.x + 178, y, 150, 40};
  buttonS(o, lg, "LOG", false);
  buttonS(o, hp, "HELP", false);
  add(lg, 9310);
  add(hp, 9311);
  text(o, box.x + 16, y + 56, 2, FAINT, "ENGINE: MDAW C++ - SAMPLE RATE SET BY DEVICE (OBOE)");
  text(o, box.x + 16, y + 78, 2, FAINT, "SETTINGS NOT LISTED HERE HAVE NO ENGINE COMMAND");
  text(o, box.x + 16, y + 100, 2, FAINT, "OR NO UI STATE IN THIS BUILD (SEE HELP/LIMITS).");
}

bool App::touchSettings(const Hit& h, int, float x, float y) {
  switch (h.id) {
    case 9301: proj_.masterLimit = !proj_.masterLimit; send(std::string("masterLimit ") + (proj_.masterLimit ? "1\n" : "0\n")); return true;
    case 9302: proj_.pdc = !proj_.pdc; send(std::string("setPdc ") + (proj_.pdc ? "1\n" : "0\n")); return true;
    case 9308: proj_.metro = !proj_.metro; send(std::string("setMetro ") + (proj_.metro ? "1\n" : "0\n")); return true;
    case 9309: follow_ = !follow_; return true;
    case 9303: case 9304: case 9307: {
      int sid = h.id == 9303 ? 1 : h.id == 9304 ? 2 : 3;
      drag_ = Drag{};
      drag_.what = kDragSlider;
      drag_.a = sid; drag_.v0 = h.r.x; drag_.w0 = h.r.w; drag_.x0 = x; drag_.y0 = y;
      applySlider(sid, (x - h.r.x) / h.r.w, false);
      return true;
    }
    case 9305: proj_.countIn = h.a; dirty_ = true; return true;
    case 9310: modal_ = Modal::Log; return true;
    case 9311: modal_ = Modal::Help; return true;
    default: return false;
  }
}

}  // namespace mdnative
