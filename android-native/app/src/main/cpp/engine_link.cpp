// engine_link.cpp - Android implementation of EngineLink (see engine_link.h).
//
// Adapted from the repo's own verified Android host,
// android/app/src/main/cpp/bridge.cpp (same author, same engine): Oboe
// LowLatency/Exclusive stream, lock-free SPSC command queue applied on the
// audio thread, render in <=512-frame planar chunks, NaN/inf guard with
// eng.panic(), ~30 Hz snapshots, cost histogram. Differences vs bridge.cpp:
// no JNI/Capacitor - the native UI calls this directly; snapshots are reduced
// to what the v1 UI needs (position, playing, cpu, xruns, master meters).
#ifdef __ANDROID__
#include "engine_link.h"
#include <android/log.h>
#include <oboe/Oboe.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <thread>
#include "commands.h"  // mdaw engine API (native/engine/)

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "MobileDAWNative", __VA_ARGS__)

using namespace mdaw;

namespace {

template <class T, int N>
class Spsc {
 public:
  bool push(T v) { int h = head.load(std::memory_order_relaxed), nx = (h + 1) % N; if (nx == tail.load(std::memory_order_acquire)) return false; buf[h] = v; head.store(nx, std::memory_order_release); return true; }
  bool pop(T& v) { int t = tail.load(std::memory_order_relaxed); if (t == head.load(std::memory_order_acquire)) return false; v = buf[t]; tail.store((t + 1) % N, std::memory_order_release); return true; }
 private:
  T buf[N]; std::atomic<int> head{0}, tail{0};
};

void flushDenormals() {
#if defined(__aarch64__)
  uint64_t fpcr; asm volatile("mrs %0, fpcr" : "=r"(fpcr)); asm volatile("msr fpcr, %0" ::"r"(fpcr | (1ULL << 24)));
#elif defined(__x86_64__) || defined(__i386__)
  uint32_t csr; asm volatile("stmxcsr %0" : "=m"(csr)); csr |= 0x8040; asm volatile("ldmxcsr %0" ::"m"(csr));
#endif
}

constexpr int kNStrips = 73;  // bridge.cpp NSTRIPS (NativeEnginePlugin.java agrees)
constexpr int kHistBuckets = 400;

class Host : public oboe::AudioStreamDataCallback, public oboe::AudioStreamErrorCallback {
 public:
  explicit Host(double sr) : eng(sr), rate(sr) { eng.onError = &Host::onEngineError; eng.errCtx = this; }
  Engine eng;
  double rate;
  Spsc<Cmd*, 8192> cmds, spent;
  std::shared_ptr<oboe::AudioStream> stream;
  std::unique_ptr<oboe::LatencyTuner> tuner;
  std::atomic<bool> wantActive{false};
  std::mutex streamLock;
  bool running = false;

  // snapshot (audio thread writes ~30 Hz, UI reads)
  static constexpr int kSnapN = 8 + 2;  // pos, playing, cin, cpuPermille, xruns, bad, meterL, meterR, pdc, -
  double snap[2][kSnapN] = {};
  std::atomic<int> snapIdx{0};
  int snapFrames = 0;

  std::atomic<uint64_t> blocks{0};
  std::atomic<uint64_t> hist[kHistBuckets] = {};
  double sumCost = 0, maxCost = 0;
  std::atomic<int64_t> badBlocks{0};
  double cpuWin = 0, cpuBusy = 0, cpuNow = 0;
  int bad = 0; bool ftz = false;
  Spsc<int64_t, 64> errs;
  float L[512], R[512];

  static void onEngineError(void* ctx, int kind, int strip, int dev, const char*) {
    static_cast<Host*>(ctx)->errs.push(((int64_t)kind << 32) | ((int64_t)(strip & 0xffff) << 16) | (dev & 0xffff));
  }

  oboe::DataCallbackResult onAudioReady(oboe::AudioStream* s, void* data, int32_t frames) override {
    if (!ftz) { flushDenormals(); ftz = true; }
    if (tuner) tuner->tune();
    auto t0 = std::chrono::steady_clock::now();
    Cmd* c;
    while (cmds.pop(c)) { apply(eng, *c); if (!spent.push(c)) { /* leak rather than free here */ } }
    float* out = static_cast<float*>(data);
    int done = 0;
    while (done < frames) {
      int n = frames - done < 512 ? frames - done : 512;
      eng.render(L, R, n);
      bool ok = true;
      for (int i = 0; i < n; i++) { float v = L[i] + R[i]; if (!(v > -64 && v < 64)) { ok = false; break; } }
      if (!ok) { std::memset(L, 0, sizeof(float) * n); std::memset(R, 0, sizeof(float) * n); badBlocks++; if (++bad >= 3) { bad = 0; eng.panic(); } } else bad = 0;
      for (int i = 0; i < n; i++) { out[(done + i) * 2] = L[i]; out[(done + i) * 2 + 1] = R[i]; }
      done += n;
    }
    double cost = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(), budget = frames / rate;
    double share = cost / budget;
    int b = (int)(share * 200); if (b >= kHistBuckets) b = kHistBuckets - 1; if (b < 0) b = 0;
    hist[b].fetch_add(1, std::memory_order_relaxed); blocks.fetch_add(1, std::memory_order_relaxed);
    sumCost += share; if (share > maxCost) maxCost = share;
    cpuBusy += cost; cpuWin += budget; if (cpuWin >= 0.5) { cpuNow = cpuBusy / cpuWin; cpuBusy = 0; cpuWin = 0; }
    snapFrames += frames;
    if (snapFrames >= rate / 30) {
      snapFrames = 0;
      int w = snapIdx.load(std::memory_order_relaxed) ^ 1; double* o = snap[w];
      o[0] = eng.tickPos(); o[1] = eng.playing ? 1 : 0; o[2] = eng.cin > 0 ? 1 : 0;
      o[3] = std::round(std::fmin(1.5, cpuNow) * 1000);
      o[4] = s->getXRunCount().value(); o[5] = (double)badBlocks.load();
      int32_t m[kNStrips * 2]; eng.fillMeters(m);
      // VERIFIED 2026-11-26: engine.cpp fillMeters writes jsRound(peak * 1000)
      // (engine.h comment: "2 * NSTRIPS peaks * 1000"), so milli-peak /1000 is
      // the correct scale - the same int32 values bridge.cpp passes through.
      o[6] = std::fmin(1.0, std::fabs(m[0]) / 1000.0);
      o[7] = std::fmin(1.0, std::fabs(m[1]) / 1000.0);
      o[8] = eng.pdcOut;
      snapIdx.store(w, std::memory_order_release);
    }
    return oboe::DataCallbackResult::Continue;
  }

  void onErrorAfterClose(oboe::AudioStream*, oboe::Result r) override {
    LOGI("stream closed: %s", oboe::convertToText(r));
    running = false;
    if (wantActive.load()) running = open();
  }

  void submit(Cmd* c) {
    if (!running) { drainQueue(); apply(eng, *c); delete c; eng.retire.drain(); return; }
    while (!cmds.push(c)) { collect(); std::this_thread::yield(); }
  }
  void drainQueue() { Cmd* c; while (cmds.pop(c)) { apply(eng, *c); delete c; } collect(); }
  void collect() { Cmd* c; while (spent.pop(c)) delete c; eng.retire.drain(); }

  bool open() {
    std::lock_guard<std::mutex> g(streamLock);
    if (stream) { stream->close(); stream.reset(); }
    tuner.reset();
    oboe::AudioStreamBuilder b;
    b.setDirection(oboe::Direction::Output)->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(oboe::SharingMode::Exclusive)->setFormat(oboe::AudioFormat::Float)
        ->setChannelCount(2)->setSampleRate((int)rate)
        ->setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium)
        ->setUsage(oboe::Usage::Media)->setContentType(oboe::ContentType::Music)
        ->setDataCallback(this)->setErrorCallback(this);
    oboe::Result r = b.openStream(stream);
    if (r != oboe::Result::OK) { LOGI("openStream failed: %s", oboe::convertToText(r)); stream.reset(); return false; }
    tuner = std::make_unique<oboe::LatencyTuner>(*stream);
    r = stream->requestStart();
    if (r != oboe::Result::OK) { LOGI("requestStart failed: %s", oboe::convertToText(r)); return false; }
    LOGI("stream: %d Hz burst %d buffer %d", stream->getSampleRate(), stream->getFramesPerBurst(), stream->getBufferSizeInFrames());
    return true;
  }
  void close() {
    std::lock_guard<std::mutex> g(streamLock);
    if (stream) { stream->stop(); stream->close(); stream.reset(); }
    tuner.reset();
  }
};

class Link : public mdnative::EngineLink {
 public:
  ~Link() override { delete host_; }

  bool start(int sr, std::vector<int>* portedInst, std::vector<int>* portedFx) override {
    std::lock_guard<std::mutex> g(lock_);
    if (!host_ || host_->rate != sr) { delete host_; host_ = new Host(sr); }
    if (portedInst) { portedInst->clear(); for (int t = 0; t < 64; t++) if (instPorted(t)) portedInst->push_back(t); }
    if (portedFx) { portedFx->clear(); for (int t = 0; t < 64; t++) if (devPorted(t)) portedFx->push_back(t); }
    return true;
  }

  std::string cmd(const std::string& text) override {
    std::lock_guard<std::mutex> g(lock_);
    if (!host_) return "not started";
    host_->collect();
    std::string errs, name; std::vector<Arg> args;
    size_t p = 0;
    while (p < text.size()) {
      size_t e = text.find('\n', p); if (e == std::string::npos) e = text.size();
      std::string line = text.substr(p, e - p); p = e + 1;
      if (line.empty()) continue;
      if (!parseLine(line, name, args, nullptr, 0)) { errs += "parse: " + line.substr(0, 60) + "\n"; continue; }
      Cmd* c = new Cmd();
      if (!prepare(name, args, host_->eng, *c) && c->op < 0) { errs += c->error + "\n"; delete c; continue; }
      if (!c->error.empty()) errs += c->error + "\n";
      host_->submit(c);
    }
    return errs;
  }

  bool setActive(bool on) override {
    std::lock_guard<std::mutex> g(lock_);
    if (!host_) return false;
    host_->wantActive = on;
    if (on) { if (!host_->running) host_->running = host_->open(); return host_->running; }
    host_->close(); host_->running = false; host_->drainQueue();
    return true;
  }

  bool poll(mdnative::Snapshot* out) override {
    std::lock_guard<std::mutex> g(lock_);
    if (!host_) return false;
    host_->collect();
    const double* s = host_->snap[host_->snapIdx.load(std::memory_order_acquire)];
    out->posTicks = s[0]; out->playing = s[1] != 0; out->countIn = s[2] != 0;
    out->cpuPermille = (int)s[3]; out->xruns = (long)s[4]; out->badBlocks = (long)s[5];
    out->meterMasterL = (float)s[6]; out->meterMasterR = (float)s[7];
    out->pdcOutFrames = s[8];
    return true;
  }

  void stats(mdnative::Stats* o, bool reset) override {
    std::lock_guard<std::mutex> g(lock_);
    *o = mdnative::Stats();
    if (!host_) return;
    uint64_t n = host_->blocks.load(); o->blocks = (double)n;
    if (n) {
      o->meanPct = host_->sumCost / n * 100; o->maxPct = host_->maxCost * 100;
      uint64_t acc = 0; bool p50 = false;
      for (int b = 0; b < kHistBuckets; b++) { acc += host_->hist[b].load(); if (!p50 && acc * 2 >= n) { o->p50Pct = b * 0.5; p50 = true; } if (acc * 100 >= n * 99) { o->p99Pct = b * 0.5; break; } }
    }
    o->badBlocks = (double)host_->badBlocks.load();
    std::lock_guard<std::mutex> sg(host_->streamLock);
    if (host_->stream) {
      o->xruns = host_->stream->getXRunCount().value();
      o->burst = host_->stream->getFramesPerBurst(); o->buffer = host_->stream->getBufferSizeInFrames();
      o->sampleRate = host_->stream->getSampleRate();
      auto lat = host_->stream->calculateLatencyMillis(); if (lat) o->latencyMs = lat.value();
    }
    if (reset) { host_->blocks = 0; for (auto& h : host_->hist) h = 0; host_->sumCost = 0; host_->maxCost = 0; }
  }

 private:
  Host* host_ = nullptr;
  std::mutex lock_;  // serialises UI-thread calls; never taken by the audio thread
};

}  // namespace

namespace mdnative {
EngineLink* createEngineLink() { return new Link(); }
void destroyEngineLink(EngineLink* e) { delete e; }
}  // namespace mdnative
#endif  // __ANDROID__
