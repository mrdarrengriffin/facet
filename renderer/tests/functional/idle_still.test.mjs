// The idle screen stays still (#233): the idle clock is the Clock screen with its orbit's dots held still, so with
// nothing changing it redraws nothing (a dimmed panel draws no frames); the Clock screen's own orbit still moves.
const areas = (t, ms) => {
  const m = t.b.m; m._fw_areas(t.b.stage);
  let n = 0;
  for (let i = 0; i < Math.ceil(ms / 16); i++) { t.b.tick(1); const p = m._fw_areas(t.b.stage) >> 2; n += new Int32Array(m.HEAPU8.buffer)[p]; }
  return n;
};
export default {
  id: 'idle_still', family: 'screens', about: 'The idle clock redraws nothing while nothing changes; the Clock screen still moves.',
  tests: [
    { name: 'idle: no redraw in 2 s with nothing changing', async run(t) {
      await t.mount('idle', {}); t.b.settle(1500);
      t.eq(areas(t, 2000), 0, 'no area redrawn');
    } },
    { name: 'clock: its orbit still moves (the dots redraw)', async run(t) {
      await t.mount('spec', { screen: 'clock' }); t.b.settle(1500);
      t.ok(areas(t, 2000) > 10, 'the dots move');
    } },
  ],
};
