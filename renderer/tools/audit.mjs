#!/usr/bin/env node
// Facet component audit (Darren 2026-10-22: "tests against them to make sure they align with the pixel grids and
// there's nothing off about them. The components don't clip out of their box and it's not cut off ... tests on
// animations to make sure that they follow the right path"). README "Audit". Runs on the WASM build, headless, over
// every registered component and screen x every variant x theme (dark, light) for one density, and checks:
//   outside   no ink outside the component's own box (+ the marks it bleeds on purpose: fw::bleed / ext draw size);
//             no visible part at least half off the stage (it would draw where no pixel is checked)
//   clip      nothing is cut off: the same render with every object allowed to draw outside its box (fw_unclip) must
//             give the same pixels; whatever differs was clipped (an ancestor's box, a custom draw's own box). Objects
//             marked fw::qa::clips(obj, why) and scroll containers keep their clip
//   text      no label's glyphs fall below its box (LVGL cuts label text at the box bottom); a CLIP label wider than
//             its text box
//   grid      every drawn box (fill or border) has its position (from the component's top-left) and size on the 4 px
//             grid; objects placed by value or angle say so with fw::qa::free(obj, why) (reported as notes)
//   touch     every object that takes touches is at least touch::min (48 / 32) with its ext click area
//   radial    meta "audit": {"radial": true}: the marks at 12, 3, 6 and 9 o'clock (ink in the outer band of the
//             component's circle) sit on the centre lines within 0.5 px
//   stale     the frame after the animations equals a full redraw at that moment (nothing left behind)
//   update    a state / value prop changed through the update path animates (3+ frames), silently
//   motion    while it animates (the first 2 s from the build, every 2nd frame): no ink leaves the box; an object
//             marked fw::qa::circle(obj, ref, cx, cy, r) keeps its centre on that circle (1 px), fw::qa::inside(obj,
//             ref) stays inside ref
//   layout    screens only (tools/audit_screens.mjs checkScreen, at the build and after the motion loop): the screen
//             root's regions don't overlap and nothing escapes its region or the screen; overlays (a bottom sheet,
//             a toast layer) say so with fw::qa::free(obj, "overlay: <why>")
// Exceptions: meta "audit": {"skip": {"<check>": "why"}}, or the same on one variant (printed as notes). Failures are errors (exit 1).
//   node tools/audit.mjs [--den 1x|1.5x|all] [--comp id,id] [--props '{"fades":true}'] [--build <dir>] [--json out.json] [--quiet] [--all-variants]
// Cache: results per component are kept while the inputs are unchanged (core + the charts library foundations/chart.cpp,
// tokens, this tool, the component's file and the files of components it builds with fw::find or names in a
// "// audit deps: a, b" comment) in $FW_CACHE/audit-<den>.json, so a build re-audits only what
// changed. --fresh ignores it.
import { readFileSync, writeFileSync, existsSync, readdirSync } from 'node:fs';
import { dirname, join, resolve, basename } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { createHash } from 'node:crypto';
import { checkScreen } from './audit_screens.mjs';

const HERE = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const ROOT = resolve(HERE, '..');
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const flag = (k) => process.argv.includes(k);
const env = (() => { try { return Object.fromEntries(readFileSync(join(ROOT, 'local.env'), 'utf8').split('\n').map((l) => l.match(/^([A-Z_]+)=(.*)$/)).filter(Boolean).map((m) => [m[1], m[2]])); } catch { return {}; } })();
const BUILD = arg('--build', process.env.FW_OUT || env.FW_OUT || join(HERE, 'out'));
const CACHE = process.env.FW_CACHE || env.FW_CACHE || join(HERE, '.cache');
const dens = arg('--den', 'all') === 'all' ? ['1x', '1.5x'] : [arg('--den')];
const only = arg('--comp') ? arg('--comp').split(',') : null;
const quiet = flag('--quiet');
const EXTRA = JSON.parse(arg('--props', '{}'));  // merged into every case (e.g. {"fades": true}); disables the cache
const TICK = 16;

// ---------------------------------------------------------------- inputs hash (cache key per component)
const sha = (s) => createHash('sha1').update(s).digest('hex').slice(0, 12);
const ls = (d, ext) => (existsSync(join(HERE, d)) ? readdirSync(join(HERE, d)).filter((f) => f.endsWith(ext)).sort().map((f) => join(d, f)) : []);
const read = (f) => readFileSync(join(HERE, f));
const coreHash = sha(Buffer.concat([...ls('core', '.h'), ...ls('core', '.cpp'), 'foundations/chart.cpp', ...ls('tokens', '.json'), 'tools/audit.mjs', 'tools/audit_screens.mjs'].flatMap((f) => [Buffer.from(f), read(f)])));
const srcOf = {};
for (const d of ['components', 'screens', 'foundations']) for (const f of ls(d, '.cpp')) srcOf[basename(f, '.cpp')] = f;
const deps = (id, seen = new Set()) => {
  if (seen.has(id) || !srcOf[id]) return seen;
  seen.add(id);
  const src = read(srcOf[id]).toString();
  for (const m of src.matchAll(/fw::find\("([a-z_0-9]+)"\)/g)) deps(m[1], seen);
  // a file that calls another component's code directly (not through fw::find) says so: "// audit deps: a, b"
  for (const m of src.matchAll(/\/\/ audit deps: ([a-z_0-9, ]+)/g)) for (const d of m[1].split(/[, ]+/)) if (d) deps(d, seen);
  // a screen built from the factory specs (spec::build: the Spec screen, Device, Home) depends on every component the
  // specs name (2026-10-23: the Calendar spec kept a stale audit after calendar_view changed)
  if (/spec::build\(|spec::FACTORY/.test(src)) for (const c of SPEC_COMPS) deps(c, seen);
  return seen;
};
const SPEC_FILES = ls('specs', '.json');
const SPEC_COMPS = [...new Set(SPEC_FILES.flatMap((f) => [...read(f).toString().matchAll(/"c":\s*"([a-z_0-9]+)"/g)].map((m) => m[1])))];
const specHash = sha(Buffer.concat(SPEC_FILES.flatMap((f) => [Buffer.from(f), read(f)])));
const compHash = (id) => sha(coreHash + specHash + [...deps(id)].sort().map((d) => d + sha(read(srcOf[d]))).join());

// ---------------------------------------------------------------- the module
async function load(den) {
  const dir = join(BUILD, den);
  const F = (await import(pathToFileURL(join(dir, 'fw.mjs')).href)).default;
  const wasm = await WebAssembly.compile(readFileSync(join(dir, 'fw.wasm')));
  const m = await F({ instantiateWasm: (imports, ok) => { WebAssembly.instantiate(wasm, imports).then((i) => ok(i, wasm)); return {}; } });
  if (!m._fw_audit) throw new Error(`${dir}: this build has no fw_audit (core/main.cpp)`);
  m._fw_init();
  return m;
}
const str = (m, s, f) => { const p = m.stringToNewUTF8(s); try { return f(p); } finally { m._free(p); } };

// ---------------------------------------------------------------- one case
function runCase(m, den, c, props, light, vskip = {}) {
  const D = den === '1x' ? 1 : 1.5, S = den === '1x' ? 480 : 720, TOUCH = den === '1x' ? 32 : 48;
  const screen = c.section === 'screens';
  const [fw, fh] = c.meta.frame || [S / D, S / D];
  let W = screen ? S : Math.round(fw * D), H = screen ? S : (fh ? Math.round(fh * D) : S);
  const errs = [], notes = [];
  const skip = { ...((c.meta.audit && c.meta.audit.skip) || {}), ...vskip };  // component, then variant exceptions
  const A = m._fw_stage(W, H);
  let t = Math.round(m.__t || 0);
  const tick = (n = 1) => { for (let i = 0; i < n; i++) { t += TICK; m._fw_tick(t); } m.__t = t; m._fw_events(); };
  tick(2);
  const pj = JSON.stringify(props);
  for (const id of [A]) {
    const e = str(m, c.id, (cp) => str(m, pj, (pp) => m.UTF8ToString(m._fw_render(id, cp, pp, light ? 1 : 0, 0))));
    if (e) { errs.push(`render: ${e}`); m._fw_stage_free(A); return { errs, notes }; }
  }
  if (!fh && !screen) {  // auto height: as the workbench sizes it
    const ch = m._fw_content_h(A);
    if (ch > H) { H = ch; m._fw_stage_resize(A, W, H); }
  }
  const objs = () => JSON.parse(m.UTF8ToString(m._fw_audit(A)));
  // ---- screens: the layout check (tools/audit_screens.mjs: regions overlap, objects escape) at the build and after
  // the motion loop (live screens move); overlays opt out with fw::qa::free(obj, "overlay: <why>")
  const screenErrs = new Set();
  if (screen) for (const e of checkScreen(objs(), W, H)) screenErrs.add(e);
  const frame = (id) => new Uint32Array(m.HEAPU8.buffer.slice(m._fw_frame(id), m._fw_frame(id) + W * H * 4));
  // ---- motion: the first 2 s from the build, every 2nd frame
  const roots0 = objs().filter((o) => o.p < 0);
  const allowed = (os) => os.filter((o) => o.p < 0).map((o) => [o.x - o.ext, o.y - o.ext, o.x + o.w - 1 + o.ext, o.y + o.h - 1 + o.ext]);
  const inAllowed = (al, x, y) => screen || al.some(([a, b, c2, d]) => x >= a && x <= c2 && y >= b && y <= d);
  let bgc = null;
  const outside = (px, al) => {
    let n = 0, bb = [W, H, -1, -1];
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
      if (px[y * W + x] === bgc || inAllowed(al, x, y)) continue;
      n++; bb = [Math.min(bb[0], x), Math.min(bb[1], y), Math.max(bb[2], x), Math.max(bb[3], y)];
    }
    return n ? { n, bb } : null;
  };
  const motionErrs = new Set();
  for (let k = 0; k < 62; k++) {
    tick(2);
    if (!m._fw_anims() && k > 2) break;
    const os = objs();
    for (const o of os) {
      if (!o.path) continue;
      const cx = o.x + o.w / 2, cy = o.y + o.h / 2;
      if (o.path.kind === 'circle' && !skip.motion) {
        const d = Math.hypot(cx - o.path.cx, cy - o.path.cy) - o.path.r;
        if (Math.abs(d) > 1) motionErrs.add(`motion: an object (${o.w}x${o.h}) leaves its circle by ${d.toFixed(1)} px (centre ${cx.toFixed(1)}, ${cy.toFixed(1)}; circle about ${o.path.cx}, ${o.path.cy} r ${o.path.r})`);
      }
      if (o.path.rw && !skip.motion && (o.x < o.path.rx || o.y < o.path.ry || o.x + o.w > o.path.rx + o.path.rw || o.y + o.h > o.path.ry + o.path.rh))
        motionErrs.add(`motion: an object (${o.w}x${o.h}) leaves its box while it moves`);
    }
    if (!screen && !skip.outside && m._fw_anims()) {
      const px = frame(A);
      bgc ??= px[0];
      const o = outside(px, allowed(os));
      if (o) motionErrs.add(`motion: ink outside the box while it animates (${o.n} px, ${o.bb.join(',')})`);
    }
  }
  errs.push(...motionErrs);
  if (screen) { for (const e of checkScreen(objs(), W, H)) screenErrs.add(e); errs.push(...screenErrs); }
  tick(60);  // settle (1 s more)
  m._fw_refresh(A);  // what timers changed after the last refresh, drawn now (no time passes)
  const os = objs(), roots = os.filter((o) => o.p < 0);
  const pxA = frame(A);
  bgc = pxA[0];
  // ---- outside
  if (!screen && !skip.outside) {
    const o = outside(pxA, allowed(os));
    if (o) errs.push(`outside: ${o.n} px of ink outside the component's box (${o.bb.join(',')})`);
    // parts placed off the stage draw where no pixel can be checked (2026-10-22: two card-preview mascots at x 72 of a
    // 72 box, read as width 0 before layout, their ancestors overflow-visible for the mascot's bleed): a visible object
    // at least half outside the stage, with nothing above it clipping, is an error
    for (const o of os) {
      if (o.p < 0 || o.opa === 0 || !o.w || !o.h) continue;
      let k = o.p; while (k >= 0 && os[k].ov && !os[k].sc) k = os[k].p;
      if (k >= 0) continue;  // an ancestor clips it (or scrolls it out of view): it cannot draw off the stage
      const vis = Math.max(0, Math.min(o.x + o.w, W) - Math.max(o.x, 0)) * Math.max(0, Math.min(o.y + o.h, H) - Math.max(o.y, 0));
      if (vis * 2 < o.w * o.h) { errs.push(`outside: a ${o.t || 'obj'} ${o.w}x${o.h} at ${o.x},${o.y} is ${vis ? 'mostly' : 'wholly'} off the ${W}x${H} stage`); break; }
    }
  } else if (skip.outside) notes.push(`outside skipped: ${skip.outside}`);
  // ---- stale: what the partial redraws left on screen must equal a full redraw at the same moment (an animation that
  // invalidates too little leaves old pixels behind)
  {
    m._fw_redraw(A);
    const pxF = frame(A);
    let n = 0, bb = [W, H, -1, -1];
    for (let i = 0; i < W * H; i++) if (pxA[i] !== pxF[i]) { n++; const x = i % W, y = (i / W) | 0; bb = [Math.min(bb[0], x), Math.min(bb[1], y), Math.max(bb[2], x), Math.max(bb[3], y)]; }
    if (n && !skip.stale) errs.push(`stale: ${n} px left behind by a partial redraw (${bb.join(',')})`);
    pxA.set(pxF);  // the clip check compares against the full redraw
  }
  // ---- clip: the same render, nothing clipped
  if (!skip.clip) {
    m._fw_unclip(A);  // the same stage, the same moment (fw_unclip redraws at once): a looping animation can't differ
    const pxB = frame(A), pxA2 = pxA;
    let n = 0, bb = [W, H, -1, -1];
    for (let i = 0; i < W * H; i++) if (pxA2[i] !== pxB[i]) { n++; const x = i % W, y = (i / W) | 0; bb = [Math.min(bb[0], x), Math.min(bb[1], y), Math.max(bb[2], x), Math.max(bb[3], y)]; }
    if (n) {
      // name it: the deepest object whose box the clipped ink touches
      const hit = os.filter((o) => o.x <= bb[2] + 1 && o.x + o.w >= bb[0] - 1 && o.y <= bb[3] + 1 && o.y + o.h >= bb[1] - 1).sort((a, b) => b.d - a.d)[0];
      errs.push(`clip: ${n} px cut off (${bb.join(',')})${hit ? ` near a ${hit.t} ${hit.w}x${hit.h} at ${hit.x},${hit.y}${hit.text ? ` "${hit.text}"` : ''}` : ''}`);
    }
  } else notes.push(`clip skipped: ${skip.clip}`);
  // ---- text
  for (const o of os) {
    if (o.cut && !skip.text) errs.push(`text: "${o.text}" is cut ${o.cut} px at the bottom of its box (${o.w}x${o.h} at ${o.x},${o.y})`);
    if (o.cutr && !skip.text) errs.push(`text: "${o.text}" is ${o.cutr} px wider than its box`);
  }
  // ---- grid: each drawn box on the 4 px grid relative to its parent (Darren: "align with the pixel grids"). Allowed
  // as alignment: a box centred in its parent on that axis (odd parity can't be both), a box filling its parent's
  // inside of the border, a hairline (1-2 px) on its thin axis. Placed by value / angle: fw::qa::free (a note).
  if (!skip.grid) {
    for (const o of os) {
      if (o.t === 'label' || o.t === 'image' || o.t === 'line') continue;
      if (!o.box && o.t !== 'arc' && o.p >= 0) continue;
      if (o.free) { notes.push(`grid: ${o.w}x${o.h} off the grid on purpose: ${o.free}`); continue; }
      const P = o.p >= 0 ? os[o.p] : null;
      const axis = (pos, size, ppos, psize, pb) => {
        if (size <= 2) return [];                                   // hairline: follows the box it lines
        const d = pos - ppos, bad = [];
        const centred = P && Math.abs((d) - (psize - d - size)) <= 1;
        const inset = P && pb > 0 && d === pb && size === psize - 2 * pb;  // fills the inside of the parent's border
        // edge to edge with its own border pushed off both sides (the Bottom sheet: its rounded top edge shows, its
        // sides fall off the screen; Darren 2026-10-22, a top-only border drew square corners)
        const bleedOwn = P && o.bw > 0 && o.bw <= 2 && d === -o.bw && size === psize + 2 * o.bw;
        if (d % 4 && !centred && !inset && !bleedOwn) bad.push(d);
        if (size % 4 && !inset && !bleedOwn) bad.push(`size ${size}`);
        return bad;
      };
      const bx = axis(o.x, o.w, P ? P.x : o.x, P ? P.w : o.w, P ? P.bw : 0), by = axis(o.y, o.h, P ? P.y : o.y, P ? P.h : o.h, P ? P.bw : 0);
      if (bx.length || by.length) errs.push(`grid: a ${o.t === 'arc' ? 'arc' : 'box'} ${o.w}x${o.h} at ${o.x - (P ? P.x : o.x)},${o.y - (P ? P.y : o.y)} in its parent is off the 4 px grid (${[...bx.map((v) => `x ${v}`), ...by.map((v) => `y ${v}`)].join(', ')})`);
    }
  } else notes.push(`grid skipped: ${skip.grid}`);
  // ---- touch
  for (const o of os) {
    if (!o.clk || skip.touch) continue;
    const w = o.w + 2 * o.pad, h = o.h + 2 * o.pad;
    if (w < TOUCH || h < TOUCH) errs.push(`touch: a target ${w}x${h} (box ${o.w}x${o.h}) is under touch::min ${TOUCH}`);
  }
  // ---- radial: a ring's marks at 12 / 3 / 6 / 9 o'clock (fw::qa::radial: fw::tick_ring declares which it has) sit on
  // the ring's centre lines: the ink-weighted centre of each mark, across its axis, within 0.5 px
  // (0.52: a 1 px hairline on an even ring sits half a pixel off by nature; anything more is a placement error)
  for (const r of os) {
    if (!r.axes || skip.radial) continue;
    const cx = r.x + r.w / 2, cy = r.y + r.h / 2, R = Math.min(r.w, r.h) / 2, half = 3;
    for (const [bit, deg, name] of [[1, 270, '12'], [2, 0, '3'], [4, 90, '6'], [8, 180, '9']]) {
      if (!(r.axes & bit)) continue;
      const ux = Math.cos((deg * Math.PI) / 180), uy = Math.sin((deg * Math.PI) / 180);
      let sw = 0, sp = 0, lo = 1e9, hi = -1e9;
      for (let y = Math.floor(cy - R); y < cy + R; y++) for (let x = Math.floor(cx - R); x < cx + R; x++) {
        if (x < 0 || y < 0 || x >= W || y >= H) continue;
        const v = pxA[y * W + x];
        if (v === bgc) continue;
        const px = x + 0.5 - cx, py = y + 0.5 - cy, along = px * ux + py * uy, perp = -px * uy + py * ux;
        if (along < R - r.depth - 0.5 || along > R + 1 || Math.abs(perp) > half) continue;
        const wgt = Math.abs(((v & 255) - (bgc & 255))) + Math.abs(((v >> 8) & 255) - ((bgc >> 8) & 255)) + Math.abs(((v >> 16) & 255) - ((bgc >> 16) & 255));
        sw += wgt; sp += wgt * perp; lo = Math.min(lo, along); hi = Math.max(hi, along);
      }
      if (!sw) errs.push(`radial: the ${name} o'clock mark is missing`);
      else if (Math.abs(sp / sw) > 0.52) errs.push(`radial: the ${name} o'clock mark sits ${(sp / sw).toFixed(2)} px off its centre line (ring ${r.w} at ${r.x},${r.y})`);
      else if (hi > R + 0.01) errs.push(`radial: the ${name} o'clock mark pokes ${(hi - R).toFixed(1)} px past the ring's edge`);
    }
  }
  m._fw_stage_free(A);
  return { errs, notes };
}

// ---- update animates (Darren 2026-10-22: "if something gets updated from Home Assistant, and it is like a toggle, I
// want to make sure it animates like as if I've pressed it. Not just flicks on instantly"): for every state / value prop
// (by name, below), an update from the defaults to a changed value must show an animated sequence (at least 3
// distinct frames in the second after it), not a single-frame jump. Declared exceptions: meta "audit": {"snaps":
// {"<prop>": "why"}} (notes); meta "audit": {"base": {...}} = props the update test starts from (a List row as a
// toggle row).
const STATE_PROPS = new Set(['on', 'value', 'progress', 'position', 'tilt', 'target', 'current', 'check', 'selected', 'tone',
  'lit', 'pct', 'count', 'playing', 'accent', 'open', 'a', 'b', 'c', 'kind', 'phase', 'n', 'status', 'action']);
function changed(p, v) {
  if (p.type === 'bool') return !v;
  if (p.type === 'range') { const lo = p.min ?? 0, hi = p.max ?? 100, mid = (lo + hi) / 2; return v >= mid ? lo + (hi - lo) * 0.2 : lo + (hi - lo) * 0.8; }
  if (p.type === 'select') { const o = p.options || []; return o[(o.indexOf(v) + 1) % o.length]; }
  return null;
}
function runUpdates(m, den, c) {
  const errs = [], notes = [];
  const snaps = (c.meta.audit && c.meta.audit.snaps) || {};
  const S = den === '1x' ? 480 : 720, D = den === '1x' ? 1 : 1.5;
  const screen = c.section === 'screens';
  const [fw, fh] = c.meta.frame || [S / D, S / D];
  const W = screen ? S : Math.round(fw * D), H = screen ? S : (fh ? Math.round(fh * D) : S);
  const defaults = Object.fromEntries((c.meta.props || []).map((p) => [p.name, p.default]));
  for (const p of c.meta.props || []) {
    if (!STATE_PROPS.has(p.name) || p.type === 'text') continue;
    if (snaps[p.name]) { notes.push(`update: ${p.name} snaps on purpose: ${snaps[p.name]}`); continue; }
    const from = { ...defaults, ...((c.meta.audit && c.meta.audit.base) || {}) }[p.name];
    const to = changed(p, from);
    if (to === null || to === undefined) continue;
    const id = m._fw_stage(W, H);
    let t = Math.round(m.__t || 0);
    const tick = (n = 1) => { for (let i = 0; i < n; i++) { t += TICK; m._fw_tick(t); } m.__t = t; m._fw_events(); };
    const base = { ...defaults, ...((c.meta.audit && c.meta.audit.base) || {}), opening: false };  // meta audit.base: the case to test from
    str(m, c.id, (cp) => str(m, JSON.stringify(base), (pp) => m._fw_render(id, cp, pp, 0, 0)));
    tick(90);
    const hash = () => { const fp = m._fw_frame(id); return createHash('sha1').update(m.HEAPU8.subarray(fp, fp + W * H * 4)).digest('hex'); };
    const h0 = hash();
    // a looping component (a pulse, a playing disc) changes on its own: measure what it does without the update first
    const idle = new Set();
    for (let k = 0; k < 20; k++) { tick(); idle.add(hash()); }
    const r = str(m, c.id, (cp) => str(m, JSON.stringify({ ...base, [p.name]: to }), (pp) => m.UTF8ToString(m._fw_update(id, cp, pp, 0, 0))));
    const seen = new Set();
    for (let k = 0; k < 62; k++) { tick(); seen.add(hash()); }
    m._fw_stage_free(id);
    if (r === 'rebuild') { errs.push(`update: ${p.name} -> ${JSON.stringify(to)} rebuilds instead of updating`); continue; }
    if (idle.size > 2) continue;  // it moves by itself: can't tell
    const fresh = [...seen].filter((h) => h !== h0 && !idle.has(h)).length;
    if (fresh === 0) continue;   // nothing visible changed (e.g. a prop with no look in this variant)
    if (fresh < 3) errs.push(`update: ${p.name} ${JSON.stringify(defaults[p.name])} -> ${JSON.stringify(to)} jumps in ${fresh} frame${fresh > 1 ? 's' : ''} (it must animate like a tap, silently)`);
  }
  return { errs, notes };
}

// ---------------------------------------------------------------- run
const cachePath = (den) => join(CACHE, `audit-${den}.json`);
let total = 0, failed = 0;
const report = {};
for (const den of dens) {
  const m = await load(den);
  const meta = JSON.parse(m.UTF8ToString(m._fw_meta())).map((c) => ({ ...c, meta: typeof c.meta === 'string' ? JSON.parse(c.meta) : c.meta }));
  const cache = !flag('--fresh') && existsSync(cachePath(den)) ? JSON.parse(readFileSync(cachePath(den), 'utf8')) : {};
  const next = {};
  for (const c of meta) {
    if (!['components', 'screens'].includes(c.section) || c.id === 'device') continue;
    if (only && !only.includes(c.id)) { if (cache[c.id]) next[c.id] = cache[c.id]; continue; }
    const key = compHash(c.id);
    let res = cache[c.id] && cache[c.id].key === key && !Object.keys(EXTRA).length ? cache[c.id] : null;
    if (!res) {
      const defaults = Object.fromEntries((c.meta.props || []).map((p) => [p.name, p.default]));
      const cases = [{ label: 'default', props: {} }, ...(c.meta.variants || [])];
      const out = [];
      if (c.section === 'components') {
        const u = runUpdates(m, den, c);
        for (const e of u.errs) out.push({ e, at: 'update' });
        for (const n of u.notes) out.push({ n, at: 'update' });
      }
      for (const v of cases) for (const light of [false, true]) {
        const r = runCase(m, den, c, { ...defaults, ...v.props, ...EXTRA }, light, (v.audit && v.audit.skip) || {});
        for (const e of r.errs) out.push({ e, at: `${v.label} · ${light ? 'light' : 'dark'}` });
        for (const n of r.notes) out.push({ n, at: `${v.label} · ${light ? 'light' : 'dark'}` });
      }
      res = { key, out };
    }
    next[c.id] = res;
    const errs = res.out.filter((x) => x.e), notes = res.out.filter((x) => x.n);
    total++;
    report[`${c.id}@${den}`] = res.out;
    if (errs.length) failed++;
    if (errs.length || (!quiet && notes.length)) {
      // one line per distinct message, with the cases it happens in
      const group = (xs, k) => { const g = new Map(); for (const x of xs) g.set(x[k], [...(g.get(x[k]) || []), x.at]); return g; };
      if (errs.length) console.log(`✗ ${c.id} [${den}]`);
      for (const [msg, at] of group(errs, 'e')) console.log(`    ${msg}  — ${at.length > 3 ? `${at.slice(0, 3).join('; ')} +${at.length - 3}` : at.join('; ')}`);
      if (!quiet) for (const [msg] of group(notes, 'n')) console.log(`    note: ${msg}`);
    }
  }
  if (!Object.keys(EXTRA).length) try { writeFileSync(cachePath(den), JSON.stringify(next)); } catch {}
}
if (arg('--json')) writeFileSync(arg('--json'), JSON.stringify(report, null, 1));
console.log(`audit: ${total} component x density checks, ${failed} with errors`);
process.exit(failed ? 1 : 0);
