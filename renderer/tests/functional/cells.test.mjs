// Cells (components/cells.cpp, contract v2), release piece 3 (2026-10-26): the Home grid as a layout of slotted
// children. Each test is one row of the release gate and fails without the behaviour. The layout fills its parent (the
// stage's content box, resized to the parent size under test); its columns come from its own width, the cells' min-w
// token and the gutter. Sample mode = a real Card in every one-slot cell, an outlined "Readout" in each half.
// Approved original: the Astro Home grid (packages/ui/src/layout.ts: 3 tiles / 2 cards per row, gutter between).
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { fuzz, leak, look, golden, stage, boundsAll } from './_release.mjs';

const TOK = join(dirname(fileURLToPath(import.meta.url)), '..', '..', 'tokens');
const SPACE = JSON.parse(readFileSync(join(TOK, 'space.json'), 'utf8')).tokens;
const LAYOUT = JSON.parse(readFileSync(join(TOK, 'layout.json'), 'utf8'));
const den = (t) => (t.b.S > 600 ? '1.5x' : '1x');
const tk = (t, n) => (den(t) === '1x' ? SPACE.layout[n]['1x'] : SPACE.layout[n].v);
const GUT = (t) => { const g = JSON.stringify(LAYOUT).match(/"gutter":\{"v":(\d+),"1x":(\d+)/); return +(den(t) === '1x' ? g[2] : g[1]); };
const INSET = { '1.5x': 32, '1x': 20 };
const CW = (t) => t.b.S - 2 * INSET[den(t)];  // the Body's content width
const TILES = { 'min-w': 'tile-w', 'min-h': 'tile-h' }, CARDS = { 'min-w': 'home-card-min-w', 'min-h': 'card-h' };
// mount Cells in a parent w wide (the stage = the parent + the page inset each side), tall enough for its rows
async function at(t, props, w, light = 0) {
  t.b.W = t.b.H = undefined;
  await t.mount('cells', props);
  if (light) look(t, 1);
  stage(t, (w ?? CW(t)) + 2 * INSET[den(t)], TALL); t.b.settle(800); t.b.events();
  return t.part('cells');
}
const cards = (t) => t.comps().filter((c) => c.ids.includes('card')).sort((a, b) => a.y - b.y || a.x - b.x);
const halves = (t) => t.objs().filter((o) => o.text && /readout/i.test(o.text));
const cols = (t) => { const cs = cards(t); return cs.filter((c) => c.y === cs[0].y).length; };
const TALL = 3000;  // a parent tall enough for every row (the layout's height = its rows)
const grid = (t) => (den(t) === '1x' ? 4 : 8);

export default {
  id: 'cells', family: 'layout', about: 'Cells layout on the contract: columns from its own width, fillers, pairs, reflow, update path, proofs.',
  tests: [
    // ---------------------------------------------------------------- 1 contract
    { name: 'tokens: 3 tiles and 2 Home cards fill the content width; a third card does not fit (two-up)', async run(t) {
      const g = GUT(t), w = CW(t);
      t.eq(3 * tk(t, 'tile-w') + 2 * g, w, '3 x tile-w + 2 gutters = content width');
      t.ok(2 * tk(t, 'home-card-min-w') + g <= w && 3 * tk(t, 'home-card-min-w') + 2 * g > w, 'home-card-min-w: 2 fit, 3 do not');
      t.eq(tk(t, 'card-min-w'), tk(t, 'tile-w'), 'card-min-w unchanged (= tile-w)');
      t.eq(2 * tk(t, 'pair-h') + 8, tk(t, 'card-h'), 'two pair-h halves + space share card-h');
    } },
    { name: 'fills its parent; tiles three to a row, cards two to a row, equal widths filling the row', async run(t) {
      for (const [p, n, h] of [[TILES, 3, 'tile-h'], [CARDS, 2, 'card-h']]) {
        const g = await at(t, { ...p, cells: 'c,c,c,c,c,c' }); t.eq(g.w, CW(t), 'the layout = its parent width');
        const cs = cards(t); t.eq(cols(t), n, `${p['min-w']}: ${n} columns`);
        const w = (CW(t) - (n - 1) * GUT(t)) / n;
        t.eq([...new Set(cs.map((c) => c.w))], [w], 'equal cells sharing the width'); t.eq([...new Set(cs.map((c) => c.h))], [tk(t, h)], `cell height = ${h}`);
        t.eq(cs[1].x - (cs[0].x + cs[0].w), GUT(t), 'gutter between cells'); t.eq(cs[n].y - (cs[0].y + cs[0].h), GUT(t), 'gutter between rows');
      }
    } },
    { name: 'a short last row keeps the cell width (fillers), left-aligned', async run(t) {
      await at(t, { ...TILES, cells: 'c,c,c,c,c' }); const cs = cards(t);
      t.eq([cs[3].x, cs[3].w, cs[4].x], [cs[0].x, cs[0].w, cs[1].x], 'row 2 cells under row 1 cells, same width');
      await at(t, { ...CARDS, cells: 'c' }); t.eq(cards(t)[0].w, (CW(t) - GUT(t)) / 2, 'one card alone = half the row, not stretched');
    } },
    { name: 'last "fill": the cells on a short last row share the whole row; full rows and the default unchanged', async run(t) {
      const half = (CW(t) - GUT(t)) / 2;
      await at(t, { ...CARDS, cells: 'c,c,c', last: 'fill' }); t.eq(cards(t).map((c) => c.w), [half, half, CW(t)], 'a lone last card = the full row');
      await at(t, { ...CARDS, cells: 'c', last: 'fill' }); t.eq(cards(t)[0].w, CW(t), 'one card alone = the full row');
      await at(t, { ...TILES, cells: 'c,c,c,c,c', last: 'fill' }); const ts = cards(t), tw = (CW(t) - 2 * GUT(t)) / 3;
      t.eq([ts[0].w, ts[3].w, ts[4].w], [tw, half, half], 'tiles: two on the last row share it');
      await at(t, { ...CARDS, cells: 'c,c,c' }); t.eq(cards(t)[2].w, half, 'default column: the lone card keeps the column width');
      const r = t.b.update('cells', { ...CARDS, cells: 'c,c,c', last: 'fill' }); t.eq(r, 'rebuild', 'last changed: rebuild');
    } },
    { name: 'wide: a wide card (full width, one per row)', async run(t) {
      await at(t, { ...CARDS, cells: 'c,c', wide: true }); t.eq(cards(t).map((c) => c.w), [CW(t), CW(t)], 'full width, one per row');
    } },
    { name: 'a pair: two half-height slots in one card cell, the first at its top, the second ending on its bottom, on the grid', async run(t) {
      await at(t, { ...CARDS, cells: 'c,p' }); const c = cards(t)[0], hs = halves(t);
      t.eq(hs.length, 2, 'two halves');
      const boxes = t.objs().filter((o) => !o.text && o.w === c.w && o.h === tk(t, 'pair-h') && o.x > c.x);
      t.eq(boxes.length >= 2, true, `two half boxes pair-h tall (${boxes.length})`);
      const ys = boxes.map((b) => b.y).sort((a, b) => a - b);
      t.eq([ys[0], ys[ys.length - 1] + tk(t, 'pair-h')], [c.y, c.y + tk(t, 'card-h')], 'top at the cell top, bottom on the cell bottom');
      t.eq(ys.every((y) => (y - c.y) % grid(t) === 0), true, 'halves on the grid');
    } },
    { name: 'bounds proof: 0 fails for tiles, cards, pairs at full, odd, wide and minimum parents', async run(t) {
      const bad = [];
      for (const p of [{ ...TILES, cells: 'c,c,c,c' }, { ...CARDS, cells: 'c,p,c' }, { ...CARDS, cells: 'c', wide: true }])
        for (const w of [CW(t), 333, tk(t, p['min-w']), tk(t, p['min-w']) + 5, 2 * tk(t, p['min-w']) + GUT(t) - 1]) {
          await at(t, p, w); for (const e of boundsAll(t)) bad.push(`${p['min-w']} ${w}: ${e}`);
        }
      t.eq(bad.slice(0, 5), [], 'bounds');
    } },
    { name: 'below the declared minimum (one cell) the bounds proof FAILS (by design)', async run(t) {
      await at(t, { ...TILES, cells: 'c' }, tk(t, 'tile-w') - 8); t.ok(boundsAll(t).length > 0, 'a layout 8 px under one tile-w is reported');
    } },
    // ---------------------------------------------------------------- 2 functional: steps + update path
    { name: 'reflows both ways: the parent narrows (3 -> 2 -> 1 columns) and widens back, slots keep their order', async run(t) {
      await at(t, { ...TILES, cells: 'c,c,c,c,c' }); t.eq(cols(t), 3, '3 at full');
      const order = () => cards(t).map((c) => `${c.x},${c.y}`).join(' ');
      for (const [w, n] of [[2 * tk(t, 'tile-w') + GUT(t) + 4, 2], [tk(t, 'tile-w') + 4, 1], [CW(t), 3]]) {
        stage(t, w + 2 * INSET[den(t)], TALL); t.b.settle(800); t.eq(cols(t), n, `${w}: ${n} columns`);
      }
      const cs = cards(t); t.ok(cs[0].x < cs[1].x && cs[2].x > cs[1].x && cs[3].y > cs[0].y, `reading order kept (${order()})`);
    } },
    { name: 'update path: cells added / removed at the end in place (same root); another minimum or wide = rebuild', async run(t) {
      const g0 = await at(t, { ...TILES, cells: 'c,c' }); t.props = { ...TILES, cells: 'c,c' }; t.comp = 'cells';
      let r = t.b.update('cells', { ...TILES, cells: 'c,c,c,c' }); t.b.settle(800);
      t.eq([r || 'ok', cards(t).length, cols(t)], ['ok', 4, 3], 'two added in place, reflowed');
      r = t.b.update('cells', { ...TILES, cells: 'c' }); t.b.settle(800); t.eq([r || 'ok', cards(t).length], ['ok', 1], 'three removed in place');
      r = t.b.update('cells', { ...TILES, cells: 'c', wide: true }); t.eq(r, 'rebuild', 'wide: rebuild');
      r = t.b.update('cells', { ...CARDS, cells: 'c,c,c' }); t.eq(r, 'rebuild', 'another minimum: rebuild');
      t.eq(t.part('cells').x, g0.x, 'still there');
    } },
    { name: 'another cell height token: rows that tall, a gutter apart, nothing overlaps', async run(t) {
      await at(t, { ...TILES, 'min-h': 'card-min-h', cells: 'c,c,c,c' });
      const cs = cards(t); t.eq(cs[0].h, tk(t, 'card-min-h'), 'cell = card-min-h'); t.eq(cs[3].y, cs[0].y + cs[0].h + GUT(t), 'rows a gutter apart'); t.eq(boundsAll(t), [], 'bounds');
    } },
    // ---------------------------------------------------------------- 3/4 look + goldens
    { name: 'golden images: tiles, cards, pair, short row, wide (dark + light): reviewed, never auto-accepted', async run(t, { den: d }) {
      const out = [];
      const cases = [['tiles', { ...TILES, cells: 'c,c,c,c,c' }], ['cards', { ...CARDS, cells: 'c,c,c' }], ['pair', { ...CARDS, cells: 'c,p' }], ['wide', { ...CARDS, cells: 'c', wide: true }], ['lone-card', { ...CARDS, cells: 'c,c,c', last: 'fill' }]];
      for (const [n, p] of cases) for (const light of [0, 1]) { await at(t, p, undefined, light); stage(t, t.b.S, t.b.S); t.b.settle(800); out.push(...golden(t, 'cells', `${n}-${light ? 'light' : 'dark'}`, d)); }
      t.eq(out, [], 'goldens');
    } },
    // ---------------------------------------------------------------- 6 robustness
    { name: 'fuzz: 120 random cell lists x minimums x parent widths >= one cell (seed 33): bounds clean', async run(t) {
      const bad = [];
      await fuzz(t, 120, 33, async (r) => {
        const p = r() < 0.5 ? TILES : CARDS, n = r.int(1, 9), cells = Array.from({ length: n }, () => (p === CARDS && r() < 0.3 ? 'p' : 'c')).join(',');
        const w = r.int(tk(t, p['min-w']), CW(t)), props = { ...p, cells, wide: r() < 0.2 };
        await at(t, props, w); for (const e of boundsAll(t)) bad.push(`${JSON.stringify(props)} ${w}: ${e}`);
      });
      t.eq(bad.slice(0, 5), [], 'bounds over the fuzz');
    } },
    { name: 'leak: 100 build / clear cycles: objects back, heap flat', async run(t) {
      await at(t, { ...CARDS, cells: 'c,p,c' });
      t.eq(leak(t, 100, () => t.b.rebuild('cells', { ...CARDS, cells: 'c,p,c' }), () => t.b.rebuild('placeholder', {}), 64 * 1024), [], 'leak');
    } },
    // ---------------------------------------------------------------- 10 product: Home
    { name: 'Home: tiles three to a row, cards two-up, two readouts share a cell, a lone last card and a wide card full width', async run(t) {
      const L = 'light.test_lamp', S = 'sensor.test_temp';
      t.ha.s[L] = { '': 'on', brightness: '179', friendly_name: 'Desk lamp', supported_color_modes: '["brightness"]' };
      t.ha.s[S] = { '': '21.5', friendly_name: 'Hall', unit_of_measurement: '°C', device_class: 'temperature' };
      const cardsDeck = [{ t: 'light', e: L, size: 's', tap: 'toggle' }, { t: 'light', e: L, size: 's', tap: 'toggle' }, { t: 'light', e: L, size: 's', tap: 'toggle' },
        { t: 'light', e: L, size: 'm', tap: 'detail', label: 'Desk lamp' }, { t: 'sensor', e: S, size: 'm' }, { t: 'sensor', e: S, size: 'm' },
        { t: 'light', e: L, size: 'm', tap: 'detail', label: 'Lamp two' }];
      await t.device({ v: 2, rev: 9, start: 'p', pages: [{ id: 'p', kind: 'home', name: 'Test', cards: cardsDeck }] });
      const ts = t.comps().filter((c) => c.ids.includes('entity_tile')), cs = t.comps().filter((c) => c.ids.includes('card')).sort((a, b) => a.y - b.y || a.x - b.x);
      t.eq(ts.length, 3, 'three tiles'); t.eq(new Set(ts.map((c) => c.y)).size, 1, 'one row of tiles');
      t.eq(cs.length, 2, 'two cards'); t.eq(cs.map((c) => c.w), [(CW(t) - GUT(t)) / 2, CW(t)], 'cards two-up (half the row); the lone last card fills the row');
      t.eq(cs[0].y, ts[0].y + ts[0].h + GUT(t), 'the card run starts a gutter under the tiles');
      t.eq(t.comps().filter((c) => c.ids.includes('readout')).length, 2, 'two readouts (one shared cell)');
      t.tap({ at: [cs[0].x + cs[0].w / 2, cs[0].y + cs[0].h / 2] }); t.eq(t.where(), 'light', 'a card opens its screen');
    } },
  ],
};
