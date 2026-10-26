// Toggle (components/toggle.cpp): tap flips on / off with toggle-on / toggle-off, the knob slides, disabled and
// unavailable block, update path slides silently.
import { D } from './_controls.mjs';
const knob = (t) => { const b = t.part('toggle'); return t.objs().filter((o) => o.w === o.h && o.w < b.h && o.x >= b.x - 1 && o.x + o.w <= b.x + b.w + 1 && o.y >= b.y - 1).pop(); };
const side = (t) => { const b = t.part('toggle'), k = knob(t); return k ? (k.x + k.w / 2 > b.x + b.w / 2 ? 'right' : 'left') : 'none'; };
export default {
  id: 'toggle', family: 'controls', about: 'On/off switch: a tap flips it, the knob slides, emits toggle on/off.',
  tests: [
    { name: 'off at rest: knob on the left', mount: ['toggle', { on: false }], run(t) { t.eq(side(t), 'left', 'knob'); } },
    { name: 'on at rest: knob on the right', mount: ['toggle', { on: true }], run(t) { t.eq(side(t), 'right', 'knob'); } },
    { name: 'tap off -> on: toggle(on), toggle-on, knob slides right', mount: ['toggle', { on: false }], run(t) {
      const ev = t.tap({ clk: 0 }); t.emitted(ev, 'toggle', 'on'); t.sounds(ev, ['toggle-on']); t.eq(side(t), 'right', 'knob');
    } },
    { name: 'tap twice: back to off with toggle-off', mount: ['toggle', { on: false }], run(t) {
      t.tap({ clk: 0 }); const ev = t.tap({ clk: 0 }); t.emitted(ev, 'toggle', 'off'); t.sounds(ev, ['toggle-off']); t.eq(side(t), 'left', 'knob');
    } },
    { name: 'hold then release flips exactly once', mount: ['toggle', { on: false }], run(t) {
      const ev = t.hold({ clk: 0 }, 1200); t.eq(ev.filter((e) => e.name === 'toggle').length, 1, 'toggle count'); t.eq(side(t), 'right', 'knob');
    } },
    { name: 'press, slide off, release: nothing changes', mount: ['toggle', { on: false }], run(t) {
      const ev = t.drag({ clk: 0 }, { by: [0, 160] }); t.notEmitted(ev, 'toggle'); t.eq(side(t), 'left', 'knob');
    } },
    { name: 'disabled: blocked + error, knob stays', mount: ['toggle', { on: true, state: 'disabled' }], run(t) {
      const ev = t.tap({ clk: 0 }); t.notEmitted(ev, 'toggle'); t.emitted(ev, 'blocked', 'on'); t.sounds(ev, ['error']); t.eq(side(t), 'right', 'knob');
    } },
    { name: 'unavailable: the word, no knob, tap = blocked(unavailable) + error', mount: ['toggle', { state: 'unavailable' }], run(t, { den }) {
      t.has(t.texts(), /^Unavailable$/, 'word'); t.eq(knob(t), undefined, 'no knob'); t.eq(t.part('toggle').h, D(den, 48, 32), 'touch-min tall');
      const ev = t.tap({ clk: 0 }); t.notEmitted(ev, 'toggle'); t.emitted(ev, 'blocked', 'unavailable'); t.sounds(ev, ['error']);
    } },
    { name: 'size: 80 x 48 at 720, 56 x 32 at 480', mount: ['toggle', {}], run(t, { den }) {
      const b = t.part('toggle'); t.eq([b.w, b.h], D(den, [80, 48], [56, 32]), 'box');
    } },
    { name: 'update on: knob slides, silent, no event', mount: ['toggle', { on: false }], run(t) {
      const ev = t.update({ on: true }); t.silent(ev); t.notEmitted(ev, 'toggle'); t.eq(side(t), 'right', 'knob');
    } },
    { name: 'update to disabled then back: taps blocked, then work again', mount: ['toggle', { on: false }], run(t) {
      t.update({ on: false, state: 'disabled' }); t.notEmitted(t.tap({ clk: 0 }), 'toggle', 'disabled');
      t.update({ on: false, state: 'default' }); t.emitted(t.tap({ clk: 0 }), 'toggle', 'on', 'enabled again');
    } },
    { name: 'update to unavailable and back: word, then the switch at its value', mount: ['toggle', { on: true }], run(t) {
      t.silent(t.update({ state: 'unavailable', on: true }), 'to unavailable'); t.has(t.texts(), /^Unavailable$/, 'word');
      t.update({ on: true, state: 'default' }); t.hasNot(t.texts(), /^Unavailable$/, 'word gone'); t.eq(side(t), 'right', 'knob back on');
    } },
  ],
};
