// ui_device.cpp - Device tab of the detail panel (web map section 2b):
// instrument card (change instrument, presets, generic parameter knobs)
// and the FX chain (cards with bypass, params, wet mix/gain, add/remove/
// copy/paste/move). Master target shows master settings instead.
#include "ui_core.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "ui_widgets.h"

namespace mdnative {
namespace {
std::string is(int v) { return std::to_string(v); }
// Draw a knob grid for a param list; returns the y after the grid.
// hits: id with (a=param index); the caller maps the id to a knob kind.
float knobGrid(App* self, std::vector<DrawCmd>* o, std::vector<Hit>& hits, float x, float y, float w,
               const defs::UnitDef* def, const std::vector<double>& vals, int hitId, int hitB,
               float top, float bottom) {
  using namespace ui;
  if (!def) return y;
  const float kw = 92, kh = 78;
  int perRow = std::max(1, (int)(w / kw));
  for (size_t i = 0; i < def->params.size(); i++) {
    float kx = x + (i % perRow) * kw;
    float ky = y + (i / perRow) * kh;
    if (ky + kh < top || ky > bottom) continue;
    double v = i < vals.size() ? vals[i] : def->params[i].def;
    Rect kr = {kx + 8, ky, 62, 48};
    knob(o, kr, "", (float)ui::normOf(def->params[i], v));
    textClip(o, kx + 2, ky + 52, 1.8f, DIM, def->params[i].name, kw - 4);
    textClip(o, kx + 2, ky + 66, 1.8f, TXT, ui::fmtVal(def->params[i], v), kw - 4);
    hits.push_back({kr, hitId, (int)i, hitB, 0});
  }
  return y + ((def->params.size() + perRow - 1) / perRow) * kh;
}
}  // namespace

void App::drawDevice(std::vector<DrawCmd>* o) {
  using namespace ui;
  Rect C = detailContentR_;
  auto add = [&](Rect r, int id, int a = 0, int b = 0, double v = 0) { hits_.push_back({r, id, a, b, v}); };
  if (selTarget_ == 2) {  // Master settings card
    text(o, C.x + 16, C.y + 10, 2.6f, TXT, "MASTER");
    text(o, C.x + 16, C.y + 40, 2.2f, DIM, "VOLUME");
    Rect sl = {C.x + 130, C.y + 38, 360, 22};
    hslider(o, sl, (float)(proj_.masterVol / 1.25));
    add(sl, 5050);
    Rect l1 = {C.x + 16, C.y + 76, 200, 40}, l2 = {C.x + 228, C.y + 76, 200, 40};
    buttonS(o, l1, "LIMITER", proj_.masterLimit);
    buttonS(o, l2, "PDC", proj_.pdc);
    add(l1, 5051);
    add(l2, 5052);
    text(o, C.x + 16, C.y + 132, 2, FAINT, "MASTER FX CHAIN: NOT MODELLED IN THIS BUILD");
    return;
  }
  Track* t = selTrackPtr();
  if (!t) {
    text(o, C.x + 16, C.y + 16, 2.4f, DIM, "DEVICE: NO TRACK SELECTED");
    return;
  }
  add(C, 5000);  // scroll surface
  float y = C.y + 6 - devScroll_;
  float top = C.y, bottom = C.y + C.h;
  // ---- instrument card (tracks only; returns have no instrument)
  if (selTarget_ == 0) {
    const defs::UnitDef* id = instDef(t->instType);
    Rect hdr = {C.x + 8, y, C.w - 16, 34};
    if (y + 34 > top && y < bottom) {
      rect(o, hdr, PANEL3);
      textClip(o, hdr.x + 8, hdr.y + 8, 2.4f, TXT, "INST: " + std::string(id ? id->name : "?"), 460);
      Rect ch = {hdr.x + hdr.w - 300, hdr.y + 4, 140, 26}, pr = {hdr.x + hdr.w - 152, hdr.y + 4, 140, 26};
      buttonS(o, ch, "CHANGE", false);
      buttonS(o, pr, "PRESETS", false);
      add(ch, 5001);
      add(pr, 5002);
    }
    y += 40;
    ensureParams(t);
    y = knobGrid(this, o, hits_, C.x + 8, y, C.w - 16, id, t->instParams, 5010, 0, top, bottom);
    y += 6;
  }
  // ---- FX cards
  for (size_t di = 0; di < t->devices.size(); di++) {
    FxDevice& dv = t->devices[di];
    ensureParams(&dv);
    const defs::UnitDef* dd = devDef(dv.type);
    Rect hdr = {C.x + 8, y, C.w - 16, 34};
    if (y + 34 > top && y < bottom) {
      rect(o, hdr, dv.bypass ? CELLOFF : PANEL3);
      textClip(o, hdr.x + 8, hdr.y + 8, 2.4f, dv.bypass ? DIM : TXT,
               "FX" + is((int)di + 1) + ": " + (dd ? dd->name : "?"), 560);
      Rect by = {hdr.x + hdr.w - 220, hdr.y + 4, 100, 26}, mn = {hdr.x + hdr.w - 112, hdr.y + 4, 100, 26};
      buttonS(o, by, dv.bypass ? "BYP OFF" : "BYPASS", dv.bypass);
      buttonS(o, mn, "MENU", false);
      add(by, 5020, (int)di);
      add(mn, 5021, (int)di);
    }
    y += 40;
    y = knobGrid(this, o, hits_, C.x + 8, y, C.w - 16, dd, dv.params, 5011, (int)di, top, bottom);
    // wet row: MIX + GAIN knobs
    if (y + 60 > top && y < bottom) {
      text(o, C.x + 16, y + 16, 2, DIM, "WET MIX");
      Rect km = {C.x + 110, y, 62, 48};
      knob(o, km, "", (float)dv.mix);
      add(km, 5012, 0, (int)di);
      text(o, C.x + 200, y + 16, 2, DIM, "WET GAIN DB");
      Rect kg = {C.x + 330, y, 62, 48};
      knob(o, kg, "", (float)((dv.gainDb + 36) / 48));
      add(kg, 5013, 0, (int)di);
      char gb[24];
      std::snprintf(gb, sizeof gb, "%.1f", dv.gainDb);
      text(o, C.x + 404, y + 16, 2.2f, TXT, gb);
    }
    y += 62;
  }
  // ---- add FX
  Rect af = {C.x + 8, y, 220, 40};
  if (y + 40 > top && y < bottom + 60) {
    buttonS(o, af, "+ ADD FX", false);
    add(af, 5030);
  }
  y += 48;
  float contentH = y - (C.y + 6 - devScroll_);
  float maxScroll = std::max(0.0f, contentH - C.h);
  if (devScroll_ > maxScroll) devScroll_ = maxScroll;
}

bool App::touchDevice(const Hit& h, int, float x, float y) {
  Track* t = selTrackPtr();
  switch (h.id) {
    case 5000:
      drag_ = Drag{};
      drag_.what = kDragScrollDev;
      drag_.x0 = x; drag_.y0 = y; drag_.v0 = devScroll_;
      return true;
    case 5001: {  // change instrument
      std::vector<MenuItem> it;
      // All engine instruments, not just the web browser's INST_LIST: the
      // classic types (Mini Synth, Drums...) are the defaults and must stay
      // re-selectable here even though the web picker hides them.
      for (const defs::UnitDef& d : defs::kInstruments)
        it.push_back({d.name, d.type, t && t->instType == d.type});
      openMenu("INSTRUMENT", kMenuInstPick, it);
      return true;
    }
    case 5002: {  // presets for the current instrument
      if (!t) return true;
      menuPresets_.clear();
      std::vector<MenuItem> it;
      int n = (int)(sizeof(defs::kPresets) / sizeof(defs::PresetDef));
      for (int i = 0; i < n; i++)
        if (defs::kPresets[i].type == t->instType) {
          menuPresets_.push_back(i);
          std::string lbl = defs::kPresets[i].cat[0] ? std::string(defs::kPresets[i].cat) + ": " : "";
          it.push_back({lbl + defs::kPresets[i].name, (int)menuPresets_.size() - 1, false});
        }
      if (it.empty()) it.push_back({"(NO FACTORY PRESETS)", 0, false, true});
      openMenu("PRESETS", kMenuPresetPick, it);
      return true;
    }
    case 5010: {  // instrument param knob (h.a = param)
      if (!t) return true;
      const defs::UnitDef* d = instDef(t->instType);
      if (!d || h.a < 0 || h.a >= (int)d->params.size()) return true;
      checkpoint();
      drag_ = Drag{};
      drag_.what = kDragKnob;
      drag_.a = 0; drag_.b = h.a;
      drag_.x0 = x; drag_.y0 = y;
      ensureParams(t);
      drag_.v0 = ui::normOf(d->params[h.a], h.a < (int)t->instParams.size() ? t->instParams[h.a] : d->params[h.a].def);
      return true;
    }
    case 5011: {  // device param knob (h.a = param, h.b = device)
      if (!t || h.b < 0 || h.b >= (int)t->devices.size()) return true;
      FxDevice& dv = t->devices[h.b];
      const defs::UnitDef* d = devDef(dv.type);
      if (!d || h.a < 0 || h.a >= (int)d->params.size()) return true;
      checkpoint();
      drag_ = Drag{};
      drag_.what = kDragKnob;
      drag_.a = 1; drag_.b = h.b; drag_.c = h.a;
      drag_.x0 = x; drag_.y0 = y;
      ensureParams(&dv);
      drag_.v0 = ui::normOf(d->params[h.a], h.a < (int)dv.params.size() ? dv.params[h.a] : d->params[h.a].def);
      return true;
    }
    case 5012: case 5013: {  // wet mix / gain (h.b = device)
      if (!t || h.b < 0 || h.b >= (int)t->devices.size()) return true;
      checkpoint();
      drag_ = Drag{};
      drag_.what = kDragKnob;
      drag_.a = h.id == 5012 ? 2 : 3;
      drag_.b = h.b;
      drag_.x0 = x; drag_.y0 = y;
      drag_.v0 = h.id == 5012 ? t->devices[h.b].mix : (t->devices[h.b].gainDb + 36) / 48;
      return true;
    }
    case 5020:  // bypass toggle
      if (t && h.a >= 0 && h.a < (int)t->devices.size()) {
        checkpoint();
        FxDevice& dv = t->devices[h.a];
        dv.bypass = !dv.bypass;
        send("devBypass " + fmtNum(selStrip()) + " " + fmtNum(h.a) + (dv.bypass ? " 1\n" : " 0\n"));
      }
      return true;
    case 5021: {  // device card menu
      std::vector<MenuItem> it = {{"REMOVE", 1}, {"COPY", 2}, {"MOVE UP", 5}, {"MOVE DOWN", 6}};
      if (devClipOk_) it.push_back({"PASTE (REPLACE)", 3});
      openMenuX("DEVICE", kMenuDevCard, it, h.a, 0);
      return true;
    }
    case 5030: {  // add FX: stage 1 = groups
      devPickFor_ = t ? (int)t->devices.size() : 0;
      std::vector<MenuItem> it;
      int gi = 0;
      for (const defs::FxGroup& g : defs::kFxGroups) it.push_back({g.name, gi++, false});
      openMenuX("ADD FX - GROUP", kMenuFxPick, it, -1, 0);
      return true;
    }
    case 5050: {
      drag_ = Drag{};
      drag_.what = kDragSlider;
      drag_.a = 2; drag_.v0 = h.r.x; drag_.w0 = h.r.w; drag_.x0 = x; drag_.y0 = y;
      applySlider(2, (x - h.r.x) / h.r.w, false);
      return true;
    }
    case 5051:
      checkpoint();
      proj_.masterLimit = !proj_.masterLimit;
      send(std::string("masterLimit ") + (proj_.masterLimit ? "1\n" : "0\n"));
      return true;
    case 5052:
      checkpoint();
      proj_.pdc = !proj_.pdc;
      send(std::string("setPdc ") + (proj_.pdc ? "1\n" : "0\n"));
      return true;
    default: return false;
  }
}

}  // namespace mdnative
