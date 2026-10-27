// Header (components/header.cpp, contract v2), round 3 2026-10-27: [Back] [Title] ... [END slot] [Time]. Each test is
// one row of the release gate and fails without the behaviour.
// Approved original: the Astro header (packages/ui/src/layout.ts header({label, onBack, right, clock}), CSS .fs-header).
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { leak, look, golden, stage, boundsAll, liveLook, lab, tune, frame565, diff565 } from './_release.mjs';

const TOK = join(dirname(fileURLToPath(import.meta.url)), '..', '..', 'tokens');
const SPACE = JSON.parse(readFileSync(join(TOK, 'space.json'), 'utf8')).tokens;
const LAYJ = JSON.stringify(JSON.parse(readFileSync(join(TOK, 'layout.json'), 'utf8')));
const den = (t) => (t.b.S > 600 ? '1.5x' : '1x');
const lay = (t, n) => { const m = LAYJ.match(new RegExp(`"${n}":\\{"v":(\\d+),"1x":(\\d+)`)); return +(den(t) === '1x' ? m[2] : m[1]); };
const TMIN = (t) => (den(t) === '1x' ? SPACE.touch.min['1x'] : SPACE.touch.min.v);
const SQ = (t) => (den(t) === '1x' ? SPACE.layout['ibtn-square']['1x'] : SPACE.layout['ibtn-square'].v);
const INSET = { '1.5x': 32, '1x': 20 };
const CW = (t) => t.b.S - 2 * INSET[den(t)];
async function at(t, props, w, light = 0, row) {
  t.b.W = t.b.H = undefined;
  await t.mount('header', { time: '21:42', ...props });
  if (light) look(t, 1);
  stage(t, (w ?? CW(t)) + 2 * INSET[den(t)], (row ?? lay(t, 'row')) + 2 * INSET[den(t)]); t.b.settle(800); t.b.events();
  return t.part('header');
}
const comp = (t, id) => t.comps().filter((c) => c.ids.includes(id) && t.visible(c)).sort((a, b) => a.x - b.x);
const label = (t, re) => t.objs().find((o) => o.text && re.test(o.text) && t.visible(o));
const cy = (o) => o.y + o.h / 2;
const VARIANTS = [{}, { back: false, settings: true }, { sample: 'end-chip' }, { sample: 'end-buttons', settings: true }, { sample: 'title-segmented' },
  { label: 'x'.repeat(60), settings: true, sample: 'full-end' }, { clock: false }, { actions: { shortcut: 'timer', to: 'timer', status: 'timer' }, settings: true, back: false }];

export default {
  id: 'header', family: 'layout', about: 'Header on the contract: Back, title slot, end slot (any number of items), Time; update path, proofs.',
  tests: [
    // ---------------------------------------------------------------- 1 contract
    { name: 'tokens: the header row is layout:row tall and >= touch:min (C20); Back (an Icon button square) fills it', async run(t) {
      t.ok(lay(t, 'row') >= TMIN(t), `row ${lay(t, 'row')} >= touch:min ${TMIN(t)}`);
      const h = await at(t, {}); t.eq([h.w, h.h], [CW(t), lay(t, 'row')], 'header = content width x row');
      const b = comp(t, 'back_button')[0]; t.eq([b.w, b.h], [SQ(t), lay(t, 'row')], 'back = the square = the row');
      t.ok(b.ids.includes('icon_button'), 'Back IS an Icon button');
    } },
    { name: 'default: back at the start, the label a back-gap after it, the Time last on the content edge, all centred', async run(t) {
      const h = await at(t, { label: 'Weather' }), b = comp(t, 'back_button')[0], l = label(t, /weather/i), tm = label(t, /21:42/), ti = comp(t, 'time')[0];
      t.eq(b.x, h.x, 'back at the start'); t.eq(l.x - (b.x + b.w), lay(t, 'back-gap'), 'back-gap to the label');
      t.eq(ti.x + ti.w, h.x + h.w, 'the Time ends on the content edge');
      t.eq(Math.round(cy(b)), Math.round(cy(h)), 'back centred on the row');
      for (const [n, o] of [['label', l], ['time', tm]]) t.ok(Math.abs(cy(o) - cy(h)) <= 1, `${n} centred on the row (${cy(o)} vs ${cy(h)})`);
    } },
    { name: 'no back (the root screen): the title starts the row, no gap', async run(t) {
      const h = await at(t, { label: 'Home', back: false, settings: true }); t.eq(label(t, /home/i).x, h.x, 'label at the start');
    } },
    { name: 'end slot: several items (3 buttons + a chip + Home\'s settings) in order, an item gap apart, then the Time', async run(t) {
      const h = await at(t, { label: 'Photo', sample: 'end-buttons', settings: true });
      const items = [...comp(t, 'icon_button').filter((c) => !c.ids.includes('back_button')), ...comp(t, 'status_chip')].sort((a, b) => a.x - b.x), tm = comp(t, 'time')[0];
      t.eq(items.length, 5, 'five end items');
      const gaps = items.slice(1).map((c, i) => c.x - (items[i].x + items[i].w)); gaps.push(tm.x - (items.at(-1).x + items.at(-1).w));
      t.eq(gaps, gaps.map(() => lay(t, 'header-item-gap')), 'header-item-gap between the end items and before the Time');
      t.ok(comp(t, 'status_chip')[0].x > items[0].x, 'spec / sample items keep their order');
      t.ok(items.at(-1).ids.includes('icon_button') && !comp(t, 'status_chip').some((c) => c.x > items.at(-1).x), 'the settings button is the last end item');
      t.eq(boundsAll(t), [], 'bounds');
      t.eq(tm.x + tm.w, h.x + h.w, 'the Time stays last');
    } },
    { name: 'title slot: a Segmented REPLACES the label and HUGS, a back-gap after Back; a row too narrow for it is reported', async run(t) {
      const h = await at(t, { label: 'Vacuum', sample: 'title-segmented' }), s = comp(t, 'segmented')[0], b = comp(t, 'back_button')[0];
      t.eq(label(t, /^vacuum$/i), undefined, 'no label'); t.eq(s.x - (b.x + b.w), lay(t, 'back-gap'), 'a back-gap after Back');
      t.ok(s.w < h.w / 2, `hugs (${s.w} px), not stretched`);
      await at(t, { label: 'Vacuum', sample: 'title-segmented' }, den(t) === '1x' ? 160 : 333); t.ok(boundsAll(t).length > 0, 'never squeezed: the overfull row is reported');
    } },
    { name: 'a long title ellipsises and never pushes the end items off', async run(t) {
      const h = await at(t, { label: 'Living room ceiling and the sofa lamps and the hall', settings: true, sample: 'full-end' });
      const c = comp(t, 'status_chip')[0], l = label(t, /^living/i), tm = comp(t, 'time')[0];
      t.ok(l.x + l.w <= c.x - lay(t, 'header-item-gap'), `title ends an item gap before the chip (${l.x + l.w} vs ${c.x})`);
      t.ok(l.w > 0 && l.x + l.w < c.x, `the title gave way (${l.w} px)`); t.eq(tm.x + tm.w, h.x + h.w, 'Time on the edge');
    } },
    { name: 'bounds proof: 0 fails for every variant at the content width and at 333 px (not the title node: the hug test)', async run(t) {
      const bad = [];
      for (const p of VARIANTS) for (const w of [CW(t), 333]) {
        if (w === 333 && (p.sample === 'end-buttons' || (p.sample === 'title-segmented' && den(t) === '1.5x'))) continue;  // overfull at 333 by design: the overfull tests
        await at(t, p, w); for (const e of boundsAll(t)) bad.push(`${JSON.stringify(p)} ${w}: ${e}`);
      }
      t.eq(bad.slice(0, 5), [], 'bounds');
    } },
    { name: 'overfull end slot: the bounds proof FAILS (reported, never a silent clip); no limit in code', async run(t) {
      await at(t, { settings: true, sample: 'end-buttons' }, den(t) === '1x' ? 160 : 240); t.ok(boundsAll(t).length > 0, 'an overfull header is reported');
      t.ok(comp(t, 'icon_button').length >= 5, 'every item was built (no cap)');
    } },
    // ---------------------------------------------------------------- slots: one table, spec checks
    { name: 'slots: one table (meta "slots" = title + end, no start); spec slot "start" is refused with a clear error', async run(t) {
      await at(t, {});
      const meta = JSON.parse(t.b.m.UTF8ToString(t.b.m._fw_meta())).find((c) => c.id === 'header');
      const m = typeof meta.meta === 'string' ? JSON.parse(meta.meta) : meta.meta;
      t.eq(m.slots.map((s) => s.name), ['title', 'end'], 'meta slots');
      const spec = (slot) => ({ v: 1, id: 'hdrslots', rev: 1, name: 'Slots', layout: 'list', nodes: [{ c: 'status_chip', p: {}, in: 'header', ...(slot ? { slot } : {}) }] });
      await t.device();
      const bad = JSON.stringify(JSON.parse(t.b.spec('check', spec('start')) || '{}'));
      t.ok(bad.includes('bad_in') && bad.includes('no start slot'), `slot start refused: ${bad.slice(0, 160)}`);
      t.ok(JSON.stringify(JSON.parse(t.b.spec('check', spec('middle')) || '{}')).includes('bad_in'), 'slot middle refused');
      t.ok(!JSON.stringify(JSON.parse(t.b.spec('check', spec('end')) || '{}')).includes('bad_in'), 'slot end ok');
    } },
    { name: 'spec: nodes in title / end; "in": "header" with no slot = end; Back first, the Time last', async run(t) {
      const spec = { v: 1, id: 'hdrslots', rev: 1, name: 'Slots', header: { label: 'Slots' }, layout: 'list', nodes: [
        { c: 'status_chip', p: { tone: 'ok', text: 'ok' }, in: 'header' },
        { c: 'icon_button', p: { icon: 'close', style: 'square' }, in: 'header', slot: 'end' },
        { c: 'segmented', p: { options: 'A, B', selected: 'a' }, in: 'header', slot: 'title' },
        { c: 'status_chip', p: { tone: 'idle', text: 'body' } }] };
      await t.device();
      t.eq(t.send(spec).status, 'applied', 'applied'); t.show('hdrslots'); t.b.settle(1500);
      const h = t.part('header'), sg = comp(t, 'segmented')[0], ok = comp(t, 'status_chip').find((o) => o.y < h.y + h.h);
      const ib = comp(t, 'icon_button').find((o) => o.y < h.y + h.h && !o.ids.includes('back_button')), tm = comp(t, 'time')[0], b = comp(t, 'back_button')[0];
      t.eq(label(t, /^slots$/i), undefined, 'title: the segmented replaced the label');
      t.ok(b && sg && ok && ib && tm && b.x < sg.x && sg.x + sg.w < ok.x && ok.x < ib.x && ib.x + ib.w < tm.x, 'Back, title, end in spec order, Time');
      t.eq(boundsAll(t).length, 0, 'bounds');
    } },
    // ---------------------------------------------------------------- tunables (Layout lab)
    { name: 'tunables: row, back-gap, item gap, max width are listed, each moves the header live, bounds clean across the range', async run(t) {
      const NAMES = ['layout:row', 'layout:back-gap', 'layout:header-item-gap', 'layout:header-max-w'];
      await at(t, { settings: true, sample: 'end-chip' });
      const toks = new Map(JSON.parse(lab(t, 'tokens')).map((r) => [r[0], r]));
      for (const n of NAMES) t.ok(toks.has(n), `${n} in the tunables table`);
      const bad = [];
      for (const n of NAMES) {
        const base = toks.get(n)[1], lo = n === 'layout:row' ? TMIN(t) : n === 'layout:header-max-w' ? 0 : 0;  // the row's floor = touch:min (C20)
        for (let v = lo; v <= base * 2; v += Math.max(4, Math.round(base / 4 / 4) * 4)) {
          await at(t, { label: 'Weather', settings: true, sample: 'end-chip' }, n === 'layout:header-max-w' ? CW(t) : undefined, 0, n === 'layout:row' ? Math.max(v, lay(t, 'row')) : undefined);
          const f0 = frame565(t); tune(t, n, v); t.b.settle(400);
          const min = n === 'layout:header-max-w' && v < 300 * (den(t) === '1x' ? 1 : 1.5);  // narrower than its items: reported by design
          if (!min) for (const e of boundsAll(t)) bad.push(`${n}=${v}: ${e}`);
          if (n === 'layout:row') { if (t.part('header').h !== v) bad.push(`row ${v}: the header is ${t.part('header').h} tall`); }  // alone the row is centred: its height is what changes
          else if (v !== base && !(n === 'layout:header-max-w' && v >= CW(t)) && !diff565(f0, frame565(t))) bad.push(`${n}=${v}: nothing moved`);
          tune(t, n, -1);
        }
      }
      tune(t, '', -1);
      t.eq(bad.slice(0, 6), [], 'tuned');
    } },
    // ---------------------------------------------------------------- 2 update path
    { name: 'update path: label and time in place (same objects); back / settings change = the row again; a replaced title stays', async run(t) {
      await at(t, { label: 'Weather' }); const n0 = t.objs().length, b0 = comp(t, 'back_button')[0];
      let r = t.b.update('header', { time: '21:42', label: 'Lights' }); t.b.settle(800); t.eq([r || 'ok', !!label(t, /lights/i), t.objs().length], ['ok', true, n0], 'label in place');
      r = t.b.update('header', { time: '08:05', label: 'Lights' }); t.b.settle(800); t.ok(!!label(t, /08:05/), 'time in place');
      t.eq(comp(t, 'back_button')[0].x, b0.x, 'back stayed');
      t.b.update('header', { time: '08:05', label: 'Lights', back: false }); t.b.settle(800); t.eq(comp(t, 'back_button').length, 0, 'back off: row again');
      t.b.update('header', { time: '08:05', label: 'Lights', back: false, settings: true }); t.b.settle(800); t.eq(comp(t, 'icon_button').length, 1, 'settings on: the end item');
      await at(t, { label: 'Vacuum', sample: 'title-segmented' }); t.b.update('header', { time: '21:42', label: 'Other', sample: 'title-segmented' }); t.b.settle(800);
      t.eq([label(t, /^other$/i), comp(t, 'segmented').length], [undefined, 1], 'a replaced title keeps its node');
    } },
    { name: 'Home end items: settings = navigate(settings) + open; a shortcut = navigate(to); a long press on the time = settings', async run(t) {
      await at(t, { label: 'Home', back: false, settings: true, actions: { shortcut: 'timer', to: 'timer' } });
      const [sc, set] = comp(t, 'icon_button'), tm = comp(t, 'time')[0];
      let ev = t.tap({ at: [sc.x + sc.w / 2, sc.y + sc.h / 2] }); t.emitted(ev, 'navigate', 'timer');
      ev = t.tap({ at: [set.x + set.w / 2, set.y + set.h / 2] }); t.emitted(ev, 'navigate', 'settings'); t.sounds(ev, ['open']);
      t.emitted(t.hold({ at: [tm.x + tm.w / 2, tm.y + tm.h / 2] }), 'navigate', 'settings', 'hold the time');
    } },
    // ---------------------------------------------------------------- 3/4 look + goldens
    { name: 'colour + pixel proof: a live dark <-> light switch (accents 0, 2) = a fresh build, every pixel', async run(t) {
      const out = [];
      for (const p of [{}, { back: false, settings: true }, { sample: 'end-buttons', settings: true }])
        out.push(...await liveLook(t, 'header', { time: '21:42', ...p }, CW(t) + 2 * INSET[den(t)], lay(t, 'row') + 2 * INSET[den(t)]));
      t.eq(out.slice(0, 4), [], 'live = fresh');
    } },
    { name: 'golden images: every variant (dark + light): reviewed, never auto-accepted', async run(t, { den: d }) {
      const out = [];
      const cases = [['default', { label: 'Weather' }], ['home', { label: 'Home', back: false, settings: true }], ['end-chip', { label: 'Agents', sample: 'end-chip' }],
        ['end-buttons', { label: 'Photo', sample: 'end-buttons', settings: true }], ['title-segmented', { label: 'Vacuum', sample: 'title-segmented' }],
        ['long-full-end', { label: 'Living room ceiling and the sofa lamps', settings: true, sample: 'full-end' }], ['no-clock', { label: 'Settings', clock: false }]];
      for (const [n, p] of cases) for (const light of [0, 1]) { await at(t, p, undefined, light); out.push(...golden(t, 'header', `${n}-${light ? 'light' : 'dark'}`, d)); }
      t.eq(out, [], 'goldens');
    } },
    // ---------------------------------------------------------------- 6 robustness
    { name: 'tiny parents (0 x 0 .. 24 x 24): builds, no crash', async run(t) {
      for (const s of [0, 1, 8, 24]) { await at(t, { settings: true, sample: 'full-end' }, s); t.ok(t.objs().length > 0, `${s} px`); }
    } },
    { name: 'leak: 100 build / clear cycles: objects back, heap flat', async run(t) {
      await at(t, { sample: 'end-buttons', settings: true });
      t.eq(leak(t, 100, () => t.b.rebuild('header', { sample: 'end-buttons', settings: true, time: '21:42', actions: { status: 'timer' } }), () => t.b.rebuild('placeholder', {}), 64 * 1024), [], 'leak');
    } },
  ],
};
