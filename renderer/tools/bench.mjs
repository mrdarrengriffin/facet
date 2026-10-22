#!/usr/bin/env node
// Facet bench (facet-153): scripted scenarios per component / screen, run headless on the WASM build, measured per
// LVGL refresh (core/bench.cpp: the same RENDER_START..READY window as the panels' GET /fw/perf), against the budgets
// per device (bench/budgets.json). Suite = bench/suite.json. README "Bench".
//   node tools/bench.mjs [--device guition|waveshare|all] [--case <id|prefix|tag:x>,...] [--scenario idle|animate|drag|navigate]
//                        [--props '<json>' merged into every case, e.g. {"fades":true}] [--out results.json] [--build <dir with 1x/ 1.5x/>] [--list] [--quiet]
// Output JSON: {v, at, build{}, devices{<id>: {fit{a, b}, anchors[]}}, results[{case, comp, scenario, device, live, wasm{}, est{}, mem{}, host{}, over[]}]}
// Times are virtual (fixed 16 ms ticks = LV_DEF_REFR_PERIOD), so redraws are deterministic; ms are host CPU (Node,
// TurboFan only: the script re-runs itself with --no-liftoff). est = a + b x WASM ms per device, fitted per run on
// the anchors (panel /fw/perf numbers; panels have a fixed per-frame cost, so a ratio alone over-reads big redraws). Exit 0 always (over budget is data, not an error), 2 on a usage error.
import { readFileSync, writeFileSync, existsSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { spawnSync, execSync } from 'node:child_process';
import { performance } from 'node:perf_hooks';

if (!process.execArgv.includes('--no-liftoff')) {
  const r = spawnSync(process.execPath, ['--no-liftoff', ...process.execArgv, fileURLToPath(import.meta.url), ...process.argv.slice(2)], { stdio: 'inherit' });
  process.exit(r.status ?? 1);
}

const HERE = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const ROOT = resolve(HERE, '..');
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const flag = (k) => process.argv.includes(k);
const env = (() => { try { return Object.fromEntries(readFileSync(join(ROOT, 'local.env'), 'utf8').split('\n').map((l) => l.match(/^([A-Z_]+)=(.*)$/)).filter(Boolean).map((m) => [m[1], m[2]])); } catch { return {}; } })();
const BUILD = arg('--build', process.env.FW_OUT || env.FW_OUT || join(HERE, 'out'));
const budgets = JSON.parse(readFileSync(join(HERE, 'bench/budgets.json'), 'utf8'));
const suite = JSON.parse(readFileSync(join(HERE, 'bench/suite.json'), 'utf8'));
const T = suite.timing;
const EXTRA = JSON.parse(arg('--props', '{}'));
const REPEAT = Math.max(1, Math.min(5, +arg('--repeat', 1) || 1));
const log = flag('--quiet') ? () => {} : (...a) => console.error(...a);
const devIds = arg('--device', 'all') === 'all' ? Object.keys(budgets.devices) : arg('--device').split(',');
for (const d of devIds) if (!budgets.devices[d]) { console.error(`unknown device ${d}: ${Object.keys(budgets.devices).join(', ')}`); process.exit(2); }

// ---------------------------------------------------------------- one WASM instance per case (shared compiled module)
const mods = {};
async function load(den) {
  if (!mods[den]) {
    const dir = join(BUILD, den);
    if (!existsSync(join(dir, 'fw.wasm'))) throw new Error(`no build in ${dir} (renderer/build.sh)`);
    mods[den] = {
      F: (await import(pathToFileURL(join(dir, 'fw.mjs')).href)).default,
      wasm: await WebAssembly.compile(readFileSync(join(dir, 'fw.wasm'))),
      manifest: JSON.parse(readFileSync(join(dir, 'manifest.json'), 'utf8')),
    };
  }
  const M = mods[den];
  const m = await M.F({ instantiateWasm: (imports, ok) => { WebAssembly.instantiate(M.wasm, imports).then((i) => ok(i, M.wasm)); return {}; } });
  if (!m._fw_bench_take) throw new Error('this build has no bench probes (core/bench.cpp)');
  m._fw_init();
  return m;
}
const str = (m, s, f) => { const p = m.stringToNewUTF8(s); try { return f(p); } finally { m._free(p); } };
const f64 = (m, p, n) => Array.from(new Float64Array(m.HEAPU8.buffer, p, n));

// ---------------------------------------------------------------- cases
let meta;
function cases() {
  const out = [];
  for (const c of meta) {
    if (!suite.auto.sections.includes(c.section) || suite.auto.skip.includes(c.id)) continue;
    out.push({ id: c.id, comp: c.id, props: {}, scenarios: suite.auto.scenarios, live: suite.live.includes(c.id), auto: true });
  }
  for (const c of suite.cases) out.push({ live: false, ...c });
  return out;
}
function pick(all) {
  const f = arg('--case');
  if (!f) return all;
  const keys = f.split(',');
  return all.filter((c) => keys.some((k) => (k.startsWith('tag:') ? (c.tags || []).includes(k.slice(4)) : c.id === k || c.id.startsWith(k + '.') || c.comp === k)));
}

// ---------------------------------------------------------------- one case x scenario on one device
const pct = (a, q) => { if (!a.length) return 0; const s = [...a].sort((x, y) => x - y); return s[Math.min(s.length - 1, Math.floor(q * s.length))]; };
const avg = (a) => (a.length ? a.reduce((x, y) => x + y, 0) / a.length : 0);
const r2 = (v) => Math.round(v * 100) / 100;
const r3 = (v) => Math.round(v * 1000) / 1000;

async function run(dev, c, scenario) {
  const D = budgets.devices[dev];
  const m = await load(D.density);
  let w = D.w, h = D.h;
  const id = m._fw_stage(w, h);
  m._fw_bench_attach();
  let t = 0;
  const tick = () => { t += T.tick_ms; m._fw_tick(t); };
  for (let i = 0; i < 3; i++) tick();
  m._fw_areas(id); m._fw_bench_take();
  const mem0 = f64(m, m._fw_bench_mem(), 4);
  // build + first frame
  const b0 = performance.now();
  const err = str(m, c.comp, (cp) => str(m, JSON.stringify({ ...(c.props || {}), ...EXTRA }), (pp) => m.UTF8ToString(m._fw_render(id, cp, pp, 0, 0))));
  if (err) { m._fw_stage_free(id); return { error: err }; }
  if (c.stage === 'content') {
    const ch = m._fw_content_h(id);
    if (ch > h) { h = ch; m._fw_stage_resize(id, w, h); }
  }
  tick();
  const build_ms = performance.now() - b0;
  const first = f64(m, m._fw_bench_take(), 8);
  m._fw_areas(id);
  // host copy proxy (the workbench's canvas copy): whole stage vs only the flushed areas (the 2026-10-22 lag fix)
  const dst = new Uint8ClampedArray(w * h * 4);
  let tn = 0;
  const frames = { at: [], render: [], work: [], px: [], areas: [], flush: 0, full: 0, renders: 0, refr: 0, hostFull: [], hostAreas: [] };
  const step = () => {
    tn++;
    const w0 = performance.now();
    tick();
    const work = performance.now() - w0;
    const s = f64(m, m._fw_bench_take(), 8);
    const ap = m._fw_areas(id);
    if (!s[0]) return;
    frames.renders += s[0]; frames.at.push(tn); frames.render.push(s[1]); frames.work.push(work); frames.px.push(s[4]); frames.areas.push(s[5]);
    frames.flush += s[3]; frames.full += s[7]; frames.refr += s[6];
    const fp = m._fw_frame(id);
    const H = m.HEAPU8;
    let h0 = performance.now();
    dst.set(H.subarray(fp, fp + w * h * 4));
    frames.hostFull.push(performance.now() - h0);
    h0 = performance.now();
    const n = new Int32Array(H.buffer, ap, 1)[0];
    const A = new Int32Array(H.buffer, ap + 4, n * 4);
    for (let i = 0; i < n; i++) {
      const [x1, y1, x2, y2] = A.subarray(i * 4, i * 4 + 4);
      for (let y = y1; y <= y2; y++) { const o = (y * w + x1) * 4; dst.set(H.subarray(fp + o, fp + o + (x2 - x1 + 1) * 4), o); }
    }
    frames.hostAreas.push(performance.now() - h0);
  };
  const ticks = (ms) => { for (let i = 0; i < Math.round(ms / T.tick_ms); i++) step(); };
  const reset = () => { for (const k of ['at', 'render', 'work', 'px', 'areas', 'hostFull', 'hostAreas']) frames[k] = []; frames.flush = frames.full = frames.renders = frames.refr = 0; };
  let window_ms = 0, note = '';
  const settle = () => { ticks(T.settle_ms); reset(); };
  if (scenario === 'animate') { ticks(T.window_ms); window_ms = T.window_ms; }
  else if (scenario === 'idle') { settle(); ticks(T.window_ms); window_ms = T.window_ms; }
  else if (scenario === 'drag') {
    settle();
    const [[x0, y0], [x1, y1]] = c.path || [[0.2, 0.5], [0.8, 0.5]];
    const n = Math.round(T.drag_ms / T.tick_ms);
    m._fw_touch(id, Math.round(x0 * w), Math.round(y0 * D.h), 1); step();
    for (let i = 1; i <= n; i++) { m._fw_touch(id, Math.round((x0 + ((x1 - x0) * i) / n) * w), Math.round((y0 + ((y1 - y0) * i) / n) * D.h), 1); step(); }
    m._fw_touch(id, Math.round(x1 * w), Math.round(y1 * D.h), 0);
    ticks(T.after_ms);
    window_ms = T.tick_ms * (n + 1) + T.after_ms;
  } else if (scenario === 'navigate') {
    settle();
    const e1 = str(m, c.to || 'settings', (p) => m.UTF8ToString(m._fw_bench_nav(p, 0)));
    ticks(T.nav_ms);
    const e2 = str(m, '', (p) => m.UTF8ToString(m._fw_bench_nav(p, 1)));
    ticks(T.nav_ms);
    window_ms = 2 * T.nav_ms;
    note = e1 || e2;
    if (note) { m._fw_stage_free(id); return { error: note }; }
  }
  const mem1 = f64(m, m._fw_bench_mem(), 4);
  m._fw_stage_free(id);
  const n = frames.render.length;
  // redraw rate while it moves: frames over the span from the first to the last redraw (a 160 ms sheet open is not '5 fps')
  const active_ms = n ? (frames.at[n - 1] - frames.at[0] + 1) * T.tick_ms : 0;
  return {
    w, h, window_ms,
    wasm: {
      build_ms: r2(build_ms), first_ms: r3(first[1]), frames: n, active_ms, redraw_fps: n ? r2((n * 1000) / active_ms) : 0,
      render_ms_avg: r3(avg(frames.render)), render_ms_p95: r3(pct(frames.render, 0.95)), render_ms_max: r3(Math.max(0, ...frames.render)),
      work_ms_avg: r3(avg(frames.work)), flush_ms_avg: r3(n ? frames.flush / n : 0),
      px_avg: Math.round(avg(frames.px)), px_pct: r2((100 * avg(frames.px)) / (w * h)), px_per_s: Math.round((frames.px.reduce((a, b) => a + b, 0) * 1000) / window_ms),
      areas_avg: r2(avg(frames.areas)), full_redraws: frames.full,
    },
    host: { copy_full_ms: r3(avg(frames.hostFull)), copy_areas_ms: r3(avg(frames.hostAreas)) },
    // heap growth of the case, minus the bigger stage buffers of a content-height stage (RGBA + RGB565 draw buffer)
    mem: { heap_kb: Math.round((mem1[0] - mem0[0] - (h - D.h) * w * 6) / 1024), heap_total_kb: Math.round(mem1[1] / 1024), objs: mem1[2], anims: mem1[3] },
  };
}

// ---------------------------------------------------------------- budgets
// fit = {a, b}: panel render ms = a + b x WASM render ms (least squares over the anchors, a >= 0)
const estMs = (fit, ms) => (ms > 0 ? fit.a + fit.b * ms : 0);
// --repeat N: each case N times; times = the fastest of the N (noise only ever adds time: host load, GC), counts = the median
async function runMed(dev, c, sc, n) {
  const rs = [];
  for (let i = 0; i < n; i++) { const r = await run(dev, c, sc); if (r.error) return r; rs.push(r); }
  if (n === 1) return rs[0];
  const timed = (f) => /_ms|_ms_/.test(f) && f !== 'active_ms';
  const med = (k) => Object.fromEntries(Object.keys(rs[0][k]).map((f) => [f, timed(f) ? Math.min(...rs.map((r) => r[k][f])) : pct(rs.map((r) => r[k][f]), 0.5)]));
  return { ...rs[0], wasm: med('wasm'), host: med('host'), mem: med('mem') };
}
// the accepted baseline (--baseline <run.json>, the hub's data/facet-bench/baseline.json): WASM ms per device|case|scenario
const BASE = (() => { try { const b = JSON.parse(readFileSync(arg('--baseline'), 'utf8')); return { at: b.at, git: b.git, m: new Map(b.results.filter((r) => r.wasm).map((r) => [`${r.device}|${r.case}|${r.scenario}`, r.wasm])) }; } catch { return null; } })();
// checks per device (budgets.json `checks`): fps / frame_ms = absolute (Waveshare); regression = WASM ms over the
// accepted baseline by more than regression_pct; pacing = panel p95 over pacing_ratio x the average (judder); idle, build, heap, objs
function judge(dev, fit, c, scenario, r) {
  const D = budgets.devices[dev], B = D.budget, on = (k) => (D.checks || ['fps', 'frame_ms', 'idle', 'build', 'heap', 'objs']).includes(k);
  const est_avg = estMs(fit, r.wasm.render_ms_avg), est_p95 = estMs(fit, r.wasm.render_ms_p95);
  const fps = r.wasm.frames ? Math.min(D.loop_cap_fps, r.wasm.redraw_fps, est_avg > 0 ? 1000 / est_avg : Infinity) : 0;
  // build: no panel anchor yet (POST /fw/show reports ms: Phase 2), so only flagged once budgets.json has build_factor
  const est = { render_ms_avg: r2(est_avg), render_ms_p95: r2(est_p95), fps: r2(fps), build_ms: D.build_factor ? r2(r.wasm.build_ms * D.build_factor) : null };
  const over = [];
  const moving = r.wasm.frames > 0 && (scenario !== 'idle' || c.live);
  est.pacing = moving && est_avg > 0 ? r2(est_p95 / est_avg) : null;
  if (on('fps') && moving && r.wasm.redraw_fps >= B.fps_min && fps < B.fps_min) over.push(`fps ${est.fps} < ${B.fps_min}`);
  if (on('frame_ms') && moving && est_p95 > B.frame_ms_max) over.push(`frame p95 ${est.render_ms_p95} ms > ${B.frame_ms_max}`);
  if (on('pacing') && moving && r.wasm.frames >= 10 && est.pacing > B.pacing_ratio) over.push(`judder p95 ${est.pacing}x avg`);
  if (on('idle') && scenario === 'idle' && !c.live && r.wasm.px_per_s > B.idle_px_per_s) over.push(`idle redraw ${r.wasm.px_per_s} px/s`);
  if (on('build') && est.build_ms != null && est.build_ms > B.build_ms_max) over.push(`build ${est.build_ms} ms > ${B.build_ms_max}`);
  if (on('heap') && r.mem.heap_kb > B.heap_kb_max) over.push(`heap ${r.mem.heap_kb} KB > ${B.heap_kb_max}`);
  if (on('objs') && r.mem.objs > B.objs_max) over.push(`objs ${r.mem.objs} > ${B.objs_max}`);
  return { est, over };
}

// ---------------------------------------------------------------- main
function fitLine(p) {
  if (!p.length) return { a: 0, b: 1 };
  if (p.length === 1) return { a: 0, b: p[0][1] / p[0][0] };
  const mx = avg(p.map((q) => q[0])), my = avg(p.map((q) => q[1]));
  const sxy = p.reduce((s, [x, y]) => s + (x - mx) * (y - my), 0), sxx = p.reduce((s, [x]) => s + (x - mx) ** 2, 0);
  let b = sxx ? sxy / sxx : my / mx, a = my - b * mx;
  if (a < 0) { a = 0; b = p.reduce((s, [x, y]) => s + x * y, 0) / p.reduce((s, [x]) => s + x * x, 0); }
  return { a, b };
}
const first = await load(budgets.devices[devIds[0]].density);
meta = JSON.parse(first.UTF8ToString(first._fw_meta()));
const all = cases();
if (flag('--list')) {
  if (flag('--json')) { process.stdout.write(JSON.stringify({ timing: T, cases: [...all, ...suite.anchors], panel_skip: suite.panel_skip || {}, devices: Object.fromEntries(Object.entries(budgets.devices).map(([k, d]) => [k, { name: d.name, w: d.w, h: d.h, density: d.density }])) })); process.exit(0); }
  for (const c of [...all, ...suite.anchors]) console.log(c.id, c.scenarios.join(','));
  process.exit(0);
}
const chosen = pick(all);
const onlyScen = arg('--scenario');

// ---------------------------------------------------------------- panel mode (Phase 2: the panel tests itself)
// --panel <device> --host <ip>: ONE batch (POST /fw/bench); the panel runs it on its own screen and keeps the results;
// the host waits the panel's estimate, then GET /fw/bench every 10 s until done (no polling while it measures).
if (arg('--panel')) {
  const dev = arg('--panel'), host = arg('--host');
  if (!budgets.devices[dev] || !host) { console.error('--panel guition|waveshare --host <ip>'); process.exit(2); }
  const base = `http://${host}`;
  const get = async (u) => (await fetch(base + u, { signal: AbortSignal.timeout(8000) })).json();
  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
  const info = await get('/fw/info');
  const have = new Set((await get('/fw/meta')).map((m) => m.id));  // what this firmware registered
  const skip = suite.panel_skip || {};
  const list = [...chosen, ...(arg('--case') ? [] : suite.anchors)];
  const cases = [], skipped = [];
  for (const c of list) for (const sc of c.scenarios) {
    if (onlyScen && sc !== onlyScen) continue;
    if (c.browser_only) { skipped.push({ case: c.id, comp: c.comp, scenario: sc, device: dev, skipped: c.browser_only }); continue; }
    if (skip[c.id]) { skipped.push({ case: c.id, comp: c.comp, scenario: sc, device: dev, skipped: skip[c.id] }); continue; }
    if (have.has(c.comp)) cases.push([c.id, c.comp, { ...(c.props || {}), ...EXTRA }, sc, ...(c.path || c.to ? [c.path ?? null, c.to ?? null] : [])]);
    else skipped.push({ case: c.id, comp: c.comp, scenario: sc, device: dev, skipped: 'not in this firmware' });
  }
  const run = arg('--run', `${dev}-${Date.now()}`);
  const res = { v: 1, source: 'panel', device: dev, run, at: new Date().toISOString(), filter: { case: arg('--case') || null, scenario: onlyScen || null, device: dev, props: Object.keys(EXTRA).length ? EXTRA : null },
    firmware: { core: info.core, glue: info.glue, built: info.built }, results: [] };
  const p = await (await fetch(base + '/fw/bench', { method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify({ run, theme: 'dark', timing: T, cases }), signal: AbortSignal.timeout(8000) })).json();
  if (!p.ok) { console.error(`panel refused the batch: ${p.error}`); process.exit(1); }
  log(`[panel ${dev}] ${p.total} cases sent, about ${p.eta_s} s; collecting after that`);
  if (arg('--status')) writeFileSync(arg('--status'), JSON.stringify({ device: dev, run, total: p.total, eta_s: p.eta_s, started: Date.now() }));
  await sleep(p.eta_s * 1000 + 3000);
  let r;
  for (let i = 0; ; i++) {
    try { r = await get('/fw/bench'); } catch (e) { r = { state: 'unreachable', error: String(e.message || e) }; }
    if (r.state === 'done' || r.state === 'interrupted' || r.state === 'idle' || r.run !== run && r.state !== 'unreachable') break;
    if (i > 60) { r = { state: 'timeout' }; break; }
    log(`[panel ${dev}] ${r.state} ${r.done ?? '?'}/${r.total ?? '?'}`);
    await sleep(10000);
  }
  if (r.state !== 'done' || r.run !== run) { res.error = r.state === 'done' ? 'another batch replaced this one' : r.state; log(`[panel ${dev}] ${res.error}`); }
  const byKey = new Map(list.flatMap((c) => c.scenarios.map((sc) => [`${c.id}|${sc}`, c])));
  for (const x of r.results || []) { const c = byKey.get(`${x.case}|${x.scenario}`); res.results.push({ ...x, comp: c?.comp, device: dev, live: !!c?.live, anchor: x.case.startsWith('anchor.') }); }
  res.results.push(...skipped);
  res.run_s = r2((Date.now() - Date.parse(res.at)) / 1000);
  const o = arg('--out');
  if (o) writeFileSync(o, JSON.stringify(res, null, 1)); else process.stdout.write(JSON.stringify(res));
  log(`[panel ${dev}] ${res.results.filter((x) => x.panel).length} measured, ${skipped.length} skipped, ${res.results.filter((x) => x.error).length} errors`);
  process.exit(res.error ? 1 : 0);
}

// ---------------------------------------------------------------- calibrate: --calibrate <panel run> <wasm run>
// pairs every case measured on both sides (frames on both), reports how far the anchor estimate was off, fits panel ms =
// a + b x WASM ms over all of them and the build factor, and writes them to budgets.json devices.<d>.measured (estimates
// then use it instead of the anchors).
if (arg('--calibrate')) {
  const P = JSON.parse(readFileSync(arg('--calibrate'), 'utf8')), W = JSON.parse(readFileSync(process.argv[process.argv.indexOf('--calibrate') + 2], 'utf8'));
  const dev = P.device, k = (r) => `${r.case}|${r.scenario}`;
  const wm = new Map(W.results.filter((r) => r.device === dev && r.wasm).map((r) => [k(r), r]));
  const pairs = P.results.filter((r) => r.panel && wm.has(k(r))).map((r) => ({ r, w: wm.get(k(r)) }));
  const moving = pairs.filter(({ r, w }) => r.panel.frames >= 3 && w.wasm.frames >= 3);
  const errs = moving.map(({ r, w }) => ({ case: r.case, scenario: r.scenario, panel: r.panel.render_ms_avg, est: w.est.render_ms_avg, err: w.est.render_ms_avg - r.panel.render_ms_avg, pct: (100 * (w.est.render_ms_avg - r.panel.render_ms_avg)) / r.panel.render_ms_avg }));
  const fit = fitLine(moving.map(({ r, w }) => [w.wasm.render_ms_avg, r.panel.render_ms_avg]));
  const after = moving.map(({ r, w }) => (100 * (fit.a + fit.b * w.wasm.render_ms_avg - r.panel.render_ms_avg)) / r.panel.render_ms_avg);
  const builds = pairs.filter(({ r, w }) => r.panel.build_ms > 0 && w.wasm.build_ms > 0).map(({ r, w }) => r.panel.build_ms / w.wasm.build_ms);
  const absMed = (a) => r2(pct(a.map(Math.abs), 0.5)), absP90 = (a) => r2(pct(a.map(Math.abs), 0.9));
  const out = { device: dev, pairs: moving.length, before: { fit: W.devices[dev]?.fit, err_pct_median: absMed(errs.map((e) => e.pct)), err_pct_p90: absP90(errs.map((e) => e.pct)), err_ms_median: absMed(errs.map((e) => e.err)) },
    after: { fit: { a: r2(fit.a), b: r2(fit.b) }, err_pct_median: absMed(after), err_pct_p90: absP90(after) }, build_factor: r2(pct(builds, 0.5)), rows: errs.map((e) => ({ ...e, err: r2(e.err), pct: r2(e.pct) })) };
  if (flag('--write')) {
    budgets.devices[dev].measured = { a: out.after.fit.a, b: out.after.fit.b, n: moving.length, at: P.at.slice(0, 10), err_pct_median: out.after.err_pct_median };
    budgets.devices[dev].build_factor = out.build_factor;
    writeFileSync(join(HERE, 'bench/budgets.json'), JSON.stringify(budgets, null, 2) + '\n');
  }
  const o = arg('--out');
  if (o) writeFileSync(o, JSON.stringify(out, null, 1)); else process.stdout.write(JSON.stringify(out, null, 1));
  process.exit(0);
}

let git = '';
try { git = execSync('git rev-parse --short HEAD', { cwd: ROOT }).toString().trim() + (execSync('git status --porcelain renderer', { cwd: ROOT }).toString().trim() ? '+wip' : ''); } catch {}
const out = { v: 1, at: new Date().toISOString(), git, filter: { case: arg('--case') || null, scenario: onlyScen || null, device: arg('--device', 'all'), props: Object.keys(EXTRA).length ? EXTRA : null, repeat: REPEAT }, baseline: BASE ? { at: BASE.at, git: BASE.git } : null, build: {}, devices: {}, results: [] };
const t0 = performance.now();
for (const dev of devIds) {
  const D = budgets.devices[dev];
  out.build[dev] = mods[D.density]?.manifest ?? (await load(D.density), mods[D.density].manifest);
  // fit the factor: warm-up pass, then the anchors (median of device ms / WASM ms)
  const pts = [], anchors = [];
  const byId = Object.fromEntries(suite.anchors.map((a) => [a.id, a]));
  for (const a of D.anchors) { const c = byId[a.case]; if (c) await run(dev, c, 'animate'); }
  for (const a of D.anchors) {
    const c = byId[a.case];
    if (!c) continue;
    const r = await run(dev, c, 'animate');
    if (r.error || !r.wasm.render_ms_avg) { anchors.push({ case: a.case, error: r.error || 'no frames' }); continue; }
    pts.push([r.wasm.render_ms_avg, a.device_ms]);
    anchors.push({ case: a.case, device_ms: a.device_ms, wasm_ms: r.wasm.render_ms_avg });
  }
  // measured (--calibrate --write, a fit over every case on the glass) wins over the anchors
  const fit = D.measured ? { a: D.measured.a, b: D.measured.b } : fitLine(pts);
  for (const a of anchors) if (a.wasm_ms) a.est_ms = r2(estMs(fit, a.wasm_ms));
  out.devices[dev] = { name: D.name, w: D.w, h: D.h, fit: { a: r2(fit.a), b: r2(fit.b), n: D.measured ? D.measured.n : pts.length, source: D.measured ? 'measured' : 'anchors' }, anchors, budget: D.budget, checks: D.checks ?? null, build_factor: D.build_factor ?? null };
  log(`[${dev}] panel ms = ${fit.a.toFixed(2)} + ${fit.b.toFixed(1)} x wasm ms (${pts.length} anchors)`);
  const list = [...chosen, ...(arg('--case') ? [] : suite.anchors)];
  for (const c of list) await run(dev, c, c.scenarios[0]);  // warm-up: every code path compiled before it is timed
  for (const c of list) {
    for (const sc of c.scenarios) {
      if (onlyScen && sc !== onlyScen) continue;
      const r = await runMed(dev, c, sc, REPEAT);
      const row = { case: c.id, comp: c.comp, props: c.props || {}, scenario: sc, device: dev, live: !!c.live, tags: c.tags || [], anchor: c.id.startsWith('anchor.') };
      if (r.error) { out.results.push({ ...row, error: r.error, over: ['error'] }); log(`[${dev}] ${c.id} ${sc}: ERROR ${r.error}`); continue; }
      const j = judge(dev, fit, c, sc, r);
      out.results.push({ ...row, w: r.w, h: r.h, window_ms: r.window_ms, wasm: r.wasm, est: j.est, host: r.host, mem: r.mem, over: j.over });
      log(`[${dev}] ${c.id} ${sc}: ${r.wasm.frames} fr, est ${j.est.render_ms_avg} ms ${j.est.fps} fps, ${r.wasm.px_pct}% px${j.over.length ? '  OVER: ' + j.over.join('; ') : ''}`);
    }
  }
}
// regression (after the run): each case's WASM ms over the baseline, divided by the run's median shift against the
// baseline (host load moves a whole run by 10-20 %; one case getting slower stands out from it). The shift is reported.
if (BASE) for (const dev of devIds) {
  const D = budgets.devices[dev], B = D.budget, on = (D.checks || []).includes('regression');
  const rows = out.results.filter((r) => r.device === dev && r.wasm?.frames >= 3);
  const ratio = (r) => { const b = BASE.m.get(`${dev}|${r.case}|${r.scenario}`); return b && b.render_ms_avg > 0 && b.frames >= 3 ? r.wasm.render_ms_avg / b.render_ms_avg : null; };
  const rs = rows.map(ratio).filter((x) => x != null);
  const shift = rs.length >= 5 ? pct(rs, 0.5) : 1;
  out.devices[dev].baseline_shift_pct = r2((shift - 1) * 100);
  const fit = out.devices[dev].fit;
  for (const r of rows) {
    const q = ratio(r);
    if (q == null) continue;
    r.est.vs_base_pct = r2((q / shift - 1) * 100);
    const extra = estMs(fit, r.wasm.render_ms_avg) - estMs(fit, r.wasm.render_ms_avg / (q / shift));
    if (on && r.est.vs_base_pct > B.regression_pct && extra > (B.regression_min_ms ?? 0.5)) r.over.push(`regression +${r.est.vs_base_pct} % vs baseline`);
  }
  log(`[${dev}] run vs baseline: ${out.devices[dev].baseline_shift_pct} % overall (host load), divided out`);
}
out.run_s = r2((performance.now() - t0) / 1000);
const o = arg('--out');
if (o) writeFileSync(o, JSON.stringify(out, null, 1));
else process.stdout.write(JSON.stringify(out));
log(`${out.results.length} results, ${out.results.filter((r) => r.over.length).length} over budget, ${out.run_s} s`);
