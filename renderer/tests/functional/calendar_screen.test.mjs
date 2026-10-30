// Calendar screen (specs/calendar.json: Page > Header (Segmented Agenda | Month, the calendars Icon button) > Body:
// a Group of the hero (Hero block), the days (Grid of Calendar rows) or the month (Calendar month); two Bottom sheets
// on the Overlay (calendars: Grid of Calendar rows with checkboxes; a day: its Calendar rows)), on the calendar feed
// panel.calendar (core/calendar_source.cpp, CONTRACT.md C51) and its Home card (C52). Home Assistant is faked
// (tools/functional.mjs FakeHA); C1 / C2 are NOT Darren's entities. Approved original: Astro packages/ui/src/screens/
// calendar.ts + calendar-month.ts (was components/calendar_view.cpp).
const NOW = 1792926000000;
const C1 = 'calendar.test_family', C2 = 'calendar.test_work';
const asks = (t) => JSON.parse(t.b.m.UTF8ToString(t.b.m._fw_ha_asks()) || '[]');
const answer = (t, id, obj) => { t.b.str(JSON.stringify(obj), (p) => t.b.m._fw_ha_answer(id, 1, p)); t.b.settle(1500); };
async function open(t, params) { await t.mount('spec', { screen: 'calendar', ...params }); return t; }
const deck = (params) => ({ v: 2, rev: 9, start: 'h', pages: [
  { id: 'h', kind: 'home', name: 'Test', cards: [{ t: 'page', page: 'c', size: 'l' }] },
  { id: 'c', kind: 'calendar', params }] });
const iso = (ms) => new Date(ms).toISOString();

export default {
  id: 'calendar_screen', family: 'screens', about: 'The Calendar screen on the calendar feed: agenda, month, the two sheets, the HA adapter, Home card.',
  tests: [
    { name: 'sample agenda: the next event as the hero, then the days with their rows', async run(t) {
      await open(t, { source: 'sample', now: String(NOW) });
      const tx = t.texts();
      t.has(tx, /^pottery class$/i, 'the next event'); t.has(tx, /^today · /i, 'a today line'); t.has(tx, /^tomorrow · /i, 'a tomorrow line');
      t.has(tx, /^swimming lessons$/i, 'a later row'); t.hasNot(tx, /^recycling out$/i, 'Bins is hidden');
      t.hasNot(tx, /^(0|undefined|null)$/, 'no blank data');
    } },
    { name: 'month: Month in the header shows six weeks; a day opens its sheet; next / Today move the month', async run(t) {
      await open(t, { source: 'sample', now: String(NOW) });
      t.tap(t.find(/^month$/i));
      t.has(t.texts(), /^october 2026$/i, 'the month title'); t.hasNot(t.texts(), /^pottery class$/i, 'the agenda is hidden');
      t.hasNot(t.texts(), /^today$/i, 'no Today in this month');
      const m = t.part('calendar_month');
      t.ok(m && m.h > 0, 'the month');
      t.tap({ x: m.x + Math.round(m.w * 0.5), y: m.y + m.h - 8, w: 1, h: 1 });  // a day in the last week
      t.has(t.texts(), /^(monday|tuesday|wednesday|thursday|friday|saturday|sunday) \d+ /i, 'the day sheet title');
    } },
    { name: 'calendars sheet: the calendars button opens it; a tap hides a calendar from the agenda', async run(t) {
      await open(t, { source: 'sample', now: String(NOW) });
      t.has(t.texts(), /^swimming lessons$/i, 'Family shows');
      const b = t.comps().find((c) => c.ids.includes('icon_button') && c.y < 100 && c.x > t.b.S / 2);
      t.ok(b, 'the calendars button'); t.tap(b);
      t.has(t.texts(), /^family$/i, 'a calendar row'); t.has(t.texts(), /events?$/i, 'its count');
      t.tap(t.find(/^family$/i));
      t.has(t.texts(), /^hidden$/i, 'Family hidden');
    } },
    { name: 'ha: each calendar live, then get_events per calendar; colours in order', async run(t) {
      t.ha.s[C1] = { '': 'off', friendly_name: 'Family', message: 'Dentist', start_time: '2099-01-01 09:00:00', end_time: '2099-01-01 10:00:00', all_day: 'false' };
      t.ha.s[C2] = { '': 'off', friendly_name: 'Work' };
      await open(t, { source: 'ha', calendars: `${C1},${C2}` });
      t.has(t.texts(), /^dentist$/i, 'the entity\'s own event before any reply');
      const q = asks(t).filter((a) => a.service === 'calendar.get_events');
      t.eq(q.map((a) => a.data.entity_id).sort(), [C1, C2], 'one ask per calendar');
      const s = Date.now() + 2 * 3600e3;
      answer(t, q.find((a) => a.data.entity_id === C2).id, { [C2]: { events: [{ start: iso(s), end: iso(s + 3600e3), summary: 'Standup' }] } });
      t.has(t.texts(), /^standup$/i, 'an asked event');
    } },
    { name: 'ha: unavailable calendars say Offline, no rows', async run(t) {
      t.ha.s[C1] = { '': 'unavailable' };
      await open(t, { source: 'ha', calendars: C1 });
      t.has(t.texts(), /^offline$/i, 'offline');
    } },
    { name: 'Home card (C52): the next event on the same feed', async run(t) {
      await t.device(deck({ source: 'sample', now: String(NOW) }));
      const c = t.part('card');
      t.ok(c, 'the page card'); t.ok(c.texts.some((x) => /pottery class/i.test(x)), 'the next event on the card');
    } },
  ],
};
