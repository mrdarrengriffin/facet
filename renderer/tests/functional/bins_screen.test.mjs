// Bins screen (specs/bins.json: the next collection as the bins themselves (Bins next), "coming up" and "your bins"
// lists, two empty states) on the bins feed panel.bins (core/bins_source.cpp, CONTRACT.md C51, the Feed base) and its
// Home card (C52). Adapters: sample (docs only) and the Waste Collection Schedule calendar. Home Assistant is faked
// (tools/functional.mjs FakeHA); the calendar is NOT a real entity.
const CAL = 'calendar.test_bins';
const ymd = (days) => { const d = new Date(Date.now() + days * 86400000); return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`; };
async function open(t, params) { await t.mount('spec', { screen: 'bins', ...params }); return t; }
// the device with the Bins page, linked to the fake HA, on Bins; platform = the calendar's integration (null = none)
async function linked(t, platform, params = {}) {
  if (platform) t.ha.s[CAL] = { '': 'off', friendly_name: 'Bins', message: 'Green Bin', start_time: `${ymd(3)} 00:00:00`, _platform: platform };
  t.ha.answers = true;
  await t.device({ v: 2, rev: 9, start: 'h', pages: [
    { id: 'h', kind: 'home', name: 'Test', cards: [{ t: 'page', page: 'bins', size: 'm' }] },
    { id: 'bins', kind: 'bins', params }] });
  t.b.spec('act', { do: 'ha.connect', url: 'http://ha.test:8123', token: 'abcdefgh1234' }); t.b.tick(8); t.b.settle(600);
  t.show('bins'); t.b.settle(400);
  for (let i = 0; i < 2; i++) { t.wait(61000); t.b.tick(8); t.ha.push(); t.b.settle(400); }  // the feed's round: once a minute
  return t;
}

export default {
  id: 'bins_screen', family: 'screens', about: 'The Bins screen on the bins feed: the next collection, coming up, your bins, the Waste Collection Schedule adapter, the empty states.',
  tests: [
    { name: 'sample tonight: the bins to put out, coming up and your bins', async run(t) {
      await open(t, { source: 'sample', sample: 'tonight' });
      const tx = t.texts();
      t.has(tx, /^put out tonight$/i, 'the moment'); t.has(tx, /^coming up$/i, 'coming up'); t.has(tx, /^your bins$/i, 'your bins');
      t.hasNot(tx, /^(0|undefined|null)$/, 'no blank data');
    } },
    { name: 'sample none: no collections says so, no lists', async run(t) {
      await open(t, { source: 'sample', sample: 'none' });
      t.has(t.texts(), /^no bin collections found$/i, 'the empty state'); t.hasNot(t.texts(), /^your bins$/i, 'no lists');
    } },
    { name: 'Waste Collection Schedule: its calendar found by itself; the next collection live, the weeks after from get_events', async run(t) {
      const orig = t.ha.reply.bind(t.ha);
      let asked = 0;
      t.ha.reply = (svc, data) => svc !== 'calendar.get_events' ? orig(svc, data) : (asked++, { response: { [CAL]: { events: [
        { start: ymd(3), summary: 'Green Bin' }, { start: ymd(10), summary: 'Black Bin' }, { start: ymd(17), summary: 'Green Bin' }] } } });
      await linked(t, 'waste_collection_schedule');
      t.ok(asked >= 1, `asked get_events (${asked})`);
      t.has(t.texts(), /^green bin$/i, 'the next collection');
      t.has(t.texts(), /^coming up$/i, 'coming up once the weeks are known'); t.has(t.texts(), /^black bin$/i, 'a later collection');
    } },
    { name: 'no Waste Collection Schedule calendar: says how to set one up', async run(t) {
      await linked(t, null);
      t.has(t.texts(), /^no bin collections found$/i, 'the empty state');
    } },
  ],
};
