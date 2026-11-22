// Harmor analysis worker: audio resynthesis and image -> partial map off the UI thread. Messages: { id, kind: 'resynth', l, r, sr,
// quality } or { id, kind: 'image', rgba, w, h, opts }. Answers { id, progress } while working, then { id, ok: { A, B, report, meta } }
// (A / B = the packed sample-bank slots, transferred) or { id, error }.
import { analyzeResynth } from './resynth.js';
import { buildImageMap, packMap } from './imagemap.js';

self.onmessage = (e) => {
  const { id, kind } = e.data;
  try {
    const m = kind === 'resynth'
      ? analyzeResynth(e.data.l, e.data.r, e.data.sr, { quality: e.data.quality, onProgress: (f) => self.postMessage({ id, progress: f }) })
      : buildImageMap(e.data.rgba, e.data.w, e.data.h, e.data.opts || {});
    const pk = packMap(m), tr = [pk.A.l.buffer];
    if (pk.A.r) tr.push(pk.A.r.buffer);
    if (pk.B) { tr.push(pk.B.l.buffer); if (pk.B.r) tr.push(pk.B.r.buffer); }
    self.postMessage({ id, ok: { A: pk.A, B: pk.B, report: m.report || null, meta: { layout: m.layout, frames: m.frames, bins: m.bins, hop: m.hop, base: m.base } } }, tr);
  } catch (err) { self.postMessage({ id, error: String((err && err.message) || err) }); }
};
