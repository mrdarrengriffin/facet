// Release checklist dry-run (PRODUCT / plan 6.0, the lines an agent can run): every shipped screen on Home Assistant
// with its entities missing, with them unavailable, and with the network pulled then restored. Missing: no crash, no
// sample word. Unavailable / network pulled: none of the texts that came from the data stays on screen (no stale paint);
// restored: they come back without a rebuild. "From the data" = texts shown with the data that are not shown with
// the entities missing. No sample word with the entities missing or present is the leak rule's (tests/rules/leak.mjs,
// gate). Home Assistant is faked (tools/lib/fakeha.mjs FakeHA): nothing real is touched. The steps are exported: STATES
// (with data, loading, not found, unavailable, offline) is what the docs' states strip shows (site/scripts/shots.mjs),
// driven by the same fake, so the pictures are the states this test checks.
import fs from 'node:fs';
import { DEMO } from '../../tools/demo_ha.mjs';
const SCHEMA = JSON.parse(fs.readFileSync(new URL('../../catalogue/schema.json', import.meta.url)));
const W = 'weather.test_home';  // the demo set has no weather entity: the test gives one (NOT a real entity)
// screens that show data from the fake HA (its entities, and its replies: weather.get_forecasts, calendar.get_events,
// the Energy dashboard's WebSocket reads over the panel's own link); Clock, Moon and Timer are the panel's own
export const DATA = ['weather', 'weather_days', 'calendar', 'media', 'energy', 'light'];
const C = 'calendar.test_family';  // a calendar the fake knows (NOT a real entity)
export const SHIPPED = ['clock', 'weather', 'weather_days', 'calendar', 'media', 'energy', 'light', 'moon', 'timer'];
export const props = (id) => {
  const p = { screen: id };
  const ps = SCHEMA.screens[id]?.params ?? {};
  if (ps.source?.options?.includes('ha')) p.source = 'ha';
  for (const [k, v] of Object.entries(ps)) {  // each entity param: the first demo entity of its domain (as the leak rule)
    if (v.type !== 'entity' && v.type !== 'entities') continue;
    const e = Object.keys(DEMO).find((x) => x.startsWith(`${v.domain}.`)) ?? (v.domain === 'weather' ? W : v.domain === 'calendar' ? C : null);
    if (e) p[k] = e;
  }
  return p;
};
export const ENTITIES = () => ({ [W]: { '': 'cloudy', temperature: '10.2', humidity: '81', wind_speed: '12' },
  [C]: { '': 'off', friendly_name: 'Family' }, 'media_player.test_tv': { '': 'playing', media_title: 'Test track', friendly_name: 'Test TV' },
  'sensor.test_grid_price': { '': '0.275', unit_of_measurement: 'GBP/kWh' }, 'sensor.test_grid_power': { '': '420', unit_of_measurement: 'W' } });
// a feed refreshes at least once a minute: a minute of panel time, so every screen has had its next round. Energy
// times its reads by the wall clock (time()), so the wall clock moves the same minute (Date.now, restored after each test).
// The clock starts at today's noon, not now: a run near midnight crossed into tomorrow mid-test ("Tmrw", today's
// energy total back to zero) and failed on a day edge, not on the screen
const realNow = Date.now;
let ahead = 0, pinned = false;
const noon = () => { const d = new Date(realNow()); d.setHours(12, 0, 0, 0); return d.getTime() - realNow(); };
const pin = () => { if (!pinned) { pinned = true, ahead = noon(); Date.now = () => realNow() + ahead; } };
export const wait = (t) => { pin(); ahead += 62000; t.b.tick(Math.ceil(62000 / 16)); };
export async function show(t, id, missing) {
  pin();
  const get = t.ha.get.bind(t.ha);
  t.ha.answers = true, t.ha.missing = !!missing, t.ha.fail = false;
  Object.assign(t.ha.s, ENTITIES());
  if (missing) t.ha.get = () => undefined;  // Home Assistant has none of the entities the screen asks for
  try {
    await t.mount('spec', props(id));
    // Energy reads HA's Energy dashboard over the panel's own link: linked to the fake (NOT a real Home Assistant)
    JSON.parse(t.b.spec('act', { do: 'ha.connect', url: 'http://fake-ha.test:8123', token: 'testtoken1234' }) || '{}');
    wait(t);
    for (let i = 0; i < 2; i++) { t.ha.push(); wait(t); }  // what the screen wanted once the link answered (the speakers, the calendars it found) answers too
  } finally { t.ha.get = get; }
  return t.texts();
}
/** the panel clock back to the wall clock (after a run that used wait) */
export const realClock = () => { Date.now = realNow, ahead = 0, pinned = false; };
/** every entity the mounted screen reads (as it asks for them now) */
export const wanted = (t) => [...new Set(t.ha.wants().map(([e]) => e))];
/** Home Assistant up but the screen's entities unavailable: their states, and its replies fail too (the forecast, the
 *  events, the Energy dashboard) */
export function unavailable(t, wants = wanted(t)) { t.ha.fail = true; for (const e of wants) t.ha.unavailable(e); wait(t); }
/** the entities back as they were */
export function available(t, wants) {
  t.ha.fail = false;
  for (const e of wants) { delete t.ha.s[e]; if (ENTITIES()[e]) t.ha.s[e] = ENTITIES()[e]; }
  t.ha.push(); wait(t);
}
/** the network pulled / restored */
export function offline(t) { t.ha.disconnect(); wait(t); }
export function online(t) { t.ha.reconnect(); wait(t); }
/** just built, nothing from Home Assistant yet: no states, no replies (what a screen shows while it waits) */
export async function loading(t, id) {
  pin();
  const get = t.ha.get.bind(t.ha);
  t.ha.answers = false, t.ha.missing = false, t.ha.fail = false;
  t.ha.get = () => undefined;
  try { await t.mount('spec', props(id)); } finally { t.ha.get = get; }
}
// the states a shipped screen is shown in, in the words the panel uses (a missing entity = "Not found", an
// unavailable one = "Unavailable", the same everywhere): to(t, id) mounts screen id on a fresh test context in it
export const STATES = [
  { id: 'data', label: 'With data', to: (t, id) => show(t, id, false) },
  { id: 'loading', label: 'Loading', to: (t, id) => loading(t, id) },
  { id: 'missing', label: 'Not found', to: (t, id) => show(t, id, true) },
  { id: 'unavailable', label: 'Unavailable', to: async (t, id) => { await show(t, id, false); unavailable(t); } },
  { id: 'offline', label: 'Offline', to: async (t, id) => { await show(t, id, false); offline(t); } },
];
const tests = SHIPPED.map((id) => ({
  name: `${id}: missing, unavailable, network pulled and restored`,
  async run(t) { try { await check(t, id); } finally { realClock(); } },
}));
// texts with their digits masked: the same data after a minute is the same text (the time, a growing total)
const shape = (xs) => [...new Set(xs.map((x) => x.replace(/\d+/g, '#')))];
async function check(t, id) {
  {
    const none0 = await show(t, id, true);  // built (a crash throws) and draws its empty / loading state
    const none = shape(none0);
    t.ok(none0.length > 0, `${id} missing: the screen draws (${none0.join(' | ')})`);
    t.log.push(`${id} missing: ${none0.join(' | ')}`);
    const ok = shape(await show(t, id, false));
    const data = ok.filter((x) => !none.includes(x));
    const wants = wanted(t);
    if (DATA.includes(id)) t.ok(data.length > 0, `${id}: its data shows (else this test checks nothing)`);
    t.log.push(`${id}: ${wants.length} entities, data texts: ${data.join(' | ')}`);
    unavailable(t, wants);
    const un = shape(t.texts());
    t.eq(data.filter((x) => un.includes(x)), [], `${id} unavailable: no stale paint`);
    available(t, wants);
    t.eq(data.filter((x) => !shape(t.texts()).includes(x)), [], `${id} available again: the data back`);
    offline(t);
    const off = shape(t.texts());
    t.eq(data.filter((x) => off.includes(x)), [], `${id} network pulled: no stale paint`);
    online(t);
    t.eq(data.filter((x) => !shape(t.texts()).includes(x)), [], `${id} network restored: the data back, no rebuild`);
  }
}
export default { id: 'release_states', family: 'device', about: 'Release dry-run: every shipped screen with its entities missing, unavailable, the network pulled and restored.', tests };
