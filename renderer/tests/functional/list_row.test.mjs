// List row (components/list_row.cpp): tap = tap(title) + tap (chevron rows: open); toggle rows hand the tap to the
// Toggle; value / chevron / sub / over parts; disabled + unavailable; stack from items (rows cap, empty); update path.
import { D, comps } from './_controls.mjs';
const P = { title: 'Lights', icon: 'bulb', trailing: 'none' };
const knobRight = (t) => { const g = comps(t, 'toggle')[0]; if (!g) return null; const k = t.objs().filter((o) => o.w === o.h && o.w < g.h && o.x >= g.x - 1 && o.x + o.w <= g.x + g.w + 1 && o.y >= g.y - 1).pop(); return k ? k.x + k.w / 2 > g.x + g.w / 2 : null; };
export default {
  id: 'list_row', family: 'controls', about: 'Hairline row: icon, title (+ sub / over), trailing value / chevron / switch.',
  tests: [
    { name: 'tap: tap(title) + tap sound', mount: ['list_row', P], run(t) { const ev = t.tap('Lights'); t.emitted(ev, 'tap', 'Lights'); t.sounds(ev, ['tap']); } },
    { name: 'tap the empty right end: the whole row is the target', mount: ['list_row', P], run(t) { const b = t.part('list_row'); t.emitted(t.tap({ at: [b.x + b.w - 8, b.y + b.h / 2] }), 'tap', 'Lights'); } },
    { name: 'chevron row: open sound', mount: ['list_row', { ...P, trailing: 'chevron' }], run(t) { const ev = t.tap('Lights'); t.emitted(ev, 'tap', 'Lights'); t.sounds(ev, ['open']); } },
    { name: 'value row: shows the value, tap sound', mount: ['list_row', { ...P, trailing: 'value', value: '42%' }], run(t) { t.has(t.texts(), '42%', 'value'); t.sounds(t.tap('Lights'), ['tap']); } },
    { name: 'value + chevron: value shown, open sound', mount: ['list_row', { ...P, trailing: 'value-chevron', value: 'Dark' }], run(t) { t.has(t.texts(), 'Dark', 'value'); t.sounds(t.tap('Lights'), ['open']); } },
    { name: 'value kind with no value: an em dash', mount: ['list_row', { ...P, trailing: 'value', value: '' }], run(t) { t.has(t.texts(), '—', 'dash'); } },
    { name: 'toggle row: a tap anywhere flips the switch (toggle on + toggle-on), again off', mount: ['list_row', { ...P, trailing: 'toggle', on: false }], run(t) {
      t.eq(knobRight(t), false, 'off at rest'); let ev = t.tap('Lights'); t.emitted(ev, 'toggle', 'on'); t.sounds(ev, ['toggle-on']); t.eq(knobRight(t), true, 'knob right');
      ev = t.tap('Lights'); t.emitted(ev, 'toggle', 'off'); t.sounds(ev, ['toggle-off']); t.eq(knobRight(t), false, 'knob left');
    } },
    { name: 'sub and over lines are shown', mount: ['list_row', { ...P, sub: '3 on', over: 'ground floor' }], run(t) { t.has(t.texts(), '3 on', 'sub'); t.has(t.texts(), 'GROUND FLOOR', 'over'); } },
    { name: 'size: content width x 96 / 64, with or without a sub', async run(t, { den }) {
      for (const sub of ['', '3 on']) { await t.mount('list_row', { ...P, sub }); const b = t.part('list_row'); t.eq([b.w, b.h], D(den, [656, 96], [440, 64]), `sub "${sub}"`); }
    } },
    { name: 'long title stays inside the row before the value', mount: ['list_row', { ...P, title: 'The very long name of a light in the downstairs hallway by the stairs', trailing: 'value', value: '42%' }], run(t) {
      const v = t.find('42%'), l = t.find(/very long/); t.ok(l.x + l.w <= v.x, 'title ends before the value');
    } },
    { name: 'disabled: blocked + error; a toggle row does not flip', async run(t) {
      await t.mount('list_row', { ...P, state: 'disabled' }); let ev = t.tap('Lights'); t.notEmitted(ev, 'tap'); t.emitted(ev, 'blocked'); t.sounds(ev, ['error']);
      await t.mount('list_row', { ...P, trailing: 'toggle', state: 'disabled' }); ev = t.tap('Lights'); t.notEmitted(ev, 'toggle'); t.eq(knobRight(t), false, 'knob');
    } },
    { name: 'unavailable: the word replaces the trailing part, no sub, tap blocked', mount: ['list_row', { ...P, trailing: 'value', value: '42%', sub: '3 on', state: 'unavailable' }], run(t) {
      t.has(t.texts(), /^Unavailable$/, 'word'); t.hasNot(t.texts(), '42%', 'value gone'); t.hasNot(t.texts(), '3 on', 'sub gone');
      const ev = t.tap('Lights'); t.notEmitted(ev, 'tap'); t.sounds(ev, ['error']);
    } },
    { name: 'pressed look still taps', mount: ['list_row', { ...P, state: 'pressed' }], run(t) { t.emitted(t.tap('Lights'), 'tap', 'Lights'); } },
    { name: 'stack: one row per item, each taps with its own title', mount: ['list_row', { ...P, list: 'stack', items: 'Kitchen,Study,Lounge' }], run(t, { den }) {
      for (const n of ['Kitchen', 'Study', 'Lounge']) t.emitted(t.tap(n), 'tap', n, n);
      const ys = ['Kitchen', 'Study', 'Lounge'].map((n) => t.find(n).y); t.eq([ys[1] - ys[0], ys[2] - ys[1]], D(den, [96, 96], [64, 64]), 'rows touch');
    } },
    { name: 'stack: object items fill their fields', mount: ['list_row', { list: 'stack', trailing: 'value', items: '[{"title":"Hall","value":"20°"},{"name":"Loft","value":"17°"}]' }], run(t) {
      for (const s of ['Hall', '20°', 'Loft', '17°']) t.has(t.texts(), s, s);
    } },
    { name: 'stack: rows cap ends with +N', mount: ['list_row', { ...P, list: 'stack', items: 'A,B,C,D,E', rows: 3 }], run(t) { t.has(t.texts(), '+3', '+N'); t.hasNot(t.texts(), 'C', 'C hidden'); } },
    { name: 'stack: no items = the empty words', mount: ['list_row', { ...P, list: 'stack', items: '', empty: 'No rooms' }], run(t) { t.has(t.texts(), 'No rooms', 'empty'); } },
    { name: 'update title / value in place, silent', mount: ['list_row', { ...P, trailing: 'value', value: '42%' }], run(t) {
      t.silent(t.update({ title: 'Lamps', value: '10%' })); t.has(t.texts(), 'Lamps', 'title'); t.has(t.texts(), '10%', 'value'); t.emitted(t.tap('Lamps'), 'tap', 'Lamps');
    } },
    { name: 'update on: the switch slides, silent, no event', mount: ['list_row', { ...P, trailing: 'toggle', on: false }], run(t) {
      const ev = t.update({ on: true }); t.silent(ev); t.notEmitted(ev, 'toggle'); t.eq(knobRight(t), true, 'knob right');
    } },
    { name: 'update stack items: same count in place, new count = new rows', mount: ['list_row', { ...P, list: 'stack', items: 'A,B' }], run(t) {
      t.update({ items: 'C,D' }); t.has(t.texts(), 'C', 'C'); t.hasNot(t.texts(), 'A', 'A gone'); t.update({ items: 'E,F,G' }); t.has(t.texts(), 'G', 'G');
    } },
  ],
};
