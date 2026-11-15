// Arrangement after Live 12 (manual ch. 6) + theme adjustments, in a real browser (Chromium, landscape phone): the Overview strip (tap, double
// tap = whole Arrangement), Set / Previous / Next Locator, Optimize Height (H) / Width (W), and the theme sliders (brightness, tone, hue).
// Usage: node tools/arr-live-test.mjs [out-dir]     (screenshots: docs/redesign/arr)
import http from 'node:http'; import fs from 'node:fs'; import path from 'node:path'; import { fileURLToPath } from 'node:url'; import { createRequire } from 'node:module';
const { chromium } = createRequire(import.meta.url)('playwright');
const root = path.join(path.dirname(fileURLToPath(import.meta.url)), '..'), out = path.resolve(process.argv[2] || path.join(root, 'docs/redesign/arr')); fs.mkdirSync(out, { recursive: true });
const TYPES = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.png': 'image/png', '.svg': 'image/svg+xml', '.wasm': 'application/wasm' };
const server = http.createServer((q, r) => { const f = path.join(root, decodeURIComponent(new URL(q.url, 'http://x').pathname)); if (!f.startsWith(root) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { r.writeHead(404); r.end(); return; } r.writeHead(200, { 'content-type': TYPES[path.extname(f)] || 'application/octet-stream' }); fs.createReadStream(f).pipe(r); }).listen(0, '127.0.0.1');
await new Promise((ok) => server.once('listening', ok));
const b = await chromium.launch(); const page = await (await b.newContext({ viewport: { width: 1000, height: 455 }, deviceScaleFactor: 2 })).newPage();
const errs = []; page.on('pageerror', (e) => errs.push(e.message));
await page.addInitScript(() => localStorage.setItem('mobiledaw.prefs.v1', JSON.stringify({ folderAsked: true, haptics: false, floatPlugins: false })));
await page.goto(`http://127.0.0.1:${server.address().port}/index.html`); await page.waitForFunction(() => window.__dawBooted, null, { timeout: 15000 }); await page.waitForTimeout(900);
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
const ev = (fn, a) => page.evaluate(fn, a);
await ev(async () => {
  const P = await import('./js/project.js'), Th = await import('./js/theme.js');
  P.newProject(false); ['midi', 'drum', 'midi', 'midi', 'midi', 'midi'].forEach((k) => P.addTrack(k));
  const cols = ['#ff3fd2', '#1fd8b8', '#3dff3d', '#e6f53c', '#5aa2ff', '#ff8a3c'];
  P.state.tracks.forEach((t, i) => { t.color = cols[i % 6]; for (let q = 0; q < 3; q++) { const c = P.addClip(i, (q * 6 + i) * P.BAR, 4 * P.BAR); for (let n = 0; n < 16; n++) c.notes.push({ t: n * 240, d: 200, p: 48 + ((n * 5 + i) % 12), v: 0.8 }); } P.clipChanged(i); });
  Th.applyTheme('middark'); if (!document.getElementById('view-arr').classList.contains('shut')) document.querySelector('#btn-shut').click(); P.selectTrack(0); P.emit('all');
});
await page.waitForTimeout(700);
const cv = page.locator('#arr canvas').first(), bx = await cv.boundingBox();
const geo = () => ev(async () => { const T = await import('./js/timeline.js'), P = await import('./js/project.js'); const r = document.querySelector('#arr canvas').getBoundingClientRect(); return { w: r.width, cw: r.width - T.headWidth(), h: r.height, ppt: T.view.ppt, x0: T.view.x0, rowH: T.view.rowH, marks: P.state.markers.length, tick: P.currentTick() }; });
const g0 = await geo(), K = bx.width / g0.w;   // the app may be zoomed: canvas units -> screen px
const tap = async (x, y) => { await page.mouse.click(bx.x + x * K, bx.y + y * K); await page.waitForTimeout(650); };
await page.screenshot({ path: path.join(out, 'arr-middark.png') });
// Set Locator: a locator at the insert marker; Previous / Next Locator jump between them
await ev(async () => { const P = await import('./js/project.js'); P.seekTo(4 * P.BAR); }); await tap(g0.cw + 20, 20); const g1 = await geo();
await ev(async () => { const P = await import('./js/project.js'); P.seekTo(9 * P.BAR); }); await tap(g0.cw + 20, 20); const g2 = await geo();
await tap(g0.cw + 6 + 30 + 3 + 12, 20); const g3 = await geo();
ok(g1.marks === g0.marks + 1 && g2.marks === g0.marks + 2 && g3.tick < g2.tick, `Set adds a locator at the insert marker (${g0.marks} -> ${g2.marks}); Previous Locator jumps back (tick ${Math.round(g2.tick)} -> ${Math.round(g3.tick)})`);
// Optimize Width (W) and Height (H)
await tap(g0.cw - 177 + 9, g0.h - 9); await page.waitForTimeout(700); const gw = await geo();
await tap(g0.cw - 198 + 9, g0.h - 9); await page.waitForTimeout(700); const gh = await geo();
const fits = await ev(async () => { const T = await import('./js/timeline.js'), P = await import('./js/project.js'); const end = P.state.tracks.reduce((m, t) => t.clips.reduce((a, c) => Math.max(a, c.start + c.len), m), 0); const r = document.querySelector('#arr canvas').getBoundingClientRect(); return { endPx: (end - T.view.x0) * T.view.ppt, cw: r.width - T.headWidth(), lastTop: T.probe(0, P.state.tracks.length - 1).top, h: r.height }; });
ok(Math.abs(gw.ppt - g0.ppt) > 1e-6 && fits.endPx <= fits.cw + 2 && fits.endPx > fits.cw * 0.7, `W fits the Arrangement into the width (end at ${Math.round(fits.endPx)} of ${Math.round(fits.cw)} px)`);
ok(gh.rowH !== gw.rowH && fits.lastTop < fits.h - 30, `H fits the tracks into the height (row ${Math.round(gw.rowH)} -> ${Math.round(gh.rowH)} px, last track at ${Math.round(fits.lastTop)} of ${Math.round(fits.h)})`);
// Overview: drag up / down zooms, double tap = the whole Arrangement
await page.mouse.move(bx.x + 200 * K, bx.y + 4 * K); await page.mouse.down(); await page.mouse.move(bx.x + 200 * K, bx.y + 70 * K, { steps: 6 }); await page.mouse.up(); await page.waitForTimeout(500); const gz = await geo();
await page.mouse.click(bx.x + 200 * K, bx.y + 4 * K); await page.waitForTimeout(90); await page.mouse.click(bx.x + 200 * K, bx.y + 4 * K); await page.waitForTimeout(900); const gf = await geo();
ok(gz.ppt > gh.ppt * 1.5 && Math.abs(gf.ppt - gw.ppt) / gw.ppt < 0.05, `Overview: dragging down zooms in (${(gz.ppt / gh.ppt).toFixed(2)}x), a double tap shows the whole Arrangement again`);
await page.screenshot({ path: path.join(out, 'arr-fit.png') });
// theme adjustments on top of a theme: brightness lifts the greys, tone tints them, hue turns the accent; reset restores the theme exactly
const tok = () => ev(() => { const s = getComputedStyle(document.documentElement); return { bg: s.getPropertyValue('--bg').trim(), acc: s.getPropertyValue('--accent').trim() }; });
const t0 = await tok();
for (const [name, adj, file] of [['brightness +50 %', { brightness: 0.5 }, 'theme-bright'], ['tone warm', { brightness: 0, tone: 1 }, 'theme-warm'], ['tone cool + hue', { tone: -1, hue: 150, intensity: -0.3 }, 'theme-cool-hue'], ['contrast', { tone: 0, hue: 0, intensity: 0, contrast: 0.8 }, 'theme-contrast']]) {
  await ev(async (a) => { const Th = await import('./js/theme.js'); Th.setThemeAdjust(a); }, adj); await page.waitForTimeout(350); await page.screenshot({ path: path.join(out, file + '.png') });
  const t = await tok(); console.log('     ' + name + ': --bg ' + t0.bg + ' -> ' + t.bg + ', --accent ' + t0.acc + ' -> ' + t.acc); if (name.startsWith('brightness')) ok(parseInt(t.bg.slice(1, 3), 16) > parseInt(t0.bg.slice(1, 3), 16) + 15, 'brightness lifts the background'); if (name.startsWith('tone warm')) ok(parseInt(t.bg.slice(1, 3), 16) > parseInt(t.bg.slice(5, 7), 16), 'warm tone: more red than blue in the greys'); if (name.includes('hue')) ok(t.acc !== t0.acc, 'hue turns the accent colour');
}
await ev(async () => { const Th = await import('./js/theme.js'); Th.setThemeAdjust({ tone: 0, brightness: 0, contrast: 0, intensity: 0, hue: 0 }); }); const t9 = await tok();
ok(t9.bg === t0.bg && t9.acc === t0.acc, 'reset: the theme is back exactly');
for (const th of ['dark', 'light', 'midlight', 'live', 'lightlc', 'medc']) { await ev(async (th) => { const Th = await import('./js/theme.js'); Th.applyTheme(th); }, th); await page.waitForTimeout(350); await page.screenshot({ path: path.join(out, 'arr-' + th + '.png') }); }
ok(!errs.length, 'no page errors' + (errs.length ? ': ' + errs.slice(0, 3).join(' | ') : ''));
await b.close(); server.close();
console.log(bad ? `\narr-live-test: ${bad} FAILED` : '\narr-live-test: all passed'); process.exit(bad ? 1 : 0);
