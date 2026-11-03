// Clock screen (specs/clock.json: Page > Header > Body: Clock orbit (the ring + three orbiting dots) with the Time in
// its centre slot: digits and the short date) and its Home card (C52) on the clock feed panel.clock
// (core/clock_source.cpp: the device clock). The Clock shows no weather and reads nothing from Home Assistant (like Moon
// and Timer). Home Assistant is faked (tools/functional.mjs FakeHA). Approved original: Astro
// packages/ui/src/screens/clock.ts (orbitFace).
async function open(t) { await t.mount('spec', { screen: 'clock' }); return t; }
const deck = () => ({ v: 2, rev: 9, start: 'h', pages: [
  { id: 'h', kind: 'home', name: 'Test', cards: [{ t: 'page', page: 'c', size: 'm' }] },
  { id: 'c', kind: 'clock' }] });
const DATE = /^(MON|TUE|WED|THU|FRI|SAT|SUN) \d{1,2} [A-Z]{3}$/;

export default {
  id: 'clock_screen', family: 'screens', about: 'The Clock screen: the orbit ring, the time with the date (no weather, no Home Assistant), Home card.',
  tests: [
    { name: 'Home card picture: an analogue face telling the card\'s own time', async run(t) {
      await t.device(); t.wait(500);
      const c = t.part('card');
      t.ok(c && c.texts.some((x) => /^\d{1,2}:\d\d/.test(x)), `the Clock card: ${JSON.stringify(c?.texts)}`);
    } },
    { name: 'the ring, the time and the date centred in it; no weather line', async run(t) {
      await open(t);
      const o = t.part('clock_orbit');
      t.ok(o && o.h > 0 && o.w >= o.h, 'the orbit box');
      const tx = t.texts();
      t.has(tx, DATE, 'the short date');
      t.hasNot(tx, / · /, 'no weather line');
      const tm = t.comps().filter((c) => c.ids.includes('time')).find((c) => c.y > o.y);
      t.ok(tm, 'the Time in the slot');
      t.ok(Math.abs(tm.x + tm.w / 2 - (o.x + o.w / 2)) <= 2 && Math.abs(tm.y + tm.h / 2 - (o.y + o.h / 2)) <= 2, 'centred in the ring');
    } },
    { name: 'reads nothing from Home Assistant', async run(t) {
      await open(t);
      t.wait(3000);
      t.eq(t.ha.wants().map(([e]) => e).filter((e) => !e.startsWith('panel.') && !e.startsWith('facet.')), [], 'no entity wanted');
      t.hasNot(t.texts(), /^(NOT FOUND|UNAVAILABLE|Not found|Unavailable)$/, 'no missing / unavailable word');
      t.has(t.texts(), DATE, 'the date');
    } },
    { name: 'Home card (C52): the time and the date ("Tue 3 Nov") on the clock feed, no weather line', async run(t) {
      await t.device(deck());
      const c = t.part('card');
      t.ok(c, 'the page card');
      t.ok(c.texts.some((x) => /^\d{2}:\d{2}$/.test(x)), 'the time on the card');
      t.ok(!c.texts.some((x) => / · /.test(x)), 'no weather line on the card');
      const now = new Date(), day = `${'Sun Mon Tue Wed Thu Fri Sat'.split(' ')[now.getDay()]} ${now.getDate()} ${'Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec'.split(' ')[now.getMonth()]}`;
      t.ok(c.texts.includes(day), `the date on the card: ${day} (${c.texts.join(' / ')})`);
    } },
  ],
};
