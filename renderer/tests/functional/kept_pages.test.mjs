// Kept pages (core/device.cpp, facet-243): Home and the deck's pages are built once in the idle gaps and kept hidden,
// so a tap shows a page instead of building it. A hidden page does no widget work: its data changes, its widgets wait
// (core/ha.h asleep) and catch up when it shows (wake). Scroll is kept. A spec / deck change rebuilds only the pages it
// touched. Home Assistant is faked (tools/functional.mjs FakeHA); W is the factory deck's weather entity.
import fs from 'node:fs';
const FACTORY = JSON.parse(fs.readFileSync(new URL('../fixtures/deck-full.json', import.meta.url)));
const W = 'weather.forecast_home';
const act = (t, a) => { const r = JSON.parse(t.b.spec('act', a) || '{}'); t.b.tick(10); t.b.settle(800); return r; };
const keep = (t, n = '') => t.b.test('keep', String(n)) ?? [];
const page = (t, id) => keep(t).find((k) => k.page === id);
const builds = (t) => t.b.test('stack')[0].builds;
const idle = (t, ms) => t.b.tick(Math.ceil(ms / 16));  // the prebuild runs in idle gaps (250 ms, one page each)

export default {
  id: 'kept_pages', family: 'device', about: 'Kept pages: shown without a build, data-only while hidden, scroll kept, only touched pages rebuilt.',
  tests: [
    { name: 'a tap made while a page builds (press and lift both inside the build) is given after it (#94)', async run(t) {
      t.ha.s[W] = { '': 'cloudy', temperature: '10.2' };
      await t.device();
      const card = t.part('card', /clock/i) || t.find(/^clock$/i);
      t.ok(card, 'Home has a Clock card');
      if (!card) return;
      keep(t, 8);
      const [x, y] = [Math.round(card.x + card.w / 2), Math.round(card.y + card.h / 2)];
      // the fake chip: read only from inside builds (between nodes and in the core atoms); press, a read, then lift
      t.b.test('chip', JSON.stringify({ steps: [`p ${x} ${y}`, '-', 'l'] }));
      let grew = null;
      for (let i = 0; i < 400 && !grew; i++) {
        const k0 = keep(t).filter((k) => k.page !== 'home').length, c0 = t.b.test('chip');  // Home is kept as shown: no build
        t.b.tick(1);
        const c1 = t.b.test('chip');
        if (keep(t).filter((k) => k.page !== 'home').length > k0) grew = { k: keep(t).map((k) => k.page), before: c0, after: c1 };
      }
      t.ok(grew, 'a kept page was built in the idle gap');
      t.eq(grew?.before.left, 3, `the tap was not read before that build (${JSON.stringify(grew?.before)})`);
      t.eq(grew?.after.left, 0, `press and lift both read inside that one build (${JSON.stringify(grew?.after)})`);
      t.ok(t.where() === 'home', `still Home right after the build (${t.where()})`);
      t.b.tick(10); t.b.settle(800);
      t.eq(t.where(), 'clock', 'the tap is given after the build: Clock opens');
      t.b.test('chip', JSON.stringify({ steps: [] }));
    } },
    { name: 'one big component builds with the chip read inside it (Moon: polls well past its nodes) (#94)', async run(t) {
      await t.device();
      t.b.test('chip', JSON.stringify({ steps: [] }));
      const p0 = t.b.test('chip').polls;
      act(t, { do: 'nav.open', screen: 'moon' });
      const n = t.b.test('chip').polls - p0;
      t.ok(n >= 20, `the chip read ${n} times while Moon built (one per core atom; was once per spec node)`);
    } },
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
    { name: 'a device mounted again while pages are kept: the old one goes cleanly, the new one keeps its own', async run(t) {
      await t.device();
      keep(t, 8);
      idle(t, 3000);
      t.b.rebuild('device', {});  // the bench and the workbench mount a device again
      idle(t, 3000);
      t.eq(keep(t).length, 8, 'the new device kept its 8 pages');
      act(t, { do: 'nav.open', screen: 'clock' });
      t.ok(page(t, 'clock').shown, 'and shows them');
    } },
    { name: 'a settings change, then a tap before the idle rebuild: built on the tap, never the stale page', async run(t) {
      await t.device();
      keep(t, 8);
      idle(t, 3000);
      t.b.test('setting', 'textSize=large');  // a look change: every kept page stale (the shown one rebuilt by its restyle)
      t.b.tick(3);  // no idle gap: the prebuild has not rebuilt anything yet
      t.ok(page(t, 'clock').stale, 'Clock is stale');
      const n = builds(t);
      act(t, { do: 'nav.open', screen: 'clock' });
      t.ok(builds(t) > n, 'the tap built Clock again');
      t.ok(page(t, 'clock').shown && !page(t, 'clock').stale, 'the shown Clock is the fresh one');
      t.eq(keep(t).filter((k) => k.page === 'clock').length, 1, 'one Clock, the stale one gone');
      t.b.test('setting', 'textSize=standard');
    } },
  ],
};
