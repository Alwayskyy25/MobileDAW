// cmds.h - engine text-command vocabulary used by the native UI.
//
// VERIFIED 2026-11-26 against the full repo source (ZIP from Rizky):
//   - native/engine/commands.cpp: the OPS table inside mdaw::prepare is the
//     single source of truth (44 commands). Spellings below are copied from it.
//   - native/engine/engine.h: PPQ = 960 ticks per quarter note; Engine::loop
//     takes (on, startTick, END tick); fillMeters writes peaks * 1000.
//   - tools/golden/scenarios.mjs (inst-drums): drums are NOT a special
//     command - a drum track is trackInst <t> 3 (DrumSampler) and steps are
//     ordinary notes via trackNotes, pitch = GM pad note (36 kick, 38 snare,
//     42 closed hat, 39 clap), start/len in ticks, velocity 0..1.
// There are NO "drum", "note", "metro" or "gain" commands in the engine;
// an unknown name makes prepare() fail with "unknown command <name>".
#pragma once

namespace mdnative {
namespace cmds {

// Transport / song setup. Arguments follow commands.cpp prepare():
//   play, stop: no arguments. tempo: <bpm>. setMetro: <0|1> (truthy).
//   loop: <0|1> <startTick> <endTick>.
inline const char* kPlay = "play";
inline const char* kStop = "stop";
inline const char* kTempo = "tempo";          // tempo <bpm>
inline const char* kMetro = "setMetro";       // setMetro <0|1>
inline const char* kLoop = "loop";            // loop <0|1> <startTick> <endTick>

// Tracks / instruments / notes (commands.cpp TRACKINST / TRACKNOTES / TRACKVOL):
//   trackInst <track> <type>   - type 3 = DrumSampler, 1 = MiniSynth (units.cpp)
//   trackNotes <track> [count v...] <count> - flat notes, stride 5:
//     startTick, lenTicks, pitch, velocity 0..1, pan -1..1 (prepNotes)
//   trackVol <strip> <value>   - strip 64 = MASTER (engine.h MASTER)
inline const char* kTrackInst = "trackInst";
inline const char* kTrackNotes = "trackNotes";
inline const char* kTrackVol = "trackVol";

// Engine grid (engine.h): PPQ = 960 ticks per quarter; a 16th step = PPQ/4.
inline constexpr int kPPQ = 960;
inline constexpr int kTicksPerStep = kPPQ / 4;   // 240
inline constexpr int kStepsPerBar = 16;
inline constexpr int kMasterStrip = 64;          // engine.h MASTER

// v1 track layout: track 0 = drums (DrumSampler), track 1 = synth (MiniSynth).
inline constexpr int kDrumTrack = 0;
inline constexpr int kSynthTrack = 1;
inline constexpr int kInstDrumSampler = 3;       // units.cpp makeInstrument(3)
inline constexpr int kInstMiniSynth = 1;         // units.cpp makeInstrument(1)
// GM pad pitches per drum row (golden inst-drums pattern): KICK SNARE HAT PERC
inline constexpr int kDrumPitch[4] = {36, 38, 42, 39};

}  // namespace cmds
}  // namespace mdnative
