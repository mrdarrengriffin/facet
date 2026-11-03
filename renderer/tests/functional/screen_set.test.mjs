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
      t.ok(rows.length >= 3, `picker rows: ${rows.length}`);
      t.eq(rows.filter((c) => (c.props?.icon ?? 'none') !== 'none').length, 0, 'no icon at the start of any row');
      t.eq(comps(t, 'radio').filter((r) => t.visible(r) && radioOn(t, r)).length, 1, 'one radio on (the chosen source)');
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
