// Harmor (instrument 38) in the project: loading an image (image synthesis) or an audio file (resynthesis) into a Harmor track's
// partial map. The work runs in js/harmor/worker.js; the packed map goes into two sample-bank slots (kept per track for the session)
// and the track's map parameters point at them. The track remembers the source (t.inst.hm: source, path, options, report), not the map:
// when the project opens the map is made again, or read from the render cache (the app's own storage) when nothing changed.
import { eng } from '../engine.js';
import * as P from '../project.js';
import { HM } from './schema.js';

let worker = null, jobId = 0;
const jobs = new Map(), slots = new Map();           // track id -> [slot A, slot B]
function work(msg, transfer, onProgress) {
  if (!worker) {
    worker = new Worker(new URL('./worker.js', import.meta.url), { type: 'module' });
    worker.onmessage = (e) => { const j = jobs.get(e.data.id); if (!j) return; if (e.data.progress != null) { if (j.onProgress) j.onProgress(e.data.progress); return; } jobs.delete(e.data.id); if (e.data.error) j.rej(new Error(e.data.error)); else j.res(e.data.ok); };
    worker.onerror = (e) => { for (const [k, j] of jobs) { jobs.delete(k); j.rej(new Error('worker: ' + (e.message || 'gagal'))); } worker = null; };
  }
  return new Promise((res, rej) => { const id = ++jobId; jobs.set(id, { res, rej, onProgress }); worker.postMessage({ id, ...msg }, transfer || []); });
}
const harmorTrack = (i) => { const t = P.state.tracks[i]; return t && t.inst && t.inst.type === 38 ? t : null; };

// the map into the engine: the track's two slots (made once), then the parameters (not an undo step: the map is data, like a sample)
async function install(i, t, out) {
  let ids = slots.get(t.id);
  if (!ids) { const a = P.allocSampleSlot(), b = P.allocSampleSlot(); if (a < 0 || b < 0) throw new Error('Slot sample penuh (256). Simpan proyek lalu buka ulang.'); ids = [a, b]; slots.set(t.id, ids); }
  const r1 = await eng.putSampleBuffers(ids[0], out.A.l, out.A.r || out.A.l); if (!r1 || r1.error) throw new Error((r1 && r1.error) || 'Engine tidak bisa memuat peta.');
  if (out.B) { const r2 = await eng.putSampleBuffers(ids[1], out.B.l, out.B.r || out.B.l); if (!r2 || r2.error) throw new Error((r2 && r2.error) || 'Engine tidak bisa memuat peta.'); }
  const j = P.state.tracks.indexOf(t); if (j < 0) return;
  P.setInstParamQuiet(j, HM.MAP_A, ids[0]); P.setInstParamQuiet(j, HM.MAP_B, out.B ? ids[1] : -1);
  if (!(t.inst.params[HM.MAP_MIX] > 0)) P.setInstParamQuiet(j, HM.MAP_MIX, 1);
}
const cacheKey = (kind, path, extra) => 'hm' + kind + '|' + path + '|' + (eng.fileSize ? eng.fileSize(path) : 0) + '|' + JSON.stringify(extra) + '|v1';
async function fromCache(key) {
  const a = await P.cacheGet(key + '|A'); if (!a) return null;
  const b = await P.cacheGet(key + '|B');
  return { A: { l: a.l, r: a.r !== a.l ? a.r : null }, B: b ? { l: b.l, r: b.r !== b.l ? b.r : null } : null };
}
function toCache(key, out) { P.cachePut(key + '|A', out.A.l, out.A.r || out.A.l); if (out.B) P.cachePut(key + '|B', out.B.l, out.B.r || out.B.l); }

// Resynthesis: an audio file of the library (WAV / MP3 / FLAC / OGG ...) -> the map. opts: { quality: 0 Draft, 1 Normal, 2 High }
export async function harmorLoadAudio(i, path, opts = {}, onProgress) {
  const t = harmorTrack(i); if (!t) return { error: 'Pilih track Harmor dulu.' };
  const quality = Math.max(0, Math.min(2, opts.quality ?? 1)), key = cacheKey('res', path, { quality });
  let out = await fromCache(key), report = null;
  if (!out) {
    const s = await P.loadSampleObj(path); const b = s && eng.sampleBuffers && eng.sampleBuffers(s.id);
    if (!b || !b.l) return { error: 'Audio tidak bisa dibaca: ' + path.split('/').pop() };
    const sr = (eng.info && eng.info().sampleRate) || 48000, l = b.l.slice(), r = b.r && b.r !== b.l ? b.r.slice() : null;
    const ok = await work({ kind: 'resynth', l, r, sr, quality }, [l.buffer, ...(r ? [r.buffer] : [])], onProgress);
    out = ok; report = ok.report; toCache(key, out);
  }
  await install(i, t, out);
  t.inst.hm = { src: 'audio', path, opts: { quality }, ...(report ? { report } : t.inst.hm && t.inst.hm.path === path && t.inst.hm.report ? { report: t.inst.hm.report } : {}) };
  P.emit('devices');
  return { ok: true, report: t.inst.hm.report || null };
}

// Image synthesis: a picture of the library (PNG / JPEG / WebP ...) -> the map. opts: buildImageMap's (crop x0..y1, frames, bins,
// layout 0 harmonic / 1 frequency, gamma, rangeDb, seconds, stereo, invert)
export async function harmorLoadImage(i, path, opts = {}) {
  const t = harmorTrack(i); if (!t) return { error: 'Pilih track Harmor dulu.' };
  const key = cacheKey('img', path, opts);
  let out = await fromCache(key);
  if (!out) {
    const bytes = await eng.readBytes(path); if (!bytes) return { error: 'Gambar tidak bisa dibaca: ' + path.split('/').pop() };
    let bmp; try { bmp = await createImageBitmap(new Blob([bytes])); } catch (_) { return { error: 'Format gambar tidak didukung.' }; }
    const sc = Math.min(1, 1024 / bmp.width, 1024 / bmp.height), w = Math.max(1, Math.round(bmp.width * sc)), h = Math.max(1, Math.round(bmp.height * sc));
    const cv = typeof OffscreenCanvas !== 'undefined' ? new OffscreenCanvas(w, h) : Object.assign(document.createElement('canvas'), { width: w, height: h });
    const g = cv.getContext('2d'); g.drawImage(bmp, 0, 0, w, h); const px = g.getImageData(0, 0, w, h).data;
    const rgba = new Uint8ClampedArray(px);
    out = await work({ kind: 'image', rgba, w, h, opts }, [rgba.buffer]); toCache(key, out);
  }
  await install(i, t, out);
  t.inst.hm = { src: 'image', path, opts };
  P.emit('devices');
  return { ok: true };
}
export function harmorClearMap(i) {
  const t = harmorTrack(i); if (!t) return;
  P.setInstParamQuiet(i, HM.MAP_A, -1); P.setInstParamQuiet(i, HM.MAP_B, -1); delete t.inst.hm; P.emit('devices');
}
// a project opening: the map of a Harmor track is made again (or read from the cache)
export async function restoreHarmorMap(t) {
  const i = P.state.tracks.indexOf(t); if (i < 0 || !t.inst || !t.inst.hm) return { ok: true };
  const h = t.inst.hm;
  try { return h.src === 'image' ? await harmorLoadImage(i, h.path, h.opts || {}) : await harmorLoadAudio(i, h.path, h.opts || {}); }
  catch (e) { return { error: (e && e.message) || String(e) }; }
}
