// Time (components/time.cpp, contract v2, Header round 3 2026-10-27): digit cells that never shift; size header = the
// Header's time (type label, text-dim, one label).
import { leak, golden, stage, boundsAll, liveLook, frame565, diff565 } from './_release.mjs';
async function at(t, props, w = 720, h = 300) { t.b.W = t.b.H = undefined; await t.mount('time', props); stage(t, w, h); t.b.settle(300); return t.part('time'); }
const cellsX = (t) => t.objs().filter((o) => o.text && t.visible(o)).map((o) => o.x);
export default {
  id: 'time', family: 'controls', about: 'Time on the contract: cells, header size, live restyle, update in place.',
  tests: [
    { name: 'header size: one label "21:42" (caps label type); 12h "9:42 PM"', async run(t) {
      await at(t, { size: 'header', time: '21:42' }); t.eq(t.texts(), ['21:42'], 'one label');
      await at(t, { size: 'header', time: '21:42', format: '12h' }); t.eq(t.texts(), ['9:42 PM'], '12h');
    } },
    { name: 'cells: the digits never shift when they change (11:11 -> 10:08: same cell positions), colon in its own cell', async run(t) {
      await at(t, { size: 'mid', time: '11:11' }); const a = cellsX(t);
      t.b.update('time', { size: 'mid', time: '10:08' }); t.b.settle(300); t.eq(cellsX(t), a, 'same x'); t.eq(t.texts().length, 5, '5 cells');
    } },
    { name: 'update in place: a new time changes only the changed cells (same objects), silent', async run(t) {
      await at(t, { size: 'mid', time: '14:52' }); const n0 = t.objs().length;
      t.b.update('time', { size: 'mid', time: '14:53' }); t.b.settle(300); t.eq([t.objs().length, t.texts().join('')], [n0, '14:53'], 'in place'); t.silent(t.b.events());
    } },
    { name: 'colour + pixel proof: a live dark <-> light switch = a fresh build (header, mid 12h, hero seconds)', async run(t) {
      const out = [];
      for (const p of [{ size: 'header', time: '21:42' }, { size: 'mid', time: '14:52', format: '12h' }, { size: 'compact', time: '09:05' }, { size: 'hero', time: '10:08:30', seconds: true }])
        out.push(...await liveLook(t, 'time', p, 720, 300));
      t.eq(out.slice(0, 4), [], 'live = fresh');
    } },
    { name: 'bounds: every size x format x seconds in a wide parent', async run(t) {
      const bad = [];
      for (const size of ['header', 'compact', 'mid', 'hero']) for (const format of ['24h', '12h']) for (const seconds of [false, true]) {
        await at(t, { size, format, seconds, time: '22:08:30' }); bad.push(...boundsAll(t).map((e) => `${size} ${format} ${seconds}: ${e}`));
      }
      t.eq(bad.slice(0, 4), [], 'bounds');
    } },
    { name: 'golden images: header + compact + mid + hero (dark + light): reviewed, never auto-accepted', async run(t, { den: d }) {
      const out = [];
      for (const [n, p] of [['header', { size: 'header', time: '21:42' }], ['compact-12h', { size: 'compact', time: '09:05', format: '12h' }], ['mid', { size: 'mid', time: '14:52' }], ['hero-seconds', { size: 'hero', time: '10:08:30', seconds: true }]])
        for (const light of [0, 1]) { t.b.W = t.b.H = undefined; await t.b.render('time', p, light); stage(t, 720, 300); t.b.settle(300); out.push(...golden(t, 'time', `${n}-${light ? 'light' : 'dark'}`, d)); }
      t.eq(out, [], 'goldens');
    } },
    { name: 'leak: 150 build / clear cycles: objects back, heap flat, the clock timer freed', async run(t) {
      await at(t, {});
      t.eq(leak(t, 150, () => t.b.rebuild('time', { size: 'hero', seconds: true, format: '12h', time: '' }), () => t.b.rebuild('placeholder', {}), 48 * 1024), [], 'leak');
    } },
  ],
};
