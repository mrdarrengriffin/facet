// The rules pass of tools/functional.mjs (--rules): one loop over components x variants x densities x themes x looks,
// one mount per case, every rule that wants that case run on it in order (plan Phase 2.1). A rule is a module in
// tests/rules/<rule>.mjs:
//   export default { id, about, tiers: ['fast', 'gate'], wants(c, opt) -> bool, themes(opt) -> 'facet' | 'all',
//                    looks: [0, 1] (built dark, built light), order (low first; a rule that changes the mount goes last),
//                    cases: 'all' (default + meta variants) | 'variants' (the variants, or default when there are none),
//                    ownStage: true (it renders on stages of its own, on a module of its own per component: what it
//                      leaves in the module, an unclipped display or a live look switch, reaches no other rule),
//                    cache: true (its results per component are kept while the component's inputs are unchanged),
//                    dens: ['1.5x'] (only those densities; default every density of the run),
//                    run(x, r), finish({ build, dens }) (optional, once after the whole run: a report of its own) }
// x = the mount: { m, st, W, H, den, D, theme, c (meta entry), v (variant), vi (its index), light, tick, settle,
//   test(cmd, a), frame565() }; for an ownStage rule only { m, den, theme, c, v, vi, light }.
// r = the report: r.check(ok, line), r.legacy(line), r.note(line). A rule never builds its own module or loops itself.
// A startup check (no component loop) has once(x, r) instead of wants / run: x = { m (a fresh module), den, build },
// run once per density in its dens (default every density of the run).
// Cache ($FW_CACHE/rules-<den>.json): per cached rule, theme and component, the results and the component's input hash
// (core + the charts library, tokens, specs, the rules and this runner, the component's file and the files of the
// components it builds with fw::find or names in a "// audit deps: a, b" comment); a component whose hash is unchanged
// is replayed, not mounted. In the fast tier every component rule is cached (fast = the changed components only).
// fresh = ignore it.
import { readFileSync, writeFileSync, existsSync, readdirSync } from 'node:fs';
import { dirname, join, resolve, basename } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
import { instance } from './fwbench.mjs';

const TICK = 16;
const HERE = resolve(dirname(fileURLToPath(import.meta.url)), '../..');

// ---------------------------------------------------------------- inputs hash (cache key per component)
const sha = (s) => createHash('sha1').update(s).digest('hex').slice(0, 12);
const ls = (d, ext) => (existsSync(join(HERE, d)) ? readdirSync(join(HERE, d)).filter((f) => f.endsWith(ext)).sort().map((f) => join(d, f)) : []);
const read = (f) => readFileSync(join(HERE, f));
let hashOf = null;
function compHash(id) {
  if (!hashOf) {
    const coreHash = sha(Buffer.concat([...ls('core', '.h'), ...ls('core', '.cpp'), 'foundations/chart.cpp', ...ls('tokens', '.json'), ...ls('tests/rules', '.mjs'), 'tools/lib/rules.mjs'].flatMap((f) => [Buffer.from(f), read(f)])));
    const srcOf = {};
    for (const d of ['components', 'screens', 'foundations']) for (const f of ls(d, '.cpp')) srcOf[basename(f, '.cpp')] = f;
    const SPEC_FILES = ls('specs', '.json');
    const SPEC_COMPS = [...new Set(SPEC_FILES.flatMap((f) => [...read(f).toString().matchAll(/"c":\s*"([a-z_0-9]+)"/g)].map((m) => m[1])))];
    const specHash = sha(Buffer.concat(SPEC_FILES.flatMap((f) => [Buffer.from(f), read(f)])));
    const deps = (c, seen = new Set()) => {
      if (seen.has(c) || !srcOf[c]) return seen;
      seen.add(c);
      const src = read(srcOf[c]).toString();
      for (const m of src.matchAll(/fw::find\("([a-z_0-9]+)"\)/g)) deps(m[1], seen);
      // a file that calls another component's code directly (not through fw::find) says so: "// audit deps: a, b"
      for (const m of src.matchAll(/\/\/ audit deps: ([a-z_0-9, ]+)/g)) for (const d of m[1].split(/[, ]+/)) if (d) deps(d, seen);
      // a screen built from the factory specs (spec::build: the Spec screen, Device, Home) depends on every component the
      // specs name (2026-10-23: the Calendar spec kept a stale audit after calendar_view changed)
      if (/spec::build\(|spec::FACTORY/.test(src)) for (const k of SPEC_COMPS) deps(k, seen);
      return seen;
    };
    hashOf = (c) => sha(coreHash + specHash + [...deps(c)].sort().map((d) => d + sha(read(srcOf[d]))).join());
  }
  return hashOf(id);
}

export function mount(m, den, c, v, light) {  // the case's stage, sized as the workbench shows it (meta "frame")
  const D = den === '1x' ? 1 : 1.5, SS = den === '1x' ? 480 : 720, [fw, fh] = c.meta.frame || [SS / D, SS / D];
  const W = Math.round(fw * D), H = fh ? Math.round(fh * D) : SS;
  let t = m.__t || 0;
  const S = (s, f) => { const p = m.stringToNewUTF8(s); try { return f(p); } finally { m._free(p); } };
  const x = {
    m, W, H, den, D, c, v, light,
    tick(n = 1) { for (let i = 0; i < n; i++) { t += TICK; m._fw_tick(t); } m._fw_events(); m.__t = t; },
    settle() { for (let i = 0, calm = 0; i < 120 && calm < 3; i++) { x.tick(); calm = m._fw_anims() ? 0 : calm + 1; } },
    test: (cmd, a = '') => S(cmd, (pc) => S(a, (pa) => m.UTF8ToString(m._fw_test(pc, pa)))),
    frame565() {  // the frame as the panel shows it (RGB565), after a refresh
      m._fw_refresh(x.st);
      const p = m._fw_frame(x.st), h = m.HEAPU8, w = new Uint16Array(W * H);
      for (let i = 0; i < W * H; i++) { const q = p + i * 4; w[i] = ((h[q] >> 3) << 11) | ((h[q + 1] >> 2) << 5) | (h[q + 2] >> 3); }
      return w;
    },
  };
  x.st = m._fw_stage(W, H);
  x.tick(2);
  x.err = S(c.id, (pc) => S(JSON.stringify(v.props || {}), (pp) => m.UTF8ToString(m._fw_render(x.st, pc, pp, light, 0))));
  if (!x.err) x.settle();
  x.free = () => m._fw_stage_free(x.st);
  return x;
}

export async function runRules(rules, { build, dens, ids, themeArg, tier, cacheDir, fresh }) {
  const res = Object.fromEntries(rules.map((r) => [r.id, { checks: 0, fails: 0, legacy: 0, out: [] }]));
  const report = (rule, rec) => {
    const s = res[rule.id];
    const put = (k, line) => {
      if (rec) rec.push([k, line]);
      if (k === 'ok') s.checks++;
      else if (k === 'fail') { s.checks++; s.fails++; s.out.push(`FAIL ${line}`); }
      else if (k === 'legacy') { s.checks++; s.legacy++; s.out.push(`LEGACY ${line}`); }
      else s.out.push(line);
    };
    return { check: (ok, line) => put(ok ? 'ok' : 'fail', line), legacy: (line) => put('legacy', line), note: (line) => put('note', line), replay: (recs) => recs.forEach(([k, line]) => put(k, line)) };
  };
  const opt = { ids, tier, themeArg };
  for (const rule of rules.filter((r) => r.once)) for (const den of dens.filter((d) => !rule.dens || rule.dens.includes(d)))
    await rule.once({ m: await instance(den, build), den, build }, report(rule));
  rules = rules.filter((r) => !r.once);
  for (const den of dens) {
    const cpath = cacheDir && join(cacheDir, `rules-${den}.json`);
    const cache = cpath && !fresh && existsSync(cpath) ? JSON.parse(readFileSync(cpath, 'utf8')) : {};
    let m = await instance(den, build);
    const metas = JSON.parse(m.UTF8ToString(m._fw_meta())).map((c) => ({ ...c, meta: typeof c.meta === 'string' ? JSON.parse(c.meta) : c.meta }));
    const builtins = ['facet', ...JSON.parse(m.UTF8ToString(m._fw_lab(-1, m.stringToNewUTF8('theme_builtins'), m.stringToNewUTF8('')))).map((x) => x.id).filter((x) => x !== 'facet')];
    const themesOf = (rule) => { const t = rule.themes ? rule.themes(opt) : 'facet'; return t === 'all' ? builtins : t.split(','); };
    const themes = [...new Set(rules.flatMap(themesOf))];
    for (const theme of themes) for (const c of metas) {
      let here = rules.filter((r) => (!r.dens || r.dens.includes(den)) && themesOf(r).includes(theme) && r.wants(c, opt)).sort((a, z) => (a.order || 0) - (z.order || 0));
      if (!here.length) continue;
      // cached rules: a component whose inputs are unchanged is replayed; the ones that run are recorded
      const cached = (r) => r.cache || tier === 'fast';
      const key = here.some(cached) ? compHash(c.id) : null, rec = {};
      here = here.filter((r) => {
        if (!cached(r)) return true;
        const k = `${r.id}|${theme}|${c.id}`, hit = cache[k];
        if (hit && hit.key === key) { report(r).replay(hit.recs); return false; }
        rec[r.id] = []; cache[k] = { key, recs: rec[r.id] };
        return true;
      });
      if (!here.length) continue;
      // a fresh module per component (one per render runs the heap out): one for the shared mount, one per ownStage rule
      const mods = new Map();
      let refused = null;
      for (const key of ['', ...here.filter((r) => r.ownStage).map((r) => r.id)]) {
        const mm = await instance(den, build);
        if (theme !== 'facet') {
          const r = JSON.parse(mm.UTF8ToString(mm._fw_lab(-1, mm.stringToNewUTF8('theme_use'), mm.stringToNewUTF8(theme))));
          if (!r.ok) refused = r;
        }
        mods.set(key, mm);
      }
      m = mods.get('');
      if (refused) { for (const rule of here) report(rule, rec[rule.id]).check(false, `theme ${theme}: refused ${JSON.stringify(refused.errors).slice(0, 200)}`); continue; }
      const vs = c.meta.variants || [];
      for (const [vi, v] of [{ label: 'default', props: {} }, ...vs].entries()) {
        for (const light of [0, 1]) {
          const mine = here.filter((r) => (r.looks || [0, 1]).includes(light) && !(r.cases === 'variants' && vs.length && vi === 0));
          if (!mine.length) continue;
          const shared = mine.some((r) => !r.ownStage);
          const x = shared ? mount(m, den, c, v, light) : null;
          if (x?.err) { for (const rule of mine.filter((r) => !r.ownStage)) report(rule, rec[rule.id]).note(`skip ${den} ${c.id} "${v.label}": ${x.err}`); }
          for (const rule of mine) {
            if (!rule.ownStage && x?.err) continue;
            rule.run(rule.ownStage ? { m: mods.get(rule.id), den, c, v, vi, light, theme } : Object.assign(x, { theme, vi }), report(rule, rec[rule.id]));
          }
          if (shared) x.free();
        }
      }
    }
    if (cpath) try { writeFileSync(cpath, JSON.stringify(cache)); } catch {}
  }
  for (const rule of rules.filter((r) => r.finish)) await rule.finish({ build, dens });
  return res;
}
