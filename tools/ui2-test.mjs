// MobileDAW 2.0 main screen (js/ui2/shell.js), Playwright + Chromium with touch: the bottom bar, the left track column, the clip bubbles,
// the range menu's playhead commands, the main menu, and that switching back to 1.0 keeps the project.
// Usage: node tools/ui2-test.mjs   (part of tools/run-tests.mjs)
import http from 'node:http'; import fs from 'node:fs'; import path from 'node:path'; import { createRequire } from 'node:module'; import { fileURLToPath } from 'node:url';
const root = path.join(path.dirname(fileURLToPath(import.meta.url)), '..');
const { chromium } = createRequire(import.meta.url)('playwright');
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
const T = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.wasm': 'application/wasm', '.png': 'image/png' };
const server = http.createServer((q, r) => { const f = path.join(root, decodeURIComponent(new URL(q.url, 'http://x').pathname)); if (!fs.existsSync(f) || fs.statSync(f).isDirectory()) { r.writeHead(404); r.end(); return; } r.writeHead(200, { 'content-type': T[path.extname(f)] || 'application/octet-stream' }); fs.createReadStream(f).pipe(r); }).listen(0);
await new Promise((o) => server.once('listening', o));
const b = await chromium.launch({ args: ['--autoplay-policy=no-user-gesture-required'] });
const page = await (await b.newContext({ viewport: { width: 1150, height: 540 }, hasTouch: true })).newPage();
const errs = []; page.on('pageerror', (e) => errs.push(e.message));
await page.addInitScript(() => { if (!sessionStorage.getItem('u2t')) { sessionStorage.setItem('u2t', '1'); localStorage.setItem('mobiledaw.prefs.v1', JSON.stringify({ v: 9, folderAsked: true, haptics: false, tutorialOffered: true, fullscreen: false, ui: 'v2', uiScale: 1 })); } });
await page.goto(`http://127.0.0.1:${server.address().port}/index.html`); await page.waitForFunction(() => window.__dawBooted); await page.waitForTimeout(900);

// a small song: two synth tracks with one clip each
await page.evaluate(async () => {
  const P = await import('./js/project.js'); P.newProject(false);
  for (const n of ['Lead', 'Bass']) { P.addTrack('midi'); const i = P.state.tracks.length - 1; P.renameTrack(i, n); const c = P.addClip(i, P.BAR, P.BAR * 2); c.notes.push({ t: 0, d: 480, p: 60, v: 0.8 }); P.touchClips(i); }
  P.emit('all');
});
await page.waitForTimeout(500);
const st = await page.evaluate(() => ({ top: getComputedStyle(document.querySelector('#top')).display, bar: !!document.querySelector('#bar2'), rows: document.querySelectorAll('#tcol2 .tc-row').length, n: (window.__P = null, 0), shut: document.querySelector('#view-arr').classList.contains('shut') }));
const n = await page.evaluate(async () => (await import('./js/project.js')).state.tracks.length);
ok(st.top === 'none' && st.bar, `2.0: the 1.0 control bar is hidden, the bottom bar is there`);
ok(st.rows >= n, `the track column has a row per track (+ Master / FX rows): ${st.rows} rows for ${n} tracks`);
ok(st.shut, 'the arrangement starts with the whole screen (the device panel is closed)');

// bottom bar: play / metronome / loop
await page.click('#bar2 .play'); await page.waitForTimeout(300);
ok(await page.evaluate(async () => (await import('./js/project.js')).state.playing), 'Play in the bottom bar starts playback');
await page.click('#bar2 .play'); await page.waitForTimeout(200);
await page.click('#bar2 [aria-label="LOOP"]'); await page.waitForTimeout(120);
ok(await page.evaluate(async () => (await import('./js/project.js')).state.loop.on), 'LOOP switches the arrangement loop on');
await page.click('#bar2 [aria-label="RACK"]'); await page.waitForTimeout(300);
ok(!(await page.evaluate(() => document.querySelector('#view-arr').classList.contains('shut'))), 'RACK opens the device panel');
await page.click('#bar2 [aria-label="RACK"]'); await page.waitForTimeout(300);
ok(await page.evaluate(() => document.querySelector('#view-arr').classList.contains('shut')), 'RACK again closes it');

// tap a clip: the bubbles; Salin duplicates, Hapus deletes, undo brings it back
const clipAt = async (i) => page.evaluate(async (i) => { const TL = await import('./js/timeline.js'), P = await import('./js/project.js'); const c = P.state.tracks[i].clips[0], bx = TL.v2ClipBox(i, c.id), r = document.querySelector('#arr canvas').getBoundingClientRect(); return { x: r.left + bx.x + 20, y: r.top + bx.y + bx.h / 2, id: c.id }; }, i);
let p = await clipAt(0);
await page.touchscreen.tap(p.x, p.y); await page.waitForTimeout(700);
ok(await page.evaluate(() => !document.querySelector('#bub2').hidden && document.querySelectorAll('#bub2 .bub').length === 5), 'a tap on a clip shows 5 bubbles (Salin, Hapus, Snap, Edit, Lainnya)');
await page.click('#bub2 [aria-label="Salin"]'); await page.waitForTimeout(200);
ok(await page.evaluate(async () => (await import('./js/project.js')).state.tracks[0].clips.length === 2), 'Salin puts a copy right after the clip');
await page.click('#bub2 [aria-label="Hapus"]'); await page.waitForTimeout(200);
ok(await page.evaluate(async () => (await import('./js/project.js')).state.tracks[0].clips.length === 1 && document.querySelector('#bub2').hidden), 'Hapus deletes the clip and hides the bubbles');
await page.click('#bar2 [aria-label="Urungkan"]'); await page.waitForTimeout(200);
ok(await page.evaluate(async () => (await import('./js/project.js')).state.tracks[0].clips.length === 2), 'Undo in the bottom bar brings it back');

// the column: tap a tile = wide column (name, on / off, solo, pan, volume); on / off mutes the track
await page.evaluate(() => document.querySelector('#tcol2 .tc-row .tc-tile').click()); await page.waitForTimeout(400);
ok(await page.evaluate(() => document.documentElement.classList.contains('tc-wide') && getComputedStyle(document.querySelector('#tcol2 .tc-row .tc-ctl')).display === 'flex'), 'a tap on a track tile widens the column (controls shown)');
await page.evaluate(() => document.querySelector('#tcol2 .tc-row .tc-on').click()); await page.waitForTimeout(150);
ok(await page.evaluate(async () => (await import('./js/project.js')).state.tracks[0].mute === true), 'the on / off button mutes the track');
{ const v = await page.evaluate(() => { const r = document.querySelector('#tcol2 .tc-row .tc-vol').getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height / 2 }; });
  const v0 = await page.evaluate(async () => (await import('./js/project.js')).state.tracks[0].vol);
  await page.mouse.move(v.x, v.y); await page.mouse.down(); await page.mouse.move(v.x - 40, v.y, { steps: 6 }); await page.mouse.up(); await page.waitForTimeout(100);
  const v1 = await page.evaluate(async () => (await import('./js/project.js')).state.tracks[0].vol);
  ok(v1 < v0 - 0.05, `dragging the volume bar left turns the track down (${v0.toFixed(2)} -> ${v1.toFixed(2)})`); }
await page.evaluate(() => document.querySelector('#tcol2 .tc-row .tc-tile').click()); await page.waitForTimeout(300);

// range menu: copy the selection to the playhead
await page.evaluate(async () => {
  const P = await import('./js/project.js'); P.seekTo(P.BAR * 6);
});
const rr = await page.evaluate(async () => { const TL = await import('./js/timeline.js'), P = await import('./js/project.js'); const r = document.querySelector('#arr canvas').getBoundingClientRect(), g = TL.v2Geom(); const b1 = TL.v2ClipBox(1, P.state.tracks[1].clips[0].id); return { y: r.top + 12, x0: r.left + b1.x - 3, x1: r.left + b1.x + b1.w + 3 }; });
await page.mouse.move(rr.x0, rr.y); await page.mouse.down(); await page.mouse.move(rr.x1, rr.y, { steps: 10 }); await page.mouse.up(); await page.waitForTimeout(300);
const items = await page.evaluate(() => [...document.querySelectorAll('.menu .mi, .menu button, .ctx button, [role=menuitem]')].map((e) => e.textContent.trim()));
ok(items.includes('Salin ke playhead') && items.includes('Pindah ke playhead'), 'the ruler range menu offers Salin / Pindah ke playhead');
if (items.includes('Salin ke playhead')) {
  await page.waitForTimeout(500); await page.click('.menu .mi:text-is("Salin ke playhead")'); await page.waitForTimeout(250);   // (a menu ignores taps in its first moments)
  ok(await page.evaluate(async () => { const P = await import('./js/project.js'); return P.state.tracks[1].clips.some((c) => c.start === P.BAR * 6); }), 'Salin ke playhead puts a copy of the range at the playhead');
}

// main menu
await page.click('#logo2'); await page.waitForTimeout(400);
ok(await page.evaluate(() => document.querySelector('#mm2').classList.contains('open') && document.querySelectorAll('#mm2 .u2m-t').length === 5), 'the logo button opens the main menu (5 tabs)');
await page.click('.u2m-t[data-k=proyek]'); await page.waitForTimeout(150);
ok(await page.evaluate(() => /Info lagu/.test(document.querySelector('#mm2 .u2m-body').textContent)), 'PROYEK shows the song info');
await page.click('.u2m-x'); await page.waitForTimeout(350);
ok(!(await page.evaluate(() => document.querySelector('#mm2').classList.contains('open'))), 'the ✕ closes it');

// back to 1.0: the project is kept (autosave, reload)
const names = await page.evaluate(async () => (await import('./js/project.js')).state.tracks.map((t) => t.name).join(','));
await page.evaluate(async () => (await import('./js/ui2/shell.js')).switchUi('v1'));
await page.waitForTimeout(800); await page.waitForFunction(() => window.__dawBooted); await page.waitForTimeout(1500);
const after = await page.evaluate(async () => ({ top: getComputedStyle(document.querySelector('#top')).display, bar: !!document.querySelector('#bar2'), names: (await import('./js/project.js')).state.tracks.map((t) => t.name).join(','), ui: JSON.parse(localStorage.getItem('mobiledaw.prefs.v1')).ui }));
ok(after.ui === 'v1' && after.top !== 'none' && !after.bar, '1.0 again: the control bar is back, no 2.0 bar');
ok(after.names === names, `the project survives the switch (${after.names})`);
ok(!errs.length, 'no page errors' + (errs.length ? ': ' + errs.slice(0, 2).join(' | ') : ''));
await b.close(); server.close();
console.log(bad ? `\n${bad} FAILED` : '\nUI 2.0 OK'); process.exit(bad ? 1 : 0);
