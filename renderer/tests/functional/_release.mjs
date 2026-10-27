// Release-gate helpers (notes: the Facet release gate, sections 1-6 + 10), shared by every component's and screen's
// functional tests. Not a test file. Everything runs on the WASM build with Home Assistant faked (tools/functional.mjs
// FakeHA); each helper returns a list of failure strings (empty = pass) so a test can t.eq(list, [], 'what').
//   fixtures   names that break layouts: very long, accents, emoji, CJK, right-to-left, empty
//   seeded     a small seeded PRNG (mulberry32): a fuzz / monkey failure is reproducible from its seed
//   bounds     the contract bounds proof (fw_test bounds) at several page insets (fx::inset) -> ["inset 16: ..."]
//   stage      resize the stage (a parent of any size); boundsAll = the bounds proof of a component mounted alone
//   look       a LIVE theme / accent switch through the shared styles (fw_test look), no rebuild
//   frame565   the stage as RGB565 words (what the panel shows); diff565 = differing pixels
//   golden     compare the stage (minus mask boxes: the live clock) with tests/golden/<suite>/<den>-<name>.565.gz; a missing or changed golden FAILS and
//              writes <name>.new.565.gz + .new.png beside it for review: never auto-accepted (FW_GOLDEN_ACCEPT=1 on a
//              run accepts the .new files after a person has looked at the PNGs)
//   faults     HA fault injection on a FakeHA: missing entity, wrong types, out of range, out-of-order echoes
//   fuzz       n random cases of (props / HA states) -> no crash, no abort, bounds proof clean
//   monkey     n random taps / holds / drags anywhere on the stage -> no crash, still responds
//   leak       open / close cycles: objects back to the baseline, heap growth under a budget
//   frames     the bench record of a gesture: renders, ms per frame (WASM), dirty px, areas
import { existsSync, mkdirSync, readFileSync, writeFileSync, renameSync } from 'node:fs';
import { gzipSync, gunzipSync } from 'node:zlib';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { png } from '../../tools/lib/fwbench.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
export const GOLDEN = join(HERE, '..', 'golden');

export const fixtures = {
  long: 'Living room ceiling light above the big dining table by the window',
  accents: 'Küche Decke Ålesund Œuvre',
  emoji: 'Lamp 💡 by the bed',
  cjk: '客厅灯',
  rtl: 'מנורה בסלון',
  empty: '',
};

export function seeded(seed) {
  let a = seed >>> 0;
  const r = () => { a = (a + 0x6d2b79f5) >>> 0; let t = a; t = Math.imul(t ^ (t >>> 15), t | 1); t ^= t + Math.imul(t ^ (t >>> 7), t | 61); return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
  r.int = (lo, hi) => lo + Math.floor(r() * (hi - lo + 1));
  r.pick = (xs) => xs[Math.floor(r() * xs.length)];
  return r;
}

const test = (t, cmd, arg = '') => t.b.str(cmd, (c) => t.b.str(String(arg), (a) => t.b.m.UTF8ToString(t.b.m._fw_test(c, a))));

export function bounds(t, insets = [null]) {
  const out = [];
  for (const i of insets) {
    if (i !== null) test(t, 'inset', i);
    t.b.tick(2);
    for (const e of JSON.parse(test(t, 'bounds') || '[]')) out.push(`${i === null ? 'token inset' : `inset ${i}`}: ${e}`);
  }
  if (insets.some((i) => i !== null)) { test(t, 'inset', -1); t.b.tick(2); }
  return out;
}

// the stage resized to w x h (a component's parent at any size; frame565 / golden follow); null = back to S x S
export function stage(t, w, h) {
  const b = t.b; b.W = w ?? undefined; b.H = h ?? undefined;
  b.m._fw_stage_resize(b.stage, w ?? b.S, h ?? b.S); b.tick(3);
}
export function boundsAll(t) {  // the bounds proof of a component mounted alone (its parent = the stage's content box)
  t.b.tick(2);
  return JSON.parse(t.b.str('bounds', (c) => t.b.str('all', (a) => t.b.m.UTF8ToString(t.b.m._fw_test(c, a)))) || '[]');
}
export function look(t, light, accent = -1) { test(t, 'look', `${light ? 1 : 0},${accent}`); t.b.tick(2); }

export function frame565(t) {
  const b = t.b; b.m._fw_refresh(b.stage);
  const p = b.m._fw_frame(b.stage), h = b.m.HEAPU8, n = (b.W || b.S) * (b.H || b.S), w = new Uint16Array(n);  // W x H: a resized stage (stage())
  for (let i = 0; i < n; i++) { const q = p + i * 4; w[i] = ((h[q] >> 3) << 11) | ((h[q + 1] >> 2) << 5) | (h[q + 2] >> 3); }
  return w;
}
export const diff565 = (a, b) => { let n = 0; for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) n++; return n; };
const rgba = (w) => { const px = new Uint8Array(w.length * 4); w.forEach((v, i) => { px[i * 4] = (v >> 11) << 3; px[i * 4 + 1] = ((v >> 5) & 63) << 2; px[i * 4 + 2] = (v & 31) << 3; px[i * 4 + 3] = 255; }); return px; };

// mask = boxes {x, y, w, h} blanked first (what changes on its own: the live clock)
export function golden(t, suite, name, den, mask = []) {
  const dir = join(GOLDEN, suite), base = join(dir, `${den}-${name}`), cur = frame565(t);
  const W = t.b.W || t.b.S, H = t.b.H || t.b.S;
  for (const m of mask) for (let y = Math.max(0, m.y); y < Math.min(H, m.y + m.h); y++) for (let x = Math.max(0, m.x); x < Math.min(W, m.x + m.w); x++) cur[y * W + x] = 0;
  mkdirSync(dir, { recursive: true });
  const save = (p) => { writeFileSync(`${p}.565.gz`, gzipSync(Buffer.from(cur.buffer))); writeFileSync(`${p}.png`, png(rgba(cur), W, H)); };
  if (process.env.FW_GOLDEN_ACCEPT === '1' && existsSync(`${base}.new.565.gz`)) {
    renameSync(`${base}.new.565.gz`, `${base}.565.gz`); renameSync(`${base}.new.png`, `${base}.png`);
  }
  if (!existsSync(`${base}.565.gz`)) { save(`${base}.new`); return [`no golden ${suite}/${den}-${name}: review ${base}.new.png, then accept (FW_GOLDEN_ACCEPT=1)`]; }
  const want = new Uint16Array(new Uint8Array(gunzipSync(readFileSync(`${base}.565.gz`))).buffer);
  const n = want.length === cur.length ? diff565(want, cur) : -1;
  if (n === 0) return [];
  save(`${base}.new`);
  return [`golden ${suite}/${den}-${name}: ${n < 0 ? 'size changed' : `${n} px differ`} (review ${base}.new.png)`];
}

// HA faults on the fake HA (t.ha): each returns what was injected (for the log)
export const faults = {
  missing: (t, e) => t.ha.set(e, ''),                         // the host's word for "HA has no such entity"
  unavailable: (t, e) => t.ha.set(e, 'unavailable'),
  unknown: (t, e) => t.ha.set(e, 'unknown'),
  wrongType: (t, e, a) => t.ha.set(e, undefined, { [a]: 'abc' }),
  outOfRange: (t, e, a, v) => t.ha.set(e, undefined, { [a]: v }),
  dropped: (t, e, a) => t.ha.set(e, undefined, { [a]: null }),
  // HA answers out of order: the newer value first, then a stale one (a slow echo of an earlier call)
  outOfOrder: (t, e, attrs, stale) => { t.ha.set(e, undefined, attrs); t.ha.set(e, undefined, stale); },
};

// n random cases: build(r, i) mounts / changes something (async ok), then the stage must still answer and pass bounds
export async function fuzz(t, n, seed, build, { insets = [null] } = {}) {
  const r = seeded(seed), out = [];
  for (let i = 0; i < n && out.length < 5; i++) {
    try { await build(r, i); t.b.settle(800); const bad = bounds(t, insets); if (bad.length) out.push(`case ${i} (seed ${seed}): ${bad[0]}`); t.objs(); }
    catch (e) { out.push(`case ${i} (seed ${seed}): ${e.message}`); }
  }
  return out;
}

// n random gestures anywhere on the stage (taps, long presses, drags, flicks, drags off the edge); after each the
// stage must still render and answer; alive(t) may check more (the device stack is sane)
export function monkey(t, n, seed, alive = () => null) {
  const r = seeded(seed), S = t.b.S, out = [];
  for (let i = 0; i < n && out.length < 5; i++) {
    const x = r.int(0, S - 1), y = r.int(0, S - 1), k = r.int(0, 9);
    try {
      let ev;
      if (k < 4) ev = t.b.tap(x, y);
      else if (k < 5) ev = t.b.tap(x, y, t.b.long_ms + 100);
      else ev = t.b.drag(x, y, r.int(-S, S) * (k === 9 ? 2 : 1), r.int(-S, S), { steps: k === 8 ? 2 : 0, after: false });
      t.ha.react(ev); t.ha.flush(); t.b.tick(r.int(1, 30));
      t.objs();
      const why = alive(t);
      if (why) out.push(`gesture ${i} (seed ${seed}): ${why}`);
    } catch (e) { out.push(`gesture ${i} (seed ${seed}): ${e.message}`); }
  }
  t.b.settle(2000);
  return out;
}

export const mem = (t) => { const a = new Float64Array(t.b.m.HEAPU8.buffer, t.b.m._fw_bench_mem(), 4); return { heap: a[0], objs: a[2], anims: a[3] }; };
// cycles x (open(); close()): objects back to the baseline, heap within budget bytes of it
export function leak(t, cycles, open, close, budget = 64 * 1024) {
  close(); t.b.settle(1500);
  open(); close(); t.b.settle(1500);  // warm up (fonts, caches, first allocations)
  const m0 = mem(t);
  for (let i = 0; i < cycles; i++) { open(); close(); }
  t.b.settle(3000);
  const m1 = mem(t), out = [];
  if (m1.objs !== m0.objs) out.push(`objects ${m0.objs} -> ${m1.objs} after ${cycles} cycles`);
  if (m1.heap - m0.heap > budget) out.push(`heap +${Math.round((m1.heap - m0.heap) / 1024)} KB after ${cycles} cycles (budget ${budget / 1024} KB)`);
  if (m1.anims) out.push(`${m1.anims} animations still running`);
  return out;
}

// the bench record of fn(): per rendered frame {ms (WASM work), px (dirty), areas, full}
export function frames(t, fn) {
  t.b.rec = []; fn(); const rec = t.b.rec.filter((r) => r.renders); t.b.rec = null;
  const ms = rec.map((r) => r.work).sort((a, b) => a - b), px = rec.map((r) => r.px);
  const q = (xs, f) => (xs.length ? xs[Math.min(xs.length - 1, Math.floor(xs.length * f))] : 0);
  return { n: rec.length, ms50: q(ms, 0.5), ms95: q(ms, 0.95), px50: q([...px].sort((a, b) => a - b), 0.5), pxMax: Math.max(0, ...px), full: rec.filter((r) => r.full).length };
}

// P4 for a component mounted alone at w x h: built in one mode then switched LIVE (fw_test look, accents 0 and 2)
// must equal a fresh build in the other mode, every RGB565 pixel (a colour held from build time shows here).
// set(t) runs after each build (press a state, wait for a fade). Returns failures.
export async function liveLook(t, comp, props, w, h, set = () => {}) {
  const out = [];
  const build = async (light, acc) => {
    t.b.W = t.b.H = undefined; await t.b.render(comp, props, light);
    t.b.str(comp, (c) => t.b.str(JSON.stringify(props), (p) => t.b.m._fw_render(t.b.stage, c, p, light, acc)));
    stage(t, w, h); t.b.settle(800); set(t); t.b.settle(800);
  };
  for (const [a, b] of [[0, 1], [1, 0]]) for (const acc of [0, 2]) {
    await build(a, 0); look(t, b, acc); t.b.settle(800); const live = frame565(t);
    await build(b, acc); const n = diff565(live, frame565(t));
    if (n) out.push(`${comp} ${JSON.stringify(props).slice(0, 60)} ${a ? 'light' : 'dark'} -> ${b ? 'light' : 'dark'} live, accent ${acc}: ${n} px differ from a fresh build`);
  }
  return out;
}
// the Layout lab on the mounted stage: its token list, and a tune (px < 0 = back to the token)
export const lab = (t, cmd, arg = '') => t.b.str(cmd, (c) => t.b.str(arg, (a) => t.b.m.UTF8ToString(t.b.m._fw_lab(t.b.stage, c, a))));
export const tune = (t, name, px) => { lab(t, 'tune', `${name}=${px < 0 ? '' : px}`); t.b.tick(3); };
