// native_main.cpp - Android entry point: NativeActivity (android_app_glue).
// This is the entire "glue" layer - there is no Java/Kotlin code in this app.
// Lifecycle: window init -> EGL + engine start + Oboe stream open;
// input motion -> ui_core touches (logical 1280x720 space);
// main loop -> poll engine snapshot, draw UI via GLES, swap.
#ifdef __ANDROID__
#include <android/input.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <chrono>
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
  int32_t action = AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_MASK;
  int a = action == AMOTION_EVENT_ACTION_DOWN ? 0 : action == AMOTION_EVENT_ACTION_UP ? 1 : action == AMOTION_EVENT_ACTION_MOVE ? 2 : -1;
  if (a < 0) return 0;
  mdnative::ViewTransform t = mdnative::ViewTransform::fit(s->width, s->height);
  float lx, ly;
  t.toLogical(AMotionEvent_getX(ev, 0), AMotionEvent_getY(ev, 0), &lx, &ly);
  s->ui.onTouch(lx, ly, a);
  return 1;
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
