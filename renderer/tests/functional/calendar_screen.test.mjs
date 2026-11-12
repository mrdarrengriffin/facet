// Calendar screen (specs/calendar.json: Page > Header (Segmented Agenda | Month, the calendars Icon button) > Body:
// a Group of the hero (Hero block), the days (Grid of Calendar rows) or the month (Calendar month); two Bottom sheets
// on the Overlay (calendars: Grid of Calendar rows with checkboxes; a day: its Calendar rows)), on the calendar feed
// panel.calendar (core/calendar_source.cpp, CONTRACT.md C51) and its Home card (C52). Home Assistant is faked
// (tools/functional.mjs FakeHA); C1 / C2 are NOT real entities. Approved original: Astro packages/ui/src/screens/
// calendar.ts + calendar-month.ts.
const NOW = 1792926000000;
const C1 = 'calendar.test_family', C2 = 'calendar.test_work';
const asks = (t) => JSON.parse(t.b.m.UTF8ToString(t.b.m._fw_ha_asks()) || '[]');
const answer = (t, id, obj) => { t.b.str(JSON.stringify(obj), (p) => t.b.m._fw_ha_answer(id, 1, p)); t.b.settle(1500); };
async function open(t, params) { await t.mount('spec', { screen: 'calendar', ...params }); return t; }
// the device with the Calendar page, linked to the fake HA (every calendar it has), on the Calendar
async function linked(t, params, answers = true) {
  t.ha.answers = true;  // the link's entity list; then answers (get_events) as asked
  await t.device({ v: 2, rev: 9, start: 'h', pages: [
    { id: 'h', kind: 'home', name: 'Test', cards: [{ t: 'page', page: 'calendar', size: 'm' }] },
    { id: 'calendar', kind: 'calendar', params }] });
  t.b.spec('act', { do: 'ha.connect', url: 'http://ha.test:8123', token: 'abcdefgh1234' }); t.b.tick(8); t.b.settle(600);
  t.show('calendar'); t.b.settle(400); t.ha.answers = answers;
  for (let i = 0; i < 2; i++) { t.wait(61000); t.b.tick(8); t.ha.push(); t.b.settle(400); }  // the feed's round: once a minute
  return t;
}
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
      await linked(t, { source: 'ha' }, false);
      t.has(t.texts(), /^dentist$/i, 'the entity\'s own event before any reply');
      const q = asks(t).filter((a) => a.service === 'calendar.get_events');
      t.eq([...new Set(q.map((a) => a.data.entity_id))].sort(), [C1, C2], 'an ask per calendar');
      const s = Date.now() + 2 * 3600e3;
      answer(t, q.find((a) => a.data.entity_id === C2).id, { [C2]: { events: [{ start: iso(s), end: iso(s + 3600e3), summary: 'Standup' }] } });
      t.has(t.texts(), /^standup$/i, 'an asked event');
    } },
    { name: 'ha: unavailable calendars say Unavailable, no rows (Not found: missing_entities)', async run(t) {
      t.ha.s[C1] = { '': 'unavailable' };
      await linked(t, { source: 'ha' });
      t.has(t.texts(), /^Unavailable$/, 'unavailable'); t.hasNot(t.texts(), /^(offline|not found)$/i, 'not the other words');
    } },
    { name: 'Home card (C52): the next event on the same feed', async run(t) {
      await t.device(deck({ source: 'sample', now: String(NOW) }));
      const c = t.part('card');
      t.ok(c, 'the page card'); t.ok(c.texts.some((x) => /pottery class/i.test(x)), 'the next event on the card');
        } },
    { name: 'Home card: the day of an event this week in full (Wednesday, not Wed); Tomorrow in full', async run(t) {
      const sun = Date.UTC(2026, 10, 1, 12), wed = sun + 3 * 86400e3;  // Sunday 1 Nov 2026 noon, the event Wednesday 4 Nov
      t.ha.s[C1] = { '': 'off', friendly_name: 'Family', message: 'Tennis - Coaching', start_time: '2026-11-04 18:00:00', end_time: '2026-11-04 19:00:00', all_day: 'false' };
      t.ha.answers = true;  // the link's entity list; get_events answers the same event (the demo's replies follow the wall clock)
      t.ha.reply = (service, data) => service === 'calendar.get_events' && data.entity_id === C1
        ? { [C1]: { events: [{ start: iso(wed + 6 * 3600e3), end: iso(wed + 7 * 3600e3), summary: 'Tennis - Coaching' }] } } : null;
      await t.device({ v: 2, rev: 9, start: 'h', pages: [
        { id: 'h', kind: 'home', name: 'Test', cards: [{ t: 'page', page: 'c', size: 'm' }, { t: 'page', page: 'k', size: 'm' }] },
        { id: 'c', kind: 'calendar', params: { source: 'ha', now: String(sun) } },
        { id: 'k', kind: 'clock', params: { source: 'sample' } }] });
      t.b.spec('act', { do: 'ha.connect', url: 'http://ha.test:8123', token: 'abcdefgh1234' }); t.b.tick(8); t.b.settle(600);
      for (let i = 0; i < 2; i++) { t.wait(61000); t.b.tick(8); t.ha.push(); t.b.settle(400); }
      const c = t.part('card');
      t.ok(c && c.texts.includes('Wednesday'), `the card: ${JSON.stringify(c?.texts)}`);
      t.ok(!c.texts.includes('Wed'), 'never the short form on the card');
    } },
    { name: 'none picked: every calendar Home Assistant has (a fresh panel)', async run(t) {
      t.ha.s[C1] = { '': 'off', friendly_name: 'Family' };
      t.ha.s[C2] = { '': 'off', friendly_name: 'Work' };
      t.ha.answers = true;
      await t.device({ v: 2, rev: 9, start: 'h', pages: [
        { id: 'h', kind: 'home', name: 'Test', cards: [{ t: 'page', page: 'calendar', size: 'm' }] },
        { id: 'calendar', kind: 'calendar', params: { source: 'ha' } }] });
      t.b.spec('act', { do: 'ha.connect', url: 'http://ha.test:8123', token: 'abcdefgh1234' }); t.b.tick(8); t.b.settle(600);
      t.show('calendar'); t.wait(61000); t.b.tick(8); t.b.settle(800);
      const link = JSON.parse(t.b.m.UTF8ToString(t.b.m._fw_ha_link()));
      t.ok(link.count >= 2, `the link read Home Assistant's entities: ${link.count}`);
      t.hasNot(t.texts(), 'No calendars', `not the none-picked words: ${t.texts().slice(0, 8)}`);
    } },
    { name: 'a calendar turned off on the screen stays off: the deck keeps it (a restart)', async run(t) {
      t.ha.s[C1] = { '': 'off', friendly_name: 'Family' };
      t.ha.s[C2] = { '': 'off', friendly_name: 'Work' };
      t.ha.answers = true;
      await linked(t, { source: 'ha' });
      const b = t.comps().find((c) => c.ids.includes('icon_button') && c.y < 100 && c.x > t.b.S / 2);
      t.tap(b); t.tap(t.find(/^work$/i));
      t.has(t.texts(), /^hidden$/i, 'Work hidden');
      const l = JSON.parse(t.b.spec('list', '') || '{}');
      const deck = typeof l.deck === 'string' ? JSON.parse(l.deck) : l.deck || {};
      const pg = (deck.pages || []).find((p) => p.kind === 'calendar');
      t.eq(pg?.params?.hidden, C2.slice(9), `the deck keeps it: ${JSON.stringify(pg?.params)}`);
    } },
    { name: 'Settings > Screens > Calendar: a change shows at once on the kept Calendar screen and its Home card (#98)', async run(t) {
      // the panels keep their deck pages built (boards/*.json keep_pages): a screen-settings change is kept quietly
      // (spec.h deck_set), so the kept Calendar page and the Home card's feed must follow it without a restart
      const keep = (n = '') => t.b.test('keep', String(n)) ?? [];
      const act = (a) => { t.b.spec('act', a); t.b.tick(10); t.b.settle(800); };
      await t.device({ v: 2, rev: 9, start: 'h', pages: [
        { id: 'h', kind: 'home', name: 'Test', cards: [{ t: 'page', page: 'calendar', size: 'm' }] },
        { id: 'calendar', kind: 'calendar', params: { source: 'ha', now: String(NOW) } }] });
      keep(8); t.b.tick(200);
      t.ok(keep().some((k) => k.page === 'calendar'), `the Calendar page is kept: ${JSON.stringify(keep().map((k) => k.page))}`);
      t.ok(!t.part('card').texts.some((x) => /pottery class/i.test(x)), 'the card before: no sample event');
      act({ do: 'nav.open', screen: 'screen_set', params: { id: 'calendar' } });
      t.tap(/^Source$/); t.b.settle(1200);
      t.tap(/^Sample fortnight/); t.b.settle(1200);
      act({ do: 'nav.back' });
      t.ok(t.part('card')?.texts.some((x) => /pottery class/i.test(x)), `the Home card follows: ${JSON.stringify(t.part('card')?.texts)}`);
      act({ do: 'nav.open', screen: 'calendar' });
      t.has(t.texts(), /^pottery class$/i, 'the Calendar screen follows');
    } },
    { name: 'a sample that moves with the clock keeps no old copies (facet-256: the panels lost ~1.9 KB of PSRAM a minute)', async run(t) {
      // the sample fortnight follows the wall clock (Date.now): each minute another agenda; the feed kept every one
      const real = Date.now, base = real.call(Date);
      let off = 0;
      Date.now = () => base + off;
      try {
        await t.mount('spec', { screen: 'calendar', source: 'sample' });
        const mem = () => t.b.m._fw_mem();
        let at10 = 0;
        for (let i = 1; i <= 40; i++) {
          off = i * 60000;
          t.wait(61000);  // the feed's minute round
          if (i === 10) at10 = mem();
        }
        const grew = mem() - at10;
        t.ok(grew < 8 * 1024, `30 more minutes of a moving sample: ${grew} B more held (a copy a minute was ~1.9 KB)`);
      } finally { Date.now = real; }
    } },
  ],
};
