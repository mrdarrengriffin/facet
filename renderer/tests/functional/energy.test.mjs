// Energy screen (specs/energy.json, CONTRACT C51 feed panel.energy): today's view, and the Week view the More row
// "last 7 days" opens on the same screen (docs energy.ts weekView: header "Last 7 days", this week's cost, the week
// against the week before as pairs of bars with the day names under them); the screen's Back closes the Week view
// first (screen.param "back"), then leaves the screen.
const back = (t) => t.part('back_button');
const vis = (t, re) => t.objs().some((o) => o.text && t.visible(o) && re.test(String(o.text)));
const DAYS = /^(Mon|Tue|Wed|Thu|Fri|Sat|Sun|Today)$/;
// a deck whose Energy is on its sample day (a panel's factory Energy reads Home Assistant: no sample data)
const SAMPLE = { v: 2, rev: 1, start: 'home', pages: [{ id: 'home', kind: 'home', name: 'Home', cards: [{ t: 'page', page: 'energy', size: 'm' }] }, { id: 'energy', kind: 'energy', params: { source: 'sample' } }] };
const realNow = Date.now;
let ahead = 0;
const later = (t, ms) => { ahead += ms; Date.now = () => realNow() + ahead; t.wait(ms); };  // Energy times its reads by time()
const money = (t) => t.objs().filter((o) => o.text && t.visible(o) && /^\u00a3/.test(String(o.text))).map((o) => String(o.text));
// linked to the fake Home Assistant (NOT a real one), the day read: the cost shows
const linked = async (t) => {
  t.ha.answers = true;
  await t.mount('spec', { screen: 'energy', source: 'ha' });
  t.b.spec('act', { do: 'ha.connect', url: 'http://fake-ha.test:8123', token: 'testtoken1234' });
  t.b.m._fw_ha_connected(1);
  later(t, 11000); t.ha.push(); later(t, 62000);
};
const isMoney = (m) => /^\u00a3\d+\.\d\d$/.test(m);
const week = async (t) => { await t.device(SAMPLE); t.show('energy'); t.tap(/^last 7 days$/i); t.wait(900); };
export default {
  id: 'energy', family: 'charts', about: 'Energy screen: today, and the last 7 days as a view of the same screen.',
  tests: [
    { name: 'today: hero, rate, More row; no week parts', async run(t) {
      await t.device(SAMPLE); t.show('energy');
      t.ok(vis(t, /^today so far$/i), 'today so far');
      t.ok(vis(t, /^last 7 days$/i), 'More row');
      t.ok(!vis(t, /^this week$/i), 'no week hero');
      t.ok(!vis(t, /^week before$/i), 'no week legend');
    } },
    { name: 'last 7 days opens the Week view on the same screen', async run(t) {
      await week(t);
      t.eq(t.where(), 'energy', 'still the Energy screen');
      t.ok(vis(t, /^Last 7 days$/i), 'header Last 7 days');
      t.ok(!vis(t, /^Energy$/i), 'header Energy gone');
      t.ok(vis(t, /^this week$/i), 'week hero label');
      t.ok(vis(t, /^week before$/i), 'legend: week before');
      t.ok(vis(t, /a day than the week before|kWh/), 'week line');
      t.ok(!vis(t, /^today so far$/i), 'today hidden');
      t.ok(!vis(t, /W now$/), 'now chip hidden');
      const ds = t.objs().filter((o) => o.text && t.visible(o) && DAYS.test(String(o.text)));
      t.eq(ds.length, 7, `7 day names: ${ds.map((o) => o.text).join(' ')}`);
      t.eq(String(ds.at(-1)?.text), 'Today', 'today last');
      t.ok(ds.every((o, i) => !i || o.x > ds[i - 1].x), 'left to right');
      const b = t.part('bar_chart');
      t.ok(b && ds.every((o) => o.x >= b.x - 1 && o.x + o.w <= b.x + b.w + 1), 'day names inside the chart width');
    } },
    { name: 'Back closes the Week view first, then leaves', async run(t) {
      await week(t);
      t.tap(back(t)); t.wait(900);
      t.eq(t.where(), 'energy', 'Back 1: still Energy');
      t.ok(vis(t, /^today so far$/i), 'today again');
      t.ok(vis(t, /^Energy$/i), 'header Energy');
      t.ok(!vis(t, /^this week$/i), 'week gone');
      t.tap(back(t)); t.wait(900);
      t.ok(t.where() !== 'energy', `Back 2 leaves (${t.stack().join(' > ')})`);
    } },
    { name: 'a refresh in flight keeps the cost: a live change mid-read never shows £0.00 (#62)', async run(t) {
      try {
        t.ha.answers = true;
        await t.mount('spec', { screen: 'energy', source: 'ha' });
        t.b.spec('act', { do: 'ha.connect', url: 'http://fake-ha.test:8123', token: 'testtoken1234' });
        t.b.m._fw_ha_connected(1);
        later(t, 11000); t.ha.push(); later(t, 62000);
        const before = money(t);
        t.ok(before.length && before.every((m) => /^\u00a3\d+\.\d\d$/.test(m)), `cost read: ${before.join(' ')} | ${t.texts().join(" / ").slice(0, 300)}`);
        t.ha.answers = false;          // the next minute's statistics read goes out and stays in flight
        later(t, 60000);
        for (const w of [510, 2327, 880]) { t.ha.set('sensor.test_grid_power', w, { unit_of_measurement: 'W' }); later(t, 2000); }
        t.eq(money(t), before, 'live changes while the read streams: the last cost stays');
        t.ha.answers = true;           // the read lands
        later(t, 3000);
        const after = money(t);
        t.ok(after.length && after.every((m) => /^\u00a3\d+\.\d\d$/.test(m)), `cost after the read: ${after.join(' ')}`);
      } finally { Date.now = realNow, ahead = 0; t.b.m._fw_ha_connected?.(0); }
    } },
    { name: 'one failed read keeps the last good day; Offline after 2 failed reads in a row (#132)', async run(t) {
      try {
        await linked(t);
        const before = money(t);
        t.ok(before.length && before.every(isMoney), `cost read: ${before.join(' ')}`);
        t.ha.fail = true;              // Home Assistant answers, but the reads fail
        later(t, 62000);
        t.eq(money(t), before, 'one failed read: the last cost stays');
        t.ok(!vis(t, /not reached/i), 'one failed read: not Offline');
        later(t, 62000);
        t.ok(money(t).every((m) => !isMoney(m)), `2 failed reads: no number (${money(t).join(' ')})`);
        t.ok(vis(t, /not reached/i), 'two failed reads: Home Assistant not reached');
        t.ha.fail = false;             // a read lands again
        later(t, 62000);
        t.ok(money(t).length && money(t).every(isMoney), `back: ${money(t).join(' ')}`);
        t.ok(!vis(t, /not reached/i), 'back: not Offline');
      } finally { Date.now = realNow, ahead = 0; t.b.m._fw_ha_connected?.(0); }
    } },
    { name: 'no read landing for 3 minutes is Offline, even with none failed (#132)', async run(t) {
      try {
        await linked(t);
        t.ha.answers = false;          // reads go out and never come back
        later(t, 120000);
        t.ok(money(t).every(isMoney) && !vis(t, /not reached/i), `2 minutes: the last cost (${money(t).join(' ')})`);
        later(t, 65000);
        t.ok(vis(t, /not reached/i), `over 3 minutes: Home Assistant not reached (${t.texts().join(' / ').slice(0, 160)})`);
      } finally { Date.now = realNow, ahead = 0; t.ha.answers = true; t.b.m._fw_ha_connected?.(0); }
    } },
  ],
};
