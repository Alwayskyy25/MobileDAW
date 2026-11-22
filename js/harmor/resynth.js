// Harmor audio resynthesis: an audio file -> a partial map (native/engine/harmor.h, "partial map"). Pure functions (Web Worker + tests).
//
// Pitched sound (a voiced majority): the note is the median YIN pitch (js/audiokit/pitch.js); every frame of an STFT (Hann, size and
// hop by quality) looks for each harmonic k near k x the frame's own f0 (the base note's when the frame is unvoiced), refines its peak
// by parabolic interpolation of the log magnitude, and records its amplitude (left and right), its frequency as a deviation in cents
// from k x the base note (so vibrato, glides and inharmonic partials come back) and its phase. Harmonic layout, base = the note.
// Unpitched sound (drums, noise, chords: no reliable pitch): a log-frequency map instead, the magnitude of every band per frame, which the
// engine reads at each partial's frequency (a spectral envelope, independent of the note played). The report says which and why.
// Amplitudes are sine amplitudes (a full-scale sine = 1), the scale Harmor's timbres use.
import { fft } from '../engine/dsp.js';
import { trackPitch, medianPitch, hzToMidi } from '../audiokit/pitch.js';
import { MAP_MAGIC, MAP_HDR } from './imagemap.js';

export const RESYNTH_QUALITY = [{ name: 'Draft', N: 2048, ov: 4 }, { name: 'Normal', N: 4096, ov: 8 }, { name: 'High', N: 8192, ov: 16 }];
const MAX_FRAMES = 4096, MAX_P = 516;

export function analyzeResynth(L, R, sr, { quality = 1, maxSec = 60, onProgress } = {}) {
  const len = Math.min(L.length, Math.round(maxSec * sr)), stereo = !!R && R !== L;
  const mono = new Float32Array(len); for (let i = 0; i < len; i++) mono[i] = stereo ? 0.5 * (L[i] + R[i]) : L[i];
  const tr = trackPitch(mono, sr, { hop: Math.round(sr * 0.005), minF: 40, maxF: 2000 });
  const voiced = tr.f0.filter((v, i) => v > 0 && tr.conf[i] > 0.85).length, audible = tr.rms.filter((d) => d > -55).length;
  const fb = medianPitch(tr), pitched = !!fb && voiced >= 0.5 * Math.max(1, audible);
  const Q = RESYNTH_QUALITY[Math.max(0, Math.min(2, quality))];
  let N = Q.N; if (pitched) while (N < 3 * sr / fb && N < 16384) N <<= 1;                 // at least 3 periods of the note in the window
  let hop = Math.max(Math.round(N / Q.ov), Math.ceil(len / (MAX_FRAMES - 1)));
  const frames = Math.max(1, Math.ceil(len / hop) + 1);
  const win = new Float64Array(N); let ws = 0; for (let i = 0; i < N; i++) { win[i] = 0.5 - 0.5 * Math.cos(2 * Math.PI * i / N); ws += win[i]; }
  const re = new Float64Array(N), im = new Float64Array(N), reR = new Float64Array(N), imR = new Float64Array(N), binHz = sr / N, half = N >> 1;
  const spec = (x, c, oRe, oIm) => { for (let i = 0; i < N; i++) { const k = c - half + i; oRe[i] = k >= 0 && k < len ? x[k] * win[i] : 0; oIm[i] = 0; } fft(oRe, oIm, N); };
  const mag = (oRe, oIm, b) => Math.hypot(oRe[b], oIm[b]);
  let w2 = 0; for (let i = 0; i < N; i++) w2 += win[i] * win[i];
  const lobe = pitched ? Math.max(2, Math.min(8, Math.floor(0.45 * fb / binHz))) : 2;
  // a sine's amplitude from the energy of its positive-frequency lobe: sum |X|^2 = N A^2 sum(w^2) / 4
  const lobeAmp = (oRe, oIm, b) => { let e = 0; for (let j = Math.max(1, b - lobe); j <= Math.min(half - 1, b + lobe); j++) e += oRe[j] * oRe[j] + oIm[j] * oIm[j]; return 2 * Math.sqrt(e / (N * w2)); };
  const f0At = (c) => { const i = Math.max(0, Math.min(tr.f0.length - 1, Math.round(c / tr.hop))); return tr.f0[i] > 0 && tr.conf[i] > 0.6 ? tr.f0[i] : 0; };
  if (pitched) {
    const K = Math.max(1, Math.min(MAX_P, Math.floor(0.47 * sr / fb))), n = frames * K;
    const aL = new Float32Array(n), aR = stereo ? new Float32Array(n) : null, dev = new Float32Array(n), ph = new Float32Array(n);
    for (let f = 0; f < frames; f++) {
      const c = f * hop, ff = f0At(c) || fb;
      spec(L, c, re, im); if (stereo) spec(R, c, reR, imR);
      for (let k = 1; k <= K; k++) {
        const fe = k * ff; if (fe >= 0.47 * sr) break;
        const lo = Math.max(1, Math.floor((fe - 0.35 * ff) / binHz)), hi = Math.min(half - 2, Math.ceil((fe + 0.35 * ff) / binHz));
        let b = Math.round(fe / binHz), bm = 0;
        for (let j = lo; j <= hi; j++) { const m = mag(re, im, j); if (m > bm) { bm = m; b = j; } }
        let fp = fe, A = 0, P = 0;
        if (b > lo && b < hi) {                       // a real peak inside the search range: parabolic on log magnitude
          const y0 = Math.log(mag(re, im, b - 1) + 1e-30), y1 = Math.log(bm + 1e-30), y2 = Math.log(mag(re, im, b + 1) + 1e-30), den = y0 - 2 * y1 + y2;
          const d = den < 0 ? Math.max(-0.5, Math.min(0.5, 0.5 * (y0 - y2) / den)) : 0;
          fp = (b + d) * binHz;
          A = lobeAmp(re, im, b, lobe);                // the energy of the whole lobe (Parseval): vibrato spreading a high partial over bins does not lower it
          P = Math.atan2(im[b], re[b]);
        } else { const bb = Math.max(1, Math.min(half - 2, Math.round(fe / binHz))); A = 2 * mag(re, im, bb) / ws; P = Math.atan2(im[bb], re[bb]); }
        const o = f * K + k - 1;
        aL[o] = A;
        if (stereo) aR[o] = lobeAmp(reR, imR, b);       // the right channel: the same lobe
        dev[o] = 1200 * Math.log2(fp / (k * fb));
        // phase as sin(w t + phi) at the frame centre: the FFT phase is at the window start (cos reference), the centre is half a window on
        let phi = P + Math.PI / 2 + 2 * Math.PI * fp * half / sr; phi = phi / (2 * Math.PI); ph[o] = phi - Math.floor(phi);
      }
      if (onProgress && f % 64 === 0) onProgress(f / frames);
    }
    return { layout: 0, frames, bins: K, hop: hop / sr, base: hzToMidi(fb), flo: 20, fhi: 20000, aL, aR, dev, ph,
      report: { pitched: true, note: hzToMidi(fb), hz: fb, voiced: voiced / Math.max(1, audible), frames, partials: K, fft: N, hopMs: 1000 * hop / sr, quality: Q.name } };
  }
  // unpitched: log-frequency bands (RMS magnitude of the bins in each band)
  const B = 256, flo = 20, fhi = Math.min(20000, 0.47 * sr), lf0 = Math.log2(flo), lsp = Math.log2(fhi) - lf0, n = frames * B;
  const aL = new Float32Array(n), aR = stereo ? new Float32Array(n) : null;
  const band = (oRe, oIm, j) => { const a = Math.pow(2, lf0 + lsp * (j - 0.5) / (B - 1)) / binHz, b = Math.pow(2, lf0 + lsp * (j + 0.5) / (B - 1)) / binHz;
    let s = 0, c = 0; for (let k = Math.max(1, Math.floor(a)); k <= Math.min(half - 1, Math.ceil(b)); k++) { const m = mag(oRe, oIm, k); s += m * m; c++; }
    if (!c) { const k = Math.max(1, Math.min(half - 1, Math.round((a + b) / 2))); return 2 * mag(oRe, oIm, k) / ws; }
    return 2 * Math.sqrt(s / c) / ws; };
  for (let f = 0; f < frames; f++) {
    spec(L, f * hop, re, im); if (stereo) spec(R, f * hop, reR, imR);
    for (let j = 0; j < B; j++) { aL[f * B + j] = band(re, im, j); if (stereo) aR[f * B + j] = band(reR, imR, j); }
    if (onProgress && f % 64 === 0) onProgress(f / frames);
  }
  return { layout: 1, frames, bins: B, hop: hop / sr, base: 0, flo, fhi, aL, aR, dev: null, ph: null,
    report: { pitched: false, why: fb ? 'nada tidak stabil (sebagian besar tidak bernada)' : 'tidak ada nada yang dapat dipercaya', voiced: voiced / Math.max(1, audible), frames, bands: B, fft: N, hopMs: 1000 * hop / sr, quality: Q.name } };
}
export { MAP_MAGIC, MAP_HDR };
