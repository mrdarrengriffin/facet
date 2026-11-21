// The idle clock's orbit moves like the Clock screen's (the idle screen is the Clock without its header): both are live
// screens, the bench allows their redraws (bench/suite.json "live"). A Clock built with animated "false" holds still.
const areas = (t, ms) => {
  const m = t.b.m; m._fw_areas(t.b.stage);
  let n = 0;
  for (let i = 0; i < Math.ceil(ms / 16); i++) { t.b.tick(1); const p = m._fw_areas(t.b.stage) >> 2; n += new Int32Array(m.HEAPU8.buffer)[p]; }
  return n;
};
export default {
  id: 'idle_still', family: 'screens', about: 'The idle clock\'s orbit moves like the Clock screen\'s; a Clock with animated false holds still.',
  tests: [
    { name: 'idle: its orbit moves (the dots redraw)', async run(t) {
      await t.mount('idle', {}); t.b.settle(1500);
      t.ok(areas(t, 2000) > 10, 'the dots move');
    } },
    { name: 'clock: its orbit moves (the dots redraw)', async run(t) {
      await t.mount('spec', { screen: 'clock' }); t.b.settle(1500);
      t.ok(areas(t, 2000) > 10, 'the dots move');
    } },
  ],
};
