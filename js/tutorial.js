// Interactive tutorial (guided tour): every control of the app, chapter by chapter, spotlighted on the real screen with what it does and how
// to use it FL Studio Mobile style (tap / double tap / long press / swipe). Practice steps wait for the user to do the thing (tap Play, add a
// track, draw a clip, tap steps...) before going on; any step can be skipped. A showcase chapter opens "Neon Horizon", a 2-minute progressive
// house song made with the built-in instruments (demos/progressive-house.mdaw, tools/make-proghouse.mjs), and takes it apart track by track
// while it plays. Animated: the card springs in, the spotlight pulses and glides, an animated finger shows the gesture, every chapter opens
// with a splash, a live timeline follows the song's sections, success bursts and a confetti finale (all off with "reduce motion").
// Opened from Menu -> Tutorial interaktif, and offered once on the first start. The spotlight leaves the control usable: four blockers
// around the hole catch every other tap.
import * as P from './project.js';
import * as T from './timeline.js';
import { prefs, savePrefs } from './prefs.js';
import { h, modal, toast } from './ui.js';

const $ = (s) => document.querySelector(s);
const vis = (s) => { const e = $(s); return !!(e && !e.hidden && e.getClientRects().length); };
const rectOf = (el) => (el ? el.getBoundingClientRect() : null);
const arrRect = (k, i) => () => {
  const ti = i ?? Math.max(0, P.state.sel.track || 0);
  if (!cur || cur.shown !== cur.k) { if (k === 'addTrack') T.revealRow(T.addRowIndex()); else if (k !== 'ruler' && k !== 'clips' && k !== 'header') T.revealRow(ti); if (cur) cur.shown = cur.k; }   // once per step: scroll it into view
  return T.tutorialRects(ti)[k] || null;
};
const view = (v) => () => { const b = $(`[data-view="${v}"]`); if (b && !b.classList.contains('on')) b.click(); };
const tab = (t) => () => { const b = $(`#dtabs [data-tab="${t}"]`); if (b && !b.classList.contains('on')) b.click(); };
const browserOff = () => { if (vis('#browser')) $('#btn-browser').click(); };
const drumIdx = () => { for (let i = P.state.tracks.length - 1; i >= 0; i--) if (P.state.tracks[i].kind === 'drum') return i; return -1; };
const firstCard = () => $('#dbody .card') || $('#dbody');
let mark = {};                                           // values remembered by a step for its check (track counts, ...)

// ---- the showcase song helpers
const BAR = 4 * P.PPQ;
const idx = (name) => P.state.tracks.findIndex((t) => t.name === name);
const isShow = () => /Neon Horizon/.test(P.state.projectName || '');
const lane = (name) => () => { const i = idx(name); if (i < 0) return null; if (cur && cur.shown !== cur.k) { T.revealRow(i); cur.shown = cur.k; } return T.tutorialRects(i).lane; };
const head = (name, part) => () => { const i = idx(name); if (i < 0) return null; if (cur && cur.shown !== cur.k) { T.revealRow(i); cur.shown = cur.k; } return T.tutorialRects(i)[part]; };
const sel = (name) => () => { view('arr')(); const i = idx(name); if (i >= 0) { P.selectTrack(i); tab('dev')(); } };
const card = (name) => () => { const c = [...document.querySelectorAll('#dbody .card')].find((x) => (x.textContent || '').slice(0, 80).includes(name)) || firstCard(); if (c && cur && cur.shown !== cur.k) { c.scrollIntoView({ block: 'nearest', inline: 'nearest' }); cur.shown = cur.k; } return c; };
const playFrom = (b) => () => { P.seekTo((b - 1) * BAR); if (!P.state.playing) P.togglePlay(); };
const stopPlay = () => { if (P.state.playing) P.togglePlay(); };
const soloOnly = (name) => () => { P.state.tracks.forEach((t, i) => { if (t.solo && t.name !== name) P.toggleSolo(i); }); const i = idx(name); if (i >= 0 && !P.state.tracks[i].solo) P.toggleSolo(i); };
const unsoloAll = () => P.state.tracks.forEach((t, i) => { if (t.solo) P.toggleSolo(i); });
async function openShowcase() {
  if (isShow()) return;
  const D = await import('./demos.js'), d = D.DEMOS.find((x) => x.id === 'neon-horizon');
  stopPlay(); await D.openDemo(d);
  view('arr')(); browserOff(); T.fitNow && T.fitNow();
}

// ---- the chapters. A step: t title, x text (what it is + how to use it), el selector / rect function, pre() before it shows,
// want() = a practice step, done when it returns true (hint = what to do).
export const CHAPTERS = [
  { id: 'show', name: 'Showcase: Progressive House 2 menit', ic: '\u266b', steps: [
    { t: 'Neon Horizon \u2014 dibongkar langkah demi langkah', live: false, x: 'Lagu progressive house asli 2 menit (128 BPM, F minor, 64 bar) yang dibuat hanya dengan instrumen bawaan MobileDAW: SuperSaw, MiniSynth, GMS dan drum sampler. Kita buka proyeknya, putar, lalu bongkar satu per satu: struktur, drum, bass dengan sidechain, arpeggio, pad bersaring, lead, riser, efek dan master.',
      tips: ['Proyekmu yang sekarang tidak hilang: autosave menyimpannya, dan kamu bisa \u201cSimpan\u201d dulu lewat tombol di bawah.', 'Semua langkah bisa diulang (Kembali) atau dilewati. Langkah \u201cCoba\u201d menunggu kamu melakukannya sendiri.', 'Pakai headphone atau speaker yang punya bass agar kick dan sidechain terdengar jelas.'],
      acts: [{ label: 'Simpan proyekku dulu', when: async () => (await import('./quicksave.js')).isDirty(), fn: async () => (await import('./quicksave.js')).quickSave() }] },
    { t: 'Membuka proyek showcase', el: '#arr', pre: openShowcase, x: 'Proyek \u201cNeon Horizon\u201d terbuka: 9 track (Kick, Drums, Cymbals, Bass, Pluck Arp, Pad, Lead, Riser, Noise Riser), 4 lane otomasi, dua track efek (Reverb dan Delay) dan rantai Master. Marker berwarna di ruler menandai bagian lagu.',
      tips: ['Jendela progres tadi menyalin 8 sample drum (NH Kick, Clap, Hat, Snare, Crash, Ride, Noise) ke Samples/Demo lalu memuat semuanya.', 'Sample-nya berlisensi CC0: boleh dipakai di lagumu sendiri, dijual, atau dibagikan tanpa izin dan tanpa kredit.', 'Klip berulang (loop) tidak menyalin not: satu pola 1 bar diputar sepanjang bagian, jadi edit sekali = berubah di semua pengulangan.', 'Proyek juga tersimpan di Proyek \u2192 \u201cNeon Horizon\u201d, jadi bisa kamu ubah dan simpan dengan namamu sendiri.'] },
    { t: 'Dengarkan dulu', el: '#btn-play', live: true, x: 'Putar dari awal atau langsung lompat ke Drop. Garis waktu di kartu ini mengikuti lagu secara langsung: bagian yang sedang berbunyi menyala.',
      tips: ['Energi lagu naik turun: Intro tipis \u2192 Build menambah lapisan \u2192 Breakdown tanpa kick \u2192 Drop paling penuh \u2192 Outro menipis untuk DJ.', 'Progressive house berpikir per 8 / 16 bar: setiap 8 bar ada elemen yang masuk atau keluar supaya telinga tidak bosan.'],
      acts: [{ label: '\u25b6 Dari awal', fn: playFrom(1) }, { label: '\u25b6 Ke Drop', go: true, fn: playFrom(33) }, { label: '\u25a0 Stop', fn: stopPlay }] },
    { t: 'Struktur lagu (marker)', el: arrRect('ruler'), live: true, gest: 'tap', x: 'Ruler memuat 7 marker: Intro (bar 1), Build (9), Groove (17), Breakdown (25), Drop (33), Drop B (49), Outro (57). Ketuk ruler untuk memindahkan playhead; tahan marker untuk mengganti nama, warna atau menghapusnya.',
      tips: ['Intro dan Outro hanya kick, hat dan pad: memudahkan DJ mencampur lagu.', 'Breakdown (8 bar) membuang kick dan bass, memperkenalkan melodi dengan pelan, lalu membangun ketegangan dengan riser dan snare roll.', 'Drop B menaikkan lead satu oktaf: cara murah menambah energi tanpa menulis melodi baru.'],
      acts: [{ label: 'Intro', fn: playFrom(1) }, { label: 'Breakdown', fn: playFrom(25) }, { label: 'Drop', go: true, fn: playFrom(33) }, { label: 'Outro', fn: playFrom(57) }] },
    { t: 'Kick 4/4 \u2014 jantung lagu', el: lane('Kick'), pre: sel('Kick'), gest: 'dtap', live: true, x: 'Kick (sample \u201cNH Kick\u201d, nada dasarnya sekitar C, kuint dari F minor) memukul setiap ketukan (four-on-the-floor) di track sendiri. Dipisah dari drum lain karena kick adalah sumber sidechain: bass, pad dan pluck \u201cmenunduk\u201d setiap kick memukul. Ketuk-ganda klipnya untuk melihat polanya di editor step.',
      tips: ['EQ Eight di track ini memotong di bawah 35 Hz (rumble yang tidak terdengar tapi memakan headroom) dan menambah sedikit \u201cklik\u201d di 3,5 kHz agar kick terdengar di speaker HP.', 'Kick paling keras di mix (sekitar -15 dB RMS sendirian); elemen lain diatur relatif terhadapnya.'],
      acts: [{ label: 'Solo Kick', fn: soloOnly('Kick') }, { label: 'Lepas solo', fn: unsoloAll }, { label: '\u25b6 Groove', go: true, fn: playFrom(17) }] },
    { t: 'Drums: clap, hat offbeat, snare roll', el: lane('Drums'), pre: sel('Drums'), live: true, x: 'Sample bergaya 909: clap di ketukan 2 dan 4, hat tertutup di offbeat (\u201cand\u201d), open hat di Drop, ghost hat 16-an dengan velocity rendah, dan snare roll yang makin rapat (1/4 \u2192 1/8 \u2192 1/16 \u2192 1/32) di bar 29\u201332.',
      tips: ['Hat offbeat adalah ciri house: kick di \u201c1\u201d, hat di \u201cand\u201d menciptakan ayunan naik turun.', 'Ghost hat diberi velocity 35\u201350% supaya menambah gerak tanpa menutupi clap.', 'Snare roll velocity-nya naik perlahan: telinga merasakan \u201csesuatu akan datang\u201d sebelum Drop.', 'Track Cymbals memberi crash di awal setiap bagian dan ride di setiap ketukan pada Drop B; EQ-nya membuang semua di bawah 300 Hz.'],
      acts: [{ label: '\u25b6 Snare roll', go: true, fn: playFrom(29) }, { label: 'Solo Drums', fn: soloOnly('Drums') }, { label: 'Lepas solo', fn: unsoloAll }] },
    { t: 'Bass bergulir + sidechain', el: card('Autoduck'), pre: sel('Bass'), live: true, x: 'MiniSynth \u201cRound Saw Bass\u201d memainkan not dasar akor di 16-an offbeat (root, oktaf, root) \u2014 bass \u201crolling\u201d khas progressive house. Kartu Autoduck yang disorot adalah sidechain: sumbernya track Kick, jadi setiap kick memukul bass turun 14 dB lalu naik lagi dalam 160 ms.',
      tips: ['Sidechain membuat ruang untuk kick (keduanya hidup di frekuensi rendah) dan menciptakan \u201cpumping\u201d yang membuat musik terasa bernapas.', 'Coba matikan Autoduck dengan tombol \u21c4 di kartunya saat Drop berbunyi: bass jadi rata dan kick tenggelam.', 'Utility +5 dB dan EQ (potong 30 Hz, kurangi 280 Hz) menjaga low end bersih.'],
      acts: [{ label: '\u25b6 Drop', go: true, fn: playFrom(33) }, { label: 'Solo Bass', fn: soloOnly('Bass') }, { label: 'Lepas solo', fn: unsoloAll }] },
    { t: 'Pluck arpeggio + delay 3/16', el: card('Delay'), pre: sel('Pluck Arp'), live: true, x: 'GMS \u201cPluck Arp\u201d memainkan nada-nada akor satu per satu di 16-an (arpeggio). Delay tempo-sync 3/16 (dotted eighth) mengisi celah di antara not dengan gema yang jatuh di luar ketukan.',
      tips: ['Delay 3/16 adalah trik klasik: gema jatuh di antara not asli dan menciptakan pola baru tanpa not tambahan.', 'Filter di delay (2,4 kHz) membuat gema lebih redup dari not asli, jadi tidak bertabrakan dengan lead.', 'Pluck juga di-sidechain ke kick, lebih halus dari bass (9 dB).'] },
    { t: 'Pad + filter yang bergerak (otomasi)', el: lane('Auto: Pad \u2022 Auto Filter Freq'), pre: () => { view('arr')(); }, gest: 'drag', live: true, x: 'Pad SuperSaw \u201cWarm Saw Pad\u201d memainkan akor Fm \u2013 Db \u2013 Ab \u2013 Eb (i\u2013VI\u2013III\u2013VII), dua bar per akor, sepanjang lagu. Yang membuatnya hidup adalah lane otomasi ini: cutoff Auto Filter membuka dari 500 Hz ke 6 kHz di Intro, ditutup di awal Breakdown lalu menyapu sampai 16 kHz, dan menutup lagi di Outro.',
      tips: ['Seret titik di lane otomasi untuk mengubah kurvanya; ketuk-ganda untuk menambah titik.', 'Filter sweep adalah alat utama progressive house: telinga mengikuti terangnya suara sebagai tanda energi.', 'Lane otomasi dibuat dengan menahan knob mana pun \u2192 \u201cOtomatisasi\u201d.'] },
    { t: 'Lead: hook 8 bar', el: lane('Lead'), pre: sel('Lead'), gest: 'dtap', live: true, x: 'SuperSaw \u201cAnthem Lead\u201d memainkan hook 8 bar di atas akor yang sama. Di Breakdown dimainkan pelan (velocity 45%), di Drop penuh, dan di Drop B satu oktaf lebih tinggi. Ketuk-ganda klipnya untuk melihat not di piano roll.',
      tips: ['Hook mengikuti nada akor di ketukan kuat (C, Ab, F, Eb) dan memakai nada lewat di ketukan lemah.', 'Ritme panjang-pendek (1,5 + 0,5 ketukan) membuat melodi mudah diingat.', 'Echo 3/16 + kirim ke Reverb memberi ruang tanpa membuat lead jauh.'],
      acts: [{ label: '\u25b6 Breakdown', fn: playFrom(25) }, { label: '\u25b6 Drop B', go: true, fn: playFrom(49) }] },
    { t: 'Riser: ketegangan sebelum Drop', el: lane('Auto: Riser \u2022 Pitch'), pre: () => { view('arr')(); }, live: true, x: 'Dua riser bekerja bersama di 4 bar terakhir Breakdown: track Riser menahan akor F dan lane otomasi Pitch menaikkannya satu oktaf penuh (+12 semitone), sementara Noise Riser (white noise 7,5 detik = tepat 4 bar di 128 BPM) dibuka filternya dari 250 Hz ke 18 kHz. Bersama snare roll dan filter pad yang membuka, inilah build-up yang dilepaskan di Drop.',
      tips: ['Kirim reverb besar (45%) membuat riser terdengar seperti \u201cmenyedot\u201d ruangan.', 'Riser berhenti tepat di bar 33: keheningan sepersekian detik sebelum kick masuk membuat Drop terasa lebih besar.'],
      acts: [{ label: '\u25b6 Build-up', go: true, fn: playFrom(29) }] },
    { t: 'Track efek: Reverb dan Delay bersama', el: head('Lead', 'sa'), pre: sel('Lead'), x: 'Send A dan B di header track mengirim sebagian sinyal ke track efek Reverb (A) dan Delay (B). Satu reverb dipakai bersama oleh drum, pluck, pad, lead dan riser: semuanya terdengar di \u201cruangan\u201d yang sama dan CPU lebih hemat.',
      tips: ['Send = efek paralel (suara asli tetap utuh). Insert (kartu di rantai track) = efek seri.', 'Reverb memotong di bawah 250 Hz (Lo Cut) supaya low end tetap rapat.', 'Seret nilai Send ke atas / bawah untuk mengatur seberapa basah sebuah track.'] },
    { t: 'Master: Glue Compressor + Limiter', el: '#view-mix', pre: view('mix'), x: 'Di Mixer, strip Master memuat Glue Compressor (menyatukan mix, threshold -14 dB) lalu Limiter (ceiling -0,5 dB) sebagai pengaman puncak. Lane otomasi Master Volume memudarkan 4 bar terakhir.',
      tips: ['Gain staging: puncak mix sebelum master di sekitar -3 dB memberi ruang untuk compressor dan limiter bekerja halus.', 'Kick sekitar -15 dB, lead -17, bass -21, pad -20, pluck dan drum -27 dB RMS: hasil pengukuran mix showcase ini.', 'Ekspor: Menu \u2192 Ekspor \u2192 Audio. Pilih WAV 24-bit untuk mastering lanjutan atau MP3 320 untuk dibagikan.'] },
    { t: 'Coba: matikan Lead', el: head('Lead', 'mute'), pre: () => { view('arr')(); playFrom(33)(); }, gest: 'tap', live: true, x: 'Saat Drop berbunyi, ketuk tombol nomor track Lead untuk membisukannya. Dengarkan berapa banyak ruang yang ditinggalkan lead di tengah mix.', want: () => { const i = idx('Lead'); return i >= 0 && P.state.tracks[i].mute; }, hint: 'Coba: ketuk tombol nomor track Lead (mute).' },
    { t: 'Coba: nyalakan lagi', el: head('Lead', 'mute'), gest: 'tap', live: true, x: 'Ketuk lagi untuk menyalakannya. Mute / Solo adalah alat utama saat mixing: bandingkan, dengarkan, putuskan.', want: () => { const i = idx('Lead'); return i >= 0 && !P.state.tracks[i].mute; }, hint: 'Coba: ketuk tombol yang sama.' },
    { t: 'Remix lagu ini!', x: 'Showcase selesai. Ide untuk mencoba: ganti preset Lead (tahan judul kartu \u2192 Preset), ubah tempo ke 124 BPM untuk nuansa deep, tambahkan Chorus ke Pad, gambar ulang kurva filter, atau rekam melodimu sendiri di track baru. Simpan dengan nama baru lewat Menu \u2192 Simpan.', finale: true, pre: () => { stopPlay(); unsoloAll(); },
      tips: ['Lagu ini dibuat oleh tools/make-proghouse.mjs dengan API proyek MobileDAW sendiri: semua yang kamu dengar bisa kamu ubah.', 'Bab berikutnya di daftar tutorial mengenalkan setiap tombol aplikasi satu per satu.'] },
  ] },
  { id: 'top', name: 'Bar kontrol (atas)', ic: '\u25b6', steps: [
    { t: 'Selamat datang di MobileDAW', x: 'Tutorial ini menunjuk setiap tombol di layar yang sebenarnya. Ketuk Lanjut untuk pindah, Kembali untuk mundur, atau Lewati kapan saja. Langkah bertanda “Coba” menunggu kamu melakukannya langsung.', pre: () => { view('arr')(); browserOff(); } },
    { t: 'Browser', el: '#btn-browser', x: 'Membuka pustaka: Sample, Instrumen, Efek, Preset, Proyek dan demo. Seret sample ke arrangement untuk membuat klip audio, atau ketuk instrumen / efek untuk memasangnya di track terpilih.', want: () => vis('#browser'), hint: 'Coba: ketuk tombol Browser.' },
    { t: 'Tutup browser', el: '#btn-browser', x: 'Ketuk lagi tombol yang sama untuk menutupnya. Browser juga tertutup sendiri setelah kamu memilih sesuatu.', want: () => !vis('#browser'), hint: 'Coba: ketuk Browser sekali lagi.' },
    { t: 'Tempo (BPM)', el: '#rd-bpm', gest: 'drag', tips: ['House 120\u2013128, techno 125\u2013135, hip hop 80\u201395, drum & bass 170\u2013175 BPM.', 'Klip audio yang di-warp mengikuti tempo baru otomatis.'], x: 'Seret ke atas / bawah untuk mengubah tempo. Ketuk untuk jendela Tempo: ketik angka, Tap Tempo, birama dan Follow.' },
    { t: 'Birama', el: '#rd-sig2', x: 'Time signature lagu (4/4, 3/4, 6/8...). Ketuk untuk mengubahnya; perubahan per bar bisa dibuat dari ruler.' },
    { t: 'Urungkan / Ulangi', el: '#btn-undo', x: 'Undo dan Redo untuk semua edit: klip, not, parameter, mixer. Riwayatnya tetap selama proyek terbuka.' },
    { t: 'Posisi lagu', el: '#rd-pos2', x: 'Posisi playhead dalam bar . ketukan . sixteenth. Ketuk ruler di arrangement untuk memindahkannya.' },
    { t: 'Ke awal', el: '#btn-rew', x: 'Mengembalikan playhead ke awal lagu (atau ke awal loop).' },
    { t: 'Play / pause', el: '#btn-play', gest: 'tap', x: 'Memutar lagu dari posisi playhead. Ketuk lagi untuk berhenti.', want: () => P.state.playing, hint: 'Coba: ketuk Play.' },
    { t: 'Berhenti', el: '#btn-play', x: 'Ketuk sekali lagi untuk berhenti. Saat berhenti, ketuk Ke awal untuk mulai dari awal lagi.', want: () => !P.state.playing, hint: 'Coba: ketuk Play lagi untuk berhenti.' },
    { t: 'Rekam', el: '#btn-rec', tips: ['Aktifkan count-in 1\u20132 bar di Pengaturan agar sempat bersiap.', 'Setiap rekaman ulang di tempat yang sama menjadi take baru; yang terakhir yang berbunyi.'], x: 'Merekam ke track yang tombol Arm-nya menyala: not dari keyboard / pad / controller MIDI, atau audio dari mikrofon di track audio. Count-in dan metronom diatur di Pengaturan.' },
    { t: 'Metronom', el: '#btn-metro', x: 'Klik ketukan saat memutar dan merekam. Levelnya di Pengaturan.' },
    { t: 'Loop', el: '.lvloop', x: 'Menyalakan loop arrangement. Kotak kiri = awal loop, kanan = panjangnya; seret di ruler untuk memilih rentang lalu pakai menu “Loop”.' },
    { t: 'FPS dan CPU', el: '.perf', x: 'Kelancaran layar (frame per detik) dan beban mesin audio. Kalau CPU sering penuh, naikkan buffer di Pengaturan → Performa atau bekukan (freeze) track berat.' },
    { t: 'Arrange / Mixer', el: '.seg', x: 'Berpindah antara arrangement (playlist) dan Mixer.', want: () => vis('#view-mix'), hint: 'Coba: ketuk Mixer.' },
    { t: 'Mixer', el: '#view-mix', x: 'Satu strip per track: fader volume, pan, send A/B ke track efek, mute / solo, meter, dan Master. Tahan fader atau knob untuk otomasi, MIDI CC, atau Nilai persis.', pre: view('mix') },
    { t: 'Kembali ke Arrange', el: '.seg', x: 'Ketuk Arrange untuk kembali.', want: () => vis('#view-arr'), hint: 'Coba: ketuk Arrange.' },
    { t: 'Menu proyek', el: '#btn-menu', gest: 'swipe', tips: ['Simpan cepat membuat cadangan otomatis versi sebelumnya (Browser \u2192 Proyek \u2192 Cadangan).', 'Impor FLM membaca tempo, klip, stretch, pitch, efek dan instrumen FL Studio Mobile.'], x: 'Simpan (geser baris “Simpan…” ke kanan = Simpan cepat, seperti FL Studio Mobile), buka proyek / demo, proyek baru, impor proyek FL Studio Mobile (.flm / zip), ekspor WAV / MP3 / stem / MIDI / ZIP, tempo, nama proyek, Pengaturan dan tutorial ini.' },
  ] },
  { id: 'arr', name: 'Arrangement (playlist)', ic: '\u25a4', steps: [
    { t: 'Ruler', el: arrRect('ruler'), x: 'Ketuk = pindahkan playhead. Seret = pilih rentang waktu, lalu menu rentang: Insert / Duplicate / Delete / Trim / Loop. Penanda (marker) dan perubahan birama juga di sini.', pre: () => { view('arr')(); browserOff(); } },
    { t: 'Area klip', el: arrRect('clips'), gest: 'dtap', tips: ['Snap mengikuti zoom; tahan sambil menyeret untuk geser bebas.', 'Seret tepi kanan bawah klip audio untuk stretch (mengikuti tempo).'], x: 'Gestur FL Studio Mobile: ketuk-ganda ruang kosong = klip baru 1 bar; ketuk klip = pilih dan tampilkan kontrolnya; ketuk-ganda klip = buka editornya (piano roll / step / audio); tahan = menu klip (salin, potong, duplikat, ganti nama, warna...). Seret tepi klip untuk panjangnya, jepit dua jari untuk zoom, geser untuk menggulir.' },
    { t: 'Kolom header track', el: arrRect('header'), x: 'Setiap baris adalah satu track (channel). Ketuk header = pilih track dan tampilkan device-nya; ketuk-ganda = lipat / buka; seret tepi kirinya untuk melebarkan kolom.' },
    { t: 'Tambah track', el: arrRect('addTrack'), gest: 'tap', x: 'Menambah track: synth (MIDI, untuk instrumen), drum (step sequencer), audio (sample / rekaman), atau FX track untuk routing dan grup. Track otomasi dibuat dengan menahan sebuah kontrol.', pre: () => { mark.n = P.state.tracks.length; }, want: () => P.state.tracks.length > mark.n, hint: 'Coba: ketuk “+ Track” lalu pilih jenis track.' },
    { t: 'Nama track', el: arrRect('title'), x: 'Ketuk = pilih track. Tahan = menu track: ganti nama, warna, duplikat, hapus, freeze, simpan / impor MIDI, template drum.' },
    { t: 'Mute', el: arrRect('mute'), x: 'Membisukan track (nomor track menyala = aktif).' },
    { t: 'Solo', el: arrRect('solo'), x: 'Hanya track ini (dan track solo lain) yang berbunyi.' },
    { t: 'Arm (rekam)', el: arrRect('arm'), x: 'Track yang di-arm menerima rekaman saat tombol Rekam ditekan.' },
    { t: 'Volume dan Pan', el: arrRect('vol'), gest: 'drag', tips: ['Mulai mixing dari kick dan bass, baru elemen lain relatif terhadapnya.', 'Pan elemen pendukung (hat, pluck) sedikit ke kiri / kanan untuk mix yang lebar; kick, bass dan lead tetap di tengah.'], x: 'Seret ke atas / bawah untuk volume (dB) dan pan di sebelahnya. Ketuk-ganda = kembali ke 0 dB / tengah. Tahan = otomasi, MIDI CC, atau Nilai persis.' },
    { t: 'Send A / B', el: arrRect('sa'), x: 'Jumlah sinyal yang dikirim ke track efek (return) A dan B, misalnya reverb dan delay bersama.' },
  ] },
  { id: 'panel', name: 'Panel bawah: device dan keyboard', ic: '\u2699', steps: [
    { t: 'Tab Device', el: '#dtabs [data-tab="dev"]', x: 'Rantai device track terpilih: instrumen lalu efek, kiri ke kanan seperti rack FL Studio Mobile. Tambah efek dari Browser → Efek.', pre: () => { view('arr')(); tab('dev')(); } },
    { t: 'Kartu device', el: firstCard, gest: 'hold', x: 'Setiap kartu satu modul. Tombol judul: ⇄ nyala / mati (bypass), ⧉ jendela mengambang, ⋯ menu (preset, reset, hapus, pindah), ▾ lipat. Knob: seret naik / turun (dua jari = halus), ketuk-ganda = nilai awal, tahan = Nilai persis (Fine tune), otomasi, MIDI CC.' },
    { t: 'Tab Keys', el: '#dtabs [data-tab="keys"]', x: 'Keyboard untuk memainkan instrumen track terpilih, atau pad untuk track drum. Ada pitch bend dan mod wheel, akor, strum, dan skala.', want: () => { const b = $('#dtabs [data-tab="keys"]'); return !!(b && b.classList.contains('on')); }, hint: 'Coba: ketuk Keys.' },
    { t: 'Ukuran panel', el: '#dtabs', x: 'Seret bilah tab ini ke atas / bawah untuk mengubah tinggi panel. Tombol di kanan: perbesar penuh atau tutup panel.' },
    { t: 'Kembali ke Device', el: '#dtabs [data-tab="dev"]', x: 'Ketuk Device untuk kembali ke rantai device.', want: () => { const b = $('#dtabs [data-tab="dev"]'); return !!(b && b.classList.contains('on')); }, hint: 'Coba: ketuk Device.' },
  ] },
  { id: 'beat', name: 'Latihan: beat pertamamu', ic: '\u25cf', steps: [
    { t: 'Buat beat dalam 6 langkah', x: 'Kita membuat pola drum dan memutarnya. Setiap langkah menunggu kamu melakukannya.', pre: () => { view('arr')(); browserOff(); tab('dev')(); } },
    { t: '1. Track drum', el: arrRect('addTrack'), x: 'Ketuk “+ Track” lalu pilih “Track drum (step sequencer)”.', pre: () => { mark.d = P.state.tracks.filter((t) => t.kind === 'drum').length; }, want: () => P.state.tracks.filter((t) => t.kind === 'drum').length > mark.d, hint: 'Coba: + Track → Track drum.' },
    { t: '2. Klip pola', gest: 'dtap', el: () => { const i = drumIdx(); if (i < 0) return null; if (cur && cur.shown !== cur.k) { T.revealRow(i); cur.shown = cur.k; } return T.tutorialRects(i).lane; }, x: 'Ketuk-ganda di lane track drum ini untuk membuat klip 1 bar.', want: () => { const i = drumIdx(); return i >= 0 && P.state.tracks[i].clips.length > 0; }, hint: 'Coba: ketuk-ganda di lane yang disorot.' },
    { t: '3. Buka editor step', gest: 'dtap', el: () => { const i = drumIdx(); return i >= 0 ? T.tutorialRects(i).lane : null; }, x: 'Ketuk-ganda klip yang baru dibuat untuk membuka editor step.', want: () => vis('#btn-edclose'), hint: 'Coba: ketuk-ganda klipnya.' },
    { t: '4. Isi step', el: '#dbody', gest: 'tap', tips: ['Kick di 1, 5, 9, 13 = four-on-the-floor; clap di 5 dan 13; hat di 3, 7, 11, 15 = offbeat.', 'Tahan sebuah step untuk mengatur velocity dan pitch-nya.'], x: 'Ketuk kotak-kotak step untuk menaruh pukulan: misalnya kick di 1, 5, 9, 13 dan snare di 5 dan 13. Seret untuk melukis beberapa sekaligus.', want: () => { const i = drumIdx(); return i >= 0 && P.state.tracks[i].clips.some((c) => (c.notes || []).length >= 3); }, hint: 'Coba: ketuk minimal 3 step.' },
    { t: '5. Putar', el: '#btn-play', x: 'Tekan Play dan dengarkan polanya berulang.', want: () => P.state.playing, hint: 'Coba: ketuk Play.' },
    { t: '6. Simpan', el: '#btn-menu', x: 'Hentikan pemutaran, lalu buka Menu dan geser baris “Simpan…” ke kanan untuk Simpan cepat (atau ketuk untuk memberi nama). Selamat, beat pertamamu jadi!' },
  ] },
  { id: 'flm', name: 'Alur FL Studio Mobile', ic: '\u2605', steps: [
    { t: 'Impor proyek FL Studio Mobile', el: '#btn-menu', x: 'Menu → Impor → Proyek FLM: pilih file .flm atau ZIP (proyek + folder My Samples). Klip audio, stretch, pitch, level, pan, efek FLM dan instrumen (DirectWave, GMS, MiniSynth, SuperSaw, 3x Osc, Slicer) ikut terbaca. Jendela progres menunjukkan sample yang dimuat dan dirender.' },
    { t: 'DirectWave: WAV, MP3, DWP, multi-sample', el: firstCard, x: 'Di track DirectWave, ketuk tombol Zona pada layarnya: Muat WAV / MP3 / program DWP, atau “Buat dari sample” untuk memetakan banyak file sekaligus menurut namanya (mis. Piano_C4_v64.wav = nada C4, layer velocity sampai 64).' },
    { t: 'CTRL dan Nilai persis', el: firstCard, x: 'Seperti tombol CTRL di FL Studio Mobile: tahan kontrol mana pun untuk Nilai persis (ketik angka), membuat track otomasi, menautkan ke track otomasi terpilih, atau menghubungkan ke knob controller MIDI.' },
    { t: 'Ekspor', el: '#btn-menu', x: 'Menu → Ekspor → Audio: WAV 16 / 24 / 32 bit atau MP3, seluruh lagu / loop / pilihan, stem per track, sample rate, normalisasi. Juga MIDI, proyek .mdaw, dan ZIP berisi proyek + sample.' },
    { t: 'Selesai', finale: true, x: 'Kamu sudah mengenal semua bagian utama. Tutorial ini selalu bisa dibuka lagi dari Menu → Tutorial interaktif, mulai dari bab mana pun.' },
  ] },
];

// ---------------------------------------------------------------- runner
let cur = null;
export const isRunning = () => !!cur;
const reduced = () => !!(window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches);
// chapterId: start there (and go on through the next chapters); only: that chapter alone
export function startTutorial(chapterId, only = false) {
  stopTutorial();
  const steps = [], from = Math.max(0, CHAPTERS.findIndex((c) => c.id === chapterId));
  const list = chapterId ? (only ? [CHAPTERS[from]] : CHAPTERS.slice(from)) : CHAPTERS.filter((c) => c.id !== 'show');
  list.forEach((c) => c.steps.forEach((s, k) => steps.push({ ...s, ch: c.name, ic: c.ic || '•', first: k === 0, chN: CHAPTERS.indexOf(c) + 1 })));
  const root = h('div', { class: 'tut' + (reduced() ? ' calm' : '') }), blk = [0, 1, 2, 3].map(() => h('div', { class: 'tut-blk' })), ring = h('div', { class: 'tut-ring' });
  const card = h('div', { class: 'tut-card', role: 'dialog' }), g = h('div', { class: 'tut-g' }, h('i'), h('b'));
  root.append(...blk, ring, g, card); (document.getElementById('layer') || document.body).before(root);   // in #layer's stacking context, under it: menus and dialogs opened during a step stay on top
  cur = { steps, k: -1, root, blk, ring, card, g, timer: 0, doneAt: 0, live: null };
  show(0);
  cur.timer = setInterval(tick, 200);
  return cur;
}
export function stopTutorial(finished) {
  if (!cur) return;
  clearInterval(cur.timer); const r = cur.root; cur = null;
  r.classList.add('out'); setTimeout(() => r.remove(), reduced() ? 0 : 220);
  if (finished !== undefined) { prefs.tutorialSeen = true; savePrefs(); }
}
function target(s) {
  if (!s.el) return null;
  let r = null;
  try { r = typeof s.el === 'string' ? rectOf($(s.el)) : (() => { const v = s.el(); return v && v.getBoundingClientRect ? rectOf(v) : v; })(); } catch (_) { r = null; }
  return r && r.width > 0 && r.height > 0 ? r : null;
}
function place(r) {
  const { blk, ring, card, g } = cur, W = innerWidth, H = innerHeight, pad = 4, s = cur.steps[cur.k] || {};
  if (!r) { blk[0].style.cssText = `left:0;top:0;width:${W}px;height:${H}px`; for (let k = 1; k < 4; k++) blk[k].style.cssText = 'display:none'; ring.style.display = 'none'; g.style.display = 'none'; }
  else {
    const x = Math.max(0, r.left - pad), y = Math.max(0, r.top - pad), x2 = Math.min(W, r.left + r.width + pad), y2 = Math.min(H, r.top + r.height + pad);
    blk[0].style.cssText = `left:0;top:0;width:${W}px;height:${y}px`; blk[1].style.cssText = `left:0;top:${y2}px;width:${W}px;height:${H - y2}px`;
    blk[2].style.cssText = `left:0;top:${y}px;width:${x}px;height:${y2 - y}px`; blk[3].style.cssText = `left:${x2}px;top:${y}px;width:${W - x2}px;height:${y2 - y}px`;
    ring.style.cssText = `display:block;left:${x}px;top:${y}px;width:${x2 - x}px;height:${y2 - y}px`;
    // the animated finger: the gesture this control takes, at its centre (in its left part for a wide target)
    if (s.gest && !cur.doneAt) { const gx = r.width > 220 ? r.left + Math.min(140, r.width * 0.25) : r.left + r.width / 2, gy = r.top + r.height / 2; g.className = 'tut-g ' + s.gest; g.style.cssText = `display:block;left:${gx}px;top:${gy}px`; } else g.style.display = 'none';
  }
  const cw = Math.min(s.live || s.tips ? 400 : 360, W - 16), ch = card.offsetHeight || 160;
  let left = r ? r.left + r.width / 2 - cw / 2 : (W - cw) / 2, top = r ? (r.top + r.height + 12 + ch < H ? r.top + r.height + 12 : r.top - 12 - ch) : (H - ch) / 2;
  if (r && (top < 8 || (top + ch > H - 8))) {                       // no room above or below: beside the target (the wider side), else at the far edge
    const lw = r.left - 12, rw = W - (r.left + r.width) - 12;
    if (Math.max(lw, rw) >= cw) { left = lw >= rw ? r.left - 12 - cw : r.left + r.width + 12; top = Math.max(8, Math.min(H - ch - 8, r.top + r.height / 2 - ch / 2)); }
    else top = Math.max(8, Math.min(H - ch - 8, r.top < H / 2 ? H - ch - 12 : 12));
  }
  left = Math.max(8, Math.min(W - cw - 8, left));
  card.style.left = left + 'px'; card.style.top = Math.max(8, top) + 'px'; card.style.width = cw + 'px'; card.style.maxHeight = (H - 16) + 'px';
}
// the live song map: the markers as coloured sections, a playhead that follows the song, the section under it lit
function liveMap() {
  const ms = [...(P.state.markers || [])].sort((a, b) => a.t - b.t); if (!ms.length) return null;
  const end = Math.max(ms[ms.length - 1].t + 8 * BAR, ...P.state.tracks.flatMap((t) => t.clips.map((c) => c.start + c.len)));
  const segs = ms.map((m, k) => ({ m, a: m.t, b: k + 1 < ms.length ? ms[k + 1].t : end }));
  const bar = h('div', { class: 'tut-map' }, ...segs.map((q) => { const e = h('button', { class: 'tut-seg', title: q.m.name, onclick: () => { P.seekTo(q.a); if (!P.state.playing) P.togglePlay(); } }, h('span', {}, q.m.name)); e.style.flex = String(Math.max(1, q.b - q.a)); e.style.setProperty('--c', q.m.color || '#888'); return e; }), h('i', { class: 'tut-ph' }));
  const eq = h('div', { class: 'tut-eq' }, ...[0, 1, 2, 3, 4, 5, 6].map(() => h('i')));
  const info = h('div', { class: 'tut-now' }, eq, h('b', {}, ''), h('span', {}, ''));
  const wrap = h('div', { class: 'tut-live' }, info, bar);
  const upd = () => {
    const pos = P.state.pos || 0, k = Math.max(0, segs.findIndex((q) => pos >= q.a && pos < q.b));
    bar.querySelector('.tut-ph').style.left = Math.min(100, pos / end * 100) + '%';
    [...bar.querySelectorAll('.tut-seg')].forEach((e, j) => e.classList.toggle('on', j === k));
    info.querySelector('b').textContent = P.state.playing ? '▶ ' + segs[k].m.name : '■ ' + segs[k].m.name;
    info.querySelector('span').textContent = 'bar ' + (Math.floor(pos / BAR) + 1) + ' / ' + Math.round(end / BAR);
    wrap.classList.toggle('play', !!P.state.playing);
  };
  upd(); return { el: wrap, upd };
}
function burst(host, n = 14) {                          // a small confetti burst out of `host` (success)
  if (reduced()) return;
  const cols = ['#ffad56', '#5ac878', '#4fd1ff', '#ff6fb5', '#ffe066', '#b28dff'];
  for (let k = 0; k < n; k++) { const p = h('i', { class: 'tut-cf' }); const a = (k / n) * Math.PI * 2, d = 50 + (k * 37) % 40; p.style.cssText = `--dx:${Math.cos(a) * d}px;--dy:${Math.sin(a) * d - 20}px;background:${cols[k % cols.length]};animation-delay:${(k % 4) * 15}ms`; host.append(p); setTimeout(() => p.remove(), 900); }
}
function rain() {                                       // the finale: confetti falling over the whole screen
  if (reduced() || !cur) return;
  const cols = ['#ffad56', '#5ac878', '#4fd1ff', '#ff6fb5', '#ffe066', '#b28dff'], box = h('div', { class: 'tut-rain' });
  for (let k = 0; k < 60; k++) { const p = h('i'); p.style.cssText = `left:${(k * 53) % 100}%;background:${cols[k % 6]};animation-delay:${(k % 12) * 90}ms;animation-duration:${1600 + (k * 71) % 1200}ms;--r:${(k * 47) % 360}deg`; box.append(p); }
  cur.root.append(box); setTimeout(() => box.remove(), 3200);
}
async function show(k) {
  if (!cur) return;
  const s = cur.steps[k]; if (!s) { stopTutorial(true); return; }
  const back = k < cur.k; cur.k = k; cur.doneAt = 0; cur.live = null; cur.shown = -1;
  const n = cur.steps.length;
  if (s.pre) {
    cur.card.classList.add('busy'); cur.card.replaceChildren(h('div', { class: 'tut-ch' }, s.ch), h('h4', {}, s.t), h('div', { class: 'tut-spin' }, h('i'), 'Menyiapkan…'));
    place(target(s));
    try { await s.pre(); } catch (e) { toast((e && e.message) || 'Langkah ini tidak bisa disiapkan.', 3500); }
    if (!cur || cur.k !== k) return;
    cur.card.classList.remove('busy'); cur.shown = -1;   // the target is looked up (and scrolled into view) again now that it exists
  }
  const want = !!s.want;
  const next = h('button', { class: 'b go tut-next', onclick: () => show(k + 1) }, k + 1 >= n ? 'Selesai' : 'Lanjut ›');
  if (want) next.disabled = true;
  const acts = [];
  for (const a of s.acts || []) { if (a.when && !(await a.when())) continue; acts.push(h('button', { class: 'b' + (a.go ? ' go' : ''), onclick: async () => { try { await a.fn(); } catch (_) { /* keep the tour going */ } } }, a.label)); }
  const live = s.live ? liveMap() : null; cur.live = live;
  const tips = s.tips && s.tips.length ? h('details', { class: 'tut-tips' }, h('summary', {}, 'Detail & tips'), h('ul', {}, ...s.tips.map((t) => h('li', {}, t)))) : null;
  if (tips) tips.open = /Showcase/.test(s.ch || '') && !s.live;   // the showcase explains in full; a step with the live map keeps the card short
  const prog = h('div', { class: 'tut-prog' }, h('i')); 
  cur.card.replaceChildren(...[
    s.first ? h('div', { class: 'tut-splash' }, h('div', { class: 'tut-ic' }, s.ic), h('div', {}, h('small', {}, 'Bab ' + s.chN), h('strong', {}, s.ch)), h('div', { class: 'tut-eq big' }, ...[0, 1, 2, 3, 4].map(() => h('i')))) : null,
    prog,
    h('div', { class: 'tut-ch' }, s.ch + '  •  ' + (k + 1) + ' / ' + n),
    h('h4', {}, s.t), h('p', {}, s.x),
    live ? live.el : null,
    tips,
    want ? h('div', { class: 'tut-try' }, h('i', { class: 'tut-dot' }), s.hint || 'Coba sekarang.') : null,
    acts.length ? h('div', { class: 'tut-xacts' }, ...acts) : null,
    h('div', { class: 'tut-acts' },
      h('button', { class: 'b tut-skip', onclick: () => stopTutorial(false) }, 'Tutup'),
      h('span', { style: 'flex:1' }),
      k > 0 ? h('button', { class: 'b', onclick: () => show(k - 1) }, '‹ Kembali') : null,
      want ? h('button', { class: 'b', onclick: () => show(k + 1) }, 'Lewati') : null,
      next)].filter(Boolean));
  cur.card.classList.remove('in', 'back'); void cur.card.offsetWidth; cur.card.classList.add('in'); if (back) cur.card.classList.add('back');
  requestAnimationFrame(() => { if (cur) prog.firstChild.style.width = ((k + 1) / n * 100) + '%'; });
  cur.ring.classList.remove('pop'); void cur.ring.offsetWidth; cur.ring.classList.add('pop');
  cur.next = next; cur.want = want;
  place(target(s)); requestAnimationFrame(() => cur && place(target(s)));
  if (s.finale) setTimeout(rain, 250);
}
function tick() {
  if (!cur || cur.k < 0) return;
  const s = cur.steps[cur.k];
  if (cur.card.classList.contains('busy')) return;
  place(target(s));
  if (cur.live) cur.live.upd();
  if (cur.want && !cur.doneAt && s.want && (() => { try { return s.want(); } catch (_) { return false; } })()) {
    cur.doneAt = Date.now(); cur.next.disabled = false; cur.card.classList.add('ok'); cur.g.style.display = 'none';
    const tr = cur.card.querySelector('.tut-try'); if (tr) tr.textContent = '✓ Berhasil!';
    burst(cur.card);
    const k = cur.k; setTimeout(() => { if (cur && cur.k === k) { cur.card.classList.remove('ok'); show(k + 1); } }, 900);
  }
}

// Menu -> Tutorial interaktif: the showcase, start from the beginning, or one chapter
export function tutorialMenu() {
  const sh = CHAPTERS[0];
  const list = h('div', { class: 'tut-list' },
    h('button', { class: 'b tut-hero', onclick: () => { close(); startTutorial('show', true); } }, h('span', { class: 'tut-eq big' }, ...[0, 1, 2, 3, 4].map(() => h('i'))), h('div', {}, h('strong', {}, sh.name), h('small', {}, 'Neon Horizon • 128 BPM • F minor • ' + sh.steps.length + ' langkah • putar dan bongkar lagu jadi'))),
    ...CHAPTERS.slice(1).map((c, k) => h('button', { class: 'b', onclick: () => { close(); startTutorial(c.id, true); } }, h('span', { class: 'tut-li' }, c.ic || '•'), (k + 1) + '. ' + c.name + '  (' + c.steps.length + ' langkah)')));
  const close = modal('Tutorial interaktif', h('div', {}, h('div', { class: 'hint', style: 'margin-bottom:8px' }, 'Setiap langkah menyorot tombol di layar dan menjelaskan cara memakainya. Langkah “Coba” menunggu kamu melakukannya langsung.'), list),
    [{ label: 'Tutup' }, { label: 'Semua bab dari awal', go: true, fn: () => startTutorial() }]);
}
// first start: a small offer at the bottom (never while automated tests drive the page)
export function offerTutorial() {
  if (prefs.tutorialSeen || navigator.webdriver) return;
  const chip = h('div', { class: 'tut-offer' }, h('span', { class: 'tut-eq' }, ...[0, 1, 2, 3].map(() => h('i'))), h('span', {}, 'Baru di MobileDAW?'),
    h('button', { class: 'b go', onclick: () => { chip.remove(); startTutorial('show', true); } }, 'Tonton showcase'),
    h('button', { class: 'b', onclick: () => { chip.remove(); startTutorial(); } }, 'Tutorial tombol'),
    h('button', { class: 'b', onclick: () => { chip.remove(); prefs.tutorialSeen = true; savePrefs(); } }, 'Nanti'));
  document.body.append(chip);
}
