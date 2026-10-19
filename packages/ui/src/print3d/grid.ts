// The 3D print screen's model as a voxel grid, so one renderer handles every source:
//   - a mesh (binary STL): a vertical ray per column, filled between hits, sliced at a fixed layer height
//   - sliced gcode (Bambu Studio / Orca style comments): every extruding move stamped into its print layer, so the
//     cut face shows the real walls and infill. Bambu printers send a .gcode.3mf with no mesh in it, only this gcode
//   - a shape function (`fromShape`), for demos
// Slices run bottom to top; zb[k]..zb[k+1] = slice k in mm. The print's own layers map to mm through layerZ, so the
// screen cuts at the current layer exactly (layerTop), not at a % of time or volume.
// No DOM or node imports: it runs in the browser and on a server (to send rendered frames to a device that can't draw).

export interface Grid {
  nx: number;
  ny: number;
  nz: number;
  cell: number; // mm per cell in x and y
  x0: number;
  y0: number; // mm of cell (0, 0)'s corner (bed coordinates for gcode)
  zb: number[]; // nz + 1 slice boundaries, mm
  layerZ: number[]; // print layer n (1-based) tops at layerZ[n - 1] mm
  data: Uint8Array; // 1 = filled; index (k * ny + j) * nx + i
  name?: string;
}

export const voxelAt = (g: Grid, i: number, j: number, k: number) =>
  i < 0 || j < 0 || k < 0 || i >= g.nx || j >= g.ny || k >= g.nz
    ? 0
    : g.data[(k * g.ny + j) * g.nx + i];

/** the slice count a cut at mm z keeps solid (slices whose top is at or below z) */
export function solidSlices(g: Grid, z: number) {
  let k = 0;
  while (k < g.nz && g.zb[k + 1] <= z + 1e-4) k++;
  return k;
}
export const layerTop = (g: Grid, layer: number) =>
  layer <= 0 ? 0 : (g.layerZ[Math.min(layer, g.layerZ.length) - 1] ?? 0);

/** quarter turns around z (0..3) so the model faces the viewer the way we want (the iso view sees +x, +y, +z faces) */
export function rotate(g: Grid, r: number): Grid {
  r = ((r % 4) + 4) % 4;
  if (!r) return g;
  const nx = r % 2 ? g.ny : g.nx,
    ny = r % 2 ? g.nx : g.ny,
    data = new Uint8Array(g.data.length);
  for (let k = 0; k < g.nz; k++)
    for (let j = 0; j < g.ny; j++)
      for (let i = 0; i < g.nx; i++) {
        if (!g.data[(k * g.ny + j) * g.nx + i]) continue;
        const [a, b] =
          r === 1 ? [g.ny - 1 - j, i] : r === 2 ? [g.nx - 1 - i, g.ny - 1 - j] : [j, g.nx - 1 - i];
        data[(k * ny + b) * nx + a] = 1;
      }
  return { ...g, nx, ny, data };
}

// ---------------------------------------------------------------- transport: run lengths of the flat array, base64
export function encode(g: Grid) {
  const runs: number[] = [];
  let cur = 0,
    n = 0;
  for (let p = 0; p < g.data.length; p++) {
    if (g.data[p] === cur) {
      n++;
      continue;
    }
    runs.push(n);
    cur = g.data[p];
    n = 1;
  }
  runs.push(n);
  const bytes: number[] = []; // LEB128 varints
  for (let v of runs) {
    do {
      let b = v & 127;
      v >>>= 7;
      if (v) b |= 128;
      bytes.push(b);
    } while (v);
  }
  let s = '';
  for (let p = 0; p < bytes.length; p += 8192)
    s += String.fromCharCode(...bytes.slice(p, p + 8192));
  const { data, ...meta } = g;
  return { ...meta, rle: btoa(s) };
}
export function decode(o: ReturnType<typeof encode>): Grid {
  const { rle, ...meta } = o;
  const s = atob(rle),
    data = new Uint8Array(meta.nx * meta.ny * meta.nz);
  let p = 0,
    cur = 0,
    v = 0,
    sh = 0;
  for (let q = 0; q < s.length; q++) {
    const b = s.charCodeAt(q);
    v |= (b & 127) << sh;
    sh += 7;
    if (b & 128) continue;
    if (cur) data.fill(1, p, p + v);
    p += v;
    cur ^= 1;
    v = 0;
    sh = 0;
  }
  return { ...meta, data };
}

// ---------------------------------------------------------------- mesh (binary STL)
/** Binary STL -> grid: per column (cell centre) a vertical ray, sorted hits, filled between pairs. */
export function fromStl(
  buf: ArrayBuffer,
  o: { cell?: number; layer?: number; maxCells?: number; name?: string } = {},
): Grid {
  const dv = new DataView(buf),
    n = dv.getUint32(80, true);
  const v = new Float32Array(n * 9);
  for (let t = 0; t < n; t++)
    for (let q = 0; q < 9; q++) v[t * 9 + q] = dv.getFloat32(84 + t * 50 + 12 + q * 4, true);
  let mnx = Infinity,
    mny = Infinity,
    mnz = Infinity,
    mxx = -Infinity,
    mxy = -Infinity,
    mxz = -Infinity;
  for (let p = 0; p < v.length; p += 3) {
    mnx = Math.min(mnx, v[p]);
    mxx = Math.max(mxx, v[p]);
    mny = Math.min(mny, v[p + 1]);
    mxy = Math.max(mxy, v[p + 1]);
    mnz = Math.min(mnz, v[p + 2]);
    mxz = Math.max(mxz, v[p + 2]);
  }
  const layer = o.layer ?? 0.2;
  const cell = o.cell ?? Math.max(0.3, Math.max(mxx - mnx, mxy - mny) / (o.maxCells ?? 160));
  const nx = Math.ceil((mxx - mnx) / cell),
    ny = Math.ceil((mxy - mny) / cell);
  const layers = Math.ceil((mxz - mnz) / layer - 1e-6),
    slices = Math.min(layers, 320);
  const layerZ = Array.from({ length: layers }, (_, i) => +((i + 1) * layer).toFixed(4));
  const zb = Array.from({ length: slices + 1 }, (_, k) =>
    k === slices ? layerZ[layers - 1] : (layerZ[Math.round((k * layers) / slices) - 1] ?? 0),
  );
  const hits: number[][] = Array.from({ length: nx * ny }, () => []);
  for (let t = 0; t < n; t++) {
    const b = t * 9;
    const ax = v[b] - mnx,
      ay = v[b + 1] - mny,
      az = v[b + 2] - mnz,
      bx = v[b + 3] - mnx,
      by = v[b + 4] - mny,
      bz = v[b + 5] - mnz;
    const cx = v[b + 6] - mnx,
      cy = v[b + 7] - mny,
      cz = v[b + 8] - mnz;
    const d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy);
    if (Math.abs(d) < 1e-12) continue; // edge-on to the ray
    const i0 = Math.max(0, Math.ceil(Math.min(ax, bx, cx) / cell - 0.5)),
      i1 = Math.min(nx - 1, Math.floor(Math.max(ax, bx, cx) / cell - 0.5));
    const j0 = Math.max(0, Math.ceil(Math.min(ay, by, cy) / cell - 0.5)),
      j1 = Math.min(ny - 1, Math.floor(Math.max(ay, by, cy) / cell - 0.5));
    for (let j = j0; j <= j1; j++)
      for (let i = i0; i <= i1; i++) {
        const px = (i + 0.5) * cell + 1e-5,
          py = (j + 0.5) * cell + 2e-5; // nudge off shared edges
        const l1 = ((by - cy) * (px - cx) + (cx - bx) * (py - cy)) / d,
          l2 = ((cy - ay) * (px - cx) + (ax - cx) * (py - cy)) / d,
          l3 = 1 - l1 - l2;
        if (l1 < 0 || l2 < 0 || l3 < 0) continue;
        hits[j * nx + i].push(l1 * az + l2 * bz + l3 * cz);
      }
  }
  const data = new Uint8Array(nx * ny * slices);
  for (let c = 0; c < hits.length; c++) {
    const h = hits[c].sort((a, b) => a - b);
    for (let q = 0; q + 1 < h.length; q += 2)
      for (let k = 0; k < slices; k++) {
        const zc = (zb[k] + zb[k + 1]) / 2;
        if (zc >= h[q] && zc <= h[q + 1]) data[k * nx * ny + c] = 1;
      }
  }
  return { nx, ny, nz: slices, cell, x0: mnx, y0: mny, zb, layerZ, data, name: o.name };
}

// ---------------------------------------------------------------- sliced gcode
const SKIP = /prime tower|wipe tower|skirt|purge|custom/i; // not part of the model (Custom = start/end gcode lines)

/** Sliced gcode (Bambu Studio / Orca Slicer comments) -> grid: layers from "; Z_HEIGHT:", features from "; FEATURE:", relative E (M83), arcs as
 *  chords. Every extruding move is stamped into its layer's slice. */
export function fromGcode(
  text: string,
  o: { maxCells?: number; maxSlices?: number; name?: string } = {},
): Grid {
  const segs: number[] = []; // x0 y0 x1 y1 layer
  const layerZ: number[] = [];
  let x = 0,
    y = 0,
    rel = false,
    relE = false,
    skip = true,
    L = -1;
  let p = 0;
  while (p < text.length) {
    let e = text.indexOf('\n', p);
    if (e < 0) e = text.length;
    const line = text.slice(p, e).trim();
    p = e + 1;
    if (!line) continue;
    if (line[0] === ';') {
      if (line.startsWith('; Z_HEIGHT:')) {
        layerZ.push(parseFloat(line.slice(11)));
        L = layerZ.length - 1;
      } else if (line.startsWith('; FEATURE:')) skip = SKIP.test(line);
      continue;
    }
    const code = line.split(/[\s;]/, 1)[0];
    if (code === 'G90') rel = false;
    else if (code === 'G91') rel = true;
    else if (code === 'M83') relE = true;
    else if (code === 'M82') relE = false;
    else if (code === 'G92') {
      const m = /X(-?[\d.]+)/.exec(line),
        n = /Y(-?[\d.]+)/.exec(line);
      if (m) x = +m[1];
      if (n) y = +n[1];
    }
    if (code !== 'G1' && code !== 'G0' && code !== 'G2' && code !== 'G3') continue;
    let nx = x,
      ny = y,
      ex = 0,
      I = 0,
      J = 0;
    for (const w of line.split(';', 1)[0].split(/\s+/)) {
      const c = w[0],
        val = parseFloat(w.slice(1));
      if (c === 'X') nx = rel ? x + val : val;
      else if (c === 'Y') ny = rel ? y + val : val;
      else if (c === 'E') ex = val;
      else if (c === 'I') I = val;
      else if (c === 'J') J = val;
    }
    const extrude = (relE || rel) && ex > 0 && !skip && L >= 0; // absolute E: not used by these slicers
    if (extrude && (nx !== x || ny !== y)) {
      if (code === 'G2' || code === 'G3') {
        const cx = x + I,
          cy = y + J,
          r = Math.hypot(I, J);
        let a0 = Math.atan2(y - cy, x - cx),
          a1 = Math.atan2(ny - cy, nx - cx);
        if (code === 'G3' && a1 <= a0) a1 += 2 * Math.PI;
        if (code === 'G2' && a1 >= a0) a1 -= 2 * Math.PI;
        const steps = Math.max(2, Math.ceil((Math.abs(a1 - a0) * r) / 0.5));
        let px = x,
          py = y;
        for (let s = 1; s <= steps; s++) {
          const a = a0 + ((a1 - a0) * s) / steps,
            qx = cx + r * Math.cos(a),
            qy = cy + r * Math.sin(a);
          segs.push(px, py, qx, qy, L);
          px = qx;
          py = qy;
        }
      } else segs.push(x, y, nx, ny, L);
    }
    x = nx;
    y = ny;
  }
  let mnx = Infinity,
    mny = Infinity,
    mxx = -Infinity,
    mxy = -Infinity;
  for (let s = 0; s < segs.length; s += 5) {
    mnx = Math.min(mnx, segs[s], segs[s + 2]);
    mxx = Math.max(mxx, segs[s], segs[s + 2]);
    mny = Math.min(mny, segs[s + 1], segs[s + 3]);
    mxy = Math.max(mxy, segs[s + 1], segs[s + 3]);
  }
  const W = 0.45; // line width: pad the box by half of it
  mnx -= W / 2;
  mny -= W / 2;
  mxx += W / 2;
  mxy += W / 2;
  const cell = Math.max(0.4, Math.max(mxx - mnx, mxy - mny) / (o.maxCells ?? 200));
  const nx = Math.ceil((mxx - mnx) / cell),
    ny = Math.ceil((mxy - mny) / cell);
  const layers = layerZ.length,
    slices = Math.min(layers, o.maxSlices ?? 320);
  const sliceOf = (l: number) => Math.min(slices - 1, Math.floor((l * slices) / layers));
  const zb = [0];
  for (let k = 0; k < slices; k++)
    zb.push(layerZ[Math.min(layers, Math.round(((k + 1) * layers) / slices)) - 1]);
  const data = new Uint8Array(nx * ny * slices);
  for (let s = 0; s < segs.length; s += 5) {
    const ax = segs[s] - mnx,
      ay = segs[s + 1] - mny,
      bx = segs[s + 2] - mnx,
      by = segs[s + 3] - mny,
      k = sliceOf(segs[s + 4]);
    const steps = Math.max(1, Math.ceil(Math.hypot(bx - ax, by - ay) / (cell / 2)));
    for (let q = 0; q <= steps; q++) {
      const i = Math.floor((ax + ((bx - ax) * q) / steps) / cell),
        j = Math.floor((ay + ((by - ay) * q) / steps) / cell);
      if (i >= 0 && j >= 0 && i < nx && j < ny) data[(k * ny + j) * nx + i] = 1;
    }
  }
  return { nx, ny, nz: slices, cell, x0: mnx, y0: mny, zb, layerZ, data, name: o.name };
}

// ---------------------------------------------------------------- shape function (demos)
/** A grid from `inside(x, y, z)` (mm, z up from the bed) over a w x d x h mm box. */
export function fromShape(
  inside: (x: number, y: number, z: number) => boolean,
  o: { w: number; d: number; h: number; cell?: number; layer?: number; name?: string },
): Grid {
  const cell = o.cell ?? 0.5,
    layer = o.layer ?? 0.2;
  const nx = Math.ceil(o.w / cell),
    ny = Math.ceil(o.d / cell),
    nz = Math.ceil(o.h / layer);
  const data = new Uint8Array(nx * ny * nz);
  for (let k = 0; k < nz; k++)
    for (let j = 0; j < ny; j++)
      for (let i = 0; i < nx; i++)
        if (inside((i + 0.5) * cell, (j + 0.5) * cell, (k + 0.5) * layer))
          data[(k * ny + j) * nx + i] = 1;
  const layerZ = Array.from({ length: nz }, (_, k) => +((k + 1) * layer).toFixed(4));
  return { nx, ny, nz, cell, x0: 0, y0: 0, zb: [0, ...layerZ], layerZ, data, name: o.name };
}
