// Layout lab ("test the spacings to see how dynamic and responsive it really is"): only with a
// contract build loaded (?build=facet-wasm-contract, the core's fw_lab export). Its own stage at any W x H, the size
// tokens tuned live through the shared token styles (fx::tune: no rebuild; every stage on the page follows), the Body
// squeezed into a narrower parent, and after every change the proof: Body objects outside their parent's content box,
// overlapping touch areas, cut text, drawn as red boxes on the stage (renderer core/main.cpp fw_lab). The size tokens
// listed = what the stage's LVGL tree reads (fw_lab uses: shared styles + pen; page tokens only with a Page) and that
// visibly change it (fw_lab probe): no hard-coded list.
import type { Engine } from './engine';

const h = (tag: string, cls = '', text = '') => { const e = document.createElement(tag); if (cls) e.className = cls; if (text) e.textContent = text; return e; };
type Box = { x: number; y: number; w: number; h: number; why: string };
type Proof = { bounds: Box[]; overlap: Box[]; trunc: Box[] };
const SIZES: [number, number][] = [[480, 480], [720, 720], [800, 480], [1024, 600], [600, 1024], [320, 320], [360, 640]];

export const hasLab = (e: Engine) => typeof e.m._fw_lab === 'function';

export function layoutLab(e: Engine, comp: string, props: () => Record<string, unknown>, light: boolean, accent: number, start: number): HTMLElement {
  const lab = (cmd: string, arg = '', id = -1): string => e.withStr(cmd, (pc) => e.withStr(arg, (pa) => e.str(e.m._fw_lab(id, pc, pa))));
  const sec = h('section', 'wb-lab');
  const left = h('div', 'wb-lab-stage'), right = h('div', 'wb-lab-ctl');
  sec.append(h('h2', '', 'Layout lab'), h('p', 'wb-lead', 'Contract build only. Only the size tokens what is on screen really uses are listed (read by its LVGL tree and visibly changing it); they change live through the shared token styles (the Preview above and every stage on this page follow); the stage lays out at any size; the proof runs after every change. Red = outside its parent, orange = touch areas overlap, purple = cut text.'), h('div', 'wb-lab-grid'));
  sec.lastElementChild!.append(left, right);
  let W = start, H = start, squeeze = 0;
  const stage = e.stage(W, H);
  stage.label = 'Layout lab';
  const wrap = h('div', 'wb-lab-wrap'), over = h('div', 'wb-lab-over');
  wrap.append(stage.canvas, over);
  const sizeLine = h('div', 'wb-lab-size'), proofOut = h('div', 'wb-lab-proof');
  left.append(wrap, sizeLine, proofOut);
  const fitCss = () => { const k = Math.min(1, 560 / Math.max(W, H)); Object.assign(wrap.style, { width: `${Math.round(W * k)}px`, height: `${Math.round(H * k)}px` }); Object.assign(stage.canvas.style, { width: '100%', height: '100%' }); };

  const proof = () => {
    let p: Proof;
    try { p = JSON.parse(lab('proof', '', stage.id)); } catch { return; }
    over.replaceChildren();
    const rows: HTMLElement[] = [];
    const kinds: [keyof Proof, string, string][] = [['bounds', 'bad', 'Bounds'], ['overlap', 'warn', 'Overlapping touch areas'], ['trunc', 'cut', 'Cut text']];
    for (const [k, cls, label] of kinds) {
      for (const b of p[k] ?? []) {
        const r = h('div', `wb-lab-box ${cls}`);
        Object.assign(r.style, { left: `${(b.x / W) * 100}%`, top: `${(b.y / H) * 100}%`, width: `${(b.w / W) * 100}%`, height: `${(b.h / H) * 100}%` });
        r.title = b.why;
        over.append(r);
      }
      const n = (p[k] ?? []).length, row = h('div', `wb-lab-row ${n ? cls : 'ok'}`);
      row.append(h('b', '', `${label}: ${n ? n : 'pass'}`));
      for (const b of (p[k] ?? []).slice(0, 6)) row.append(h('div', 'wb-lab-why', b.why));
      if (n > 6) row.append(h('div', 'wb-lab-why', `+ ${n - 6} more`));
      rows.push(row);
    }
    proofOut.replaceChildren(...rows);
  };
  const reproof = () => { proof(); window.setTimeout(proof, 400); window.setTimeout(proof, 1600); };  // after the opening motion too
  // what the stage really uses (no hard-coded list): tokens its LVGL tree reads (fw_lab uses) that visibly change it (probe)
  let used: string[] = [], quiet: string[] = [];
  const scan = () => {
    try {
      const u = JSON.parse(lab('uses', '', stage.id)) as string[], p = JSON.parse(lab('probe', String(squeeze || ''), stage.id)) as Record<string, number>;
      used = u.filter((n) => p[n] === 1); quiet = u.filter((n) => p[n] !== 1);
    } catch { used = []; quiet = []; }
    drawToks();
  };
  const render = () => {
    if (stage.w !== W || stage.h !== H) stage.resize(W, H);
    const err = stage.render(comp, { ...props(), opening: false }, light, accent);
    scan();
    sizeLine.textContent = `${W} x ${H} px${squeeze ? `, Body max ${squeeze} px` : ''}${err ? ` · error: ${err}` : ''}`;
    fitCss(); reproof();
  };

  // canvas size: presets + free W x H
  const sizes = h('div', 'wb-lab-sizes');
  const wIn = h('input') as HTMLInputElement, hIn = h('input') as HTMLInputElement;
  for (const i of [wIn, hIn]) { i.type = 'number'; i.min = '120'; i.max = '2048'; i.step = '1'; i.className = 'wb-lab-num'; }
  const setSize = (w: number, hh: number) => { W = Math.max(120, Math.min(2048, w | 0)); H = Math.max(120, Math.min(2048, hh | 0)); wIn.value = String(W); hIn.value = String(H); sq.max = String(W); if (squeeze > W) setSqueeze(0); render(); };
  for (const [w, hh] of SIZES) { const b = h('button', 'wb-btn', `${w}x${hh}`) as HTMLButtonElement; b.type = 'button'; b.onclick = () => setSize(w, hh); sizes.append(b); }
  const go = h('button', 'wb-btn', 'Set') as HTMLButtonElement;
  go.type = 'button'; go.onclick = () => setSize(Number(wIn.value), Number(hIn.value));
  for (const i of [wIn, hIn]) i.onkeydown = (ev) => { if (ev.key === 'Enter') go.click(); };
  const free = h('div', 'wb-lab-free');
  free.append(wIn, h('span', '', 'x'), hIn, go);

  // squeeze: the Body inside a narrower parent (its max width), header untouched
  const sq = h('input') as HTMLInputElement, sqOut = h('span', 'wb-lab-val');
  sq.type = 'range'; sq.min = '120'; sq.step = '4';
  const setSqueeze = (v: number) => { squeeze = v >= W ? 0 : v; sq.value = String(squeeze || W); sqOut.textContent = squeeze ? `${squeeze} px` : 'full'; lab('tune', `squeeze=${squeeze || -1}`); render(); };
  sq.oninput = () => setSqueeze(Number(sq.value));

  // the token sliders
  const toks = h('div', 'wb-lab-toks');
  // [name, token px, now px, kind, lo, hi]: kind "fixed" = a LAYOUT size (Facet's, not themeable: tuned here only to
  // prove a layout), "look" = themeable within lo..hi (renderer/CONTRACT.md "Size tokens: layout vs look")
  const all = () => JSON.parse(lab('tokens')) as [string, number, number, string?, number?, number?][];
  const drawToks = () => {
    const list = all(), byName = new Map(list.map((t) => [t[0], t]));
    toks.replaceChildren();
    sqRow.style.display = sqHead.style.display = used.includes('squeeze') ? '' : 'none';  // page only
    if (!used.some((n) => n !== 'squeeze')) toks.append(h('p', 'wb-lab-why', 'No size token changes what is on this stage.'));
    for (const name of used) {
      const t = byName.get(name);
      if (!t) continue;
      const [, base, now, kind = 'fixed', lo = 0, hi = 0] = t, look = kind === 'look';
      const row = h('label', `wb-lab-tok${now !== base ? ' changed' : ''}`), r = h('input') as HTMLInputElement, val = h('span', 'wb-lab-val', `${now} px`);
      r.type = 'range'; r.min = String(look ? lo : 0);
      r.max = String(look ? hi : name === 'layout:inset' ? 64 : Math.max(16, base * 2, now));
      r.step = name.startsWith('stroke:') || base % 4 ? '1' : '4';
      r.value = String(now);
      r.oninput = () => { val.textContent = `${r.value} px`; row.classList.toggle('changed', Number(r.value) !== base); lab('tune', `${name}=${r.value}`); reproof(); };
      row.title = look ? `look size: themeable ${lo}-${hi} px (token ${base} px)` : `layout size: fixed by Facet at ${base} px (moved here only to prove a layout)`;
      const tag = h('span', `wb-lab-kind${look ? '' : ' fixed'}`, look ? `look ${lo}-${hi}` : 'fixed');
      row.append(h('code', '', name), tag, r, val);
      toks.append(row);
    }
    if (quiet.length) toks.append(h('p', 'wb-lab-why', `Read but no visible change here (overridden or not in this state): ${quiet.join(', ')}`));
  };
  const reset = h('button', 'wb-btn', 'Reset tokens + squeeze') as HTMLButtonElement;
  reset.type = 'button';
  reset.onclick = () => { lab('tune', '='); squeeze = 0; sq.value = String(W); sqOut.textContent = 'full'; render(); };

  const sqRow = h('label', 'wb-lab-tok'), sqHead = h('h3', '', 'Squeeze');
  sqRow.append(h('code', '', 'Body max width'), sq, sqOut);
  right.append(h('h3', '', 'Canvas'), sizes, free, sqHead, sqRow, h('h3', '', 'Size tokens'), toks, reset);
  lab('tune', 'squeeze=-1');  // the squeeze is this lab's own (the token sliders keep what the core has)
  wIn.value = String(W); hIn.value = String(H); sq.max = String(W); sq.value = String(squeeze || W); sqOut.textContent = squeeze ? `${squeeze} px` : 'full';
  render();
  return sec;
}
