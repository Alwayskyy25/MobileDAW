// Harmor (instrument 38, native/engine/harmor.cpp) audio tests on the WebAssembly build of the engine (the same code the web app and the
// Android native engine run). Known signals in, measured audio out: no test looks at the UI.
//   fundamental frequency, harmonic distribution (saw 1/k, square odd 1/k, triangle odd 1/k^2, sine), partial phase (shift-invariant
//   phi_k - k phi_1), drawn harmonic table (level + phase) through the sample bank, timbre mix vs morph, phase randomisation (same
//   magnitudes, different waveform), partial count + quality caps, no aliasing near Nyquist, sub-octave partials, stretch, polyphony /
//   voice stealing without clicks, release to silence, pitch bend, 8-voice chords never clip, 44.1 and 48 kHz, CPU cost.
// Usage: node tools/harmor-test.mjs   (needs js/engine/mdaw.wasm: sh native/wasm/build.sh)
import { loadWasmEngine } from './wasm-engine.mjs';

let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
const db = (x) => 20 * Math.log10(Math.max(1e-12, x));

// Goertzel: complex DFT bin at frequency f over x[a .. a+n) (Hann window, amplitude-normalised)
function bin(x, sr, f, a, n) {
  let re = 0, im = 0, ws = 0;
  for (let i = 0; i < n; i++) { const w = 0.5 - 0.5 * Math.cos(2 * Math.PI * i / (n - 1)), ph = 2 * Math.PI * f * i / sr; re += x[a + i] * w * Math.cos(ph); im -= x[a + i] * w * Math.sin(ph); ws += w; }
  const amp = 2 * Math.hypot(re, im) / ws; return { amp, ph: Math.atan2(im, re) + Math.PI / 2 };   // x = A sin(wt + phi) -> phi
}
// the frequency of the strongest component near f0 (phase advance of the bin between two windows)
function freqAt(x, sr, f0, a, n) { const d = Math.round(sr * 0.01), p1 = bin(x, sr, f0, a, n).ph, p2 = bin(x, sr, f0, a + d, n).ph; let dp = p2 - p1 - 2 * Math.PI * f0 * d / sr; dp = Math.atan2(Math.sin(dp), Math.cos(dp)); return f0 + dp * sr / (2 * Math.PI * d); }
const wrap = (p) => Math.atan2(Math.sin(p), Math.cos(p));

async function synth(sr, params, notes, secs, extra = '') {
  const w = await loadWasmEngine(sr);
  let cmd = 'masterLimit 0\ntrackVol 0 1\ntrackInst 0 38\n';
  for (const [k, v] of Object.entries(params)) cmd += `instParam 0 ${k} ${v}\n`;
  cmd += extra;
  const er = w.cmd(cmd); if (er) throw new Error('engine: ' + er);
  const n = Math.round(secs * sr), L = new Float32Array(n), R = new Float32Array(n);
  let at = 0;
  const ev = notes.slice().sort((a, b) => a.t - b.t);
  for (const e of ev) {
    const to = Math.round(e.t * sr); if (to > at) { const l = new Float32Array(to - at), r = new Float32Array(to - at); w.render(l, r, to - at); L.set(l, at); R.set(r, at); at = to; }
    const er2 = w.cmd(e.cmd); if (er2) throw new Error('engine: ' + er2);
  }
  if (at < n) { const l = new Float32Array(n - at), r = new Float32Array(n - at); w.render(l, r, n - at); L.set(l, at); R.set(r, at); }
  return { L, R, w };
}
const on = (t, p, v = 1) => ({ t, cmd: `preview 0 ${p} ${v} 1` }), off = (t, p) => ({ t, cmd: `preview 0 ${p} 0 0` });
const peak = (x, a = 0, b = x.length) => { let m = 0; for (let i = a; i < b; i++) m = Math.max(m, Math.abs(x[i])); return m; };
const flat = { 40: 0.001, 41: 0.1, 42: 1, 43: 0.05, 44: 0 };   // envelope out of the way: instant attack, full sustain, no velocity scaling

for (const SR of [48000, 44100]) {
  const F = 220, A = Math.round(SR * 0.3), N = Math.round(SR * 0.5);
  // 1. fundamental + saw harmonic distribution + phase
  {
    const { L } = await synth(SR, { ...flat, 10: 1 }, [on(0, 57)], 1);
    const f = freqAt(L, SR, F, A, N), h = [1, 2, 3, 4, 5, 8].map((k) => bin(L, SR, F * k, A, N)), r = h.map((x) => x.amp / h[0].amp), want = [1, 1 / 2, 1 / 3, 1 / 4, 1 / 5, 1 / 8];
    ok(Math.abs(f - F) < 0.01, `${SR}: fundamental of A3 = ${f.toFixed(4)} Hz (220)`);
    ok(r.every((x, i) => Math.abs(db(x) - db(want[i])) < 0.15), `${SR}: saw harmonics fall as 1/k (${r.map((x) => db(x).toFixed(2)).join(', ')} dB)`);
    const rel = [2, 3, 4].map((k, i) => wrap(h[i + 1].ph - k * h[0].ph)), wantP = [Math.PI, 0, Math.PI];
    ok(rel.every((x, i) => Math.abs(wrap(x - wantP[i])) < 0.05), `${SR}: saw partial phases (phi_k - k phi_1 = ${rel.map((x) => x.toFixed(3)).join(', ')}; want pi, 0, pi)`);
  }
}
const SR = 48000, F = 220, A = Math.round(SR * 0.3), N = Math.round(SR * 0.5);
// 2. square / triangle / sine distributions
{
  const sq = (await synth(SR, { ...flat, 10: 2 }, [on(0, 57)], 1)).L, tr = (await synth(SR, { ...flat, 10: 3 }, [on(0, 57)], 1)).L, si = (await synth(SR, { ...flat, 10: 0 }, [on(0, 57)], 1)).L;
  const H = (x) => [1, 2, 3, 4, 5].map((k) => bin(x, SR, F * k, A, N).amp);
  const s = H(sq), t = H(tr), n = H(si);
  ok(Math.abs(db(s[2] / s[0]) - db(1 / 3)) < 0.15 && Math.abs(db(s[4] / s[0]) - db(1 / 5)) < 0.15 && s[1] / s[0] < 1e-3 && s[3] / s[0] < 1e-3, `square: odd harmonics 1/k, no even (${s.map((x) => db(x / s[0]).toFixed(1)).join(', ')} dB)`);
  ok(Math.abs(db(t[2] / t[0]) - db(1 / 9)) < 0.15 && Math.abs(db(t[4] / t[0]) - db(1 / 25)) < 0.2 && t[1] / t[0] < 1e-3, `triangle: odd harmonics 1/k^2 (${t.map((x) => db(x / t[0]).toFixed(1)).join(', ')} dB)`);
  ok(n.slice(1).every((x) => x / n[0] < 1e-3), `sine: a single partial (others below ${db(Math.max(...n.slice(1)) / n[0]).toFixed(0)} dB)`);
}
// 3. a drawn harmonic table (sample bank slot: l = level, r = phase) shapes the Custom timbre and offsets the phases
{
  const w0 = await loadWasmEngine(SR); void w0;
  const lv = new Float32Array(516), ph = new Float32Array(516); lv[0] = 1; lv[2] = 0.5; lv[6] = 0.25; ph[2] = 0.25;   // partials 1, 3, 7; partial 3 a quarter cycle on
  const w = await loadWasmEngine(SR), blob = new Float32Array(1032); blob.set(lv, 0); blob.set(ph, 516); w.setBlob(blob);
  let cmd = 'masterLimit 0\ntrackVol 0 1\nsample 20 @0:516 @516:516\ntrackInst 0 38\n'; for (const [k, v] of Object.entries({ ...flat, 10: 4, 13: 20 })) cmd += `instParam 0 ${k} ${v}\n`;
  const er = w.cmd(cmd + 'preview 0 57 1 1\n'); if (er) throw new Error(er);
  const n = SR, L = new Float32Array(n), R = new Float32Array(n); w.render(L, R, n);
  const h = [1, 2, 3, 4, 7].map((k) => bin(L, SR, F * k, A, N));
  ok(Math.abs(db(h[2].amp / h[0].amp) - db(0.5)) < 0.1 && Math.abs(db(h[4].amp / h[0].amp) - db(0.25)) < 0.1 && h[1].amp / h[0].amp < 1e-3 && h[3].amp / h[0].amp < 1e-3, `drawn levels play: partial 3 = ${db(h[2].amp / h[0].amp).toFixed(2)} dB (-6.02), 7 = ${db(h[4].amp / h[0].amp).toFixed(2)} dB (-12.04), 2 and 4 silent`);
  const rel = wrap(h[2].ph - 3 * h[0].ph);
  ok(Math.abs(wrap(rel - Math.PI / 2)) < 0.05, `drawn phase plays: partial 3 at ${rel.toFixed(3)} rad (pi/2)`);
  // editing the table while the note sounds changes the sound (the table is read again when its slot is set)
  lv[4] = 0.5; blob.set(lv, 0); w.setBlob(blob); w.cmd('sample 21 @0:516 @516:516\ninstParam 0 13 21\n');
  const L2 = new Float32Array(n), R2 = new Float32Array(n); w.render(L2, R2, n);
  ok(bin(L2, SR, F * 5, A, N).amp / bin(L2, SR, F, A, N).amp > 0.45, 'an edited table changes the sounding note (partial 5 appears)');
}
// 4. timbre mix (complex sum) vs morph (amplitude + phase direction)
{
  const base = { ...flat, 10: 1, 20: 2, 30: 0.5 };
  const mx = (await synth(SR, { ...base, 31: 0 }, [on(0, 57)], 1)).L, mo = (await synth(SR, { ...base, 31: 1 }, [on(0, 57)], 1)).L;
  const a = [1, 2, 3].map((k) => bin(mx, SR, F * k, A, N).amp), b = [1, 2, 3].map((k) => bin(mo, SR, F * k, A, N).amp);
  const want1 = 0.5 * (2 / Math.PI) + 0.5 * (4 / Math.PI), want2 = 0.5 * (2 / (2 * Math.PI)), want3 = 0.5 * (2 / (3 * Math.PI)) + 0.5 * (4 / (3 * Math.PI));
  ok(Math.abs(db(a[1] / a[0]) - db(want2 / want1)) < 0.15 && Math.abs(db(a[2] / a[0]) - db(want3 / want1)) < 0.15, `mix 50 % saw + square: partial 2 = ${db(a[1] / a[0]).toFixed(2)} dB (${db(want2 / want1).toFixed(2)}), 3 = ${db(a[2] / a[0]).toFixed(2)} dB (${db(want3 / want1).toFixed(2)})`);
  ok(Math.abs(db(b[0] / a[0])) < 0.1 && Math.abs(db(b[2] / a[2])) < 0.1, 'morph keeps the amplitudes of partials in phase (1, 3)');
  const sawPh = (await synth(SR, { ...flat, 10: 1, 20: 1, 22: 0, 30: 0.5, 31: 1, 12: 0 }, [on(0, 57)], 1)).L;
  ok(Math.abs(db(bin(sawPh, SR, F * 2, A, N).amp / bin(sawPh, SR, F, A, N).amp) - db(0.5)) < 0.1, 'morph of two equal timbres = that timbre');
}
// 5. phase randomisation: the same magnitudes, a different waveform
{
  const a = (await synth(SR, { ...flat, 10: 1, 12: 0 }, [on(0, 57)], 1)).L, b = (await synth(SR, { ...flat, 10: 1, 12: 1 }, [on(0, 57)], 1)).L;
  const ha = [1, 2, 3, 5].map((k) => bin(a, SR, F * k, A, N).amp), hb = [1, 2, 3, 5].map((k) => bin(b, SR, F * k, A, N).amp);
  let diff = 0; for (let i = A; i < A + N; i++) diff += (a[i] - b[i]) ** 2;
  ok(ha.every((x, i) => Math.abs(db(hb[i] / x)) < 0.1) && Math.sqrt(diff / N) > 0.02, `random phases: magnitudes kept (within 0.1 dB), waveform changed (RMS difference ${Math.sqrt(diff / N).toFixed(3)})`);
}
// 6. partial count, quality caps, Nyquist: a high saw has no energy above Nyquist and no aliases between the harmonics
{
  const few = (await synth(SR, { ...flat, 10: 1, 1: 4 }, [on(0, 57)], 1)).L;
  ok(bin(few, SR, F * 4, A, N).amp / bin(few, SR, F, A, N).amp > 0.2 && bin(few, SR, F * 5, A, N).amp / bin(few, SR, F, A, N).amp < 1e-4, 'Partials = 4: partial 4 plays, partial 5 does not');
  const draft = (await synth(SR, { ...flat, 10: 1, 2: 0, 1: 516 }, [on(0, 33)], 1)).L;   // A1 55 Hz: 436 partials below Nyquist
  ok(bin(draft, SR, 55 * 64, A, N).amp > 1e-4 && bin(draft, SR, 55 * 65, A, N).amp < 1e-5, 'Draft quality caps a voice at 64 partials');
  const hi = (await synth(SR, { ...flat, 10: 1, 1: 516, 2: 2 }, [on(0, 100)], 1)).L;   // E7 2637 Hz: partials 1..8 below 0.47 SR
  const f0 = 440 * Math.pow(2, (100 - 69) / 12); let alias = 0, harm = 0;
  for (let f = 100; f < SR / 2 - 100; f += 37) { const near = Math.abs(f / f0 - Math.round(f / f0)) * f0 < 60; const a = bin(hi, SR, f, A, N).amp; if (near) harm = Math.max(harm, a); else alias = Math.max(alias, a); }
  ok(db(alias / harm) < -80, `no aliasing for a high saw (strongest off-harmonic component ${db(alias / harm).toFixed(0)} dB)`);
}
// 7. sub-octave partials and harmonic stretch
{
  const s = (await synth(SR, { ...flat, 10: 0, 33: 1 }, [on(0, 57)], 1)).L;
  ok(Math.abs(db(bin(s, SR, F / 2, A, N).amp / bin(s, SR, F, A, N).amp) - db(0.5)) < 0.1, 'Sub: a partial one octave down at half level');
  const st = (await synth(SR, { ...flat, 10: 1, 32: 1 }, [on(0, 57)], 1)).L, f4 = F * Math.pow(4, 1.1);
  ok(freqAt(st, SR, f4, A, N) - f4 < 0.05 && bin(st, SR, F * 4, A, N).amp < 0.2 * bin(st, SR, f4, A, N).amp, `Stretch +1: partial 4 moves to ${f4.toFixed(1)} Hz (k^1.1)`);
}
// 8. polyphony, stealing without clicks, release to silence, pitch bend
{
  const notes = []; for (let k = 0; k < 12; k++) notes.push(on(0.05 * k, 48 + k));
  const offs = [...Array(12)].map((_, k) => off(1, 48 + k)), d2 = (x) => { let m = 0; for (let i = 2; i < Math.round(SR * 0.9); i++) m = Math.max(m, Math.abs(x[i] - 2 * x[i - 1] + x[i - 2])); return m; };   // 2nd difference: ~0 for sines, large at a discontinuity
  const { L, w } = await synth(SR, { ...flat, 10: 0, 3: 8, 43: 0.2 }, notes.concat(offs), 2), ref = (await synth(SR, { ...flat, 10: 0, 3: 16, 43: 0.2 }, notes.concat(offs), 2)).L;
  const js = d2(L), jr = d2(ref);
  ok(js <= jr * 1.3, `12 sine notes on 8 voices: the 4 stolen voices fade out, no click (largest 2nd difference ${js.toFixed(4)}, ${jr.toFixed(4)} with 16 voices and no stealing)`);
  ok(peak(L, Math.round(SR * 1.7)) < 1e-4, `after release the voices go silent (${db(peak(L, Math.round(SR * 1.7))).toFixed(0)} dB)`); void w;
  const chord = await synth(SR, { 10: 1, 3: 8, 0: 1, 42: 1, 44: 0, 40: 0.001 }, [48, 52, 55, 59, 62, 64, 67, 71].map((p) => on(0, p)), 1);
  ok(peak(chord.L) < 1, `an 8-voice saw chord at full volume does not clip (peak ${db(peak(chord.L)).toFixed(1)} dBFS)`);
  const b = await synth(SR, { ...flat, 10: 0 }, [on(0, 57), { t: 0.2, cmd: 'bend 0 2' }], 1);
  ok(Math.abs(freqAt(b.L, SR, F * Math.pow(2, 2 / 12), A, N) - F * Math.pow(2, 2 / 12)) < 0.05, 'pitch bend +2 semitones moves every partial');
}
// 9. CPU: 8 voices x 256 partials (Normal) in the WebAssembly engine, as on the device
{
  const w = await loadWasmEngine(SR); let cmd = 'masterLimit 0\ntrackInst 0 38\ninstParam 0 10 1\ninstParam 0 1 256\n'; for (const p of [36, 40, 43, 47, 50, 55, 59, 62]) cmd += `preview 0 ${p} 1 1\n`; w.cmd(cmd);
  const n = SR * 4, L = new Float32Array(n), R = new Float32Array(n), t0 = performance.now(); w.render(L, R, n); const ms = performance.now() - t0;
  console.log(`info 8 voices (Normal quality, adaptive partials): ${(ms / 40).toFixed(1)} % of one desktop core per second of audio`);
  ok(ms / 4000 < 0.35, 'Normal quality stays well inside real time on a desktop core');
}
console.log(bad ? `\n${bad} FAILED` : '\nharmor-test: all passed'); process.exit(bad ? 1 : 0);
