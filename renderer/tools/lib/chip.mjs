// The channel chip on the README's screen pictures (tools/readme_assets.mjs; RELEASING.md "Screen channels"): a pill in
// the bottom-right corner saying BETA (accent fill, on-accent label) or COMING SOON (the docs' .badge: surface-press
// fill, text-dim label, a line-hi outline: the same chip, a quieter tone), in the docs' mono caps (JetBrains Mono 500, tracked like the docs' .badge), with a ring
// of the canvas colour so it reads on any screen. Sizes are in dp of the 480 reference, so a 480 (1x) and a 720 (1.5x)
// frame get the same chip at the same place. No dependencies: the font's outlines are read and filled here.
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const R = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
export const CHIP = { font: 20, height: 36, pad: 14, margin: 20, ring: 3, outline: 1.5, tracking: 0.08 };  // dp
export const CHIP_TEXT = { beta: 'BETA', soon: 'COMING SOON' };

const colour = JSON.parse(readFileSync(join(R, 'tokens', 'colour.json'), 'utf8')).colours;
const hex = (h) => [1, 3, 5].map((i) => parseInt(h.slice(i, i + 2), 16));
/** fill, label and ring colours of a channel's chip in a theme ('dark' | 'light') */
export function chipColours(channel, theme) {
  const c = (k) => hex(colour[k][theme]);
  return channel === 'beta' ? { fill: c('accent'), label: c('on-accent'), ring: c('bg'), outline: null }
    : { fill: c('surface-press'), label: c('text-dim'), ring: c('bg'), outline: c('line-hi') };
}

// --- TrueType outlines (glyf), enough for caps: cmap format 4, simple glyphs, one advance (a monospace font)
let FONT;
function font() {
  if (FONT) return FONT;
  const b = readFileSync(join(R, 'fonts', 'JetBrainsMono-500.ttf'));
  const t = {};
  for (let i = 0, n = b.readUInt16BE(4); i < n; i++) t[b.toString('latin1', 12 + 16 * i, 16 + 16 * i)] = b.readUInt32BE(20 + 16 * i);
  const upem = b.readUInt16BE(t.head + 18), longLoca = b.readInt16BE(t.head + 50) === 1;
  const advance = b.readUInt16BE(t.hmtx);
  const capH = b.readInt16BE(t['OS/2'] + 88);
  // cmap: the (3,1) format 4 subtable
  let sub = 0;
  for (let i = 0, n = b.readUInt16BE(t.cmap + 2); i < n; i++) {
    const r = t.cmap + 4 + 8 * i;
    if (b.readUInt16BE(r) === 3 && b.readUInt16BE(r + 2) === 1) sub = t.cmap + b.readUInt32BE(r + 4);
  }
  const segX2 = b.readUInt16BE(sub + 6), ends = sub + 14, starts = ends + segX2 + 2, deltas = starts + segX2, offs = deltas + segX2;
  const gid = (ch) => {
    for (let i = 0; i < segX2 / 2; i++) {
      if (ch > b.readUInt16BE(ends + 2 * i) || ch < b.readUInt16BE(starts + 2 * i)) continue;
      const ro = b.readUInt16BE(offs + 2 * i), d = b.readInt16BE(deltas + 2 * i);
      if (!ro) return (ch + d) & 0xffff;
      const g = b.readUInt16BE(offs + 2 * i + ro + 2 * (ch - b.readUInt16BE(starts + 2 * i)));
      return g ? (g + d) & 0xffff : 0;
    }
    return 0;
  };
  const loca = (g) => (longLoca ? b.readUInt32BE(t.loca + 4 * g) : 2 * b.readUInt16BE(t.loca + 2 * g));
  /** contours of a glyph: arrays of {x, y, on} in font units */
  const glyph = (ch) => {
    const g = gid(ch.charCodeAt(0)), o = t.glyf + loca(g);
    if (loca(g + 1) === loca(g)) return [];
    const nc = b.readInt16BE(o);
    if (nc < 0) throw new Error(`chip: ${ch} is a composite glyph`);
    const endPts = Array.from({ length: nc }, (_, i) => b.readUInt16BE(o + 10 + 2 * i));
    const np = endPts.at(-1) + 1;
    let p = o + 10 + 2 * nc;
    p += 2 + b.readUInt16BE(p);  // instructions
    const flags = [];
    while (flags.length < np) { const f = b[p++]; flags.push(f); if (f & 8) for (let r = b[p++]; r > 0; r--) flags.push(f); }
    const coord = (short, same) => {
      const out = []; let v = 0;
      for (const f of flags) {
        if (f & short) { const d = b[p++]; v += f & same ? d : -d; } else if (!(f & same)) { v += b.readInt16BE(p); p += 2; }
        out.push(v);
      }
      return out;
    };
    const xs = coord(2, 16), ys = coord(4, 32);
    const pts = xs.map((x, i) => ({ x, y: ys[i], on: !!(flags[i] & 1) }));
    return endPts.map((e, i) => pts.slice(i ? endPts[i - 1] + 1 : 0, e + 1));
  };
  FONT = { upem, advance, capH, glyph };
  return FONT;
}

/** a contour (quadratic TrueType points) as a polygon */
function flatten(c) {
  const out = [];
  const n = c.length;
  let start = c.findIndex((q) => q.on);
  let pts = c;
  if (start < 0) { pts = c.flatMap((q, i) => { const r = c[(i + 1) % n]; return [q, { x: (q.x + r.x) / 2, y: (q.y + r.y) / 2, on: true }]; }); start = 1; }
  const m = pts.length;
  out.push([pts[start].x, pts[start].y]);
  let prev = pts[start];
  for (let k = 1; k <= m; k++) {
    const q = pts[(start + k) % m];
    if (q.on) { if (!prev.on) continue; out.push([q.x, q.y]); prev = q; continue; }
    const nx = pts[(start + k + 1) % m];
    const end = nx.on ? nx : { x: (q.x + nx.x) / 2, y: (q.y + nx.y) / 2, on: true };
    const p0 = out.at(-1);
    for (let s = 1; s <= 8; s++) {
      const u = s / 8, a = (1 - u) * (1 - u), bb = 2 * (1 - u) * u, cc = u * u;
      out.push([a * p0[0] + bb * q.x + cc * end.x, a * p0[1] + bb * q.y + cc * end.y]);
    }
    prev = end;
    if (nx.on) k++;
  }
  return out;
}

/** alpha (0..1) coverage of polygons (pixel coordinates) on a W x H grid, nonzero rule, 4 x 4 samples per pixel */
function fill(polys, W, H) {
  const a = new Float32Array(W * H);
  const edges = polys.flatMap((p) => p.map((q, i) => [q, p[(i + 1) % p.length]])).filter(([p, q]) => p[1] !== q[1]);
  const SS = 4;
  for (let y = 0; y < H; y++) for (let sy = 0; sy < SS; sy++) {
    const yy = y + (sy + 0.5) / SS;
    const xs = [];
    for (const [p, q] of edges) {
      const [lo, hi] = p[1] < q[1] ? [p, q] : [q, p];
      if (yy < lo[1] || yy >= hi[1]) continue;
      xs.push([lo[0] + ((yy - lo[1]) / (hi[1] - lo[1])) * (hi[0] - lo[0]), p[1] < q[1] ? 1 : -1]);
    }
    xs.sort((u, v) => u[0] - v[0]);
    let wind = 0;
    for (let i = 0; i < xs.length - 1; i++) {
      wind += xs[i][1];
      if (!wind) continue;
      for (let sx = 0; sx < SS; sx++) {
        const off = (sx + 0.5) / SS;
        for (let x = Math.max(0, Math.ceil(xs[i][0] - off)); x + off < xs[i + 1][0] && x < W; x++) a[y * W + x] += 1 / (SS * SS);
      }
    }
  }
  return a;
}

/** the chip's box on a frame of S x S px (dp scaled by S / 480): { x, y, w, h } in px */
export function chipBox(channel, S) {
  const k = S / 480, f = font(), text = CHIP_TEXT[channel];
  const em = CHIP.font * k, adv = (f.advance / f.upem) * em, tr = CHIP.tracking * em;
  const textW = text.length * adv + (text.length - 1) * tr;
  const h = Math.round(CHIP.height * k), w = Math.round(textW + 2 * CHIP.pad * k);
  return { x: S - Math.round(CHIP.margin * k) - w, y: S - Math.round(CHIP.margin * k) - h, w, h, em, adv, tr };
}

/** draws a channel's chip (beta | soon) onto an RGBA frame of S x S px (opaque), in place */
export function drawChip(rgba, S, channel, theme) {
  if (!CHIP_TEXT[channel]) return rgba;
  const k = S / 480, f = font(), box = chipBox(channel, S), col = chipColours(channel, theme);
  const pill = (x, y, w, h) => {  // a rounded rect (radius h / 2) as a polygon
    const r = h / 2, out = [];
    for (let i = 0; i <= 16; i++) { const t = Math.PI / 2 + (Math.PI * i) / 16; out.push([x + r + r * Math.cos(t), y + r - r * Math.sin(t)]); }
    for (let i = 0; i <= 16; i++) { const t = -Math.PI / 2 + (Math.PI * i) / 16; out.push([x + w - r + r * Math.cos(t), y + r - r * Math.sin(t)]); }
    return out;
  };
  const ring = CHIP.ring * k, ol = CHIP.outline * k;
  const grow = (d) => fill([pill(box.x - d, box.y - d, box.w + 2 * d, box.h + 2 * d)], S, S);
  const layers = [[grow(ring), col.ring], ...(col.outline ? [[grow(0), col.outline]] : []), [col.outline ? grow(-ol) : grow(0), col.fill]];
  // the label: caps centred on the pill's middle line
  const sc = box.em / f.upem, base = box.y + box.h / 2 + (f.capH * sc) / 2;
  const polys = [];
  let x = box.x + CHIP.pad * k;
  for (const ch of CHIP_TEXT[channel]) {
    for (const c of f.glyph(ch)) polys.push(flatten(c).map(([gx, gy]) => [x + gx * sc, base - gy * sc]));
    x += box.adv + box.tr;
  }
  layers.push([fill(polys, S, S), col.label]);
  for (const [a, c] of layers) for (let i = 0; i < S * S; i++) {
    const v = Math.min(1, a[i]);
    if (!v) continue;
    for (let j = 0; j < 3; j++) rgba[i * 4 + j] = Math.round(rgba[i * 4 + j] * (1 - v) + c[j] * v);
  }
  return rgba;
}
