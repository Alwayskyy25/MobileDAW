// ui_widgets.h - shared immediate-mode drawing helpers for the native UI.
// Everything is rects + 3x5 pixel text (the GLES renderer knows only those
// two primitives), in the fixed 1280x720 logical space.
#pragma once
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "font3x5.h"
#include "ui_core.h"  // Rect, DrawCmd

namespace mdnative {
namespace ui {

// Palette (web theme: dark #242426 family, orange accent).
constexpr uint32_t BG = 0x1B1B1EFF, PANEL = 0x242428FF, PANEL2 = 0x2E2E34FF,
                   PANEL3 = 0x3A3A42FF, LINE = 0x45454FFF, TXT = 0xEEEEF2FF,
                   DIM = 0x9A9AA6FF, FAINT = 0x6A6A76FF, ACC = 0xFF8C2EFF,
                   ACC2 = 0x3ECF6EFF, RED = 0xFF4444FF, YEL = 0xFFD94DFF,
                   CELL = 0x33333CFF, CELLOFF = 0x26262CFF, DARK = 0x101014FF,
                   WHITEKEY = 0xD8D8DEFF, BLACKKEY = 0x17171BFF;

inline void rect(std::vector<DrawCmd>* o, Rect r, uint32_t c) {
  DrawCmd d;
  d.kind = DrawCmd::FillRect;
  d.r = r;
  d.color = c;
  o->push_back(d);
}
inline void frame(std::vector<DrawCmd>* o, Rect r, uint32_t c) {
  rect(o, {r.x, r.y, r.w, 2}, c);
  rect(o, {r.x, r.y + r.h - 2, r.w, 2}, c);
  rect(o, {r.x, r.y, 2, r.h}, c);
  rect(o, {r.x + r.w - 2, r.y, 2, r.h}, c);
}
inline float textW(const std::string& s, float scale) { return font3x5::textWidth(s.c_str(), scale); }
inline void text(std::vector<DrawCmd>* o, float x, float y, float scale, uint32_t c, const std::string& s) {
  DrawCmd d;
  d.kind = DrawCmd::Text;
  d.r = {x, y, 0, 0};
  d.color = c;
  d.text = s;
  d.textScale = scale;
  o->push_back(d);
}
// Text clipped to a width (drops whole characters that do not fit).
inline void textClip(std::vector<DrawCmd>* o, float x, float y, float scale, uint32_t c, std::string s, float w) {
  float cw = 4 * scale;
  size_t maxn = cw > 0 ? (size_t)(w / cw) : 0;
  if (s.size() > maxn) s = s.substr(0, maxn);
  text(o, x, y, scale, c, s);
}
inline void textR(std::vector<DrawCmd>* o, float xr, float y, float scale, uint32_t c, const std::string& s) {
  text(o, xr - textW(s, scale), y, scale, c, s);
}
inline void textC(std::vector<DrawCmd>* o, Rect r, float scale, uint32_t c, const std::string& s) {
  float w = textW(s, scale);
  text(o, r.x + (r.w - w) / 2, r.y + (r.h - 5 * scale) / 2, scale, c, s);
}
// A push button; returns nothing - hit testing lives in the App hit list.
inline void button(std::vector<DrawCmd>* o, Rect r, const std::string& s, bool active, uint32_t base = PANEL2) {
  rect(o, r, active ? ACC : base);
  textC(o, r, 2.5f, active ? DARK : TXT, s);
}
inline void buttonS(std::vector<DrawCmd>* o, Rect r, const std::string& s, bool active, uint32_t base = PANEL2) {
  rect(o, r, active ? ACC : base);
  textC(o, r, 2, active ? DARK : TXT, s);
}
// Knob: square body, a position tick, and a value bar under it. value01 0..1.
inline void knob(std::vector<DrawCmd>* o, Rect r, const std::string& label, float value01, bool bipolar = false) {
  rect(o, r, PANEL3);
  float cx = r.cx(), cy = r.y + r.h * 0.42f;
  float ang = (bipolar ? (value01 - 0.5f) * 2 : value01) * 2.2f;  // radians-ish sweep
  float len = r.w * 0.32f;
  for (int i = 1; i <= 4; i++) {
    float t = i / 4.0f;
    float px = cx + std::sin(ang) * len * t, py = cy - std::cos(ang) * len * t;
    rect(o, {px - 2, py - 2, 4, 4}, ACC);
  }
  rect(o, {cx - 2, cy - 2, 4, 4}, TXT);
  rect(o, {r.x + 3, r.y + r.h - 12, r.w - 6, 4}, CELLOFF);
  rect(o, {r.x + 3, r.y + r.h - 12, (r.w - 6) * value01, 4}, ACC2);
  textClip(o, r.x + 2, r.y + r.h + 3, 1.8f, DIM, label, r.w);
}
// Vertical fader: track, fill from bottom, handle line. value01 0..1.
inline void fader(std::vector<DrawCmd>* o, Rect r, float value01, uint32_t fill = ACC2) {
  rect(o, r, CELLOFF);
  rect(o, {r.x, r.y + r.h * (1 - value01), r.w, r.h * value01}, fill);
  float hy = r.y + r.h * (1 - value01);
  rect(o, {r.x - 3, hy - 3, r.w + 6, 6}, TXT);
}
// Horizontal slider.
inline void hslider(std::vector<DrawCmd>* o, Rect r, float value01) {
  rect(o, r, CELLOFF);
  rect(o, {r.x, r.y, r.w * value01, r.h}, ACC);
  rect(o, {r.x + r.w * value01 - 3, r.y - 3, 6, r.h + 6}, TXT);
}
// Param value <-> knob position conversions (defs.js normOf/valOf).
inline double normOf(const defs::ParamDef& p, double v) {
  double x;
  if (p.logCurve) {
    if (p.min > 0) x = std::log(v / p.min) / std::log(p.max / p.min);
    else x = std::cbrt(std::max(0.0, (v - p.min) / (p.max - p.min)));
  } else x = (v - p.min) / (p.max - p.min);
  return x < 0 ? 0 : x > 1 ? 1 : x;
}
inline double valOf(const defs::ParamDef& p, double x) {
  x = x < 0 ? 0 : x > 1 ? 1 : x;
  double v;
  if (p.logCurve) {
    if (p.min > 0) v = p.min * std::pow(p.max / p.min, x);
    else v = p.min + (p.max - p.min) * x * x * x;
  } else v = p.min + (p.max - p.min) * x;
  if (p.step) v = std::round(v / p.step) * p.step;
  return v < p.min ? p.min : v > p.max ? p.max : v;
}
inline std::string fmtVal(const defs::ParamDef& p, double v) {
  if (p.items && p.items[0]) {
    // '|' separated item labels for stepped params
    std::string items = p.items;
    int idx = (int)std::round(v);
    size_t at = 0;
    for (int i = 0; i < idx; i++) {
      size_t nx = items.find('|', at);
      if (nx == std::string::npos) { at = items.size(); break; }
      at = nx + 1;
    }
    size_t nx = items.find('|', at);
    return items.substr(at, nx == std::string::npos ? nx : nx - at);
  }
  if (p.step >= 1) {
    char b[24];
    std::snprintf(b, sizeof b, "%d", (int)std::round(v));
    return b;
  }
  char b[32];
  if (std::fabs(v) >= 100) std::snprintf(b, sizeof b, "%.0f", v);
  else if (std::fabs(v) >= 10) std::snprintf(b, sizeof b, "%.1f", v);
  else std::snprintf(b, sizeof b, "%.2f", v);
  return b;
}
inline double linToDb(double v) { return v <= 0.0001 ? -60.0 : 20.0 * std::log10(v); }
inline double dbToLin(double db) { return std::pow(10.0, db / 20.0); }

}  // namespace ui
}  // namespace mdnative
