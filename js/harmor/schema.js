// Harmor (instrument 38): the parameter schema. The index of a parameter is its stable ID (native/engine/harmor.h uses the same
// numbers): never renumber or reuse one, only add at free indices. HM_SCHEMA is the version written with presets / projects so a later
// version can migrate them. Engine: native/engine/harmor.cpp (C++, web = its WebAssembly build); this file, the UI, the editors, the
// image / audio analysis and the presets are JavaScript.
export const HM_SCHEMA = 3;   // 2: pitch / filters / unison / LFOs / articulators; 3: Prism / Pluck / Blur / spectral phaser / effects rack
export const HM = { VOL: 0, PARTIALS: 1, QUALITY: 2, POLY: 3, PITCH: 4, FINE: 5, GLIDE: 6, MONO: 7, LEGATO: 8, T_WAVE: 10, T_LEVEL: 11, T_PRND: 12, T_TABLE: 13, T: 10,
  MIX: 30, BLEND: 31, STRETCH: 32, SUB: 33, ATT: 40, DEC: 41, SUS: 42, REL: 43, VELS: 44, VIB_RATE: 45, VIB_DEPTH: 46, VIB_DELAY: 47,
  F: 50, F_STRIDE: 10, SELFOSC: 70, VEL_FLT: 71, U_VOICES: 75, U_DETUNE: 76, U_SPREAD: 77, U_PHASE: 78, U_PITCH: 79, U_VOLDIST: 80,
  L: 91, L_STRIDE: 6, ENV: 110, ENV_STRIDE: 60, ENV_PTS: 16,
  PRISM_AMT: 81, PRISM_MODE: 82, PLUCK_AMT: 83, PLUCK_TIME: 84, PLUCK_DAMP: 85, BLUR_ATT: 86, BLUR_REL: 87, BLUR_HARM: 88,
  SPH_DEPTH: 103, SPH_DENS: 104, SPH_SPEED: 105, SPH_SHARP: 106, SPH_OFS: 107, FX_ORDER: 712, FX: 720, FX_STRIDE: 48 };
// the effects rack: Harmor runs these MobileDAW effects (DEVICES types) inside itself; effect k's parameter j is HM.FX + 48 k + 2 + j
export const HM_FX = [['Distortion', 3], ['Chorus', 20], ['Phaser', 21], ['Delay', 17], ['Reverb', 7], ['Compressor', 4], ['EQ', 31]];
// filter slots (HM.F + 10 f + k), LFO slots (HM.L + 6 l + k), articulator slots (HM.ENV + 60 e + k): see native/engine/harmor.h
export const HM_FSLOT = { ON: 0, TYPE: 1, CUT: 2, RES: 3, WIDTH: 4, OFS: 5, SLOPE: 6, KEY: 7, MIX: 8, TABLE: 9 };
export const HM_LSLOT = { RATE: 0, SHAPE: 1, PHASE: 2, DEPTH: 3, DEST: 4, RETRIG: 5 };
export const HM_ESLOT = { ON: 0, DEPTH: 1, POINTS: 2, SUS: 3, LOOP: 4, A: 5, H: 6, D: 7, S: 8, R: 9, PT: 10 };   // point j: PT + 3 j = time, level, curve
export const HM_FILTERS = ['Low-pass', 'High-pass', 'Band-pass', 'Band-stop', 'Formant', 'Custom'];
export const HM_SLOPES = ['12 dB', '24 dB', '48 dB'];
export const HM_LFO_SHAPES = ['Sine', 'Triangle', 'Saw', 'Square', 'S&H'];
export const HM_LFO_DEST = ['Off', 'Pitch', 'Cutoff 1', 'Cutoff 2', 'Volume', 'Timbre Mix', 'Harm Phase', 'Pan', 'Stretch'];
export const HM_ENVS = ['Volume', 'Pitch', 'Cutoff 1', 'Cutoff 2', 'Frequency', 'Harm Level', 'Harm Phase', 'Timbre Mix', 'Image Freq', 'Image Gain'];
export const HM_WAVES = ['Sine', 'Saw', 'Square', 'Triangle', 'Custom'];
export const HM_QUALITY = ['Draft', 'Normal', 'High'];
export const HM_MAXP = 516;

const list = (n, items, def = 0) => ({ n, min: 0, max: items.length - 1, def, step: 1, fmt: 'list', items });
const q = (n, min, max, def, o = {}) => ({ n, min, max, def, ...o });
const time = (n, def) => q(n, 0.001, 10, def, { curve: 'log', unit: 's' });
const onoff = (n, def = 0) => ({ n, min: 0, max: 1, def, step: 1, fmt: 'onoff' });

export function harmorDef(DEVICES) {
  const P = [];
  const at = (k, d) => { while (P.length < k) P.push({ n: '-', min: 0, max: 1, def: 0, hidden: true }); P[k] = d; };
  at(HM.VOL, q('Volume', 0, 1, 0.7, { unit: '%' }));
  at(HM.PARTIALS, q('Partials', 1, HM_MAXP, 256, { step: 1, fmt: 'num' }));
  at(HM.QUALITY, list('Quality', HM_QUALITY, 1));
  at(HM.POLY, q('Voices', 1, 16, 8, { step: 1, fmt: 'num' }));
  at(HM.PITCH, q('Coarse', -24, 24, 0, { step: 1, unit: 'st' }));
  at(HM.FINE, q('Fine', -100, 100, 0, { unit: 'ct' }));
  for (let t = 0; t < 2; t++) {
    const b = HM.T * (t + 1), name = 'T' + (t + 1) + ' ';
    at(b, list(name + 'Wave', HM_WAVES, t ? 2 : 1));
    at(b + 1, q(name + 'Level', 0, 1, 1, { unit: '%' }));
    at(b + 2, q(name + 'Phase Rnd', 0, 1, 0, { unit: '%' }));
    at(b + 3, q(name + 'Table', -1, 255, -1, { step: 1, hidden: true }));   // sample-bank slot of the drawn harmonic table (set by the editor / on load)
  }
  at(HM.MIX, q('Timbre Mix', 0, 1, 0, { unit: '%' }));
  at(HM.BLEND, list('Blend', ['Mix', 'Morph'], 0));
  at(HM.STRETCH, q('Stretch', -1, 1, 0));
  at(HM.SUB, q('Sub', 0, 1, 0, { unit: '%' }));
  at(HM.ATT, time('Attack', 0.005)); at(HM.DEC, time('Decay', 0.3)); at(HM.SUS, q('Sustain', 0, 1, 0.8, { unit: '%' })); at(HM.REL, time('Release', 0.3));
  at(HM.VELS, q('Vel > Vol', 0, 1, 0.7, { unit: '%' }));
  // phase 2: pitch, filters, unison, LFOs, articulators
  at(HM.GLIDE, time('Glide', 0.08)); at(HM.MONO, onoff('Mono')); at(HM.LEGATO, onoff('Legato'));
  at(HM.VIB_RATE, q('Vib Rate', 0.05, 20, 5, { curve: 'log', unit: 'Hz' })); at(HM.VIB_DEPTH, q('Vib Depth', 0, 200, 0, { unit: 'ct' })); at(HM.VIB_DELAY, q('Vib Delay', 0, 5, 0, { unit: 's' }));
  for (let f = 0; f < 2; f++) {
    const b = HM.F + 10 * f, n = 'F' + (f + 1) + ' ';
    at(b, onoff(n + 'On')); at(b + 1, list(n + 'Type', HM_FILTERS, 0)); at(b + 2, q(n + 'Cutoff', 20, 20000, 2000, { curve: 'log', unit: 'Hz' }));
    at(b + 3, q(n + 'Res', 0, 1, 0, { unit: '%' })); at(b + 4, q(n + 'Res Width', 0.05, 3, 0.5, { unit: 'oct' })); at(b + 5, q(n + 'Res Offset', -4, 4, 0, { unit: 'oct' }));
    at(b + 6, list(n + 'Slope', HM_SLOPES, 1)); at(b + 7, q(n + 'Key', 0, 1, 0, { unit: '%' })); at(b + 8, q(n + 'Mix', 0, 1, 1, { unit: '%' }));
    at(b + 9, q(n + 'Curve', -1, 255, -1, { step: 1, hidden: true }));   // sample-bank slot of the drawn filter curve
  }
  at(HM.SELFOSC, q('Self-Osc', 0, 1, 0, { unit: '%' })); at(HM.VEL_FLT, q('Vel > Cutoff', 0, 1, 0, { unit: '%' }));
  at(HM.U_VOICES, q('Unison', 1, 8, 1, { step: 1, fmt: 'num' })); at(HM.U_DETUNE, q('Uni Detune', 0, 100, 15, { unit: 'ct' })); at(HM.U_SPREAD, q('Uni Spread', 0, 1, 0.6, { unit: '%' }));
  at(HM.U_PHASE, q('Uni Phase', 0, 1, 1, { unit: '%' })); at(HM.U_PITCH, q('Uni Pitch', -24, 24, 0, { unit: 'st' })); at(HM.U_VOLDIST, q('Uni Vol Dist', -1, 1, 0));
  for (let l = 0; l < 2; l++) {
    const b = HM.L + HM.L_STRIDE * l, n = 'LFO' + (l + 1) + ' ';
    at(b, q(n + 'Rate', 0.01, 40, 2, { curve: 'log', unit: 'Hz' })); at(b + 1, list(n + 'Shape', HM_LFO_SHAPES, 0)); at(b + 2, q(n + 'Phase', 0, 1, 0, { unit: '%' }));
    at(b + 3, q(n + 'Depth', 0, 1, 0, { unit: '%' })); at(b + 4, list(n + 'Dest', HM_LFO_DEST, 0)); at(b + 5, onoff(n + 'Retrig', 1));
  }
  for (let e = 0; e < HM_ENVS.length; e++) {
    const b = HM.ENV + HM.ENV_STRIDE * e, n = HM_ENVS[e] + ' ', img = e >= 8, hid = img ? { hidden: true } : {};   // image articulators: phase 3
    at(b, onoff(n + 'Env', 0)); at(b + 1, q(n + 'Depth', -1, 1, e === 0 ? 1 : 0.5, { unit: '%', ...hid }));
    if (img) P[b].hidden = true;
    at(b + 2, q(n + 'Points', 0, 16, 0, { step: 1, hidden: true })); at(b + 3, q(n + 'Sus Pt', -1, 15, -1, { step: 1, hidden: true })); at(b + 4, q(n + 'Loop Pt', -1, 15, -1, { step: 1, hidden: true }));
    at(b + 5, time(n + 'A', 0.01)); at(b + 6, q(n + 'H', 0, 10, 0, { unit: 's' })); at(b + 7, time(n + 'D', 0.4)); at(b + 8, q(n + 'S', 0, 1, e === 0 ? 0.8 : 0, { unit: '%' })); at(b + 9, time(n + 'R', 0.3));
    for (const k of [5, 6, 7, 8, 9]) if (img) P[b + k].hidden = true;
    for (let j = 0; j < HM.ENV_PTS; j++) { at(b + 10 + 3 * j, q(n + 'P' + j + ' Time', 0, 30, 0, { hidden: true })); at(b + 11 + 3 * j, q(n + 'P' + j + ' Level', 0, 1, 0, { hidden: true })); at(b + 12 + 3 * j, q(n + 'P' + j + ' Curve', -1, 1, 0, { hidden: true })); }
  }
  // phase 2b: special processing and the effects rack
  at(HM.PRISM_AMT, q('Prism', 0, 1, 0, { unit: '%' })); at(HM.PRISM_MODE, list('Prism Mode', ['Scatter', 'Shift'], 0));
  at(HM.PLUCK_AMT, q('Pluck', 0, 1, 0, { unit: '%' })); at(HM.PLUCK_TIME, q('Pluck Time', 0.01, 10, 1, { curve: 'log', unit: 's' })); at(HM.PLUCK_DAMP, q('Pluck Damp', 0, 1, 0.5, { unit: '%' }));
  at(HM.BLUR_ATT, q('Blur Attack', 0, 5, 0, { unit: 's' })); at(HM.BLUR_REL, q('Blur Release', 0, 5, 0, { unit: 's' })); at(HM.BLUR_HARM, q('Blur Harm', 0, 1, 0, { unit: '%' }));
  at(HM.SPH_DEPTH, q('Phaser Depth', 0, 1, 0, { unit: '%' })); at(HM.SPH_DENS, q('Phaser Notches', 0.1, 8, 1, { unit: '/oct' })); at(HM.SPH_SPEED, q('Phaser Speed', -8, 8, 0.2, { unit: 'Hz' }));
  at(HM.SPH_SHARP, q('Phaser Sharp', 0, 1, 0.5, { unit: '%' })); at(HM.SPH_OFS, q('Phaser Offset', 0, 1, 0, { unit: '%' }));
  for (let s0 = 0; s0 < HM_FX.length; s0++) at(HM.FX_ORDER + s0, list('FX Slot ' + (s0 + 1), HM_FX.map(([n]) => n), s0));
  for (let k = 0; k < HM_FX.length; k++) {
    const [name, type] = HM_FX[k], b = HM.FX + HM.FX_STRIDE * k, dp = (DEVICES && DEVICES[type] && DEVICES[type].params) || [];
    at(b, onoff(name + ' On')); at(b + 1, q(name + ' Mix', 0, 1, 1, { unit: '%' }));
    dp.forEach((d, j) => at(b + 2 + j, { ...d, n: name + ' ' + d.n, hidden: true }));   // the effect's own panel shows these (phase 6); the generic panel only On / Mix
  }
  return { name: 'Harmor', full: true, native: true, schema: HM_SCHEMA, params: P };
}
