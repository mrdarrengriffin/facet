// Pick-up drag: a Home card / an overview tile follows the pointer, the others make room, drop = the new
// order in the deck (and on the preview) as ONE Undo step; Esc mid-drag = everything back, nothing recorded.
import { describe, it, beforeEach, afterEach, after } from 'node:test';
import assert from 'node:assert/strict';
import { openEditor, close, fe, texts, settle, homeOf, ids, modalOpen, dragCard, dragTile } from './lib.mjs';

const kitchen = async (page) => homeOf(await fe(page, 'deck'), 'kitchen').cards;
const labelsDrawn = async (page, labels) => { const L = labels.map((x) => x.toLowerCase()); return (await texts(page, 'Preview')).map((t) => t.toLowerCase()).filter((t) => L.includes(t)); };

describe('drag to reorder', () => {
  let E, page;
  afterEach(async () => { assert.deepEqual(E.errors, []); await E.done(); });

  describe('Home cards', () => {
    beforeEach(async () => { E = await openEditor({ query: 'home=kitchen' }); page = E.page; });

    it('drop moves the card: deck, preview order and one Undo step', async () => {
      const c0 = await kitchen(page), labels = c0.map((c) => c.label).filter(Boolean);
      await dragCard(page, 0, 2);
      const c1 = await kitchen(page);
      assert.deepEqual(c1, [c0[1], c0[2], c0[0], ...c0.slice(3)], 'card 1 now sits third');
      assert.equal(await modalOpen(page), false, 'a drop is not a click');
      assert.deepEqual(await labelsDrawn(page, labels), c1.map((c) => c.label).filter(Boolean).map((x) => x.toLowerCase()), 'the preview draws the new order');
      assert.equal(await fe(page, 'undo'), 1, 'one Undo step');
      await page.keyboard.press('Control+z'); await settle(page);
      assert.deepEqual(await kitchen(page), c0);
      assert.deepEqual(await labelsDrawn(page, labels), labels.map((x) => x.toLowerCase()));
    });

    it('Esc mid-drag puts everything back and records nothing', async () => {
      const c0 = await kitchen(page);
      await dragCard(page, 0, 3, { esc: true });
      assert.deepEqual(await kitchen(page), c0);
      assert.equal(await fe(page, 'undo'), 0);
      assert.equal(await modalOpen(page), false);
      assert.equal(await page.locator('.ed-lift').count(), 0, 'no floating copy left behind');
    });
  });

  describe('overview tiles', () => {
    beforeEach(async () => { E = await openEditor(); page = E.page; });

    it('screen tiles: drop = the deck order, persists across views, one Undo step', async () => {
      const d0 = await fe(page, 'deck'), s0 = d0.pages.filter((p) => p.kind !== 'home').map((p) => p.id);
      await dragTile(page, 'screen-tile', s0[0], s0[2]);
      const s1 = (await fe(page, 'deck')).pages.filter((p) => p.kind !== 'home').map((p) => p.id);
      assert.deepEqual(s1, [s0[1], s0[2], s0[0], ...s0.slice(3)]);
      assert.deepEqual(await page.locator('[data-testid="screen-tile"]').evaluateAll((ts) => ts.map((t) => t.dataset.id)), s1, 'tiles in deck order');
      // into a screen and back: the order holds
      await page.locator(`[data-testid="screen-tile"][data-id="${s1[0]}"] .shot`).click(); await settle(page);
      await page.locator('#ed-crumbs button', { hasText: 'Panel' }).click(); await settle(page);
      assert.deepEqual(await page.locator('[data-testid="screen-tile"]').evaluateAll((ts) => ts.map((t) => t.dataset.id)), s1);
      assert.equal(await fe(page, 'undo'), 1);
      await page.keyboard.press('Control+z'); await settle(page);
      assert.deepEqual((await fe(page, 'deck')).pages.filter((p) => p.kind !== 'home').map((p) => p.id), s0);
    });

    it('screen tiles: Esc mid-drag = the old order, nothing recorded', async () => {
      const s0 = (await fe(page, 'deck')).pages.filter((p) => p.kind !== 'home').map((p) => p.id);
      await dragTile(page, 'screen-tile', s0[0], s0[3], { esc: true });
      assert.deepEqual((await fe(page, 'deck')).pages.filter((p) => p.kind !== 'home').map((p) => p.id), s0);
      assert.deepEqual(await page.locator('[data-testid="screen-tile"]').evaluateAll((ts) => ts.map((t) => t.dataset.id)), s0, 'tiles back in place');
      assert.equal(await fe(page, 'undo'), 0);
    });

    it('Homes: drop swaps them in the deck and counts as a change to save', async () => {
      const h0 = ids((await fe(page, 'deck')).pages.filter((p) => p.kind === 'home'));
      await dragTile(page, 'home-tile', h0[0], h0[1]);
      const h1 = ids((await fe(page, 'deck')).pages.filter((p) => p.kind === 'home'));
      assert.deepEqual(h1, [h0[1], h0[0], ...h0.slice(2)]);
      assert.equal(await page.locator('#ed-save').isEnabled(), true, 'Save is on after a Homes reorder');
      assert.equal(await fe(page, 'undo'), 1);
    });
  });
});
after(close);
