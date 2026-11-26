// ui_core.h - platform-neutral UI of MobileDAW Native (full web-parity UI).
//
// No Android/GL includes here: the UI produces a DrawList (coloured rects +
// text runs in a fixed 1280x720 logical space) and consumes touch points in
// the same space. native_main.cpp scales real pixels to logical space and
// hands the DrawList to the GLES renderer. This compiles and is tested on a
// desktop host (see ../../host/).
//
// The App implementation is split by screen, one file per module:
//   ui_core.cpp    core: engine sync, hit dispatch, menus/modals, undo, files
//   ui_topbar.cpp  top bar, tempo window, project menu, on-screen keyboard
//   ui_arrange.cpp Arrange view (timeline) + automation envelope editor
//   ui_editor.cpp  detail panel frame + clip editors (piano roll/step/audio)
//   ui_mixer.cpp   Mixer view
//   ui_device.cpp  Device tab (instrument + FX chain)
//   ui_keys.cpp    Keys tab (keyboard / drum pads / wheels / recording)
//   ui_browser.cpp Browser drawer (5 tabs) + Settings modal
// The project model + engine command builders live in ui_model.*.
#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "cmds.h"
#include "engine_link.h"
#include "ui_model.h"

namespace mdnative {

struct Rect {
  float x = 0, y = 0, w = 0, h = 0;
  bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
  float cx() const { return x + w / 2; }
  float cy() const { return y + h / 2; }
};

struct DrawCmd {
  enum Kind { FillRect, Text } kind = FillRect;
  Rect r;
  uint32_t color = 0xFFFFFFFFu;  // RGBA
  std::string text;              // Text only
  float textScale = 3;           // logical px per font pixel (font is 3x5)
};

// One hit-testable widget, rebuilt every draw(). Modules append hits while
// drawing; App scans back-to-front on touch-down. id ranges per module:
//   1000 topbar   2000 arrange   3000 editor   4000 mixer   5000 device
//   6000 keys     7000 browser   8000 detail frame   9000 modal layer
struct Hit {
  Rect r;
  int id = 0;
  int a = 0, b = 0;   // module payload (indices)
  double v = 0;       // module payload (value)
};

enum class View { Arrange, Mixer };
enum class DetailTab { Device, Clip, Keys };
enum class Modal { None, Menu, Tempo, Keyboard, Settings, EnvEdit, Log, Help };

// Menu contexts (App::openMenu / menuAction) and keyboard targets.
enum MenuCtx {
  kMenuNone = 0, kMenuProject, kMenuTrack, kMenuClip, kMenuAddTrack,
  kMenuStrip, kMenuDevCard, kMenuFxPick, kMenuInstPick, kMenuLoopLen,
  kMenuMetro, kMenuSnap, kMenuPatLen, kMenuAudioTo, kMenuPresetPick,
  kMenuEdTool, kMenuSamplePick, kMenuBrowserProj, kMenuCountIn, kMenuSig
};
enum KbTarget {
  kKbNone = 0, kKbProjName, kKbSaveAs, kKbSearch, kKbTrackName, kKbClipName
};

// Drag::what values (ui_core.cpp dragMove/dragEnd switch).
enum DragWhat {
  kDragNone = 0, kDragBpm, kDragKnob, kDragFader, kDragClip, kDragNote,
  kDragVel, kDragScrollArr, kDragScrollEd, kDragScrollMix, kDragScrollBro,
  kDragScrollMenu, kDragScrollDev, kDragHeadVol, kDragHeadPan, kDragEnvPt,
  kDragWheel, kDragSeek, kDragHeadW, kDragSplit, kDragSlider, kDragPaint
};

class App {
 public:
  static constexpr float kDesignW = 1280, kDesignH = 720;

  // ---- shell interface (native_main) ----
  void setEngine(EngineLink* e);
  void setStatus(const std::string& s);  // shell-level status override
  void onSnapshot(const Snapshot& s) { snap_ = s; }
  void onTouch(float lx, float ly, int action);                 // pointer 0
  void onTouchP(float lx, float ly, int action, int pid);       // any pointer
  void draw(std::vector<DrawCmd>* out);

  // ---- file bridge (native_main performs the actual IO) ----
  struct FileOp {
    int kind = 0;  // 0 = save, 1 = load, 2 = list
    std::string name;   // logical name without extension ("autosave", ...)
    std::string data;   // save payload
    bool silent = false;
  };
  bool takeFileOp(FileOp* out);
  void deliverFileList(const std::vector<std::string>& names);
  void deliverLoad(const std::string& name, bool ok, const std::string& data, bool silent);
  void requestAutoLoad();

  // ---- state accessors (host tests) ----
  const Project& project() const { return proj_; }
  Project& project() { return proj_; }
  View view() const { return view_; }
  DetailTab detailTab() const { return tab_; }
  int selTrack() const { return selTrack_; }
  int selTarget() const { return selTarget_; }  // 0 track, 1 return, 2 master
  int selClip() const { return selClip_; }
  bool playing() const { return snap_.playing; }
  bool wantPlay() const { return wantPlay_; }
  bool browserOpen() const { return browserOpen_; }
  Modal modal() const { return modal_; }
  const std::string& status() const { return status_; }
  const std::vector<std::string>& log() const { return log_; }
  double barTicks() const {
    double beat = (double)(int)(4.0 * cmds::kPPQ / (proj_.sigDen > 0 ? proj_.sigDen : 4) + 0.5);
    return beat * proj_.sigNum;
  }
  double snapTicks() const;
  const Hit* hitAt(float x, float y) const;
  const std::vector<Hit>& hits() const { return hits_; }

  // ---- engine sync (ui_core.cpp) ----
  void sendRaw(const std::string& text);        // no setup guard
  void send(const std::string& text);           // ensureSetup() first
  void ensureSetup();
  void syncTrack(int i);                        // inst+devices+mixer+notes+clips
  void syncReturn(int i);
  void syncMixerStrip(int strip);
  void syncNotes(int i);
  void syncAll();
  void checkpoint();                            // undo snapshot
  void undo();
  void redo();
  void setStatusLine(const std::string& s);
  void pushLog(const std::string& s);

  // ---- menus / modals (ui_core.cpp) ----
  struct MenuItem {
    std::string label;
    int id = 0;
    bool checked = false, disabled = false;
  };
  void openMenu(const std::string& title, int ctx, std::vector<MenuItem> items);
  void openMenuX(const std::string& title, int ctx, std::vector<MenuItem> items, int a, int b);
  void closeModal() { modal_ = Modal::None; }
  void openKeyboard(int target, const std::string& initial, const std::string& title);
  void drawModal(std::vector<DrawCmd>* o);
  void menuAction(int ctx, int id);
  void keyboardDone(int target, const std::string& text);

  // ---- module entry points ----
  void drawTopbar(std::vector<DrawCmd>* o);
  void drawArrange(std::vector<DrawCmd>* o);
  void drawDetail(std::vector<DrawCmd>* o);      // frame + tab dispatch
  void drawEditor(std::vector<DrawCmd>* o);      // Clip tab content
  void drawPianoRoll(std::vector<DrawCmd>* o, Rect body);
  void drawStepEditor(std::vector<DrawCmd>* o, Rect body);
  void drawAudioEditor(std::vector<DrawCmd>* o, Rect body);
  void drawMixer(std::vector<DrawCmd>* o);
  void drawDevice(std::vector<DrawCmd>* o);
  void drawKeys(std::vector<DrawCmd>* o);
  void drawBrowser(std::vector<DrawCmd>* o);
  void drawSettings(std::vector<DrawCmd>* o);
  void drawTempo(std::vector<DrawCmd>* o);
  void drawKeyboard(std::vector<DrawCmd>* o);
  void drawEnvEditor(std::vector<DrawCmd>* o);
  bool touchTempo(const Hit& h, int action, float x, float y);
  bool touchKeyboard(const Hit& h, int action, float x, float y);
  bool touchEnvEditor(const Hit& h, int action, float x, float y);
  void editorTapAdd(float x, float y);   // tap on empty piano-roll grid
  void editorPaint(float x, float y);    // drag-paint in the drum step editor
  void keysGeom(Rect* area, int* whites) const;
  int keysPitchAt(float x, float y) const;
  int padAt(float x, float y) const;
  int arrRowCount() const;
  float arrRowY(int row) const;
  float arrRowH(int row) const;
  int arrRowAt(float y) const;           // row index at a y, or -1
  bool touchTopbar(const Hit& h, int action, float x, float y);
  bool touchArrange(const Hit& h, int action, float x, float y);
  bool touchEditor(const Hit& h, int action, float x, float y);
  bool touchMixer(const Hit& h, int action, float x, float y);
  bool touchDevice(const Hit& h, int action, float x, float y);
  bool touchKeys(const Hit& h, int action, float x, float y);
  bool touchBrowser(const Hit& h, int action, float x, float y);
  bool touchDetail(const Hit& h, int action, float x, float y);
  bool touchModal(const Hit& h, int action, float x, float y);
  bool touchSettings(const Hit& h, int action, float x, float y);
  void dragMove(float x, float y);
  void dragEnd(float x, float y);
  // Shared drag appliers (knob kinds: 0 instParam, 1 devParam, 2 devMix,
  // 3 devGain, 4 stripPan, 5 stripWidth, 6 stripSend; final=true on release)
  void applyKnob(int kind, int b, int c, double norm01, bool final);
  void applyFader(int strip, double vol, bool final);
  void applySlider(int id, double norm01, bool final);
  void envRange(const AutoLane& l, double* lo, double* hi) const;
  // Project/file helpers
  void applyPresetToTrack(Track* t, const defs::PresetDef& p);
  void applyLoaded(Project p, const std::string& what);
  void saveToSlot(const std::string& name, bool silent);
  void loadByName(const std::string& name, bool silent);
  void keysPointer(int pid, float x, float y, int action);
  Track* selTrackPtr();
  int selStrip() const;
  Clip* selClipPtr();

 private:
  EngineLink* eng_ = nullptr;
  Snapshot snap_;
  Project proj_ = makeDefaultProject();
  View view_ = View::Arrange;
  DetailTab tab_ = DetailTab::Device;
  bool detailOpen_ = true, detailMax_ = false;
  int selTarget_ = 0;      // 0 track, 1 return, 2 master
  int selTrack_ = 0;       // index into tracks (or returns when selTarget_==1)
  int selClip_ = -1;       // clip index on the selected track
  bool wantPlay_ = false;
  bool setupDone_ = false;
  bool recArm_ = false;
  bool follow_ = true;
  int snapMode_ = 2;       // 0 off, 1 bar, 2 beat, 3 1/2, 4 1/4, 5 1/8
  std::string status_ = "MOBILEDAW NATIVE";
  std::vector<std::string> log_;
  std::vector<Hit> hits_;
  std::vector<std::string> undo_, redo_;
  bool dirty_ = false;
  long frame_ = 0;

  // topbar / transport state
  double posAtDraw_ = 0;
  float fps_ = 0;
  long lastFpsFrame_ = 0;
  double lastFpsTime_ = 0;

  // arrange geometry
  double arrTick0_ = 0;    // left-edge tick
  float arrY0_ = 0;        // top scroll (px)
  double arrPpt_ = 0.055;  // px per tick
  float rowH_ = 66;
  float headW_ = 208;
  int dragClipMode_ = 0;

  // editor geometry
  double edTick0_ = 0;
  int edTopPitch_ = 72;    // pitch at the top row (piano roll)
  double edPpt_ = 0.11;
  float edRowH_ = 22;
  int edMode_ = 0;         // drum tracks: 0 = Step, 1 = Notes
  int selNote_ = -1;
  double edLastLen_ = 240;

  // mixer geometry
  float mixX0_ = 0;

  // browser state
  bool browserOpen_ = false, browserPin_ = false;
  int browserTab_ = 0;     // 0 inst, 1 fx, 2 sample, 3 preset, 4 project
  float broScroll_[5] = {0, 0, 0, 0, 0};
  int fxGroup_ = -1;       // expanded FX group in browser / device picker
  std::string broSearch_;
  std::vector<std::string> fileList_;  // from the shell (saved projects)

  // device panel state
  float devScroll_ = 0;
  int devPickFor_ = -2;    // -2 none, -1 instrument change, >=0 insert FX at pos

  // keys state
  int keysOctave_ = 4;     // base octave for the keyboard
  float bendV_ = 0, modV_ = 0;
  struct HeldKey { int pid; int pitch; double startTick; };
  std::vector<HeldKey> held_;

  // modal state
  Modal modal_ = Modal::None;
  std::string menuTitle_;
  int menuCtx_ = 0, menuA_ = 0, menuB_ = 0;
  std::vector<MenuItem> menuItems_;
  std::vector<int> menuPresets_;  // preset-pick menu: indices into defs::kPresets
  FxDevice devClip_;              // device copy/paste clipboard
  bool devClipOk_ = false;
  float menuScroll_ = 0;
  int kbTarget_ = 0;
  std::string kbText_, kbTitle_;
  int setPage_ = 0;
  // envelope editor state
  int envLane_ = -1;       // index into proj_.autoLanes being edited
  int envSelPt_ = -1;

  // drag state (single pointer 0)
  struct Drag {
    int what = 0;          // DragWhat
    int a = 0, b = 0, c = 0;
    float x0 = 0, y0 = 0;
    double v0 = 0, v1 = 0, w0 = 0;
    double t0 = 0;
    bool moved = false;
  } drag_;

  // layout of the last draw() (used by touch routing + modules)
  Rect bodyR_, arrR_, detailR_, keysArea_;
  Rect detailContentR_, edGridR_, edVelR_, edKeysR_;
  int previewPitch_ = -1;  // editor key column preview note sounding now

  // file bridge state
  std::vector<FileOp> fileOps_;
  std::vector<std::pair<std::string, std::string>> memSlots_;
  bool autoLoadAsked_ = false;
  int autoSaveAt_ = 0;

  friend struct UiTestAccess;
};

}  // namespace mdnative
