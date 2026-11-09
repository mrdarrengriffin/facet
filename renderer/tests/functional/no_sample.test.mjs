// A panel never shows sample data unless its deck says so (facet-261): the factory deck on a linked panel, every page
// opened with the fake Home Assistant answering, shows none of the feeds' sample words. The words: each feed's last
// sample round (core/feed.h samples(), the lab "feeds"), from rendering every screen that offers a sample source on it.
import { readFileSync, readdirSync } from 'node:fs';
const SPECS = new URL('../../specs/', import.meta.url);
const specs = readdirSync(SPECS).filter((f) => f.endsWith('.json') && f !== 'deck.json').map((f) => JSON.parse(readFileSync(new URL(f, SPECS), 'utf8')));
const word = (s) => { s = String(s || '').trim(); return s.length >= 4 && !/^[\d\s.,:%°+-]*$/.test(s) ? s : null; };
export default {
  id: 'device', family: 'device', about: 'The factory deck on a linked panel shows no sample data.',
  tests: [
    { name: 'factory deck, linked: no sample words on any page (sample is for the site and the tests)', async run(t) {
      const lab = () => JSON.parse(t.b.str('feeds', (p) => t.b.str('', (q) => t.b.m.UTF8ToString(t.b.m._fw_lab(t.b.stage, p, q)))));
      const sample = new Set();
      for (const sp of specs.filter((s) => (s.params?.source?.options ?? []).includes('sample'))) {
        await t.mount('spec', { screen: sp.id, source: 'sample' });
        for (const f of lab()) for (const w of f.samples ?? []) if (word(w) && !w.startsWith('[') && !w.startsWith('{')) sample.add(w);
      }
      // today's date is real on both (the Clock's "MON 2 NOV"; the fortnight starts today): not sample data
      const now = new Date(), today = new RegExp(`\\b${now.getDate()}\\s+${now.toLocaleString('en-GB', { month: 'short' })}\\b`, 'i');
      for (const w of [...sample]) if (today.test(w)) sample.delete(w);
      t.ok(sample.size > 10, `sample words collected: ${sample.size}`);
      await t.device();
      t.ha.answers = true;
      t.b.spec('act', { do: 'ha.connect', url: 'http://homeassistant.local:8123', token: 'abcdefgh1234' }); t.b.settle(600);
      const deck = JSON.parse(readFileSync(new URL('../tests/fixtures/deck-full.json', SPECS), 'utf8'));  // a deck with every screen (the factory one is one empty Home)
      const seen = [];
      for (const pg of deck.pages) {
        if (pg.kind !== 'home') t.b.spec('act', { do: 'nav.open', screen: pg.id });
        t.wait(3000);
        for (const s of t.texts()) if (sample.has(s)) seen.push(`${pg.id}: "${s}"`);
        if (pg.kind !== 'home') t.b.spec('act', { do: 'nav.back' });
        t.b.settle(600);
      }
      t.eq(seen, [], 'no sample words on a linked panel');
    } },
  ],
};
