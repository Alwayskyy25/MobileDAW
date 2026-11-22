// Harmor (instrument 38): additive synthesis, MobileDAW's own DSP (see harmor.h).
//
// Every voice is a bank of sine partials, each a complex rotation oscillator (cos, sin of its running phase): the phase stays continuous
// whatever the frequency does, nothing is read from a table, and the amplitude is renormalised once per step. A partial sounds
//   out += sin(psi) * X + cos(psi) * Y,   X = a cos(phi), Y = a sin(phi)
// so its amplitude a and phase phi (from the two timbres) are two coefficients, ramped linearly over every SUB-sample step (no clicks
// when a level, the mix or the partial count changes). Timbre mix: "Mix" adds the two complex partials, "Morph" interpolates the
// amplitude and the phase direction. Partials near Nyquist fade out (0.40 .. 0.47 of the rate) and above it are not computed at all.
// The partial count adapts to the quality (Draft / Normal / High caps) and to the number of sounding voices (a partial-voice budget),
// the highest partials fading out first.
#include "harmor.h"
#include "dsp.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace mdaw {
namespace {

constexpr double TAU = 6.283185307179586;
constexpr int SUB = 32;                 // samples per coefficient step
constexpr int NX = HM_MAXP + 2;         // + two sub-octave partials (f0 / 2, f0 / 4)
constexpr int CAP[3] = { 64, 256, HM_MAXP }, BUDGET[3] = { 512, 2048, 8192 };   // Draft / Normal / High: partials per voice, partial-voices in all

struct Timbre {
  float a[HM_MAXP] = {}, ph[HM_MAXP] = {};          // amplitude (>= 0), phase (radians)
  float tl[HM_MAXP] = {}, tp[HM_MAXP] = {};         // the drawn table: level, phase (0..1 of a cycle)
  bool table = false;
};

struct Voice {
  bool on = false, held = false, fresh = false; int pitch = 0; double vel = 0; uint32_t age = 0;
  Adsr env; double f0 = 0, bend = 1, rotF = -1;     // rotF: the frequency the rotation coefficients were made for
  int n = 0;                                       // partials computed (the sounding ones and those fading out)
  bool dirty = true;                               // phase vectors to rebuild (note start, timbre phase change)
  int pPitch = -1; double pVel = 0; int fade = 0;  // stealing: fade out over `fade` samples, then start the pending note
  float c[NX], s[NX], cr[NX], sr[NX], X[NX], Y[NX], u[NX], e1x[NX], e1y[NX], e2x[NX], e2y[NX];
};

class Harmor : public Instrument {
 public:
  Harmor(double rate, Bank* b) : sr(rate), bank(b) {
    for (double& x : p) x = 0;
    p[HM_VOL] = 0.7; p[HM_PARTIALS] = 256; p[HM_QUALITY] = 1; p[HM_POLY_P] = 8;
    p[HM_T_WAVE] = HM_SAW; p[HM_T_LEVEL] = 1; p[HM_T_TABLE] = -1;
    p[HM_T_WAVE + 10] = HM_SQUARE; p[HM_T_LEVEL + 10] = 1; p[HM_T_TABLE + 10] = -1;
    p[HM_BLEND] = 0; p[HM_ATT] = 0.005; p[HM_DEC] = 0.3; p[HM_SUS] = 0.8; p[HM_REL] = 0.3; p[HM_VELS] = 0.7;
    voices = new Voice[HM_POLY];
    buildTimbre(0); buildTimbre(1); upMult(); upEnv();
  }
  ~Harmor() override { delete[] voices; }

  void setParam(int i, double v) override {
    if (i < 0 || i >= HM_PARAMS) return;
    p[i] = v;
    switch (i) {
      case HM_T_WAVE: case HM_T_WAVE + 10: buildTimbre(i == HM_T_WAVE ? 0 : 1); break;
      case HM_T_TABLE: case HM_T_TABLE + 10: loadTable(i == HM_T_TABLE ? 0 : 1, (int)v); break;
      case HM_T_PRND: case HM_T_PRND + 10: for (int k = 0; k < HM_POLY; k++) voices[k].dirty = true; break;
      case HM_STRETCH: upMult(); break;
      case HM_ATT: case HM_DEC: case HM_SUS: case HM_REL: upEnv(); break;
      default: break;
    }
  }
  void noteOn(int pitch, double vel) override {
    int maxv = std::max(1, std::min(HM_POLY, (int)std::lround(p[HM_POLY_P]))), n = 0;
    for (int k = 0; k < HM_POLY; k++) if (voices[k].on) n++;
    Voice* v = nullptr;
    if (n < maxv) for (int k = 0; k < HM_POLY; k++) if (!voices[k].on) { v = &voices[k]; break; }
    if (!v) {                                            // steal: the oldest released voice, else the oldest; it fades out first (no click)
      for (int pass = 0; pass < 2 && !v; pass++) for (int k = 0; k < HM_POLY; k++) {
        Voice& x = voices[k]; if (!x.on || (pass == 0 && x.held) || x.pPitch >= 0) continue;
        if (!v || x.age < v->age) v = &x;
      }
      if (!v) v = &voices[0];
      if (v->on) { v->pPitch = pitch; v->pVel = vel; v->fade = std::max(v->fade, (int)(sr * 0.003)); v->held = false; return; }
    }
    start(*v, pitch, vel);
  }
  void noteOff(int pitch) override {
    for (int k = 0; k < HM_POLY; k++) { Voice& v = voices[k]; if (v.pPitch == pitch) v.pPitch = -1; if (v.on && v.held && v.pitch == pitch) { v.held = false; v.env.release(); } }
  }
  void allOff() override { for (int k = 0; k < HM_POLY; k++) { Voice& v = voices[k]; v.pPitch = -1; if (v.on) { v.held = false; v.env.release(); } } }
  bool active() override { for (int k = 0; k < HM_POLY; k++) if (voices[k].on) return true; return false; }
  int voicesOn() override { int n = 0; for (int k = 0; k < HM_POLY; k++) if (voices[k].on) n++; return n; }
  bool canBend() override { return true; }
  void bendSounding(double r) override { for (int k = 0; k < HM_POLY; k++) if (voices[k].on) voices[k].bend *= r; }
  void bendNewest(double r) override { for (int k = 0; k < HM_POLY; k++) if (voices[k].on && voices[k].age == counter) voices[k].bend *= r; }

  void render(float* L, float* R, int a, int b) override {
    int on = voicesOn(); if (!on) return;
    int q = std::max(0, std::min(2, (int)std::lround(p[HM_QUALITY])));
    int want = std::max(1, std::min({ HM_MAXP, CAP[q], (int)std::lround(p[HM_PARTIALS]) }));
    want = std::min(want, std::max(8, BUDGET[q] / on));                      // adaptive: the partial-voice budget of the quality
    // voices into the instrument's own buffer, then a soft limit (transparent below 0.7, never past 1) added to the track
    for (int at = a; at < b; ) {
      int m = std::min(SUB, b - at); std::memset(oL, 0, sizeof(float) * m); std::memset(oR, 0, sizeof(float) * m);
      for (int k = 0; k < HM_POLY; k++) { Voice& v = voices[k]; if (v.on) renderVoice(v, oL, oR, m, want); }
      for (int i = 0; i < m; i++) { L[at + i] += soft(oL[i]); R[at + i] += soft(oR[i]); }
      at += m;
    }
  }

 private:
  double sr; Bank* bank; double p[HM_PARAMS]; Voice* voices = nullptr; Timbre tb[2]; float mult[NX]; AdsrP ap; uint32_t counter = 0, seed = 0x9e3779b9u;
  float buf[SUB], oL[SUB], oR[SUB];
  static float soft(float x) { float a = std::fabs(x); if (a <= 0.7f) return x; float y = 0.7f + 0.3f * std::tanh((a - 0.7f) / 0.3f); return x < 0 ? -y : y; }

  float rnd() { seed = seed * 1664525u + 1013904223u; return (seed >> 8) * (1.0f / 16777216.0f); }
  void upEnv() { ap = adsrParams(std::max(0.001, p[HM_ATT]), std::max(0.001, p[HM_DEC]), std::max(0.0, std::min(1.0, p[HM_SUS])), std::max(0.002, p[HM_REL]), sr); }
  // harmonic mapping: partial k (1-based) at k^(1 + 0.1 stretch) times the fundamental; the sub partials at 1/2 and 1/4
  void upMult() {
    double e = 1 + 0.1 * std::max(-1.0, std::min(1.0, p[HM_STRETCH]));
    for (int k = 0; k < HM_MAXP; k++) mult[k] = (float)std::pow((double)(k + 1), e);
    mult[HM_MAXP] = 0.5f; mult[HM_MAXP + 1] = 0.25f;
    for (int k = 0; k < HM_POLY; k++) voices[k].rotF = -1;
  }
  void loadTable(int t, int id) {
    const Sample* s = id >= 0 && id < 256 && bank ? bank->s[id] : nullptr;
    Timbre& T = tb[t]; T.table = s && s->frames >= 1;
    if (T.table) { int n = std::min(HM_MAXP, s->frames); for (int k = 0; k < HM_MAXP; k++) { T.tl[k] = k < n ? std::max(0.0f, std::min(1.0f, s->l[k])) : 0; T.tp[k] = k < n && s->r ? s->r[k] : 0; } }
    buildTimbre(t);
  }
  // the timbre's partials: the waveform's Fourier series (sign as a half-cycle phase), times the drawn level, plus the drawn phase
  void buildTimbre(int t) {
    Timbre& T = tb[t]; int w = std::max(0, std::min(4, (int)std::lround(p[HM_T_WAVE + 10 * t])));
    double e2 = 0;
    for (int k = 0; k < HM_MAXP; k++) {
      int h = k + 1; double a = 0, ph = 0;
      switch (w) {
        case HM_SINE: a = h == 1 ? 1 : 0; break;
        case HM_SAW: a = 2 / (M_PI * h); ph = (h & 1) ? 0 : M_PI; break;
        case HM_SQUARE: a = (h & 1) ? 4 / (M_PI * h) : 0; break;
        case HM_TRI: a = (h & 1) ? 8 / (M_PI * M_PI * h * h) : 0; ph = ((h - 1) / 2) & 1 ? M_PI : 0; break;
        default: a = T.table ? 1 : 1.0 / h; break;    // Custom: the drawn levels themselves (no table yet: a saw-like 1/k)
      }
      if (T.table) { a *= T.tl[k]; ph += T.tp[k] * TAU; }
      T.a[k] = (float)a; T.ph[k] = (float)ph; e2 += a * a;
    }
    if (w == HM_CUSTOM && e2 > 0) {                    // a drawn spectrum never louder than a saw (RMS)
      double g = std::min(1.0, std::sqrt((4 / (M_PI * M_PI)) * (M_PI * M_PI / 6) / e2));
      for (int k = 0; k < HM_MAXP; k++) T.a[k] = (float)(T.a[k] * g);
    }
    for (int k = 0; k < HM_POLY; k++) voices[k].dirty = true;
  }
  void start(Voice& v, int pitch, double vel) {
    v.on = v.held = true; v.fresh = true; v.pitch = pitch; v.vel = vel; v.age = ++counter; v.bend = 1; v.rotF = -1; v.dirty = true; v.fade = 0; v.pPitch = -1;
    v.f0 = 440.0 * std::pow(2.0, (pitch - 69) / 12.0);
    for (int k = 0; k < NX; k++) { v.c[k] = 1; v.s[k] = 0; v.X[k] = v.Y[k] = 0; v.u[k] = rnd(); }
    v.n = 0; v.env.v = 0; v.env.start();
  }
  void phaseVectors(Voice& v) {
    for (int t = 0; t < 2; t++) {
      const Timbre& T = tb[t]; double r = std::max(0.0, std::min(1.0, p[HM_T_PRND + 10 * t])) * TAU;
      float* ex = t ? v.e2x : v.e1x; float* ey = t ? v.e2y : v.e1y;
      for (int k = 0; k < HM_MAXP; k++) { double ph = T.ph[k] + r * v.u[k]; ex[k] = (float)std::cos(ph); ey[k] = (float)std::sin(ph); }
    }
    v.dirty = false;
  }
  void renderVoice(Voice& v, float* L, float* R, int m, int want) {
    if (v.dirty) phaseVectors(v);
    double f0 = v.f0 * v.bend * std::pow(2.0, (p[HM_PITCH] + p[HM_FINE] / 100) / 12.0), nyq = sr * 0.5;
    if (std::fabs(f0 - v.rotF) > 1e-9 * f0) {          // rotation per sample of every partial (only when the pitch changed)
      for (int k = 0; k < NX; k++) { double d = TAU * f0 * mult[k] / sr; if (d < M_PI) { v.cr[k] = (float)std::cos(d); v.sr[k] = (float)std::sin(d); } else { v.cr[k] = 1; v.sr[k] = 0; } }
      v.rotF = f0;
    }
    double mix = std::max(0.0, std::min(1.0, p[HM_MIX])), w1 = (1 - mix) * std::max(0.0, p[HM_T_LEVEL]), w2 = mix * std::max(0.0, p[HM_T_LEVEL + 10]);
    bool morph = p[HM_BLEND] > 0.5; double sub = std::max(0.0, std::min(1.0, p[HM_SUB]));
    std::memset(buf, 0, sizeof(float) * m);
    float inv = 1.0f / m; int last = 0;
    int top = std::max(v.n, want);
    for (int k = 0; k < NX; k++) {
      if (k >= top && k < HM_MAXP) { k = HM_MAXP - 1; continue; }   // jump to the sub partials
      double f = f0 * mult[k], g = f >= 0.47 * sr ? 0 : f > 0.40 * sr ? (0.47 * sr - f) / (0.07 * sr) : 1;
      float tx = 0, ty = 0;
      if (k < HM_MAXP) {
        if (k < want && g > 0) {
          double a1 = w1 * tb[0].a[k], a2 = w2 * tb[1].a[k];
          if (!morph) { tx = (float)(g * (a1 * v.e1x[k] + a2 * v.e2x[k])); ty = (float)(g * (a1 * v.e1y[k] + a2 * v.e2y[k])); }
          else {                                         // morph: amplitude and phase direction interpolated
            double A = g * (a1 + a2), dx = (1 - mix) * v.e1x[k] + mix * v.e2x[k], dy = (1 - mix) * v.e1y[k] + mix * v.e2y[k], l = std::sqrt(dx * dx + dy * dy);
            if (l < 1e-6) { dx = v.e1x[k]; dy = v.e1y[k]; l = 1; }
            tx = (float)(A * dx / l); ty = (float)(A * dy / l);
          }
        }
      } else if (sub > 0 && g > 0) tx = (float)(sub * (k == HM_MAXP ? 0.5 : 0.25) * g);
      if (f >= nyq) { tx = ty = 0; }
      if (v.fresh) { v.X[k] = tx; v.Y[k] = ty; }
      float x = v.X[k], y = v.Y[k];
      if (x == 0 && y == 0 && tx == 0 && ty == 0) continue;
      if (k < HM_MAXP) last = k + 1;
      float dx = (tx - x) * inv, dy = (ty - y) * inv, c = v.c[k], s = v.s[k];
      const float cr = v.cr[k], sn = v.sr[k];
      for (int i = 0; i < m; i++) {
        buf[i] += s * x + c * y;
        const float c2 = c * cr - s * sn; s = s * cr + c * sn; c = c2;
        x += dx; y += dy;
      }
      const float g2 = 1.5f - 0.5f * (c * c + s * s);   // keep the oscillator on the unit circle
      v.c[k] = c * g2; v.s[k] = s * g2; v.X[k] = tx; v.Y[k] = ty;
    }
    v.n = last; v.fresh = false;
    // amplitude: envelope, velocity, volume; a stolen voice fades out, then its pending note starts
    double vs = std::max(0.0, std::min(1.0, p[HM_VELS])), gain = 0.25 * std::max(0.0, p[HM_VOL]) * ((1 - vs) + vs * v.vel);
    for (int i = 0; i < m; i++) {
      double e = v.env.next(ap) * gain;
      if (v.fade > 0) { e *= (double)v.fade / (sr * 0.003 + 1); v.fade--; if (v.fade == 0) { if (v.pPitch >= 0) { int pp = v.pPitch; double pv = v.pVel; start(v, pp, pv); } else { v.on = false; } float o = (float)(buf[i] * e); L[i] += o; R[i] += o; for (int j = i + 1; j < m; j++) buf[j] = 0; break; } }
      float o = (float)(buf[i] * e); L[i] += o; R[i] += o;
    }
    if (v.env.st == 0 && v.fade == 0) v.on = false;
  }
};

}  // namespace

Instrument* makeHarmor(double sr, Bank* bank) { return new Harmor(sr, bank); }

}  // namespace mdaw
