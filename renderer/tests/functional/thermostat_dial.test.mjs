// Thermostat dial (components/thermostat_dial.cpp): the set point on the ring (knob); drag or tap the ring; change on
// release; clamps in the gap; steps 0.5 / 1; range; action off = no knob, no touch; centre free; disabled +
// unavailable block; update path: set point + current move silently, the user wins while holding.
import { D, last, num, ring, frac } from './_controls.mjs';
const P = { target: 21, current: 19.4, action: 'heating', min: 5, max: 30, step: '0.5', opening: false };
const arc = (t) => t.find({ type: 'arc' });
const knob = (t, den) => t.objs().find((o) => o.t !== 'arc' && o.w === D(den, 28, 20) && o.h === o.w && o.opa !== 0);
const kv = (t, den, p = P) => { const k = knob(t, den); return k ? p.min + (p.max - p.min) * frac(arc(t), k.x + k.w / 2, k.y + k.h / 2) : null; };
const ch = (ev) => num(last(ev, 'change'));
const at = (t, v, p = P) => ring(arc(t), (v - p.min) / (p.max - p.min), 3);
const dragOn = (t, a, b, p = P) => { const [x0, y0] = at(t, a, p), [x1, y1] = at(t, b, p); return t.drag(null, { from: [x0, y0], by: [x1 - x0, y1 - y0] }); };
export default {
  id: 'thermostat_dial', family: 'controls', about: 'Thermostat ring: drag the set point; heating / cooling span.',
  tests: [
    { name: 'knob sits on the set point', mount: ['thermostat_dial', P], run(t, { den }) { const v = kv(t, den); t.ok(v !== null && Math.abs(v - 21) <= 0.3, `knob at ${v}`); } },
    { name: 'drag the knob 21 -> 24: change 24 in half steps, ticks, knob follows', mount: ['thermostat_dial', P], run(t, { den }) {
      const ev = dragOn(t, 21, 24), v = ch(ev); t.ok(v !== null && Math.abs(v - 24) <= 0.5 && (v * 2) % 1 === 0, `change ${v}`);
      t.ok(ev.filter((e) => e.kind === 'sound' && /tick/.test(e.name)).length >= 3, 'ticks'); t.ok(Math.abs(kv(t, den) - v) <= 0.3, 'knob on the new set point');
    } },
    { name: 'tap the ring at 26: jumps there', mount: ['thermostat_dial', P], run(t, { den }) {
      const v = ch(t.tap({ at: at(t, 26) })); t.ok(v !== null && Math.abs(v - 26) <= 0.5, `change ${v}`); t.ok(Math.abs(kv(t, den) - v) <= 0.3, 'knob swept there');
    } },
    { name: 'drag into the gap past max: clamps to 30', mount: ['thermostat_dial', P], run(t) { t.eq(ch(dragOn(t, 21, 33)), 30, 'max'); } },
    { name: 'drag down into the gap past min: clamps to 5', mount: ['thermostat_dial', { ...P, target: 7 }], run(t) { t.eq(ch(dragOn(t, 7, 3)), 5, 'min'); } },
    { name: 'from 29.5 through the gap: stays at 30, never jumps to 5', mount: ['thermostat_dial', { ...P, target: 29.5 }], run(t) { t.eq(ch(dragOn(t, 29.5, 4.5)), 30, 'value'); } },
    { name: 'step 1: whole degrees', mount: ['thermostat_dial', { ...P, step: '1' }], run(t) {
      const v = ch(t.tap({ at: at(t, 24.4) })); t.ok(v !== null && v % 1 === 0, `change ${v} not whole`);
    } },
    { name: 'range 10..25: clamps to its own ends', async run(t) {
      const p = { ...P, min: 10, max: 25, target: 20 }; await t.mount('thermostat_dial', p); t.eq(ch(dragOn(t, 20, 27, p)), 25, 'max');
      await t.mount('thermostat_dial', { ...p, target: 12 }); t.eq(ch(dragOn(t, 12, 8, p)), 10, 'min');
    } },
    { name: 'tap the centre: nothing (centre is free)', mount: ['thermostat_dial', P], run(t) {
      const a = arc(t), ev = t.tap({ at: [a.x + a.w / 2, a.y + a.h / 2] }); t.notEmitted(ev, 'change'); t.silent(ev);
    } },
    { name: 'hold the knob without moving: no change', mount: ['thermostat_dial', P], run(t) { t.notEmitted(t.hold({ at: at(t, 21) }, 1200), 'change'); } },
    { name: 'action off: no knob, the ring takes no touch', mount: ['thermostat_dial', { ...P, action: 'off' }], run(t, { den }) {
      t.eq(knob(t, den), undefined, 'no knob'); const ev = t.tap({ at: at(t, 25) }); t.notEmitted(ev, 'change'); t.silent(ev);
    } },
    { name: 'idle and cooling still take a drag', async run(t) {
      for (const action of ['idle', 'cooling']) { await t.mount('thermostat_dial', { ...P, action }); t.ok(ch(dragOn(t, 21, 18)) !== null, `${action}: change`); }
    } },
    { name: 'disabled: blocked + error, knob stays', mount: ['thermostat_dial', { ...P, state: 'disabled' }], run(t, { den }) {
      const ev = dragOn(t, 21, 25); t.notEmitted(ev, 'change'); t.emitted(ev, 'blocked'); t.sounds(ev, ['error']); t.ok(Math.abs(kv(t, den) - 21) <= 0.3, 'knob');
    } },
    { name: 'unavailable: the word, no knob, tap blocked', mount: ['thermostat_dial', { ...P, state: 'unavailable' }], run(t, { den }) {
      t.has(t.texts(), /^Unavailable$/, 'word'); t.eq(knob(t, den), undefined, 'no knob'); const ev = t.tap({ at: at(t, 25) }); t.emitted(ev, 'blocked', 'unavailable'); t.sounds(ev, ['error']);
    } },
    { name: 'size: 480 / 320', mount: ['thermostat_dial', P], run(t, { den }) { const b = t.part('thermostat_dial'); t.eq([b.w, b.h], D(den, [480, 480], [320, 320]), 'box'); } },
    { name: 'update target: knob sweeps there silently, no change', mount: ['thermostat_dial', P], run(t, { den }) {
      const ev = t.update({ target: 26 }); t.silent(ev); t.notEmitted(ev, 'change'); t.ok(Math.abs(kv(t, den) - 26) <= 0.3, `knob at ${kv(t, den)}`);
    } },
    { name: 'update current / action: silent, knob stays', mount: ['thermostat_dial', P], run(t, { den }) {
      t.silent(t.update({ current: 23 }), 'current'); t.silent(t.update({ action: 'cooling' }), 'action'); t.ok(Math.abs(kv(t, den) - 21) <= 0.3, 'knob');
    } },
    { name: 'update to action off and back: knob goes, comes back, drags again', mount: ['thermostat_dial', P], run(t, { den }) {
      t.update({ action: 'off' }); t.eq(knob(t, den), undefined, 'gone'); t.update({ action: 'heating' }); t.ok(knob(t, den), 'back'); t.ok(ch(dragOn(t, 21, 24)) !== null, 'drag works');
    } },
  ],
};
