// Layout lab test (hub src/wasm/lablayout.ts, core/main.cpp fw_lab): every size token the Card uses repaints a
// mounted Card LIVE (fx::tune -> shared token styles -> every stage), and the page-level tokens are reported "page
// only" (a component mounted alone has no Page). Darren 2026-10-26: "changed every lab setting, nothing changed".
//   node tools/lab_test.mjs [--build <FW_OUT>]
import { existsSync, readFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

// parts whose whole look is a type style + a colour (no size of their own): nothing to tune, the check passes them
// (Darren 2026-10-28: Title, the Header's text)
const TEXT_ONLY = new Set(['title']);
const HERE = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const env = (() => { try { return Object.fromEntries(readFileSync(join(HERE, '..', 'local.env'), 'utf8').split('\n').map((l) => l.match(/^([A-Z_]+)=(.*)$/)).filter(Boolean).map((m) => [m[1], m[2]])); } catch { return {}; } })();
const i = process.argv.indexOf('--build');
const BUILD = i > 0 ? process.argv[i + 1] : process.env.FW_OUT || env.FW_OUT || join(HERE, 'out');
const CARD = ['layout:card-pad', 'radius:card', 'stroke:hair'];   // must repaint the Card
const PAGE = ['layout:inset', 'layout:gutter', 'layout:top', 'layout:header-gap', 'squeeze'];  // page only
// Header round 3 (2026-10-27): EVERY contract component (meta "contract": 2) is mounted and must list >= 1 tunable token
// that visibly changes it, and each listed token must repaint it live and come back (C23 / P3 for all). Some have
// extra must / must-not lists (below); props = what the mount needs to show the parts that read the tokens.
// layout:row: read, but alone on a stage the row is centred (only a Page's head shows it): header.test.mjs checks it
const HEADER = ['layout:back-gap', 'layout:header-item-gap'];
const EXTRA = { card: [{ opening: false }, CARD, PAGE],
  cells: [{ cells: 'c,c,c,c,c' }, ['layout:gutter', 'layout:tile-w', 'layout:tile-h', ...CARD], ['layout:inset', 'layout:top', 'layout:header-gap', 'squeeze']],
  header: [{ label: 'Weather', settings: true, sample: 'end-buttons' }, HEADER, PAGE],
  icon_button: [{ style: 'square', icon: 'plus' }, ['layout:ibtn-square', 'radius:square', 'stroke:hair'], PAGE],
  section_header: [{ title: 'Needs you', count: 3, meta: 'updated 2m' }, ['space:5'], PAGE],
  status_chip: [{}, ['layout:chip-h', 'radius:chip', 'space:2'], PAGE],
  time: [{ time: '14:52', format: '12h' }, ['space:2'], PAGE],
  stat: [{ label: 'lit', value: '63%', tone: 'text' }, ['space:2'], PAGE],
  // a whole screen (meta stage: screen): its own Page, so the page tokens are its own
  bottom_sheet: [{}, ['layout:inset', 'radius:ring'], []],
  toast_layer: [{ timer: false }, ['layout:inset', 'layout:toast-h', 'layout:toast-pad', 'layout:toast-top'], []],
  toast: [{ timer: false }, ['layout:toast-h', 'layout:toast-pad', 'stroke:edge'], PAGE],
  // later bars are outlines (stroke:hair); still (opening false: no growth while it is probed); every drawn size is a
  // chart:* token (chart.json geometry), so the lab has a slider for each
  bar_chart: [{ colour: 'series', opening: false }, ['stroke:hair', 'chart:bar-gap', 'chart:bar-radius', 'chart:plot-top', 'chart:plot-bottom', 'chart:grid'], PAGE],
  rate_strip: [{ ticks: 6 }, ['chart:strip-h', 'chart:bar-radius', 'stroke:edge', 'space:1'], PAGE],
  status_line: [{}, ['space:2'], PAGE],
  bins_art: [{ colours: 'black,green' }, ['layout:bin-hero', 'stroke:edge'], PAGE],
  sparkline: [{ opening: false }, ['stroke:hair', 'chart:spark-h', 'chart:line-thin', 'chart:dot'], PAGE],  // stroke:hair = the dotted baseline
  // a layout whose parts sit a gutter apart (the gutter is its own, not the page's)
  group: [{ sample: 4 }, ['layout:gutter'], PAGE.filter((n) => n !== 'layout:gutter')],
  grid: [{}, ['layout:gutter'], PAGE.filter((n) => n !== 'layout:gutter')],
  hero_block: [{ n: '3', label: 'need you', line: '2 reviews' }, ['layout:gutter', 'space:2'], PAGE.filter((n) => n !== 'layout:gutter')],
  // the ring + its centre slot (a sample Hero block in it shows the hole's width); the chip: its ring, pad, gap, edge
  progress_ring: [{ value: 272, max: 600, sample: 'running' }, ['layout:timer-ring', 'layout:timer-ring-hole', 'stroke:arc', 'stroke:indicator'], PAGE],
  timer_chip: [{}, ['layout:timer-chip-ring', 'touch:min', 'space:3', 'radius:chip', 'stroke:hair', 'stroke:edge', 'stroke:arc'], PAGE] };
// the wall clock pinned (a screen's Header shows the live time: a minute ticking over between a tune and its "and back"
// changed the pixels, a flaky fail, 2026-10-28); the module reads it through Date.now (emscripten_date_now)
const PINNED = Date.UTC(2026, 9, 28, 9, 41, 0);
Date.now = () => PINNED;
const di = process.argv.indexOf('--den');
const m0 = await (await import(pathToFileURL(join(BUILD, di > 0 ? process.argv[di + 1] : ['1.5x', '1x'].find((d) => existsSync(join(BUILD, d, 'fw.mjs'))) || '1.5x', 'fw.mjs')).href)).default();
m0._fw_init();
const META = JSON.parse(m0.UTF8ToString(m0._fw_meta())).map((c) => ({ ...c, meta: typeof c.meta === 'string' ? JSON.parse(c.meta) : c.meta }));
const CONTRACT = META.filter((c) => c.section !== 'screens' && c.meta?.contract === 2).map((c) => c.id);
const MOUNTS = CONTRACT.map((id) => [id, ...(EXTRA[id] || [{}, [], PAGE])]);
let fails = 0;
const ok = (cond, what) => { if (!cond) fails++; if (!cond || process.argv.includes('-v')) console.log(`${cond ? 'pass' : 'FAIL'}  ${what}`); };
// a density not built yet (a fresh worktree's first 1.5x build) is skipped, not an import error
for (const den of (di > 0 ? [process.argv[di + 1]] : ['1x', '1.5x']).filter((d) => existsSync(join(BUILD, d, 'fw.mjs')))) {
  const m = await (await import(pathToFileURL(join(BUILD, den, 'fw.mjs')).href)).default();
  m._fw_init();
  const W = 480, H = 360, st = m._fw_stage(W, H);
  let t = 0;
  const tick = (n = 4) => { for (let k = 0; k < n; k++) { t += 16; m._fw_tick(t); } };
  const S = (p, f) => { const q = m.stringToNewUTF8(p); try { return f(q); } finally { m._free(q); } };
  const lab = (c, a = '', id = -1) => S(c, (pc) => S(a, (pa) => m.UTF8ToString(m._fw_lab(id, pc, pa))));
  const hash = () => { const px = new Uint32Array(m.HEAPU8.buffer, m._fw_frame(st), W * H); let h = 0; for (let k = 0; k < px.length; k++) h = (Math.imul(h, 31) + px[k]) | 0; return h; };
  for (const [id, props, MUST, NOT] of MOUNTS) {
  S(id, (pc) => S(JSON.stringify(props), (pp) => m._fw_render(st, pc, pp, 0, -1)));
  tick(30);
  const toks = new Map(JSON.parse(lab('tokens')).map((r) => [r[0], r]));
  const base = hash();
  // the lab's list = the size tokens the mounted component's LVGL tree reads (styles + pen) that visibly change it (probe)
  const uses = JSON.parse(lab('uses', '', st)), seen = JSON.parse(lab('probe', '', st));
  tick();
  ok(hash() === base, `${den} ${id} uses + probe leave the mounted component as it was`);
  const listed = uses.filter((n) => seen[n] === 1), quiet = uses.filter((n) => seen[n] !== 1);
  ok(TEXT_ONLY.has(id) || listed.filter((n) => n !== 'squeeze').length >= 1, `${den} ${id} lists >= 1 tunable token that visibly changes it (${listed.join(', ') || 'none'})`);
  for (const n of MUST) ok(listed.includes(n), `${den} ${id} ${n}: listed`);
  for (const n of NOT) ok(!listed.includes(n), `${den} ${id} ${n}: page only, not listed`);
  if (quiet.length) console.log(`note  ${den} ${id} read but no visible change here: ${quiet.join(', ')}`);
  // live: tune each listed token, let the frame loop run, read the pixels (no re-render: the shared styles carry it)
  for (const n of listed.filter((x) => x !== 'squeeze')) {
    const [, , now, kind, lo, hi] = toks.get(n);
    // a fixed size: up by half, else down by a quarter (a floor such as a Cells min-w shows only when it crosses a column)
    const vs = kind === 'look' ? [now < hi ? hi : lo] : [now + Math.max(4, now >> 1), Math.max(4, now - Math.max(4, now >> 2)), Math.max(4, (now >> 3) << 2), now * 3, Math.max(0, now - Math.max(4, now >> 1))];  // the probe's own down step last (a size capped above: a 4 px bar radius on a narrow bar shows only going down); up by half, down a quarter, half (a cap such as a max width shows only below the stage), x3 (a grid-rounded size can land on the same step at x1.5)
    let moved = '';
    for (const v of vs) {
      lab('tune', `${n}=${v}`); tick();
      if (!moved && hash() !== base) moved = `${v}`;
      lab('tune', `${n}=`); tick();
      ok(hash() === base, `${den} ${id} ${n} ${v} and back: the mounted component is as before`);
    }
    ok(moved, `${den} ${id} ${n} ${now} -> ${moved || vs.join(' / ')}: repaints live`);
  }
  }
  // an update that changes nothing (the same props + one unknown key, so the update path runs) leaves every object
  // where it was, in every variant: an update path that drops a root style (2026-10-28: the Hero block's refill took
  // its box style off and it shrank to its text) fails here, whatever the audit's frame count sees
  for (const id of CONTRACT) {
    const c = META.find((x) => x.id === id);
    for (const v of [{ label: 'default', props: {} }, ...(c.meta.variants || [])]) {
      const s2 = m._fw_stage(720, 720), p = { ...v.props, opening: false, timer: false };
      S(id, (pc) => S(JSON.stringify(p), (pp) => m._fw_render(s2, pc, pp, 0, 0))); tick(60);
      const r0 = m.UTF8ToString(m._fw_rects(s2));
      S(id, (pc) => S(JSON.stringify({ ...p, _noop: 1 }), (pp) => m._fw_update(s2, pc, pp, 0, 0))); tick(60);
      ok(m.UTF8ToString(m._fw_rects(s2)) === r0, `${den} ${id} [${v.label}] a no-op update leaves every object in place`);
      m._fw_stage_free(s2);
    }
  }
}
console.log(fails ? `lab_test: ${fails} failed` : 'lab_test: all passed');
process.exit(fails ? 1 : 0);
