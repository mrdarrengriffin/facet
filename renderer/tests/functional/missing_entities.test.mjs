// The two states of an entity a screen reads (CONTRACT C38, C51a), on every screen that reads one and on the Home
// cards: not in Home Assistant = "Not found"; there but with no state now (unavailable / unknown) = "Unavailable";
// never one for the other. Missing is the real path: Home Assistant never answers an entity it does not have, so a
// state still unanswered ha::MISSING_MS (10 s) after it was wanted while connected turns missing (""); not connected
// (the workbench, a panel before Home Assistant connects) nothing turns missing. Then the same entities unavailable,
// then back with a state (neither word). Run at each density (--den 1.5x, --den 1x). Home Assistant is faked
// (tools/functional.mjs FakeHA absent / set); every entity here is a test one, NOT a real entity.
const W = 'weather.test_home', C = 'calendar.test_family', P = 'media_player.test_tv';
const ROOM = ['light.living_room_ceiling', 'light.reading_lamp', 'light.sofa_lamp'];
// screen: [params, the entities it reads, their state back, how many times the word shows (at least)]
const SCREENS = {
  weather: [{ source: 'ha', weather: W }, { [W]: { '': 'rainy', temperature: '13.4' } }, 1],
  weather_days: [{ source: 'ha', weather: W }, { [W]: { '': 'rainy', temperature: '13.4' } }, 1],
  calendar: [{ source: 'ha' }, { [C]: { '': 'off', friendly_name: 'Family' } }, 1],
  media: [{ source: 'ha' }, { [P]: { '': 'playing', media_title: 'Test track', friendly_name: 'Test TV' } }, 1],  // no speaker in HA: the label; then the player's
  energy: [{ source: 'ha' }, { 'sensor.test_grid_power': { '': '420', unit_of_measurement: 'W' }, 'sensor.test_grid_price': { '': '0.275', unit_of_measurement: 'GBP/kWh' } }, 2],  // the live pill + the rate
  light: [{ entity: 'light.test_lamp', name: 'Ceiling' }, { 'light.test_lamp': { '': 'on', brightness: '128', friendly_name: 'Lamp' } }, 1],
  living_room: [{}, Object.fromEntries(ROOM.map((e) => [e, { '': 'off' }])), 3],
};
const HOME = ['weather', 'calendar', 'media'];  // the cards that fit one Home page; Energy's on its own (Clock, Moon, Timer read no entity)
const words = (t) => t.texts().map((s) => s.toLowerCase());
const count = (t, w) => words(t).filter((s) => s === w.toLowerCase()).length;
const realNow = Date.now;
let ahead = 0;
// panel time; the wall clock moves with it (Energy times its reads by time())
const wait = (t, ms) => { ahead += ms; Date.now = () => realNow() + ahead; t.wait(ms); };
async function open(t, mount, ents) {
  t.ha.answers = true;
  t.ha.absent = new Set(ents);
  await mount();
  t.b.spec('act', { do: 'ha.connect', url: 'http://fake-ha.test:8123', token: 'testtoken1234' });  // Energy's link (the fake)
  wait(t, 11000);
}
// missing (connected 11 s), then unavailable, then a state again; n = the word's shows at least
async function both(t, ents, back, n, what) {
  t.eq(count(t, 'Not found'), 0, `${what} not connected: nothing turns missing (${t.texts().join(' / ').slice(0, 160)})`);
  t.b.m._fw_ha_connected(1);
  wait(t, 5000);
  t.eq(count(t, 'Not found'), 0, `${what} connected 5 s: still waiting for Home Assistant`);
  wait(t, 6000); t.ha.push(); wait(t, 62000);  // an entity wanted later (Energy's sensors, after its dashboard) too
  t.ok(count(t, 'Not found') >= n, `${what} not in Home Assistant: "Not found" x${n} (${t.texts().join(' / ').slice(0, 200)})`);
  t.eq(count(t, 'Unavailable'), 0, `${what} not in Home Assistant: never "Unavailable"`);
  t.ha.absent = new Set();
  for (const e of ents) t.ha.set(e, 'unavailable');
  for (let i = 0; i < 2; i++) { wait(t, 62000); t.ha.push(); }  // found again (Media's speakers, the Calendar's calendars: looked for again), then wanted: answers as HA does
  wait(t, 1000);
  t.ok(count(t, 'Unavailable') >= n, `${what} unavailable: "Unavailable" x${n} (${t.texts().join(' / ').slice(0, 200)})`);
  t.eq(count(t, 'Not found'), 0, `${what} unavailable: never "Not found"`);
  for (const [e, s] of Object.entries(back)) { delete t.ha.s[e]; t.ha.set(e, s[''], Object.fromEntries(Object.entries(s).filter(([k]) => k))); }
  t.ha.push(); wait(t, 62000);
  t.eq([count(t, 'Not found'), count(t, 'Unavailable')], [0, 0], `${what} back: neither word (${t.texts().join(' / ').slice(0, 200)})`);
  t.b.m._fw_ha_connected(0);
}
const tests = Object.entries(SCREENS).map(([scr, [params, back, n]]) => ({
  name: `${scr}: an entity not in Home Assistant shows Not found, an unavailable one Unavailable`, async run(t) {
    try {
      const ents = Object.keys(back);
      await open(t, () => t.mount('spec', { screen: scr, ...params }), ents);
      await both(t, ents, back, n, scr);
    } finally { Date.now = realNow, ahead = 0; }
  },
}));
const deck = (ids) => ({ v: 2, rev: 9, start: 'h', pages: [
  { id: 'h', kind: 'home', name: 'Test', cards: ids.map((s) => ({ t: 'page', page: s, size: 'm' })) },
  ...ids.map((s) => ({ id: s, kind: s, params: SCREENS[s][0] }))] });
for (const ids of [HOME, ['energy']]) tests.push({
  name: `Home cards (${ids.join(', ')}): Not found, then Unavailable`, async run(t) {
    try {
      const back = Object.assign({}, ...ids.map((s) => SCREENS[s][1])), ents = Object.keys(back);
      await open(t, () => t.device(deck(ids)), ents);
      await both(t, ents, back, ids.length, 'Home');
    } finally { Date.now = realNow, ahead = 0; }
  },
});
export default {
  id: 'device', family: 'device', about: 'Entities not in Home Assistant show Not found, unavailable ones Unavailable, on every screen and Home card that reads one.',
  tests,
};
