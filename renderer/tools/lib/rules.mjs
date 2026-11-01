// The rules pass of tools/functional.mjs (--rules): one loop over components x variants x densities x themes x looks,
// one mount per case, every rule that wants that case run on it in order (plan Phase 2.1). A rule is a module in
// tests/rules/<rule>.mjs:
//   export default { id, about, tiers: ['fast', 'gate'], wants(c, opt) -> bool, themes(opt) -> 'facet' | 'all',
//                    looks: [0, 1] (built dark, built light), order (low first; a rule that changes the mount goes last),
//                    run(x, r) }
// x = the mount: { m, st, W, H, den, D, theme, c (meta entry), v (variant), light, tick, settle, test(cmd, a), frame565() }.
// r = the report: r.check(ok, line), r.legacy(line), r.note(line). A rule never builds its own module or loops itself.
// A startup check (no component loop) has once(x, r) instead of wants / run: x = { m (a fresh module), den, build },
// run once per density in its dens (default every density of the run).
import { instance } from './fwbench.mjs';

const TICK = 16;

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

export async function runRules(rules, { build, dens, ids, themeArg, tier }) {
  const res = Object.fromEntries(rules.map((r) => [r.id, { checks: 0, fails: 0, legacy: 0, out: [] }]));
  const report = (rule) => {
    const s = res[rule.id];
    return {
      check(ok, line) { s.checks++; if (!ok) { s.fails++; s.out.push(`FAIL ${line}`); } },
      legacy(line) { s.checks++; s.legacy++; s.out.push(`LEGACY ${line}`); },
      note(line) { s.out.push(line); },
    };
  };
  const opt = { ids, tier, themeArg };
  for (const rule of rules.filter((r) => r.once)) for (const den of dens.filter((d) => !rule.dens || rule.dens.includes(d)))
    await rule.once({ m: await instance(den, build), den, build }, report(rule));
  rules = rules.filter((r) => !r.once);
  for (const den of dens) {
    let m = await instance(den, build);
    const metas = JSON.parse(m.UTF8ToString(m._fw_meta())).map((c) => ({ ...c, meta: typeof c.meta === 'string' ? JSON.parse(c.meta) : c.meta }));
    const builtins = ['facet', ...JSON.parse(m.UTF8ToString(m._fw_lab(-1, m.stringToNewUTF8('theme_builtins'), m.stringToNewUTF8('')))).map((x) => x.id).filter((x) => x !== 'facet')];
    const themesOf = (rule) => { const t = rule.themes ? rule.themes(opt) : 'facet'; return t === 'all' ? builtins : t.split(','); };
    const themes = [...new Set(rules.flatMap(themesOf))];
    for (const theme of themes) for (const c of metas) {
      const here = rules.filter((r) => themesOf(r).includes(theme) && r.wants(c, opt)).sort((a, z) => (a.order || 0) - (z.order || 0));
      if (!here.length) continue;
      m = await instance(den, build);  // a fresh module per component (one per render runs the heap out)
      if (theme !== 'facet') {
        const r = JSON.parse(m.UTF8ToString(m._fw_lab(-1, m.stringToNewUTF8('theme_use'), m.stringToNewUTF8(theme))));
        if (!r.ok) { for (const rule of here) report(rule).check(false, `theme ${theme}: refused ${JSON.stringify(r.errors).slice(0, 200)}`); continue; }
      }
      for (const v of c.meta.variants?.length ? c.meta.variants : [{ label: 'default', props: {} }]) {
        for (const light of [0, 1]) {
          const mine = here.filter((r) => (r.looks || [0, 1]).includes(light));
          if (!mine.length) continue;
          const x = mount(m, den, c, v, light);
          x.theme = theme;
          if (x.err) { for (const rule of mine) report(rule).note(`skip ${den} ${c.id} "${v.label}": ${x.err}`); x.free(); continue; }
          for (const rule of mine) rule.run(x, report(rule));
          x.free();
        }
      }
    }
  }
  return res;
}
