// Card (components/card.cpp, contract v2), release piece 2 (2026-10-26). Each test is one row of the release gate and
// fails without the behaviour. The card has no size of its own: it fills its parent (here the stage's content box, the
// stage resized to the parent size under test; on Home the grid cell). Declared minimum: layout:card-min-w x card-min-h.
// Approved original: Astro packages/ui/src/layout.ts card() + docs CSS .fs-card (320 x 280 at 720).
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { fixtures, fuzz, monkey, leak, look, frame565, diff565, golden, frames, stage, boundsAll } from './_release.mjs';

const TOK = join(dirname(fileURLToPath(import.meta.url)), '..', '..', 'tokens');
const SPACE = JSON.parse(readFileSync(join(TOK, 'space.json'), 'utf8')).tokens, TYPE = JSON.parse(readFileSync(join(TOK, 'type.json'), 'utf8')).styles;
const den = (t) => (t.b.S > 600 ? '1.5x' : '1x');
const tk = (t, g, n) => (den(t) === '1x' ? SPACE[g][n]['1x'] : SPACE[g][n].v);
const box = (t, n) => (den(t) === '1x' ? TYPE[n]['1x'].box : TYPE[n].box);
const INSET = { '1.5x': 32, '1x': 20 };
const FULL = (t) => [tk(t, 'layout', 'card-w'), tk(t, 'layout', 'card-h')];
const MIN = (t) => [tk(t, 'layout', 'card-min-w'), tk(t, 'layout', 'card-min-h')];
const P = { label: 'Weather', icon: 'weather', preview: 'none', value: '13°', sub: 'Dry for the next hour' };
// mount the card in a parent of w x h (the stage = the parent + the page inset each side)
async function at(t, props, w, h, light = 0) {
  t.b.W = t.b.H = undefined;
  await t.mount('card', { ...P, ...props });
  if (light) { look(t, 1); }
  stage(t, w + 2 * INSET[den(t)], h); t.b.settle(800); t.b.events();
  return t.part('card');
}
const full = (t, props = {}) => at(t, props, ...FULL(t));
const pix = (t, r) => { const W = t.b.W, f = frame565(t), out = []; for (let y = r.y; y < r.y + r.h; y++) for (let x = r.x; x < r.x + r.w; x++) out.push(f[y * W + x]); return out; };
const px = (t, x, y) => frame565(t)[y * t.b.W + x];
const preview = (t, c) => t.objs().filter((o) => o.x > c.x + c.w / 2 && o.y < c.y + c.h / 3 && o.w >= 40 && o.h >= 40 && !o.text);
const subOf = (t, s) => t.objs().find((o) => o.text === s);
// the drawn previews (components/card_previews.cpp) are legacy: their marks overflow their own boxes (agents dots, energy
// bars), which the card's proof sees. Their migration is its own piece: proven separately (xfail below), the card's
// rows use the weather-icon previews (contract icons)
const DRAWN = ['agents', 'media', 'dial', 'switch', 'clock', 'timer', 'github', 'energy', 'lightning'];
const VARIANTS = [{}, { mode: 'text', text: 'Paper Lanterns on a Quiet Evening by the Sea' }, { accent: true }, { tone: 'warn' }, { preview: 'partly-day' },
  { preview: 'clear-night', icon: 'energy' }, { state: 'unavailable' }, { state: 'pressed' }, { icon: 'none' }];

export default {
  id: 'card', family: 'layout', about: 'Home card on the contract: fills its parent, declared minimum, scale steps, states, proofs.',
  tests: [
    // ---------------------------------------------------------------- 1 contract
    { name: 'fills its parent: full size, odd sizes, wide (no card-w anywhere)', async run(t) {
      for (const [w, h] of [FULL(t), [333, 251], [t.b.S - 2 * INSET[den(t)], FULL(t)[1]], [MIN(t)[0] + 7, MIN(t)[1] + 13], [517, 401]]) {
        const c = await at(t, {}, w, h); t.eq([c.w, c.h], [w, h], `card = its parent ${w}x${h}`);
      }
    } },
    { name: 'declared minimum = the compact step\'s sum (layout relationship), card-h holds the full step', async run(t) {
      const pad = tk(t, 'layout', 'card-pad'), s2 = tk(t, 'space', '2'), s9 = tk(t, 'space', '9');
      t.eq(MIN(t)[1], 2 * pad + box(t, 'label') + box(t, 'title') + s2 + box(t, 'body'), 'card-min-h = 2 pad + label + title + space 2 + body');
      t.eq(MIN(t)[0], tk(t, 'layout', 'tile-w'), 'card-min-w = tile-w');
      t.ok(FULL(t)[1] >= 2 * pad + box(t, 'label') + box(t, 'value') + s9 + box(t, 'body'), 'card-h holds the full step');
      t.eq(s9 + box(t, 'body') + pad, den(t) === '1x' ? 56 : 88, 'value bottom = card-value-bottom from the edge (docs)');
    } },
    { name: 'bounds proof: 0 fails for every variant at full, odd, wide and minimum parents', async run(t) {
      const sizes = [FULL(t), [333, 251], [t.b.S - 2 * INSET[den(t)], FULL(t)[1]], MIN(t), [MIN(t)[0] + 5, MIN(t)[1] + 3]], bad = [];
      for (const v of VARIANTS) for (const [w, h] of sizes) { await at(t, v, w, h); for (const e of boundsAll(t)) bad.push(`${JSON.stringify(v)} ${w}x${h}: ${e}`); }
      t.eq(bad.slice(0, 5), [], 'bounds');
    } },
    { name: 'below the declared minimum the bounds proof FAILS (by design)', async run(t) {
      await at(t, {}, MIN(t)[0], MIN(t)[1] - 8); t.ok(boundsAll(t).length > 0, 'a card 8 px under card-min-h is reported');
    } },
    { name: 'scales: full step (value type, preview) -> compact below the full need -> full again when the parent grows', async run(t) {
      let c = await full(t, { preview: 'partly-day' }); const v0 = t.objs().find((o) => o.text === '13°');
      t.ok(preview(t, c).length > 0, 'full: preview'); t.eq(v0.h, box(t, 'value'), 'full: value type box');
      stage(t, MIN(t)[0] + 40 + 2 * INSET[den(t)], MIN(t)[1] + 8); t.b.settle(800); c = t.part('card');
      const v1 = t.objs().find((o) => o.text === '13°'); t.eq(preview(t, c).length, 0, 'compact: no preview'); t.eq(v1.h, box(t, 'title'), 'compact: value in type title');
      stage(t, FULL(t)[0] + 2 * INSET[den(t)], FULL(t)[1]); t.b.settle(800); c = t.part('card');
      t.ok(preview(t, c).length > 0, 'full again: preview back'); t.eq(t.objs().find((o) => o.text === '13°').h, box(t, 'value'), 'value type back');
    } },
    { name: 'hit areas: one touch target per card (the card), >= touch min; Home cards never overlap', async run(t) {
      const c = await full(t, { accent: true, preview: 'partly-day' });
      const clk = t.objs().filter((o) => o.clk && o.x >= c.x && o.y >= c.y && o.x + o.w <= c.x + c.w && o.y + o.h <= c.y + c.h);
      t.eq(clk.length, 1, 'only the card itself is clickable');
      t.ok(c.h >= tk(t, 'touch', 'min') && c.w >= tk(t, 'touch', 'min'), 'target >= touch min');
    } },
    // ---------------------------------------------------------------- 2 functional: props + states
    { name: 'value vs text: value one line; text up to two lines of title then "..."', async run(t) {
      await full(t, { mode: 'text', text: 'Paper Lanterns on a Quiet Evening by the Sea with a very long name that goes on' });
      const tx = t.objs().find((o) => o.text && /^Paper/.test(o.text)); t.ok(tx.h <= 2 * box(t, 'title') && tx.h > box(t, 'title'), `two lines (${tx.h})`);
      t.hasNot(t.texts(), '13°', 'no value in text mode');
      await full(t, { value: '1234567890123 kWh' }); const v = t.objs().find((o) => o.text && /^12345/.test(o.text)); t.eq(v.h, box(t, 'value'), 'value one line');
    } },
    { name: 'sub tones: ok / warn / error each colour the sub line differently from dim', async run(t) {
      const ink = async (tone) => { await full(t, { tone }); const s = subOf(t, P.sub); return new Set(pix(t, s)).size > 1 ? [...new Set(pix(t, s))].sort().join() : ''; };
      const dim = await ink('dim'); for (const tn of ['ok', 'warn', 'error']) t.ok((await ink(tn)) !== dim, `${tn} differs from dim`);
    } },
    { name: 'accent: the dot after the label + value in accent-text; none without', async run(t) {
      const dot = (c) => t.objs().filter((o) => o.w === tk(t, 'layout', 'dot') && o.h === tk(t, 'layout', 'dot') && o.y < c.y + c.h / 3);
      let c = await full(t, { accent: true }); t.eq(dot(c).length, 1, 'dot'); const lab = t.objs().find((o) => o.text === 'WEATHER');
      t.ok(dot(c)[0].x >= lab.x + lab.w && dot(c)[0].x % 4 === 0, 'dot after the label, on the grid');
      const va = pix(t, t.objs().find((o) => o.text === '13°')).join();
      c = await full(t, {}); t.eq(dot(c).length, 0, 'no dot'); t.ok(pix(t, t.objs().find((o) => o.text === '13°')).join() !== va, 'value colour differs');
    } },
    { name: 'preview kinds: every option builds inside the card (none = nothing top right)', async run(t) {
      const bad = [];
      for (const pv of ['none', 'partly-day', 'rain-light', 'clear-night']) {
        const c = await full(t, { preview: pv }); bad.push(...boundsAll(t).map((e) => `${pv}: ${e}`));
        if (pv === 'none' && preview(t, c).length) bad.push('none shows a preview');
      }
      t.eq(bad.slice(0, 5), [], 'previews');
    } },
    { name: 'drawn previews (legacy card_previews) stay inside their box', xfail: 'card_previews is not on the contract yet: its own piece', async run(t) {
      const bad = []; for (const pv of DRAWN) { await full(t, { preview: pv }); bad.push(...boundsAll(t).map((e) => `${pv}: ${e}`)); } t.eq(bad.slice(0, 3), [], 'drawn previews');
    } },
    { name: 'pressed: fill = tint bg; border = edge only; border-fill = both (corner + edge pixels)', async run(t) {
      const sample = async (props) => { const c = await full(t, props); return { bg: px(t, c.x + c.w - 6, c.y + c.h - 6), edge: px(t, c.x + c.w / 2, c.y) }; };
      const n = await sample({}), f = await sample({ state: 'pressed', press: 'fill' }), b = await sample({ state: 'pressed', press: 'border' }), bf = await sample({ state: 'pressed', press: 'border-fill' });
      t.ok(f.bg !== n.bg, 'fill: bg tinted'); t.ok(b.bg === n.bg, 'border: bg stays surface'); t.ok(b.edge !== n.edge, 'border: edge lit');
      t.ok(bf.bg !== n.bg && bf.edge !== n.edge, 'border-fill: both');
    } },
    { name: 'unavailable: em dash, "Unavailable", no preview, no press look; tap = blocked + error, no tap', async run(t) {
      const c = await full(t, { state: 'unavailable', preview: 'partly-day' });
      t.has(t.texts(), '—', 'em dash'); t.has(t.texts(), 'Unavailable', 'word'); t.eq(preview(t, c).length, 0, 'no preview');
      const before = pix(t, { x: c.x + 2, y: c.y + c.h - 8, w: c.w - 4, h: 4 }).join();
      t.b.touch(c.x + c.w / 2, c.y + c.h / 2, 1); t.b.tick(20); const held = pix(t, { x: c.x + 2, y: c.y + c.h - 8, w: c.w - 4, h: 4 }).join(); t.b.touch(c.x + c.w / 2, c.y + c.h / 2, 0); t.b.settle(800); t.b.events();
      t.eq(held === before, true, 'no press look');
      const ev = t.tap({ at: [c.x + c.w / 2, c.y + c.h / 2] }); t.emitted(ev, 'blocked'); t.sounds(ev, ['error']); t.notEmitted(ev, 'tap');
    } },
    { name: 'empty values: no label, no value, no sub: builds, bounds clean', async run(t) {
      await full(t, { label: '', value: '', sub: '', icon: 'none' }); t.eq(boundsAll(t), [], 'bounds');
      await full(t, { mode: 'text', text: '' }); t.eq(boundsAll(t), [], 'bounds text');
    } },
    // ---------------------------------------------------------------- 2 functional: interaction
    { name: 'tap: emits tap (detail = the label) + sound open; dragging off cancels (nothing)', async run(t) {
      const c = await full(t, {});
      const ev = t.tap({ at: [c.x + c.w / 2, c.y + c.h / 2] }); t.emitted(ev, 'tap', 'WEATHER'); t.sounds(ev, ['open']);
      const off = t.drag(null, { from: [c.x + c.w / 2, c.y + c.h / 2], by: [0, c.h] }); t.notEmitted(off, 'tap', 'dragged off: no tap');
    } },
    { name: 'long + non-ASCII labels / values / units: cut by rule, inside the card, the dot stays in', async run(t) {
      const bad = [];
      for (const [k, n] of Object.entries(fixtures)) for (const sz of [FULL(t), MIN(t)]) {
        await at(t, { label: n, value: n + ' 123456 kWh', sub: n, accent: true }, ...sz); bad.push(...boundsAll(t).map((e) => `${k} ${sz}: ${e}`));
      }
      t.eq(bad.slice(0, 5), [], 'bounds');
    } },
    { name: 'update: a tone / accent change fades (not one frame), silent, and ends on the shared style', async run(t) {
      const c = await full(t, {}); const s0 = pix(t, subOf(t, P.sub)).join();
      t.props = { ...P }; t.comp = 'card';
      const r = t.b.update('card', { ...P, tone: 'error' }); t.b.tick(2); const mid = pix(t, subOf(t, P.sub)).join(); t.b.settle(1500);
      const end = pix(t, subOf(t, P.sub)).join(); if (den(t) === '1.5x') t.ok(mid !== end || mid === s0, `still fading after 2 ticks (${r || 'ok'})`);  // 1x = the Guition profile: fades off by design (Motion), the change is instant t.ok(end !== s0, 'ends on error');
      look(t, 1, 2); const live = pix(t, subOf(t, P.sub)).join(); await at(t, { tone: 'error' }, ...FULL(t)); look(t, 1, 2);
      t.eq(live === pix(t, subOf(t, P.sub)).join(), true, 'after the fade a theme switch reaches it (no colour left behind)');
    } },
    // ---------------------------------------------------------------- 3/4 colour + pixel proof
    { name: 'colour + pixel proof: live light/dark x 5 accents = a fresh build (incl. a card pressed + released before)', async run(t) {
      const bad = [];
      for (const props of [{ accent: true, tone: 'ok' }, { mode: 'text', text: 'Quiet Evening' }, { state: 'pressed', press: 'border-fill' }, { state: 'unavailable' }]) {
        for (const light of [0, 1]) for (let a = 0; a < 5; a++) {
          let c = await full(t, props);
          if (props.state !== 'pressed') { t.tap({ at: [c.x + c.w / 2, c.y + c.h / 2] }); t.b.settle(800); }  // pressed before the switch (the old tile accent bug class)
          look(t, light, a); t.b.settle(800); c = t.part('card'); const live = pix(t, c);
          t.b.W = t.b.H = undefined; await t.b.render('card', { ...P, ...props }, light);
          t.b.str('card', (cc) => t.b.str(JSON.stringify({ ...P, ...props }), (pp) => t.b.m._fw_render(t.b.stage, cc, pp, light, a)));
          stage(t, FULL(t)[0] + 2 * INSET[den(t)], FULL(t)[1]); t.b.settle(1500);
          const n = diff565(live, pix(t, t.part('card'))); if (n) bad.push(`${JSON.stringify(props)} ${light ? 'light' : 'dark'} accent ${a}: ${n} px`);
        }
      }
      t.eq(bad.slice(0, 6), [], 'live switch = fresh build');
    } },
    { name: 'golden images: states x full / compact (dark; default + pressed light): reviewed, never auto-accepted', async run(t, { den: d }) {
      const out = [];
      const states = [['default', {}], ['preview', { preview: 'partly-day' }], ['energy', { preview: 'energy', icon: 'energy', value: '£2.42', sub: 'Today so far' }],
        ['text', { mode: 'text', text: 'Paper Lanterns on a Quiet Evening', icon: 'media', label: 'Media' }], ['accent', { accent: true, icon: 'timer', label: 'Timer', value: '04:32' }],
        ['warn', { tone: 'warn' }], ['unavailable', { state: 'unavailable' }], ['pressed-fill', { state: 'pressed', press: 'fill' }],
        ['pressed-border', { state: 'pressed', press: 'border' }], ['pressed-border-fill', { state: 'pressed', press: 'border-fill' }]];
      for (const [n, p] of states) for (const [sn, sz] of [['full', FULL(t)], ['min', MIN(t)]]) { await at(t, p, ...sz); out.push(...golden(t, 'card', `${n}-${sn}-dark`, d)); }
      for (const [n, p] of [states[0], states[7]]) { await at(t, p, ...FULL(t), 1); out.push(...golden(t, 'card', `${n}-full-light`, d)); }
      t.eq(out, [], 'goldens');
    } },
    // ---------------------------------------------------------------- 6 robustness + performance
    { name: 'fuzz: 150 random props x parent sizes >= the minimum (seed 21): no crash, bounds clean', async run(t) {
      const opts = { mode: ['value', 'text'], tone: ['dim', 'ok', 'warn', 'error', 'bogus'], state: ['default', 'pressed', 'unavailable', 'x'], press: ['fill', 'border', 'border-fill', 'x'],
        preview: ['none', 'partly-day', 'rain-light', 'clear-night', 'energy', 'nope'], icon: ['none', 'weather', 'light', 'nope'] };
      const words = [...Object.values(fixtures), '13°', '-4.5 °C', '100000000 kWh'];
      t.eq(await fuzz(t, 150, 21, async (r) => {
        const p = { label: r.pick(words), value: r.pick(words), text: r.pick(words), sub: r.pick(words), accent: r() < 0.5 };
        for (const [k, v] of Object.entries(opts)) p[k] = r.pick(v);
        const [w0, h0] = MIN(t); await at(t, p, r.int(w0, t.b.S - 2 * INSET[den(t)]), r.int(h0, t.b.S));
      }).then(() => []), [], 'fuzz builds');  // the shared fuzz checks page bodies: the card's own proof below
      const r2 = [], [w0, h0] = MIN(t);
      for (let i = 0; i < 40; i++) { await at(t, { label: fixtures.long, value: fixtures.emoji, accent: true, preview: 'partly-day' }, w0 + i * 9, h0 + i * 7); r2.push(...boundsAll(t).map((e) => `${w0 + i * 9}x${h0 + i * 7}: ${e}`)); }
      t.eq(r2.slice(0, 5), [], 'bounds over a sweep of parent sizes');
    } },
    { name: 'monkey: 200 random gestures (seed 9): no crash, no stuck animation, still taps', async run(t) {
      const c = await full(t, { preview: 'partly-day' });
      t.eq(monkey(t, 200, 9, (t) => (t.objs().length ? null : 'empty')), [], 'monkey');
      t.b.settle(3000); t.emitted(t.tap({ at: [c.x + c.w / 2, c.y + c.h / 2] }), 'tap');
    } },
    { name: 'leak: 150 build / clear cycles: objects back, heap flat', async run(t) {
      await full(t, {});
      t.eq(leak(t, 150, () => t.b.rebuild('card', { ...P, accent: true, preview: 'partly-day' }), () => t.b.rebuild('placeholder', {}), 48 * 1024), [], 'leak');
    } },
    { name: 'perf: a press repaints only the card (no full redraw), within its area', async run(t) {
      const c = await full(t, { preview: 'partly-day' });
      const s = frames(t, () => { t.b.touch(c.x + c.w / 2, c.y + c.h / 2, 1); t.b.tick(20); t.b.touch(c.x + c.w / 2, c.y + c.h / 2, 0); t.b.settle(800); });
      t.log.push(`perf ${den(t)}: ${s.n} frames, WASM ${s.ms50.toFixed(2)} ms p50 / ${s.ms95.toFixed(2)} p95, dirty px p50 ${s.px50}, max ${s.pxMax}, full ${s.full}`);
      t.eq(s.full, 0, 'no full redraw'); t.ok(s.pxMax <= c.w * c.h + 4 * (c.w + c.h), `dirty max ${s.pxMax} <= the card`);
    } },
    // ---------------------------------------------------------------- 10 product: Home
    { name: 'Home: a light card fills its Home cell (two-up: the Cells layout), taps open its screen', async run(t) {
      const E = 'light.test_lamp';
      t.ha.s[E] = { '': 'on', brightness: '179', friendly_name: 'Desk lamp', supported_color_modes: '["brightness"]' };
      await t.device({ v: 2, rev: 9, start: 'p', pages: [{ id: 'p', kind: 'home', name: 'Test', cards: [{ t: 'light', e: E, size: 'm', tap: 'detail', label: 'Desk lamp' }, { t: 'light', e: E, size: 'm', tap: 'detail', label: 'Lamp two' }] }] });
      const cs = t.comps().filter((c) => c.ids.includes('card')); t.eq(cs.length, 2, 'two cards');
      const half = (t.b.S - 2 * INSET[den(t)] - 16) / 2;  // two-up: (content - gutter) / 2 (Darren 2026-10-26)
      t.eq(cs.map((c) => c.w), [half, half], 'each = half the row'); t.eq(cs[0].y, cs[1].y, 'side by side');
      t.ok(cs[1].x >= cs[0].x + cs[0].w, 'no overlap');
      t.tap({ at: [cs[0].x + cs[0].w / 2, cs[0].y + cs[0].h / 2] }); t.eq(t.where(), 'light', 'opens the light screen');
    } },
  ],
};
