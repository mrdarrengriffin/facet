// Status chip (components/status_chip.cpp, contract v2, Header round 3 2026-10-27): tinted pill, dot + caps words.
// The regression this piece fixes: the chip baked its colours from the theme at build, so a live dark -> light restyle
// left near-white words on the light tint (Darren's screenshot; tools/pixel_contrast.mjs measured 1.1:1).
import { D } from './_controls.mjs';
import { leak, golden, stage, boundsAll, liveLook, frame565, diff565, fixtures } from './_release.mjs';
const TONES = ['ok', 'warn', 'error', 'working', 'info', 'idle'];
async function at(t, props, w = 400, h = 96) { t.b.W = t.b.H = undefined; await t.mount('status_chip', props); stage(t, w, h); t.b.settle(800); return t.part('status_chip'); }
export default {
  id: 'status_chip', family: 'controls', about: 'Status chip on the contract: hugs, tones, live restyle, update in place.',
  tests: [
    { name: 'hugs its words: chip-h tall, as wide as dot + words, the same in any parent', async run(t, { den }) {
      const a = await at(t, { text: 'working' }, 400), b = await at(t, { text: 'working' }, 700, 200), c = await at(t, { text: 'working working' }, 400);
      t.eq(a.h, D(den, 32, 24), 'chip-h'); t.eq([a.w, a.h], [b.w, b.h], 'parent-independent'); t.ok(c.w > a.w, 'longer words, wider chip');
    } },
    { name: 'tones: every tone tints differently (pixels)', async run(t) {
      const seen = new Set();
      for (const tone of TONES) { await at(t, { tone, text: 'x' }); seen.add(frame565(t).join()); }
      t.eq(seen.size, TONES.length, 'distinct');
    } },
    { name: 'live restyle: dark -> light (and back) repaints the fill, dot and words = a fresh build, every pixel', async run(t) {
      const out = [];
      for (const tone of TONES) out.push(...await liveLook(t, 'status_chip', { tone, text: 'sys: ' + tone }, 400, 96));
      t.eq(out.slice(0, 4), [], 'live = fresh');
    } },
    { name: 'update: words in place (same objects, silent); a new tone = the chip again, faded in, ending on the new tint', async run(t) {
      await at(t, { tone: 'ok', text: 'ok' }); const n0 = t.objs().length;
      t.b.update('status_chip', { tone: 'ok', text: '3 working' }); t.b.settle(800); t.eq([t.objs().length, t.texts().includes('3 WORKING')], [n0, true], 'words in place');
      const f0 = frame565(t); t.b.update('status_chip', { tone: 'error', text: '3 working' }); t.b.settle(1500); t.ok(diff565(f0, frame565(t)) > 0, 'new tone shows');
      t.silent(t.b.events());
    } },
    { name: 'bounds: every tone + long / non-ASCII words in a wide parent; a parent narrower than the chip is reported', async run(t) {
      const bad = [];
      for (const tone of TONES) for (const text of ['x', '▲ 18% vs yday', fixtures.accents, fixtures.cjk]) { await at(t, { tone, text }, 700); bad.push(...boundsAll(t)); }
      t.eq(bad.slice(0, 4), [], 'bounds'); await at(t, { text: fixtures.long }, 120); t.ok(boundsAll(t).length > 0, 'overfull reported');
    } },
    { name: 'golden images: every tone (dark + light): reviewed, never auto-accepted', async run(t, { den: d }) {
      const out = [];
      for (const tone of TONES) for (const light of [0, 1]) { t.b.W = t.b.H = undefined; await t.b.render('status_chip', { tone, text: 'sys: ' + tone }, light); stage(t, 400, 96); t.b.settle(800); out.push(...golden(t, 'status_chip', `${tone}-${light ? 'light' : 'dark'}`, d)); }
      t.eq(out, [], 'goldens');
    } },
    { name: 'leak: 150 build / clear cycles (pulse on): objects back, heap flat, no animation left', async run(t) {
      await at(t, {});
      t.eq(leak(t, 150, () => t.b.rebuild('status_chip', { tone: 'working', text: '3 working', pulse: true }), () => t.b.rebuild('placeholder', {}), 48 * 1024), [], 'leak');
    } },
  ],
};
