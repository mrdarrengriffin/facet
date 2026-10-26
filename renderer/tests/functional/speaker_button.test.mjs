// Speaker button (components/speaker_button.cpp): tap = tap(name) + open; disabled blocks; touch-min tall; long
// names cap the width; update path silent.
import { D } from './_controls.mjs';
const P = { name: 'Kitchen', playing: true, icon: 'speaker' };
export default {
  id: 'speaker_button', family: 'controls', about: 'Header button that opens the speaker picker.',
  tests: [
    { name: 'shows the name in caps', mount: ['speaker_button', P], run(t) { t.has(t.texts(), 'KITCHEN', 'name'); } },
    { name: 'tap: tap(name) + open sound', mount: ['speaker_button', P], run(t) { const ev = t.tap('KITCHEN'); t.emitted(ev, 'tap', /kitchen/i); t.sounds(ev, ['open']); } },
    { name: 'pressed look still taps', mount: ['speaker_button', { ...P, state: 'pressed' }], run(t) { t.emitted(t.tap('KITCHEN'), 'tap'); } },
    { name: 'disabled: blocked + error', mount: ['speaker_button', { ...P, state: 'disabled' }], run(t) {
      const ev = t.tap('KITCHEN'); t.notEmitted(ev, 'tap'); t.emitted(ev, 'blocked'); t.sounds(ev, ['error']);
    } },
    { name: 'height 48 / 32, width on the grid', mount: ['speaker_button', P], run(t, { den }) { const b = t.part('speaker_button'); t.eq(b.h, D(den, 48, 32), 'height'); t.ok(b.w % 4 === 0, `width ${b.w}`); } },
    { name: 'longer name = wider, very long names cap the width', async run(t) {
      const w = async (name) => { await t.mount('speaker_button', { ...P, name }); return t.part('speaker_button').w; };
      const a = await w('Den'), b = await w('Living room'), c = await w('Living room and kitchen and dining room'), d = await w('Living room and kitchen and dining room and garden and garage');
      t.ok(a < b && b < c, `widths ${a} ${b} ${c}`); t.eq(d, c, 'capped width');
    } },
    { name: 'icon none: narrower than with the speaker icon', async run(t) {
      await t.mount('speaker_button', P); const a = t.part('speaker_button').w; await t.mount('speaker_button', { ...P, icon: 'none' }); t.ok(t.part('speaker_button').w < a, 'narrower');
    } },
    { name: 'update name: new words, silent', mount: ['speaker_button', P], run(t) { t.silent(t.update({ name: 'Study' })); t.has(t.texts(), 'STUDY', 'name'); t.emitted(t.tap('STUDY'), 'tap', /study/i); } },
  ],
};
