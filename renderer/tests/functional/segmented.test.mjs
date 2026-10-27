// Segmented (components/segmented.cpp): tap a segment = change(id), the chosen one again = tap(id); one chosen at a
// time; hug / fill widths; disabled + unavailable block; update path silent.
import { D, px, diff, ctr } from './_controls.mjs';
import { stage, frame565 } from './_release.mjs';
const O = { options: 'Auto, Dark, Light', selected: 'auto' };
const seg = (t, label) => t.objs().find((o) => o.clk && t.find(label) && o.x <= t.find(label).x && o.x + o.w >= t.find(label).x + t.find(label).w);
const plate = (t, label) => { const s = seg(t, label); return px(t, s.x + 8, s.y + s.h / 2); };  // padding, left of the label
const chosen = (t) => { const L = ['AUTO', 'DARK', 'LIGHT'].filter((l) => t.texts().includes(l)), p = L.map((l) => plate(t, l));
  return L.filter((l, i) => p.filter((q, j) => j !== i).every((q) => diff(p[i], q, 12))); };  // the one plate that differs from the rest
// centred labels (Darren 2026-10-27: the labels sat left of centre): every Segmented variant (and the Header's title
// slot one) in every built-in theme, its labels' ink (the pixels that change when the label is veiled) centred in
// its segment within 1 px. Fails when the label's text sits at the left of its grid-rounded box.
const THEMES = ['facet', 'blockcraft', 'win98'];
const fwt = (t, c, a = '') => t.b.str(c, (pc) => t.b.str(a, (pa) => t.b.m.UTF8ToString(t.b.m._fw_test(pc, pa))));
const fwlab = (t, c, a = '') => JSON.parse(t.b.str(c, (pc) => t.b.str(a, (pa) => t.b.m.UTF8ToString(t.b.m._fw_lab(-1, pc, pa)))));
async function centring(t) {
  const Dn = t.b.S > 600 ? 1.5 : 1, meta = JSON.parse(t.b.m.UTF8ToString(t.b.m._fw_meta())).find((c) => c.id === 'segmented');
  const m = typeof meta.meta === 'string' ? JSON.parse(meta.meta) : meta.meta, out = [];
  const cases = m.variants.filter((v) => v.props.state !== 'unavailable').map((v) => ({ id: 'segmented', label: v.label, props: v.props, W: Math.round(m.frame[0] * Dn), H: Math.round(m.frame[1] * Dn) }));
  cases.push({ id: 'header', label: 'header title slot', props: { label: 'Vacuum', sample: 'title-segmented' }, W: t.b.S, H: Math.round(96 * Dn) });
  for (const th of THEMES) for (const c of cases) {
    t.b.W = t.b.H = undefined; await t.b.render('placeholder', { name: 'Theme' }, 0);
    const r = fwlab(t, 'theme_use', th); if (!r.ok) { out.push(`theme ${th} refused`); continue; }
    t.b.tick(2); t.b.settle(800); t.b.rebuild(c.id, c.props, 0); stage(t, c.W, c.H); t.b.settle(1500);
    const W = t.b.W || t.b.S, objs = t.objs(), items = JSON.parse(fwt(t, 'inks') || '[]').filter((k) => k.k === 'text' && k.c === 'segmented');
    if (!items.length) out.push(`${th} ${c.label}: no labels`);
    const base = frame565(t);
    for (const it of items) {
      fwt(t, 'veil', `${it.i},1`); const v = frame565(t); fwt(t, 'veil', `${it.i},0`);
      let a = Infinity, b = -Infinity;
      for (let i = 0; i < base.length; i++) if (base[i] !== v[i]) { const x = i % W; if (x < a) a = x; if (x > b) b = x; }
      const cx = it.x + it.w / 2, cy = it.y + it.h / 2;
      const s = objs.filter((o) => o.clk && o.x <= cx && o.x + o.w >= cx && o.y <= cy && o.y + o.h >= cy).sort((p, q) => p.w - q.w)[0];
      if (!s || a === Infinity) { out.push(`${th} ${c.label} ${it.s}: ${s ? 'no ink' : 'no segment'}`); continue; }
      const d = (a + b + 1) / 2 - (s.x + s.w / 2);
      if (Math.abs(d) > 1) out.push(`${th} ${c.label} ${it.s}: ink ${a}..${b} centre off by ${d} px in ${s.x}+${s.w}`);
    }
  }
  return out;
}
export default {
  id: 'segmented', family: 'controls', about: 'Up to three segments, exactly one chosen; a tap chooses.',
  tests: [
    { name: 'shows every option in caps, the selected one chosen', mount: ['segmented', { ...O, selected: 'dark' }], run(t) {
      for (const l of ['AUTO', 'DARK', 'LIGHT']) t.has(t.texts(), l, l); t.eq(chosen(t), ['DARK'], 'chosen');
    } },
    { name: 'tap another segment: change(id) + tap sound, it becomes the chosen one', mount: ['segmented', O], run(t) {
      const ev = t.tap('LIGHT'); t.emitted(ev, 'change', 'light'); t.sounds(ev, ['tap']); t.eq(chosen(t), ['LIGHT'], 'chosen');
    } },
    { name: 'tap the chosen segment: tap(id), no change', mount: ['segmented', O], run(t) {
      const ev = t.tap('AUTO'); t.notEmitted(ev, 'change'); t.emitted(ev, 'tap', 'auto'); t.sounds(ev, ['tap']); t.eq(chosen(t), ['AUTO'], 'chosen');
    } },
    { name: 'three taps in a row: each one a change, the last one chosen', mount: ['segmented', O], run(t) {
      t.emitted(t.tap('DARK'), 'change', 'dark'); t.emitted(t.tap('LIGHT'), 'change', 'light'); t.emitted(t.tap('AUTO'), 'change', 'auto'); t.eq(chosen(t), ['AUTO'], 'chosen');
    } },
    { name: 'selected by label works too', mount: ['segmented', { ...O, selected: 'Light' }], run(t) { t.eq(chosen(t), ['LIGHT'], 'chosen'); } },
    { name: 'no selected: the first is chosen', mount: ['segmented', { options: 'Agenda, Month' }], run(t) {
      t.ok(t.texts().includes('AGENDA'), 'label'); const ev = t.tap('AGENDA'); t.notEmitted(ev, 'change', 'first already chosen');
    } },
    { name: 'press, slide off, release: no change', mount: ['segmented', O], run(t) {
      const ev = t.drag('DARK', { by: [0, 160] }); t.notEmitted(ev, 'change'); t.eq(chosen(t), ['AUTO'], 'chosen');
    } },
    { name: 'hug: height touch-min, segments on the 4 px grid', mount: ['segmented', O], run(t, { den }) {
      const b = t.part('segmented'); t.eq(b.h, D(den, 48, 32), 'height');
      for (const l of ['AUTO', 'DARK', 'LIGHT']) { const s = seg(t, l); t.ok((s.x - b.x) % 4 === 0 && s.w % 4 === 0, `${l} segment ${s.x}+${s.w} off the grid`); }
    } },
    { name: 'fill: content width, equal segments (within one grid step)', mount: ['segmented', { ...O, width: 'fill' }], run(t, { den }) {
      t.eq(t.part('segmented').w, D(den, 656, 440), 'width'); const w = ['AUTO', 'DARK', 'LIGHT'].map((l) => seg(t, l).w);
      t.ok(Math.max(...w) - Math.min(...w) <= 8 && w.every((v) => v % 4 === 0), `segments ${w}`);
    } },
    { name: 'disabled: blocked(chosen id) + error, choice kept', mount: ['segmented', { ...O, state: 'disabled' }], run(t) {
      const ev = t.tap('DARK'); t.notEmitted(ev, 'change'); t.emitted(ev, 'blocked', 'auto'); t.sounds(ev, ['error']);
    } },
    { name: 'unavailable: the word, no segments, tap blocked', mount: ['segmented', { ...O, state: 'unavailable' }], run(t, { den }) {
      t.has(t.texts(), /^Unavailable$/, 'word'); t.hasNot(t.texts(), 'AUTO', 'segments gone'); t.eq(t.part('segmented').h, D(den, 48, 32), 'height');
      const ev = t.tap('Unavailable'); t.emitted(ev, 'blocked', 'unavailable'); t.sounds(ev, ['error']);
    } },
    { name: 'update selected: choice moves silently, no change event', mount: ['segmented', O], run(t) {
      const ev = t.update({ selected: 'light' }); t.silent(ev); t.notEmitted(ev, 'change'); t.eq(chosen(t), ['LIGHT'], 'chosen');
      t.emitted(t.tap('AUTO'), 'change', 'auto', 'tap after the update');
    } },
    { name: 'update to disabled and back: blocked, then taps work', mount: ['segmented', O], run(t) {
      t.update({ state: 'disabled' }); t.notEmitted(t.tap('DARK'), 'change', 'disabled'); t.update({ state: 'default' }); t.emitted(t.tap('DARK'), 'change', 'dark', 'enabled');
    } },
    { name: 'every label centred in its segment (within 1 px): every variant, the Header title slot, every theme', mount: ['segmented', O], async run(t) {
      const bad = await centring(t); t.ok(!bad.length, `${bad.length} off: ${bad.slice(0, 6).join(' | ')}`);
    } },
  ],
};
