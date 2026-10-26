// Segmented (components/segmented.cpp): tap a segment = change(id), the chosen one again = tap(id); one chosen at a
// time; hug / fill widths; disabled + unavailable block; update path silent.
import { D, px, diff, ctr } from './_controls.mjs';
const O = { options: 'Auto, Dark, Light', selected: 'auto' };
const seg = (t, label) => t.objs().find((o) => o.clk && t.find(label) && o.x <= t.find(label).x && o.x + o.w >= t.find(label).x + t.find(label).w);
const plate = (t, label) => { const s = seg(t, label); return px(t, s.x + 8, s.y + s.h / 2); };  // padding, left of the label
const chosen = (t) => { const L = ['AUTO', 'DARK', 'LIGHT'].filter((l) => t.texts().includes(l)), p = L.map((l) => plate(t, l));
  return L.filter((l, i) => p.filter((q, j) => j !== i).every((q) => diff(p[i], q, 12))); };  // the one plate that differs from the rest
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
  ],
};
