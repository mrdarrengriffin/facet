// Settings > Screens > a screen (screens/screen_set.cpp): a change reaches the deck and the screen that reads it; a
// change the deck refuses says so (a warn toast in the panel's words), sends no "deck" event, and the screen shows
// what the panel kept.
const deck = (t) => JSON.parse(JSON.parse(t.b.spec('list', '')).deck);
const act = (t, a) => { const r = JSON.parse(t.b.spec('act', a) || '{}'); t.b.tick(10); t.b.settle(800); return r; };
import fs from 'node:fs';
import { px, diff, comps } from './_controls.mjs';
const radioOn = (t, r) => diff(px(t, r.x + r.w / 2, r.y + r.h / 2), px(t, r.x - 12, r.y + r.h / 2));
const FACTORY = JSON.parse(fs.readFileSync(new URL('../fixtures/deck-full.json', import.meta.url)));

export default {
  id: 'screen_set', family: 'device', about: 'A screen\'s settings: a change reaches the deck and the screen; a refused one says so.',
  tests: [
    { name: 'Media > Visual: cassette reaches the deck and the Media screen', async run(t) {
      await t.device();
      act(t, { do: 'nav.open', screen: 'screen_set', params: { id: 'media' } });
      t.drag({ at: [360, 600] }, { by: [0, -400] }); t.drag({ at: [360, 600] }, { by: [0, -400] });
      const evs = t.tap(/^cassette$/i);  // four options: the Segmented in the Visual row
      t.ok(evs.some((e) => e.name === 'deck' && e.detail === 'media params.visual=cassette'), 'the deck takes visual=cassette');
      act(t, { do: 'nav.back' });
      t.ok(t.comps().some((c) => c.ids.at(-1) === 'media_visual'), 'Media shows its visual');
    } },
    { name: 'Media: a saved Visual = disc (folded into the CD) opens as the CD; Spin during playback reaches the deck and the screen', async run(t) {
      const d = JSON.parse(JSON.stringify(FACTORY));
      const mp = d.pages.find((p) => p.kind === 'media');
      mp.params = { ...(mp.params || {}), visual: 'disc' };
      await t.device(d);
      act(t, { do: 'nav.open', screen: 'media' });
      t.ok(t.comps().some((c) => c.ids.at(-1) === 'media_visual'), 'Media opens with its visual');
      act(t, { do: 'nav.open', screen: 'screen_set', params: { id: 'media' } });
      t.drag({ at: [360, 600] }, { by: [0, -400] }); t.drag({ at: [360, 600] }, { by: [0, -400] });
      const seg = t.comps().find((c) => c.ids.includes('segmented') && t.visible(c)), words = seg ? t.texts(seg) : [];
      t.ok(words.some((x) => /^cd$/i.test(x)) && !t.texts().some((x) => /^disc$/i.test(x)), `Visual offers CD, no Disc: ${words}`);
      // the selected segment: the one whose fill (near its top edge, clear of the words) differs from the other three
      const fills = [0, 1, 2, 3].map((i) => px(t, seg.x + (i + 0.5) * seg.w / 4, seg.y + 5));
      const odd = fills.findIndex((f, i) => fills.every((g, j) => j === i || diff(f, g)));
      t.eq(odd, 0, `the saved disc shows as CD selected (fills ${JSON.stringify(fills)})`);
      t.has(t.texts(), /^Spin during playback$/, 'the Spin during playback row');
      const evs = t.tap('Spin during playback');
      t.ok(evs.some((e) => e.name === 'deck' && e.detail === 'media params.spin=false'), `on by default: the tap turns it off (spin=false): ${evs.map((e) => `${e.name}(${e.detail})`).join(' ')}`);
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
    { name: 'Card on Home: on puts a Moon card on Home, off takes it away; the screen and its settings stay', async run(t) {
      await t.device();
      act(t, { do: 'nav.open', screen: 'screen_set', params: { id: 'moon' } });
      const on = t.tap('Card on Home');
      t.ok(on.some((e) => e.name === 'deck' && e.detail === 'moon on=true'), `kept: ${on.map((e) => `${e.name}(${e.detail})`).join(' ')}`);
      let d = deck(t);
      t.eq(d.pages.find((p) => p.kind === 'home').cards.filter((c) => c.page === 'moon').length, 1, 'one Moon card on Home');
      const off = t.tap('Card on Home');
      t.ok(off.some((e) => e.name === 'deck' && e.detail === 'moon on=false'), 'off');
      t.ok(!t.texts().includes('Not changed'), 'no refusal');
      d = deck(t);
      t.ok(d.pages.some((p) => p.id === 'moon'), 'Moon keeps its deck page (no on / off: #177)');
      t.ok(!d.pages.some((p) => (p.cards ?? []).some((c) => c.page === 'moon')), 'its Home card went');
    } },
    { name: 'a built-in screen with no deck page: its settings open, a pick makes its page (no card), Card on Home adds one', async run(t) {
      const d0 = JSON.parse(JSON.stringify(FACTORY));
      d0.pages = d0.pages.filter((p) => p.kind !== 'moon');
      for (const p of d0.pages) if (p.cards) p.cards = p.cards.filter((c) => c.page !== 'moon');
      await t.device(d0);
      act(t, { do: 'nav.open', screen: 'moon' });
      t.eq(t.where(), 'moon', 'Moon opens with no deck page');
      act(t, { do: 'nav.open', screen: 'screen_set', params: { id: 'moon' } });
      t.ok(t.part('list_row', 'Card on Home'), 'its settings show');
      const on = t.tap('Card on Home');
      t.ok(on.some((e) => e.name === 'deck' && e.detail === 'moon on=true'), `kept: ${on.map((e) => `${e.name}(${e.detail})`).join(' ')}`);
      const d = deck(t);
      t.ok(d.pages.some((p) => p.kind === 'moon'), 'its deck page now holds its settings');
      t.eq(d.pages.find((p) => p.kind === 'home').cards.filter((c) => c.page === 'moon').length, 1, 'a Moon card on Home');
    } },
    { name: 'a refused change: a warn toast in the panel\'s words, no deck event, the screen shows what is kept', async run(t) {
      const d0 = JSON.parse(JSON.stringify(FACTORY));
      const home = d0.pages.find((p) => p.kind === 'home');
      home.cards = Array.from({ length: 24 }, () => ({ t: 'page', page: 'clock', size: '1x1' }));  // a full Home (budget cards)
      for (const p of d0.pages) if (p !== home && p.cards) p.cards = p.cards.filter((c) => c.page !== 'moon');
      await t.device(d0);
      act(t, { do: 'nav.open', screen: 'screen_set', params: { id: 'moon' } });
      const evs = t.tap('Card on Home');
      t.b.settle(1200);
      t.ok(!evs.some((e) => e.name === 'deck'), `no deck event (${evs.map((e) => e.name).join(' ')})`);
      t.ok(t.texts().includes('Not changed'), 'a toast says Not changed');
      t.ok(t.texts().some((x) => /cards on a home/i.test(x)), 'in the panel\'s words: the Home is full');
      t.ok(t.part('list_row', 'Card on Home'), 'the screen is back');
    } },
  ],
};
