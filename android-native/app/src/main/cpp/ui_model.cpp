// ui_model.cpp - see ui_model.h. Command spellings/formats verified against
// native/engine/commands.cpp (prepare/apply) and js/project.js (syncClips):
//   trackNotes stride 6 (start,len,pitch,vel,pan,slide), clips flattened to
//   absolute ticks per loop cycle; trackClips stride 10 for audio
//   (start,len,sample,gain,offset,fadeIn,fadeOut,pan,envFirst,envCount);
//   automation lanes: [kind,track,dev,param,npts, (tick,val)...] concatenated.
#include "ui_model.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include "cmds.h"

namespace mdnative {

std::string fmtNum(double v) {
  if (v == (double)(long long)v && std::fabs(v) < 1e15) return std::to_string((long long)v);
  char b[40];
  std::snprintf(b, sizeof b, "%.4f", v);
  std::string s = b;
  while (!s.empty() && s.back() == '0') s.pop_back();
  if (!s.empty() && s.back() == '.') s.pop_back();
  return s;
}
static std::string num(double v) { return fmtNum(v); }

const defs::UnitDef* findIn(const defs::UnitDef* tab, size_t n, int type) {
  for (size_t i = 0; i < n; i++)
    if (tab[i].type == type) return &tab[i];
  return nullptr;
}
const defs::UnitDef* instDef(int type) {
  return findIn(defs::kInstruments, sizeof(defs::kInstruments) / sizeof(defs::UnitDef), type);
}
const defs::UnitDef* devDef(int type) {
  return findIn(defs::kDevices, sizeof(defs::kDevices) / sizeof(defs::UnitDef), type);
}
std::vector<double> defaultParams(const defs::UnitDef* d) {
  std::vector<double> v;
  if (d) for (const auto& p : d->params) v.push_back(p.def);
  return v;
}
void ensureParams(Track* t) {
  const defs::UnitDef* d = instDef(t->instType);
  size_t want = d ? d->params.size() : 0;
  if (t->instParams.size() != want) {
    std::vector<double> np = defaultParams(d);
    for (size_t i = 0; i < want && i < t->instParams.size(); i++) np[i] = t->instParams[i];
    t->instParams = np;
  }
}
void ensureParams(FxDevice* d) {
  const defs::UnitDef* def = devDef(d->type);
  size_t want = def ? def->params.size() : 0;
  if (d->params.size() != want) {
    std::vector<double> np = defaultParams(def);
    for (size_t i = 0; i < want && i < d->params.size(); i++) np[i] = d->params[i];
    d->params = np;
  }
}

// ------------------------------------------------------------ command builds
std::string buildTransport(const Project& p, bool /*forPlay*/) {
  return "tempo " + num(p.bpm) + "\n";
}
std::string buildSig(const Project& p) {
  // Exactly what the web sends (project.js syncMeter): eng.sig(BEAT, n) with
  // BEAT = round(PPQ*4/den) ticks per beat; prepare() SETSIG mirrors
  // core.js setSig, so identical arguments give identical behaviour.
  double beat = std::round(4.0 * cmds::kPPQ / (p.sigDen > 0 ? p.sigDen : 4));
  return "setSig " + num(beat) + " " + num(p.sigNum) + "\n";
}
std::string buildLoop(const Project& p) {
  return "loop " + std::string(p.loopOn ? "1 " : "0 ") + num(p.loopStart) + " " +
         num(p.loopStart + p.loopLen) + "\n";
}
static void appStrip(std::string* s, int strip, const Track& t) {
  *s += "trackVol " + num(strip) + " " + num(t.vol) + "\n";
  *s += "trackPan " + num(strip) + " " + num(t.pan) + "\n";
  *s += "trackWidth " + num(strip) + " " + num(t.width) + "\n";
  *s += "trackMute " + num(strip) + (t.mute ? " 1\n" : " 0\n");
  *s += "trackSolo " + num(strip) + (t.solo ? " 1\n" : " 0\n");
  *s += "trackSend " + num(strip) + " 0 " + num(t.send[0]) + "\n";
  *s += "trackSend " + num(strip) + " 1 " + num(t.send[1]) + "\n";
  if (strip < kMasterStrip) *s += "trackOut " + num(strip) + " " + num(t.out) + "\n";
}
std::string buildStripMixer(int strip, const Track& t) {
  std::string s;
  appStrip(&s, strip, t);
  return s;
}
std::string buildMasterMixer(const Project& p) {
  std::string s = "trackVol " + num(kMasterStrip) + " " + num(p.masterVol) + "\n";
  s += "trackPan " + num(kMasterStrip) + " " + num(p.masterPan) + "\n";
  s += "masterLimit " + std::string(p.masterLimit ? "1\n" : "0\n");
  s += "setPdc " + std::string(p.pdc ? "1\n" : "0\n");
  s += "setMetroVol " + num(p.metroVol) + "\n";
  return s;
}
std::string buildTrackInst(int strip, const Track& t) {
  std::string s = "trackInst " + num(strip) + " " + num(t.instType) + "\n";
  const defs::UnitDef* d = instDef(t.instType);
  size_t n = d ? d->params.size() : 0;
  for (size_t i = 0; i < n && i < t.instParams.size(); i++)
    s += "instParam " + num(strip) + " " + num((double)i) + " " + num(t.instParams[i]) + "\n";
  return s;
}
std::string buildTrackDevices(int strip, const Track& t, int oldCount) {
  std::string s;
  for (int i = 0; i < oldCount; i++) s += "removeDevice " + num(strip) + " 0\n";
  for (size_t i = 0; i < t.devices.size(); i++) {
    const FxDevice& dv = t.devices[i];
    s += "insertDevice " + num(strip) + " " + num((double)i) + " " + num(dv.type) + "\n";
    const defs::UnitDef* d = devDef(dv.type);
    size_t n = d ? d->params.size() : 0;
    for (size_t j = 0; j < n && j < dv.params.size(); j++)
      s += "devParam " + num(strip) + " " + num((double)i) + " " + num((double)j) + " " + num(dv.params[j]) + "\n";
    s += "devParam " + num(strip) + " " + num((double)i) + " 1000 " + num(dv.mix) + "\n";
    double g = std::pow(10.0, dv.gainDb / 20.0);
    s += "devParam " + num(strip) + " " + num((double)i) + " 1001 " + num(g) + "\n";
    if (dv.bypass) s += "devBypass " + num(strip) + " " + num((double)i) + " 1\n";
  }
  return s;
}

std::vector<Note> flattenClipNotes(const Clip& c) {
  std::vector<Note> out;
  if (c.patternLen <= 0) return out;
  // For pattern clips, Clip.offset is the pattern phase in ticks (left trim
  // shifts it); cycles start at -phase. (Audio clips use offset as the
  // engine sample offset instead - the two never mix, kind decides.)
  double phase = std::fmod(c.offset, c.patternLen);
  if (phase < 0) phase += c.patternLen;
  for (double o = -phase; o < c.len - 1e-9; o += c.patternLen) {
    double cycEnd = std::min(o + c.patternLen, c.len);
    for (const Note& n : c.notes) {
      if (n.start < 0 || n.start >= c.patternLen) continue;
      Note m = n;
      m.start = c.start + o + n.start;
      if (m.start < c.start - 1e-9) continue;  // fell before the clip (phase)
      double maxLen = (c.start + cycEnd) - m.start;
      if (m.len > maxLen) m.len = maxLen;
      if (m.len > 0) out.push_back(m);
    }
  }
  std::stable_sort(out.begin(), out.end(), [](const Note& a, const Note& b) { return a.start < b.start; });
  return out;
}

std::string buildTrackNotes(int strip, const Track& t) {
  std::vector<Note> all;
  if (t.kind == kMidi || t.kind == kDrum)
    for (const Clip& c : t.clips) {
      if (c.muted) continue;
      std::vector<Note> f = flattenClipNotes(c);
      all.insert(all.end(), f.begin(), f.end());
    }
  std::stable_sort(all.begin(), all.end(), [](const Note& a, const Note& b) { return a.start < b.start; });
  std::string flat;
  for (const Note& n : all)
    flat += " " + num(n.start) + " " + num(n.len) + " " + num(n.pitch) + " " + num(n.vel) + " " + num(n.pan) + " 0";
  return "trackNotes " + num(strip) + " [" + num((double)all.size() * 6) + flat + "] " +
         num((double)all.size()) + " 6\n";
}

std::string buildTrackClips(int strip, const Track& t) {
  std::string flat;
  int n = 0;
  if (t.kind == kAudio)
    for (const Clip& c : t.clips) {
      if (c.muted || c.sampleId < 0) continue;
      flat += " " + num(c.start) + " " + num(c.len) + " " + num(c.sampleId) + " " + num(c.gain) + " " +
              num(c.offset) + " " + num(c.fadeIn) + " " + num(c.fadeOut) + " " + num(c.pan) + " 0 0";
      n++;
    }
  return "trackClips " + num(strip) + " [" + num((double)n * 10) + flat + "] " + num(n) + " 10\n";
}

std::string buildAutomation(const Project& p) {
  std::string flat;
  long total = 0;
  for (const AutoLane& l : p.autoLanes) {
    if (l.pts.empty()) continue;
    flat += " " + num(l.kind) + " " + num(l.track) + " " + num(l.dev) + " " + num(l.param) + " " +
            num((double)l.pts.size());
    total += 5;
    for (auto& pt : l.pts) {
      flat += " " + num(pt.first) + " " + num(pt.second);
      total += 2;
    }
  }
  return "automation [" + num((double)total) + flat + "] " + num(total) + "\n";
}

std::string buildResetStrips(int nTracks, int nReturns) {
  std::string s;
  for (int i = 0; i < nTracks; i++) s += "resetTrack " + num(i) + "\n";
  for (int i = 0; i < nReturns; i++) s += "resetTrack " + num(stripOfReturn(i)) + "\n";
  return s;
}

std::string buildSyncAll(const Project& p) {
  std::string s;
  s += buildTransport(p, false);
  s += buildLoop(p);
  s += "setMetro " + std::string(p.metro ? "1\n" : "0\n");
  s += buildMasterMixer(p);
  for (size_t i = 0; i < p.tracks.size(); i++) {
    const Track& t = p.tracks[i];
    s += buildTrackInst((int)i, t);
    s += buildTrackDevices((int)i, t, 0);
    s += buildStripMixer((int)i, t);
    s += buildTrackNotes((int)i, t);
    s += buildTrackClips((int)i, t);
  }
  for (size_t i = 0; i < p.returns.size(); i++) {
    const Track& t = p.returns[i];
    int st = stripOfReturn((int)i);
    s += buildTrackDevices(st, t, 0);
    s += buildStripMixer(st, t);
  }
  s += buildAutomation(p);
  return s;
}

// ------------------------------------------------------------ save format
// Native text format "MDAWN1" (NOT the web .mdaw JSON - the web format is a
// JSON serialization of project.js state; this stores the same model fields
// in a line format the C++ UI can parse without a JSON library):
//   MDAWN1
//   proj <bpm> <sigNum> <sigDen> <loopOn> <loopStart> <loopLen> <metro>
//        <metroVol> <countIn> <masterLimit> <pdc> <masterVol> <masterPan>
//   name <escaped>
//   track <kind> <instType> <color> <vol> <pan> <width> <mute> <solo>
//         <send0> <send1> <out> <folded>
//   tname <escaped>
//   iparam <idx> <value>            (sparse; defaults otherwise)
//   dev <type> <bypass> <mix> <gainDb>
//   dparam <idx> <value>             (sparse)
//   clip <start> <len> <muted> <patternLen> <sampleId> <gain> <offset>
//        <fadeIn> <fadeOut> <pan>
//   cname <escaped>
//   note <start> <len> <pitch> <vel> <pan>
//   endclip / enddev / endtrack
//   return ... (same body as track) / endreturn
//   lane <kind> <track> <dev> <param> <npts> <tick> <val> ...
//   end
static std::string esc(const std::string& s) {
  std::string o;
  for (char c : s) {
    if (c == '%') o += "%25";
    else if (c == '\n') o += "%0A";
    else if (c == ' ') o += "%20";
    else o += c;
  }
  return o;
}
static std::string unesc(const std::string& s) {
  std::string o;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '%' && i + 2 < s.size() + 0 && i + 2 <= s.size() - 1) {
      std::string h = s.substr(i + 1, 2);
      if (h == "25") { o += '%'; i += 2; continue; }
      if (h == "0A") { o += '\n'; i += 2; continue; }
      if (h == "20") { o += ' '; i += 2; continue; }
    }
    o += s[i];
  }
  return o;
}

static void serTrack(std::string* o, const char* kw, const Track& t) {
  *o += kw;
  *o += " " + num(t.kind) + " " + num(t.instType) + " " + num((double)(uint32_t)t.color) + " " +
        num(t.vol) + " " + num(t.pan) + " " + num(t.width) + (t.mute ? " 1" : " 0") +
        (t.solo ? " 1" : " 0") + " " + num(t.send[0]) + " " + num(t.send[1]) + " " + num(t.out) +
        (t.folded ? " 1\n" : " 0\n");
  *o += "tname " + esc(t.name) + "\n";
  const defs::UnitDef* id = instDef(t.instType);
  for (size_t i = 0; i < t.instParams.size(); i++) {
    double dfl = id && i < id->params.size() ? id->params[i].def : 0;
    if (t.instParams[i] != dfl) *o += "iparam " + num((double)i) + " " + num(t.instParams[i]) + "\n";
  }
  for (const FxDevice& dv : t.devices) {
    *o += "dev " + num(dv.type) + (dv.bypass ? " 1 " : " 0 ") + num(dv.mix) + " " + num(dv.gainDb) + "\n";
    const defs::UnitDef* dd = devDef(dv.type);
    for (size_t i = 0; i < dv.params.size(); i++) {
      double dfl = dd && i < dd->params.size() ? dd->params[i].def : 0;
      if (dv.params[i] != dfl) *o += "dparam " + num((double)i) + " " + num(dv.params[i]) + "\n";
    }
    *o += "enddev\n";
  }
  for (const Clip& c : t.clips) {
    *o += "clip " + num(c.start) + " " + num(c.len) + (c.muted ? " 1 " : " 0 ") + num(c.patternLen) +
          " " + num(c.sampleId) + " " + num(c.gain) + " " + num(c.offset) + " " + num(c.fadeIn) + " " +
          num(c.fadeOut) + " " + num(c.pan) + "\n";
    *o += "cname " + esc(c.name) + "\n";
    if (!c.sampleName.empty()) *o += "sname " + esc(c.sampleName) + "\n";
    for (const Note& n : c.notes)
      *o += "note " + num(n.start) + " " + num(n.len) + " " + num(n.pitch) + " " + num(n.vel) + " " +
            num(n.pan) + "\n";
    *o += "endclip\n";
  }
  *o += std::string("end") + (kw[0] == 't' ? "track\n" : "return\n");
}

std::string serializeProject(const Project& p) {
  std::string o = "MDAWN1\n";
  o += "proj " + num(p.bpm) + " " + num(p.sigNum) + " " + num(p.sigDen) + (p.loopOn ? " 1 " : " 0 ") +
       num(p.loopStart) + " " + num(p.loopLen) + (p.metro ? " 1 " : " 0 ") + num(p.metroVol) + " " +
       num(p.countIn) + (p.masterLimit ? " 1 " : " 0 ") + (p.pdc ? " 1 " : " 0 ") + num(p.masterVol) +
       " " + num(p.masterPan) + "\n";
  o += "name " + esc(p.name) + "\n";
  for (const Track& t : p.tracks) serTrack(&o, "track", t);
  for (const Track& t : p.returns) serTrack(&o, "return", t);
  for (const AutoLane& l : p.autoLanes) {
    o += "lane " + num(l.kind) + " " + num(l.track) + " " + num(l.dev) + " " + num(l.param) + " " +
         num((double)l.pts.size());
    for (auto& pt : l.pts) o += " " + num(pt.first) + " " + num(pt.second);
    o += "\n";
  }
  o += "end\n";
  return o;
}

static std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> v;
  std::istringstream is(s);
  std::string w;
  while (is >> w) v.push_back(w);
  return v;
}
static double atod(const std::vector<std::string>& v, size_t i, double d = 0) {
  return i < v.size() ? std::atof(v[i].c_str()) : d;
}

bool parseProject(const std::string& text, Project* out, std::string* err) {
  Project p;
  std::istringstream is(text);
  std::string line;
  Track* cur = nullptr;
  FxDevice* curDev = nullptr;
  Clip* curClip = nullptr;
  bool header = false;
  while (std::getline(is, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) continue;
    std::vector<std::string> v = split(line);
    if (v.empty()) continue;
    const std::string& k = v[0];
    if (!header) {
      if (k != "MDAWN1") { if (err) *err = "not an MDAWN1 file"; return false; }
      header = true;
      continue;
    }
    if (k == "proj" && v.size() >= 14) {
      p.bpm = atod(v, 1, 120); p.sigNum = (int)atod(v, 2, 4); p.sigDen = (int)atod(v, 3, 4);
      p.loopOn = atod(v, 4) != 0; p.loopStart = atod(v, 5); p.loopLen = atod(v, 6, 3840);
      p.metro = atod(v, 7) != 0; p.metroVol = atod(v, 8, 0.7); p.countIn = (int)atod(v, 9);
      p.masterLimit = atod(v, 10, 1) != 0; p.pdc = atod(v, 11, 1) != 0;
      p.masterVol = atod(v, 12, 0.9); p.masterPan = atod(v, 13);
    } else if (k == "name") {
      p.name = v.size() > 1 ? unesc(v[1]) : "";
    } else if (k == "track" || k == "return") {
      Track t;
      t.kind = (int)atod(v, 1); t.instType = (int)atod(v, 2, 1);
      t.color = (uint32_t)atod(v, 3); t.vol = atod(v, 4, 0.85); t.pan = atod(v, 5);
      t.width = atod(v, 6, 1); t.mute = atod(v, 7) != 0; t.solo = atod(v, 8) != 0;
      t.send[0] = atod(v, 9); t.send[1] = atod(v, 10); t.out = (int)atod(v, 11, -1);
      t.folded = atod(v, 12) != 0;
      if (k == "return") { p.returns.push_back(t); cur = &p.returns.back(); }
      else { p.tracks.push_back(t); cur = &p.tracks.back(); }
      curDev = nullptr; curClip = nullptr;
    } else if (k == "tname" && cur) {
      cur->name = v.size() > 1 ? unesc(v[1]) : "";
    } else if (k == "iparam" && cur) {
      ensureParams(cur);
      size_t i = (size_t)atod(v, 1);
      if (i < cur->instParams.size()) cur->instParams[i] = atod(v, 2);
    } else if (k == "dev" && cur) {
      FxDevice d;
      d.type = (int)atod(v, 1); d.bypass = atod(v, 2) != 0; d.mix = atod(v, 3, 1); d.gainDb = atod(v, 4);
      ensureParams(&d);
      cur->devices.push_back(d);
      curDev = &cur->devices.back();
    } else if (k == "dparam" && curDev) {
      size_t i = (size_t)atod(v, 1);
      if (i < curDev->params.size()) curDev->params[i] = atod(v, 2);
    } else if (k == "enddev") {
      curDev = nullptr;
    } else if (k == "clip" && cur) {
      Clip c;
      c.start = atod(v, 1); c.len = atod(v, 2, 3840); c.muted = atod(v, 3) != 0;
      c.patternLen = atod(v, 4, 3840); c.sampleId = (int)atod(v, 5, -1); c.gain = atod(v, 6, 1);
      c.offset = atod(v, 7); c.fadeIn = atod(v, 8); c.fadeOut = atod(v, 9); c.pan = atod(v, 10);
      cur->clips.push_back(c);
      curClip = &cur->clips.back();
    } else if (k == "cname" && curClip) {
      curClip->name = v.size() > 1 ? unesc(v[1]) : "";
    } else if (k == "sname" && curClip) {
      curClip->sampleName = v.size() > 1 ? unesc(v[1]) : "";
    } else if (k == "note" && curClip) {
      Note n;
      n.start = atod(v, 1); n.len = atod(v, 2, 240); n.pitch = (int)atod(v, 3, 60);
      n.vel = (float)atod(v, 4, 0.9); n.pan = (float)atod(v, 5);
      curClip->notes.push_back(n);
    } else if (k == "endclip") {
      curClip = nullptr;
    } else if (k == "endtrack" || k == "endreturn") {
      cur = nullptr; curDev = nullptr; curClip = nullptr;
    } else if (k == "lane") {
      AutoLane l;
      l.kind = (int)atod(v, 1); l.track = (int)atod(v, 2); l.dev = (int)atod(v, 3, -1);
      l.param = (int)atod(v, 4);
      int n = (int)atod(v, 5);
      for (int i = 0; i < n; i++) l.pts.push_back({atod(v, 6 + 2 * i), atod(v, 7 + 2 * i)});
      p.autoLanes.push_back(l);
    } else if (k == "end") {
      break;
    }
  }
  if (!header) { if (err) *err = "empty file"; return false; }
  for (Track& t : p.tracks) ensureParams(&t);
  for (Track& t : p.returns) ensureParams(&t);
  *out = p;
  return true;
}

Project makeDefaultProject() {
  Project p;
  p.name = "Untitled";
  Track drum;
  drum.kind = kDrum; drum.name = "Drums"; drum.instType = 3; drum.color = 0xFFCF6EFF;
  Clip dc; dc.name = "Pattern 1"; dc.patternLen = 3840; dc.len = 3840;
  drum.clips.push_back(dc);
  Track keys;
  keys.kind = kMidi; keys.name = "Keys"; keys.instType = 1; keys.color = 0xFF8C2EFF;
  Clip kc; kc.name = "Pattern 1"; kc.patternLen = 3840; kc.len = 3840;
  keys.clips.push_back(kc);
  Track ret;
  ret.kind = kReturn; ret.name = "FX A"; ret.color = 0x3ECF6EFF;
  p.tracks = {drum, keys};
  p.returns = {ret};
  for (Track& t : p.tracks) ensureParams(&t);
  return p;
}

}  // namespace mdnative
