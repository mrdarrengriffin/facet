// Editor v1, part 2 (workbench/EDITOR-V1.md): your own screen's grid on the live panel. The editing layer sits on the
// renderer's own card boxes (also after the panel scrolls); an empty square adds a card there without moving the
// others; a half takes another half under it; a card resizes, moves and goes; the deck check takes the result.
import { describe, it, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { launch, close, HUB, settle } from './lib.mjs';

const CARDS = [
  { t: 'light', e: 'light.demo_light_9', size: 'half' },
  { t: 'page', page: 'clock', size: '1x1' },
  { t: 'page', page: 'weather', size: '2x2' },
  { t: 'page', page: 'media', size: '2x1' },
  { t: 'page', page: 'calendar', size: '2x1' },
];

async function open(device) {
  const b = await launch();
  const ctx = await b.newContext({ viewport: { width: 1440, height: 1000 }, reducedMotion: 'reduce' });
  const page = await ctx.newPage();
  const errors = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  await page.goto(`${HUB}/wasm/editor-v1?device=${device}`);
  await page.waitForFunction(() => window.__v1?.engine(), null, { timeout: 60000 });
  await page.evaluate((cards) => { const v = window.__v1, d = v.draft(); const h = d.pages.find((x) => x.kind === 'home'); h.cards = cards; v.load(d); }, CARDS);
  await settle(page, 300);
  return { page, errors, done: () => ctx.close() };
}
const cards = (page) => page.evaluate(() => window.__v1.draft().pages.find((x) => x.kind === 'home').cards);
/** the renderer's card boxes and the editing layer's, in device px */
const boxes = (page) => page.evaluate(() => {
  const st = [...window.__v1.engine().views].find((s) => s.label === 'Live panel');
  const cr = st.canvas.getBoundingClientRect(), k = cr.width / st.w;
  const r = st.comps().filter((c) => c.d === 3 && (c.ids.includes('entity_tile') || c.ids.includes('card'))).map((c) => [c.x, c.y, c.w, c.h].join(',')).sort();
  const l = [...document.querySelectorAll('.v1-cell.card')].map((e) => { const b = e.getBoundingClientRect(); return [(b.left - cr.left) / k, (b.top - cr.top) / k, b.width / k, b.height / k].map(Math.round).join(','); }).sort();
  return { r, l };
});
const pick = async (page, q) => {
  await page.fill('.v1-side .v1-input', q); await settle(page, 100);
  await page.locator('.v1-cardlist .v1-opt').first().click(); await settle(page, 300);
};

for (const device of ['guition', 'waveshare'])
  describe(`Editor v1 grid (${device})`, () => {
    let E;
    before(async () => { E = await open(device); });
    after(async () => { assert.deepEqual(E.errors, []); await E.done(); await close(); });

    it('the editing layer sits on the renderer\'s card boxes, before and after a scroll', async () => {
      const a = await boxes(E.page);
      assert.equal(a.r.length, CARDS.length);
      assert.deepEqual(a.l, a.r);
      const c = await E.page.locator('.v1-clip').boundingBox();
      await E.page.mouse.move(c.x + c.width / 2, c.y + c.height / 2);
      for (let i = 0; i < 6; i++) { await E.page.mouse.wheel(0, 60); await settle(E.page, 30); }
      await settle(E.page, 1500);
      const b = await boxes(E.page);
      assert.notDeepEqual(b.r, a.r, 'the panel scrolled');
      assert.deepEqual(b.l, b.r);
      for (let i = 0; i < 12; i++) { await E.page.mouse.wheel(0, -60); await settle(E.page, 30); }
      await settle(E.page, 1500);
      assert.deepEqual((await boxes(E.page)).r, a.r, 'back at the top');
    });

    it('a half + under the half adds a second half there; nothing else moves', async () => {
      const was = (await boxes(E.page)).r;
      await E.page.locator('.v1-cell.add.half').click(); await settle(E.page, 200);
      await pick(E.page, 'sensor.');
      const cs = await cards(E.page);
      assert.equal(cs[1].t, 'sensor'); assert.equal(cs[1].size, 'half');
      const now = await boxes(E.page);
      assert.deepEqual(now.l, now.r);
      assert.equal(now.r.length, was.length + 1);
      for (const x of was) assert.ok(now.r.includes(x), `card at ${x} stayed`);
      assert.equal(await E.page.locator('.v1-sidetitle b').textContent(), 'Dishwasher Plug Power', 'the new card is picked');
    });

    it('an empty square adds a 1 x 1 there; the card resizes, then goes', async () => {
      const was = (await boxes(E.page)).r;
      const n = (await cards(E.page)).length;
      await E.page.locator('.v1-cell.add:not(.half)').first().click(); await settle(E.page, 200);
      await pick(E.page, 'Chair');
      assert.equal((await cards(E.page)).length, n + 1);
      const now = (await boxes(E.page)).r;
      for (const x of was) assert.ok(now.includes(x), `card at ${x} stayed`);
      await E.page.locator('.v1-side .seg button', { hasText: '2 × 1' }).click(); await settle(E.page, 300);
      const sel = (await cards(E.page)).find((c) => c.e === 'light.demo_light_9' && c.size === '2x1');
      assert.ok(sel, 'resized to 2 x 1');
      const b = await boxes(E.page); assert.deepEqual(b.l, b.r);
      await E.page.locator('.v1-side .btn', { hasText: 'Remove this card' }).click(); await settle(E.page, 300);
      assert.equal((await cards(E.page)).length, n);
    });

    it('the deck check takes it and Save says the changes in words', async () => {
      assert.deepEqual(await E.page.evaluate(() => window.__v1.check()), { ok: true, errors: [] });
      const ch = await E.page.evaluate(() => window.__v1.changes());
      assert.ok(ch.some((c) => c.what === 'Added a Sensor card'));
      await E.page.locator('#v1-save').click(); await settle(E.page, 200);
      await E.page.locator('.v1-acts .btn.primary', { hasText: 'Save' }).click(); await settle(E.page, 200);
      assert.equal((await E.page.evaluate(() => window.__v1.changes())).length, 0);
    });
  });
