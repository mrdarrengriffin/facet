// Scroll track (components/scroll_track.cpp): a scrolling column of List rows with the track; drag scrolls (no
// events of its own), stops at the ends, few rows = no scroll, empty words, height prop, update keeps the scroll.
import { D } from './_controls.mjs';
const items = (n) => Array.from({ length: n }, (_, i) => `Room ${i + 1}`).join(',');
const P = { items: items(12), height: 256 };
const y = (t, s) => { const o = t.objs().find((q) => q.text === s); return o ? o.y : null; };
export default {
  id: 'scroll_track', family: 'controls', about: 'Scrolling list area with the 4 px scroll track.',
  tests: [
    { name: 'height prop = the visible area', async run(t, { den }) {
      for (const h of [128, 256]) { await t.mount('scroll_track', { ...P, height: h }); t.eq(t.part('scroll_track').h, D(den, h * 1.5, h), `height ${h}`); }
    } },
    { name: 'drag up scrolls the rows, no tap, no event', mount: ['scroll_track', P], run(t) {
      const b = t.part('scroll_track'), a = y(t, 'Room 1'), ev = t.drag(null, { from: [b.x + b.w / 2, b.y + b.h - 20], by: [0, -b.h / 2] });
      t.ok(y(t, 'Room 1') < a, `Room 1 at ${y(t, 'Room 1')}, was ${a}`); t.eq(ev.filter((e) => e.kind === 'event').length, 0, `events ${JSON.stringify(ev.filter((e) => e.kind === 'event'))}`);
    } },
    { name: 'scroll to the end: the last row ends at the bottom, not past it', mount: ['scroll_track', P], run(t) {
      const b = t.part('scroll_track'); for (let i = 0; i < 4; i++) t.drag(null, { from: [b.x + b.w / 2, b.y + b.h - 10], by: [0, -b.h + 20] });
      const l = t.objs().find((q) => q.text === 'Room 12'), row = t.objs().filter((o) => o.clk && o.y <= l.y && o.y + o.h >= l.y + l.h && o.h < b.h).pop();
      t.ok(row && Math.abs(row.y + row.h - (b.y + b.h)) <= 2, `last row bottom ${row && row.y + row.h} vs ${b.y + b.h}`);
    } },
    { name: 'scroll back up: the first row returns to the top', mount: ['scroll_track', P], run(t) {
      const b = t.part('scroll_track'), a = y(t, 'Room 1'); t.drag(null, { from: [b.x + b.w / 2, b.y + b.h - 10], by: [0, -b.h + 20] });
      for (let i = 0; i < 3; i++) t.drag(null, { from: [b.x + b.w / 2, b.y + 10], by: [0, b.h - 20] }); t.eq(y(t, 'Room 1'), a, 'Room 1 back');
    } },
    { name: 'few rows: nothing scrolls', mount: ['scroll_track', { ...P, items: 'Kitchen,Study' }], run(t) {
      const b = t.part('scroll_track'), a = y(t, 'Kitchen'); t.drag(null, { from: [b.x + b.w / 2, b.y + b.h - 20], by: [0, -b.h / 2] }); t.eq(y(t, 'Kitchen'), a, 'no move');
    } },
    { name: 'a tap on a row reaches the row', mount: ['scroll_track', P], run(t) { t.emitted(t.tap('Room 2'), 'tap', 'Room 2'); } },
    { name: 'no items: the empty words', async run(t) {
      await t.mount('scroll_track', { items: '' }); t.has(t.texts(), 'Nothing here', 'default'); await t.mount('scroll_track', { items: '', empty: 'No rooms yet' }); t.has(t.texts(), 'No rooms yet', 'custom');
    } },
    { name: 'update items keeps the scroll position', mount: ['scroll_track', P], run(t) {
      const b = t.part('scroll_track'); t.drag(null, { from: [b.x + b.w / 2, b.y + b.h - 20], by: [0, -b.h / 2] }); const a = y(t, 'Room 1');
      t.silent(t.update({ items: items(13) })); t.ok(t.objs().some((o) => o.text === 'Room 13'), 'new row built'); t.eq(y(t, 'Room 1'), a, 'scroll kept');
    } },
  ],
};
