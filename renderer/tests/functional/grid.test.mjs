// Grid (components/grid.cpp, contract v2; geometry = the core atom fx::grid), facet-205 (2026-10-27): equal cells from
// the grid's OWN width, slots with allow lists, items repeating from a list, rows / +N, the empty state. Each test is
// one row of the release gate and fails without the behaviour.
// Approved original: the pre-contract Grid (board facet-171, notes icon-grid-proposal §3): same sums, same look.
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { leak, look, golden, stage, boundsAll, liveLook, lab, tune, frame565, diff565 } from './_release.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const CORPUS = join(HERE, '..', 'spec-corpus', 'specs');
const den = (t) => (t.b.S > 600 ? '1.5x' : '1x');
const INSET = { '1.5x': 32, '1x': 20 };
const CW = (t) => t.b.S - 2 * INSET[den(t)];
const G4 = 4;  // the 4 px grid (1x); 8 at 1.5x (sp::GRID)
const GR = (t) => (den(t) === '1x' ? 4 : 8);
const BIG = (t) => { const o = metaOf(t, 'grid').props.find((p) => p.name === 'min').options; return o[o.length - 1]; };
const toks = (t) => new Map(JSON.parse(lab(t, 'tokens')).map((r) => [r[0], r]));
const px = (t, n) => toks(t).get(n)[2];
const ITEMS = 'bulb,power,timer,bell,person,door,window,house';
async function at(t, props, w, light = 0, h) {
  h = h ?? t.b.S;
  t.b.W = t.b.H = undefined;
  await t.mount('grid', props);
  if (light) look(t, 1);
  stage(t, (w ?? CW(t)) + 2 * INSET[den(t)], h); t.b.settle(800); t.b.events();
  return t.part('grid');
}
const cells = (t) => t.objs().filter((o) => o.clk && t.visible(o)).sort((a, b) => a.y - b.y || a.x - b.x);
const icons = (t) => t.comps().filter((c) => c.ids.includes('icon') && t.visible(c)).sort((a, b) => a.y - b.y || a.x - b.x);
const lab0 = (t, s) => t.objs().find((o) => o.text && String(o.text).toLowerCase() === s.toLowerCase() && t.visible(o));
const mid = (o) => [o.x + o.w / 2, o.y + o.h / 2];
const rows1 = (xs) => xs.filter((c) => c.y === xs[0].y).length;
const metaOf = (t, id) => { const m = JSON.parse(t.b.m.UTF8ToString(t.b.m._fw_meta())).find((c) => c.id === id); return typeof m.meta === 'string' ? JSON.parse(m.meta) : m.meta; };
const check = (t, nodes, header) => JSON.stringify(JSON.parse(t.b.spec('check', { v: 1, id: 'gslots', rev: 1, name: 'Slots', layout: 'list', ...(header ? { header } : {}), nodes }) || '{}'));
const expectW = (t, W, gap, cols) => Math.floor((W - gap * (cols - 1)) / cols / GR(t)) * GR(t);

export default {
  id: 'grid', family: 'layout', about: 'Grid layout on the contract: own-width columns, slots with allow lists, items, rows / +N, empty, update path, proofs.',
  tests: [
    // ---------------------------------------------------------------- slots (facet-205)
    { name: 'slots: meta declares cell (64, max 1) and item (template), each with an allow list of real components; contract 2', async run(t) {
      await at(t, { outline: true });
      const m = metaOf(t, 'grid'), ids = new Set(JSON.parse(t.b.m.UTF8ToString(t.b.m._fw_meta())).map((c) => c.id));
      t.eq(m.contract, 2, 'on the contract');
      t.eq(m.slots.map((s) => [s.name, s.count || 0, s.max || 0, !!s.template]), [['cell', 64, 1, false], ['item', 0, 0, true]], 'cell + item declarations');
      for (const comp of ['grid', 'header']) for (const s of metaOf(t, comp).slots) {
        t.ok(Array.isArray(s.accepts) && s.accepts.length > 0, `${comp}.${s.name}: an allow list`);
        const bad = s.accepts.filter((a) => !ids.has(a)); t.eq(bad, [], `${comp}.${s.name}: every accepted id is a component`);
      }
    } },
    { name: 'slot rules refused at spec load with a path: wrong type in a cell, two in one cell, wrong item template, each into a cell', async run(t) {
      await t.device();
      const g = { c: 'grid', p: { cols: 3 } };
      let r = check(t, [g, { c: 'stat', p: {}, in: 0, slot: 0 }, { c: 'icon', p: { name: 'bulb' }, in: 0, slot: 1 }]);
      t.ok(!r.includes('bad_slot'), `allowed nodes pass: ${r.slice(0, 120)}`);
      r = check(t, [g, { c: 'slider', p: {}, in: 0, slot: 0 }]);
      t.ok(r.includes('bad_slot') && r.includes('.nodes[1].c') && r.includes('not slider'), `slider in a cell refused at .nodes[1].c: ${r.slice(0, 200)}`);
      r = check(t, [g, { c: 'stat', p: {}, in: 0, slot: 2 }, { c: 'icon', p: { name: 'bulb' }, in: 0, slot: 2 }]);
      t.ok(r.includes('bad_slot') && r.includes('.nodes[2].in') && r.includes('at most 1'), `a second node in cell 2 refused at .nodes[2].in: ${r.slice(0, 200)}`);
      r = check(t, [{ c: 'grid', p: { items: 'a,b' }, cell: { c: 'slider', p: {} } }]);
      t.ok(r.includes('bad_slot') && r.includes('.nodes[0].cell.c'), `a slider item template refused at .nodes[0].cell.c: ${r.slice(0, 200)}`);
      r = check(t, [{ c: 'grid', p: { items: 'a,b' }, cell: { c: 'checkbox', p: { label: '$item' } } }]);
      t.ok(!r.includes('bad_slot'), `a checkbox template passes: ${r.slice(0, 120)}`);
      r = check(t, [g, { c: 'stat', p: {}, in: 0, slot: 0, each: 'rooms' }]);
      t.ok(r.includes('bad_slot') && r.includes('.nodes[1].each'), `each into a one-node cell refused: ${r.slice(0, 200)}`);
      r = check(t, [{ c: 'group', p: {} }, { c: 'slider', p: {}, in: 0 }]);
      t.ok(!r.includes('bad_slot'), 'a legacy layout (slots: N) takes any component, as before');
    } },
    // ---------------------------------------------------------------- contract: geometry from its own box
    { name: 'columns from its OWN width: equal square cells on the 4 px grid, centred within 4 px, the gutter between', async run(t) {
      for (const w of [CW(t), 333]) {
        const g = await at(t, { cols: 4, items: ITEMS }, w), gap = px(t, 'layout:gutter'); const cs = cells(t), cw = expectW(t, w, gap, 4);
        t.eq(g.w, w, `${w}: the grid = its parent width`);
        t.eq([cs.length, rows1(cs)], [8, 4], `${w}: 8 cells, 4 to a row`);
        t.eq([...new Set(cs.map((c) => `${c.w}x${c.h}`))], [`${cw}x${cw}`], `${w}: square cells ${cw} (on the grid)`);
        t.eq(cs[1].x - (cs[0].x + cs[0].w), gap, `${w}: gutter between`); t.eq(cs[4].y - (cs[0].y + cs[0].h), gap, `${w}: gutter between rows`);
        const left = cs[0].x - g.x, right = g.x + g.w - (cs[3].x + cs[3].w);
        t.ok(left % GR(t) === 0 && left >= 0 && right - left >= 0 && right - left < 2 * GR(t), `${w}: centred within 4 px (left ${left}, right ${right})`);
      }
    } },
    { name: 'gap none / near / block and aspect wide: the tokens and 16:9 on the grid', async run(t) {
      for (const [gp, tk] of [['none', null], ['near', 'space:3'], ['block', 'layout:group']]) {
        await at(t, { cols: 4, gap: gp, items: ITEMS }); const cs = cells(t);
        t.eq(cs[1].x - (cs[0].x + cs[0].w), tk ? px(t, tk) : 0, `gap ${gp}`);
      }
      await at(t, { cols: 2, aspect: 'wide', items: 'bulb,power' }); const c = cells(t)[0];
      t.eq(c.h, Math.floor(c.w * 9 / 16 / GR(t)) * GR(t), 'wide: 16:9 snapped down to the grid');
    } },
    { name: 'min: as many min cells as fit; the parent narrows and widens back = fewer then more columns (steps both ways)', async run(t) {
      await at(t, { outline: true }); const p = { min: BIG(t), gap: 'none', items: Array(30).fill('bolt').join(',') };
      await at(t, p); const n0 = rows1(icons(t));
      stage(t, Math.round(CW(t) / 2) + 2 * INSET[den(t)], t.b.S); t.b.settle(800); const n1 = rows1(icons(t));
      stage(t, CW(t) + 2 * INSET[den(t)], t.b.S); t.b.settle(800); const n2 = rows1(icons(t));
      t.ok(n0 > n1 && n1 >= 1 && n2 === n0 && n0 <= 12, `columns ${n0} -> ${n1} -> ${n2} (at most 12)`);
    } },
    { name: 'rows cap: rows x cols - 1 items and a last "+N" cell; a narrower parent = another column count, re-expanded', async run(t) {
      await at(t, { outline: true }); const p = { min: BIG(t), gap: 'none', rows: 1, items: Array(18).fill('bolt').join(',') };
      await at(t, p); const n = icons(t).length;
      t.ok(n >= 1 && !!lab0(t, `+${18 - n}`), `${n} icons + "+${18 - n}"`);
      stage(t, Math.round(CW(t) / 2) + 2 * INSET[den(t)], t.b.S); t.b.settle(800); const m = icons(t).length;
      t.ok(m < n && !!lab0(t, `+${18 - m}`), `narrower: ${m} icons + "+${18 - m}"`);
      t.eq(boundsAll(t), [], 'bounds');
    } },
    { name: 'spans: a 2 x 2 first cell, the rest in reading order into the first free place', async run(t) {
      await at(t, { cols: 4, outline: true, _cells: 6, _spans: { 0: [2, 2] } });
      const c = [0, 1, 2, 3, 4, 5].map((i) => mid(lab0(t, `cell ${i}`)));
      t.ok(c[1][0] > c[0][0] && c[2][0] > c[1][0] && c[1][1] === c[2][1] && c[1][1] < c[0][1], 'cells 1, 2: row 0 right of the span');
      t.ok(c[3][1] === c[4][1] && c[3][1] > c[1][1] && c[3][0] === c[1][0] && c[4][0] === c[2][0], 'cells 3, 4: row 1 right of the span');
      t.ok(c[5][1] > c[3][1] && c[5][0] < c[1][0], 'cell 5: row 2, the first column');
      t.eq(boundsAll(t), [], 'bounds');
    } },
    // ---------------------------------------------------------------- functional: items, empty, taps
    { name: 'items: a tap = pick(item) + tap sound; cells under touch:min are shown, never tapped (C32)', async run(t) {
      await at(t, { cols: 4, items: ITEMS }); const cs = cells(t);
      const ev = t.tap({ at: mid(cs[2]) }); t.emitted(ev, 'pick', 'timer'); t.sounds(ev, ['tap']);
      const w = 8 * (px(t, 'touch:min') - GR(t)); await at(t, { cols: 8, gap: 'none', items: 'bolt,bolt,bolt' }, w);
      t.eq([icons(t).length, cells(t).length], [3, 0], `cells ${w} / 8 wide (under touch:min): three icons, none clickable`);
    } },
    { name: 'empty: the empty words (not blank); an update fills it in place', async run(t) {
      const g0 = await at(t, { cols: 4, _items: true, items: '', empty: 'No bins today' });
      t.ok(!!lab0(t, 'No bins today'), 'the empty text'); t.ok(g0.h > 0, 'not a blank zero-height grid');
      const r = t.b.update('grid', { cols: 4, _items: true, items: 'bulb,power', empty: 'No bins today' }); t.b.settle(800);
      t.eq([r || 'ok', icons(t).length, !!lab0(t, 'No bins today')], ['ok', 2, false], 'filled in place');
    } },
    { name: 'update path: a changed item rebinds in place; more / fewer items in place; another shape = rebuild', async run(t) {
      await at(t, { cols: 4, items: 'bulb,power,timer' }); const n0 = t.objs().length;
      let r = t.b.update('grid', { cols: 4, items: 'bulb,bell,timer' }); t.b.settle(800);
      t.eq([r || 'ok', icons(t).length, t.objs().length], ['ok', 3, n0], 'same count: in place, same objects');
      r = t.b.update('grid', { cols: 4, items: 'bulb,bell,timer,door,window' }); t.b.settle(800); t.eq([r || 'ok', icons(t).length, rows1(cells(t))], ['ok', 5, 4], 'two added');
      r = t.b.update('grid', { cols: 4, items: 'bulb' }); t.b.settle(800); t.eq([r || 'ok', icons(t).length], ['ok', 1], 'four removed');
      r = t.b.update('grid', { cols: 3, items: 'bulb' }); t.eq(r, 'rebuild', 'cols: rebuild');
    } },
    // ---------------------------------------------------------------- tunables (Layout lab)
    { name: 'tunables: layout:gutter, space:3, layout:group are listed; each moves the cells live (no rebuild: the live object tree), bounds clean', async run(t) {
      const bad = [];
      for (const [gp, n] of [['group', 'layout:gutter'], ['near', 'space:3'], ['block', 'layout:group']]) {
        await at(t, { cols: 4, gap: gp, items: ITEMS });
        const tk = toks(t); if (!tk.has(n)) { bad.push(`${n} not in the tunables table`); continue; }
        const base = tk.get(n)[1];
        for (const v of [0, base + 8, base * 2]) {
          await at(t, { cols: 4, gap: gp, items: ITEMS });
          tune(t, n, v); t.b.settle(400); const cs = cells(t);
          if (cs.length && cs[1].x - (cs[0].x + cs[0].w) !== v) bad.push(`${n}=${v}: the gap is ${cs[1].x - (cs[0].x + cs[0].w)}`);
          if (cs.length && cs[0].w !== expectW(t, CW(t), v, 4)) bad.push(`${n}=${v}: cells ${cs[0].w}, want ${expectW(t, CW(t), v, 4)}`);
          for (const e of boundsAll(t)) bad.push(`${n}=${v}: ${e}`);
          tune(t, n, -1);
        }
      }
      tune(t, '', -1);
      t.eq(bad.slice(0, 6), [], 'tuned');
    } },
    // ---------------------------------------------------------------- proofs
    { name: 'bounds proof: 0 fails for every variant at the content width and 333 px, one column at the declared minimum (tile-w)', async run(t) {
      await at(t, { outline: true }); const bad = [], tw = px(t, 'layout:tile-w'), big = BIG(t);
      const vs = [{ outline: true }, { cols: 2, aspect: 'wide', outline: true }, { cols: 4, outline: true, _cells: 6, _spans: { 0: [2, 2] } }, { cols: 4, gap: 'near', items: ITEMS },
        { min: big, gap: 'none', rows: 1, items: Array(18).fill('bolt').join(',') }, { aspect: 'row', outline: true }, { _items: true, items: '' }];
      // the declared minimum = one tile-w column (meta min): a one-column grid at tile-w; every variant at 720 / 480 widths
      vs.push({ cols: 1, outline: true }, { cols: 1, items: 'bulb,power' });
      for (const p of vs) for (const w of p.cols === 1 ? [tw] : [CW(t), 333]) { await at(t, p, w); for (const e of boundsAll(t)) bad.push(`${JSON.stringify(p).slice(0, 40)} ${w}: ${e}`); }
      t.eq(bad.slice(0, 5), [], 'bounds');
    } },
    { name: 'colour + pixel proof: a live dark <-> light switch (accents 0, 2) = a fresh build, every pixel', async run(t) {
      const out = [];
      // the grid's own looks (outline sample, empty text, +N); an items grid's Icons are legacy components (not on the
      // contract yet: they hold their colour) and are proved when the Icon migrates
      for (const p of [{ outline: true }, { cols: 4, outline: true, _cells: 6, _spans: { 0: [2, 2] } }, { _items: true, items: '' }])
        out.push(...await liveLook(t, 'grid', p, CW(t) + 2 * INSET[den(t)], 480));
      t.eq(out.slice(0, 4), [], 'live = fresh');
    } },
    { name: 'golden images: outline, spans, wide, items, +N, empty (dark + light): reviewed, never auto-accepted', async run(t, { den: d }) {
      const out = [];
      const cases = [['outline', { outline: true }], ['spans', { cols: 4, outline: true, _cells: 6, _spans: { 0: [2, 2] } }], ['wide', { cols: 2, aspect: 'wide', outline: true }],
        ['items', { cols: 4, gap: 'near', items: ITEMS }], ['plus-n', { min: 'feature', gap: 'none', rows: 1, items: Array(18).fill('bolt').join(',') }], ['empty', { _items: true, items: '' }]];
      for (const [n, p] of cases) for (const light of [0, 1]) { await at(t, p, undefined, light, t.b.S); out.push(...golden(t, 'grid', `${n}-${light ? 'light' : 'dark'}`, d)); }
      t.eq(out, [], 'goldens');
    } },
    // ---------------------------------------------------------------- robustness
    { name: 'tiny parents (0 x 0 .. 24 x 24): builds, no crash', async run(t) {
      for (const s of [0, 1, 8, 24]) for (const p of [{ outline: true }, { min: 'glyph', items: ITEMS, rows: 1 }, { _items: true, items: '' }]) { await at(t, p, s, 0, s + 2 * INSET[den(t)]); t.ok(t.objs().length > 0, `${s} px`); }
    } },
    { name: 'leak: 100 build / clear cycles and 100 item updates: objects back, heap flat', async run(t) {
      await at(t, { cols: 4, items: ITEMS });
      t.eq(leak(t, 100, () => t.b.rebuild('grid', { cols: 4, rows: 1, items: ITEMS }), () => t.b.rebuild('placeholder', {}), 64 * 1024), [], 'leak (build)');
      await at(t, { cols: 4, items: ITEMS });
      t.eq(leak(t, 100, () => t.b.update('grid', { cols: 4, items: 'bulb,power,timer' }), () => t.b.update('grid', { cols: 4, items: ITEMS }), 64 * 1024), [], 'leak (updates)');
    } },
    // ---------------------------------------------------------------- product: the existing callers
    { name: 'existing callers: every corpus spec with a grid still checks clean and applies', async run(t) {
      await t.device();
      for (const f of ['icon_grid', 'grid_items', 'grid_items_row', 'isaac_run']) {
        const spec = JSON.parse(readFileSync(join(CORPUS, `${f}.json`), 'utf8'));
        const r = JSON.stringify(JSON.parse(t.b.spec('check', spec) || '{}'));
        t.ok(!r.includes('bad_slot') && !r.includes('bad_cell'), `${f}: no slot errors (${r.slice(0, 120)})`);
        t.eq(t.send(spec).status, 'applied', `${f}: applied`);
      }
    } },
  ],
};
