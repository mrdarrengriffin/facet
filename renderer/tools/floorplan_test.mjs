#!/usr/bin/env node
// Floor plan generator tests (core/floorplan_auto.cpp, through the WASM build's fw_fpgen): every adapter on public
// fixtures (tests/floorplan/README.md: sources + licences), plus the cleanup's quality checks done again here, independently:
// outlines rectilinear, clockwise, on the grid, not self-crossing, rooms not overlapping, plausible sizes, every room there.
//   valetudo_json      Valetudo's own test maps (Roborock S5, Dreame D9, Viomi V7): each room vs its exact segment (IoU)
//   room_colour_image  the same maps drawn as pictures in two renderer styles (a Roborock-like and a Dreame-like palette,
//                      path + robot drawn over, upscaled): must give the same rooms as the JSON + the same robot spot;
//                      the Xiaomi Cloud Map Extractor's sample picture (real renderer output, colours reused across rooms)
//   polygons           made-up outlines in mm, y up, one slanted corner, one overlap
// Usage: node tools/floorplan_test.mjs [--build <out dir>] [--den 1.5x] [--verbose]. Exit 1 on any failure.
import { readFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { inflateSync } from 'node:zlib';

const HERE = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const FX = join(HERE, 'tests', 'floorplan');
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const verbose = process.argv.includes('--verbose');
const env = Object.fromEntries((() => { try { return readFileSync(join(HERE, '..', 'local.env'), 'utf8'); } catch { return ''; } })()
  .split('\n').filter((l) => /^\w+=/.test(l)).map((l) => [l.split('=')[0], l.slice(l.indexOf('=') + 1)]));
const BUILD = arg('--build', process.env.FW_OUT || env.FW_OUT || join(HERE, 'out'));
const den = arg('--den', '1.5x');

export function decodePng(buf) {  // 8-bit RGB / RGBA, non-interlaced
  let p = 8, W = 0, H = 0, ct = 0; const idat = [];
  while (p < buf.length) { const l = buf.readUInt32BE(p), t = buf.toString('ascii', p + 4, p + 8), d = buf.subarray(p + 8, p + 8 + l); if (t === 'IHDR') { W = d.readUInt32BE(0); H = d.readUInt32BE(4); ct = d[9]; } else if (t === 'IDAT') idat.push(d); p += 12 + l; }
  const bpp = ct === 6 ? 4 : 3, raw = inflateSync(Buffer.concat(idat)), out = new Uint8Array(W * H * 4), line = W * bpp, prev = new Uint8Array(line), cur = new Uint8Array(line);
  for (let y = 0; y < H; y++) {
    const f = raw[y * (line + 1)], s = raw.subarray(y * (line + 1) + 1, (y + 1) * (line + 1));
    for (let i = 0; i < line; i++) { const a = i >= bpp ? cur[i - bpp] : 0, b = prev[i], c = i >= bpp ? prev[i - bpp] : 0; let v = s[i];
      if (f === 1) v += a; else if (f === 2) v += b; else if (f === 3) v += (a + b) >> 1; else if (f === 4) { const pp = a + b - c, pa = Math.abs(pp - a), pb = Math.abs(pp - b), pc = Math.abs(pp - c); v += pa <= pb && pa <= pc ? a : pb <= pc ? b : c; }
      cur[i] = v & 255; }
    for (let x = 0; x < W; x++) { for (let k = 0; k < 3; k++) out[(y * W + x) * 4 + k] = cur[x * bpp + k]; out[(y * W + x) * 4 + 3] = bpp === 4 ? cur[x * bpp + 3] : 255; }
    prev.set(cur);
  }
  return { W, H, px: out };
}

export async function loadGen(build = BUILD, d = den) {
  const dir = join(build, d);
  const F = (await import(pathToFileURL(join(dir, 'fw.mjs')).href)).default;
  const wasm = await WebAssembly.compile(readFileSync(join(dir, 'fw.wasm')));
  const m = await F({ instantiateWasm: (imports, ok) => { WebAssembly.instantiate(wasm, imports).then((i) => ok(i, wasm)); return {}; } });
  // gen(opts, {text} | {rgba, W, H}) -> report object
  return (opts, src) => {
    const po = m.stringToNewUTF8(JSON.stringify(opts)), pt = src.text ? m.stringToNewUTF8(src.text) : 0;
    let pr = 0;
    if (src.rgba) { pr = m._malloc(src.rgba.length); m.HEAPU8.set(src.rgba, pr); }
    try { return JSON.parse(m.UTF8ToString(m._fw_fpgen(po, pt, pr, src.W || 0, src.H || 0))); }
    finally { m._free(po); if (pt) m._free(pt); if (pr) m._free(pr); }
  };
}

// ---- geometry (plan metres)
const area = (v) => Math.abs(v.reduce((s, p, i) => s + p[0] * v[(i + 1) % v.length][1] - v[(i + 1) % v.length][0] * p[1], 0)) / 2;
const signed = (v) => v.reduce((s, p, i) => s + p[0] * v[(i + 1) % v.length][1] - v[(i + 1) % v.length][0] * p[1], 0) / 2;
const inside = (v, x, z) => { let c = false; for (let i = 0, j = v.length - 1; i < v.length; j = i++) { const a = v[i], b = v[j]; if ((a[1] > z) !== (b[1] > z) && x < a[0] + ((z - a[1]) * (b[0] - a[0])) / (b[1] - a[1])) c = !c; } return c; };
const cross = (a, b, c, d) => { // proper crossing or overlap of two axis-aligned segments that don't share an end
  const o = (p, q, r) => Math.sign((q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0]));
  return o(a, b, c) * o(a, b, d) < 0 && o(c, d, a) * o(c, d, b) < 0; };
// independent checks of one report: returns a list of problems
function validate(r, grid = 0.05) {
  const bad = [], all = [...r.rooms, ...r.spare];
  for (const room of all) {
    const v = room.poly, n = v.length;
    if (n < 4) { bad.push(`${room.name}: ${n} corners`); continue; }
    for (let i = 0; i < n; i++) {
      const a = v[i], b = v[(i + 1) % n];
      if (a[0] !== b[0] && a[1] !== b[1]) bad.push(`${room.name}: slanted edge`);
      for (const c of a) if (Math.abs(c / grid - Math.round(c / grid)) > 1e-6) bad.push(`${room.name}: ${c} off the ${grid} grid`);
      for (let j = i + 2; j < n; j++) if ((j + 1) % n !== i && cross(a, b, v[j], v[(j + 1) % n])) bad.push(`${room.name}: edges cross`);
    }
    if (signed(v) <= 0) bad.push(`${room.name}: not clockwise on screen`);
  }
  // overlaps: sample every 2.5 cm cell centre
  const xs = all.flatMap((q) => q.poly.map((p) => p[0])), zs = all.flatMap((q) => q.poly.map((p) => p[1]));
  const s = grid / 2;
  for (let x = Math.min(...xs) + s / 2; x < Math.max(...xs); x += s) for (let z = Math.min(...zs) + s / 2; z < Math.max(...zs); z += s) {
    let k = 0; for (const q of all) if (inside(q.poly, x, z)) k++;
    if (k > 1) { bad.push(`overlap at ${x.toFixed(2)},${z.toFixed(2)}`); x = Infinity; break; }
  }
  for (const room of r.rooms) if (room.m2 < 0.5 || room.m2 > 400) bad.push(`${room.name}: ${room.m2} m2`);
  if (Math.abs(r.size[0] - Math.max(...xs)) > 0.011 || Math.min(...xs) !== 0 || Math.min(...zs) !== 0) bad.push('plan origin is not its rear-left corner');
  return [...new Set(bad.concat(r.checks || []))];
}
const tf = (r) => r.transform.split(',').map(Number);
const toPlan = (r, X, Y) => { const t = tf(r); return [t[0] * X + t[1] * Y + t[2], t[3] * X + t[4] * Y + t[5]]; };

// ---- Valetudo maps: exact segment pixels, and a picture of them in a renderer style
export function valetudo(map) {
  const ps = map.pixelSize, cells = new Map();
  const each = (l, fn) => { const c = l.compressedPixels; if (c?.length) for (let i = 0; i + 2 < c.length; i += 3) for (let k = 0; k < c[i + 2]; k++) fn(c[i] + k, c[i + 1]); else for (let i = 0; i + 1 < (l.pixels || []).length; i += 2) fn(l.pixels[i], l.pixels[i + 1]); };
  let x0 = 1e9, y0 = 1e9, x1 = -1, y1 = -1;
  for (const l of map.layers) each(l, (x, y) => { x0 = Math.min(x0, x); y0 = Math.min(y0, y); x1 = Math.max(x1, x); y1 = Math.max(y1, y); });
  const W = x1 - x0 + 1, H = y1 - y0 + 1, kind = new Uint8Array(W * H), seg = new Int16Array(W * H).fill(-1), segs = [];
  for (const t of ['floor', 'wall', 'segment']) for (const l of map.layers.filter((q) => q.type === t)) {
    const si = t === 'segment' ? segs.push(l.metaData.segmentId) - 1 : -1;
    each(l, (x, y) => { const i = (y - y0) * W + (x - x0); kind[i] = t === 'floor' ? 1 : t === 'wall' ? 2 : 3; if (si >= 0) seg[i] = si; });
  }
  const ent = (t) => map.entities.find((e) => e.type === t)?.points;
  return { ps, x0, y0, W, H, kind, seg, segs, robot: ent('robot_position'), path: map.entities.filter((e) => e.type === 'path').flatMap((e) => e.points) };
}
const STYLES = {  // what common renderers draw (colours from the open-source map parsers, Apache-2.0 / MIT)
  roborock: { k: 4, bg: [19, 87, 148, 255], floor: [32, 115, 185, 255], wall: [93, 109, 126, 255], path: [147, 194, 238, 255], robot: [255, 255, 255, 255],
    room: (id) => ({ 1: [240, 178, 122], 2: [133, 193, 233], 3: [217, 136, 128], 4: [52, 152, 219], 5: [205, 97, 85], 6: [243, 156, 18], 7: [88, 214, 141], 8: [245, 176, 65], 9: [252, 212, 81], 10: [72, 201, 176], 11: [84, 153, 199], 12: [133, 193, 233], 13: [245, 176, 65], 14: [82, 190, 128], 15: [72, 201, 176], 16: [220, 118, 51] })[((+id - 1) % 16) + 1] },
  dreame: { k: 3, bg: [0, 0, 0, 0], floor: [221, 221, 221, 255], wall: [159, 159, 159, 255], path: [255, 255, 255, 255], robot: [34, 109, 242, 255],
    four: [[171, 199, 248], [249, 224, 125], [184, 227, 255], [184, 217, 141]] },
};
export function render(v, style) {
  const S = STYLES[style], k = S.k, W = v.W * k, H = v.H * k, px = new Uint8Array(W * H * 4);
  // room colours: by id (Roborock) or a greedy four-colouring of neighbouring rooms (Dreame / Valetudo style)
  let col = v.segs.map((id) => S.room?.(id));
  if (S.four) {
    const adj = v.segs.map(() => new Set());
    for (let y = 0; y < v.H; y++) for (let x = 0; x < v.W; x++) { const a = v.seg[y * v.W + x]; if (a < 0) continue;
      for (let dy = -3; dy <= 3; dy++) for (let dx = -3; dx <= 3; dx++) { const X = x + dx, Y = y + dy; if (X < 0 || Y < 0 || X >= v.W || Y >= v.H) continue; const b = v.seg[Y * v.W + X]; if (b >= 0 && b !== a) adj[a].add(b); } }
    col = []; v.segs.forEach((_, i) => { const used = new Set([...adj[i]].map((j) => col[j]).filter(Boolean)); col[i] = S.four.find((c) => !used.has(c)) || S.four[i % 4]; });
  }
  const put = (x, y, c) => { if (x < 0 || y < 0 || x >= W || y >= H) return; px.set(c.length === 4 ? c : [...c, 255], (y * W + x) * 4); };
  for (let y = 0; y < v.H; y++) for (let x = 0; x < v.W; x++) {
    const i = y * v.W + x, c = v.kind[i] === 3 ? col[v.seg[i]] : v.kind[i] === 2 ? S.wall : v.kind[i] === 1 ? S.floor : S.bg;
    for (let dy = 0; dy < k; dy++) for (let dx = 0; dx < k; dx++) put(x * k + dx, y * k + dy, c);
  }
  const at = (X, Y) => [((X / v.ps) - v.x0) * k, ((Y / v.ps) - v.y0) * k];  // cm -> picture px (continuous)
  for (let i = 0; i + 3 < v.path.length; i += 2) {  // 1 px path
    const [ax, ay] = at(v.path[i], v.path[i + 1]), [bx, by] = at(v.path[i + 2], v.path[i + 3]), n = Math.ceil(Math.max(Math.abs(bx - ax), Math.abs(by - ay)));
    for (let s = 0; s <= n; s++) put(Math.floor(ax + ((bx - ax) * s) / Math.max(n, 1)), Math.floor(ay + ((by - ay) * s) / Math.max(n, 1)), S.path);
  }
  const [rx, ry] = at(v.robot[0], v.robot[1]), rr = 1.5 * k;  // the robot icon
  for (let y = Math.floor(ry - rr); y <= ry + rr; y++) for (let x = Math.floor(rx - rr); x <= rx + rr; x++) if ((x + 0.5 - rx) ** 2 + (y + 0.5 - ry) ** 2 <= rr * rr) put(x, y, S.robot);
  return { rgba: px, W, H, anchor: { px: [rx, ry], vac: [v.robot[0], v.robot[1]] } };
}
// IoU of each generated room against its exact segment pixels (in the Valetudo pixel grid)
function segIoU(r, v) {
  const t = tf(r), out = {};
  r.rooms.forEach((room) => {
    const si = v.segs.indexOf(room.id); if (si < 0) return;
    let I = 0, U = 0;
    for (let y = 0; y < v.H; y++) for (let x = 0; x < v.W; x++) {
      const X = (x + v.x0 + 0.5) * v.ps, Y = (y + v.y0 + 0.5) * v.ps, a = v.seg[y * v.W + x] === si, b = inside(room.poly, t[0] * X + t[1] * Y + t[2], t[3] * X + t[4] * Y + t[5]);
      I += a && b; U += a || b;
    }
    out[room.name] = I / U;
  });
  return out;
}
// IoU between the rooms of two plans in the same vacuum frame (matched by the best overlap)
function planIoU(a, b) {
  const ta = tf(a), tb = tf(b), res = [];
  const xs = a.rooms.flatMap((q) => q.poly.map((p) => p[0])), zs = a.rooms.flatMap((q) => q.poly.map((p) => p[1]));
  for (const ra of a.rooms) {
    let best = 0;
    for (const rb of b.rooms) {
      let I = 0, U = 0;
      for (let x = Math.min(...xs) + 0.0125; x < Math.max(...xs) + 0.5; x += 0.025) for (let z = Math.min(...zs) + 0.0125; z < Math.max(...zs) + 0.5; z += 0.025) {
        // a point in a's plan -> the vacuum's units -> b's plan (both transforms are axis scalings + offsets)
        const X = (x - ta[2]) / ta[0], Y = (z - ta[5]) / ta[4], xb = tb[0] * X + tb[2], zb = tb[4] * Y + tb[5];
        const p = inside(ra.poly, x, z), q = inside(rb.poly, xb, zb); I += p && q; U += p || q;
      }
      best = Math.max(best, U ? I / U : 0);
    }
    res.push(best);
  }
  return res;
}

async function main() {
  const gen = await loadGen();
  let pass = 0, fail = 0;
  const t = (name, problems, info = '') => {
    if (problems.length) { fail++; console.log(`FAIL ${name}: ${problems.slice(0, 4).join('; ')}`); }
    else { pass++; if (verbose) console.log(`PASS ${name}${info ? ` (${info})` : ''}`); }
  };
  const read = (f) => readFileSync(join(FX, f), 'utf8');
  const MAPS = { roborock_s5: 'Roborock S5', dreame_d9: 'Dreame D9', viomi_v7: 'Viomi V7' };
  for (const [f, brand] of Object.entries(MAPS)) {
    const text = read(`valetudo/${f}.json`), map = JSON.parse(text), v = valetudo(map);
    const r = gen({ adapter: 'valetudo_json' }, { text });
    if (r.error) { t(`valetudo ${brand}`, [r.error]); continue; }
    const p = validate(r), ious = segIoU(r, v), worst = Math.min(...Object.values(ious));
    if (r.rooms.length !== v.segs.length) p.push(`${r.rooms.length} rooms, the map has ${v.segs.length}`);
    for (const l of map.layers.filter((q) => q.type === 'segment' && q.metaData.name)) if (!r.rooms.some((q) => q.name === l.metaData.name)) p.push(`name ${l.metaData.name} lost`);
    const mean = Object.values(ious).reduce((a, b) => a + b, 0) / Object.values(ious).length;
    if (worst < 0.65 || mean < 0.75) p.push(`room IoU vs segments worst ${worst.toFixed(2)}, mean ${mean.toFixed(2)}`);  // straightening an irregular mask and moving sides out to walls cost IoU on purpose; the picture-vs-JSON tests are the tight ones
    if (!r.robot || r.robot[0] < -0.5 || r.robot[1] < -0.5 || r.robot[0] > r.size[0] + 0.5 || r.robot[1] > r.size[1] + 0.5) p.push('robot off the plan');
    t(`valetudo_json ${brand}`, p, `${r.rooms.length} rooms, IoU vs segments ${Object.values(ious).map((x) => x.toFixed(2)).join(' ')}, cuts ${r.cuts.length}`);
    for (const style of Object.keys(STYLES)) {
      const img = render(v, style), ri = gen({ adapter: 'room_colour_image', anchor: { px: img.anchor.px, vac: img.anchor.vac }, flip_y: false, vac_per_m: 100 }, img);
      if (ri.error) { t(`image ${style} ${brand}`, [ri.error]); continue; }
      const q = validate(ri), m = planIoU(r, ri), lo = Math.min(...m);
      if (ri.rooms.length !== r.rooms.length) q.push(`${ri.rooms.length} rooms, the JSON gives ${r.rooms.length}`);
      const avg = m.reduce((a, b) => a + b, 0) / m.length;
      if (lo < 0.8 || avg < 0.9) q.push(`room IoU vs the JSON's plan worst ${lo.toFixed(2)}, mean ${avg.toFixed(2)}`);
      // planIoU maps through the vacuum's units, so a wrong transform fails it; the robot must land on the same spot too
      const rp = toPlan(r, v.robot[0], v.robot[1]), ip = ri.robot || [NaN, NaN], pb = toPlan(ri, img.anchor.vac[0], img.anchor.vac[1]);
      const ra = r.rooms.find((x) => inside(x.poly, rp[0], rp[1])), rb = ri.rooms.find((x) => inside(x.poly, ip[0], ip[1]));
      if (Math.hypot(ip[0] - pb[0], ip[1] - pb[1]) > 0.01) q.push('robot not at its anchor');
      if (!!ra !== !!rb) q.push(`robot in ${ra?.name || 'no room'} by the JSON, ${rb?.name || 'no room'} by the picture`);
      t(`room_colour_image ${style}-style ${brand}`, q, `${ri.rooms.length} rooms, IoU vs JSON ${m.map((x) => x.toFixed(2)).join(' ')}, drawn at ${ri.notes[0]}`);
    }
  }
  { // a real renderer's picture (Xiaomi Cloud Map Extractor sample, colours reused by rooms that don't touch)
    const img = decodePng(readFileSync(join(FX, 'images', 'xiaomi_map_rooms.png')));
    const r = gen({ adapter: 'room_colour_image' }, { rgba: img.px, W: img.W, H: img.H });
    const p = r.error ? [r.error] : validate(r);
    if (!r.error && r.rooms.length !== 9) p.push(`${r.rooms.length} rooms, the picture shows 9`);
    t('room_colour_image Xiaomi extractor sample', p, r.error ? '' : `${r.rooms.length} rooms ${r.rooms.map((q) => q.m2).join(' ')} m2; ${r.notes.slice(0, 2).join('; ')}`);
  }
  { // outlines
    const src = JSON.parse(read('polygons/outlines_mm.json'));
    const r = gen({ adapter: 'polygons' }, { text: JSON.stringify(src) });
    const p = r.error ? [r.error] : validate(r);
    if (!r.error) {
      if (r.rooms.map((q) => q.name).join() !== 'Kitchen,Hall,Lounge,Bathroom') p.push(`rooms ${r.rooms.map((q) => q.name)}`);
      src.rooms.forEach((s, i) => { const want = area(s.points.map(([x, y]) => [x / 1000, y / 1000])), got = r.rooms[i]?.m2 || 0; if (Math.abs(got - want) / want > 0.12) p.push(`${s.name} ${got} m2, outline ${want.toFixed(2)}`); });
      if (r.cuts.some((c) => c.kind === 'spill')) p.push('outlines are never spill-cut');
      const rb = r.robot, lounge = r.rooms[2];
      if (!rb || !inside(lounge.poly, rb[0], rb[1])) p.push('robot not in the lounge');
    }
    t('polygons outlines (mm, y up)', p, r.error ? '' : r.rooms.map((q) => `${q.name} ${q.m2}`).join(', '));
  }
  { // refusals
    const a = gen({ adapter: 'nope' }, {}), b = gen({ adapter: 'valetudo_json' }, { text: '{"pixelSize":5,"layers":[{"type":"floor","compressedPixels":[1,1,5]}],"entities":[]}' });
    t('refusals', [!a.error && 'unknown adapter accepted', !b.error && 'a map without segments accepted'].filter(Boolean));
  }
  console.log(`floorplan: ${pass}/${pass + fail} passed`);
  process.exit(fail ? 1 : 0);
}
if (import.meta.url === pathToFileURL(process.argv[1]).href) main();
