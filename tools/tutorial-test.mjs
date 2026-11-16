// The interactive tutorial (js/tutorial.js) in the real app (Playwright + Chromium, landscape phone size): every step of every chapter
// finds its control on screen (a spotlight inside the viewport, the card inside the viewport and not covering the spotlight), the practice
// steps wait for the action and go on by themselves once it is done (Browser, Play, Mixer, + Track, the drum beat exercise), Kembali / Lewati
// work, Menu -> Tutorial interaktif opens the chapter list, and the first-start offer stays away while tests drive the page.
// Usage: node tools/tutorial-test.mjs [--shots]   (part of tools/run-tests.mjs)
import http from 'node:http'; import fs from 'node:fs'; import path from 'node:path'; import { createRequire } from 'node:module'; import { fileURLToPath } from 'node:url';
const root = path.join(path.dirname(fileURLToPath(import.meta.url)), '..');
const { chromium } = createRequire(import.meta.url)('playwright');
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
const T = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.wasm': 'application/wasm' };
const server = http.createServer((q, r) => { const f = path.join(root, decodeURIComponent(new URL(q.url, 'http://x').pathname)); if (!fs.existsSync(f) || fs.statSync(f).isDirectory()) { r.writeHead(404); r.end(); return; } r.writeHead(200, { 'content-type': T[path.extname(f)] || 'application/octet-stream' }); fs.createReadStream(f).pipe(r); }).listen(0);
await new Promise((o) => server.once('listening', o));
const b = await chromium.launch(), page = await (await b.newContext({ viewport: { width: 915, height: 412 }, deviceScaleFactor: 1, hasTouch: true })).newPage();
const errs = []; page.on('pageerror', (e) => errs.push(e.message));
await page.addInitScript(() => localStorage.setItem('mobiledaw.prefs.v1', JSON.stringify({ folderAsked: true, haptics: false, floatPlugins: false })));
await page.goto(`http://127.0.0.1:${server.address().port}/index.html`); await page.waitForFunction(() => window.__dawBooted); await page.waitForTimeout(2000);
ok(!(await page.$('.tut-offer')), 'no first-start offer while automated tests drive the page');
const shots = process.argv.includes('--shots');
// 1. every step: spotlight and card placement
const geo = await page.evaluate(async () => {
  const TU = await import('./js/tutorial.js'), P = await import('./js/project.js');
  P.newProject(false); P.addTrack('midi'); P.setInstrument(0, 30); P.addTrack('drum');
  const out = []; const wait = (ms) => new Promise((o) => setTimeout(o, ms));
  for (const c of TU.CHAPTERS) {
    TU.startTutorial(c.id);
    for (let k = 0; k < c.steps.length; k++) {
      await wait(260);
      const ring = document.querySelector('.tut-ring'), card = document.querySelector('.tut-card'), s = c.steps[k];
      const rr = ring && ring.style.display !== 'none' ? ring.getBoundingClientRect() : null, cr = card.getBoundingClientRect();
      const inView = (r) => r.left >= -1 && r.top >= -1 && r.right <= innerWidth + 1 && r.bottom <= innerHeight + 1;
      const cover = rr ? Math.max(0, Math.min(rr.right, cr.right) - Math.max(rr.left, cr.left)) * Math.max(0, Math.min(rr.bottom, cr.bottom) - Math.max(rr.top, cr.top)) / Math.max(1, rr.width * rr.height) : 0;
      out.push({ ch: c.id, k, t: s.t, wantsTarget: !!s.el, spot: !!rr, spotIn: rr ? inView(rr) : true, cardIn: inView(cr), cover: +cover.toFixed(2) });
      // a practice step: skip it here (part 2 does them for real)
      const btn = [...card.querySelectorAll('.b')].find((x) => x.textContent === 'Lewati') || card.querySelector('.tut-next');
      btn.click();
      if (k === c.steps.length - 1) break;
      const curT = document.querySelector('.tut-card h4'); if (!curT) break;
    }
    TU.stopTutorial();
  }
  return out;
});
const missing = geo.filter((g) => g.wantsTarget && !g.spot), outside = geo.filter((g) => !g.spotIn || !g.cardIn), covered = geo.filter((g) => g.cover > 0.35);
ok(geo.length >= 45, 'the tutorial has ' + geo.length + ' steps in ' + new Set(geo.map((g) => g.ch)).size + ' chapters');
ok(!missing.length, 'every step finds its control on screen' + (missing.length ? ': missing ' + missing.map((g) => g.ch + '#' + g.k + ' ' + g.t).join(', ') : ''));
ok(!outside.length, 'spotlight and card inside the screen' + (outside.length ? ': ' + outside.map((g) => g.ch + '#' + g.k).join(', ') : ''));
ok(!covered.length, 'the card never hides the control it explains' + (covered.length ? ': ' + covered.map((g) => g.ch + '#' + g.k + ' ' + g.cover).join(', ') : ''));
// 2. practice steps done for real: the top-bar chapter (Browser, Play, Mixer) and the beat exercise
// page coordinates are layout pixels (js/uiscale.js scales the UI by K); the mouse works in screen pixels
const K = () => page.evaluate(async () => (await import('./js/uiscale.js')).emuK());
const tapRing = async () => { await page.waitForTimeout(350); const k = await K(), r = await page.evaluate(() => { const e = document.querySelector('.tut-ring').getBoundingClientRect(); return { x: e.left + e.width / 2, y: e.top + e.height / 2 }; }); await page.mouse.click(r.x * k, r.y * k); };
const title = () => page.evaluate(() => (document.querySelector('.tut-card h4') || {}).textContent || '');
const next = async () => { await page.evaluate(() => document.querySelector('.tut-next').click()); await page.waitForTimeout(250); };
const waitTitle = async (t, ms = 4000) => { for (let k = 0; k < ms / 100; k++) { if ((await title()) === t) return true; await page.waitForTimeout(100); } return false; };
await page.evaluate(async () => { (await import('./js/tutorial.js')).startTutorial('top'); }); await page.waitForTimeout(300);
if (shots) await page.screenshot({ path: path.join(root, 'docs/redesign/tutorial/01-welcome.png') });
await next();                                                       // -> Browser (practice)
const nextOff = await page.evaluate(() => document.querySelector('.tut-next').disabled);
await page.click('#btn-browser'); const a1 = await waitTitle('Tutup browser');
ok(nextOff && a1, 'practice step: Lanjut waits; tapping the real Browser button moves on by itself');
await page.click('#btn-browser'); await waitTitle('Tempo (BPM)');
for (let k = 0; k < 5; k++) await next();                            // -> Play
const atPlay = await title(); if (shots) await page.screenshot({ path: path.join(root, 'docs/redesign/tutorial/02-play.png') });
await tapRing(); const a2 = await waitTitle('Berhenti'); await tapRing(); const a3 = await waitTitle('Rekam');
ok(atPlay === 'Play / pause' && a2 && a3, 'Play then stop through the spotlight: both practice steps complete (' + [atPlay, a2, a3, await title()].join(' / ') + ')');
for (let k = 0; k < 4; k++) await next();                            // -> Arrange / Mixer
await page.click('[data-view="mix"]'); const a4 = await waitTitle('Mixer');
if (shots) await page.screenshot({ path: path.join(root, 'docs/redesign/tutorial/03-mixer.png') });
await next(); await page.click('[data-view="arr"]'); const a5 = await waitTitle('Menu proyek');
ok(a4 && a5, 'Mixer and back to Arrange as practice steps');
await page.evaluate(() => [...document.querySelectorAll('.tut-card .b')].find((x) => x.textContent === 'Kembali').click()); await page.waitForTimeout(200);
ok((await title()) === 'Kembali ke Arrange', 'Kembali goes one step back');
await page.evaluate(() => [...document.querySelectorAll('.tut-card .b')].find((x) => x.textContent === 'Tutup').click()); await page.waitForTimeout(200);
ok(!(await page.$('.tut')), 'Tutup closes the tutorial');
// the beat exercise
await page.evaluate(async () => { const P = await import('./js/project.js'); P.newProject(false); (await import('./js/tutorial.js')).startTutorial('beat'); }); await page.waitForTimeout(300);
await next(); const n0 = await page.evaluate(async () => (await import('./js/project.js')).state.tracks.length);
await tapRing(); await page.waitForTimeout(700);   // menus ignore taps right after they open (ghost-tap guard)
const drumItem = await page.evaluate(() => { const it = [...document.querySelectorAll('#layer .mi')].find((x) => /Track drum/.test(x.textContent)); if (it) { const r = it.getBoundingClientRect(); return { x: r.left + 20, y: r.top + r.height / 2 }; } return null; });
if (drumItem) { const k = await K(); await page.mouse.click(drumItem.x * k, drumItem.y * k); }
const b1 = await waitTitle('2. Klip pola');
if (shots) await page.screenshot({ path: path.join(root, 'docs/redesign/tutorial/04-beat-clip.png') });
await page.waitForTimeout(350);   // the spotlight ring slides to its new target (0.15 s transition)
const lane = await page.evaluate(() => { const e = document.querySelector('.tut-ring').getBoundingClientRect(); return { x: e.left + e.width * 0.3, y: e.top + e.height / 2 }; });   // away from the screen edge (edge guard for the system back swipe)
const kk = await K();
// a double tap under a loaded machine can be read as two taps: try again (the app's own double-tap window, Settings -> Sentuhan)
const dbl = async (x, y, want) => { for (let k = 0; k < 4; k++) { await page.mouse.dblclick(x * kk, y * kk); if (await waitTitle(want, 2500)) return true; await page.waitForTimeout(500); } return false; };
const b2 = await dbl(lane.x, lane.y, '3. Buka editor step'); 
await page.waitForTimeout(400); const b3 = await dbl(lane.x + 8, lane.y, '4. Isi step');
await page.evaluate(async () => { const P = await import('./js/project.js'); const i = P.state.tracks.findIndex((t) => t.kind === 'drum'); if (i < 0 || !P.state.tracks[i].clips[0]) return; const c = P.state.tracks[i].clips[0]; for (const t of [0, 960, 1920]) c.notes.push({ t, d: 240, p: 36, v: 0.8 }); P.clipChanged(i); });
const b4 = await waitTitle('5. Putar');
ok(n0 >= 0 && b1 && b2 && b3 && b4, `beat exercise: + Track -> Drum (${b1}), double tap the lane = clip (${b2}), double tap the clip = step editor (${b3}), steps placed (${b4})`);
await page.evaluate(async () => { (await import('./js/tutorial.js')).stopTutorial(); const { eng } = await import('./js/engine.js'); eng.stop && eng.stop(); });
// 3. the menu entry and the chapter list
await page.click('#btn-menu'); await page.waitForTimeout(700);
const item = await page.evaluate(() => { const it = [...document.querySelectorAll('#layer .mi')].find((x) => /Tutorial interaktif/.test(x.textContent)); if (!it) return false; it.click(); return true; });
await page.waitForTimeout(400);
const list = await page.evaluate(() => [...document.querySelectorAll('.tut-list .b')].map((x) => x.textContent));
if (shots) await page.screenshot({ path: path.join(root, 'docs/redesign/tutorial/05-chapters.png') });
ok(item && list.length === 5, 'Menu -> Tutorial interaktif lists the chapters: ' + list.join(' | '));
ok(!errs.length, 'no page errors' + (errs.length ? ': ' + errs.join(' | ') : ''));
await b.close(); server.close();
console.log(bad ? `\n${bad} FAILED` : '\ntutorial-test: all passed'); process.exit(bad ? 1 : 0);
