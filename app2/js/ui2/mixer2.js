// MobileDAW 2's mixer, its only mixer view: one strip per track, then the master, each with its name in the track colour, a tall fader with
// a round thumb, its level meter, a pan ring, Mute / Solo / Rekam, and the track's kind at the bottom (tap = select, the strip lights up).
import { h, clamp, menu, modal, knob } from '../ui.js';
import * as P from '../project.js';
import { MASTER, RET0 } from '../engine.js';
import { checkpoint } from '../history.js';
import { gestureTiming } from '../gesture.js';
const { state } = P;
const VMAX = 1.4, meterFrac = (lv) => clamp((20 * Math.log10(Math.max(lv, 1e-4)) + 60) / 66, 0, 1);
let root = null, list = null, strips = [], sig = '';

const ICON = {
  midi: '<path d="M4 5h16v14H4zM8 5v14M12 5v14M16 5v14"/><path d="M6.6 5v7.5M10.6 5v7.5M14.6 5v7.5M18.2 5v7.5" stroke-width="2.4"/>',
  drum: '<path d="M4 5h4v4H4zM10 5h4v4h-4zM16 5h4v4h-4zM4 11h4v4H4zM10 11h4v4h-4zM16 11h4v4h-4zM4 17h4v2H4zM10 17h4v2h-4zM16 17h4v2h-4z"/>',
  audio: '<path d="M3 12h2M6.5 8v8M9.5 5v14M12.5 9v6M15.5 6.5v11M18.5 10v4"/>',
  bus: '<path d="M5 19V10M9.5 19V5M14 19v-7M18.5 19V8"/>',
};
const svg = (d) => `<svg viewBox="0 0 24 24" aria-hidden="true">${d}</svg>`;
// the tracks, then the Master (the return channels A / B live in "Mixer lengkap", not here)
const order = () => { const o = []; state.tracks.forEach((t, i) => { if (t.kind !== 'auto') o.push(i); }); o.push(MASTER); return o; };
const colorOf = (i) => (i === MASTER ? (state.master.color || '#c9c9cf') : i >= RET0 ? (state.returns[i - RET0].color || '#a89be0') : state.tracks[i].color);
const nameOf = (i) => (i === MASTER ? 'Master' : P.tr(i).name);
const dbTxt = (v) => (v > 1e-4 ? (20 * Math.log10(v)).toFixed(1) : '-∞');

// a control dragged along one axis (relative), double tap = its default
function drag(el, axis, get, set, reset, span) {
  let d = null, last = 0;
  el.addEventListener('pointerdown', (e) => {
    e.stopPropagation(); el.setPointerCapture(e.pointerId);
    const now = performance.now(); if (now - last < gestureTiming().dtapMs) { checkpoint(); reset(); last = 0; return; } last = now;
    d = { p: axis === 'y' ? e.clientY : e.clientX, u: get(), moved: false };
  });
  el.addEventListener('pointermove', (e) => {
    if (!d) return; const q = axis === 'y' ? d.p - e.clientY : e.clientX - d.p, sp = span(); if (!d.moved && Math.abs(q) < 3) return;
    if (!d.moved) { d.moved = true; checkpoint(); } set(clamp(d.u + q / sp, 0, 1));
  });
  const end = () => { d = null; }; el.addEventListener('pointerup', end); el.addEventListener('pointercancel', end);
}

function strip(i) {
  const isT = i < MASTER, t = P.tr(i), kind = isT ? t.kind : 'bus';
  const name = h('div', { class: 'm2-name' }), fad = h('div', { class: 'm2-fad', 'aria-label': 'Volume' }, h('i', { class: 'trk' }), h('i', { class: 'fill' }), h('em'));
  const met = h('div', { class: 'm2-met' }, h('i'), h('i')), db = h('div', { class: 'm2-db' });
  const pan = h('div', { class: 'm2-pan', 'aria-label': 'Pan' }, h('i'));
  const on = h('button', { class: 'm2-b m2-m', 'aria-label': 'Mute' }, 'M'), solo = h('button', { class: 'm2-b m2-s', 'aria-label': 'Solo' }, 'S'), rec = h('button', { class: 'm2-b m2-r', 'aria-label': 'Rekam' }, h('i'));
  const kindB = h('button', { class: 'm2-kind', 'aria-label': 'Pilih track' }); kindB.innerHTML = svg(ICON[kind] || ICON.midi);
  const el = h('div', { class: 'm2-strip' + (isT ? '' : ' bus') }, name, h('div', { class: 'm2-mid' }, fad, met), db, pan, h('div', { class: 'm2-row' }, ...(isT ? [on, solo, rec] : [])), kindB);
  el.style.setProperty('--tc', colorOf(i));
  drag(fad, 'y', () => Math.sqrt(clamp(P.tr(i).vol / VMAX, 0, 1)), (u) => { P.setVol(i, u * u * VMAX); paint(s); }, () => { P.setVol(i, 1); paint(s); }, () => Math.max(60, fad.clientHeight - 20));
  drag(pan, 'x', () => (clamp(P.tr(i).pan || 0, -1, 1) + 1) / 2, (u) => { P.setPan(i, Math.round((u * 2 - 1) * 100) / 100); paint(s); }, () => { P.setPan(i, 0); paint(s); }, () => 90);
  on.onclick = () => { P.toggleMute(i); paint(s); };
  solo.onclick = () => { P.toggleSolo(i); paint(s); };
  rec.onclick = () => { P.toggleArm(i); paint(s); };
  kindB.onclick = () => { P.selectTrack(i); };
  name.onclick = () => { P.selectTrack(i); if (isT) routeMenu(i, name.getBoundingClientRect()); };
  const s = { i, el, name, fad, met, db, pan, on, solo, rec, kindB, lv: [0, 0] };
  paint(s); return s;
}
// a track's routing (the name's menu): where it goes (Master or an FX track) and its two sends
const SEND = { n: 'Send', min: 0, max: 1, def: 0, unit: '%' };
const pct = (p, v) => Math.round(v * 100) + ' %';
function routeMenu(i, r) {
  const t = state.tracks[i]; if (!t) return;
  menu([{ label: 'KELUAR KE', dim: true }, { label: 'Master', check: !(t.out >= 0), fn: () => P.setOut(i, -1) },
    ...state.returns.map((rt, k) => ({ label: rt.name, check: t.out === k, fn: () => P.setOut(i, k) })),
    { sep: true }, { label: 'Send A / Send B\u2026', dim: !state.returns.length, fn: () => {
      const ks = state.returns.slice(0, 2).map((rt, k) => knob({ ...SEND, n: 'Send ' + 'AB'[k] }, () => (t.send || [0, 0])[k] || 0, (v) => P.setSend(i, k, v), pct));
      modal('Send \u2022 ' + t.name, h('div', { class: 'knobs' }, ...ks), [{ label: 'Tutup', go: true }]);
    } }], r.left, r.bottom + 4);
}
function paint(s) {
  const t = P.tr(s.i); if (!t) return;
  s.name.textContent = nameOf(s.i);
  const u = Math.sqrt(clamp(t.vol / VMAX, 0, 1)); s.fad.style.setProperty('--u', u.toFixed(4)); s.db.textContent = dbTxt(t.vol);
  s.pan.style.setProperty('--a', ((clamp(t.pan || 0, -1, 1)) * 135).toFixed(1) + 'deg');
  s.on.classList.toggle('on', !!t.mute); s.solo.classList.toggle('on', !!t.solo);
  if (s.i < MASTER) { const can = ['audio', 'midi', 'drum'].includes(t.kind); s.rec.classList.toggle('on', !!t.arm); s.rec.disabled = !can; }
  s.el.classList.toggle('sel', state.sel.track === s.i); s.el.classList.toggle('muted', !!t.mute);
}

export function build() {
  if (!root) return;
  strips = order().map(strip); list.replaceChildren(...strips.map((s) => s.el));
  sig = order().join(',');
}
export function update() { if (!root) return; if (order().join(',') !== sig) build(); for (const s of strips) { s.el.style.setProperty('--tc', colorOf(s.i)); paint(s); } }
export function frame() {
  if (!root || root.hidden) return;
  for (const s of strips) for (let ch = 0; ch < 2; ch++) {   // meters: instant attack, a gentle release
    const lv = (state.meters[s.i * 2 + ch] || 0) / 1000, f = meterFrac(lv); s.lv[ch] = f > s.lv[ch] ? f : Math.max(f, s.lv[ch] - 0.02);
    const bar = s.met.children[ch], hgt = (s.lv[ch] * 100).toFixed(1) + '%'; if (bar.style.height !== hgt) bar.style.height = hgt;
  }
}
export function show(on) { if (root) { root.hidden = !on; if (on) update(); } }
export const visible = () => !!root && !root.hidden;
export function initMixer2(host) {
  list = h('div', { class: 'm2-list' });
  root = h('section', { id: 'mix2', hidden: true }, list);
  host.append(root);
  P.onChange((w) => { if (w === 'tracks' || w === 'all') build(); else if (w === 'mix' || w === 'sel') update(); });
  build();
}
