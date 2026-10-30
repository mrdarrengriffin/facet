// Robot vacuum screen (specs/vacuum.json: Page > Header > Body: Column: the Rooms view (the state Status block, the
// run's Level bar, Room tiles, the Vacuum mode row), the bar at the foot (Dock / Pause / Resume / Clean Buttons); the
// Map view waits for the Iso floor plan on the contract, 2026-10-30), on the vacuum
// feed panel.vacuum (core/vacuum_source.cpp, CONTRACT.md C51) and its Home card (C52). Home Assistant is faked
// (tools/functional.mjs FakeHA); V is NOT Darren's entity. Approved original: the panel lab's Vacuum
// (apps/hub panel/vacuum/screen.ts).
const V = 'vacuum.test_robot';
async function open(t, params) { await t.mount('spec', { screen: 'vacuum', ...params }); return t; }
const deck = (params) => ({ v: 2, rev: 9, start: 'h', pages: [
  { id: 'h', kind: 'home', name: 'Test', cards: [{ t: 'page', page: 'v', size: 'm' }] },
  { id: 'v', kind: 'vacuum', params }] });

export default {
  id: 'vacuum_screen', family: 'screens', about: 'The Robot vacuum screen on the vacuum feed: the state, the rooms, the run, the buttons, the HA adapter, Home card.',
  tests: [
    { name: 'sample docked: the state, the battery, three rooms, Clean all', async run(t) {
      await open(t, { source: 'sample' });
      const tx = t.texts();
      t.has(tx, /^Docked$/, 'the state'); t.has(tx, /100%/, 'the battery'); t.has(tx, /charged/, 'charged');
      t.has(tx, /^Lounge$/, 'a room tile'); t.has(tx, /^Clean all$/i, 'Clean all');
      t.ok(!tx.some((x) => /^Dock$/i.test(x)), 'no Dock while docked');
    } },
    { name: 'sample: pick a room, Clean runs it; nothing goes to Home Assistant', async run(t) {
      await open(t, { source: 'sample' });
      t.tap(t.find(/^Dining$/));
      t.has(t.texts(), /^Clean Dining$/i, 'the button names the room');
      t.tap(t.find(/^Clean Dining$/i));
      t.wait(3000);
      const tx = t.texts();
      t.has(tx, /^Cleaning$/, 'cleaning'); t.has(tx, /^Pause$/i, 'Pause'); t.has(tx, /^Dock$/i, 'Dock');
      t.has(tx, /^Dining · \d+% · /, 'the room it is in, the progress');
      t.tap(t.find(/^Pause$/i));
      t.wait(400);
      t.has(t.texts(), /^Paused$/, 'paused'); t.has(t.texts(), /^Resume$/i, 'Resume');
      t.eq(t.ha.calls.length, 0, 'no HA call on the sample');
    } },
    { name: 'ha: the vacuum live; Clean with a picked room goes out as a room clean', async run(t) {
      t.ha.s[V] = { '': 'docked', battery_level: '88', fan_speed: 'balanced' };
      await open(t, { source: 'ha', entity: V, areas: 'Kitchen:3,Hall:1' });
      t.has(t.texts(), /^Docked$/, 'docked'); t.has(t.texts(), /88%/, 'the battery');
      t.tap(t.find(/^Kitchen$/));
      t.tap(t.find(/^Clean Kitchen$/i));
      t.ok(t.ha.calls.some((c) => c.service === 'vacuum.send_command' && c.entity === V), 'a room clean on the vacuum');
    } },
    { name: 'ha: cleaning shows the run and Pause pauses it', async run(t) {
      t.ha.s[V] = { '': 'cleaning', battery_level: '70' };
      await open(t, { source: 'ha', entity: V });
      t.has(t.texts(), /^Cleaning$/, 'cleaning');
      t.tap(t.find(/^Pause$/i));
      t.ok(t.ha.calls.some((c) => c.service === 'vacuum.pause' && c.entity === V), 'vacuum.pause');
    } },
    { name: 'ha: an unavailable vacuum says Offline and offers no Clean', async run(t) {
      t.ha.s[V] = { '': 'unavailable' };
      await open(t, { source: 'ha', entity: V });
      t.has(t.texts(), /^Offline$/, 'offline');
      t.ok(!t.texts().some((x) => /^Clean/i.test(x)), 'no Clean');
    } },
    { name: 'Home card (C52): the state and the robot on the same feed', async run(t) {
      await t.device(deck({ source: 'sample' }));
      const c = t.part('card');
      t.ok(c, 'the page card'); t.ok(c.texts.some((x) => /^Docked$/.test(x)), 'the state on the card');
      t.ok(c.texts.some((x) => /100% · charged/.test(x)), 'the line on the card');
      t.ok(t.part('vacuum_robot'), 'the robot on the card');
    } },
  ],
};
