// Settings > Screens > a screen (screens/screen_set.cpp): a change reaches the deck and the screen that reads it; a
// change the deck refuses says so (a warn toast in the panel's words), sends no "deck" event, and the screen shows
// what the panel kept.
const deck = (t) => JSON.parse(JSON.parse(t.b.spec('list', '')).deck);
const act = (t, a) => { const r = JSON.parse(t.b.spec('act', a) || '{}'); t.b.tick(10); t.b.settle(800); return r; };
import fs from 'node:fs';
import { px, diff, comps } from './_controls.mjs';
const radioOn = (t, r) => diff(px(t, r.x + r.w / 2, r.y + r.h / 2), px(t, r.x - 12, r.y + r.h / 2));
const FACTORY = JSON.parse(fs.readFileSync(new URL('../../specs/deck.json', import.meta.url)));

export default {
  id: 'screen_set', family: 'device', about: 'A screen\'s settings: a change reaches the deck and the screen; a refused one says so.',
  tests: [
    { name: 'Media > Visual: cassette reaches the deck and the Media screen', async run(t) {
      await t.device();
      act(t, { do: 'nav.open', screen: 'screen_set', params: { id: 'media' } });
      t.drag({ at: [360, 600] }, { by: [0, -400] }); t.drag({ at: [360, 600] }, { by: [0, -400] });
      const evs = t.tap(/^cassette$/i);
      t.ok(evs.some((e) => e.name === 'deck' && e.detail === 'media params.visual=cassette'), 'the deck takes visual=cassette');
      act(t, { do: 'nav.back' });
      t.ok(t.comps().some((c) => c.ids.at(-1) === 'media_visual'), 'Media shows its visual');
    } },
    { name: 'Calendar > Source: one mark per row (its radio), no tick at the start', async run(t) {
      await t.device();
      act(t, { do: 'nav.open', screen: 'screen_set', params: { id: 'calendar' } });
      t.tap(/^Source$/); t.b.settle(1200);
      t.has(t.texts(), /^Sample fortnight/, 'the Source sheet is open');
      const rows = t.comps().filter((c) => c.ids.includes('picker_row') && t.visible(c));
      t.ok(rows.length >= 2, `picker rows: ${rows.length}`);
      t.hasNot(t.texts(), /iCal/, 'no source this firmware cannot read (the iCal link is planned)');
      t.eq(rows.filter((c) => (c.props?.icon ?? 'none') !== 'none').length, 0, 'no icon at the start of any row');
      t.eq(comps(t, 'radio').filter((r) => t.visible(r) && radioOn(t, r)).length, 1, 'one radio on (the chosen source)');
    } },
    { name: 'Calendar > Calendars (pick many): a checkbox per row, not radios; a tap ticks only that row', async run(t) {
      t.ha.s['calendar.test_family'] = { '': 'off', friendly_name: 'Family' };
      t.ha.s['calendar.test_work'] = { '': 'off', friendly_name: 'Work' };
      t.ha.answers = true;
      await t.device();
      t.b.spec('act', { do: 'ha.connect', url: 'http://ha.test:8123', token: 'abcdefgh1234' }); t.b.tick(8); t.b.settle(600);
      act(t, { do: 'nav.open', screen: 'screen_set', params: { id: 'calendar' } });
      for (let i = 0; i < 6 && !t.texts().includes('Calendars'); i++) { t.drag({ at: [t.b.S / 2, t.b.S * 0.8] }, { by: [0, -t.b.S * 0.4] }); t.wait(500); }
      t.drag({ at: [t.b.S / 2, t.b.S * 0.8] }, { by: [0, -t.b.S * 0.3] }); t.wait(500);  // the row clear of the bottom edge
      t.tap(/^Calendars$/); t.b.tick(8); t.b.settle(1200); t.b.tick(8); t.b.settle(800);
      t.has(t.texts(), 'Family', 'the calendars listed');
      t.eq(comps(t, 'radio').filter((r) => t.visible(r)).length, 0, 'no radios');
      t.ok(comps(t, 'checkbox').filter((r) => t.visible(r)).length >= 2, 'a checkbox per calendar');
      const evs = t.tap('Family'); t.b.tick(8); t.b.settle(800);
      t.ok(evs.some((e) => e.name === 'deck' && /calendar\.test_family/.test(e.detail)), `Family picked: ${evs.map((e) => e.name + '(' + e.detail + ')').join(' ').slice(0, 200)}`);
      const boxes = comps(t, 'checkbox').filter((r) => t.visible(r)).sort((a, z) => a.y - z.y);
      const ink = (b) => px(t, b.x + b.w / 2, b.y + b.h / 2), bg = (b) => px(t, b.x - 12, b.y + b.h / 2);
      t.ok(diff(ink(boxes[0]), bg(boxes[0])), 'Family ticked');
      t.ok(!diff(ink(boxes[1]), bg(boxes[1])), 'Work not (the other row untouched)');
    } },
    { name: 'Show on Home off: the screen leaves the deck and its Home card goes with it; on again brings both back', async run(t) {
      await t.device();
      act(t, { do: 'nav.open', screen: 'screen_set', params: { id: 'moon' } });
      const off = t.tap('Show on Home');
      t.ok(off.some((e) => e.name === 'deck' && e.detail === 'moon on=false'), `kept: ${off.map((e) => `${e.name}(${e.detail})`).join(' ')}`);
      t.ok(!t.texts().includes('Not changed'), 'no refusal');
      let d = deck(t);
      t.ok(!d.pages.some((p) => p.id === 'moon'), 'Moon left the deck');
      t.ok(!d.pages.some((p) => (p.cards ?? []).some((c) => c.page === 'moon')), 'its Home card went with it');
      const on = t.tap('Show on Home');
      t.ok(on.some((e) => e.name === 'deck' && e.detail === 'moon on=true'), 'on again');
      d = deck(t);
      t.ok(d.pages.some((p) => p.id === 'moon') && d.pages.find((p) => p.kind === 'home').cards.some((c) => c.page === 'moon'), 'Moon and its Home card are back');
    } },
    { name: 'a refused change: a warn toast in the panel\'s words, no deck event, the screen shows what is kept', async run(t) {
      await t.device();
      const r = JSON.parse(t.b.spec('deck', { ...FACTORY, rev: FACTORY.rev + 1, start: 'clock' }) || '{}');
      t.ok(r.ok, `a deck that starts on Clock: ${JSON.stringify(r.errors ?? '')}`);
      act(t, { do: 'nav.open', screen: 'screen_set', params: { id: 'clock' } });
      const evs = t.tap('Show on Home');
      t.b.settle(1200);
      t.ok(!evs.some((e) => e.name === 'deck'), `no deck event (${evs.map((e) => e.name).join(' ')})`);
      t.ok(t.texts().includes('Not changed'), 'a toast says Not changed');
      t.ok(t.texts().some((x) => /start page/i.test(x)), 'in the panel\'s words: the start page stays on');
      const row = t.part('list_row', 'Show on Home');
      t.ok(row, 'the screen is back');
    } },
  ],
};
