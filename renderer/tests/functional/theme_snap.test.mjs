// Snapped drawing (renderer/CONTRACT.md C29, shape.snap; theme engine T3; the snapped-drawing faults of board
// facet-201 as tests). A probe (core/pen_lab.cpp) draws the Pen's primitives at an odd screen offset in the live theme:
// an arc band, minor + major ticks, a knob on the arc, a dot at the centre, a thick line, a pixel icon. Each test fails
// without the behaviour it names:
//   grid     every drawn pixel is part of a whole cs x cs cell of one colour on the SCREEN grid (cs = stroke:pixel)
//   centre   the Pen centre is a cell centre: a dot there is symmetric round it (fault: pen centre off cell centre)
//   knob     the knob sits on the arc's cells: same grid, centred on the arc's radius (fault: knob/arc not on the same cells)
//   thick    a thick line is k whole cells across at every step (fault: thick-segment ticks drawn as blobs)
//   icon     the pixel icon is drawn in whole cells of the theme cell (fault: pixel icons not sized to the cell)
//   palette  no rendered pixel of the probe outside the theme's palette and inks (no anti-aliasing, C29 palette proof)
//   break    Facet (no snap) fails the grid proof: the proof can fail
import { readFileSync, writeFileSync } from 'node:fs';
let dumps = 0;
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { frame565 } from './_release.mjs';

const THEMES = join(dirname(fileURLToPath(import.meta.url)), '..', '..', 'themes');

const lab = (t, c, a = '') => JSON.parse(t.b.str(c, (pc) => t.b.str(typeof a === 'string' ? a : JSON.stringify(a), (pa) => t.b.m.UTF8ToString(t.b.m._fw_lab(c.startsWith('pen_') ? t.b.stage : -1, pc, pa)))) || '{}');
const M = ['placeholder', { name: 'Snap' }];
const use = (t, id) => { lab(t, 'theme_use', id); t.b.tick(2); t.b.settle(300); };
const to565 = (hex) => { const v = parseInt(hex.slice(1), 16); return (((v >> 16) & 255) >> 3) << 11 | (((v >> 8) & 255) >> 2) << 5 | ((v & 255) >> 3); };
const SIZE = (t) => (t.b.S > 600 ? 300 : 200);
function probe(t, o = {}) {
  const p = lab(t, 'pen_probe', { x: 7, y: 5, size: SIZE(t), knob_deg: 0, line_deg: 30, ...o });
  t.b.tick(3); t.b.settle(200);
  const f = frame565(t);
  if (process.env.SNAP_DUMP) writeFileSync(`${process.env.SNAP_DUMP}/probe-${dumps++}.raw`, Buffer.from(f.buffer));  // debugging only
  return { ...p, f, W: t.b.W || t.b.S, x0: p.box[0], y0: p.box[1] };
}
// cells inside the probe that are not one colour (a drawn pixel off the grid or anti-aliased)
function offGrid(P, cs) {
  const bad = [], bg = P.f[0];
  const X1 = Math.floor(P.x0 / cs) * cs, Y1 = Math.floor(P.y0 / cs) * cs, X2 = P.box[2] + 1, Y2 = P.box[3] + 1;  // the Pen's box (the icon: its own test)
  for (let y = Y1; y + cs <= Y2; y += cs) for (let x = X1; x + cs <= X2; x += cs) {
    const c = P.f[y * P.W + x];
    let ok = true;
    for (let dy = 0; dy < cs && ok; dy++) for (let dx = 0; dx < cs && ok; dx++) if (P.f[(y + dy) * P.W + x + dx] !== c) ok = false;
    if (!ok) bad.push(`${x},${y}`);
  }
  void bg;
  return bad;
}
const SIZEW = (P) => P.size;
function bbox(P, col, x1, y1, x2, y2) {
  [x1, y1, x2, y2] = [Math.floor(x1), Math.floor(y1), Math.floor(x2), Math.floor(y2)];
  let a = [1e9, 1e9, -1, -1], n = 0;
  for (let y = y1; y <= y2; y++) for (let x = x1; x <= x2; x++) if (P.f[y * P.W + x] === col) { n++; a = [Math.min(a[0], x), Math.min(a[1], y), Math.max(a[2], x), Math.max(a[3], y)]; }
  return n ? { x1: a[0], y1: a[1], x2: a[2], y2: a[3], n } : null;
}

export default {
  id: 'theme_snap', family: 'foundations', about: 'Snapped themes: every Pen primitive in whole cells on the screen grid, knob on the arc cells, thick lines k cells, pixel icons, palette proof.',
  tests: [
    { name: 'grid: every drawn pixel is a whole cell of stroke:pixel on the screen grid (Blockcraft, Win98)', mount: M, run(t) {
      const out = [];
      for (const th of ['blockcraft', 'win98']) {
        use(t, th);
        for (const [x, y] of [[7, 5], [10, 11], [0, 0]]) {
          const P = probe(t, { x, y, size: SIZE(t) });
          if (!P.cell) { out.push(`${th}: not snapped`); continue; }
          const b = offGrid({ ...P, size: SIZE(t) }, P.cell);
          if (b.length) out.push(`${th} at ${x},${y}: ${b.length} cells not whole (${b.slice(0, 3).join(' ')})`);
        }
      }
      lab(t, 'pen_clear'); use(t, 'facet');
      t.eq(out, [], 'whole cells only, whatever the box offset');
    } },
    { name: 'break: Facet (no snap) fails the grid proof (anti-aliased arcs): the proof can fail', mount: M, run(t) {
      use(t, 'facet');
      const P = probe(t);
      t.ok(offGrid({ ...P, size: SIZE(t) }, 3).length > 0, 'Facet draws off a 3 px grid');
      lab(t, 'pen_clear');
    } },
    { name: 'centre: the Pen centre is a cell centre: the centre dot is symmetric round it', mount: M, run(t) {
      use(t, 'blockcraft');
      const out = [];
      for (const [x, y] of [[7, 5], [8, 6], [9, 4]]) {
        const P = probe(t, { x, y, line_deg: 200 });
        const cs = P.cell, ox = Math.floor(P.cx / cs) * cs + cs / 2, oy = Math.floor(P.cy / cs) * cs + cs / 2;
        const text = P.f[(P.cy) * P.W + P.cx];
        const b = bbox(P, text, P.cx - 40, P.cy - 40, P.cx + 40, P.cy + 40);
        if (!b) { out.push(`${x},${y}: no dot`); continue; }
        const mx = (b.x1 + b.x2 + 1) / 2, my = (b.y1 + b.y2 + 1) / 2;
        if (mx !== ox || my !== oy) out.push(`${x},${y}: dot centred at ${mx},${my}, the centre cell's middle is ${ox},${oy}`);
        if ((b.x2 - b.x1 + 1) !== (b.y2 - b.y1 + 1)) out.push(`${x},${y}: dot ${b.x2 - b.x1 + 1} x ${b.y2 - b.y1 + 1}`);
      }
      lab(t, 'pen_clear'); use(t, 'facet');
      t.eq(out, [], 'dot centred on the centre cell, square');
    } },
    { name: 'knob: the knob sits on the arc\'s cells, centred on its radius, an odd number of cells across', mount: M, run(t) {
      use(t, 'blockcraft');
      const out = [];
      for (const deg of [0, 90, 180, 270]) {
        const P = probe(t, { knob_deg: deg, line_deg: deg + 45 });
        const cs = P.cell, ox = Math.floor(P.cx / cs) * cs + cs / 2, oy = Math.floor(P.cy / cs) * cs + cs / 2;
        const rc = (v) => Math.sign(v) * Math.round(Math.abs(v));  // C roundf: halves away from zero
        const kx = ox + rc(P.knob[0] / cs) * cs, ky = oy + rc(P.knob[1] / cs) * cs;
        const acc = P.f[Math.floor(ky) * P.W + Math.floor(kx)];
        const b = bbox(P, acc, kx - 60, ky - 60, kx + 60, ky + 60);
        if (!b) { out.push(`${deg}: no knob`); continue; }
        const w = (b.x2 - b.x1 + 1) / cs, h = (b.y2 - b.y1 + 1) / cs, mx = (b.x1 + b.x2 + 1) / 2, my = (b.y1 + b.y2 + 1) / 2;
        if (mx !== kx || my !== ky) out.push(`${deg}: knob centred at ${mx},${my}, its cell middle is ${kx},${ky}`);
        if (w !== h || w % 2 !== 1) out.push(`${deg}: knob face ${w} x ${h} cells (want odd, square)`);
        const r = Math.hypot(kx - ox, ky - oy);
        if (Math.abs(r - P.r) > cs) out.push(`${deg}: knob at radius ${r.toFixed(1)}, the arc at ${P.r}`);
      }
      lab(t, 'pen_clear'); use(t, 'facet');
      t.eq(out, [], 'knob on the arc cells');
    } },
    { name: 'thick: a thick line is the same whole number of cells across at every step', mount: M, run(t) {
      use(t, 'blockcraft');
      const out = [];
      for (const deg of [20, 30, 160, 250]) {
        const P = probe(t, { line_deg: deg, knob_deg: deg + 180 });
        const cs = P.cell, a = (deg * Math.PI) / 180, hor = Math.abs(Math.cos(a)) >= Math.abs(Math.sin(a));
        const ex = P.cx + Math.round((Math.cos(a) * P.r) / 4), ey = P.cy + Math.round((Math.sin(a) * P.r) / 4);  // a quarter out: on the line
        const col = P.f[ey * P.W + ex];
        const counts = new Map();
        const R = Math.ceil(P.r / 2) + 4 * cs;
        for (let y = P.cy - R; y <= P.cy + R; y += cs) for (let x = P.cx - R; x <= P.cx + R; x += cs) {
          const gx = Math.floor(x / cs) * cs, gy = Math.floor(y / cs) * cs;
          if (P.f[gy * P.W + gx] !== col) continue;
          const k = hor ? gx : gy;
          counts.set(k, (counts.get(k) || 0) + 1);
        }
        const c0 = hor ? Math.floor(P.cx / cs) * cs : Math.floor(P.cy / cs) * cs, skip = Math.ceil(P.r / 10 / cs + 2) * cs;  // the dot at the centre
        const v = [...counts.entries()].sort((a, b) => a[0] - b[0]).filter(([k]) => Math.abs(k - c0) > skip).map(([, n]) => n).slice(0, -1);  // the far end cell
        if (!v.length || v.some((n) => n !== v[0])) out.push(`${deg} deg: cells across per step ${JSON.stringify(v.slice(0, 12))}`);
      }
      lab(t, 'pen_clear'); use(t, 'facet');
      t.eq(out, [], 'constant k cells across');
    } },
    { name: 'icon: the pixel icon is 8 x 8 whole theme cells (on the screen grid when it fits its box there)', mount: M, run(t) {
      const out = [];
      for (const th of ['blockcraft', 'win98']) {
        use(t, th);
        const P = probe(t);
        const cs = P.cell, [ix1, iy1, ix2, iy2] = P.icon, bg = P.f[(iy1 - 1) * P.W + ix1];
        let x1 = 1e9, y1 = 1e9, x2 = -1, y2 = -1;
        for (let y = iy1; y <= iy2; y++) for (let x = ix1; x <= ix2; x++) if (P.f[y * P.W + x] !== bg) { x1 = Math.min(x1, x); y1 = Math.min(y1, y); x2 = Math.max(x2, x); y2 = Math.max(y2, y); }
        if (x2 < 0) { out.push(`${th}: no ink`); continue; }
        const w = x2 - x1 + 1, h = y2 - y1 + 1;
        let broken = 0, ink = 0;
        for (let y = y1; y + cs <= y2 + 1; y += cs) for (let x = x1; x + cs <= x2 + 1; x += cs) {
          const c = P.f[y * P.W + x];
          let same = true;
          for (let dy = 0; dy < cs; dy++) for (let dx = 0; dx < cs; dx++) if (P.f[(y + dy) * P.W + x + dx] !== c) same = false;
          if (!same) broken++; else if (c !== bg) ink++;
        }
        if (w % cs || h % cs || broken || ink !== 22) out.push(`${th}: art ${w} x ${h} px (cell ${cs}), ${ink} ink cells (the bulb has 22), ${broken} cells not whole`);
      }
      lab(t, 'pen_clear'); use(t, 'facet');
      t.eq(out, [], 'whole cells, the whole art');
    } },
    { name: 'palette: every pixel of the probe is a colour of the theme\'s palette or inks (no anti-aliasing, C29)', mount: M, run(t) {
      const out = [];
      for (const th of ['blockcraft', 'win98']) {
        use(t, th);
        const dump = lab(t, 'theme_dump', readFileSync(join(THEMES, `${th}.json`), 'utf8'));
        const pal = new Set((JSON.stringify(dump).match(/#[0-9A-Fa-f]{6}/g) || []).map(to565));
        const P = probe(t);
        const off = new Map();
        for (let y = P.y0; y < P.y0 + SIZE(t) + 50; y++) for (let x = P.x0; x < P.x0 + SIZE(t); x++) { const c = P.f[y * P.W + x]; if (!pal.has(c)) off.set(c, (off.get(c) || 0) + 1); }
        if (off.size) out.push(`${th}: ${off.size} colours outside the palette (${[...off.values()].reduce((a, b) => a + b, 0)} px)`);
      }
      lab(t, 'pen_clear'); use(t, 'facet');
      const dump = lab(t, 'theme_dump', readFileSync(join(THEMES, 'facet.json'), 'utf8'));
      const pal = new Set((JSON.stringify(dump).match(/#[0-9A-Fa-f]{6}/g) || []).map(to565));
      const P = probe(t); let n = 0; for (let y = P.y0; y < P.y0 + SIZE(t); y++) for (let x = P.x0; x < P.x0 + SIZE(t); x++) if (!pal.has(P.f[y * P.W + x])) n++;
      lab(t, 'pen_clear');
      t.ok(n > 0, `Facet (anti-aliased) has ${n} px outside its palette: the proof can fail`);
      t.eq(out, [], 'palette only');
    } },
  ],
};
