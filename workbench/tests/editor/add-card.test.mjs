// Part 1 regression: adding a card showed the WRONG card. The card modal's scratch page id was longer than
// the engine allows, the whole deck was refused, and the modal rendered the start Home instead. Per card type, on both
// panels: the modal's isolated preview, the Home preview and the overview thumbnail all draw THAT card.
import { describe, it, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { openEditor, close, fe, texts, settle, homeOf, expectedTitle, lower, modalOpen } from './lib.mjs';

for (const device of ['guition', 'waveshare']) {
  describe(`add a card (${device})`, () => {
    let E, page, types;
    before(async () => {
      E = await openEditor({ device, query: 'home=kitchen' });
      page = E.page;
      types = await page.locator('[data-testid="add-card"] button').evaluateAll((bs) => bs.map((b) => b.dataset.v));
    });
    after(async () => { await E?.done(); });

    it('offers every card type the engine knows', () => {
      assert.deepEqual([...types].sort(), ['light', 'media', 'page', 'sensor']);
    });

    for (const t of ['light', 'media', 'sensor', 'page']) {
      it(`${t}: the modal, the Home preview and the deck show the card just added`, async () => {
        const before = homeOf(await fe(page, 'deck'), 'kitchen').cards;
        const others = new Set(lower((await texts(page, 'Preview')) ?? []));
        await page.locator(`[data-testid="add-card"] button[data-v="${t}"]`).click();
        await settle(page, 900);
        const cards = homeOf(await fe(page, 'deck'), 'kitchen').cards, card = cards.at(-1);
        assert.equal(cards.length, before.length + 1, 'one card more');
        assert.equal(card.t, t);
        const f = await fe(page, 'flow');
        assert.equal(f.err, '', 'the engine took the deck');
        assert.equal(f.modal, cards.length - 1, 'the modal edits the new card');
        const title = (await page.locator('[data-testid="card-modal"] h2').innerText()).trim();
        const want = expectedTitle(card, E.src);
        if (want) assert.equal(title, want);
        const drawn = lower(await texts(page, 'Card preview'));
        assert.ok(drawn.includes(title.toLowerCase()), `card preview draws "${title}" (drew ${drawn.join(' | ')})`);
        // only this card: nothing from the Home's other cards, nor the start Home (the old bug drew the Living room)
        for (const x of ['pantry', 'ceiling', 'sofa table', 'fridge']) assert.ok(!drawn.includes(x), `card preview has no "${x}"`);
        assert.match(await page.locator('[data-testid="card-modal"] .cap').innerText(), new RegExp(`card ${cards.length} of ${cards.length}`, 'i'));
        await page.keyboard.press('Escape');
        await settle(page);
        assert.equal(await modalOpen(page), false);
        const main = lower(await texts(page, 'Preview'));
        assert.ok(main.filter((x) => x === title.toLowerCase()).length > [...others].filter((x) => x === title.toLowerCase()).length || main.includes(title.toLowerCase()), `Home preview draws "${title}"`);
      });
    }

    it('one Undo takes the added card back out (preview too)', async () => {
      const n = homeOf(await fe(page, 'deck'), 'kitchen').cards.length;
      await page.locator('[data-testid="add-card"] button[data-v="light"]').click();
      await settle(page);
      await page.keyboard.press('Escape');
      const cards = homeOf(await fe(page, 'deck'), 'kitchen').cards, title = expectedTitle(cards.at(-1), E.src).toLowerCase();
      assert.equal(cards.length, n + 1);
      const before = lower(await texts(page, 'Preview')).filter((x) => x === title).length;
      await page.keyboard.press('Control+z');
      await settle(page);
      assert.equal(homeOf(await fe(page, 'deck'), 'kitchen').cards.length, n);
      assert.equal(lower(await texts(page, 'Preview')).filter((x) => x === title).length, before - 1);
    });

    it('the overview thumbnail of the Home shows the added cards', async () => {
      await page.locator('#ed-crumbs button', { hasText: 'Panel' }).click();
      await settle(page);
      const th = lower(await texts(page, 'Thumb home kitchen'));
      const last = homeOf(await fe(page, 'deck'), 'kitchen').cards.at(-1);
      assert.ok(th.includes(expectedTitle(last, E.src).toLowerCase()), 'thumbnail draws the last added card');
      assert.match(await page.locator('[data-testid="home-tile"][data-id="kitchen"] .sub').innerText(), new RegExp(`^${homeOf(await fe(page, 'deck'), 'kitchen').cards.length} cards`));
    });
  });
}
after(close);
