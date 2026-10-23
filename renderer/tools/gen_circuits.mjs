#!/usr/bin/env node
// F1 circuits -> compact Facet asset (Sport foundation, facet-157). Source: bacinger/f1-circuits (MIT, (c) 2019-2025
// Tomislav Bacinger), one GeoJSON LineString per circuit (closed loop), re-started on the line and turned the race's way by
// tools/circuits_calib.json (calibrate_circuits.mjs).
// Run once when the calendar changes (the panels' flash holds the result, no network at run time):
//   node renderer/tools/gen_circuits.mjs <dir of the repo's circuits/*.geojson>   -> renderer/core/sport_circuits.cpp
// Each track: lon / lat projected to metres around its own centre (equirectangular: under 0.1 % error over a 2 km
// track), kept north-up, scaled so the longer side = 4095, Douglas-Peucker at 2 units (0.35 px on a 720 px box), then
// packed as 12-bit x / y pairs (3 bytes a point). The renderer fits the aspect into its box.
import { readFileSync, writeFileSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const SRC = process.argv[2];
if (!SRC) { console.error('usage: gen_circuits.mjs <circuits dir>'); process.exit(1); }
const OUT = join(dirname(fileURLToPath(import.meta.url)), '..', 'core', 'sport_circuits.cpp');
// the 2026 calendar, 23 rounds in order (= dataset championships/f1-locations-2026.json = Jolpica /f1/2026/races,
// checked 2026-10-22: no Bahrain / Jeddah, Sepang round 16)
const CAL = [
  ['au-1953', 'Melbourne', 'Australia'], ['cn-2004', 'Shanghai', 'China'], ['jp-1962', 'Suzuka', 'Japan'],
  ['us-2022', 'Miami', 'Miami'], ['ca-1978', 'Montreal', 'Canada'], ['mc-1929', 'Monaco', 'Monaco'],
  ['es-1991', 'Barcelona', 'Barcelona'], ['at-1969', 'Spielberg', 'Austria'], ['gb-1948', 'Silverstone', 'Great Britain'],
  ['be-1925', 'Spa', 'Belgium'], ['hu-1986', 'Budapest', 'Hungary'], ['nl-1948', 'Zandvoort', 'Netherlands'],
  ['it-1922', 'Monza', 'Italy'], ['es-2026', 'Madrid', 'Madrid'], ['az-2016', 'Baku', 'Azerbaijan'],
  ['my-1999', 'Sepang', 'Malaysia'], ['sg-2008', 'Singapore', 'Singapore'], ['us-2012', 'Austin', 'United States'],
  ['mx-1962', 'Mexico City', 'Mexico City'], ['br-1940', 'Sao Paulo', 'Brazil'], ['us-2023', 'Las Vegas', 'Las Vegas'],
  ['qa-2004', 'Lusail', 'Qatar'], ['ae-2009', 'Yas Marina', 'Abu Dhabi'],
];
const Q = 4095, TOL = 2;
// tools/calibrate_circuits.mjs: race direction, the start / finish line, marshal sectors, broadcast rotation
const CALIB = JSON.parse(readFileSync(join(dirname(fileURLToPath(import.meta.url)), 'circuits_calib.json'), 'utf8')).circuits;

function dp(pts, tol) {  // Douglas-Peucker on an open polyline
  const keep = new Uint8Array(pts.length); keep[0] = keep[pts.length - 1] = 1;
  const st = [[0, pts.length - 1]];
  while (st.length) {
    const [a, b] = st.pop(); let m = -1, md = tol;
    const [ax, ay] = pts[a], [bx, by] = pts[b], dx = bx - ax, dy = by - ay, L = Math.hypot(dx, dy) || 1;
    for (let i = a + 1; i < b; i++) {
      const d = Math.abs((pts[i][0] - ax) * dy - (pts[i][1] - ay) * dx) / L;
      if (d > md) md = d, m = i;
    }
    if (m > 0) keep[m] = 1, st.push([a, m], [m, b]);
  }
  return pts.filter((_, i) => keep[i]);
}
const cstr = (s) => JSON.stringify(s).replace(/[^\x20-\x7e]/g, (c) => `\\u${c.charCodeAt(0).toString(16).padStart(4, '0')}`);
const rows = [], data = [], sec = [];
let total = 0, off = 0;
for (const [id, place, gp] of CAL) {
  const f = JSON.parse(readFileSync(join(SRC, `${id}.geojson`), 'utf8')).features[0];
  let c = f.geometry.coordinates;
  if (c.length > 1 && c[0][0] === c.at(-1)[0] && c[0][1] === c.at(-1)[1]) c = c.slice(0, -1);
  // calibrated: start the loop on the line (an exact point on the outline) and run it the way the race goes
  const cal = CALIB[id];
  if (cal) {
    const la = c.reduce((s, p) => s + p[1], 0) / c.length * Math.PI / 180;  // the calibration's metres
    const L = c.map((p, i) => Math.hypot((c[(i + 1) % c.length][0] - p[0]) * Math.cos(la) * 111320, (c[(i + 1) % c.length][1] - p[1]) * 110540));
    const tot = L.reduce((a, b) => a + b, 0);
    let d = cal.start * tot, i = 0;
    while (i < c.length - 1 && d > L[i]) d -= L[i++];
    const u = L[i] ? d / L[i] : 0, a = c[i], b = c[(i + 1) % c.length];
    const sf = [a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u];
    const fwd = [...c.slice(i + 1), ...c.slice(0, i + 1)];  // after the line ... back to the segment before it
    c = cal.reverse ? [sf, ...fwd.reverse()] : [sf, ...fwd];
  }
  const lat0 = c.reduce((s, p) => s + p[1], 0) / c.length * Math.PI / 180, R = 6371008.8;
  const m = c.map(([lo, la]) => [lo * Math.PI / 180 * Math.cos(lat0) * R, -la * Math.PI / 180 * R]);  // y down
  const xs = m.map((p) => p[0]), ys = m.map((p) => p[1]);
  const x0 = Math.min(...xs), y0 = Math.min(...ys), w = Math.max(...xs) - x0, h = Math.max(...ys) - y0, s = Q / Math.max(w, h);
  let q = m.map(([x, y]) => [Math.round((x - x0) * s), Math.round((y - y0) * s)]);
  const n0 = q.length;
  // closed: split at the point farthest from the start, simplify both halves (start / finish stays a vertex)
  let far = 0; q.forEach((p, i) => { if (Math.hypot(p[0] - q[0][0], p[1] - q[0][1]) > Math.hypot(q[far][0] - q[0][0], q[far][1] - q[0][1])) far = i; });
  q = [...dp(q.slice(0, far + 1), TOL).slice(0, -1), ...dp([...q.slice(far), q[0]], TOL).slice(0, -1)];
  for (const [x, y] of q) data.push(x & 0xff, ((x >> 8) & 0x0f) | ((y & 0x0f) << 4), (y >> 4) & 0xff);
  const bytes = q.length * 3;
  const secs = cal ? cal.sectors.map((x) => Math.round(x * 65535)) : [];
  rows.push({ id, name: f.properties.Name, place, gp, len: f.properties.length, w: Math.round(w * s), h: Math.round(h * s), n: q.length, off, n0, bytes, metres: Math.max(w, h),
              rot: cal ? cal.broadcast : 0, cal: !!cal, secs, soff: sec.length });
  sec.push(...secs);
  off += bytes; total += bytes + secs.length * 2;
}
const L = [
  '// GENERATED by tools/gen_circuits.mjs from bacinger/f1-circuits (MIT, (c) 2019-2025 Tomislav Bacinger): rerun it,',
  '// don\'t edit. The 2026 F1 calendar: 12-bit x / y per point (3 bytes: x low 8, x high 4 | y low 4 << 4, y high 8),',
  '// north-up, y down, longer side = 4095; calibrated tracks (tools/circuits_calib.json) start on the line and run the',
  '// way the race goes, with their marshal sector starts (lap fraction x 65535) and the broadcast rotation (deg, y up,',
  '// counter-clockwise, applied to the north-up outline).',
  '#include "sport.h"', '', 'namespace fw::sport {', 'namespace {',
  `const uint8_t PTS[${data.length}] = {`];
for (let i = 0; i < data.length; i += 24) L.push('  ' + data.slice(i, i + 24).join(',') + ',');
L.push('};', `const uint16_t SEC[${Math.max(1, sec.length)}] = {${sec.join(',') || 0}};`, '}  // namespace', `const Circuit CIRCUITS[] = {`);
for (const r of rows) L.push(`  {${cstr(r.id)}, ${cstr(r.name)}, ${cstr(r.place)}, ${cstr(r.gp)}, ${r.len}, ${r.w}, ${r.h}, ${r.n}, PTS + ${r.off}, ${r.rot}, ${r.cal}, ${r.secs.length}, SEC + ${r.soff}},`);
L.push('};', `const int N_CIRCUITS = ${rows.length};`, `const int CIRCUIT_BYTES = ${total};  // points only; + ${rows.length} x sizeof(Circuit) table`, '}  // namespace fw::sport', '');
writeFileSync(OUT, L.join('\n'));
for (const r of rows) console.log(`${r.id.padEnd(8)} ${String(r.n0).padStart(4)} -> ${String(r.n).padStart(4)} pts ${String(r.bytes + r.secs.length * 2).padStart(5)} B  ${r.cal ? '' : '(uncalibrated) '}${r.gp}`);
console.log(`total ${total} B (points + sectors), ${rows.length} circuits, avg ${Math.round(total / rows.length)} B`);
