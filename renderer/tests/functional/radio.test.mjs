// Radio (components/radio.cpp): standalone tap selects (never un-selects), disabled blocks, hit area, update path.
import { D, px, diff } from './_controls.mjs';
const on = (t) => { const b = t.part('radio'); return diff(px(t, b.x + b.w / 2, b.y + b.h / 2), px(t, b.x - 12, b.y - 12)); };
export default {
  id: 'radio', family: 'controls', about: 'Choose-one ring: a tap selects it; tapping it again keeps it on.',
  tests: [
    { name: 'off: no dot; on: dot', async run(t) {
      await t.mount('radio', { on: false }); t.ok(!on(t), 'off has no dot'); await t.mount('radio', { on: true }); t.ok(on(t), 'on has a dot');
    } },
    { name: 'tap an off radio: tap(on) + tap sound, dot appears', mount: ['radio', { on: false }], run(t) {
      const ev = t.tap({ clk: 0 }); t.emitted(ev, 'tap', 'on'); t.sounds(ev, ['tap']); t.ok(on(t), 'dot');
    } },
    { name: 'tap an on radio: stays on (never un-chosen)', mount: ['radio', { on: true }], run(t) { t.tap({ clk: 0 }); t.ok(on(t), 'still on'); } },
    { name: 'hit area: touch-min round the ring', mount: ['radio', { on: false }], run(t, { den }) {
      const b = t.part('radio'), m = D(den, 48, 32); t.emitted(t.tap({ at: [b.x + b.w / 2, b.y + b.h / 2 - m / 2 + 3] }), 'tap', 'on');
    } },
    { name: 'disabled: blocked + error, stays off', mount: ['radio', { on: false, state: 'disabled' }], run(t) {
      const ev = t.tap({ clk: 0 }); t.notEmitted(ev, 'tap'); t.emitted(ev, 'blocked', 'off'); t.sounds(ev, ['error']); t.ok(!on(t), 'off');
    } },
    { name: 'size 24 / 16', mount: ['radio', {}], run(t, { den }) { const b = t.part('radio'); t.eq([b.w, b.h], D(den, [24, 24], [16, 16]), 'box'); } },
    { name: 'update on / off: dot follows, silent', mount: ['radio', { on: false }], run(t) {
      t.silent(t.update({ on: true })); t.ok(on(t), 'on'); t.silent(t.update({ on: false })); t.ok(!on(t), 'off');
    } },
  ],
};
