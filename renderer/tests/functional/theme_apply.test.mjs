// Theme apply path (core/fx_core.cpp: the live Skin -> every display's shared styles, part:* tokens, the Pen's drawn
// parts; renderer/CONTRACT.md C44-C47, theme engine design T2). Every contract component and the Light screen, each
// built-in theme, dark + light, at this density. Each test fails without the behaviour it names:
//   S1     Facet is a no-op: back to Facet after another theme = the frame before, every RGB565 pixel (and every
//          existing golden of the contract components stays as it is: their own suites, run unchanged)
//   P4     a theme switched LIVE (fx::restyle, no rebuild, no component told) = a fresh build in that theme, every pixel
//   reach  each theme changes each component (its parts drawn: edges, faces, knob / tick shapes, text inks)
//   refuse a refused file leaves the live look exactly as it was (pixels + generation)
//   break  with the repaint switched off (fx::break_stale) the live = fresh proof goes red: the proof can fail
//   golden one candidate per component x theme x mode (tests/golden/theme): reviewed by a person, never auto-accepted
import { frame565, diff565, golden, stage } from './_release.mjs';

const lab = (t, c, a = '') => JSON.parse(t.b.str(c, (pc) => t.b.str(a, (pa) => t.b.m.UTF8ToString(t.b.m._fw_lab(-1, pc, pa)))));
const THEMES = ['blockcraft', 'win98'];
const E = 'light.test_lamp', LAMP = { '': 'on', brightness: '179', friendly_name: 'Desk lamp', supported_color_modes: '["brightness"]' };
const M = ['placeholder', { name: 'Theme' }];  // any mount: the module the tests talk to
const den = (t) => (t.b.S > 600 ? '1.5x' : '1x');
// every contract component (catalogue meta "contract": 2) with its first two variants, plus the Light screen
function cases(t) {
  const metas = JSON.parse(t.b.m.UTF8ToString(t.b.m._fw_meta())).map((c) => ({ ...c, meta: typeof c.meta === 'string' ? JSON.parse(c.meta) : c.meta }));
  const D = t.b.S > 600 ? 1.5 : 1, out = [];
  for (const c of metas.filter((x) => x.section !== 'screens' && x.meta && x.meta.contract === 2)) {
    const [fw, fh] = c.meta.frame || [t.b.S / D, t.b.S / D];
    for (const v of (c.meta.variants?.length ? c.meta.variants : [{ label: 'default', props: {} }]).slice(0, 2))
      out.push({ id: c.id, name: `${c.id}-${out.filter((o) => o.id === c.id).length}`, label: v.label, props: v.props || {}, W: Math.round(fw * D), H: fh ? Math.round(fh * D) : t.b.S });
  }
  out.push({ id: 'spec', name: 'light_screen', label: 'Light screen', props: { screen: 'light', entity: E, opening: false }, W: t.b.S, H: t.b.S, screen: true });
  return out;
}
// a fresh build of case c with `theme` live BEFORE it is built (t.b.render = a new module: the theme is made live in it,
// then the case built again in that module)
async function build(t, c, light, theme) {
  t.b.W = t.b.H = undefined;
  if (c.screen) t.ha.s[E] = { ...LAMP };
  await t.b.render('placeholder', { name: 'Theme' }, light);
  use(t, theme);
  t.b.rebuild(c.id, c.props, light);
  stage(t, c.W, c.H); t.b.settle(1500);
  if (c.screen) { t.ha.push(); t.b.settle(1500); }
}
const use = (t, id) => { const r = lab(t, 'theme_use', id); if (!r.ok) throw new Error(`theme ${id} refused: ${JSON.stringify(r.errors)}`); t.b.tick(2); t.b.settle(800); return r; };

export default {
  id: 'theme_apply', family: 'foundations', about: 'Themes applied live to every contract component: Facet no-op, live = fresh, reach, refusals, break-it, golden candidates.',
  tests: [
    { name: 'S1: back to Facet after each built-in theme = the frame before it, every pixel (dark + light)', mount: M, async run(t) {
      const out = [];
      for (const c of cases(t)) for (const light of [0, 1]) {
        await build(t, c, light, 'facet'); const f0 = frame565(t);
        for (const th of THEMES) { use(t, th); use(t, 'facet'); const n = diff565(f0, frame565(t)); if (n) out.push(`${c.name} ${light ? 'light' : 'dark'} via ${th}: ${n} px`); }
      }
      t.eq(out.slice(0, 6), [], 'facet = before');
    } },
    { name: 'P4: each built-in theme switched live from Facet = a fresh build in it, every pixel (dark + light)', mount: M, async run(t) {
      const out = [];
      for (const th of THEMES) for (const c of cases(t)) for (const light of [0, 1]) {
        await build(t, c, light, 'facet'); use(t, th); const live = frame565(t);
        await build(t, c, light, th); const n = diff565(live, frame565(t));
        if (n) out.push(`${th} ${c.name} ${light ? 'light' : 'dark'}: ${n} px differ from a fresh build`);
      }
      use(t, 'facet');
      t.eq(out.slice(0, 6), [], 'live = fresh');
    } },
    { name: 'P4: one theme to another live (blockcraft -> win98 -> blockcraft) = a fresh build, every pixel', mount: M, async run(t) {
      const out = [];
      for (const c of cases(t)) {
        await build(t, c, 0, 'blockcraft'); use(t, 'win98'); use(t, 'blockcraft'); const live = frame565(t);
        await build(t, c, 0, 'blockcraft'); const n = diff565(live, frame565(t)); if (n) out.push(`${c.name}: ${n} px`);
      }
      use(t, 'facet');
      t.eq(out.slice(0, 6), [], 'A -> B -> A live = fresh A');
    } },
    { name: 'P2 + P6 per theme: no object outside its parent and no text cut that Facet does not cut (theme fonts and sizes included)', mount: M, async run(t) {
      const out = [];
      const proof = (t) => JSON.parse(t.b.str('proof', (c) => t.b.str('', (a) => t.b.m.UTF8ToString(t.b.m._fw_lab(t.b.stage, c, a)))));
      for (const c of cases(t)) {
        await build(t, c, 0, 'facet');
        const f = proof(t), cut = new Set(f.trunc.map((x) => x.why.replace(/ needs .*/, '')));
        for (const th of THEMES) {
          await build(t, c, 0, th);
          const p = proof(t);
          for (const b of p.bounds.slice(0, 2)) out.push(`${th} ${c.name}: ${b.why}`);
          for (const x of p.trunc) if (!cut.has(x.why.replace(/ needs .*/, ''))) out.push(`${th} ${c.name}: ${x.why}`);
        }
      }
      use(t, 'facet');
      t.eq(out.slice(0, 8), [], 'bounds and fit as Facet');
    } },
    { name: 'reach: every theme changes every contract component and the Light screen', mount: M, async run(t) {
      const out = [];
      for (const c of cases(t)) {
        await build(t, c, 0, 'facet'); const f0 = frame565(t);
        for (const th of THEMES) { use(t, th); if (!diff565(f0, frame565(t))) out.push(`${c.name} unchanged in ${th}`); use(t, 'facet'); }
      }
      t.eq(out, [], 'themed');
    } },
    { name: 'parts drawn: the card wears blockcraft\'s outer edge (#6C6C76) and win98\'s header its gradient', mount: M, async run(t) {
      const D = t.b.S > 600 ? 1.5 : 1, at = (x, y) => frame565(t)[y * t.b.W + x];
      const w565 = (h) => (((h >> 19) & 31) << 11) | (((h >> 10) & 63) << 5) | ((h >> 3) & 31);
      await build(t, { id: 'card', props: { label: 'Weather', icon: 'weather', value: '13°' }, W: Math.round(400 * D), H: Math.round(300 * D) }, 0, 'blockcraft');
      const k = t.part('card');
      t.eq(at(k.x, k.y + (k.h >> 1)), w565(0x6c6c76), 'card outer edge, left side');
      t.eq(at(k.x + k.w - 1, k.y + (k.h >> 1)), w565(0x6c6c76), 'card outer edge, right side');
      await build(t, { id: 'header', props: { label: 'Lights', time: '21:42' }, W: t.b.S, H: Math.round(160 * D) }, 1, 'win98');
      const h = t.part('header'), y = h.y + (h.h >> 1);
      t.ok(at(h.x + 2, y) !== at(h.x + h.w - 3, y), 'gradient: left != right');
      use(t, 'facet');
    } },
    { name: 'refused: a file the guards refuse leaves the live look as it was (every pixel, same generation)', mount: M, async run(t) {
      const c = cases(t).find((x) => x.id === 'card'); await build(t, c, 0, 'win98'); const f0 = frame565(t), g0 = lab(t, 'theme_live').gen;
      for (const bad of ['{"schema": 1, "id": "half", "colour": {"bg": {"light": "#FF0000"}}, "parts": {"card": {"glow": 1}}}',
        '{"schema": 1, "id": "low", "colour": {"text": {"dark": "#111111", "light": "#EEEEEE"}}}', '{"schema": 1, "id": "cut"']) {
        const r = lab(t, 'theme_apply', bad); t.b.tick(2); t.b.settle(300);
        t.eq([r.ok, diff565(f0, frame565(t)), lab(t, 'theme_live').gen], [false, 0, g0], `refused ${bad.slice(0, 40)}`);
      }
      use(t, 'facet');
    } },
    { name: 'break-it: with the repaint off (fx::break_stale) a live switch differs from a fresh build', mount: M, async run(t) {
      const c = cases(t).find((x) => x.id === 'card');
      await build(t, c, 0, 'facet'); lab(t, 'theme_break', 'stale'); use(t, 'blockcraft'); const live = frame565(t);
      lab(t, 'theme_break', ''); await build(t, c, 0, 'blockcraft');
      t.ok(diff565(live, frame565(t)) > 0, 'the stale look shows: the proof can fail');
      use(t, 'facet');
    } },
    { name: 'golden candidates: every contract component + the Light screen in each built-in theme (dark + light), reviewed', mount: M, async run(t) {
      const out = [];
      for (const th of THEMES) for (const c of cases(t)) for (const light of [0, 1]) {
        await build(t, c, light, th);
        const mask = (c.screen || c.id === 'header') ? (() => {  // the live clock (a screen's header, the Header itself)
           const k = t.part('time'), h = t.part('header'), g = den(t) === '1x' ? 8 : 12; return k && h ? [{ x: k.x - g, y: h.y, w: k.w + 2 * g, h: h.h }] : []; })() : [];  // the whole time (a theme font can be wider)
        out.push(...golden(t, 'theme', `${th}-${c.name}-${light ? 'light' : 'dark'}`, den(t), mask));
      }
      use(t, 'facet');
      t.eq(out.slice(0, 4), [], `goldens (${out.length} awaiting review)`);
    } },
  ],
};
