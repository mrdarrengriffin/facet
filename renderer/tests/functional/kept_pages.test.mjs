// Kept pages (core/device.cpp, facet-243): Home and the deck's pages are built once in the idle gaps and kept hidden,
// so a tap shows a page instead of building it. A hidden page does no widget work: its data changes, its widgets wait
// (core/ha.h asleep) and catch up when it shows (wake). Scroll is kept. A spec / deck change rebuilds only the pages it
// touched. Home Assistant is faked (tools/functional.mjs FakeHA); W is the factory deck's weather entity.
import fs from 'node:fs';
const FACTORY = JSON.parse(fs.readFileSync(new URL('../../specs/deck.json', import.meta.url)));
const W = 'weather.forecast_home';
const act = (t, a) => { const r = JSON.parse(t.b.spec('act', a) || '{}'); t.b.tick(10); t.b.settle(800); return r; };
const keep = (t, n = '') => t.b.test('keep', String(n)) ?? [];
const page = (t, id) => keep(t).find((k) => k.page === id);
const builds = (t) => t.b.test('stack')[0].builds;
const idle = (t, ms) => t.b.tick(Math.ceil(ms / 16));  // the prebuild runs in idle gaps (250 ms, one page each)

export default {
  id: 'kept_pages', family: 'device', about: 'Kept pages: shown without a build, data-only while hidden, scroll kept, only touched pages rebuilt.',
  tests: [
    { name: 'every deck page is built once in the idle gaps; a tap shows it without a build, Back too', async run(t) {
      t.ha.s[W] = { '': 'cloudy', temperature: '10.2' };
      await t.device();
      keep(t, 8);
      idle(t, 3000);
      const ids = keep(t).map((k) => k.page).sort();
      t.eq(ids, ['calendar', 'clock', 'energy', 'home', 'media', 'moon', 'timer', 'weather'], 'Home + the 7 deck pages kept');
      const n = builds(t);
      act(t, { do: 'nav.open', screen: 'clock' });
      t.ok(page(t, 'clock').shown && !page(t, 'home').shown, 'Clock shown, Home hidden');
      act(t, { do: 'nav.back' });
      act(t, { do: 'nav.open', screen: 'weather' });
      act(t, { do: 'nav.back' });
      t.eq(builds(t), n, 'four screen changes, no build');
      t.ok(page(t, 'home').shown, 'Home shown again');
    } },
    { name: 'a hidden page: its data changes, its widgets wait until it shows', async run(t) {
      t.ha.s[W] = { '': 'cloudy', temperature: '10.2' };
      await t.device();
      keep(t, 8);
      idle(t, 3000);
      act(t, { do: 'nav.open', screen: 'weather' });
      t.has(t.texts(), /^10°$/, 'Weather shows 10°');
      act(t, { do: 'nav.back' });
      t.ha.set(W, 'sunny', { temperature: '15.4' });
      t.ok(page(t, 'weather').waiting > 0, `the hidden Weather page waits (${page(t, 'weather').waiting} binds)`);
      act(t, { do: 'nav.open', screen: 'weather' });
      t.has(t.texts(), /^15°$/, 'shown: 15° at once');
      t.eq(page(t, 'weather').waiting, 0, 'nothing waiting once shown');
    } },
    { name: 'scroll is kept: Home scrolled, a page, Back: Home where it was', async run(t) {
      await t.device();
      keep(t, 8);
      idle(t, 3000);
      t.drag({ at: [t.b.S / 2, t.b.S * 0.8] }, { by: [0, -t.b.S * 0.4] });
      const y0 = t.b.scrolls().map((s) => s.sy).find((y) => y > 0);
      t.ok(y0 > 0, `Home scrolled (${y0})`);
      act(t, { do: 'nav.open', screen: 'clock' });
      act(t, { do: 'nav.back' });
      t.ok(t.b.scrolls().some((s) => s.sy === y0), `Home at ${y0} again`);
    } },
    { name: 'a deck change rebuilds only the pages it touched', async run(t) {
      await t.device();
      keep(t, 8);
      idle(t, 3000);
      const deck = JSON.parse(JSON.stringify(FACTORY));
      deck.rev = FACTORY.rev + 1;
      deck.pages.find((p) => p.id === 'weather').params = { source: 'sample' };
      const r = JSON.parse(t.b.spec('deck', deck) || '{}');
      t.ok(r.ok, `deck applied ${JSON.stringify(r.errors ?? '')}`);
      t.ok(page(t, 'weather').stale, 'Weather (its params changed) is stale');
      t.ok(!page(t, 'clock').stale && !page(t, 'home').stale, 'Clock and Home are not');
      const n = builds(t);
      act(t, { do: 'nav.open', screen: 'clock' });
      act(t, { do: 'nav.back' });
      act(t, { do: 'nav.open', screen: 'weather' });
      t.has(t.texts(), /^Weather$/i, 'Weather shows');
      t.eq(builds(t), n + 1, 'one build: Weather again (in an idle gap or on its tap); Clock and Home none');
      t.ok(!page(t, 'weather').stale, 'and kept fresh');
    } },
  ],
};
