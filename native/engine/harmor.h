// Harmor (MobileDAW instrument 38): an additive synthesizer in the spirit of Image-Line's Harmor, MobileDAW's own DSP (no Image-Line
// code or data). Every voice is a bank of up to 516 sine partials; two timbres (a waveform + a drawn harmonic level / phase table each)
// give every partial its amplitude and phase, mixed (complex sum) or morphed (amplitude + phase direction interpolated).
// The parameter indices below are the stable IDs of js/harmor/schema.js: never renumber, only add.
// Phase 1: partial engine, timbres, voices, MIDI (pitch bend), amplitude envelope. Later phases add the pitch / filter / articulator /
// unison / effects / image / resynthesis sections at the indices reserved in the schema.
#pragma once
#include "units.h"

namespace mdaw {

constexpr int HM_PARAMS = 512;
constexpr int HM_MAXP = 516;          // partials per voice
constexpr int HM_POLY = 16;           // voices
// global
constexpr int HM_VOL = 0, HM_PARTIALS = 1, HM_QUALITY = 2, HM_POLY_P = 3, HM_PITCH = 4, HM_FINE = 5;
// timbre t (0 / 1): base + 10 * t
constexpr int HM_T_WAVE = 10, HM_T_LEVEL = 11, HM_T_PRND = 12, HM_T_TABLE = 13;
// timbre mix / harmonic mapping
constexpr int HM_MIX = 30, HM_BLEND = 31, HM_STRETCH = 32, HM_SUB = 33;
// amplitude envelope
constexpr int HM_ATT = 40, HM_DEC = 41, HM_SUS = 42, HM_REL = 43, HM_VELS = 44;
// waveforms of a timbre
enum { HM_SINE = 0, HM_SAW = 1, HM_SQUARE = 2, HM_TRI = 3, HM_CUSTOM = 4 };

Instrument* makeHarmor(double sr, Bank* bank);

}  // namespace mdaw
