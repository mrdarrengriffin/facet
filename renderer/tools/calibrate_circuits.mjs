#!/usr/bin/env node
// Circuit calibration (Sport foundation, facet-157): the facts the survey outlines (bacinger/f1-circuits) lack, taken
// from the timing-frame circuit data the F1 apps use (MultiViewer's public circuit API, keyed by the OpenF1
// circuit_key): which way the race goes, where the start / finish line is, the marshal sectors (yellow flags are given
// per marshal sector) and the broadcast map rotation. Only these derived facts are kept (tools/circuits_calib.json),
// no coordinates. Run when the calendar changes, then tools/gen_circuits.mjs.
//   node renderer/tools/calibrate_circuits.mjs <dir of bacinger circuits/*.geojson>
// How: the timing path (driving order, from the line) is fitted onto the outline (rotation 0-360 in 0.5 deg + mirror,
// scale refined around the RMS ratio, offset = centroids; mean distance reported), then the line, the direction and
// each marshal sector start are projected onto the outline as lap fractions of the ORIGINAL outline order.
import { readFileSync, writeFileSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const SRC = process.argv[2];
const OUT = join(dirname(fileURLToPath(import.meta.url)), 'circuits_calib.json');
// facet id -> [OpenF1 circuit_short_name, year of the newest race there]
const MAP = {
  'au-1953': ['Melbourne', 2026], 'cn-2004': ['Shanghai', 2026], 'jp-1962': ['Suzuka', 2026], 'us-2022': ['Miami', 2026],
  'ca-1978': ['Montreal', 2026], 'mc-1929': ['Monte Carlo', 2026], 'es-1991': ['Catalunya', 2026], 'at-1969': ['Spielberg', 2026],
  'gb-1948': ['Silverstone', 2026], 'be-1925': ['Spa-Francorchamps', 2026], 'hu-1986': ['Hungaroring', 2026],
  'nl-1948': ['Zandvoort', 2026], 'it-1922': ['Monza', 2026], 'es-2026': ['Madring', 2026], 'az-2016': ['Baku', 2026],
  'my-1999': ['Kuala Lumpur', 2026], 'sg-2008': ['Singapore', 2026], 'us-2012': ['Austin', 2025], 'mx-1962': ['Mexico City', 2025],
  'br-1940': ['Interlagos', 2025], 'us-2023': ['Las Vegas', 2025], 'qa-2004': ['Lusail', 2025], 'ae-2009': ['Yas Marina Circuit', 2025],
};
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const keys = {};
for (const y of [2025, 2026]) {
  for (const s of await (await fetch(`https://api.openf1.org/v1/sessions?year=${y}&session_name=Race`)).json()) keys[`${s.circuit_short_name}|${y}`] = s.circuit_key;
  await sleep(500);
}
const outline = (id) => {
  let c = JSON.parse(readFileSync(join(SRC, `${id}.geojson`), 'utf8')).features[0].geometry.coordinates;
  if (c[0][0] === c.at(-1)[0] && c[0][1] === c.at(-1)[1]) c = c.slice(0, -1);
  const lat0 = c.reduce((s, p) => s + p[1], 0) / c.length * Math.PI / 180;
  const O = c.map(([lo, la]) => [lo * Math.cos(lat0) * 111320, la * 110540]);
  O.push(O[0]);
  const cum = [0];
  for (let i = 1; i < O.length; i++) cum.push(cum[i - 1] + Math.hypot(O[i][0] - O[i - 1][0], O[i][1] - O[i - 1][1]));
  return { O, cum, len: cum.at(-1) };
};
function project(o, [x, y]) {
  let best = [0, Infinity];
  for (let i = 0; i + 1 < o.O.length; i++) {
    const [ax, ay] = o.O[i], [bx, by] = o.O[i + 1], dx = bx - ax, dy = by - ay, L2 = dx * dx + dy * dy || 1;
    const u = Math.max(0, Math.min(1, ((x - ax) * dx + (y - ay) * dy) / L2));
    const d = Math.hypot(ax + u * dx - x, ay + u * dy - y);
    if (d < best[1]) best = [(o.cum[i] + u * Math.sqrt(L2)) / o.len, d];
  }
  return best;
}
const centre = (pts) => {
  const cx = pts.reduce((s, p) => s + p[0], 0) / pts.length, cy = pts.reduce((s, p) => s + p[1], 0) / pts.length;
  return { cx, cy, rms: Math.sqrt(pts.reduce((s, p) => s + (p[0] - cx) ** 2 + (p[1] - cy) ** 2, 0) / pts.length) };
};
const res = {};
for (const [id, [name, year]] of Object.entries(MAP)) {
  const ck = keys[`${name}|${year}`];
  if (!ck) { console.error(`${id}: no OpenF1 circuit key for ${name} ${year}`); continue; }
  let mv = null;
  for (let y = year; y >= 2017; y--) {  // the newest year the API has (a new circuit may be missing in its first year)
    const r = await fetch(`https://api.multiviewer.app/api/v1/circuits/${ck}/${y}`);
    await sleep(300);
    if (r.ok) { mv = await r.json().catch(() => null); if (mv) break; }
  }
  if (!mv || !mv.x) { console.error(`${id}: no timing-frame circuit data (key ${ck}): kept as drawn, no sectors`); continue; }
  const P = mv.x.map((x, i) => [x, mv.y[i]]);
  const o = outline(id), co = centre(o.O.slice(0, -1)), cp = centre(P);
  let fit = null;
  const errOf = (tf, step) => { let e = 0, n = 0; for (let i = 0; i < P.length; i += step) e += project(o, tf(P[i]))[1], n++; return e / n; };
  const make = (deg, m, k) => { const a = deg * Math.PI / 180, c = Math.cos(a), s = Math.sin(a);
    return ([x, y]) => { const u = (x - cp.cx) * m, v = y - cp.cy; return [co.cx + k * (u * c - v * s), co.cy + k * (u * s + v * c)]; }; };
  const k0 = co.rms / cp.rms;
  for (const m of [1, -1]) for (let deg = 0; deg < 360; deg += 2) { const e = errOf(make(deg, m, k0), 12); if (!fit || e < fit.e) fit = { e, deg, m, k: k0 }; }
  for (let deg = fit.deg - 2; deg <= fit.deg + 2; deg += 0.25) for (let k = k0 * 0.95; k <= k0 * 1.05; k += k0 * 0.005) {
    const e = errOf(make(deg, fit.m, k), 6); if (e < fit.e) fit = { ...fit, e, deg, k };
  }
  const tf = make(fit.deg, fit.m, fit.k);
  const err = errOf(tf, 1);
  const start = project(o, tf(P[0]))[0];
  // direction: the path a fifth of a lap on, measured forwards from the line along the outline order
  const ahead = project(o, tf(P[Math.floor(P.length / 5)]))[0];
  const fwd = ((ahead - start) % 1 + 1) % 1;
  const reverse = fwd > 0.5;
  const toRace = (f) => { let d = reverse ? start - f : f - start; return ((d % 1) + 1) % 1; };  // fraction from the line, race order
  const sectors = (mv.marshalSectors || []).map((s) => +toRace(project(o, tf([s.trackPosition.x, s.trackPosition.y]))[0]).toFixed(4)).sort((a, b) => a - b);
  const corners = (mv.corners || []).map((c) => [c.number, +toRace(project(o, tf([c.trackPosition.x, c.trackPosition.y]))[0]).toFixed(4)]);
  // the broadcast view (FastF1 / MultiViewer draw the timing path rotated by mv.rotation, y up, counter-clockwise):
  // timing = R(-fit) M (outline - c) / k, so the outline shown broadcast-way = R(rotation - fit) (mirrored if M is).
  // Checked here: the outline turned that way vs the timing path turned by mv.rotation, both centred + RMS-scaled.
  const bdeg = ((((mv.rotation || 0) - fit.deg * fit.m) % 360) + 360) % 360;
  const rot = (pts, deg) => { const a = deg * Math.PI / 180, c = Math.cos(a), s2 = Math.sin(a); return pts.map(([x, y]) => [x * c - y * s2, x * s2 + y * c]); };
  const norm = (pts) => { const q = centre(pts); return pts.map(([x, y]) => [(x - q.cx) / q.rms, (y - q.cy) / q.rms]); };
  const ob = norm(rot(o.O.slice(0, -1).map(([x, y]) => [x * fit.m, y]), fit.m < 0 ? (mv.rotation || 0) + fit.deg : bdeg));
  const pb = norm(rot(P, mv.rotation || 0));
  let berr = 0;
  for (let i = 0; i < pb.length; i += 4) { let b = Infinity; for (const q of ob) b = Math.min(b, Math.hypot(q[0] - pb[i][0], q[1] - pb[i][1])); berr += b; }
  berr /= Math.ceil(pb.length / 4);
  res[id] = { circuit_key: ck, year, start: +start.toFixed(5), reverse, rotation: Math.round(mv.rotation || 0), fit_deg: fit.deg, mirror: fit.m,
              broadcast: Math.round(bdeg), broadcast_check: +berr.toFixed(3), fit_m: +err.toFixed(1), sectors, corners: corners.length };
  console.error(`${id.padEnd(8)} key ${ck} ${reverse ? 'reversed' : 'as drawn'} start ${start.toFixed(3)} fit ${fit.deg}${fit.m < 0 ? ' mirrored' : ''} ${err.toFixed(1)} m, broadcast ${res[id].broadcast} deg (check ${berr.toFixed(3)} rms), ${sectors.length} marshal sectors`);
}
writeFileSync(OUT, JSON.stringify({ about: 'tools/calibrate_circuits.mjs: race direction, start / finish (fraction of the outline as drawn), marshal sector starts (fractions from the line, race order), broadcast = the rotation (deg, y up, counter-clockwise) that turns the north-up outline into the broadcast map, with its check (mean distance in RMS units between that and the timing path turned by the API rotation; ~0.02 = same picture), fit error (m). Derived facts only.', circuits: res }, null, 1) + '\n');
