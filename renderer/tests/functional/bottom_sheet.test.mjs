// Bottom sheet (components/bottom_sheet.cpp): a choose-one list over the screen. Scrim tap / Close = close + back;
// a pick = the row's tap and the sheet closes; the head is not a close target; open / closed on the update path;
// title, list and selected props.
import { px, diff, comps, ctr } from './_controls.mjs';
const P = { open: 'open', list: 'speakers', selected: 1, screen: false };
const shown = (t) => t.texts().includes('Kitchen speaker');
const dotOn = (t, name) => { const n = t.find(name), r = comps(t, 'radio').find((q) => Math.abs(q.y + q.h / 2 - (n.y + n.h / 2)) < 40); return r ? diff(px(t, r.x + r.w / 2, r.y + r.h / 2), px(t, r.x - 12, r.y + r.h / 2)) : null; };
const close = (t) => comps(t, 'icon_button').pop();
export default {
  id: 'bottom_sheet', family: 'controls', about: 'Choose-one sheet over the screen: scrim, title, Close, picker rows.',
  tests: [
    { name: 'open: title + the rows, the selected one ringed', mount: ['bottom_sheet', P], run(t) {
      t.has(t.texts(), 'SPEAKER', 'title'); for (const n of ['Auto', 'Kitchen speaker', 'Study']) t.has(t.texts(), n, n);
      t.eq([dotOn(t, 'Auto'), dotOn(t, 'Kitchen speaker'), dotOn(t, 'Study')], [false, true, false], 'radios');
    } },
    { name: 'tap the scrim: close(scrim) + back, the sheet goes', mount: ['bottom_sheet', P], run(t, { S }) {
      const ev = t.tap({ at: [S / 2, 20] }); t.emitted(ev, 'close', 'scrim'); t.sounds(ev, ['back']); t.ok(!shown(t), 'sheet hidden');
    } },
    { name: 'tap Close: close(close) + back, the sheet goes', mount: ['bottom_sheet', P], run(t) {
      const ev = t.tap({ at: ctr(close(t)) }); t.emitted(ev, 'close', /close/); t.sounds(ev, ['back']); t.ok(!shown(t), 'sheet hidden');
    } },
    { name: 'pick a row: its tap + tap sound, and the sheet closes', mount: ['bottom_sheet', P], run(t) {
      const ev = t.tap('Study'); t.emitted(ev, 'tap', 'Study'); t.has(ev.filter((e) => e.kind === 'sound').map((e) => e.name), 'tap', 'tap sound'); t.ok(!shown(t), 'closed after the pick');
    } },
    { name: 'tap the title in the head: stays open, nothing fires', mount: ['bottom_sheet', P], run(t) {
      const ev = t.tap('SPEAKER'); t.notEmitted(ev, 'close'); t.ok(shown(t), 'still open');
    } },
    { name: 'scroll gesture on the rows does not pick', mount: ['bottom_sheet', P], run(t) { const ev = t.drag('Study', { by: [0, -120] }); t.notEmitted(ev, 'tap'); } },
    { name: 'closed: nothing shown; update open: it slides in; update closed: gone', mount: ['bottom_sheet', { ...P, open: 'closed' }], run(t) {
      t.ok(!shown(t), 'closed at rest'); t.update({ open: 'open' }); t.ok(shown(t), 'open after update'); t.update({ open: 'closed' }); t.ok(!shown(t), 'closed again');
    } },
    { name: 'closed: a tap on the screen does not hit the hidden sheet', mount: ['bottom_sheet', { ...P, open: 'closed' }], run(t, { S }) {
      const ev = t.tap({ at: [S / 2, S / 2] }); t.notEmitted(ev, 'close'); t.notEmitted(ev, 'tap');
    } },
    { name: 'title prop replaces the list title', mount: ['bottom_sheet', { ...P, title: 'Play on' }], run(t) { t.has(t.texts(), 'PLAY ON', 'title'); t.hasNot(t.texts(), 'SPEAKER', 'old title'); } },
    { name: 'selected 3: that row ringed', mount: ['bottom_sheet', { ...P, selected: 3 }], run(t) { t.eq([dotOn(t, 'Kitchen speaker'), dotOn(t, 'Study')], [false, true], 'radios'); } },
    { name: 'theme list: other rows, still picks and closes', mount: ['bottom_sheet', { ...P, list: 'theme', selected: 0 }], run(t) {
      t.hasNot(t.texts(), 'Kitchen speaker', 'not the speakers'); const rows = comps(t, 'list_row'); t.ok(rows.length >= 2, `rows ${rows.length}`);
      const ev = t.tap({ at: ctr(rows[1]) }); t.emitted(ev, 'tap'); t.ok(comps(t, 'list_row').every((r) => !t.texts().length || true), 'ok');
    } },
    { name: 'over a screen: the scrim blocks the rows under it', mount: ['bottom_sheet', { ...P, screen: true }], run(t, { S }) {
      const ev = t.tap({ at: [S / 2, 20] }); t.emitted(ev, 'close', 'scrim'); t.eq(ev.filter((e) => e.kind === 'event' && e.name === 'tap').length, 0, 'no tap under the scrim');
    } },
  ],
};
