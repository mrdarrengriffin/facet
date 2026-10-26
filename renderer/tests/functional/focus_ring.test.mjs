// Focus ring (components/focus_ring.cpp): an outline round a control at the chosen offset; not a touch target.
import { px, diff } from './_controls.mjs';
const inkLeft = (t) => {  // first drawn pixel scanning right along the box's middle row, from well outside it
  const b = t.part('focus_ring'), y = b.y + b.h / 2, bg = px(t, b.x - 48, y);
  for (let x = b.x - 40; x < b.x + 8; x++) if (diff(px(t, x, y), bg)) return x; return null;
};
export default {
  id: 'focus_ring', family: 'controls', about: 'Focus outline round a control: offset none / focus / space-2 / space-4.',
  tests: [
    { name: 'a bigger offset puts the ring further out', async run(t) {
      const x = {}; for (const offset of ['none', 'space-2', 'focus-offset', 'space-4']) { await t.mount('focus_ring', { on: 'box', offset }); x[offset] = inkLeft(t); }
      t.ok(Object.values(x).every((v) => v !== null), `ring not found ${JSON.stringify(x)}`);
      t.ok(x.none > x['space-2'] && x['space-2'] > x['space-4'], `order ${JSON.stringify(x)}`); t.ok(x['focus-offset'] < x.none, 'focus offset is a gap');
    } },
    { name: 'drawn for button and play shapes, and when disabled', async run(t) {
      for (const p of [{ on: 'button' }, { on: 'play' }, { on: 'box', disabled: true }]) { await t.mount('focus_ring', p); t.ok(inkLeft(t) !== null, `ring ${JSON.stringify(p)}`); }
    } },
    { name: 'not a touch target: a tap does nothing', mount: ['focus_ring', { on: 'box' }], run(t) {
      const b = t.part('focus_ring'), ev = t.tap({ at: [b.x + b.w / 2, b.y + b.h / 2] }); t.silent(ev); t.eq(ev.filter((e) => e.kind === 'event').length, 0, 'events');
    } },
    { name: 'update offset: the ring moves, silent', mount: ['focus_ring', { on: 'box', offset: 'none' }], run(t) {
      const a = inkLeft(t); t.silent(t.update({ offset: 'space-4' })); t.ok(inkLeft(t) < a, 'moved out');
    } },
  ],
};
