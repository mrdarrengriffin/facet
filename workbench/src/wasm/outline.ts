// Components view (sidebar Settings "Pixel grid" > Components, localStorage wb.pixgrid = comps): outlines every
// component instance on a stage (core fw_comps, a WASM-only introspection hook: every build is tagged with its id) with
// a thin box + its id, to see how a page is built and check that it uses components. Kinds: layout (split / group /
// column, dashed), component (solid, deeper = lighter + inset), hand-built view (legacy *_view page bodies: hatched
// warning fill, labelled), screen (the page frame, label only). Overlay canvas over the preview canvas (any zoom),
// polled while the page lives so it follows updates; plus a legend and a one-line summary (app.ts).
import type { CompBox, SlotBox, Stage } from './engine';

const h = (tag: string, cls = '', text = '') => { const e = document.createElement(tag); if (cls) e.className = cls; if (text) e.textContent = text; return e; };
export type Kind = 'layout' | 'comp' | 'hand' | 'screen';
const LAYOUTS = new Set(['split', 'group', 'column']);
export const handBuilt = (id: string) => id.endsWith('_view');
/** sections by component id (engine.comps) -> the kind of an instance (outermost id wins) */
export function kindOf(id: string, section: (id: string) => string | undefined): Kind {
  if (handBuilt(id)) return 'hand';
  if (LAYOUTS.has(id)) return 'layout';
  const s = section(id);
  return s === 'screens' || s === 'foundations' ? 'screen' : 'comp';
}
// debug overlay colours (like the Pixel grid's): component = cyan, layout = violet dashed, hand-built = orange
const COL: Record<Kind, string> = { comp: '#00c8ff', layout: '#b07cff', hand: '#ff8a00', screen: '#9aa0a6' };
const SLOT = '#ff3fa4';

/** one-line summary of a stage's instances */
export function summary(list: CompBox[], kind: (b: CompBox) => Kind) {
  const n = { comp: 0, layout: 0, hand: 0, screen: 0 } as Record<Kind, number>;
  const hands: string[] = [];
  let inside = 0, handDepth = 0;
  for (const b of list) {
    const k = kind(b);
    if (handDepth && b.d <= handDepth) handDepth = 0;
    if (handDepth && k !== 'hand') inside++;
    if (k === 'hand') { hands.push(b.ids[0]); if (!handDepth) handDepth = b.d; }
    n[k]++;
  }
  if (hands.length) return `${hands.length} hand-built view${hands.length > 1 ? 's' : ''} (${[...new Set(hands)].join(', ')}) · ${inside} component${inside === 1 ? '' : 's'} inside`;
  return `${n.comp} component${n.comp === 1 ? '' : 's'} · ${n.layout} layout${n.layout === 1 ? '' : 's'} · 0 hand-built`;
}

export function legend() {
  const el = h('div', 'wb-ol-legend');
  for (const [k, t] of [['comp', 'component'], ['layout', 'layout'], ['hand', 'hand-built view'], ['screen', 'screen']] as [Kind, string][]) {
    const i = h('span', `wb-ol-key wb-ol-${k}`);
    i.append(h('i'), t);
    el.append(i);
  }
  const sl = h('span', 'wb-ol-key wb-ol-slot');
  sl.append(h('i'), 'slot (components can go here)');
  el.append(sl);
  return el;
}

/** wrap the stage canvas in a box with the overlay; returns the box (put it where the canvas was) */
export function outlineView(s: Stage, section: (id: string) => string | undefined, onSummary: (t: string) => void) {
  const box = h('div', 'wb-ol'), cv = document.createElement('canvas');
  s.canvas.replaceWith(box);
  box.append(s.canvas, cv);
  const g = cv.getContext('2d')!;
  const kind = (b: CompBox) => kindOf(b.ids[0], section);
  let list: CompBox[] = [], slots: SlotBox[] = [], key = '', last = 0, size = '';
  const draw = () => {
    const r = s.canvas.getBoundingClientRect(), dpr = window.devicePixelRatio || 1;
    const W = Math.max(1, Math.round(r.width * dpr)), H = Math.max(1, Math.round(r.height * dpr)), sz = `${W}x${H}`;
    if (sz !== size) { size = sz; cv.width = W; cv.height = H; }
    const k = W / s.w, kh = H / s.h;
    g.clearRect(0, 0, W, H);
    const fs = Math.round(11 * dpr);
    g.font = `600 ${fs}px ui-monospace, monospace`;
    g.textBaseline = 'top';
    const lw = Math.max(1, Math.round(dpr));
    const tags: [number, number, string, string][] = [];
    for (const b of list) {
      const t = kind(b), col = COL[t], inset = (b.d - 1) * lw;
      const x = Math.round(b.x * k) + inset + lw / 2, y = Math.round(b.y * kh) + inset + lw / 2;
      const w = Math.round(b.w * k) - 2 * inset - lw, hh = Math.round(b.h * kh) - 2 * inset - lw;
      if (w < 2 || hh < 2) continue;
      g.globalAlpha = t === 'hand' ? 1 : Math.max(0.45, 1 - (b.d - 1) * 0.18);
      if (t === 'hand') {
        g.save(); g.beginPath(); g.rect(x, y, w, hh); g.clip();
        g.strokeStyle = 'rgba(255,138,0,0.28)'; g.lineWidth = 2 * lw;
        for (let i = -hh; i < w; i += 10 * dpr) { g.beginPath(); g.moveTo(x + i, y + hh); g.lineTo(x + i + hh, y); g.stroke(); }
        g.restore();
      }
      g.strokeStyle = col; g.lineWidth = t === 'hand' ? 2 * lw : lw;
      g.setLineDash(t === 'layout' ? [4 * dpr, 3 * dpr] : t === 'screen' ? [2 * dpr, 4 * dpr] : []);
      g.strokeRect(x, y, w, hh);
      g.setLineDash([]);
      const label = (t === 'hand' ? 'hand-built view · ' : '') + b.ids.join(' › ');
      tags.push([x, y, label, col]);
    }
    // slots (meta "slots", C8a): a pink dashed area with a faint fill, labelled by name, where other components may go
    for (const b of slots) {
      const x = Math.round(b.x * k) + lw / 2, y = Math.round(b.y * kh) + lw / 2, w = Math.round(b.w * k) - lw, hh = Math.round(b.h * kh) - lw;
      if (hh < 2) continue;
      g.globalAlpha = 1;
      if (b.empty || w < 2) {  // an empty slot: an insertion bar where its first item would go
        g.fillStyle = SLOT; g.fillRect(x - 2 * lw, y, 3 * lw, hh);
        tags.push([x + 3 * lw, y + hh - fs - 4 * dpr, `slot: ${b.n} (empty)`, SLOT]);
        continue;
      }
      g.fillStyle = 'rgba(255,63,164,0.12)'; g.fillRect(x, y, w, hh);
      g.strokeStyle = SLOT; g.lineWidth = 2 * lw; g.setLineDash([5 * dpr, 3 * dpr]);
      g.strokeRect(x, y, w, hh);
      g.setLineDash([]);
      tags.push([x, y + hh - fs - 4 * dpr, `slot: ${b.n}`, SLOT]);
    }
    // labels last (on top of every box), skipped where one already sits
    const taken: [number, number, number][] = [];
    for (const [x, y, t, col] of tags) {
      const tw = g.measureText(t).width + 6 * dpr, th = fs + 4 * dpr;
      let ly = y;
      while (taken.some(([ax, ay, aw]) => Math.abs(ay - ly) < th && x < ax + aw && ax < x + tw)) ly += th;
      if (ly + th > H) continue;
      taken.push([x, ly, tw]);
      g.globalAlpha = 1;
      g.fillStyle = 'rgba(0,0,0,0.72)'; g.fillRect(x, ly, tw, th);
      g.fillStyle = col; g.fillText(t, x + 3 * dpr, ly + 2 * dpr);
    }
    g.globalAlpha = 1;
  };
  const tick = (t: number) => {
    if (!box.isConnected && last) return;  // page gone: stop
    requestAnimationFrame(tick);
    if (!last || t - last > 150) {
      last = t || 1;
      list = s.comps();
      slots = s.slots();
      const nk = JSON.stringify(list);
      if (nk !== key) { key = nk; onSummary(summary(list, kind)); }
    }
    draw();
  };
  requestAnimationFrame(tick);
  return box;
}
