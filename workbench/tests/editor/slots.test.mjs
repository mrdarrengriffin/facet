// Slots (CONTRACT C8a) in the screen view: the header's end slot is outlined; its drawer adds an item from what the
// slot accepts (into the screen's spec, installed like a sent screen), the slot then holds it; Remove empties it again.
import { describe, it, after } from 'node:test';
import assert from 'node:assert/strict';
import { openEditor, close, settle } from './lib.mjs';

describe('Slots in the screen view', () => {
  after(close);
  it('adds a button to the header end slot and removes it', async () => {
    const E = await openEditor({ device: 'waveshare', query: 'screen=agents' }), { page } = E;
    const endSlot = page.locator('.sv-slots .slot[data-label^="end"]');
    await endSlot.waitFor();
    assert.equal(await endSlot.getAttribute('data-label'), 'end (empty)');
    await endSlot.click(); await settle(page);
    await page.locator('#ed-drawer .slot-add select').selectOption('icon_button');
    await page.locator('#ed-drawer .slot-add button').click(); await settle(page, 900);
    assert.equal(await page.locator('#ed-drawer .slot-item').count(), 1, 'one item in the slot');
    assert.equal(await page.locator('.sv-slots .slot[data-label^="end"]').getAttribute('data-label'), 'end', 'the slot holds it');
    await page.locator('#ed-drawer .slot-item button[aria-label="Remove"]').click(); await settle(page, 900);
    assert.equal(await page.locator('#ed-drawer .slot-item').count(), 0);
    assert.equal(await page.locator('.sv-slots .slot[data-label^="end"]').getAttribute('data-label'), 'end (empty)');
    assert.deepEqual(E.errors, []);
    await E.done();
  });
});
