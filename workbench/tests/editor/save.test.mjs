// Save / load: what the Save dialog sends is exactly the deck the editor shows (rev + 1); a deck sent and loaded back
// edits and draws the same; a deck the engine refuses is said so, not silently drawn as something else.
import { describe, it, after } from 'node:test';
import assert from 'node:assert/strict';
import { openEditor, close, fe, texts, settle, homeOf, fixture, clickCard } from './lib.mjs';

describe('save and load', () => {
  it('Save sends what the editor shows; after saving nothing is unsaved', async () => {
    const E = await openEditor({ query: 'home=kitchen' }), { page } = E;
    assert.equal(await page.locator('#ed-save').isDisabled(), true, 'nothing to save yet');
    await page.locator('[data-testid="add-card"] button[data-v="cover"]').click(); await settle(page);
    await page.keyboard.press('Escape'); await settle(page, 300);
    const shown = await fe(page, 'deck');
    await page.locator('#ed-save').click(); await settle(page, 300);
    assert.match(await page.locator('#ed-dialog h2').innerText(), /^1 change for/);
    const sent = JSON.parse(await page.locator('#ed-dialog pre').textContent());
    assert.deepEqual(sent, { ...shown, rev: shown.rev + 1 });
    await page.locator('#ed-dialog .btn.primary').click(); await settle(page);
    assert.deepEqual(await fe(page, 'saved'), sent);
    assert.equal(await page.locator('#ed-dirty').isHidden(), true);
    assert.equal(await page.locator('#ed-save').isDisabled(), true);
    assert.deepEqual(E.errors, []);
    await E.done();
  });

  it('a saved deck loads back the same and draws the same Homes', async () => {
    const A = await openEditor({ query: 'home=kitchen' });
    await A.page.locator('[data-testid="add-card"] button[data-v="media"]').click(); await settle(A.page);
    await A.page.keyboard.press('Escape'); await settle(A.page);
    const deck = { ...(await fe(A.page, 'deck')), rev: 9 };
    const drawnA = await texts(A.page, 'Preview');
    await A.done();
    const src = fixture(); src.deck = deck;
    const B = await openEditor({ query: 'home=kitchen', src });
    assert.deepEqual(await fe(B.page, 'deck'), deck);
    assert.deepEqual((await texts(B.page, 'Preview')).filter((t) => !/^\d\d:\d\d$/.test(t)), drawnA.filter((t) => !/^\d\d:\d\d$/.test(t)));
    assert.equal(await B.page.locator('#ed-save').isDisabled(), true, 'a fresh load is not unsaved');
    await B.done();
  });

  it('a deck the engine refuses is said so, and fixing it clears the message', async () => {
    const src = fixture();
    homeOf(src.deck, 'kitchen').cards.unshift({ t: 'page', page: 'nowhere', size: 'm' });
    const E = await openEditor({ query: 'home=kitchen', src }), { page } = E;
    const bar = page.locator('.sv-bar .ed-meta');
    assert.match(await bar.innerText(), /deck refused/i);
    assert.notEqual((await fe(page, 'flow')).err, '');
    await page.locator('#ed-drawer button', { hasText: 'nowhere' }).first().click().catch(() => clickCard(page, 0));
    await settle(page);
    await page.locator('[data-testid="card-modal"] button', { hasText: 'Remove' }).click(); await settle(page);
    assert.equal((await fe(page, 'flow')).err, '');
    assert.doesNotMatch(await bar.innerText(), /deck refused/i);
    await E.done();
  });
});
after(close);
