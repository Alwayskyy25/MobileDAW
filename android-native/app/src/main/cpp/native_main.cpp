// native_main.cpp - Android entry point: NativeActivity (android_app_glue).
// This is the entire "glue" layer - there is no Java/Kotlin code in this app.
// Lifecycle: window init -> EGL + engine start + Oboe stream open;
// input motion -> ui_core touches (logical 1280x720 space);
// main loop -> poll engine snapshot, draw UI via GLES, swap.
#ifdef __ANDROID__
#include <android/input.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <dirent.h>
#include <EGL/egl.h>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include "engine_link.h"
#include "gles_renderer.h"
#include "ui_core.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "MobileDAWNative", __VA_ARGS__)

namespace {

struct State {
  android_app* app = nullptr;
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLSurface surface = EGL_NO_SURFACE;
  EGLContext context = EGL_NO_CONTEXT;
  int width = 0, height = 0;
  bool running = false;
  bool rendererOk = false;  // false = GL pipeline dead, app stays alive anyway
  mdnative::EngineLink* engine = nullptr;
  mdnative::App ui;
  mdnative::GlesRenderer renderer;
  std::vector<mdnative::DrawCmd> drawList;
};

void termEgl(State* s) {
  s->renderer.destroy();
  s->rendererOk = false;
  if (s->display != EGL_NO_DISPLAY) {
    eglMakeCurrent(s->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (s->context != EGL_NO_CONTEXT) eglDestroyContext(s->display, s->context);
    if (s->surface != EGL_NO_SURFACE) eglDestroySurface(s->display, s->surface);
    eglTerminate(s->display);
  }
  s->display = EGL_NO_DISPLAY; s->surface = EGL_NO_SURFACE; s->context = EGL_NO_CONTEXT;
  s->width = s->height = 0;
}

// Crash hardening (v1 audit, see REBUILD-PLAN.md): every EGL result is now
// checked and any failure is NON-fatal - return false, the app stays alive
// and the next APP_CMD_INIT_WINDOW retries. v1 ignored eglInitialize /
// eglCreateContext / eglCreateWindowSurface results entirely and returned
// renderer.init() as the overall result, so one GL hiccup at launch took
// the whole app down before the first frame.
bool initEgl(State* s) {
  const EGLint attribs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE};
  s->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (s->display == EGL_NO_DISPLAY || !eglInitialize(s->display, nullptr, nullptr)) {
    LOGI("eglInitialize failed: 0x%x", eglGetError());
    termEgl(s);
    return false;
  }
  EGLConfig config; EGLint n = 0;
  eglChooseConfig(s->display, attribs, &config, 1, &n);
  if (!n) { LOGI("eglChooseConfig: no matching config"); termEgl(s); return false; }
  EGLint format; eglGetConfigAttrib(s->display, config, EGL_NATIVE_VISUAL_ID, &format);
  ANativeWindow_setBuffersGeometry(s->app->window, 0, 0, format);
  const EGLint ctxAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  s->context = eglCreateContext(s->display, config, EGL_NO_CONTEXT, ctxAttribs);
  s->surface = eglCreateWindowSurface(s->display, config, s->app->window, nullptr);
  if (s->context == EGL_NO_CONTEXT || s->surface == EGL_NO_SURFACE ||
      !eglMakeCurrent(s->display, s->surface, s->surface, s->context)) {
    LOGI("egl context/surface failed: 0x%x", eglGetError());
    termEgl(s);
    return false;
  }
  eglQuerySurface(s->display, s->surface, EGL_WIDTH, &s->width);
  eglQuerySurface(s->display, s->surface, EGL_HEIGHT, &s->height);
  // A dead renderer must not kill the app either: keep the loop and the
  // engine running; draw calls are skipped while rendererOk is false.
  s->rendererOk = s->renderer.init();
  if (!s->rendererOk) LOGI("GLES renderer init failed - drawing disabled, app continues");
  return true;
}

void handleCmd(android_app* app, int32_t cmd) {
  State* s = static_cast<State*>(app->userData);
  switch (cmd) {
    case APP_CMD_INIT_WINDOW:
      if (s->app->window && initEgl(s)) {
        s->running = true;
        // Engine creation + engine construction (mdaw::Engine) + the Oboe
        // stream open all happen here, on the main thread, inside the glue
        // callback. v1 called them bare: any exception escaping this C
        // callback (bad_alloc in the engine ctor, Oboe internals, ...) is
        // an instant process abort - "installs fine, crashes on open".
        // Now: failure degrades to a status line, the app stays open.
        try {
          if (!s->engine) {
            s->engine = mdnative::createEngineLink();
            s->ui.setEngine(s->engine);
            std::vector<int> inst, fx;
            // 48000: Oboe resamples if the device differs (SRC quality Medium,
            // same choice as bridge.cpp). Native-rate query is a v1.1 item.
            s->engine->start(48000, &inst, &fx);
            LOGI("engine started, %zu instruments / %zu fx ported", inst.size(), fx.size());
          }
          if (!s->engine->setActive(true)) {
            LOGI("audio stream open failed - UI continues without audio");
            s->ui.setStatus("AUDIO ERROR: STREAM FAILED - UI ONLY");
          }
        } catch (...) {
          LOGI("engine start threw - UI continues without audio");
          s->ui.setStatus("ENGINE ERROR: AUDIO OFF - UI ONLY");
        }
        s->ui.requestAutoLoad();  // restore autosave (fresh session) + list files
      }
      break;
    case APP_CMD_TERM_WINDOW:
      s->running = false;
      if (s->engine) {
        try { s->engine->setActive(false); } catch (...) { LOGI("engine setActive(false) threw"); }
      }
      termEgl(s);
      break;
    case APP_CMD_LOST_FOCUS:
      break;  // keep audio running in background, like a real DAW
    default:
      break;
  }
}

int32_t handleInput(android_app* app, AInputEvent* ev) {
  State* s = static_cast<State*>(app->userData);
  if (AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION || s->width <= 0) return 0;
  int32_t raw = AMotionEvent_getAction(ev);
  int32_t action = raw & AMOTION_EVENT_ACTION_MASK;
  mdnative::ViewTransform t = mdnative::ViewTransform::fit(s->width, s->height);
  auto sendPtr = [&](size_t idx, int a) {
    float lx, ly;
    t.toLogical(AMotionEvent_getX(ev, idx), AMotionEvent_getY(ev, idx), &lx, &ly);
    s->ui.onTouchP(lx, ly, a, AMotionEvent_getPointerId(ev, idx));
  };
  size_t count = AMotionEvent_getPointerCount(ev);
  switch (action) {
    case AMOTION_EVENT_ACTION_DOWN: sendPtr(0, 0); return 1;
    case AMOTION_EVENT_ACTION_UP: case AMOTION_EVENT_ACTION_CANCEL: sendPtr(0, 1); return 1;
    case AMOTION_EVENT_ACTION_POINTER_DOWN:
      sendPtr((raw & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT, 0);
      return 1;
    case AMOTION_EVENT_ACTION_POINTER_UP:
      sendPtr((raw & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT, 1);
      return 1;
    case AMOTION_EVENT_ACTION_MOVE:
      for (size_t i = 0; i < count; i++) sendPtr(i, 2);
      return 1;
    default: return 0;
  }
}

// ------------------------------------------------------------ file bridge
// The UI is platform-neutral; project save/load/list requests arrive as
// App::FileOp and are executed here against the app's external data dir
// (no Java, no SAF). Names are sanitised to a flat "<name>.mdaw" file.
std::string fileDir(android_app* app) {
  const char* d = app->activity->externalDataPath;
  if (!d || !*d) d = app->activity->internalDataPath;
  return d ? d : "";
}
bool safeName(const std::string& n) {
  if (n.empty() || n.size() > 60) return false;
  for (char c : n)
    if (!(std::isalnum((unsigned char)c) || c == ' ' || c == '-' || c == '_' || c == '.')) return false;
  return n.find("..") == std::string::npos;
}
void processFileOps(State* s) {
  mdnative::App::FileOp op;
  while (s->ui.takeFileOp(&op)) {
    try {
      std::string dir = fileDir(s->app);
      if (op.kind == 2) {  // list
        std::vector<std::string> names;
        if (DIR* d = opendir(dir.c_str())) {
          while (dirent* e = readdir(d)) {
            std::string f = e->d_name;
            if (f.size() > 5 && f.compare(f.size() - 5, 5, ".mdaw") == 0) names.push_back(f.substr(0, f.size() - 5));
          }
          closedir(d);
        }
        s->ui.deliverFileList(names);
      } else if (safeName(op.name)) {
        std::string path = dir + "/" + op.name + ".mdaw";
        if (op.kind == 0) {  // save
          if (FILE* f = std::fopen(path.c_str(), "wb")) {
            std::fwrite(op.data.data(), 1, op.data.size(), f);
            std::fclose(f);
          }
        } else {  // load
          std::string data;
          bool ok = false;
          if (FILE* f = std::fopen(path.c_str(), "rb")) {
            char buf[16384];
            size_t n;
            while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) data.append(buf, n);
            std::fclose(f);
            ok = true;
          }
          s->ui.deliverLoad(op.name, ok, data, op.silent);
        }
      }
    } catch (...) {
      LOGI("file op failed (kind %d)", op.kind);
    }
  }
}

}  // namespace

void android_main(android_app* app) {
  // CRITICAL (v1 launch-crash suspect #1, see REBUILD-PLAN.md): force the
  // linker to keep android_native_app_glue - including ANativeActivity_onCreate,
  // which NativeActivity looks up by name. Nothing in this app references a
  // glue symbol (android_main is referenced BY the glue, not the reverse),
  // so without this call the static archive member holding onCreate can be
  // dropped at link time and the activity dies the moment it opens.
  // CMakeLists.txt also passes -Wl,-u,ANativeActivity_onCreate as backup.
  app_dummy();
  State s;
  s.app = app;
  app->userData = &s;
  app->onAppCmd = handleCmd;
  app->onInputEvent = handleInput;
  while (true) {
    int ident, events;
    android_poll_source* source;
    int timeout = s.running ? 0 : -1;  // block when no window, spin (pollOnce 0 + sleep) when running
    while ((ident = ALooper_pollOnce(timeout, nullptr, &events, reinterpret_cast<void**>(&source))) >= 0) {
      if (source) source->process(app, source);
      if (app->destroyRequested) {
        if (s.engine) {
          try { s.engine->setActive(false); mdnative::destroyEngineLink(s.engine); } catch (...) {}
          s.engine = nullptr;
        }
        return;
      }
      timeout = 0;
    }
    if (s.running) {
      processFileOps(&s);
      mdnative::Snapshot snap;
      if (s.engine && s.engine->poll(&snap)) s.ui.onSnapshot(snap);
      if (s.rendererOk && s.display != EGL_NO_DISPLAY && s.surface != EGL_NO_SURFACE) {
        s.ui.draw(&s.drawList);
        s.renderer.draw(s.drawList, s.width, s.height);
        // v1 ignored the swap result and kept "running" on a dead surface
        // forever. A lost context now tears EGL down so the next
        // APP_CMD_INIT_WINDOW rebuilds it instead of spinning.
        if (!eglSwapBuffers(s.display, s.surface) && eglGetError() == EGL_CONTEXT_LOST) {
          LOGI("EGL context lost - tearing down, will re-init on next window");
          s.running = false;
          termEgl(&s);
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(8));  // ~60 fps cap with vsync
    }
  }
}
#endif  // __ANDROID__
