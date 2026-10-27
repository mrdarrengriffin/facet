import fs from 'node:fs';
// Slider (components/slider.cpp): drag, tap-to-jump, clamp at min / max, step snapping, disabled / unavailable,
// update path. The value comes back as the "change" event detail on release; the knob position is read from the tree.
const C = JSON.parse(fs.readFileSync(new URL('../../tokens/colour.json', import.meta.url))).colours;
const is = (hex, tok) => !!hex && [C[tok].dark, C[tok].light].some((v) => v.toUpperCase() === String(hex).toUpperCase());
const inBox = (o, b) => o.x >= b.x && o.y >= b.y && o.x + o.w <= b.x + b.w && o.y + o.h <= b.y + b.h;
const KNOB = { '1.5x': 24, '1x': 16 };
const geo = (t, den) => {  // slider box, track (inset half a knob each side), knob centre x
  const b = t.part('slider'), k = KNOB[den];
  const knob = t.objs().filter((o) => o.w === k && o.h === k && o.x >= b.x && o.x + o.w <= b.x + b.w).pop();
  return { b, x0: b.x + k / 2, x1: b.x + b.w - k / 2, y: b.y + b.h / 2, knob, kx: knob ? knob.x + k / 2 : NaN };
};
const change = (evs) => { const e = evs.filter((x) => x.kind === 'event' && x.name === 'change').pop(); return e ? Number(e.detail) : null; };
const at = (g, f) => g.x0 + (g.x1 - g.x0) * f;
export default {
  id: 'slider', family: 'controls', about: 'Horizontal slider: drag or tap-to-jump, value on release, clamps and snaps.',
  tests: [
    { name: 'fill and knob are accent-text (3:1 on the track, WCAG 1.4.11)', mount: ['slider', { value: 40 }], run(t, { den }) {
      const g = geo(t, den), boxes = t.objs().filter((o) => o.bg && t.visible(o) && inBox(o, g.b) && o.w < g.b.w);
      t.ok(g.knob && is(boxes.find((o) => o.x === g.knob.x && o.w === g.knob.w)?.bg, 'accent-text'), 'knob accent-text');
      const fill = boxes.filter((o) => o !== g.knob && o.h < KNOB[den] && o.w > 0 && o.w < g.b.w - KNOB[den]);
      t.ok(fill.some((o) => is(o.bg, 'accent-text')), `fill accent-text (${fill.map((o) => o.bg).join()})`);
      t.ok(!fill.some((o) => is(o.bg, 'blue')), 'no blue fill');
    } },
    { name: 'knob sits on the value at rest', mount: ['slider', { value: 40 }], run(t, { den }) {
      const g = geo(t, den); t.ok(g.knob, 'knob found'); t.ok(Math.abs(g.kx - at(g, 0.4)) <= 2, `knob at ${g.kx}, want ${at(g, 0.4)}`);
    } },
    { name: 'drag the knob right: change = the new value, step ticks', mount: ['slider', { value: 40 }], run(t, { den }) {
      const g = geo(t, den), ev = t.drag(null, { from: [g.kx, g.y], toX: at(g, 0.7) });
      const v = change(ev); t.ok(v !== null && Math.abs(v - 70) <= 1, `change ${v}, want ~70`);
      t.ok(ev.filter((e) => e.kind === 'sound' && /tick/.test(e.name)).length >= 3, 'step ticks while dragging');
      t.ok(Math.abs(geo(t, den).kx - at(g, v / 100)) <= 2, 'knob stays on the released value');
    } },
    { name: 'drag past the right end clamps to max', mount: ['slider', { value: 40 }], run(t, { den, S }) {
      const g = geo(t, den), ev = t.drag(null, { from: [g.kx, g.y], toX: S - 2 });
      t.eq(change(ev), 100, 'value at max'); const k = geo(t, den);
      t.ok(Math.abs(k.kx - g.x1) <= 1, `knob at the track end (${k.kx} vs ${g.x1})`); t.ok(k.knob.x + k.knob.w <= g.b.x + g.b.w, 'knob inside the box');
      t.has(ev.filter((e) => e.kind === 'sound').map((e) => e.name), /landmark/, 'landmark at the end');
    } },
    { name: 'drag past the left end clamps to min', mount: ['slider', { value: 40 }], run(t, { den }) {
      const g = geo(t, den); t.eq(change(t.drag(null, { from: [g.kx, g.y], toX: 1 })), 0, 'value at min');
      t.ok(Math.abs(geo(t, den).kx - g.x0) <= 1, 'knob at the track start');
    } },
    { name: 'custom range 10..50 clamps to its own min and max', mount: ['slider', { value: 30, min: 10, max: 50 }], run(t, { den, S }) {
      let g = geo(t, den); t.ok(Math.abs(g.kx - at(g, 0.5)) <= 2, 'value 30 sits mid-track');
      t.eq(change(t.drag(null, { from: [g.kx, g.y], toX: S - 2 })), 50, 'max');
      g = geo(t, den); t.eq(change(t.drag(null, { from: [g.kx, g.y], toX: 1 })), 10, 'min');
    } },
    { name: 'step 25 snaps to whole steps', mount: ['slider', { value: 50, step: 25 }], run(t, { den }) {
      const g = geo(t, den), v = change(t.drag(null, { from: [g.kx, g.y], toX: at(g, 0.81) }));
      t.ok(v !== null && v % 25 === 0, `value ${v} not a multiple of 25`); t.eq(v, 75, 'nearest step');
    } },
    { name: 'tap on the track jumps there', mount: ['slider', { value: 20 }], run(t, { den }) {
      const g = geo(t, den), ev = t.tap({ at: [Math.round(at(g, 0.8)), Math.round(g.y)] });
      const v = change(ev); t.ok(v !== null && Math.abs(v - 80) <= 1, `change ${v}, want ~80`);
      t.ok(Math.abs(geo(t, den).kx - at(g, v / 100)) <= 2, 'knob swept to the tap');
    } },
    { name: 'tap or hold on the knob without moving: no change', mount: ['slider', { value: 40 }], run(t, { den }) {
      const g = geo(t, den);
      t.notEmitted(t.tap({ at: [g.kx, g.y] }), 'change', 'tap'); t.notEmitted(t.hold({ at: [g.kx, g.y] }), 'change', 'hold');
    } },
    { name: 'disabled: drag changes nothing, error once', mount: ['slider', { value: 40, state: 'disabled' }], run(t, { den }) {
      const g = geo(t, den), ev = t.drag(null, { from: [g.kx, g.y], toX: at(g, 0.9) });
      t.notEmitted(ev, 'change'); t.emitted(ev, 'blocked'); t.sounds(ev, ['error']);
      t.ok(Math.abs(geo(t, den).kx - g.kx) <= 1, 'knob did not move');
    } },
    { name: 'unavailable: says so, no track, drag changes nothing', mount: ['slider', { value: 40, state: 'unavailable' }], run(t, { den }) {
      t.has(t.texts(), /unavailable/i, 'word'); const b = t.part('slider');
      const ev = t.drag(null, { from: [b.x + b.w / 2, b.y + b.h / 2], by: [64, 0] }); t.notEmitted(ev, 'change'); t.sounds(ev, ['error']);
    } },
    { name: 'update value: knob moves there silently, no change event', mount: ['slider', { value: 40 }], run(t, { den }) {
      const ev = t.update({ value: 90 }); t.silent(ev); t.notEmitted(ev, 'change'); const g = geo(t, den);
      t.ok(Math.abs(g.kx - at(g, 0.9)) <= 2, `knob at ${g.kx}, want ${at(g, 0.9)}`);
    } },
    { name: 'update out of range is clamped on screen', mount: ['slider', { value: 40 }], run(t, { den }) {
      t.update({ value: 150 }); let g = geo(t, den); t.ok(g.kx <= g.x1 + 1, `knob past the end at value 150 (${g.kx} > ${g.x1})`);
      t.update({ value: -20 }); g = geo(t, den); t.ok(g.kx >= g.x0 - 1, `knob before the start at value -20 (${g.kx} < ${g.x0})`);
    } },
    { name: 'width prop = track length', mount: ['slider', { width: 160 }], run(t, { den }) {
      const g = geo(t, den); t.eq(g.x1 - g.x0, Math.round(160 * (den === '1x' ? 1 : 1.5) / 4) * 4, 'track length');
    } },
  ],
};
