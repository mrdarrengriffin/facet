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
