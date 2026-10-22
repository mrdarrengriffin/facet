#!/usr/bin/env node
// Screen layout check (2026-10-22, after the F1 screen drew its circuit over the header and the standings): the
// component audit checks parts alone; this checks how a SCREEN places them. For every screen x variant x theme x
// density, at the build and again after 1 s and 3 s of updates (live screens move):
//   overlap   two regions (the screen root's direct children: header, region boxes, ...) whose boxes intersect
//   escape    an object inside one region whose box (+ its ext draw size), cut by the ancestors that clip (scroll
//             lists, anything not overflow-visible), reaches into ANOTHER region's box, or off the screen
// A region that is meant to sit over others marks itself fw::qa::free(obj, "overlay: <why>") (a bottom sheet,
// a toast layer): it is skipped for both checks. Zero-size boxes (holders of state) are not regions.
// Usage:  node renderer/tools/audit_screens.mjs [--den 1x|1.5x] [--screen id,id] [--json out.json]
// As a module (tools/audit.mjs can call it per screen case): import { checkScreen } and pass the fw_audit objects.
import { readFileSync, writeFileSync, existsSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const inter = (a, b) => Math.max(0, Math.min(a.x2, b.x2) - Math.max(a.x1, b.x1) + 1) * Math.max(0, Math.min(a.y2, b.y2) - Math.max(a.y1, b.y1) + 1);
const box = (o, ext = 0) => ({ x1: o.x - ext, y1: o.y - ext, x2: o.x + o.w - 1 + ext, y2: o.y + o.h - 1 + ext });
const name = (os, i) => {
  const o = os[i];
  const kids = os.filter((k) => k.p === i);
  const txt = o.text || (kids.find((k) => k.text) || {}).text;
  return `${o.t || 'obj'} ${o.w}x${o.h} at ${o.x},${o.y}${txt ? ` "${String(txt).slice(0, 24)}"` : ''}`;
};

// objs = fw_audit(stage) of a stage showing one screen; W x H = the screen
export function checkScreen(objs, W, H) {
  const errs = [];
  const root = objs.findIndex((o) => o.p < 0);
  if (root < 0) return errs;
  const overlay = (o) => typeof o.free === 'string' && /^overlay/i.test(o.free);
  const regions = objs.map((o, i) => ({ o, i })).filter(({ o }) => o.p === root && o.w > 0 && o.h > 0 && !overlay(o));
  // region of every object (walk up to the root's child)
  const regionOf = objs.map((o, i) => {
    let k = i;
    while (k >= 0 && objs[k].p !== root) k = objs[k].p;
    return k;
  });
  for (let a = 0; a < regions.length; a++)
    for (let b = a + 1; b < regions.length; b++) {
      const n = inter(box(regions[a].o), box(regions[b].o));
      if (n > 0) errs.push(`overlap: ${name(objs, regions[a].i)} and ${name(objs, regions[b].i)} share ${n} px`);
    }
  const screen = { x1: objs[root].x, y1: objs[root].y, x2: objs[root].x + W - 1, y2: objs[root].y + H - 1 };
  const seen = new Set();
  objs.forEach((o, i) => {
    const r = regionOf[i];
    if (r < 0 || r === i || objs[r].p !== root || overlay(objs[r]) || o.w <= 0 || o.h <= 0 || (o.opa !== undefined && o.opa === 0)) return;
    // what can actually show: the box cut by every ancestor that clips (not overflow-visible; scroll lists clip)
    let b = box(o, o.ext || 0);
    for (let k = o.p; k >= 0 && k !== root && b; k = objs[k].p) {
      if (objs[k].ov && !objs[k].sc) continue;
      const c = box(objs[k], objs[k].ov ? objs[k].ext || 0 : 0);
      b = { x1: Math.max(b.x1, c.x1), y1: Math.max(b.y1, c.y1), x2: Math.min(b.x2, c.x2), y2: Math.min(b.y2, c.y2) };
      if (b.x1 > b.x2 || b.y1 > b.y2) b = null;
    }
    if (!b) return;
    for (const g of regions) {
      if (g.i === r) continue;
      const n = inter(b, box(g.o));
      const key = `${r}>${g.i}`;
      if (n > 0 && !seen.has(key)) { seen.add(key); errs.push(`escape: ${name(objs, i)} (in ${name(objs, r)}) reaches ${n} px into ${name(objs, g.i)}`); }
    }
    if ((b.x1 < screen.x1 || b.y1 < screen.y1 || b.x2 > screen.x2 || b.y2 > screen.y2) && !seen.has(`${r}>off`)) {
      seen.add(`${r}>off`);
      errs.push(`escape: ${name(objs, i)} (in ${name(objs, r)}) leaves the screen`);
    }
  });
  return errs;
}

// ---------------------------------------------------------------- CLI
if (import.meta.url === pathToFileURL(process.argv[1]).href) {
  const HERE = resolve(dirname(fileURLToPath(import.meta.url)), '..');
  const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
  const env = (() => { try { return Object.fromEntries(readFileSync(join(HERE, '..', 'local.env'), 'utf8').split('\n').map((l) => l.match(/^([A-Z_]+)=(.*)$/)).filter(Boolean).map((m) => [m[1], m[2]])); } catch { return {}; } })();
  const BUILD = process.env.FW_OUT || env.FW_OUT || join(HERE, 'out');
  const dens = arg('--den', 'all') === 'all' ? ['1x', '1.5x'] : [arg('--den')];
  const only = arg('--screen') ? arg('--screen').split(',') : null;
  const report = {};
  let failed = 0, total = 0;
  for (const den of dens) {
    const dir = join(BUILD, den);
    const F = (await import(pathToFileURL(join(dir, 'fw.mjs')).href)).default;
    const wasm = await WebAssembly.compile(readFileSync(join(dir, 'fw.wasm')));
    const m = await F({ instantiateWasm: (imports, ok) => { WebAssembly.instantiate(wasm, imports).then((i) => ok(i, wasm)); return {}; } });
    m._fw_init();
    const str = (s, f) => { const p = m.stringToNewUTF8(s); try { return f(p); } finally { m._free(p); } };
    const S = den === '1x' ? 480 : 720;
    const meta = JSON.parse(m.UTF8ToString(m._fw_meta())).map((c) => ({ ...c, meta: typeof c.meta === 'string' ? JSON.parse(c.meta) : c.meta }));
    let t = 0;
    for (const c of meta.filter((c) => c.section === 'screens' && (!only || only.includes(c.id)))) {
      const defaults = Object.fromEntries((c.meta.props || []).map((p) => [p.name, p.default]));
      const out = new Map();
      for (const v of [{ label: 'default', props: {} }, ...(c.meta.variants || [])])
        for (const light of [false, true]) {
          const A = m._fw_stage(S, S);
          const e = str(c.id, (cp) => str(JSON.stringify({ ...defaults, ...v.props }), (pp) => m.UTF8ToString(m._fw_render(A, cp, pp, light ? 1 : 0, 0))));
          if (e) { out.set(`render: ${e}`, [`${v.label}`]); m._fw_stage_free(A); continue; }
          for (const at of [0, 1000, 3000]) {
            while (t < (m.__t0 || 0) + at) { t += 16; m._fw_tick(t); }
            m._fw_events();
            for (const err of checkScreen(JSON.parse(m.UTF8ToString(m._fw_audit(A))), S, S))
              out.set(err, [...(out.get(err) || []), `${v.label} · ${light ? 'light' : 'dark'} · ${at / 1000}s`]);
          }
          m.__t0 = t;
          m._fw_stage_free(A);
        }
      total++;
      report[`${c.id}@${den}`] = [...out].map(([e, at]) => ({ e, at }));
      if (out.size) {
        failed++;
        console.log(`✗ ${c.id} [${den}]`);
        for (const [e, at] of out) console.log(`    ${e}  — ${at.length > 3 ? `${at.slice(0, 3).join('; ')} +${at.length - 3}` : at.join('; ')}`);
      }
    }
  }
  if (arg('--json')) writeFileSync(arg('--json'), JSON.stringify(report, null, 1));
  console.log(`screens: ${total} screen x density checks, ${failed} with layout errors`);
  process.exit(failed ? 1 : 0);
}
