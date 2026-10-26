// Back button (components/back_button.cpp, built on Icon button): tap = back + sound back; disabled blocks.
import { D } from './_controls.mjs';
export default {
  id: 'back_button', family: 'controls', about: 'Header back arrow: a tap goes back.',
  tests: [
    { name: 'tap: back + back sound', mount: ['back_button', {}], run(t) { const ev = t.tap({ clk: 0 }); t.emitted(ev, 'back'); t.sounds(ev, ['back']); t.notEmitted(ev, 'tap'); } },
    { name: 'pressed look still goes back', mount: ['back_button', { state: 'pressed' }], run(t) { t.emitted(t.tap({ clk: 0 }), 'back'); } },
    { name: 'hold then release: exactly one action (back, or home where long-press-home is built), back sound', mount: ['back_button', {}], run(t) {
      const ev = t.hold({ clk: 0 }, 1200), n = ev.filter((e) => e.kind === 'event' && (e.name === 'back' || e.name === 'home')).length; t.eq(n, 1, 'back + home count'); t.sounds(ev, ['back']);
    } },
    { name: 'slide off: nothing', mount: ['back_button', {}], run(t) { t.notEmitted(t.drag({ clk: 0 }, { by: [0, 200] }), 'back'); } },
    { name: 'disabled: blocked + error, no back', mount: ['back_button', { state: 'disabled' }], run(t) {
      const ev = t.tap({ clk: 0 }); t.notEmitted(ev, 'back'); t.emitted(ev, 'blocked'); t.sounds(ev, ['error']);
    } },
    { name: 'size = touch back (48 / 32), square', mount: ['back_button', {}], run(t, { den }) { const b = t.find({ clk: 0 }); t.eq([b.w, b.h], D(den, [48, 48], [32, 32]), 'box'); } },
    { name: 'update to disabled and back', mount: ['back_button', {}], run(t) {
      t.silent(t.update({ state: 'disabled' })); t.notEmitted(t.tap({ clk: 0 }), 'back', 'disabled'); t.update({ state: 'default' }); t.emitted(t.tap({ clk: 0 }), 'back', undefined, 'enabled');
    } },
  ],
};
