// Moon screen (specs/moon.json: Page > Header (the title per view) > Body: Column: now = the Moon phase hero (the moon,
// the phase and one line under it) + a "details" More row; details = six Stats (lit, age, moonrise, moonset, next full,
// next new) and the next four phases (a Grid of Moon phase columns) + a "next N days" More row; days = the evening line
// over a Bar chart of the evenings (marks over the full and new moons)), on the moon feed panel.moon
// (core/moon_source.cpp, CONTRACT.md C51) and its Home card (C52). Approved original: Astro
// packages/ui/src/screens/moon.ts (was moon_view, moon_facts, moon_phases, moon_range and specs moon_details /
// moon_days). The sample is 22 Oct 2026 21:00 UTC at the docs sample place.
async function open(t, params) { await t.mount('spec', { screen: 'moon', ...params }); return t; }
const more = (t) => t.comps().filter((c) => c.ids.includes('more_row')).pop();
const deck = (params) => ({ v: 2, rev: 9, start: 'h', pages: [
  { id: 'h', kind: 'home', name: 'Test', cards: [{ t: 'page', page: 'm', size: 'm' }] },
  { id: 'm', kind: 'moon', params }] });

export default {
  id: 'moon_screen', family: 'screens', about: 'The Moon screen on the moon feed: the moon tonight, the details, the next days chart, Back between views, the Home card.',
  tests: [
    { name: 'sample: the moon tonight as the hero, one line under it', async run(t) {
      await open(t, { source: 'sample' });
      const tx = t.texts();
      t.has(tx, /^moon$/i, 'the title'); t.has(tx, /^Waxing gibbous$/, 'the phase'); t.has(tx, /87% · full on Mon/, 'the line');
      const m = t.part('moon_icon');
      t.ok(m && m.h > 300 * t.b.S / 720, 'the moon is the hero');
      t.eq(t.ha.calls.length, 0, 'nothing goes to Home Assistant');
    } },
    { name: 'details: the numbers and the next four phases; Back returns to the moon', async run(t) {
      await open(t, { source: 'sample' });
      t.tap(more(t)); t.wait(400);
      const tx = t.texts();
      t.has(tx, /^moon details$/i, 'the title');
      for (const w of [/^lit$/i, /^age$/i, /^moonrise$/i, /^moonset$/i, /^next full$/i, /^next new$/i]) t.has(tx, w, `stat ${w}`);
      t.has(tx, /^87%$/, 'lit'); t.has(tx, /^12\.2 days$/, 'age'); t.has(tx, /^Mon · 04:11$/, 'next full');
      t.has(tx, /^full$/i, 'a full moon next'); t.has(tx, /^last quarter$/i, 'then last quarter'); t.has(tx, /^9 Nov$/, 'the new moon day');
      t.eq(t.comps().filter((c) => c.ids.includes('moon_icon')).length, 4, 'four phase moons');
      t.tap(t.part('back_button')); t.wait(400);
      t.has(t.texts(), /^Waxing gibbous$/, 'Back = the moon');
    } },
    { name: 'next days: one bar per evening with the moons marked; Back returns to the details', async run(t) {
      await open(t, { source: 'sample' });
      t.tap(more(t)); t.wait(400);
      t.tap(more(t)); t.wait(800);
      const tx = t.texts();
      t.has(tx, /^next \d+ days$/i, 'the title'); t.has(tx, /87% · tonight/, 'the evening now');
      t.has(tx, /^Nov$/, 'the month under the 1st');
      const c = t.part('bar_chart');
      t.ok(c && c.h > 300 * t.b.S / 720, 'the chart fills the view');
      t.tap(t.part('back_button')); t.wait(400);
      t.has(t.texts(), /^moon details$/i, 'Back = the details');
    } },
    { name: 'panel: worked out on the panel for a southern place', async run(t) {
      await open(t, { source: 'panel', lat: '-33.9', lon: '151.2' });
      t.has(t.texts(), /^(New moon|Waxing crescent|First quarter|Waxing gibbous|Full moon|Waning gibbous|Last quarter|Waning crescent)$/, 'a phase now');
      t.has(t.texts(), /\d+% · (full|new) /, 'the line');
    } },
    { name: 'Home card (C52): the moon, how much is lit and the phase on the same feed', async run(t) {
      await t.device(deck({ source: 'sample' }));
      const c = t.part('card');
      t.ok(c, 'the page card'); t.ok(c.texts.some((x) => /^87%$/.test(x)), 'lit on the card');
      t.ok(c.texts.some((x) => /^Waxing gibbous$/.test(x)), 'the phase on the card');
      t.ok(t.part('moon_icon'), 'the moon on the card');
    } },
  ],
};
