// Sky radar (components/sky_radar.cpp): the ring numbers read right (the old lab drew them as garbage when a number
// made in a stack buffer reached LVGL's deferred label draw; Pen::text now hands LVGL a copy), the range moves the rings,
// a trail is drawn, a tap picks the nearest plane. Read from the pixels: the ring numbers are Pen text, not labels.
import { frame565 } from './_release.mjs';

const big = (t) => t.comps().find((c) => c.ids.includes('sky_radar'));
// the text lines in the strip right of the south line (where the ring numbers are): [{y0, y1, w}] = each line's rows
// and its ink width. Text ink (fg:text-dim) is brighter than any ring (fg:line, fg:line-hi, on the NE diagonal): only pixels brighter than the rings count
// (measured on the NE diagonal, which crosses every ring and no text)
const lum = (v) => ((v >> 11) << 3) * 0.3 + (((v >> 5) & 63) << 2) * 0.59 + ((v & 31) << 3) * 0.11;
function numbers(t) {
  const c = big(t), px = frame565(t), W = t.b.W || t.b.S, cx = Math.round(c.x + c.w / 2), cy = Math.round(c.y + c.h / 2);
  let ring = 0;
  for (let d = 8; d < c.w / 2 / Math.SQRT2; d++) for (let e = -2; e <= 2; e++) ring = Math.max(ring, lum(px[(cy - d + e) * W + cx + d]));  // NE: every ring, no text
  const x0 = cx + 2, x1 = Math.min(c.x + c.w, cx + Math.round(c.w * 0.3)), lines = [];
  let run = null;
  for (let y = cy + 4; y < c.y + c.h - 2; y++) {  // (the S at the foot is cut by the box: too short to count)
    let first = -1, last = -1;
    for (let x = x0; x < x1; x++) if (lum(px[y * W + x]) > ring + 8) { if (first < 0) first = x; last = x; }
    if (first >= 0) { if (!run) run = { y0: y, y1: y, a: first, b: last }; else { run.y1 = y; run.a = Math.min(run.a, first); run.b = Math.max(run.b, last); } }
    else if (run) { if (run.y1 - run.y0 >= 6) lines.push({ y0: run.y0, y1: run.y1, w: run.b - run.a + 1 }); run = null; }
  }
  return lines;
}

export default {
  id: 'sky_radar', family: 'components', about: 'The Sky radar: ring numbers, range, trails, picking a plane.',
  tests: [
    { name: 'ring numbers: four lines, each as wide as its own number (no garbage, no copies of one)', async run(t) {
      await t.mount('sky_radar', { size: 'radar', planes: '[]' });
      const n = numbers(t);
      t.eq(n.length, 4, `four ring numbers ${JSON.stringify(n)}`);
      if (n.length === 4) {
        t.ok(n[0].w < n[1].w, `"5" is narrower than "15" (${n[0].w} < ${n[1].w})`);
        t.ok(Math.abs(n[1].w - n[2].w) <= 3, `"15" and "30" are about as wide (${n[1].w}, ${n[2].w})`);
        t.ok(n[3].w > n[2].w * 1.8, `"50 km" is the widest (${n[3].w})`);
      }
      await t.mount('sky_radar', { size: 'radar', planes: '[]', range: '100' });
      const m = numbers(t);
      t.eq(m.length, 4, 'range 100: four');
      if (m.length === 4 && n.length === 4) t.ok(m[0].w > n[0].w && m[3].w > n[3].w, `"10" is wider than "5", "100 km" than "50 km" (${m[0].w}, ${m[3].w})`);
    } },
    { name: 'range: a plane 8 km out sits further out on a 10 km radar than on a 50 km one', async run(t) {
      const one = '[[8,90,0,0,"",""]]';
      const ink = async (range) => {
        await t.mount('sky_radar', { size: 'radar', planes: one, range });
        const c = big(t), px = frame565(t), W = t.b.W || t.b.S, cy = Math.round(c.y + c.h / 2), cx = Math.round(c.x + c.w / 2);
        const base = frame565((await t.mount('sky_radar', { size: 'radar', planes: '[]', range }), t));
        let far = -1;
        for (let x = cx + 4; x < c.x + c.w; x++) for (let y = cy - 4; y <= cy + 4; y++) if (px[y * W + x] !== base[y * W + x]) far = Math.max(far, x - cx);
        return far;
      };
      const r10 = await ink('10'), r50 = await ink('50');
      t.ok(r10 > 0 && r50 > 0 && r10 > r50 * 1.5, `10 km: ${r10} px out, 50 km: ${r50} px out`);
    } },
    { name: 'a trail draws where the plane was; none without one', async run(t) {
      const without = '[[20,90,90,0,"",""]]', with_ = '[[20,90,90,0,"","",[10,90,15,90]]]';
      await t.mount('sky_radar', { size: 'radar', planes: without });
      const a = frame565(t);
      await t.mount('sky_radar', { size: 'radar', planes: with_ });
      const b = frame565(t);
      let n = 0;
      for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) n++;
      t.ok(n > 20, `the trail changes ${n} px`);
      await t.mount('sky_radar', { size: 'radar', planes: '[[20,90,90,0,"","",[1e999,90]]]' });
      t.ok(big(t), 'a bad trail point is refused, the radar still builds');
    } },
    { name: 'a tap picks the nearest plane by hex; hero and card take no taps', async run(t) {
      await t.mount('sky_radar', { size: 'radar', planes: '[[25,90,0,0,"AB1","abc123"],[25,270,0,0,"CD2","cd2222"]]' });
      const c = big(t), R0 = c.w / 2, glyph = t.b.S > 600 ? 32 : 24, r = (R0 - glyph / 2) * Math.sqrt(25 / 50);
      const ev = t.tap({ at: [c.x + R0 - r, c.y + R0] }); t.wait(200);
      t.emitted(ev, 'pick', 'cd2222', 'the west one');
      const none = t.tap({ at: [c.x + R0, c.y + R0 + r] }); t.wait(200);
      t.ok(!none.some((e) => e.name === 'pick'), 'no plane there: nothing');
      await t.mount('sky_radar', { size: 'hero', planes: '[[25,90,0,0,"AB1","abc123"]]' });
      const h = big(t), ev2 = t.tap({ at: [h.x + h.w / 2 + 20, h.y + h.h / 2] }); t.wait(200);
      t.ok(!ev2.some((e) => e.name === 'pick'), 'the hero size is a read-out');
    } },
  ],
};
