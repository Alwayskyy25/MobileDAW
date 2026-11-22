# Harmor & NewTone — Phase 0: Audit & Arsitektur

Dokumen ini adalah hasil audit MobileDAW sebelum menulis kode Harmor dan NewTone. Semua temuan diambil dari source code
yang ada di repo ini (bukan asumsi), dan dipakai untuk menentukan di mana kedua plugin menempel ke sistem yang sudah berjalan.

## 1. Temuan audit

| Area | Implementasi yang ada | File utama |
|---|---|---|
| Audio engine | **Satu engine C++** (`native/engine/*`, ~12.800 baris). Web: dikompilasi ke WebAssembly dan dijalankan di AudioWorklet (`worklet-wasm.js`). Android: engine yang sama sebagai native C++ (Oboe). Engine JS (`js/engine/*`, `worklet.js`) hanya cadangan bila WASM gagal dimuat. | `native/engine/engine.cpp`, `js/engine/web.js`, `native/wasm/build.sh` |
| Kesetaraan JS ↔ C++ | Golden test membandingkan engine JS dan C++ sampai -90 dB; skenario native membandingkan desktop dan WASM. | `tools/golden/*` |
| Audio routing | Track → rantai device → send A/B / FX return → master (index 64 = master, 65+ = return). | `js/project.js` (`syncTrackFull`, `syncAll`) |
| Plugin host / registry | Instrumen = `INSTRUMENTS[type]` (definisi parameter) + factory engine `makeInstrument` (C++ `units.cpp`, JS `instruments.js`). Efek = `DEVICES[type]`. | `js/defs.js`, `native/engine/units.cpp` |
| Instrumen besar terbaru | Transistor Bass (34), Slicer (35), 3x Osc (36), **Moisture (37)**: DSP **hanya C++**, engine JS mengembalikan `null`. UI, definisi, preset, loader tabel: JS. | `native/engine/moist2.cpp`, `js/moistdefs.js`, `js/moistview.js` |
| Data besar ke engine | Bank sample (256 slot): JS mengirim `Float32Array` lewat `putSampleBuffers(id, l, r)`; instrumen membaca slot lewat parameter ID (Moisture: param 302/303 = wavetable). Di Android, web layer meneruskan buffer yang sama ke engine native (`nat.mirror` → `nat.sink.sample`). | `js/engine/web.js`, `js/project.js` |
| MIDI input | Web MIDI (desktop/Chromium) → preview / rekam ke track; keyboard layar; pitch bend & mod wheel (CC1). | `js/midi.js`, `js/midiio.js`, `js/keys.js` |
| Parameter | Parameter instrumen = indeks angka di `t.inst.params[k]`; definisi `{n, min, max, def, curve, step, unit/fmt}`; dikirim ke engine lewat `instParam(track, k, v)`. | `js/defs.js`, `js/project.js` |
| Automation | Lane otomasi target `kind 5` = parameter instrumen (indeks). | `js/project.js` (automation) |
| Preset | Preset pabrik per instrumen (`PRESETS`, `presetlib.js`) + file preset user di `/Presets/Instruments` (simpan/muat/hapus lewat browser). | `js/presets.js`, `js/presetlib.js` |
| Save / load | Proyek `.mdaw` = JSON (`serialize` / `deserialize`); sample disimpan sebagai path; pemuatan dua tahap (struktur dulu, audio di belakang). | `js/project.js` |
| Undo / redo | `HK.add({...})` per perubahan; grup per aksi. | `js/history.js` |
| Worker | Render stretch/pitch di pool Web Worker (sampai 4). | `js/engine/stretchworker.js`, `js/project.js` |
| Penyimpanan | IndexedDB (`mobiledaw`): indeks file + data; dicerminkan ke folder perangkat (File System Access / SAF di Android). Cache render di `/.cache/render`. | `js/engine/web.js`, `js/storage.js` |
| Offline | Service worker dengan daftar aset (`sw.js` ASSETS, dibangun `tools/build.mjs`). Tidak ada CDN. | `sw.js`, `tools/build.mjs` |
| UI | Komponen gaya Live (`liveui.js`: knob, fader, toggle, choice, display canvas), kit panel device (`fxkit.js`), jendela plugin mengambang untuk plugin besar (`floatwin.js`, contoh `moistview.js`). | `js/liveui.js`, `js/fxkit.js`, `js/floatwin.js` |
| Responsif | `uiscale.js` (skala UI), tata letak per lebar (`data-w`), tes OPPO / Pixel. | `js/uiscale.js`, `js/layout.js` |

## 2. Keputusan arsitektur

### 2.1 Bahasa DSP — temuan yang bertentangan dengan permintaan "JavaScript-first"

DSP real-time yang ditulis **hanya** di JavaScript **tidak akan berbunyi** di engine default MobileDAW:
web memakai engine C++/WASM, Android memakai engine C++ native, dan engine JS hanya cadangan. Membuat AudioWorklet
terpisah khusus Harmor berarti membuat audio host kedua di luar mixer, efek, otomasi, ekspor, dan Android native —
itu persis yang dilarang ("jangan membuat sistem audio atau plugin host baru").

Profiling (desktop, satu core, oscillator tabel per partial, 48 kHz):

| Beban | JS | C++ (-O2) |
|---|---|---|
| 128 partial × 8 voice | 16 % | 13 % |
| 516 partial × 8 voice | 28 % | 23 % |
| 516 partial × 16 voice | 55 % | 45 % |

Kesimpulan jujur: **C++ tidak jauh lebih cepat** untuk inti ini. Alasan memakai C++ adalah **arsitektur host**, bukan kecepatan.
Kedua angka juga menunjukkan oscillator naif per partial **terlalu berat untuk HP** di 516 partial, jadi Harmor memakai
sintesis yang lebih hemat (bank oscillator rotasi kompleks / IFFT overlap-add) apa pun bahasanya.

Rekomendasi (mengikuti pola Moisture):

- **Harmor**: loop suara real-time (partial, voice, filter, articulator, unison, efek internal) di **C++** di engine yang sama
  (`native/engine/harmor.*`, instrumen baru), sehingga otomatis jalan di web (WASM) **dan** Android native, ikut mixer,
  efek, otomasi, freeze, dan ekspor. **Semua yang lain JavaScript**: definisi parameter + schema, UI (MAIN / IMG / FX / ADV),
  editor harmonik/envelope, analisis gambar & audio (Web Worker) yang menghasilkan tabel partial, preset, tes.
- **NewTone**: **seluruhnya JavaScript**. NewTone bekerja pada klip audio (bukan aliran real-time), jadi cocok dengan pipeline
  non-destruktif yang sudah ada: instruksi edit disimpan di klip, analisis & rekonstruksi di Web Worker, hasilnya buffer turunan
  (seperti render stretch/pitch) yang diputar engine mana pun, termasuk Android native, dan disimpan di cache render.

### 2.2 Titik integrasi

**Harmor**
- `js/defs.js` — instrumen baru (tipe 38) dengan schema parameter ber-ID stabil + versi schema.
- `native/engine/harmor.h/.cpp` — DSP; `units.cpp` — factory `case 38`; `js/engine/instruments.js` — `case 38: return null` (seperti 34–37).
- Tabel gambar / resynthesis: dibuat di Worker JS → `putSampleBuffers` → ID slot di parameter (pola Moisture 302/303).
- UI: `js/harmor/` (view plugin + editor), didaftarkan sebagai jendela plugin seperti `moistview.js`.
- Preset: `PRESETS[38]` (pabrik) + mekanisme preset user yang sudah ada; ekspor/impor `.mdawpreset`.
- Tes: `tools/harmor-test.mjs` (render C++ desktop host, ukur frekuensi, distribusi harmonik, filter, clipping, dll.).

**NewTone**
- Data di klip audio: `c.nt = { v, analysis ref, notes[], global }` (instruksi edit, non-destruktif).
- `js/newtone/` — analisis (pYIN + autokorelasi + spektral, onset, voicing, segmentasi) di Worker, editor nada (canvas, touch),
  rekonstruksi (PSOLA/WSOLA/phase vocoder + formant) di Worker.
- Hasil render → buffer turunan klip (jalur `deriveSample` / cache render) → diputar engine.
- Tes: `tools/newtone-test.mjs` dengan sinyal uji yang diketahui (sinus, vibrato sintetis, vokal sintetis berformant).

## 3. Urutan kerja

| Fase | Hasil yang bisa diuji |
|---|---|
| 0 | Dokumen ini + keputusan arsitektur. |
| 1 | Harmor: mesin partial + timbre 1/2 + voice + MIDI; tes frekuensi dasar, distribusi harmonik, amplitudo/fase partial, blending, polifoni, clipping. |
| 2 | Pitch, filter (+ kurva respons), articulator, LFO, unison, efek. |
| 3 | Image synthesis + audio resynthesis (Worker → tabel → engine). |
| 4 | NewTone analisis: pitch, onset, voicing, segmentasi, confidence. |
| 5 | NewTone edit: koreksi (Center/Variation/Transition), timing, vibrato, formant, rekonstruksi, ekspor WAV. |
| 6 | UI lengkap gaya Live 12 untuk keduanya (portrait/landscape). |
| 7 | Integrasi preset, otomasi, proyek, undo. |
| 8 | Optimasi Android, offline, CPU/memori, kualitas audio. |

Setiap fase baru dianggap selesai bila tes audionya lulus.
