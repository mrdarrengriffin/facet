// Idle clock (core/device.h idle, screens/idle.cpp): Settings > Idle after with no touch = the stack [home, idle],
// the Clock screen without its header, dimmed (the event "idle" <Idle brightness>, "" = undimmed); the first touch goes
// Home and clicks nothing under the finger; 0 = never; a toast shows over it and it stays.
import { frame565 } from './_release.mjs';
const DATE = /^(MON|TUE|WED|THU|FRI|SAT|SUN) \d{1,2} [A-Z]{3}$/;
const idle = (evs) => evs.filter((e) => e.name === 'idle').map((e) => e.detail);
async function on(t) { await t.device(); t.setting('idleAfter', 60); t.b.events(); return t; }  // t.device() turns it off
async function idled(t) {
  await on(t);
  return t.wait(Math.max(0, 62000 - t.since()));
}

// Idle shows = Album art: the device linked to the fake HA with one speaker playing a red picture
const P = 'media_player.test_lounge', HA = 'http://ha.test:8123', ART = `/api/media_player_proxy/${P}?token=x&cache=red`;
async function artIdle(t, state = 'playing') {
  t.ha.s[P] = { '': state, friendly_name: 'Lounge', media_title: 'Red Song', media_artist: 'The Reds', media_duration: '200', media_position: '20', entity_picture: ART };
  t.img.serve(HA + ART, 64, 64, [230, 30, 30, 255]);
  t.ha.answers = true;
  await t.device();
  t.b.spec('act', { do: 'ha.connect', url: HA, token: 'abcdefgh1234' }); t.b.tick(8); t.b.settle(600);
  t.setting('idleShow', 'art'); t.setting('idleAfter', 60); t.b.events();
  t.wait(Math.max(0, 62000 - t.since()));
  for (let i = 0; i < 3; i++) { t.ha.push(); t.wait(1500); }
  return t;
}
const rgb = (t, x, y) => { const q = frame565(t)[y * t.b.S + x]; return [(q >> 11) << 3, ((q >> 5) & 63) << 2, (q & 31) << 3]; };
const isRed = (c) => c[0] > 180 && c[1] < 90 && c[2] < 90;

export default {
  id: 'idle', family: 'device', about: 'The idle clock: Idle after with no touch, dimmed; a touch goes Home and clicks nothing; Idle shows Album art while music plays.',
  tests: [
    { name: 'Idle shows Album art: while music plays, the art alone covers the screen (no words, no shade); paused, the clock; a tap goes Home', async run(t) {
      await artIdle(t);
      const S = t.b.S;
      t.eq(t.stack(), ['home', 'idle'], 'idle');
      t.ok(t.img.asked.includes(HA + ART), `the art fetched: ${t.img.asked}`);
      const top = rgb(t, S >> 1, S >> 3), foot = rgb(t, 4, S - 4);
      t.ok(isRed(top), `the art at the top: ${top}`);
      t.ok(isRed(foot), `its foot is the art too, no shade: ${foot}`);
      t.ha.set(P, 'paused'); t.wait(1500);
      t.ok(!isRed(rgb(t, S >> 1, S >> 3)), `paused: no art (${rgb(t, S >> 1, S >> 3)})`);
      t.ok(t.texts().some((x) => DATE.test(x)), 'the clock shows');
      t.ha.set(P, 'playing'); t.wait(1500);
      t.ok(isRed(rgb(t, S >> 1, S >> 3)), 'playing again: the art');
      const evs = t.tap({ at: [S / 2, S / 2] });
      t.eq(t.stack(), ['home'], 'a tap: Home');
      t.notEmitted(evs, 'navigate');
    } },
    { name: 'Idle shows Clock (the default): the clock even while music plays', async run(t) {
      await artIdle(t);
      t.setting('idleShow', 'clock'); t.tap({ at: [t.b.S / 2, t.b.S / 2] });
      t.wait(62000); for (let i = 0; i < 2; i++) { t.ha.push(); t.wait(1500); }
      t.eq(t.stack(), ['home', 'idle'], 'idle again');
      t.ok(!isRed(rgb(t, t.b.S >> 1, t.b.S >> 3)), 'no art');
    } },
    { name: 'Idle after (60 s): Home until then, then the idle clock without its header, dimmed to Idle brightness', async run(t) {
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
    { name: 'Idle after 0 = never (10 min with no touch)', async run(t) {
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
