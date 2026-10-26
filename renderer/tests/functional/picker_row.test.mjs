// Picker row (components/picker_row.cpp): one option of a choose-one list (List row + Radio). Tap = tap(name) + tap,
// its radio on; rows sharing a parent are one group (a tap moves the choice); list group from items (rows cap, +N,
// empty words); disabled + unavailable block; update path.
import { D, px, diff, comps } from './_controls.mjs';
const P = { icon: 'speaker', name: 'Kitchen speaker', status: 'playing · Northbound', tone: 'working', selected: false };
const radios = (t) => comps(t, 'radio').sort((a, b) => a.y - b.y);
const dotOn = (t, r) => diff(px(t, r.x + r.w / 2, r.y + r.h / 2), px(t, r.x - 12, r.y + r.h / 2));
const rowOn = (t, name) => { const n = t.find(name), r = radios(t).find((q) => Math.abs(q.y + q.h / 2 - (n.y + n.h / 2)) < 40); return r ? dotOn(t, r) : null; };
const G = { list: 'group', items: 'Kitchen,Study,Lounge', icon: 'speaker' };
export default {
  id: 'picker_row', family: 'controls', about: 'Choose-one row: name, status, radio; a tap picks it.',
  tests: [
    { name: 'shows name, status (caps) and an off radio', mount: ['picker_row', P], run(t) {
      t.has(t.texts(), 'Kitchen speaker', 'name'); t.has(t.texts(), 'PLAYING · NORTHBOUND', 'status'); t.eq(rowOn(t, 'Kitchen speaker'), false, 'radio off');
    } },
    { name: 'selected: radio on', mount: ['picker_row', { ...P, selected: true }], run(t) { t.eq(rowOn(t, 'Kitchen speaker'), true, 'radio on'); } },
    { name: 'tap: tap(name) + tap sound, radio on', mount: ['picker_row', P], run(t) {
      const ev = t.tap('Kitchen speaker'); t.emitted(ev, 'tap', 'Kitchen speaker'); t.sounds(ev, ['tap']); t.eq(rowOn(t, 'Kitchen speaker'), true, 'radio on');
    } },
    { name: 'tap on the radio itself: the row takes it, one tap', mount: ['picker_row', P], run(t) {
      const r = radios(t)[0], ev = t.tap({ at: [r.x + r.w / 2, r.y + r.h / 2] }); t.eq(ev.filter((e) => e.kind === 'event' && e.name === 'tap').map((e) => e.detail), ['Kitchen speaker'], 'taps');
    } },
    { name: 'slide off: nothing chosen', mount: ['picker_row', P], run(t) { t.notEmitted(t.drag('Kitchen speaker', { by: [0, 200] }), 'tap'); t.eq(rowOn(t, 'Kitchen speaker'), false, 'radio off'); } },
    { name: 'disabled: blocked + error, radio stays off', mount: ['picker_row', { ...P, state: 'disabled' }], run(t) {
      const ev = t.tap('Kitchen speaker'); t.notEmitted(ev, 'tap'); t.emitted(ev, 'blocked'); t.sounds(ev, ['error']); t.eq(rowOn(t, 'Kitchen speaker'), false, 'off');
    } },
    { name: 'unavailable: says so, tap blocked', mount: ['picker_row', { ...P, state: 'unavailable' }], run(t) {
      t.has(t.texts(), /^Unavailable$/, 'word'); const ev = t.tap('Kitchen speaker'); t.notEmitted(ev, 'tap'); t.sounds(ev, ['error']);
    } },
    { name: 'height 96 / 64', mount: ['picker_row', P], run(t, { den }) { t.eq(t.part('list_row', 'Kitchen speaker').h, D(den, 96, 64), 'height'); } },
    { name: 'group: one row per item, a tap moves the choice', mount: ['picker_row', G], run(t) {
      for (const n of ['Kitchen', 'Study', 'Lounge']) t.has(t.texts(), n, n);
      t.emitted(t.tap('Study'), 'tap', 'Study'); t.eq(['Kitchen', 'Study', 'Lounge'].map((n) => rowOn(t, n)), [false, true, false], 'after Study');
      t.tap('Lounge'); t.eq(['Kitchen', 'Study', 'Lounge'].map((n) => rowOn(t, n)), [false, false, true], 'after Lounge');
    } },
    { name: 'group: JSON items with a selected field', mount: ['picker_row', { ...G, items: '[{"name":"Kitchen"},{"name":"Study","selected":true}]' }], run(t) {
      t.eq(['Kitchen', 'Study'].map((n) => rowOn(t, n)), [false, true], 'radios');
    } },
    { name: 'group: rows cap with a +N row for the rest', mount: ['picker_row', { ...G, items: 'A,B,C,D,E', rows: 3 }], run(t) {
      t.has(t.texts(), 'A', 'A'); t.has(t.texts(), 'B', 'B'); t.has(t.texts(), '+3', '+3'); t.hasNot(t.texts(), 'C', 'C hidden');
    } },
    { name: 'group: the +N row is not a choosable option (no radio)', mount: ['picker_row', { ...G, items: 'A,B,C,D,E', rows: 3 }], run(t) {
      t.eq(rowOn(t, '+3'), null, '+N row has a radio');
    } },
    { name: 'group: no items = the empty words', async run(t) {
      await t.mount('picker_row', { ...G, items: '' }); t.has(t.texts(), 'Nothing here', 'default words');
      await t.mount('picker_row', { ...G, items: '', empty: 'No speakers found' }); t.has(t.texts(), 'No speakers found', 'custom words');
    } },
    { name: 'update selected: radio on silently', mount: ['picker_row', P], run(t) { t.silent(t.update({ selected: true })); t.eq(rowOn(t, 'Kitchen speaker'), true, 'on'); } },
    { name: 'update name / status: new words in place', mount: ['picker_row', P], run(t) {
      t.silent(t.update({ name: 'Study', status: 'idle' })); t.has(t.texts(), 'Study', 'name'); t.has(t.texts(), 'IDLE', 'status');
    } },
    { name: 'update group items: new rows', mount: ['picker_row', G], run(t) {
      t.update({ items: 'Garden,Garage' }); t.has(t.texts(), 'Garden', 'new'); t.hasNot(t.texts(), 'Kitchen', 'old gone'); t.emitted(t.tap('Garage'), 'tap', 'Garage');
    } },
  ],
};
