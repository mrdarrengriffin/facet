// Layout lab test (hub src/wasm/lablayout.ts, core/main.cpp fw_lab): every size token the Card uses repaints a
// mounted Card LIVE (fx::tune -> shared token styles -> every stage), and the page-level tokens are reported "page
// only" (a component mounted alone has no Page). Darren 2026-10-26: "changed every lab setting, nothing changed".
//   node tools/lab_test.mjs [--build <FW_OUT>]
import { existsSync, readFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const HERE = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const env = (() => { try { return Object.fromEntries(readFileSync(join(HERE, '..', 'local.env'), 'utf8').split('\n').map((l) => l.match(/^([A-Z_]+)=(.*)$/)).filter(Boolean).map((m) => [m[1], m[2]])); } catch { return {}; } })();
const i = process.argv.indexOf('--build');
const BUILD = i > 0 ? process.argv[i + 1] : process.env.FW_OUT || env.FW_OUT || join(HERE, 'out');
const CARD = ['layout:card-pad', 'radius:card', 'stroke:hair'];   // must repaint the Card
const PAGE = ['layout:inset', 'layout:gutter', 'layout:top', 'layout:header-gap', 'squeeze'];  // page only
// each mounted contract component: the tokens that must repaint it live, the ones that must not be listed for it
const MOUNTS = [['card', { opening: false }, CARD, PAGE],
  ['cells', { cells: 'c,c,c,c,c' }, ['layout:gutter', 'layout:tile-w', 'layout:tile-h', ...CARD], ['layout:inset', 'layout:top', 'layout:header-gap', 'squeeze']]];
let fails = 0;
const ok = (cond, what) => { if (!cond) fails++; if (!cond || process.argv.includes('-v')) console.log(`${cond ? 'pass' : 'FAIL'}  ${what}`); };
// a density not built yet (a fresh worktree's first 1.5x build) is skipped, not an import error
for (const den of ['1x', '1.5x'].filter((d) => existsSync(join(BUILD, d, 'fw.mjs')))) {
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
  for (const n of MUST) ok(listed.includes(n), `${den} ${id} ${n}: listed`);
  for (const n of NOT) ok(!listed.includes(n), `${den} ${id} ${n}: page only, not listed`);
  if (quiet.length) console.log(`note  ${den} ${id} read but no visible change here: ${quiet.join(', ')}`);
  // live: tune each listed token, let the frame loop run, read the pixels (no re-render: the shared styles carry it)
  for (const n of listed.filter((x) => x !== 'squeeze')) {
    const [, , now, kind, lo, hi] = toks.get(n);
    // a fixed size: up by half, else down by a quarter (a floor such as a Cells min-w shows only when it crosses a column)
    const vs = kind === 'look' ? [now < hi ? hi : lo] : [now + Math.max(4, now >> 1), Math.max(4, now - Math.max(4, now >> 2))];
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
}
console.log(fails ? `lab_test: ${fails} failed` : 'lab_test: all passed');
process.exit(fails ? 1 : 0);
