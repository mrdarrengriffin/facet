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
const money = (t) => t.objs().filter((o) => o.text && t.visible(o) && /^~?\u00a3/.test(String(o.text))).map((o) => String(o.text));
// linked to the fake Home Assistant (NOT a real one), the day read: the cost shows
const linked = async (t) => {
  t.ha.answers = true;
  await t.mount('spec', { screen: 'energy', source: 'ha' });
  t.b.spec('act', { do: 'ha.connect', url: 'http://fake-ha.test:8123', token: 'testtoken1234' });
  t.b.m._fw_ha_connected(1);
  later(t, 11000); t.ha.push(); later(t, 62000);
};
const isMoney = (m) => /^\u00a3\d+\.\d\d$/.test(m);
// a time-of-use day as Home Assistant records it (#134): a grid import statistic with no cost statistic of its own, a
// price entity, and the cost sensor HA made for it (energy/info cost_sensors) pricing each 5 minutes at its own rate:
// 0.10 before 07:00, 0.30 after. The price now (noon) is 0.45: kWh x the price now would be wrong. Test ids only.
const TOU = { in: 'sensor.test_tou_in', cost: 'sensor.test_tou_in_cost', price: 'sensor.test_tou_price' };
const rate = (ms) => (new Date(ms).getHours() < 7 ? 0.10 : 0.30);
const touRows = (from, now, cost) => { const out = []; for (let t = from; t < now; t += 300000) out.push({ start: t, end: t + 300000, change: cost ? +(0.05 * rate(t)).toFixed(5) : 0.05 }); return out; };
const touWs = (costSensors) => (cmd) => {
  const ok = (result) => ({ id: 1, type: 'result', success: true, result });
  if (cmd.type === 'energy/get_prefs') return ok({ energy_sources: [{ type: 'grid', stat_energy_from: TOU.in, stat_energy_to: null, stat_cost: null, entity_energy_price: TOU.price, number_energy_price: null }], device_consumption: [] });
  if (cmd.type === 'energy/info') return ok({ cost_sensors: costSensors ? { [TOU.in]: TOU.cost } : {}, solar_forecast_domains: [] });
  if (cmd.type === 'recorder/statistics_during_period') {
    const r = {}, from = Date.parse(cmd.start_time), now = Date.now();
    for (const id of cmd.statistic_ids || []) if (id === TOU.in || id === TOU.cost) r[id] = touRows(from, now, id === TOU.cost);
    return ok(r);
  }
  return null;
};
const atNoon = () => { const d = new Date(realNow()); d.setHours(12, 0, 0, 0); ahead = d.getTime() - realNow(); Date.now = () => realNow() + ahead; };
const touDay = async (t, costSensors) => {
  atNoon();
  t.ha.wsx = touWs(costSensors);
  t.ha.s[TOU.price] = { '': '0.45', unit_of_measurement: 'GBP/kWh' };
  await linked(t);
  t.ha.push(); later(t, 62000);  // the price entity, wanted once the dashboard was read
  const now = Date.now(), d0 = new Date(now); d0.setHours(0, 0, 0, 0);
  const rows = touRows(+d0, now, true);
  return { cost: rows.reduce((a, r) => a + r.change, 0), kwh: rows.length * 0.05 };
};
import { demoWs } from '../../tools/demo_ha_replies.mjs';
const field = (t, a) => t.b.test('ha_get', `panel.energy|${a}`);
const at = (h) => { const d = new Date(realNow()); d.setHours(h, 0, 0, 0); ahead = d.getTime() - realNow(); Date.now = () => realNow() + ahead; };
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
    { name: 'cost = Home Assistant\'s cost sensor (each moment at its own rate), not kWh x the price now (#134)', async run(t) {
      try {
        const want = await touDay(t, true);
        // the read landed up to a minute before now: within a few 5-minute rows of the sum to now
        const near = (m) => m.length === 1 && /^\u00a3\d+\.\d\d$/.test(m[0]) && Math.abs(+m[0].slice(1) - want.cost) <= 0.05;
        t.ok(near(money(t)), `the day priced at the time of use: ${money(t)} ~ \u00a3${want.cost.toFixed(2)} (kWh x 0.45 now would be \u00a3${(want.kwh * 0.45).toFixed(2)})`);
        const was = money(t);
        t.ha.set(TOU.price, 'unknown');  // the price entity between rates: the cost does not blink
        later(t, 2000);
        t.eq(money(t), was, 'price unknown: the cost stays');
      } finally { Date.now = realNow, ahead = 0; t.ha.wsx = null; t.b.m._fw_ha_connected?.(0); }
    } },
    { name: 'no cost sensor: kWh x the price, marked as an estimate (~) (#134)', async run(t) {
      try {
        const want = await touDay(t, false);
        const m = money(t);
        t.ok(m.length === 1 && /^~\u00a3\d+\.\d\d$/.test(m[0]) && Math.abs(+m[0].slice(2) - want.kwh * 0.45) <= 0.1, `the estimate, marked: ${m} ~ ~\u00a3${(want.kwh * 0.45).toFixed(2)}`);
        t.ha.set(TOU.price, 'unknown');
        later(t, 2000);
        t.eq(money(t), m, 'price unknown: the last price, no blink');
      } finally { Date.now = realNow, ahead = 0; t.ha.wsx = null; t.b.m._fw_ha_connected?.(0); }
    } },
    { name: 'incremental statistics reads: the day is the same as a full read, at a fraction of the bytes (#133)', async run(t) {
      try {
        at(18);  // late in the day: a full read is at its largest
        let cut = () => Date.now() - 3 * 60000;  // the data HA has: up to 3 minutes ago (5-minute rows compile late)
        const reads = [];
        const all = [];  // every WebSocket read's time: the connections under the panel's 15 s reuse (fw_device.h WS_IDLE_MS)
        t.ha.wsx = (cmd) => {
          all.push(Date.now());
          if (cmd.type !== 'recorder/statistics_during_period') return null;
          const r = demoWs(cmd, { prefix: 'test_', now: cut() });
          // what a full read (yesterday's midnight to now) would have cost at this moment
          const d0 = new Date(Date.now()); d0.setHours(0, 0, 0, 0); d0.setDate(d0.getDate() - 1);
          const full = demoWs({ ...cmd, start_time: d0.toISOString() }, { prefix: 'test_', now: cut() });
          if (cmd.period === '5minute') reads.push({ at: Date.now(), bytes: JSON.stringify(r).length, full: JSON.stringify(full).length });
          return r;
        };
        await linked(t);
        const n0 = reads.length;
        for (let i = 0; i < 60; i++) later(t, 60000);  // an hour: a read a minute, a full one every 15
        const hour = reads.slice(n0), b = hour.reduce((a, r) => a + r.bytes, 0), f = hour.reduce((a, r) => a + r.full, 0);
        const ws = all.filter((x) => x >= hour[0].at - 1000), conns = ws.filter((x, i) => !i || x - ws[i - 1] > 15000).length;
        t.log.push(`one hour from 18:00: ${hour.length} statistics reads, ${b} bytes (a full read each time: ${f} bytes, ${(100 * b / f).toFixed(0)}%); ${ws.length} WebSocket reads = ${ws.length} connections before, ${conns} with a round's reads on one`);
        t.ok(hour.length >= 55, `a read a minute (${hour.length})`);
        t.ok(b < f * 0.35, `incremental bytes ${b} under 35% of full ${f}`);
        const T = Date.now(); cut = () => T - 3 * 60000;  // HA's data stops moving: the partial reads settle on it
        later(t, 60000); later(t, 60000);
        const A = { values: field(t, 'values'), cost: field(t, 'cost') };
        // a fresh panel at the same data: one full read
        t.b.m._fw_ha_connected?.(0);
        await linked(t);
        const B = { values: field(t, 'values'), cost: field(t, 'cost') };
        t.ok(A.values && A.cost, `the fields read (${JSON.stringify(A).slice(0, 80)})`);
        t.eq(A, B, 'the half-hours and the cost after partial reads = a full read of the same data');
      } finally { Date.now = realNow, ahead = 0; t.ha.wsx = null; t.b.m._fw_ha_connected?.(0); }
    } },
  ],
};
