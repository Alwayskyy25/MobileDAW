// ui_model.h - native project model mirroring the web app's project.js state
// (tracks/clips/returns/master/loop/bpm/sig) + the pure functions that turn
// model state into engine text commands (native/engine/commands.cpp) and
// into the native save format. Everything here is platform-neutral and
// host-tested; the UI (ui_core.cpp) owns a Project and sends the strings
// these builders produce through EngineLink.
#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "ui_defs.h"

namespace mdnative {

// Track kinds (web project.js: midi / drum / audio / auto).
enum TrackKind { kMidi = 0, kDrum = 1, kAudio = 2, kAuto = 3, kReturn = 4 };

struct Note {
  double start = 0, len = 240;  // ticks, relative to the clip pattern start
  int pitch = 60;
  float vel = 0.9f, pan = 0;
};

struct Clip {
  double start = 0, len = 3840;  // ticks on the song timeline
  bool muted = false;
  std::string name;
  // midi/drum clips: a pattern of patternLen ticks, repeated across len.
  std::vector<Note> notes;
  double patternLen = 3840;
  // audio clips (sample bytes cannot cross the text protocol - see report):
  int sampleId = -1;
  std::string sampleName;
  double gain = 1, offset = 0, fadeIn = 0, fadeOut = 0, pan = 0;
};

struct FxDevice {
  int type = 0;
  bool bypass = false;
  std::vector<double> params;  // sized to the device def, defaults applied
  double mix = 1;              // devParam 1000 (MIX_P), 0..1
  double gainDb = 0;           // devParam 1001 (GAIN_P), dB -36..+12
};

struct Track {
  int kind = kMidi;
  std::string name = "Track";
  uint32_t color = 0xFF8C2EFF;
  int instType = 1;
  std::vector<double> instParams;
  std::vector<FxDevice> devices;
  std::vector<Clip> clips;
  double vol = 0.85, pan = 0, width = 1;
  bool mute = false, solo = false, arm = false;
  double send[2] = {0, 0};
  int out = -1;  // -1 = Master, 0..7 = return index (engine trackOut)
  bool folded = false;
};

struct AutoLane {
  int kind = 0;    // engine Lane kinds: 0 vol, 1 pan, 2 sendA, 3 sendB,
                   // 4 devParam (dev,param), 5 instParam (param; 90 pitch), 7 width
  int track = 0;   // engine strip
  int dev = -1, param = 0;
  std::vector<std::pair<double, double>> pts;  // (tick, value), sorted
};

struct Project {
  std::string name = "Untitled";
  double bpm = 120;
  int sigNum = 4, sigDen = 4;
  bool loopOn = true;
  double loopStart = 0, loopLen = 3840;
  bool metro = false;
  double metroVol = 0.7;
  int countIn = 0;  // bars
  bool masterLimit = true, pdc = true;
  std::vector<Track> tracks;
  std::vector<Track> returns;  // strips 65+i
  double masterVol = 0.9, masterPan = 0;
  std::vector<AutoLane> autoLanes;
};

// Engine strip indices (engine.h: MAX_TRACKS 64, MASTER 64, RET0 65, NFX 8).
inline int stripOfTrack(int i) { return i; }
inline int stripOfReturn(int i) { return 65 + i; }
inline constexpr int kMasterStrip = 64;

// ---- metadata lookups (ui_defs.h tables) ----
const defs::UnitDef* instDef(int type);
const defs::UnitDef* devDef(int type);
std::vector<double> defaultParams(const defs::UnitDef* d);
void ensureParams(Track* t);
void ensureParams(FxDevice* d);

// ---- command builders (pure; return newline-terminated command text) ----
std::string fmtNum(double v);  // compact number for the protocol
std::string buildTransport(const Project& p, bool forPlay);
std::string buildSig(const Project& p);
std::string buildLoop(const Project& p);
std::string buildStripMixer(int strip, const Track& t);
std::string buildMasterMixer(const Project& p);
std::string buildTrackInst(int strip, const Track& t);
std::string buildTrackDevices(int strip, const Track& t, int oldCount);
std::string buildTrackNotes(int strip, const Track& t);
std::string buildTrackClips(int strip, const Track& t);
std::string buildAutomation(const Project& p);
std::string buildSyncAll(const Project& p);
std::string buildResetStrips(int nTracks, int nReturns);

// Flatten a pattern clip's notes onto the song timeline (loop cycles, notes
// cut at cycle/clip end) - same semantics as project.js clipNoteEvents.
std::vector<Note> flattenClipNotes(const Clip& c);

// ---- save format (native text format "MDAWN1"; documented in ui_model.cpp) ----
std::string serializeProject(const Project& p);
bool parseProject(const std::string& text, Project* out, std::string* err);

Project makeDefaultProject();

}  // namespace mdnative
