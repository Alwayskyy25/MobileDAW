// The loading window (js/loadprog.js) while a project opens (Playwright + Chromium): a project with 12 audio clips (6 stretched / pitched,
// 2 warped onto a swing grid) and a drum kit is saved, then opened through the project list. The window appears, its bar moves forward,
// its labels name the steps (samples, stretch / pitch renders, clips fitted to the tempo), its count reaches the total, and it is gone
// once the project is open. Usage: node tools/loadprog-test.mjs   (part of tools/run-tests.mjs)
import http from 'node:http'; import fs from 'node:fs'; import path from 'node:path'; import { createRequire } from 'node:module'; import { fileURLToPath } from 'node:url';
const root = path.join(path.dirname(fileURLToPath(import.meta.url)), '..');
const { chromium } = createRequire(import.meta.url)('playwright');
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
const T = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.wasm': 'application/wasm' };
const server = http.createServer((q, r) => { const f = path.join(root, decodeURIComponent(new URL(q.url, 'http://x').pathname)); if (!fs.existsSync(f) || fs.statSync(f).isDirectory()) { r.writeHead(404); r.end(); return; } r.writeHead(200, { 'content-type': T[path.extname(f)] || 'application/octet-stream' }); fs.createReadStream(f).pipe(r); }).listen(0);
await new Promise((o) => server.once('listening', o));
const b = await chromium.launch(), page = await (await b.newContext({ viewport: { width: 1280, height: 800 } })).newPage();
const errs = []; page.on('pageerror', (e) => errs.push(e.message));
await page.addInitScript(() => localStorage.setItem('mobiledaw.prefs.v1', JSON.stringify({ folderAsked: true, haptics: false })));
await page.goto(`http://127.0.0.1:${server.address().port}/index.html`); await page.waitForFunction(() => window.__dawBooted); await page.waitForTimeout(800);
const r = await page.evaluate(async () => {
  const P = await import('./js/project.js'), { eng } = await import('./js/engine.js'), PJ = await import('./js/projects.js');
  const SR = 48000, wav = (sec, hz) => { const n = Math.round(SR * sec), d = new DataView(new ArrayBuffer(44 + n * 2)), w = (o, s) => [...s].forEach((c, i) => d.setUint8(o + i, c.charCodeAt(0)));
    w(0, 'RIFF'); d.setUint32(4, 36 + n * 2, true); w(8, 'WAVEfmt '); d.setUint32(16, 16, true); d.setUint16(20, 1, true); d.setUint16(22, 1, true); d.setUint32(24, SR, true); d.setUint32(28, SR * 2, true); d.setUint16(32, 2, true); d.setUint16(34, 16, true); w(36, 'data'); d.setUint32(40, n * 2, true);
    for (let i = 0; i < n; i++) d.setInt16(44 + 2 * i, Math.round(9000 * Math.sin(2 * Math.PI * hz * i / SR) * (i % 12000 < 600 ? 2 : 1)), true); return d.buffer; };
  for (let k = 0; k < 12; k++) await eng.saveBytes('/Samples/LP/loop' + k + '.wav', wav(4, 110 * (1 + k % 5)));
  P.newProject(false); P.addTrack('audio'); const i = P.state.tracks.length - 1;
  for (let k = 0; k < 12; k++) { const c = await P.addAudioClip(i, '/Samples/LP/loop' + k + '.wav', k * 3840); if (k % 2 === 0) c.warp = { beats: 0, ratio: 1.1, rev: false, pitch: k % 4 ? 2 : 0 }; if (k === 1 || k === 3) { c.srcBpm = 110; c.warpAuto = true; c.swing = 0.5; } }
  await P.ensureBakes(); P.state.projectName = 'LoadProg'; eng.writeText('/Projects/LoadProg.mdaw', P.serialize());
  P.newProject(false);
  const seen = []; let maxDone = 0, maxTotal = 0;
  const obs = new MutationObserver(() => { const el = document.querySelector('.ldp'); if (!el) return; const l = el.querySelector('.ldp-l').textContent, c = el.querySelector('.ldp-c').textContent, w = parseFloat(el.querySelector('.ldp-bar i').style.width) || 0;
    seen.push([l, c, w]); const m = /(\d+) \/ (\d+)/.exec(c); if (m) { maxDone = Math.max(maxDone, +m[1]); maxTotal = Math.max(maxTotal, +m[2]); } });
  obs.observe(document.body, { childList: true, subtree: true, characterData: true, attributes: true });
  window.__ldpShot = new Promise((o) => { const mo = new MutationObserver(() => { const e = document.querySelector('.ldp .ldp-c'); if (e && /\d+ \/ \d+/.test(e.textContent) && parseFloat(document.querySelector('.ldp-bar i').style.width) > 40) { mo.disconnect(); o(true); } }); mo.observe(document.body, { subtree: true, childList: true, characterData: true, attributes: true }); });
  const res = await PJ.openProject('LoadProg'); await new Promise((o) => setTimeout(o, 100)); obs.disconnect();
  return { ok: res.ok, clips: P.state.tracks[0] ? P.state.tracks[0].clips.length : 0, seen: seen.length, labels: [...new Set(seen.map((x) => x[0].split(':')[0]))], widths: seen.map((x) => x[2]), maxDone, maxTotal, gone: !document.querySelector('.ldp'), pending: P.bakesPending() };
});
ok(r.ok && r.clips === 12, 'the project opened through the project list (' + r.clips + ' clips)');
ok(r.seen > 3, 'the loading window showed (' + r.seen + ' updates)');
ok(r.labels.some((l) => /Memuat sample klip/.test(l)) && r.labels.some((l) => /Merender stretch/.test(l)) && r.labels.some((l) => /tempo/.test(l)), 'its labels name the steps: ' + r.labels.join(' | '));
ok(r.widths.some((w) => w > 0 && w < 100) && Math.max(...r.widths) >= 90, 'the bar moves forward (' + Math.round(Math.min(...r.widths.filter((w) => w > 0))) + ' .. ' + Math.round(Math.max(...r.widths)) + ' %)');
ok(r.maxTotal > 0 && r.maxDone >= r.maxTotal - 1, 'the count reaches its total (' + r.maxDone + ' / ' + r.maxTotal + ')');
ok(r.gone && r.pending === 0, 'the window is gone once the project is open, every swing clip already rendered');
ok(!errs.length, 'no page errors' + (errs.length ? ': ' + errs.join(' | ') : ''));
await b.close(); server.close();
console.log(bad ? `\n${bad} FAILED` : '\nloadprog-test: all passed'); process.exit(bad ? 1 : 0);
