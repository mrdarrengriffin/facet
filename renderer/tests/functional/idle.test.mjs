// Idle clock (core/device.h idle, screens/idle.cpp): Settings > Clock after with no touch = the stack [home, idle],
// the Clock screen without its header, dimmed (the event "idle" <Idle brightness>, "" = undimmed); the first touch goes
// Home and clicks nothing under the finger; 0 = never; a toast shows over it and it stays.
const DATE = /^(MON|TUE|WED|THU|FRI|SAT|SUN) \d{1,2} [A-Z]{3}$/;
const idle = (evs) => evs.filter((e) => e.name === 'idle').map((e) => e.detail);
async function on(t) { await t.device(); t.setting('idleAfter', 60); t.b.events(); return t; }  // t.device() turns it off
async function idled(t) {
  await on(t);
  return t.wait(Math.max(0, 62000 - t.since()));
}

export default {
  id: 'idle', family: 'device', about: 'The idle clock: Clock after with no touch, dimmed; a touch goes Home and clicks nothing.',
  tests: [
    { name: 'Clock after (60 s): Home until then, then the idle clock without its header, dimmed to Idle brightness', async run(t) {
      await on(t);
      t.eq(t.setting('idleBrightness'), '70', 'default 70 %');
      t.wait(Math.max(0, 55000 - t.since()));
      t.eq(t.stack(), ['home'], '55 s: still Home');
      const evs = t.wait(7000);
      t.eq(t.stack(), ['home', 'idle'], '62 s: idle');
      t.eq(idle(evs), ['70'], 'dimmed once, to 70');
      t.wait(1000);
      t.ok(t.texts().some((x) => DATE.test(x)), `the date shows (${t.texts().join(' / ')})`);
      t.hasNot(t.texts(), 'Clock', 'no header');
    } },
    { name: 'the first touch goes Home, undimmed, and clicks nothing under the finger', async run(t) {
      await idled(t);
      const n = t.ha.calls.length, S = t.b.S;
      const evs = t.tap({ at: [S / 2, S / 2] });
      t.eq(t.stack(), ['home'], 'Home');
      t.notEmitted(evs, 'navigate');
      t.eq(idle(evs), [''], 'undimmed');
      t.eq(t.ha.calls.length, n, 'no Home Assistant call');
      t.eq(idle(t.wait(30000)), [], '30 s later: still Home');
      t.eq(t.stack(), ['home'], 'the count starts again from the touch');
    } },
    { name: 'Idle brightness: the dim follows the setting', async run(t) {
      await on(t); t.setting('idleBrightness', 40);
      t.eq(idle(t.wait(Math.max(0, 62000 - t.since()))), ['40'], 'dimmed to 40');
    } },
    { name: 'Clock after 0 = never (10 min with no touch)', async run(t) {
      await on(t); t.setting('idleAfter', 0);
      t.eq(idle(t.wait(600000)), [], 'no idle');
      t.eq(t.stack(), ['home'], 'Home');
    } },
    { name: 'a toast shows over the idle clock, which stays', async run(t) {
      await idled(t);
      t.toast({ title: 'Washing done', kind: 'info' });
      t.has(t.texts(), 'Washing done', 'the toast');
      t.eq(t.stack(), ['home', 'idle'], 'still idle');
    } },
  ],
};
