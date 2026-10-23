#!/usr/bin/env node
// Dry run of a screen spec or deck on the WASM build (the hub's /api/wasm/send runs this before it sends anything):
// the same validation the panel runs (core/spec.h), then the screen built at the target density with the panel's
// builder: build errors, its objects, anything outside the screen. Reads the spec on stdin.
// --known a,b: screens the target already has (its hello): a deck naming them is fine although this build lacks them.
// Usage: node tools/spec.mjs [--den 1x|1.5x] [--build <dir>] [--known ids] < spec.json   -> JSON {ok, kind, errors, components, nodes,
//        binds, render?: {error, objects, outside}}; exit 0 (the answer says ok or not)
import { readFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const HERE = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const env = (() => { try { return Object.fromEntries(readFileSync(join(HERE, '..', 'local.env'), 'utf8').split('\n').map((l) => l.match(/^([A-Z_]+)=(.*)$/)).filter(Boolean).map((m) => [m[1], m[2]])); } catch { return {}; } })();
const den = arg('--den', '1.5x'), BUILD = arg('--build', process.env.FW_OUT || env.FW_OUT || join(HERE, 'out'));
const text = readFileSync(0, 'utf8');
const F = (await import(pathToFileURL(join(BUILD, den, 'fw.mjs')).href)).default;
const m = await F();
m._fw_init();
const W = den === '1x' ? 480 : 720, st = m._fw_stage(W, W);
const S = (p, f) => { const q = m.stringToNewUTF8(p); try { return f(q); } finally { m._free(q); } };
const spec = (c, a) => S(c, (pc) => S(a, (pa) => JSON.parse(m.UTF8ToString(m._fw_spec(pc, pa)))));
const out = spec('check', text);
let parsed = null;
try { parsed = JSON.parse(text); } catch { /* the check said so */ }
out.kind = parsed && (Array.isArray(parsed.pages) || Array.isArray(parsed.screens)) ? 'deck' : 'screen';
const known = (arg('--known', '') || '').split(',').filter(Boolean);
if (out.kind === 'deck' && known.length) {
  out.errors = out.errors.filter((e) => !(e.code === 'unknown_screen' && known.includes(parsed.pages?.[Number(e.path.match(/\[(\d+)\]/)?.[1])]?.kind)));
  out.ok = out.errors.length === 0;
}
if (out.ok && out.kind === 'screen') {
  const ap = spec('screen', text);  // into this run's memory store, built once off screen, then shown here
  if (!ap.ok) { out.ok = false; out.errors = ap.errors ?? [{ code: ap.status, msg: 'refused' }]; }
  else {
    const e = S('spec', (pc) => S(JSON.stringify({ screen: parsed.id }), (pp) => m.UTF8ToString(m._fw_render(st, pc, pp, 0, -1))));
    let t = 0;
    for (let i = 0; i < 60; i++) { t += 16; m._fw_tick(t); }
    const rects = JSON.parse(m.UTF8ToString(m._fw_rects(st)));
    const outside = rects.filter((r) => r.x < 0 || r.y < 0 || r.x + r.w > W || (r.y + r.h > W && r.d <= 3)).length;
    out.render = { error: e, objects: rects.length, outside };
    if (e) { out.ok = false; out.errors = [{ path: '', code: 'build', msg: e }]; }
  }
}
process.stdout.write(JSON.stringify(out) + '\n');
