// Fades on light themes stay grey (#39, core/blend565.cpp). The panels draw in RGB565 (green 6 bits, red and blue 5);
// LVGL's own mix let green move a step ahead of red and blue, so a white card fading in over the light canvas sat at
// (247,251,247) and fading text turned green. Checked two ways: the mix itself for every grey of the light theme over
// its canvas and surfaces, at every opacity (no greener than the true colour by more than SLACK), and the frames of a
// real screen rise (Home > Weather in light mode: no near-grey pixel clearly green).
import { readFileSync } from 'node:fs';
import { frame565 } from './_release.mjs';
const SLACK = 2;
const rgb = (v) => { const r = v >> 11, g = (v >> 5) & 63, b = v & 31; return [(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)]; };
const to565 = (hex) => { const n = parseInt(hex.slice(1), 16); return ((n >> 19) << 11) | (((n >> 10) & 63) << 5) | ((n >> 3) & 31); };
const spread = ([r, g, b]) => Math.max(r, g, b) - Math.min(r, g, b);
const green = ([r, g, b]) => g - Math.max(r, b);
const COLOURS = JSON.parse(readFileSync(new URL('../../tokens/colour.json', import.meta.url), 'utf8')).colours;
export function riseFrames(t, screen = 'weather', n = 30) {
  const from = frame565(t), frames = [];
  t.b.str(screen, (p) => t.b.m._fw_bench_nav(p, 0));
  for (let i = 0; i < n; i++) { t.b.tick(1); frames.push(frame565(t)); }
  t.b.settle(1500);
  return { from, frames, to: frame565(t) };
}
export default {
  id: 'fade_tint', family: 'screens', about: 'Light-theme fades stay grey: no green tint mid-fade (RGB565 mixing, #39).',
  tests: [
    { name: 'the mix: every light-theme grey over the canvas and surfaces, at every opacity, no greener than the true colour (±2)', async run(t) {
      await t.mount('device', {});
      const greys = Object.entries(COLOURS).filter(([, c]) => /^#[0-9a-f]{6}$/i.test(c.light || '') && spread(rgb(to565(c.light))) <= 24);
      const grounds = ['bg', 'surface', 'surface-press'].map((k) => [k, to565(COLOURS[k].light)]);
      const bad = [];
      for (const [k, c] of greys) for (const [gk, g] of grounds) {
        const mix = t.b.test('mix565', `${to565(c.light)},${g}`), f = rgb(to565(c.light)), b = rgb(g);
        for (let m = 1; m < 255; m++) {
          const ideal = f.map((x, i) => (x * m + b[i] * (255 - m)) / 255), got = rgb(mix[m]);
          const over = green(got) - green(ideal);
          if (over > SLACK) { bad.push(`${k} over ${gk} at ${m}: ${got} (true ${ideal.map(Math.round)}, +${over.toFixed(1)})`); break; }
        }
      }
      t.ok(greys.length >= 8, `the light theme's greys (${greys.length})`);
      t.eq(bad, [], 'no grey mixes greener than it is');
    } },
    { name: 'light: the frames of the Weather rise show no green greys (a white card was (247,251,247))', async run(t) {
      await t.device();
      t.setting('theme', 'light'); t.b.settle(1500);
      const { frames, to } = riseFrames(t);
      t.ok(frames.filter((f) => f.some((v, i) => v !== to[i])).length >= 3, 'the rise fades over several frames');
      let n = 0, worst = 0;
      for (const f of frames) for (const v of f) {
        const c = rgb(v);
        if (spread(c) > 24) continue;  // an icon's colour, the accent: not a grey
        const x = green(c);
        if (x > 2 * SLACK) n++;
        if (x > worst) worst = x;
      }
      t.eq(n, 0, `no near-grey pixel green by more than ${2 * SLACK} (worst +${worst}; RGB565 greys themselves can be +3)`);
    } },
  ],
};
