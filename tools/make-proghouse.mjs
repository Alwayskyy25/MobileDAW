// Builds the tutorial showcase song demos/progressive-house.mdaw: "Neon Horizon", an original progressive house track, 128 BPM, F minor,
// 64 bars = 2:00, made with MobileDAW's built-in instruments (FL Studio Mobile's SuperSaw, MiniSynth, GMS), stock effects and CC0 drum
// samples from the Stargate DAW sample pack (demos/Samples/NH *.wav, see demos/README.txt), through the app's own project API in Chromium,
// so the file is exactly what the app writes.
// Form (bars): Intro 1-8, Build 9-16, Groove 17-24, Breakdown 25-32 (riser + snare roll), Drop 33-48, Drop B 49-56, Outro 57-64.
// Chords i-VI-III-VII (Fm - Db - Ab - Eb), two bars each. Bass, pad and pluck duck under the kick (Autoduck = sidechain).
// Usage: node tools/make-proghouse.mjs [--render out.wav]   (the render is a mixdown for listening checks)
import http from 'node:http'; import fs from 'node:fs'; import path from 'node:path'; import { createRequire } from 'node:module'; import { fileURLToPath } from 'node:url';
const root = path.join(path.dirname(fileURLToPath(import.meta.url)), '..');
const { chromium } = createRequire(import.meta.url)('playwright');
const T = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.wasm': 'application/wasm' };
const server = http.createServer((q, r) => { const f = path.join(root, decodeURIComponent(new URL(q.url, 'http://x').pathname)); if (!fs.existsSync(f) || fs.statSync(f).isDirectory()) { r.writeHead(404); r.end(); return; } r.writeHead(200, { 'content-type': T[path.extname(f)] || 'application/octet-stream' }); fs.createReadStream(f).pipe(r); }).listen(0);
await new Promise((o) => server.once('listening', o));
const b = await chromium.launch(), page = await (await b.newContext({ viewport: { width: 1400, height: 900 } })).newPage();
page.on('pageerror', (e) => console.log('pageerror', e.message));
await page.addInitScript(() => localStorage.setItem('mobiledaw.prefs.v1', JSON.stringify({ folderAsked: true, haptics: false, floatPlugins: false })));
await page.goto(`http://127.0.0.1:${server.address().port}/index.html`); await page.waitForFunction(() => window.__dawBooted); await page.waitForTimeout(800);
const renderTo = (process.argv.includes('--render') && process.argv[process.argv.indexOf('--render') + 1]) || null;

const out = await page.evaluate(async (render) => {
  const P = await import('./js/project.js'), { PRESETS, DEVICES, normOf } = await import('./js/defs.js'), { eng } = await import('./js/engine.js');
  const PPQ = 960, BEAT = PPQ, BAR = 4 * BEAT, S16 = BEAT / 4, bar = (n) => (n - 1) * BAR;   // bar(1) = song start
  P.newProject(false); P.HK.on = false;
  // the CC0 samples, as the demo loader puts them (demos.js: demos/Samples/<name>.wav -> /Samples/Demo/<name>.wav)
  const SMP = ['NH Kick', 'NH Clap', 'NH Hat Closed', 'NH Hat Open', 'NH Snare', 'NH Crash', 'NH Ride', 'NH Noise Riser'];
  for (const n of SMP) await eng.saveBytes('/Samples/Demo/' + n + '.wav', await (await fetch('demos/Samples/' + encodeURIComponent(n) + '.wav')).arrayBuffer());
  const smp = (n) => '/Samples/Demo/' + n + '.wav';
  P.setBpm(128); P.state.projectName = 'Neon Horizon (Progressive House Showcase)';
  const preset = (i, type, name) => { const p = (PRESETS[type] || []).find((x) => x.name === name); if (!p) throw new Error('preset ' + name); P.applyPreset(i, p); };
  const track = (kind, name, color) => { P.addTrack(kind); const i = P.state.tracks.length - 1; P.renameTrack(i, name); if (color) P.state.tracks[i].color = color; return i; };
  const dev = (i, type, params = {}) => { P.addDevice(i, type); const t = i === 64 ? P.state.master : i >= 65 ? P.state.returns[i - 65] : P.state.tracks[i], di = t.devices.length - 1; for (const [k, v] of Object.entries(params)) P.setDeviceParam(i, di, +k, v); return di; };
  // a pattern clip: `notes` = [beat, len beats, pitch, vel] inside one loop of `loopBars`, repeated over `bars` bars from bar `from`
  const clip = (i, from, bars, loopBars, notes, name) => {
    const c = P.addClip(i, bar(from), bars * BAR); c.name = name || '';
    c.notes = notes.map(([bt, len, p, v]) => ({ t: Math.round(bt * BEAT), d: Math.max(30, Math.round(len * BEAT)), p, v }));
    if (loopBars && loopBars < bars) { c.ll = loopBars * BAR; c.pb = loopBars * BAR; } else c.pb = bars * BAR;
    P.clipChanged(i); return c;
  };
  const rep = (n, f) => Array.from({ length: n }, (_, k) => f(k)).flat();
  // ---- harmony: Fm - Db - Ab - Eb, two bars each (8-bar cycle)
  const CH = [[53, 56, 60, 65], [53, 56, 61, 65], [51, 56, 60, 63], [51, 55, 58, 63]], ROOT = [41, 37, 44, 39];
  // ---- 1. Kick (its own track: the sidechain source)
  const kick = track('drum', 'Kick', '#ff5f4a'); P.state.tracks[kick].vol = 0.68;
  await P.setPadSample(kick, 0, smp('NH Kick'));
  const four = rep(4, (k) => [[k, 0.25, 36, 1]]);
  for (const [a, n] of [[1, 24], [33, 32]]) clip(kick, a, n, 1, four, 'Kick 4/4');
  dev(kick, 31, { 0: 1, 1: 0, 2: 35, 15: 1, 16: 3, 17: 3500, 18: 2, 19: 0.8 });          // EQ Eight: low cut 35 Hz, a little click at 3.5 kHz
  // ---- 2. Drums: clap 2 & 4, offbeat hat, 16th ghost hats in the drops, open hat, snare roll into the drop
  const dr = track('drum', 'Drums', '#ffb347'); P.state.tracks[dr].vol = 0.55;
  for (const [k, n] of [[1, 'NH Snare'], [2, 'NH Clap'], [3, 'NH Hat Closed'], [4, 'NH Hat Open']]) await P.setPadSample(dr, k, smp(n));
  const hatOff = rep(4, (k) => [[k + 0.5, 0.2, 42, 0.85]]);
  const clap = [[1, 0.25, 39, 0.95], [3, 0.25, 39, 0.95]];
  const ghost = rep(16, (k) => (k % 4 === 2 ? [] : [[k / 4, 0.1, 42, k % 2 ? 0.35 : 0.5]]));
  clip(dr, 1, 8, 1, hatOff, 'Hats');
  clip(dr, 9, 8, 1, [...hatOff, ...clap], 'Hats + Clap');
  clip(dr, 17, 8, 1, [...hatOff, ...clap, [3.5, 0.4, 46, 0.6]], 'Groove');
  // the build: quarter -> 8th -> 16th -> 32nd snares, velocity rising (bars 29-32)
  const roll = [...rep(4, (k) => [[k, 0.2, 38, 0.45 + k * 0.02]]), ...rep(8, (k) => [[4 + k / 2, 0.15, 38, 0.55 + k * 0.015]]), ...rep(16, (k) => [[8 + k / 4, 0.1, 38, 0.65 + k * 0.01]]), ...rep(32, (k) => [[12 + k / 8, 0.06, 38, 0.75 + k * 0.007]])];
  clip(dr, 29, 4, 0, roll, 'Snare Roll');
  clip(dr, 33, 16, 1, [...ghost, ...clap, ...rep(4, (k) => [[k + 0.5, 0.35, 46, 0.7]])], 'Drop Drums');
  clip(dr, 49, 8, 1, [...ghost, ...clap, ...rep(4, (k) => [[k + 0.5, 0.35, 46, 0.75]]), [2.75, 0.2, 41, 0.6]], 'Drop B Drums');
  clip(dr, 57, 8, 1, [...hatOff, ...clap], 'Outro');
  dev(dr, 7, { 0: 0.35, 2: 0.12 });                                                    // a little room on the claps and hats
  // ---- 2b. Cymbals: a crash on every section's downbeat, a ride on the beat in Drop B
  const cy = track('drum', 'Cymbals', '#ffd166'); P.state.tracks[cy].vol = 0.3;
  const cr = await P.addPadSample(cy, smp('NH Crash')), rd = await P.addPadSample(cy, smp('NH Ride'));
  const CR = P.state.tracks[cy].inst.pads[cr.k].pitch, RD = P.state.tracks[cy].inst.pads[rd.k].pitch;
  for (const b0 of [9, 17, 33, 57]) clip(cy, b0, 1, 0, [[0, 2, CR, 0.9]], 'Crash');
  clip(cy, 49, 8, 1, [[0, 0.5, RD, 0.55], [1, 0.5, RD, 0.5], [2, 0.5, RD, 0.55], [3, 0.5, RD, 0.5]], 'Ride');
  P.state.tracks[cy].clips.find((c) => c.start === bar(49)).notes.push({ t: 0, d: 1920, p: CR, v: 0.9 });
  dev(cy, 31, { 0: 1, 1: 0, 2: 300 });                                               // cymbals: nothing below 300 Hz
  // ---- 3. Bass: rolling offbeat 16ths on the chord root (classic progressive house), ducked by the kick
  const bass = track('midi', 'Bass', '#4fd1ff'); P.setInstrument(bass, 32); preset(bass, 32, 'Round Saw Bass'); P.state.tracks[bass].vol = 1.25;
  const roll16 = (root) => rep(4, (k) => [[k + 0.25, 0.2, root, 0.85], [k + 0.5, 0.22, root + 12, 0.95], [k + 0.75, 0.2, root, 0.8]]);
  const bassLine = rep(4, (c) => rep(2, (h) => roll16(ROOT[c]).map(([bt, l, p, v]) => [bt + (c * 2 + h) * 4, l, p, v])));
  for (const [a, n] of [[9, 16], [33, 24]]) clip(bass, a, n, 8, bassLine, 'Rolling Bass');
  clip(bass, 57, 4, 0, bassLine.filter((n) => n[0] < 16), 'Bass Outro');
  dev(bass, 10, { 0: kick + 1, 1: -32, 2: 14, 3: 2, 4: 160 });                         // Autoduck from the Kick track
  dev(bass, 0, { 0: 5 });                                                              // Utility: +5 dB (the preset is quiet)
  dev(bass, 31, { 0: 1, 1: 0, 2: 30, 15: 1, 16: 3, 17: 280, 18: -3, 19: 1.2 });         // tidy low end, less boxiness
  // ---- 4. Pluck arp (GMS "Pluck Arp"): 16th arpeggio over the chord, one octave up, with a dotted-8th delay
  const pl = track('midi', 'Pluck Arp', '#7cf29c'); P.setInstrument(pl, 31); preset(pl, 31, 'Pluck Arp'); P.state.tracks[pl].vol = 1.0;
  const ORD = [0, 1, 2, 3, 2, 1, 2, 3, 0, 1, 2, 3, 2, 3, 1, 2];
  const arp = rep(4, (c) => rep(2, (h) => ORD.map((o, k) => [(c * 2 + h) * 4 + k / 4, 0.22, CH[c][o] + 12, k % 4 === 0 ? 0.9 : 0.7])));
  for (const [a, n] of [[17, 8], [25, 8], [33, 24]]) clip(pl, a, n, 8, arp, 'Arp');
  dev(pl, 0, { 0: 4 });
  dev(pl, 10, { 0: kick + 1, 1: -30, 2: 9, 3: 2, 4: 180 });
  dev(pl, 17, { 1: 3, 5: 3, 9: 0.38, 12: 2400, 19: 0.22 });                          // Delay: 3/16 (dotted 8th), filtered repeats
  // ---- 5. Pad (SuperSaw "Warm Saw Pad") through an Auto Filter whose cutoff is automated (intro opening, breakdown, outro closing)
  const pad = track('midi', 'Pad', '#b28dff'); P.setInstrument(pad, 33); preset(pad, 33, 'Warm Saw Pad'); P.state.tracks[pad].vol = 0.16;
  const chords = rep(4, (c) => CH[c].map((p) => [c * 8, 7.9, p, 0.75]));
  clip(pad, 1, 64, 8, chords, 'Chords');
  const padF = dev(pad, 1, { 0: 0, 1: 900, 2: 0.9 });
  dev(pad, 10, { 0: kick + 1, 1: -34, 2: 10, 3: 3, 4: 220 });
  // ---- 6. Lead (SuperSaw "Anthem Lead"): the 8-bar hook, in the breakdown (softer) and both drops
  const ld = track('midi', 'Lead', '#ff6fb5'); P.setInstrument(ld, 33); preset(ld, 33, 'Anthem Lead'); P.state.tracks[ld].vol = 0.42;
  const hook = [[0, 1.5, 72], [1.5, 0.5, 68], [2, 1, 72], [3, 1, 75], [4, 2, 77], [6, 1, 75], [7, 1, 72], [8, 1.5, 73], [9.5, 0.5, 72], [10, 2, 68], [12, 1, 65], [13, 1, 68], [14, 2, 72],
    [16, 1.5, 75], [17.5, 0.5, 72], [18, 1, 75], [19, 1, 79], [20, 2, 80], [22, 1, 79], [23, 1, 75], [24, 1.5, 79], [25.5, 0.5, 77], [26, 2, 75], [28, 1, 70], [29, 1, 72], [30, 1, 75], [31, 1, 77]];
  const hk = (v) => hook.map(([bt, l, p]) => [bt, l * 0.95, p, v]);
  clip(ld, 25, 8, 8, hk(0.45), 'Hook (breakdown)');
  clip(ld, 33, 16, 8, hk(0.9), 'Hook');
  clip(ld, 49, 8, 8, hk(0.85).map(([bt, l, p, v]) => [bt, l, p + 12, v]), 'Hook +8va');
  dev(ld, 6, { 8: 1, 9: 3, 10: 3, 1: 0.32, 2: 0.18, 4: 5000 });                        // Echo synced 3/16
  // ---- 7. Riser: a held chord whose pitch climbs an octave over the last 4 bars of the breakdown
  const rs = track('midi', 'Riser', '#ffe066'); P.setInstrument(rs, 33); preset(rs, 33, 'Huge Pad Bright'); P.state.tracks[rs].vol = 0.2;
  clip(rs, 29, 4, 0, [[0, 15.9, 53, 0.8], [0, 15.9, 60, 0.8], [0, 15.9, 65, 0.8]], 'Riser');
  // ---- 7b. Noise riser: 4 bars of white noise (7.5 s = 4 bars at 128 BPM) through an Auto Filter that opens from 250 Hz to 18 kHz
  const nz = track('audio', 'Noise Riser', '#e0e0e0'); P.state.tracks[nz].vol = 0.3;
  const nc = await P.addAudioClip(nz, smp('NH Noise Riser'), bar(29)); if (nc) { nc.len = 4 * BAR; nc.fadeOut = 0; }
  const nzF = dev(nz, 1, { 0: 0, 1: 250, 2: 1.6 });
  // ---- returns: A = big reverb, B = tempo delay; sends
  for (const [k, ps] of [[65, { 0: 0.82, 1: 0.45, 2: 1, 3: 25, 6: 250 }], [66, { 1: 2, 5: 3, 9: 0.45, 19: 1 }]]) dev(k, k === 65 ? 7 : 17, ps);
  P.state.returns[0].name = 'Reverb'; P.state.returns[1].name = 'Delay';
  for (const [i, a, bb] of [[dr, 0.08, 0], [cy, 0.12, 0], [pl, 0.22, 0.18], [pad, 0.3, 0], [ld, 0.32, 0.2], [rs, 0.45, 0.15], [nz, 0.35, 0.2]]) { P.setSend(i, 0, a); P.setSend(i, 1, bb); }
  // ---- master: glue compression, gentle tilt, limiter
  dev(64, 24, { 0: 1, 1: 1, 2: 0, 3: -14, 4: 2, 7: 1 });
  dev(64, 5, { 0: -0.5 });
  // ---- automation lanes: pad cutoff (open in the intro, closed then opening in the breakdown, closing in the outro), riser pitch,
  //      master volume fade-out in the last 4 bars
  const auto = (target, label, pts) => { const t = P.addAutoTrack(target, label); const i = P.state.tracks.indexOf(t); t.clips = []; const c = P.addClip(i, 0, 64 * BAR); c.pts = pts.map(([b0, v]) => ({ t: Math.round(b0 * BAR), v })); P.clipChanged(i); return i; };
  const fNorm = (hz) => { const d = DEVICES[1].params[1]; return Math.log(hz / d.min) / Math.log(d.max / d.min); };
  auto({ kind: 4, strip: P.stripOf(pad), devId: P.state.tracks[pad].devices[padF].id, param: 1, def: DEVICES[1].params[1] }, 'Pad • Auto Filter Freq',
    [[0, fNorm(500)], [16, fNorm(6000)], [24, fNorm(9000)], [24.01, fNorm(700)], [32, fNorm(16000)], [56, fNorm(16000)], [64, fNorm(400)]]);
  auto({ kind: 4, strip: P.stripOf(nz), devId: P.state.tracks[nz].devices[nzF].id, param: 1, def: DEVICES[1].params[1] }, 'Noise Riser \u2022 Auto Filter Freq', [[0, fNorm(250)], [28, fNorm(250)], [32, fNorm(18000)], [64, fNorm(18000)]]);
  const pitchDef = { n: 'Pitch', min: -24, max: 24 };
  auto({ kind: 5, strip: P.stripOf(rs), param: 90, def: pitchDef }, 'Riser • Pitch', [[28, 0.5], [32, 0.75]]);
  auto({ kind: 0, strip: 'master', param: 0, def: P.AUTO_VOL }, 'Master • Volume', [[0, normOf(P.AUTO_VOL, 0.85)], [60, normOf(P.AUTO_VOL, 0.85)], [64, 0]]);
  // ---- section markers (the tutorial jumps between them)
  for (const [b0, n, c] of [[1, 'Intro', '#8f9bb3'], [9, 'Build', '#4fd1ff'], [17, 'Groove', '#7cf29c'], [25, 'Breakdown', '#b28dff'], [33, 'Drop', '#ff5f4a'], [49, 'Drop B', '#ff6fb5'], [57, 'Outro', '#8f9bb3']]) P.addMarker(bar(b0), n, c);
  P.state.loop = { on: false, start: 0, end: BAR * 2 };
  P.syncAll && P.syncAll();
  const text = P.serialize();
  let wav = null;
  if (render) {
    globalThis.__exportSecs = 124; const done = new Promise((r) => { globalThis.onExportDone = r; });
    eng.exportWav('/Exports/neon.wav', 64 * BAR, 128, 16); const r = await done; if (r.ok) { const u = new Uint8Array(await eng.readBytes('/Exports/neon.wav')); let bin = ''; for (let k = 0; k < u.length; k += 32768) bin += String.fromCharCode.apply(null, u.subarray(k, k + 32768)); wav = btoa(bin); }
  }
  return { text, tracks: P.state.tracks.map((t) => t.name + ':' + t.clips.length), wav };
}, renderTo);
fs.writeFileSync(path.join(root, 'demos/progressive-house.mdaw'), out.text);
console.log('demos/progressive-house.mdaw', out.text.length, 'bytes;', out.tracks.join(', '));
if (renderTo && out.wav) { fs.writeFileSync(renderTo, Buffer.from(out.wav, 'base64')); console.log('render ->', renderTo); }
await b.close(); server.close();
