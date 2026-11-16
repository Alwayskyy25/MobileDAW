// The loading window shown while a project opens: every sample decoded, every stretch / pitch render and every clip warped onto the tempo
// map / swing grid is a step (project.js deserialize onProgress and bakeHooks.progress). It appears only when loading takes longer than
// 200 ms (a small project opens without a flash), and always goes away, also when loading fails.
import * as P from './project.js';

let depth = 0;
export async function withLoading(title, run, { bakes = true } = {}) {
  let el = null, bar = null, lab = null, cnt = null, pct = null, last = [0, '', 0, 0];
  const paint = () => {
    if (!el) return;
    const [f, label, done, total] = last;
    bar.style.width = (Math.max(0, Math.min(1, f)) * 100).toFixed(1) + '%'; pct.textContent = Math.round(f * 100) + '%';
    lab.textContent = label || ''; cnt.textContent = total ? done + ' / ' + total : '';
  };
  const show = () => {
    if (el) return;
    el = document.createElement('div'); el.className = 'ldp'; el.setAttribute('role', 'progressbar');
    el.innerHTML = '<div class="ldp-box"><div class="ldp-t"></div><div class="ldp-row"><span class="ldp-l"></span><b class="ldp-p"></b></div><div class="ldp-bar"><i></i></div><div class="ldp-c"></div></div>';
    el.querySelector('.ldp-t').textContent = title;
    bar = el.querySelector('.ldp-bar i'); lab = el.querySelector('.ldp-l'); cnt = el.querySelector('.ldp-c'); pct = el.querySelector('.ldp-p');
    document.body.append(el); paint();
  };
  const timer = setTimeout(show, 200);
  const upd = (f, label, done, total) => { last = [f, label || last[1], done ?? last[2], total ?? last[3]]; paint(); };
  depth++; if (bakes) P.bakeHooks.hold = true;
  try {
    const r = await run(upd);
    P.bakeHooks.hold = false;
    if (bakes && P.bakesPending()) {                                // clips warped onto the tempo map / swing grid: rendered now, not while playing
      upd(0, 'Menyesuaikan klip ke tempo…', 0, P.bakesPending());
      P.bakeHooks.progress = (d, t, name) => upd(t ? d / t : 1, 'Menyesuaikan klip ke tempo: ' + name, d, t);
      await P.ensureBakes();
    }
    return r;
  } finally {
    depth--; if (!depth) { P.bakeHooks.progress = null; P.bakeHooks.hold = false; }
    clearTimeout(timer); if (el) el.remove();
  }
}
// P.deserialize with the loading window
export const openWithLoading = (text, title = 'Membuka proyek') => withLoading(title, (upd) => P.deserialize(text, upd));
