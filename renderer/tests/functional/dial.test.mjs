// Dial (components/dial.cpp): drag the ring or tap it to jump; change on release; clamps in the bottom gap, never
// jumps across it; step snapping; custom range; the ON / OFF button; 0 = off for brightness; centre is free;
// disabled + unavailable block; update path silent.
import { D, last, num, ring, comps } from './_controls.mjs';
const P = { value: 64, min: 0, max: 100, step: 1, on: true, caption: 'Brightness', unit: '%', opening: false };
// the value arc's square (layout:dial-arc), centred in the dial: the ring is drawn by the Pen now (contract v2), no lv_arc
const arc = (t) => { const b = t.part('dial'), a = t.b.S > 600 ? 432 : 288; return { x: b.x + b.w / 2 - a / 2, y: b.y + b.h / 2 - a / 2, w: a, h: a }; };
const val = (t) => { const v = t.texts().filter((s) => /^-?\d+(\.\d+)?$/.test(s)); return v.length ? Number(v[0]) : null; };
const ch = (ev) => num(last(ev, 'change'));
const at = (t, f) => ring(arc(t), f, 3);
const dragOn = (t, f0, f1) => { const [x0, y0] = at(t, f0), [x1, y1] = at(t, f1); return t.drag(null, { from: [x0, y0], by: [x1 - x0, y1 - y0] }); };
const near = (a, b, d = 1) => a !== null && Math.abs(a - b) <= d;
export default {
  id: 'dial', family: 'controls', about: 'Round 270 deg dial: drag or tap the ring; centre ON / OFF button.',
  tests: [
    { name: 'shows caption, value and unit, and the ON button', mount: ['dial', P], run(t) {
      t.has(t.texts(), 'BRIGHTNESS', 'caption'); t.eq(val(t), 64, 'value'); t.has(t.texts(), '%', 'unit'); t.has(t.texts(), 'ON', 'button');
    } },
    { name: 'tap the ring at 80 %: change ~80, the value follows, step ticks', mount: ['dial', P], run(t) {
      const ev = t.tap({ at: at(t, 0.8) }), v = ch(ev); t.ok(near(v, 80), `change ${v}, want ~80`); t.eq(val(t), v, 'shown value');
      t.ok(ev.filter((e) => e.kind === 'sound' && /tick/.test(e.name)).length >= 3, 'step ticks on the sweep'); t.notEmitted(ev, 'toggle');
    } },
    { name: 'drag the knob 64 -> 90: change ~90', mount: ['dial', P], run(t) { const v = ch(dragOn(t, 0.64, 0.9)); t.ok(near(v, 90), `change ${v}`); t.eq(val(t), v, 'shown'); } },
    { name: 'drag into the bottom gap past max: clamps to 100, landmark', mount: ['dial', P], run(t) {
      const ev = dragOn(t, 0.64, 1.1); t.eq(ch(ev), 100, 'value'); t.has(ev.filter((e) => e.kind === 'sound').map((e) => e.name), /landmark/, 'landmark at max');
    } },
    { name: 'drag from 95 through the gap: stays at 100, never jumps to 0', mount: ['dial', { ...P, value: 95 }], run(t) {
      const ev = dragOn(t, 0.95, -0.04); t.eq(ch(ev), 100, 'value'); t.notEmitted(ev, 'toggle', 'no off');
    } },
    { name: 'drag down past 0: value 0 and it switches off (brightness)', mount: ['dial', { ...P, value: 10 }], run(t) {
      const ev = dragOn(t, 0.1, -0.1); t.eq(ch(ev), 0, 'value'); t.emitted(ev, 'toggle', 'off'); t.has(t.texts(), 'OFF', 'button says OFF');
    } },
    { name: 'range 5..30: releasing at the minimum does not switch off', mount: ['dial', { ...P, value: 10, min: 5, max: 30, unit: '°' }], run(t) {
      const ev = dragOn(t, 0.2, -0.1); t.eq(ch(ev), 5, 'value at min'); t.notEmitted(ev, 'toggle');
    } },
    { name: 'step 25: snaps to whole steps', mount: ['dial', { ...P, value: 50, step: 25 }], run(t) {
      const v = ch(t.tap({ at: at(t, 0.8) })); t.ok(v !== null && v % 25 === 0, `value ${v}`); t.eq(v, 75, 'nearest step');
    } },
    { name: 'range 16..30 step 0.5: half steps, decimals shown', mount: ['dial', { ...P, value: 21, min: 16, max: 30, step: 0.5, unit: '°', caption: 'Target' }], run(t) {
      t.eq(val(t), 21, 'value'); const v = ch(t.tap({ at: at(t, (25.5 - 16) / 14) })); t.ok(near(v, 25.5, 0.5) && (v * 2) % 1 === 0, `change ${v}`);
      t.ok(t.texts().some((s) => /^\d+\.\d$/.test(s)), 'one decimal shown');
    } },
    { name: 'tap ON: toggle(off) + toggle-off, OFF; again: toggle(on) back at the value', mount: ['dial', P], run(t) {
      let ev = t.tap('ON'); t.emitted(ev, 'toggle', 'off'); t.sounds(ev, ['toggle-off']); t.has(t.texts(), 'OFF', 'OFF'); t.notEmitted(ev, 'change');
      ev = t.tap('OFF'); t.emitted(ev, 'toggle', 'on'); t.sounds(ev, ['toggle-on']); t.eq(val(t), 64, 'value back');
    } },
    { name: 'while off, a drag switches it on', mount: ['dial', { ...P, on: false }], run(t) {
      const ev = dragOn(t, 0.3, 0.5); t.emitted(ev, 'toggle', 'on'); t.has(t.texts(), 'ON', 'button ON');
    } },
    { name: 'tap the centre (not the button): nothing', mount: ['dial', P], run(t) {
      const ev = t.tap('64'); t.notEmitted(ev, 'change'); t.notEmitted(ev, 'toggle'); t.silent(ev); t.eq(val(t), 64, 'value');
    } },
    { name: 'hold the knob without moving: no change', mount: ['dial', P], run(t) { t.notEmitted(t.hold({ at: at(t, 0.64) }, 1200), 'change'); } },
    { name: 'disabled: ring and button blocked + error, nothing moves', mount: ['dial', { ...P, state: 'disabled' }], run(t) {
      let ev = dragOn(t, 0.64, 0.9); t.notEmitted(ev, 'change'); t.emitted(ev, 'blocked'); t.sounds(ev, ['error']); t.eq(val(t), 64, 'value');
      ev = t.tap('ON'); t.notEmitted(ev, 'toggle', 'button'); t.sounds(ev, ['error'], 'button error');
    } },
    { name: 'unavailable: the word, no value, no button; ring blocked', mount: ['dial', { ...P, state: 'unavailable' }], run(t) {
      t.has(t.texts(), /^Unavailable$/, 'word'); t.eq(val(t), null, 'no value'); t.hasNot(t.texts(), 'ON', 'no button');
      const b = t.part('dial'), r = D(t.b.S > 600 ? '1.5x' : '1x', 216, 144) - 3, ev = t.tap({ at: [b.x + b.w / 2, b.y + b.h / 2 - r] }); t.emitted(ev, 'blocked', 'unavailable'); t.sounds(ev, ['error']);
    } },
    { name: 'size: 560 / 376 box', mount: ['dial', P], run(t, { den }) { const b = t.part('dial'); t.eq([b.w, b.h], D(den, [560, 560], [376, 376]), 'box'); } },
    { name: 'update value: shown silently, no change event', mount: ['dial', P], run(t) {
      const ev = t.update({ value: 30 }); t.silent(ev); t.notEmitted(ev, 'change'); t.eq(val(t), 30, 'value');
      t.ok(near(ch(t.tap({ at: at(t, 0.5) })), 50), 'ring still works after');
    } },
    { name: 'update on off: button OFF, silent', mount: ['dial', P], run(t) { const ev = t.update({ on: false }); t.silent(ev); t.notEmitted(ev, 'toggle'); t.has(t.texts(), 'OFF', 'OFF'); } },
    { name: 'update to unavailable and back', mount: ['dial', P], run(t) {
      t.update({ state: 'unavailable' }); t.has(t.texts(), /^Unavailable$/, 'word'); t.update({ state: 'default', value: 40 }); t.eq(val(t), 40, 'value back'); t.has(t.texts(), 'ON', 'button back');
    } },
  ],
};
