// Rotate screen: Settings > Rotate screen sets "rotation" (0 / 90 / 180 / 270, clockwise); the panel turns
// LVGL's picture and maps touches back (ESPHome software rotation, see t.turn in tools/functional.mjs). Checks: the
// setting takes each value, and after each turn a finger on a control where it shows on the glass lands on it.
// the Sofa table tile's name in caps; at 480 it takes two lines (SOFA / TABLE): either finds the tile
const ctr = (b) => [b.x + b.w / 2, b.y + b.h / 2];
export default {
  id: 'rotate', family: 'device', about: 'Rotate screen: the setting, and taps landing after a turn.',
  tests: [
    { name: 'Settings: tapping 90° sets rotation 90, 0° back to 0', async run(t) {
      await t.device('house'); t.show('settings'); for (let i = 0; i < 4 && !t.texts().includes('90°'); i++) t.drag({ at: [t.b.S / 2, t.b.S * 0.8] }, { by: [0, -t.b.S / 3] });  // scrolled to Rotate screen t.tap('90°'); t.eq(t.setting('rotation'), '90', 'rotation');
      t.tap('0°'); t.eq(t.setting('rotation'), '0', 'back to 0');
    } },
    ...[90, 180, 270].map((deg) => ({ name: `${deg}°: a tap where Sofa table shows on the glass toggles it`, async run(t) {
      await t.device('house'); t.turn(deg); const [px, py] = t.glass(ctr(t.find(/^sofa( table)?$/i)));
      const ev = t.tapGlass(px, py); t.call({ service: 'light.toggle', entity: 'light.sofa_lamp' });
      t.ok(ev.some((e) => e.kind === 'sound' && e.name !== 'tick'), 'the tap sound');
    } })),
    ...[90, 270].map((deg) => ({ name: `${deg}°: a tap at the upright spot does not reach Sofa table (the map is not a no-op)`, async run(t) {
      await t.device('house'); t.turn(deg); const before = t.ha.calls.length; t.tapGlass(...ctr(t.find(/^sofa( table)?$/i)));
      t.ok(!t.ha.calls.slice(before).some((c) => c.entity === 'light.sofa_lamp'), 'no sofa call');
    } })),
  ],
};
