// MobileDAW 2's module cards: one Material You card for every effect and instrument, built from the module's parameter list.
//   header   on / off switch (effects), the name (tap = swap the module), Preset (instruments), ⋯ (full editor, move, reset, delete)
//   tabs     the module's sections: FL Studio Mobile's own section layout for the FLM effects (flmlayout.js), the name prefixes of the
//            parameters for the instruments (Osc 1, Flt, LFO, ...), "Keluaran" (Mix / Gain) last
//   controls a round dial for a continuous value (drag up / down, double tap = default, long press = automation lane), a switch for
//            on / off, segmented buttons for a short list, a drop-down field for a long one; an envelope drawing above A / D / S / R
// The classic full editor (with its graphs and sample zones) stays one tap away in a floating window: nothing is lost.
import { h, clamp, menu, toNorm, fromNorm, toast } from '../ui.js';
import * as P from '../project.js';
import { INSTRUMENTS, DEVICES, fmtParam, NO_WET_FX, WET_PARAMS, MIX_P, FX_GROUPS } from '../defs.js';
import { FLM_LAYOUT } from '../flmlayout.js';
import { offerAutomation } from '../automate.js';
import { gestureTiming } from '../gesture.js';

const NS = 'http://www.w3.org/2000/svg';
const tabMem = new Map();                        // the open tab per card (device id / instrument track id), kept across re-renders
const A0 = -135, A1 = 135;
const pt = (r, a) => { const q = (a - 90) * Math.PI / 180; return [24 + r * Math.cos(q), 24 + r * Math.sin(q)]; };
const arc = (r, a, b) => { if (b - a < 0.5) return ''; const [x0, y0] = pt(r, a), [x1, y1] = pt(r, b); return `M${x0.toFixed(2)} ${y0.toFixed(2)}A${r} ${r} 0 ${b - a > 180 ? 1 : 0} 1 ${x1.toFixed(2)} ${y1.toFixed(2)}`; };
const svgEl = (tag, at) => { const e = document.createElementNS(NS, tag); for (const k in at) e.setAttribute(k, at[k]); return e; };
const isList = (p) => Array.isArray(p.items) && p.items.length > 1;
const isSwitch = (p) => p.fmt === 'onoff' || (p.min === 0 && p.max === 1 && p.step === 1 && !isList(p));

// ---- controls (each returns an element with .sync() that re-reads the value)
function dial(p, label, get, set, onLong) {
  const s = svgEl('svg', { viewBox: '0 0 48 48', 'aria-hidden': 'true' });
  const trk = svgEl('path', { class: 'trk', d: arc(19, A0, A1) }), val = svgEl('path', { class: 'val' }), dot = svgEl('circle', { class: 'dot', r: 3.2 });
  s.append(svgEl('circle', { class: 'disc', cx: 24, cy: 24, r: 13 }), trk, val, dot);
  const v = h('span', { class: 'v' }), el = h('div', { class: 'u2d', role: 'slider', 'aria-label': label, tabindex: 0 }, h('span', { class: 'n', title: p.n }, label), s, v);
  const bip = p.min < 0 && p.max > 0 && p.curve !== 'log';
  const paint = () => {
    const x = get(), n = toNorm(p, x), a = A0 + n * (A1 - A0), z = bip ? A0 + toNorm(p, 0) * (A1 - A0) : A0;
    val.setAttribute('d', arc(19, Math.min(a, z), Math.max(a, z)));
    const [dx, dy] = pt(13 - 5, a); dot.setAttribute('cx', dx.toFixed(2)); dot.setAttribute('cy', dy.toFixed(2));
    v.textContent = fmtParam(p, x); el.setAttribute('aria-valuetext', v.textContent);
  };
  let d = null, last = 0;
  el.addEventListener('pointerdown', (e) => {
    e.stopPropagation(); el.setPointerCapture(e.pointerId);
    d = { y: e.clientY, n: toNorm(p, get()), moved: false, fired: false, t: 0 };
    if (onLong) d.t = setTimeout(() => { if (d && !d.moved) { d.fired = true; onLong(el.getBoundingClientRect()); } }, gestureTiming().longMs);
  });
  el.addEventListener('pointermove', (e) => {
    if (!d || d.fired) return; const dy = d.y - e.clientY;
    if (!d.moved && Math.abs(dy) < 3) return;
    if (!d.moved) { d.moved = true; clearTimeout(d.t); el.classList.add('act'); }
    set(fromNorm(p, clamp(d.n + dy / 180, 0, 1))); paint();
  });
  const end = () => {
    if (!d) return; clearTimeout(d.t); el.classList.remove('act');
    if (!d.moved && !d.fired) { const now = performance.now(); if (now - last < gestureTiming().dtapMs) { set(p.def); paint(); last = 0; } else last = now; }
    d = null;
  };
  el.addEventListener('pointerup', end); el.addEventListener('pointercancel', end);
  el.addEventListener('keydown', (e) => {
    const k = { ArrowUp: 1, ArrowRight: 1, ArrowDown: -1, ArrowLeft: -1 }[e.key]; if (!k) return; e.preventDefault();
    set(fromNorm(p, clamp(toNorm(p, get()) + k * (p.step ? p.step / (p.max - p.min) : 0.01), 0, 1))); paint();
  });
  el.sync = () => { if (!d) paint(); };
  paint(); return el;
}
function toggle(p, label, get, set) {
  const sw = h('i', { class: 'u2sw' }), el = h('button', { class: 'u2t', role: 'switch' }, h('span', {}, label), sw);
  const paint = () => { const on = get() >= (p.min + p.max) / 2; el.classList.toggle('on', on); el.setAttribute('aria-checked', String(on)); };
  el.onclick = (e) => { e.stopPropagation(); set(get() >= (p.min + p.max) / 2 ? p.min : p.max); paint(); };
  el.sync = paint; paint(); return el;
}
function choice(p, label, get, set) {
  const items = p.items, val = (k) => p.min + k * (p.step || 1), idx = () => clamp(Math.round((get() - p.min) / (p.step || 1)), 0, items.length - 1);
  if (items.length <= 4 && items.every((x) => String(x).length <= 9)) {
    const bs = items.map((x, k) => h('button', { onclick: (e) => { e.stopPropagation(); set(val(k)); paint(); } }, String(x)));
    const el = h('div', { class: 'u2seg-f' }, h('span', { class: 'n' }, label), h('div', { class: 'u2seg' }, ...bs));
    const paint = () => { const k = idx(); bs.forEach((b, j) => b.classList.toggle('on', j === k)); };
    el.sync = paint; paint(); return el;
  }
  const v = h('b'), el = h('button', { class: 'u2dd' }, h('span', { class: 'n' }, label), v, h('i', { class: 'car', 'aria-hidden': 'true' }));
  const paint = () => { v.textContent = String(items[idx()] ?? ''); };
  el.onclick = (e) => {
    e.stopPropagation(); const r = el.getBoundingClientRect(), k0 = idx();
    menu(items.map((x, k) => ({ label: String(x), check: k === k0, fn: () => { set(val(k)); paint(); } })), r.left, r.bottom);
  };
  el.sync = paint; paint(); return el;
}
function control(p, label, get, set, onLong) {
  if (isList(p)) return choice(p, label, get, set);
  if (isSwitch(p)) return toggle(p, label, get, set);
  return dial(p, label, get, set, onLong);
}

// ---- envelope drawing for a group that has A / D / S / R
function envView(ps, get) {
  const find = (re) => ps.find((x) => re.test(x.p.n));
  const A = find(/(^|\s)(Attack|Atk|Att)$/i), D = find(/(^|\s)(Decay|Dec)$/i), S = find(/(^|\s)(Sustain|Sus)$/i), R = find(/(^|\s)(Release|Rel)$/i);
  if (!A || !D || (!S && !R)) return null;
  const s = svgEl('svg', { viewBox: '0 0 200 64', preserveAspectRatio: 'none', class: 'u2env' }), fill = svgEl('path', { class: 'f' }), line = svgEl('path', { class: 'l' });
  s.append(fill, line);
  const n = (x) => (x ? toNorm(x.p, get(x.k)) : 0);
  s.sync = () => {
    const a = 0.08 + n(A) * 0.9, dd = 0.08 + n(D) * 0.9, sl = S ? n(S) : 0, rr = R ? 0.08 + n(R) * 0.9 : 0.1, hold = 0.5, k = 192 / (a + dd + hold + rr);
    const x1 = 4 + a * k, x2 = x1 + dd * k, x3 = x2 + hold * k, x4 = x3 + rr * k, ys = 6 + (1 - sl) * 52;
    const dPath = `M4 58L${x1.toFixed(1)} 6L${x2.toFixed(1)} ${ys.toFixed(1)}L${x3.toFixed(1)} ${ys.toFixed(1)}L${x4.toFixed(1)} 58`;
    line.setAttribute('d', dPath); fill.setAttribute('d', dPath + 'Z');
  };
  s.sync(); return s;
}

// ---- sections
const GROUP_NAMES = { Flt: 'Filter Env', Frq: 'Pitch Env', EG: 'Envelope', MW: 'Mod Wheel', Dist: 'Distorsi', Mod: 'Modulasi', Vib: 'Vibrato', Sync: 'Sync' };
function prefixOf(n) {
  const w = String(n).split(/\s+/); if (w.length < 2) return '';
  if (/^\d+$/.test(w[1]) && w.length > 2) return w[0] + ' ' + w[1];
  return w[0];
}
// FL Studio Mobile's own pages for its four synths (the same tabs as the full editor, flmpanels.js)
const R = (a, b) => Array.from({ length: b - a + 1 }, (_, q) => a + q);
const SECTIONS = {
  30: [['Sample', [...R(0, 11), 34]], ['Filter', R(12, 16)], ['Filter Env', R(17, 21)], ['Pitch Env', R(22, 26)], ['LFO', R(27, 33)]],
  31: [['Filter', R(0, 6)], ['Osc', R(7, 13)], ['Mod', R(14, 21)], ['Level', R(22, 27)], ['EG', R(28, 34)], ['LFO', R(35, 38)], ['Voices', R(39, 46)], ['FX 1', R(47, 53)], ['FX 2', R(54, 60)], ['FX 3', R(61, 67)]],
  32: [['Osc', [0, 1, 2, 3, 43]], ['Filter', R(4, 7)], ['Filter Env', R(8, 12)], ['LFO', R(13, 18)], ['Master', R(19, 24)], ['Distorsi', R(25, 27)], ['Delay', R(28, 32)], ['Chorus', R(33, 36)], ['Phaser', R(37, 42)]],
  33: [['Osc', R(0, 4)], ['Unison', R(5, 9)], ['Sync', R(10, 17)], ['Filter', R(18, 26)], ['LFO', R(27, 33)], ['Amp', R(34, 39)], ['Wheels', R(40, 42)], ['Voices', R(43, 51)], ['Distorsi', [52, 53]], ['Delay', R(54, 57)]],
};
function instGroups(def, type) {
  if (SECTIONS[type]) {
    const used = new Set(), out = SECTIONS[type].map(([n, ks]) => [n, ks.filter((k) => def.params[k] && !def.params[k].hidden && !used.has(k) && used.add(k)), '']).filter((g) => g[1].length);
    const rest = def.params.map((_, k) => k).filter((k) => !used.has(k) && !def.params[k].hidden);
    if (rest.length) out.push(['Lainnya', rest, '']);
    return out;
  }
  const idx = def.params.map((p, k) => k).filter((k) => !def.params[k].hidden);
  const cnt = new Map(); for (const k of idx) { const pf = prefixOf(def.params[k].n); if (pf) cnt.set(pf, (cnt.get(pf) || 0) + 1); }
  const order = [], by = new Map();
  for (const k of idx) {
    let pf = prefixOf(def.params[k].n); if (!pf || cnt.get(pf) < 3) pf = '';
    if (pf && !/^[A-Z]$/.test(pf)) pf = pf.replace(/\s?\d+$/, '') || pf;   // LFO1 / LFO2, Img 2 / Img 3, Mod10 ... share one tab
    if (!by.has(pf)) { by.set(pf, []); order.push(pf); } by.get(pf).push(k);
  }
  const main = by.get('') || [];
  const out = [];
  if (main.length) out.push(['Utama', main, '']);
  for (const pf of order) if (pf) out.push([/^[A-Z]$/.test(pf) ? 'Osc ' + pf : (GROUP_NAMES[pf] || pf), by.get(pf), pf]);
  // many tiny groups (Harmor, Moisture): fold groups of 1-2 params into the previous one
  return out;
}
function fxGroups(def) {
  const L = def.flm != null ? FLM_LAYOUT[def.flm] : null, used = new Set(), out = [];
  if (L && L.s) for (const [title, ks] of L.s) {
    const list = ks.filter((k) => def.params[k] && !def.params[k].hidden && !used.has(k)); list.forEach((k) => used.add(k));
    if (list.length) out.push([title, list, '', L.names || {}]);
  }
  const rest = def.params.map((_, k) => k).filter((k) => !used.has(k) && !def.params[k].hidden && def.params[k].fmt !== 'src');
  if (rest.length) out.push([out.length ? 'Lainnya' : 'Parameter', rest, '', (L && L.names) || {}]);
  return out;
}
const tidy = (n, pf) => { let s = String(n); if (pf && s.startsWith(pf + ' ')) s = s.slice(pf.length + 1); s = s.trim(); return /^[A-Z0-9 .\-/]+$/.test(s) && s.length > 3 ? s[0] + s.slice(1).toLowerCase() : s; };

// a card body with tabs (more than one section) and a grid of controls
function body(key, groups, mk, every, bigTabs) {
  const box = h('div', { class: 'u2c-b' }), tabs = h('div', { class: 'u2c-tabs', role: 'tablist' }), grid = h('div', { class: 'u2c-g' });
  let live = [];
  const show = (gi) => {
    tabMem.set(key, gi); const g = groups[gi]; live = [];
    [...tabs.children].forEach((b, j) => { b.classList.toggle('on', j === gi); b.setAttribute('aria-selected', String(j === gi)); });
    const parts = [];
    const ctl = g[1].map((k) => { const c = mk(k, g[2], g[3] || {}); live.push(c); return { k, c }; });
    const env = envView(ctl.map(({ k }) => ({ k, p: mk.def(k) })), mk.get); if (env) live.push(env);
    // order: lists and switches first (full-width rows), then the dials
    const rows = ctl.filter(({ c }) => !c.classList.contains('u2d')).map(({ c }) => c), dials = ctl.filter(({ c }) => c.classList.contains('u2d')).map(({ c }) => c);
    // one grid that fills the rack's height row by row and then grows sideways (a field takes two dial cells): nothing is cut off
    if (env || rows.length || dials.length) parts.push(h('div', { class: 'u2c-dials' }, ...(env ? [env] : []), ...rows, ...dials));
    grid.replaceChildren(...parts); fit();
  };
  // rows: as many as the rack is tall (the card is only as tall as its controls, so measure against the rack, not the card)
  const fit = () => requestAnimationFrame(() => {
    const sc = grid.closest('.dev-scroll'), dl = grid.querySelector('.u2c-dials'); if (!sc) return;
    const hh = sc.clientHeight - (grid.getBoundingClientRect().top - sc.getBoundingClientRect().top) - 22;
    if (hh > 0) grid.style.setProperty('--rows', String(Math.max(1, Math.min(Math.floor((hh + 6) / 66), dl ? dl.children.length : 1))));
  });
  if (typeof ResizeObserver !== 'undefined') { let last = 0; new ResizeObserver(() => { const sc = grid.closest('.dev-scroll'), hgt = sc ? sc.clientHeight : 0; if (hgt !== last) { last = hgt; fit(); } }).observe(grid); requestAnimationFrame(() => { const sc = grid.closest('.dev-scroll'); if (sc) new ResizeObserver(fit).observe(sc); }); }
  if (groups.length > 1 || bigTabs) groups.forEach((g, gi) => tabs.append(h('button', { role: 'tab', onclick: (e) => { e.stopPropagation(); show(gi); } }, g[0])));
  if (tabs.children.length) box.append(tabs);
  box.append(grid);
  show(Math.min(tabMem.get(key) || 0, Math.max(0, groups.length - 1)));
  every(120, () => { for (const c of live) if (c.sync) c.sync(); });
  return box;
}

function moreBtn(items) {
  return h('button', { class: 'u2c-more', 'aria-label': 'Menu modul', onclick: (e) => { e.stopPropagation(); const r = e.currentTarget.getBoundingClientRect(); menu(items(r), r.right - 220, r.bottom + 4); } },
    h('i'), h('i'), h('i'));
}
function powerSwitch(on, flip, label) {
  const b = h('button', { class: 'u2sw-b' + (on ? ' on' : ''), role: 'switch', 'aria-checked': String(on), 'aria-label': label, onclick: (e) => { e.stopPropagation(); flip(); } }, h('i', { class: 'u2sw' }));
  return b;
}

// ---- the card's identity: each effect family has its own colour and icon (the instruments take their track's colour)
const SV = (d) => `<svg viewBox="0 0 24 24" aria-hidden="true">${d}</svg>`;
const CAT = {
  echo: ['Echo & Reverb', '#7cc8ff', SV('<path d="M4 12h2M8 8v8M12 5v14M16 8v8M20 11v2"/>')],
  level: ['Level & Dinamika', '#9be37a', SV('<path d="M5 19V9M10 19V5M15 19v-8M20 19v-5"/>')],
  eq: ['EQ & Filter', '#ffc65c', SV('<path d="M3 16c3 0 4-8 7-8s3 6 6 6 3-4 5-4"/>')],
  mod: ['Modulasi', '#c9b3ff', SV('<path d="M3 12c2-6 4-6 6 0s4 6 6 0 4-6 6 0"/>')],
  pitch: ['Pitch & Stereo', '#ff9ccb', SV('<path d="M8 4v16M8 4l-3 3M8 4l3 3M16 20V4M16 20l-3-3M16 20l3-3"/>')],
  misc: ['Alat', '#a8b4c8', SV('<path d="M12 3v4M12 17v4M3 12h4M17 12h4M6 6l2.5 2.5M15.5 15.5 18 18M6 18l2.5-2.5M15.5 8.5 18 6"/>')],
  flmnote: ['Note FX', '#6fe0cc', SV('<path d="M9 18V6l10-2v12"/><circle cx="6.5" cy="18" r="2.5"/><circle cx="16.5" cy="16" r="2.5"/>')],
};
const INST_IC = SV('<path d="M4 5h16v14H4zM8 5v14M12 5v14M16 5v14"/><path d="M6.6 5v7M10.6 5v7M14.6 5v7M18.2 5v7" stroke-width="2.6"/>');
const catOf = (type) => { const g = FX_GROUPS.find((x) => x[2].includes(type)); return CAT[g ? g[0] : 'misc'] || CAT.misc; };
// the section tabs sit in the title row, right of the title (no row of their own, no empty band)
function inHead(head, b) { const tb = b.querySelector(':scope > .u2c-tabs'); if (tb) head.append(tb); return b; }
// header: [icon badge] [name / family] ... [extra] [switch] [⋯]
function header(icon, nameEl, sub, ...right) {
  const b = h('i', { class: 'u2c-badge', 'aria-hidden': 'true' }); b.innerHTML = icon;
  return h('div', { class: 'u2c-h' }, b, h('div', { class: 'u2c-tt' }, nameEl, h('small', {}, sub)), ...right.filter(Boolean));
}

// ---- effect card
// api: { fxPick(rect, fn), openWindow(), stripName, every }
export function fxCard(i, d, di, n, api) {
  const def = DEVICES[d.type], off = !!d.bypass;
  const name = h('button', { class: 'u2c-name', onclick: (e) => { e.stopPropagation(); api.fxPick(e.currentTarget.getBoundingClientRect(), (ty) => P.replaceDevice(i, di, ty)); } }, h('span', {}, def.name), h('i', { class: 'car', 'aria-hidden': 'true' }));
  const items = () => [
    { label: 'Editor lengkap…', fn: () => api.openWindow() },
    { sep: true },
    { label: 'Geser ke kiri', dim: di <= 0, fn: () => { if (di > 0) P.moveDevice(i, di, di - 1); } },
    { label: 'Geser ke kanan', dim: di >= n - 1, fn: () => { if (di < n - 1) P.moveDevice(i, di, di + 1); } },
    { label: 'Reset parameter', fn: () => { P.resetDevice(i, di); toast(def.name + ': parameter kembali ke awal.', 1400); } },
    { sep: true },
    { label: 'Hapus', fn: () => P.removeDevice(i, di) },
  ];
  const cat = catOf(d.type);
  const head = header(cat[2], name, cat[0], powerSwitch(!off, () => P.toggleBypass(i, di), off ? 'Nyalakan ' + def.name : 'Matikan ' + def.name), moreBtn(items));
  const groups = fxGroups(def);
  if (!NO_WET_FX.has(d.type)) groups.push(['Keluaran', ['mix', 'gain'], '']);
  const get = (k) => (k === 'mix' || k === 'gain' ? P.wetOf(d)[k] : (d.params[k] ?? def.params[k].def));
  const pdef = (k) => (k === 'mix' || k === 'gain' ? WET_PARAMS[k] : def.params[k]);
  const mk = (k, pf, names) => {
    const p = pdef(k), wet = k === 'mix' || k === 'gain';
    const set = wet ? (v) => P.setDeviceWet(i, di, k, v) : (v) => P.setDeviceParam(i, di, k, v);
    const auto = wet && k === 'gain' ? null : (rect) => offerAutomation(rect, { kind: 4, strip: P.stripOf(i), devId: d.id, param: wet ? MIX_P : k, def: p }, `${api.stripName(i)} • ${def.name} ${p.n}`);
    return control(p, wet ? p.n : tidy(names[k] || p.n, pf), () => get(k), set, auto);
  };
  mk.get = get; mk.def = pdef;
  const parts = [head];
  if (d.type === 16) parts.push(h('div', { class: 'u2c-cta' }, h('p', {}, 'Meter, spektrum, osiloskop dan stereo di satu jendela.'), h('button', { class: 'u2btn fill', onclick: () => api.openWindow() }, 'Buka MiniMeters')));
  if (groups.length) parts.push(inHead(head, body('d' + d.id, groups, mk, api.every)));
  else parts.push(h('div', { class: 'u2c-empty' }, 'Modul ini tidak punya parameter.'));
  const card = h('div', { class: 'card u2c fx' + (off ? ' off' : '') }, ...parts); card.style.setProperty('--cc', cat[1]); return card;
}

// ---- instrument card
// api: { pickInst(rect), presetMenu(rect), openWindow(), extra: [{label, fn}], every }
export function instCard(i, t, api) {
  const def = INSTRUMENTS[t.inst.type], midi = t.kind === 'midi';
  const name = midi ? h('button', { class: 'u2c-name', onclick: (e) => { e.stopPropagation(); api.pickInst(e.currentTarget.getBoundingClientRect()); } }, h('span', {}, def.name), h('i', { class: 'car', 'aria-hidden': 'true' }))
    : h('div', { class: 'u2c-name' }, h('span', {}, def.name));
  const items = () => [{ label: 'Editor lengkap…', fn: () => api.openWindow() }, ...(api.extra.length ? [{ sep: true }, ...api.extra] : [])];
  const head = header(INST_IC, name, (t.kind === 'drum' ? 'Drum' : 'Instrumen') + ' \u2022 ' + t.name,
    midi ? h('button', { class: 'u2chip', onclick: (e) => { e.stopPropagation(); api.presetMenu(e.currentTarget.getBoundingClientRect()); } }, 'Preset') : null, moreBtn(items));
  const parts = [head];
  if (t.inst.missing) { const x = t.inst.missing; parts.push(h('div', { class: 'u2c-warn' }, (x.name || 'Instrumen') + ': ' + (x.sample ? 'sample ' + x.sample.split('/').pop() : 'program') + ' tidak tersedia')); }
  const acts = [];
  if (def.zones || def.floatOnly || t.inst.type === 6 || t.inst.type === 35 || t.inst.type === 38 || !midi) acts.push(h('button', { class: 'u2btn tonal', onclick: () => api.openWindow() }, t.inst.type === 6 ? 'File SoundFont & program' : def.zones ? 'Sample & zona' : t.inst.type === 35 ? 'Sample & irisan' : !midi ? 'Pad & sample' : 'Editor lengkap'));
  for (const x of api.extra.slice(0, 2)) acts.push(h('button', { class: 'u2btn', onclick: (e) => x.fn(e.currentTarget.getBoundingClientRect()) }, x.label.replace(/…$/, '')));
  if (acts.length) head.append(h('div', { class: 'u2c-acts' }, ...acts));   // the instrument's own actions (sample, SoundFont file...) sit in the title row
  const groups = instGroups(def, t.inst.type);
  const get = (k) => t.inst.params[k] ?? def.params[k].def;
  const mk = (k, pf) => { const p = def.params[k];
    return control(p, tidy(p.n, pf), () => get(k), (v) => P.setInstParam(i, k, v), (rect) => offerAutomation(rect, { kind: 5, strip: P.stripOf(i), param: k, def: p }, `${t.name} • ${p.n}`)); };
  mk.get = get; mk.def = (k) => def.params[k];
  if (groups.length) parts.push(inHead(head, body('i' + t.id, groups, mk, api.every)));
  else if (!acts.length) parts.push(h('div', { class: 'u2c-empty' }, 'Instrumen ini diatur lewat editornya.'));
  const card = h('div', { class: 'card u2c inst' }, ...parts); card.style.setProperty('--cc', t.color || 'var(--accent)'); return card;
}

// ---- audio track: the source card
export function audioCard() {
  return h('div', { class: 'card u2c src' }, h('div', { class: 'u2c-h' }, h('i', { class: 'u2c-ic au', 'aria-hidden': 'true' }), h('div', { class: 'u2c-name' }, h('span', {}, 'Audio'))),
    h('div', { class: 'u2c-empty' }, 'Klip audio diputar, lalu lewat efek di sebelah kanan.'));
}
