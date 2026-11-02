#!/usr/bin/env node
// Functional tests (tests/functional/README.md): end-to-end checks that each component and screen DOES what it should,
// read back from what it renders. Behaviour tests (tests/rules/behaviour.mjs) sweep every id for generic rules (one sound
// per tap, disabled fires nothing, scroll limits, lag); these are written by hand per component: a slider clamps at
// max, a light tile's state line follows the live HA state after a toggle, a sensor readout comes back after a
// reconnect. Headless on the WASM build (no browser): mount a component or the device, script a fake Home Assistant
// (states, unavailable, disconnect / reconnect, auto-reacting to service calls), touch it (tap, hold, drag), read the
// texts, boxes, events and sounds back.
// Usage: node renderer/tools/functional.mjs [--den 1.5x|1x|all] [--ids a,b] [--tier nightly] [--grep text] [--png fail|all|none]
//        [--out dir] [--json file] [--soft] [-v] [--probe comp '{props}']   exit 1 = a test failed (unless --soft)
//        --rules [--rule a,b | --skip a,b] [--tier fast|gate] [--theme id,id|all] [--fresh]: the rules (tests/rules/*.mjs) instead of the
//        per-component tests, over every component on one mount per case (tools/lib/rules.mjs)
// Results: tests/functional/out/results.json + per-test PNGs (default: failures only) in tests/functional/out/png/.
import { readFileSync, writeFileSync, existsSync, readdirSync, mkdirSync, rmSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { performance } from 'node:perf_hooks';
import { Bench, setBuild, shot } from './lib/fwbench.mjs';
import { runRules } from './lib/rules.mjs';
import { Ctx } from './lib/fakeha.mjs';

const HERE = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const argv = process.argv;
const arg = (k, d) => { const i = argv.indexOf(k); return i > 0 ? argv[i + 1] : d; };
const DIR = join(HERE, 'tests/functional'), OUT = resolve(arg('--out', join(DIR, 'out')));
const env = (() => { try { return Object.fromEntries(readFileSync(join(HERE, '..', 'local.env'), 'utf8').split('\n').map((l) => l.match(/^([A-Z_]+)=(.*)$/)).filter(Boolean).map((m) => [m[1], m[2]])); } catch { return {}; } })();
const BUILD = process.env.FW_OUT || env.FW_OUT || join(HERE, 'out');
setBuild(BUILD);
const B = JSON.parse(readFileSync(join(HERE, 'tests/behaviour/_budgets.json'), 'utf8'));
const DENS = (arg('--den', 'all') === 'all' ? ['1.5x', '1x'] : [arg('--den')]).filter((d) => existsSync(join(BUILD, d, 'fw.wasm')));
const ONLY = arg('--ids') ? arg('--ids').split(',') : null, GREP = arg('--grep'), PNG = arg('--png', 'fail');
const VERBOSE = argv.includes('-v');
const NIGHTLY = arg('--tier') === 'nightly';  // a suite with tier: 'nightly' runs only then (and then only those)

const slug = (s) => s.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '').slice(0, 60);

// ---------------------------------------------------------------- probe: print what a mount shows (writing tests)
if (argv.includes('--probe')) {
  const i = argv.indexOf('--probe'), comp = argv[i + 1], props = JSON.parse(argv[i + 2] || '{}');
  const b = new Bench(null, DENS[0], { build: BUILD, long_ms: B.long_press_ms, drag_px: B.drag_px_per_tick }), t = new Ctx(b);
  await (comp === 'device' ? t.device() : t.mount(comp, props));
  for (const o of t.objs().filter((o) => t.visible(o) && (o.text || o.clk))) console.log(`${o.clk ? 'CLK ' : '    '}${o.t} ${o.w}x${o.h}@${o.x},${o.y}${o.text ? ' "' + o.text + '"' : ''}`);
  console.log('comps:', t.comps().filter((c) => c.d <= 3).map((c) => `${c.ids.at(-1)} ${c.w}x${c.h}@${c.x},${c.y}`).join(' | '));
  process.exit(0);
}

// ---------------------------------------------------------------- rules (tests/rules/*.mjs)
if (argv.includes('--rules')) {
  const RDIR = join(HERE, 'tests/rules'), want = arg('--rule') ? arg('--rule').split(',') : null, skip = (arg('--skip') || '').split(','), tier = arg('--tier', 'gate');
  const rules = [];
  for (const f of readdirSync(RDIR).filter((f) => f.endsWith('.mjs')).sort()) {
    const r = (await import(pathToFileURL(join(RDIR, f)).href)).default;
    if ((!want || want.includes(r.id)) && !skip.includes(r.id) && r.tiers.includes(tier)) rules.push(r);
  }
  const cacheDir = process.env.FW_CACHE || env.FW_CACHE || join(HERE, '.cache');
  const res = await runRules(rules, { build: BUILD, dens: DENS, ids: ONLY, themeArg: arg('--theme'), tier, cacheDir, fresh: argv.includes('--fresh') });
  let bad = 0;
  for (const r of rules) {
    if (r.dens && !DENS.some((d) => r.dens.includes(d))) continue;  // a startup rule of another density
    const s = res[r.id];
    for (const l of s.out) console.log(l);
    console.log(`${r.id}: ${s.checks} checks, ${s.fails} failed${s.legacy ? `, ${s.legacy} in legacy parts` : ''} (${DENS.filter((d) => !r.dens || r.dens.includes(d)).join(' + ')}${arg('--theme') ? `, themes ${arg('--theme')}` : ''}${ONLY ? `, ${ONLY.join(',')}` : ''})`);
    bad += s.fails;
  }
  process.exit(bad ? 1 : 0);
}

// ---------------------------------------------------------------- run
const files = readdirSync(DIR).filter((f) => f.endsWith('.test.mjs')).sort();
const suites = [];
for (const f of files) { const s = (await import(pathToFileURL(join(DIR, f)).href)).default; if ((!ONLY || ONLY.includes(s.id)) && (s.tier === 'nightly') === NIGHTLY) suites.push({ ...s, file: f }); }
if (PNG !== 'none') { rmSync(join(OUT, 'png'), { recursive: true, force: true }); mkdirSync(join(OUT, 'png'), { recursive: true }); }
const t0 = performance.now(), results = [];
for (const s of suites) for (const test of s.tests) {
  if (GREP && !test.name.includes(GREP)) continue;
  for (const den of DENS.filter((d) => !test.den || test.den === d)) {
    const b = new Bench(null, den, { build: BUILD, long_ms: B.long_press_ms, drag_px: B.drag_px_per_tick }), t = new Ctx(b);
    const a = performance.now();
    try {
      if (test.mount) await t.mount(test.mount[0] || s.id, test.mount[1] || {});
      await test.run(t, { den, S: b.S });
    } catch (e) { t.fails.push(`error: ${e.message}`); if (VERBOSE) console.error(e.stack); }
    if (t.fails.length && b.stage >= 0) try { t.log.push(`screen texts: ${JSON.stringify(t.texts())}`); } catch {}
    const ok = !t.fails.length, xfail = test.xfail || null;
    const r = { id: s.id, family: s.family || '', test: test.name, den, ok, xfail, fails: t.fails, log: t.log, ms: Math.round(performance.now() - a) };
    if (b.stage >= 0 && (PNG === 'all' || (PNG === 'fail' && !ok))) {
      const p = `png/${s.id}/${den}-${slug(test.name)}.png`;
      mkdirSync(join(OUT, 'png', s.id), { recursive: true }); writeFileSync(join(OUT, p), shot(b)); r.png = p;
    }
    results.push(r);
    const tag = ok ? (xfail ? 'XPASS' : 'PASS') : xfail ? 'XFAIL' : 'FAIL';
    if (VERBOSE || !ok) console.log(`${tag} ${s.id} [${den}] ${test.name}${ok ? '' : ': ' + t.fails.slice(0, 3).join(' | ')}${xfail && !ok ? ` (${xfail})` : ''}`);
    if (VERBOSE && !ok) for (const l of t.log) console.log(`    ${l}`);
  }
}
const fails = results.filter((r) => !r.ok && !r.xfail), xf = results.filter((r) => !r.ok && r.xfail), xp = results.filter((r) => r.ok && r.xfail);
const summary = { at: new Date().toISOString(), build: BUILD, dens: DENS, total: results.length, pass: results.length - fails.length - xf.length, fail: fails.length, xfail: xf.length, xpass: xp.length, sec: +((performance.now() - t0) / 1000).toFixed(1) };
const doc = { ...summary, suites: suites.map((s) => ({ id: s.id, family: s.family || '', about: s.about || '', tests: s.tests.map((x) => x.name) })), results };
mkdirSync(OUT, { recursive: true });
writeFileSync(join(OUT, 'results.json'), JSON.stringify(doc, null, 1));
if (arg('--json')) writeFileSync(arg('--json'), JSON.stringify(doc, null, 1));
for (const r of xp) console.log(`XPASS ${r.id} [${r.den}] ${r.test}: passes now, remove its xfail`);
console.log(`functional: ${summary.total} tests, ${summary.pass} pass, ${summary.fail} FAIL, ${summary.xfail} expected-fail, ${summary.xpass} xpass (${summary.sec} s)`);
process.exit(fails.length && !argv.includes('--soft') ? 1 : 0);
