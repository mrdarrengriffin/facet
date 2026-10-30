// Clock screen (specs/clock.json: Page > Header > Body: Clock orbit (the ring + three orbiting dots) with the Time in
// its centre slot: digits, the short date and the weather line), the weather line on the weather feed panel.weather
// (brief) and its Home card (C52) on the clock feed panel.clock (core/clock_source.cpp: the device clock) + the same
// weather line. Home Assistant is faked (tools/functional.mjs FakeHA); W is NOT Darren's entity. Approved original:
// Astro packages/ui/src/screens/clock.ts (orbitFace; was the orbit face of components/clock_face.cpp).
const W = 'weather.test_home';
async function open(t, params) { await t.mount('spec', { screen: 'clock', ...params }); return t; }
const deck = (params) => ({ v: 2, rev: 9, start: 'h', pages: [
  { id: 'h', kind: 'home', name: 'Test', cards: [{ t: 'page', page: 'c', size: 'm' }] },
  { id: 'c', kind: 'clock', params }] });

export default {
  id: 'clock_screen', family: 'screens', about: 'The Clock screen: the orbit ring, the time with the date and the weather line, the HA adapter, Home card.',
  tests: [
    { name: 'sample: the ring, the time, the date and the weather line centred in it', async run(t) {
      await open(t, { source: 'sample' });
      const o = t.part('clock_orbit');
      t.ok(o && o.h > 0 && o.w >= o.h, 'the orbit box');
      const tx = t.texts();
      t.has(tx, /^(MON|TUE|WED|THU|FRI|SAT|SUN) \d{1,2} [A-Z]{3}$/, 'the short date');
      t.has(tx, /^[A-Z ]+ · -?\d+°$/, 'the weather line (condition · temperature)');
      const tm = t.comps().filter((c) => c.ids.includes('time')).find((c) => c.y > o.y);
      t.ok(tm, 'the Time in the slot');
      t.ok(Math.abs(tm.x + tm.w / 2 - (o.x + o.w / 2)) <= 2 && Math.abs(tm.y + tm.h / 2 - (o.y + o.h / 2)) <= 2, 'centred in the ring');
    } },
    { name: 'ha: the weather entity gives the line; unavailable = no line', async run(t) {
      t.ha.s[W] = { '': 'rainy', temperature: '13.4' };
      await open(t, { source: 'ha', weather: W });
      t.has(t.texts(), /^RAIN(Y)? · 13°$/, 'the line from HA');
      t.ha.s[W] = { '': 'unavailable' };
      await open(t, { source: 'ha', weather: W });
      t.hasNot(t.texts(), / · /, 'no weather line');
      t.has(t.texts(), /^(MON|TUE|WED|THU|FRI|SAT|SUN) \d{1,2} [A-Z]{3}$/, 'the date stays');
    } },
    { name: 'Home card (C52): the time on the clock feed + the weather line', async run(t) {
      await t.device(deck({ source: 'sample' }));
      const c = t.part('card');
      t.ok(c, 'the page card');
      t.ok(c.texts.some((x) => /^\d{2}:\d{2}$/.test(x)), 'the time on the card');
      t.ok(c.texts.some((x) => / · -?\d+°$/i.test(x)), 'the weather line on the card');
    } },
  ],
};
