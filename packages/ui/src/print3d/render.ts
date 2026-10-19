// Isometric render of a print grid: below the cut is the printed part, solid, in flat tones; the last few layers
// printed wear the accent (the one thing that matters); above the cut is a ghost: a faint fill and hairline edges, with
// the printed part showing through it. Shading: `faces` = one tone per face direction (default), `smooth` = normals
// from the voxel neighbourhood in a few flat bands.
// Software z-buffer: each exposed voxel face (+x, +y, +z, the faces a true-iso camera sees) is filled as a parallelogram,
// supersampled, then averaged down. Output = RGBA bytes: `putImageData` in a browser, or encode it on a server and send
// the frame to a device.
import { color } from '@facet/tokens';
import { voxelAt, solidSlices, type Grid } from './grid.js';

export type Rgb = [number, number, number];
export interface Palette {
  bg: Rgb | null; // null = transparent
  lo: Rgb;
  hi: Rgb; // solid tone ramp (darkest face .. lit top)
  hot: Rgb;
  hotLo: Rgb; // the layer being printed
  ghostLo: Rgb;
  ghostHi: Rgb;
  ghostEdge: Rgb;
  ghostAlpha: number; // ghost fill over the solid part behind it
}
export interface RenderOpts {
  w: number;
  h: number;
  cutZ: number; // mm printed so far (layerTop(grid, current_layer)); >= top = finished, all solid
  pal: Palette;
  shade?: 'faces' | 'smooth'; // faces = 3 tones by face direction; smooth = normals from the voxel neighbourhood
  bands?: number; // tone bands for smooth (flat steps, no gradient)
  ss?: number; // supersampling (2 = 4 samples per pixel)
  pad?: number; // px around the model
  hotSlices?: number; // how many slices below the cut wear the accent (1 = one layer)
  frame?: Grid; // fit to this grid's box instead (keep scale while rotating etc.)
}

const hex = (s: string): Rgb => [
  parseInt(s.slice(1, 3), 16),
  parseInt(s.slice(3, 5), 16),
  parseInt(s.slice(5, 7), 16),
];
const mix = (a: Rgb, b: Rgb, t: number): Rgb => [
  a[0] + (b[0] - a[0]) * t,
  a[1] + (b[1] - a[1]) * t,
  a[2] + (b[2] - a[2]) * t,
];

/** Facet palettes, colours from tokens.json only (dark / light); solid = neutral (text ramp), steel (blue) or a filament
 *  colour (the AMS tray's, "#RRGGBB") */
export function palette(
  theme: 'dark' | 'light',
  solid: 'neutral' | 'steel' | string = 'neutral',
): Palette {
  const c = (id: keyof typeof color) => hex(color[id][theme]);
  const dark = theme === 'dark';
  const base: Rgb | null =
    solid === 'neutral' || !/^(steel|#[0-9a-f]{6})$/i.test(solid)
      ? null
      : solid === 'steel'
        ? c('blue')
        : hex(solid);
  return {
    bg: c('bg'),
    lo: base ? mix(base, [0, 0, 0], dark ? 0.7 : 0.55) : c(dark ? 'line' : 'text-dim'),
    hi: base ? mix(base, [255, 255, 255], dark ? 0.1 : 0.2) : c(dark ? 'text' : 'surface-press'),
    hot: c('accent'),
    hotLo: c('accent-press'),
    ghostLo: c(dark ? 'surface' : 'surface-press'),
    ghostHi: c(dark ? 'surface-press' : 'line'),
    ghostEdge: c(dark ? 'text-mute' : 'line-hi'),
    ghostAlpha: 0.6,
  };
}

const C30 = Math.cos(Math.PI / 6),
  S30 = 0.5;

/** the model's iso projection extents in mm (unscaled screen units), from its occupied voxels; cached per grid */
const BOUNDS = new WeakMap<Grid, { x0: number; x1: number; y0: number; y1: number }>();
export function bounds(g: Grid) {
  let b = BOUNDS.get(g);
  if (b) return b;
  b = { x0: Infinity, x1: -Infinity, y0: Infinity, y1: -Infinity };
  for (let k = 0; k < g.nz; k++)
    for (let j = 0; j < g.ny; j++)
      for (let i = 0; i < g.nx; i++) {
        if (g.data[(k * g.ny + j) * g.nx + i] !== 1) continue;
        const x0 = i * g.cell,
          y0 = j * g.cell,
          x1 = x0 + g.cell,
          y1 = y0 + g.cell;
        b.x0 = Math.min(b.x0, (x0 - y1) * C30);
        b.x1 = Math.max(b.x1, (x1 - y0) * C30);
        b.y0 = Math.min(b.y0, (x0 + y0) * S30 - g.zb[k + 1]);
        b.y1 = Math.max(b.y1, (x1 + y1) * S30 - g.zb[k]);
      }
  BOUNDS.set(g, b);
  return b;
}
const L = [0.38, 0.66, 0.94]; // light: lit top, +y (left) face mid, +x (right) face dark

/** surface normal from the occupied neighbourhood (radius ~1 mm), pointing out of the part; zero inside */
function normalAt(g: Grid, i: number, j: number, k: number): [number, number, number] {
  const R = 3,
    h = g.zb[k + 1] - g.zb[k] || 0.2,
    RZ = Math.max(1, Math.round((R * g.cell) / h));
  let x = 0,
    y = 0,
    z = 0;
  for (let c = -RZ; c <= RZ; c++)
    for (let b = -R; b <= R; b++)
      for (let a = -R; a <= R; a++)
        if (voxelAt(g, i + a, j + b, k + c)) {
          x -= a * g.cell;
          y -= b * g.cell;
          z -= c * h;
        }
  const m = Math.hypot(x, y, z);
  return m < 1e-6 ? [0, 0, 0] : [x / m, y / m, z / m];
}
/** per-grid normal cache (NaN = not computed yet), so scrubbing through layers only rasterises */
const NORMALS = new WeakMap<Grid, Float32Array>();
const normals = (g: Grid) => {
  let n = NORMALS.get(g);
  if (!n) {
    n = new Float32Array(g.data.length * 3).fill(NaN);
    NORMALS.set(g, n);
  }
  return n;
};

export function render(g: Grid, o: RenderOpts): Uint8ClampedArray {
  const ss = o.ss ?? 2,
    W = o.w * ss,
    H = o.h * ss,
    pad = (o.pad ?? 16) * ss;
  // fit the full model (not the printed part) so it never moves while it fills up
  const b = bounds(o.frame ?? g);
  const sc = Math.min((W - 2 * pad) / (b.x1 - b.x0), (H - 2 * pad) / (b.y1 - b.y0));
  const ox = W / 2 - ((b.x0 + b.x1) / 2) * sc,
    oy = H / 2 - ((b.y0 + b.y1) / 2) * sc;
  const PX = (x: number, y: number) => ox + (x - y) * C30 * sc;
  const PY = (x: number, y: number, z: number) => oy + ((x + y) * S30 - z) * sc;

  const cut = solidSlices(g, o.cutZ),
    hotFrom = cut - (o.hotSlices ?? 4);
  const done = cut >= g.nz;
  const N = W * H;
  const sd = new Float32Array(N).fill(-Infinity),
    gd = new Float32Array(N).fill(-Infinity);
  const sc0 = new Float32Array(N * 3),
    gt = new Float32Array(N),
    hotPx = new Uint8Array(N); // solid colour, ghost tone
  const { nx, ny, nz, cell, zb, data } = g;
  const occ = (i: number, j: number, k: number, lim: number) =>
    i >= 0 && j >= 0 && k >= 0 && i < nx && j < ny && k < lim && data[(k * ny + j) * nx + i] === 1;

  const bands = o.bands ?? 5,
    smooth = (o.shade ?? 'faces') === 'smooth';
  const tone = (t: number) => {
    t = Math.max(0, Math.min(1, t));
    return smooth ? Math.round(t * (bands - 1)) / (bands - 1) : t;
  };
  const nrm = normals(g);
  const normal = (i: number, j: number, k: number): [number, number, number] | null => {
    const p = ((k * ny + j) * nx + i) * 3;
    if (Number.isNaN(nrm[p])) {
      const v = normalAt(g, i, j, k);
      nrm[p] = v[0];
      nrm[p + 1] = v[1];
      nrm[p + 2] = v[2];
    }
    return nrm[p] || nrm[p + 1] || nrm[p + 2] ? [nrm[p], nrm[p + 1], nrm[p + 2]] : null; // inside (a cut face): by face
  };

  // one parallelogram, corners p0, p0 + u, p0 + v (screen px), constant depth
  const quad = (
    x0: number,
    y0: number,
    ux: number,
    uy: number,
    vx: number,
    vy: number,
    d: number,
    buf: Float32Array,
    put: (p: number) => void,
  ) => {
    const xs = [x0, x0 + ux, x0 + vx, x0 + ux + vx],
      ys = [y0, y0 + uy, y0 + vy, y0 + uy + vy];
    const minX = Math.max(0, Math.floor(Math.min(...xs))),
      maxX = Math.min(W - 1, Math.ceil(Math.max(...xs)));
    const minY = Math.max(0, Math.floor(Math.min(...ys))),
      maxY = Math.min(H - 1, Math.ceil(Math.max(...ys)));
    const det = ux * vy - uy * vx;
    if (!det) return;
    for (let py = minY; py <= maxY; py++)
      for (let px = minX; px <= maxX; px++) {
        const dx = px + 0.5 - x0,
          dy = py + 0.5 - y0;
        const a = (dx * vy - dy * vx) / det,
          b = (ux * dy - uy * dx) / det;
        if (a < 0 || a > 1 || b < 0 || b > 1) continue;
        const p = py * W + px;
        if (d <= buf[p]) continue;
        buf[p] = d;
        put(p);
      }
  };

  const pass = (ghost: boolean) => {
    const lim = ghost ? nz : cut,
      k0 = ghost ? cut : 0;
    const buf = ghost ? gd : sd;
    for (let k = k0; k < lim; k++) {
      const z0 = zb[k],
        z1 = zb[k + 1];
      for (let j = 0; j < ny; j++)
        for (let i = 0; i < nx; i++) {
          if (data[(k * ny + j) * nx + i] !== 1) continue;
          // the ghost is everything (so the ghost hull stays whole); the solid only what's printed
          const top = !occ(i, j, k + 1, ghost ? nz : lim),
            right = !occ(i + 1, j, k, lim),
            left = !occ(i, j + 1, k, lim);
          if (!top && !right && !left) continue;
          const x0 = i * cell,
            x1 = x0 + cell,
            y0 = j * cell,
            y1 = y0 + cell;
          const n = smooth ? normal(i, j, k) : null;
          const hot = !ghost && !done && k >= hotFrom;
          const put = (t: number) => {
            const tt = tone(n ? n[0] * L[0] + n[1] * L[1] + n[2] * L[2] : t);
            if (ghost)
              return (p: number) => {
                gt[p] = tt;
              };
            const c = hot ? mix(o.pal.hotLo, o.pal.hot, tt) : mix(o.pal.lo, o.pal.hi, tt);
            const hf = hot ? 1 : 0;
            return (p: number) => {
              sc0[p * 3] = c[0];
              sc0[p * 3 + 1] = c[1];
              sc0[p * 3 + 2] = c[2];
              hotPx[p] = hf;
            };
          };
          if (top)
            quad(
              PX(x0, y0),
              PY(x0, y0, z1),
              PX(x1, y0) - PX(x0, y0),
              PY(x1, y0, z1) - PY(x0, y0, z1),
              PX(x0, y1) - PX(x0, y0),
              PY(x0, y1, z1) - PY(x0, y0, z1),
              x0 + y0 + cell + z1,
              buf,
              put(L[2]),
            );
          if (right)
            quad(
              PX(x1, y0),
              PY(x1, y0, z1),
              PX(x1, y1) - PX(x1, y0),
              PY(x1, y1, z1) - PY(x1, y0, z1),
              0,
              (z1 - z0) * sc,
              x1 + y1 + (z0 + z1) / 2 - cell / 2,
              buf,
              put(L[0]),
            );
          if (left)
            quad(
              PX(x0, y1),
              PY(x0, y1, z1),
              PX(x1, y1) - PX(x0, y1),
              PY(x1, y1, z1) - PY(x0, y1, z1),
              0,
              (z1 - z0) * sc,
              x1 + y1 + (z0 + z1) / 2 - cell / 2,
              buf,
              put(L[1]),
            );
        }
    }
  };
  pass(false);
  if (!done) pass(true);

  // ghost edges: silhouette + creases (depth jumps) at ~1 px of the final image
  const edge = new Uint8Array(N);
  if (!done) {
    const jump = 2.5 * cell,
      r = Math.max(1, Math.round(ss * 0.75));
    for (let y = 0; y < H; y++)
      for (let x = 0; x < W; x++) {
        const p = y * W + x,
          d = gd[p];
        if (d === -Infinity) continue;
        for (const [a, b] of [
          [r, 0],
          [-r, 0],
          [0, r],
          [0, -r],
        ]) {
          const X = x + a,
            Y = y + b,
            q = Y * W + X;
          if (X < 0 || Y < 0 || X >= W || Y >= H || gd[q] === -Infinity || gd[q] - d > jump) {
            edge[p] = 1;
            break;
          }
        }
      }
  }

  // compose + downsample
  const out = new Uint8ClampedArray(o.w * o.h * 4),
    pal = o.pal;
  for (let y = 0; y < o.h; y++)
    for (let x = 0; x < o.w; x++) {
      let r = 0,
        gg = 0,
        b = 0,
        a = 0;
      for (let sy = 0; sy < ss; sy++)
        for (let sx = 0; sx < ss; sx++) {
          const p = (y * ss + sy) * W + x * ss + sx;
          let c: Rgb | null = pal.bg,
            al = pal.bg ? 1 : 0;
          if (sd[p] !== -Infinity) {
            c = [sc0[p * 3], sc0[p * 3 + 1], sc0[p * 3 + 2]];
            al = 1;
          }
          if (gd[p] !== -Infinity && gd[p] > sd[p]) {
            const gc = mix(pal.ghostLo, pal.ghostHi, gt[p]);
            if (sd[p] !== -Infinity)
              c = mix(c!, gc, hotPx[p] ? pal.ghostAlpha * 0.3 : pal.ghostAlpha);
            else {
              c = gc;
              al = 1;
            }
            if (edge[p]) c = mix(c, pal.ghostEdge, sd[p] !== -Infinity ? 0.6 : 1);
          }
          if (c) {
            r += c[0] * al;
            gg += c[1] * al;
            b += c[2] * al;
            a += al;
          }
        }
      const q = (y * o.w + x) * 4,
        n = ss * ss;
      if (a) {
        out[q] = r / a;
        out[q + 1] = gg / a;
        out[q + 2] = b / a;
      }
      out[q + 3] = (a / n) * 255;
    }
  return out;
}
