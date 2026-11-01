// Pixel grid debug view (sidebar Settings "Pixel grid", localStorage wb.pixgrid): a stage's pixels magnified
// nearest-neighbour with a 1 px grid, every 4th line stronger (the Facet 4 px grid, at absolute stage coordinates), a
// hover readout (x / y in device px + the pixel colour + the objects under it) and optional outlines of the build's
// LVGL objects (core fw_rects) with their x / y / w / h. Live: follows the stage while it animates. Used under the
// component preview and by meta "compare" size studies (app.ts).
import type { Rect, Stage } from './engine';

const h = (tag: string, cls = '', text = '') => { const e = document.createElement(tag); if (cls) e.className = cls; if (text) e.textContent = text; return e; };
const hex = (r: number, g: number, b: number) => `#${[r, g, b].map((v) => v.toString(16).padStart(2, '0')).join('').toUpperCase()}`;
// outline colour per depth (1 = the component's root object)
const BOX = ['#ff3bd4', '#00d0ff', '#ffd400', '#7cff4a', '#ff8a00'];

export type PixOpts = { scale?: number; maxW?: number; boxes?: boolean; margin?: number };

/** crop = union of the top-level objects + margin, origin snapped to the 4 px grid, clamped to the stage */
function crop(s: Stage, rects: Rect[], margin: number) {
  const top = rects.filter((r) => r.d === 1 && r.w > 0 && r.h > 0);
  if (!top.length) return { x: 0, y: 0, w: s.w, h: s.h };
  let x1 = Math.min(...top.map((r) => r.x)) - margin, y1 = Math.min(...top.map((r) => r.y)) - margin;
  let x2 = Math.max(...top.map((r) => r.x + r.w)) + margin, y2 = Math.max(...top.map((r) => r.y + r.h)) + margin;
  x1 = Math.max(0, Math.floor(x1 / 4) * 4); y1 = Math.max(0, Math.floor(y1 / 4) * 4);
  x2 = Math.min(s.w, Math.ceil(x2 / 4) * 4); y2 = Math.min(s.h, Math.ceil(y2 / 4) * 4);
  return { x: x1, y: y1, w: Math.max(4, x2 - x1), h: Math.max(4, y2 - y1) };
}

export function pixelView(s: Stage, o: PixOpts = {}) {
  const margin = o.margin ?? 8, boxes = o.boxes ?? true;
  const box = h('div', 'wb-pix'), cv = document.createElement('canvas'), read = h('div', 'wb-pix-read', 'hover a pixel');
  const list = h('ol', 'wb-pix-rects');
  box.append(cv, read, list);
  const g = cv.getContext('2d')!;
  const src = s.canvas.getContext('2d', { willReadFrequently: true })!;
  let rects: Rect[] = [], c = { x: 0, y: 0, w: 1, h: 1 }, S = 8, hover: [number, number] | null = null, lastRects = 0, rectKey = '';

  const refresh = () => {
    rects = s.rects();
    const k = JSON.stringify(rects);
    if (k === rectKey) return;
    rectKey = k;
    const nc = crop(s, rects, margin);
    if (nc.x !== c.x || nc.y !== c.y || nc.w !== c.w || nc.h !== c.h) {
      c = nc;
      S = o.scale ?? Math.max(2, Math.min(12, Math.floor((o.maxW ?? 720) / c.w)));
      cv.width = c.w * S; cv.height = c.h * S;
      Object.assign(cv.style, { width: `${(c.w * S) / (window.devicePixelRatio || 1)}px`, height: `${(c.h * S) / (window.devicePixelRatio || 1)}px` });
    }
    // each object: size + position inside its parent (DFS order: the parent = the last object one level up); the 4 px
    // check covers size and in-parent offsets (the root's place on the stage is the workbench's centring, not the component's)
    const up: Rect[] = [];
    list.replaceChildren(...rects.map((r) => {
      up[r.d] = r;
      const p = r.d > 1 ? up[r.d - 1] : null, lx = p ? r.x - p.x : 0, ly = p ? r.y - p.y : 0;
      const off = r.w % 4 || r.h % 4 || lx % 4 || ly % 4;
      const li = h('li'), sw = h('span', 'wb-pix-sw', '■ ');
      sw.style.color = BOX[(r.d - 1) % BOX.length];
      li.append('  '.repeat(r.d - 1), sw, `${r.w} x ${r.h}${p ? `  at ${lx}, ${ly} in parent (gap right ${p.w - lx - r.w}, bottom ${p.h - ly - r.h})` : `  at ${r.x}, ${r.y} on the stage`}${off ? '  · off the 4 px grid' : ''}`);
      return li;
    }));
  };

  const draw = () => {
    g.imageSmoothingEnabled = false;
    g.drawImage(s.canvas, c.x, c.y, c.w, c.h, 0, 0, c.w * S, c.h * S);
    // 1 px grid (faint) + the 4 px grid (stronger), on absolute stage coordinates
    for (let i = 0; i <= c.w; i++) {
      const ax = c.x + i;
      g.fillStyle = ax % 4 === 0 ? 'rgba(128,128,128,0.75)' : 'rgba(128,128,128,0.22)';
      g.fillRect(i * S - (ax % 4 === 0 ? 1 : 0), 0, ax % 4 === 0 ? 2 : 1, c.h * S);
    }
    for (let j = 0; j <= c.h; j++) {
      const ay = c.y + j;
      g.fillStyle = ay % 4 === 0 ? 'rgba(128,128,128,0.75)' : 'rgba(128,128,128,0.22)';
      g.fillRect(0, j * S - (ay % 4 === 0 ? 1 : 0), c.w * S, ay % 4 === 0 ? 2 : 1);
    }
    if (boxes) {
      g.font = `${Math.max(10, Math.min(13, S + 3))}px ui-monospace, monospace`;
      g.textBaseline = 'bottom';
      for (const r of rects) {
        const col = BOX[(r.d - 1) % BOX.length];
        g.strokeStyle = col; g.lineWidth = 2;
        g.strokeRect((r.x - c.x) * S + 1, (r.y - c.y) * S + 1, r.w * S - 2, r.h * S - 2);
      }
      for (const r of rects) {
        const t = `${r.x},${r.y} ${r.w}x${r.h}`, tx = (r.x - c.x) * S + 2, ty = (r.y - c.y) * S + (r.d === 1 ? -2 : r.h * S - 2);
        const tw = g.measureText(t).width;
        g.fillStyle = 'rgba(0,0,0,0.7)'; g.fillRect(tx - 1, ty - 13, tw + 2, 13);
        g.fillStyle = BOX[(r.d - 1) % BOX.length]; g.fillText(t, tx, ty);
      }
    }
    if (hover) { g.strokeStyle = '#fff'; g.lineWidth = 2; g.strokeRect((hover[0] - c.x) * S, (hover[1] - c.y) * S, S, S); }
  };

  const tick = (t: number) => {
    if (!box.isConnected && lastRects) return; // removed from the page: stop
    requestAnimationFrame(tick);
    if (t - lastRects > 100 || !lastRects) { lastRects = t || 1; refresh(); }
    draw();
  };
  requestAnimationFrame(tick);

  cv.addEventListener('mousemove', (ev) => {
    const b = cv.getBoundingClientRect();
    const x = c.x + Math.floor(((ev.clientX - b.left) / b.width) * c.w), y = c.y + Math.floor(((ev.clientY - b.top) / b.height) * c.h);
    hover = [x, y];
    const [r, gg, bb] = src.getImageData(x, y, 1, 1).data;
    const under = rects.filter((q) => x >= q.x && x < q.x + q.w && y >= q.y && y < q.y + q.h).map((q) => `d${q.d} ${q.w}x${q.h} @${q.x},${q.y} (local ${x - q.x},${y - q.y})`);
    read.textContent = `x ${x}  y ${y}  ${hex(r, gg, bb)}${under.length ? `  ·  ${under.join('  ›  ')}` : ''}`;
  });
  cv.addEventListener('mouseleave', () => { hover = null; read.textContent = 'hover a pixel'; });
  return box;
}
