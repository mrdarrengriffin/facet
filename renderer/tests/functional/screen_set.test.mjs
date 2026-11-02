// Settings > Screens > a screen (screens/screen_set.cpp): a change reaches the deck and the screen that reads it; a
// change the deck refuses says so (a warn toast in the panel's words), sends no "deck" event, and the screen shows
// what the panel kept.
const act = (t, a) => { const r = JSON.parse(t.b.spec('act', a) || '{}'); t.b.tick(10); t.b.settle(800); return r; };
import fs from 'node:fs';
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
