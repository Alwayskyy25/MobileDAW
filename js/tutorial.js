// Interactive tutorial (guided tour): every control of the app, chapter by chapter, spotlighted on the real screen with what it does and how
// to use it FL Studio Mobile style (tap / double tap / long press / swipe). Practice steps wait for the user to do the thing (tap Play, add a
// track, draw a clip, tap steps...) before going on; any step can be skipped. Opened from Menu -> Tutorial interaktif, and offered once on
// the first start. The spotlight leaves the control usable: four blockers around the hole catch every other tap.
import * as P from './project.js';
import * as T from './timeline.js';
import { prefs, savePrefs } from './prefs.js';
import { h, modal } from './ui.js';

const $ = (s) => document.querySelector(s);
const vis = (s) => { const e = $(s); return !!(e && !e.hidden && e.getClientRects().length); };
const rectOf = (el) => (el ? el.getBoundingClientRect() : null);
const arrRect = (k, i) => () => (T.tutorialRects(i ?? Math.max(0, P.state.sel.track || 0))[k] || null);
const view = (v) => () => { const b = $(`[data-view="${v}"]`); if (b && !b.classList.contains('on')) b.click(); };
const tab = (t) => () => { const b = $(`#dtabs [data-tab="${t}"]`); if (b && !b.classList.contains('on')) b.click(); };
const browserOff = () => { if (vis('#browser')) $('#btn-browser').click(); };
const drumIdx = () => { for (let i = P.state.tracks.length - 1; i >= 0; i--) if (P.state.tracks[i].kind === 'drum') return i; return -1; };
const firstCard = () => $('#dbody .card') || $('#dbody');
let mark = {};                                           // values remembered by a step for its check (track counts, ...)

// ---- the chapters. A step: t title, x text (what it is + how to use it), el selector / rect function, pre() before it shows,
// want() = a practice step, done when it returns true (hint = what to do).
export const CHAPTERS = [
  { id: 'top', name: 'Bar kontrol (atas)', steps: [
    { t: 'Selamat datang di MobileDAW', x: 'Tutorial ini menunjuk setiap tombol di layar yang sebenarnya. Ketuk Lanjut untuk pindah, Kembali untuk mundur, atau Lewati kapan saja. Langkah bertanda “Coba” menunggu kamu melakukannya langsung.', pre: () => { view('arr')(); browserOff(); } },
    { t: 'Browser', el: '#btn-browser', x: 'Membuka pustaka: Sample, Instrumen, Efek, Preset, Proyek dan demo. Seret sample ke arrangement untuk membuat klip audio, atau ketuk instrumen / efek untuk memasangnya di track terpilih.', want: () => vis('#browser'), hint: 'Coba: ketuk tombol Browser.' },
    { t: 'Tutup browser', el: '#btn-browser', x: 'Ketuk lagi tombol yang sama untuk menutupnya. Browser juga tertutup sendiri setelah kamu memilih sesuatu.', want: () => !vis('#browser'), hint: 'Coba: ketuk Browser sekali lagi.' },
    { t: 'Tempo (BPM)', el: '#rd-bpm', x: 'Seret ke atas / bawah untuk mengubah tempo. Ketuk untuk jendela Tempo: ketik angka, Tap Tempo, birama dan Follow.' },
    { t: 'Birama', el: '#rd-sig2', x: 'Time signature lagu (4/4, 3/4, 6/8...). Ketuk untuk mengubahnya; perubahan per bar bisa dibuat dari ruler.' },
    { t: 'Urungkan / Ulangi', el: '#btn-undo', x: 'Undo dan Redo untuk semua edit: klip, not, parameter, mixer. Riwayatnya tetap selama proyek terbuka.' },
    { t: 'Posisi lagu', el: '#rd-pos2', x: 'Posisi playhead dalam bar . ketukan . sixteenth. Ketuk ruler di arrangement untuk memindahkannya.' },
    { t: 'Ke awal', el: '#btn-rew', x: 'Mengembalikan playhead ke awal lagu (atau ke awal loop).' },
    { t: 'Play / pause', el: '#btn-play', x: 'Memutar lagu dari posisi playhead. Ketuk lagi untuk berhenti.', want: () => P.state.playing, hint: 'Coba: ketuk Play.' },
    { t: 'Berhenti', el: '#btn-play', x: 'Ketuk sekali lagi untuk berhenti. Saat berhenti, ketuk Ke awal untuk mulai dari awal lagi.', want: () => !P.state.playing, hint: 'Coba: ketuk Play lagi untuk berhenti.' },
    { t: 'Rekam', el: '#btn-rec', x: 'Merekam ke track yang tombol Arm-nya menyala: not dari keyboard / pad / controller MIDI, atau audio dari mikrofon di track audio. Count-in dan metronom diatur di Pengaturan.' },
    { t: 'Metronom', el: '#btn-metro', x: 'Klik ketukan saat memutar dan merekam. Levelnya di Pengaturan.' },
    { t: 'Loop', el: '.lvloop', x: 'Menyalakan loop arrangement. Kotak kiri = awal loop, kanan = panjangnya; seret di ruler untuk memilih rentang lalu pakai menu “Loop”.' },
    { t: 'FPS dan CPU', el: '.perf', x: 'Kelancaran layar (frame per detik) dan beban mesin audio. Kalau CPU sering penuh, naikkan buffer di Pengaturan → Performa atau bekukan (freeze) track berat.' },
    { t: 'Arrange / Mixer', el: '.seg', x: 'Berpindah antara arrangement (playlist) dan Mixer.', want: () => vis('#view-mix'), hint: 'Coba: ketuk Mixer.' },
    { t: 'Mixer', el: '#view-mix', x: 'Satu strip per track: fader volume, pan, send A/B ke track efek, mute / solo, meter, dan Master. Tahan fader atau knob untuk otomasi, MIDI CC, atau Nilai persis.', pre: view('mix') },
    { t: 'Kembali ke Arrange', el: '.seg', x: 'Ketuk Arrange untuk kembali.', want: () => vis('#view-arr'), hint: 'Coba: ketuk Arrange.' },
    { t: 'Menu proyek', el: '#btn-menu', x: 'Simpan (geser baris “Simpan…” ke kanan = Simpan cepat, seperti FL Studio Mobile), buka proyek / demo, proyek baru, impor proyek FL Studio Mobile (.flm / zip), ekspor WAV / MP3 / stem / MIDI / ZIP, tempo, nama proyek, Pengaturan dan tutorial ini.' },
  ] },
  { id: 'arr', name: 'Arrangement (playlist)', steps: [
    { t: 'Ruler', el: arrRect('ruler'), x: 'Ketuk = pindahkan playhead. Seret = pilih rentang waktu, lalu menu rentang: Insert / Duplicate / Delete / Trim / Loop. Penanda (marker) dan perubahan birama juga di sini.', pre: () => { view('arr')(); browserOff(); } },
    { t: 'Area klip', el: arrRect('clips'), x: 'Gestur FL Studio Mobile: ketuk-ganda ruang kosong = klip baru 1 bar; ketuk klip = pilih dan tampilkan kontrolnya; ketuk-ganda klip = buka editornya (piano roll / step / audio); tahan = menu klip (salin, potong, duplikat, ganti nama, warna...). Seret tepi klip untuk panjangnya, jepit dua jari untuk zoom, geser untuk menggulir.' },
    { t: 'Kolom header track', el: arrRect('header'), x: 'Setiap baris adalah satu track (channel). Ketuk header = pilih track dan tampilkan device-nya; ketuk-ganda = lipat / buka; seret tepi kirinya untuk melebarkan kolom.' },
    { t: 'Tambah track', el: arrRect('addTrack'), x: 'Menambah track: synth (MIDI, untuk instrumen), drum (step sequencer), audio (sample / rekaman), atau FX track untuk routing dan grup. Track otomasi dibuat dengan menahan sebuah kontrol.', pre: () => { mark.n = P.state.tracks.length; }, want: () => P.state.tracks.length > mark.n, hint: 'Coba: ketuk “+ Track” lalu pilih jenis track.' },
    { t: 'Nama track', el: arrRect('title'), x: 'Ketuk = pilih track. Tahan = menu track: ganti nama, warna, duplikat, hapus, freeze, simpan / impor MIDI, template drum.' },
    { t: 'Mute', el: arrRect('mute'), x: 'Membisukan track (nomor track menyala = aktif).' },
    { t: 'Solo', el: arrRect('solo'), x: 'Hanya track ini (dan track solo lain) yang berbunyi.' },
    { t: 'Arm (rekam)', el: arrRect('arm'), x: 'Track yang di-arm menerima rekaman saat tombol Rekam ditekan.' },
    { t: 'Volume dan Pan', el: arrRect('vol'), x: 'Seret ke atas / bawah untuk volume (dB) dan pan di sebelahnya. Ketuk-ganda = kembali ke 0 dB / tengah. Tahan = otomasi, MIDI CC, atau Nilai persis.' },
    { t: 'Send A / B', el: arrRect('sa'), x: 'Jumlah sinyal yang dikirim ke track efek (return) A dan B, misalnya reverb dan delay bersama.' },
  ] },
  { id: 'panel', name: 'Panel bawah: device dan keyboard', steps: [
    { t: 'Tab Device', el: '#dtabs [data-tab="dev"]', x: 'Rantai device track terpilih: instrumen lalu efek, kiri ke kanan seperti rack FL Studio Mobile. Tambah efek dari Browser → Efek.', pre: () => { view('arr')(); tab('dev')(); } },
    { t: 'Kartu device', el: firstCard, x: 'Setiap kartu satu modul. Tombol judul: ⇄ nyala / mati (bypass), ⧉ jendela mengambang, ⋯ menu (preset, reset, hapus, pindah), ▾ lipat. Knob: seret naik / turun (dua jari = halus), ketuk-ganda = nilai awal, tahan = Nilai persis (Fine tune), otomasi, MIDI CC.' },
    { t: 'Tab Keys', el: '#dtabs [data-tab="keys"]', x: 'Keyboard untuk memainkan instrumen track terpilih, atau pad untuk track drum. Ada pitch bend dan mod wheel, akor, strum, dan skala.', want: () => { const b = $('#dtabs [data-tab="keys"]'); return !!(b && b.classList.contains('on')); }, hint: 'Coba: ketuk Keys.' },
    { t: 'Ukuran panel', el: '#dtabs', x: 'Seret bilah tab ini ke atas / bawah untuk mengubah tinggi panel. Tombol di kanan: perbesar penuh atau tutup panel.' },
    { t: 'Kembali ke Device', el: '#dtabs [data-tab="dev"]', x: 'Ketuk Device untuk kembali ke rantai device.', want: () => { const b = $('#dtabs [data-tab="dev"]'); return !!(b && b.classList.contains('on')); }, hint: 'Coba: ketuk Device.' },
  ] },
  { id: 'beat', name: 'Latihan: beat pertamamu', steps: [
    { t: 'Buat beat dalam 6 langkah', x: 'Kita membuat pola drum dan memutarnya. Setiap langkah menunggu kamu melakukannya.', pre: () => { view('arr')(); browserOff(); tab('dev')(); } },
    { t: '1. Track drum', el: arrRect('addTrack'), x: 'Ketuk “+ Track” lalu pilih “Track drum (step sequencer)”.', pre: () => { mark.d = P.state.tracks.filter((t) => t.kind === 'drum').length; }, want: () => P.state.tracks.filter((t) => t.kind === 'drum').length > mark.d, hint: 'Coba: + Track → Track drum.' },
    { t: '2. Klip pola', el: () => { const i = drumIdx(); return i >= 0 ? T.tutorialRects(i).lane : null; }, x: 'Ketuk-ganda di lane track drum ini untuk membuat klip 1 bar.', want: () => { const i = drumIdx(); return i >= 0 && P.state.tracks[i].clips.length > 0; }, hint: 'Coba: ketuk-ganda di lane yang disorot.' },
    { t: '3. Buka editor step', el: () => { const i = drumIdx(); return i >= 0 ? T.tutorialRects(i).lane : null; }, x: 'Ketuk-ganda klip yang baru dibuat untuk membuka editor step.', want: () => vis('#btn-edclose'), hint: 'Coba: ketuk-ganda klipnya.' },
    { t: '4. Isi step', el: '#dbody', x: 'Ketuk kotak-kotak step untuk menaruh pukulan: misalnya kick di 1, 5, 9, 13 dan snare di 5 dan 13. Seret untuk melukis beberapa sekaligus.', want: () => { const i = drumIdx(); return i >= 0 && P.state.tracks[i].clips.some((c) => (c.notes || []).length >= 3); }, hint: 'Coba: ketuk minimal 3 step.' },
    { t: '5. Putar', el: '#btn-play', x: 'Tekan Play dan dengarkan polanya berulang.', want: () => P.state.playing, hint: 'Coba: ketuk Play.' },
    { t: '6. Simpan', el: '#btn-menu', x: 'Hentikan pemutaran, lalu buka Menu dan geser baris “Simpan…” ke kanan untuk Simpan cepat (atau ketuk untuk memberi nama). Selamat, beat pertamamu jadi!' },
  ] },
  { id: 'flm', name: 'Alur FL Studio Mobile', steps: [
    { t: 'Impor proyek FL Studio Mobile', el: '#btn-menu', x: 'Menu → Impor → Proyek FLM: pilih file .flm atau ZIP (proyek + folder My Samples). Klip audio, stretch, pitch, level, pan, efek FLM dan instrumen (DirectWave, GMS, MiniSynth, SuperSaw, 3x Osc, Slicer) ikut terbaca. Jendela progres menunjukkan sample yang dimuat dan dirender.' },
    { t: 'DirectWave: WAV, MP3, DWP, multi-sample', el: firstCard, x: 'Di track DirectWave, ketuk tombol Zona pada layarnya: Muat WAV / MP3 / program DWP, atau “Buat dari sample” untuk memetakan banyak file sekaligus menurut namanya (mis. Piano_C4_v64.wav = nada C4, layer velocity sampai 64).' },
    { t: 'CTRL dan Nilai persis', el: firstCard, x: 'Seperti tombol CTRL di FL Studio Mobile: tahan kontrol mana pun untuk Nilai persis (ketik angka), membuat track otomasi, menautkan ke track otomasi terpilih, atau menghubungkan ke knob controller MIDI.' },
    { t: 'Ekspor', el: '#btn-menu', x: 'Menu → Ekspor → Audio: WAV 16 / 24 / 32 bit atau MP3, seluruh lagu / loop / pilihan, stem per track, sample rate, normalisasi. Juga MIDI, proyek .mdaw, dan ZIP berisi proyek + sample.' },
    { t: 'Selesai', x: 'Kamu sudah mengenal semua bagian utama. Tutorial ini selalu bisa dibuka lagi dari Menu → Tutorial interaktif, mulai dari bab mana pun.' },
  ] },
];

// ---------------------------------------------------------------- runner
let cur = null;
export const isRunning = () => !!cur;
export function startTutorial(chapterId) {
  stopTutorial();
  const steps = [], from = Math.max(0, CHAPTERS.findIndex((c) => c.id === chapterId));
  for (const c of CHAPTERS.slice(chapterId ? from : 0)) for (const s of c.steps) steps.push({ ...s, ch: c.name });
  const root = h('div', { class: 'tut' }), blk = [0, 1, 2, 3].map(() => h('div', { class: 'tut-blk' })), ring = h('div', { class: 'tut-ring' });
  const card = h('div', { class: 'tut-card', role: 'dialog' });
  root.append(...blk, ring, card); (document.getElementById('layer') || document.body).before(root);   // in #layer's stacking context, under it: menus and dialogs opened during a step stay on top
  cur = { steps, k: 0, root, blk, ring, card, timer: 0, doneAt: 0 };
  show(0);
  cur.timer = setInterval(tick, 200);
  return cur;
}
export function stopTutorial(finished) {
  if (!cur) return;
  clearInterval(cur.timer); cur.root.remove(); cur = null;
  if (finished !== undefined) { prefs.tutorialSeen = true; savePrefs(); }
}
function target(s) {
  if (!s.el) return null;
  let r = null;
  try { r = typeof s.el === 'string' ? rectOf($(s.el)) : (() => { const v = s.el(); return v && v.getBoundingClientRect ? rectOf(v) : v; })(); } catch (_) { r = null; }
  return r && r.width > 0 && r.height > 0 ? r : null;
}
function place(r) {
  const { blk, ring, card } = cur, W = innerWidth, H = innerHeight, pad = 4;
  if (!r) { blk[0].style.cssText = `left:0;top:0;width:${W}px;height:${H}px`; for (let k = 1; k < 4; k++) blk[k].style.cssText = 'display:none'; ring.style.display = 'none'; }
  else {
    const x = Math.max(0, r.left - pad), y = Math.max(0, r.top - pad), x2 = Math.min(W, r.left + r.width + pad), y2 = Math.min(H, r.top + r.height + pad);
    blk[0].style.cssText = `left:0;top:0;width:${W}px;height:${y}px`; blk[1].style.cssText = `left:0;top:${y2}px;width:${W}px;height:${H - y2}px`;
    blk[2].style.cssText = `left:0;top:${y}px;width:${x}px;height:${y2 - y}px`; blk[3].style.cssText = `left:${x2}px;top:${y}px;width:${W - x2}px;height:${y2 - y}px`;
    ring.style.cssText = `display:block;left:${x}px;top:${y}px;width:${x2 - x}px;height:${y2 - y}px`;
  }
  const cw = Math.min(360, W - 16), ch = card.offsetHeight || 160;
  let left = r ? r.left + r.width / 2 - cw / 2 : (W - cw) / 2, top = r ? (r.top + r.height + 12 + ch < H ? r.top + r.height + 12 : r.top - 12 - ch) : (H - ch) / 2;
  if (r && (top < 8 || (top + ch > H - 8))) top = Math.max(8, Math.min(H - ch - 8, r.top < H / 2 ? H - ch - 12 : 12));   // a tall target: the card sits at the far edge
  left = Math.max(8, Math.min(W - cw - 8, left));
  card.style.cssText = `left:${left}px;top:${Math.max(8, top)}px;width:${cw}px`;
}
function show(k) {
  if (!cur) return;
  const s = cur.steps[k]; if (!s) { stopTutorial(true); return; }
  cur.k = k; cur.doneAt = 0;
  if (s.pre) { try { s.pre(); } catch (_) { /* a missing part: the step still explains */ } }
  const n = cur.steps.length, want = !!s.want;
  const next = h('button', { class: 'b go tut-next', onclick: () => show(k + 1) }, k + 1 >= n ? 'Selesai' : 'Lanjut');
  if (want) next.disabled = true;
  cur.card.replaceChildren(
    h('div', { class: 'tut-ch' }, s.ch + '  •  ' + (k + 1) + ' / ' + n),
    h('h4', {}, s.t), h('p', {}, s.x),
    want ? h('div', { class: 'tut-try' }, s.hint || 'Coba sekarang.') : null,
    h('div', { class: 'tut-acts' },
      h('button', { class: 'b tut-skip', onclick: () => stopTutorial(false) }, 'Tutup'),
      h('span', { style: 'flex:1' }),
      k > 0 ? h('button', { class: 'b', onclick: () => show(k - 1) }, 'Kembali') : null,
      want ? h('button', { class: 'b', onclick: () => show(k + 1) }, 'Lewati') : null,
      next));
  cur.next = next; cur.want = want;
  place(target(s)); requestAnimationFrame(() => cur && place(target(s)));
}
function tick() {
  if (!cur) return;
  const s = cur.steps[cur.k];
  place(target(s));
  if (cur.want && !cur.doneAt && s.want && (() => { try { return s.want(); } catch (_) { return false; } })()) {
    cur.doneAt = Date.now(); cur.next.disabled = false; cur.card.classList.add('ok');
    const tr = cur.card.querySelector('.tut-try'); if (tr) tr.textContent = '✓ Berhasil!';
    const k = cur.k; setTimeout(() => { if (cur && cur.k === k) { cur.card.classList.remove('ok'); show(k + 1); } }, 900);
  }
}

// Menu -> Tutorial interaktif: start from the beginning or from one chapter
export function tutorialMenu() {
  const list = h('div', { class: 'tut-list' }, ...CHAPTERS.map((c, k) => h('button', { class: 'b', onclick: () => { close(); startTutorial(c.id); } }, (k + 1) + '. ' + c.name + '  (' + c.steps.length + ' langkah)')));
  const close = modal('Tutorial interaktif', h('div', {}, h('div', { class: 'hint', style: 'margin-bottom:8px' }, 'Setiap langkah menyorot tombol di layar dan menjelaskan cara memakainya. Langkah “Coba” menunggu kamu melakukannya langsung.'), list),
    [{ label: 'Tutup' }, { label: 'Mulai dari awal', go: true, fn: () => startTutorial() }]);
}
// first start: a small offer at the bottom (never while automated tests drive the page)
export function offerTutorial() {
  if (prefs.tutorialSeen || navigator.webdriver) return;
  const chip = h('div', { class: 'tut-offer' }, h('span', {}, 'Baru di MobileDAW?'),
    h('button', { class: 'b go', onclick: () => { chip.remove(); startTutorial(); } }, 'Mulai tutorial'),
    h('button', { class: 'b', onclick: () => { chip.remove(); prefs.tutorialSeen = true; savePrefs(); } }, 'Nanti'));
  document.body.append(chip);
}
