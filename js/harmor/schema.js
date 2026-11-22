// Harmor (instrument 38): the parameter schema. The index of a parameter is its stable ID (native/engine/harmor.h uses the same
// numbers): never renumber or reuse one, only add at free indices. HM_SCHEMA is the version written with presets / projects so a later
// version can migrate them. Engine: native/engine/harmor.cpp (C++, web = its WebAssembly build); this file, the UI, the editors, the
// image / audio analysis and the presets are JavaScript.
export const HM_SCHEMA = 1;
export const HM = { VOL: 0, PARTIALS: 1, QUALITY: 2, POLY: 3, PITCH: 4, FINE: 5, T_WAVE: 10, T_LEVEL: 11, T_PRND: 12, T_TABLE: 13, T: 10,
  MIX: 30, BLEND: 31, STRETCH: 32, SUB: 33, ATT: 40, DEC: 41, SUS: 42, REL: 43, VELS: 44 };
export const HM_WAVES = ['Sine', 'Saw', 'Square', 'Triangle', 'Custom'];
export const HM_QUALITY = ['Draft', 'Normal', 'High'];
export const HM_MAXP = 516;

const list = (n, items, def = 0) => ({ n, min: 0, max: items.length - 1, def, step: 1, fmt: 'list', items });
const q = (n, min, max, def, o = {}) => ({ n, min, max, def, ...o });
const time = (n, def) => q(n, 0.001, 10, def, { curve: 'log', unit: 's' });

export function harmorDef() {
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
  return { name: 'Harmor', full: true, native: true, schema: HM_SCHEMA, params: P };
}
