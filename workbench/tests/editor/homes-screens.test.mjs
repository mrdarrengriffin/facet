// The overview (Panel): Homes added / renamed / made the start page / removed, screens added from the gallery and
// switched off; each lands in the deck the panel would get, as one Undo step.
import { describe, it, beforeEach, afterEach, after } from 'node:test';
import assert from 'node:assert/strict';
import { openEditor, close, fe, texts, settle, homeOf, ids, lower } from './lib.mjs';

const homes = async (page) => (await fe(page, 'deck')).pages.filter((p) => p.kind === 'home');
const toPanel = async (page) => { await page.locator('#ed-crumbs button', { hasText: 'Panel' }).click(); await settle(page); };

describe('Homes and screens in the overview', () => {
  let E, page;
  beforeEach(async () => { E = await openEditor(); page = E.page; });
  afterEach(async () => { assert.deepEqual(E.errors, []); await E.done(); });

  it('Add Home: a new empty Home opens, takes cards, and shows in the overview', async () => {
    const h0 = await homes(page);
    await page.locator('[data-testid="add-home"]').click(); await settle(page);
    const h1 = await homes(page), nu = h1.at(-1);
    assert.equal(h1.length, h0.length + 1);
    assert.deepEqual(nu.cards, []);
    assert.match(nu.id, /^[a-z0-9_]{1,12}$/, 'an id the engine accepts');
    const f = await fe(page, 'flow');
    assert.equal(f.step, 'home'); assert.equal(f.sel, nu.id); assert.equal(f.err, '');
    assert.ok(lower(await texts(page, 'Preview')).includes(nu.name.toLowerCase()), 'the preview draws the new Home');
    await page.locator('[data-testid="add-card"] button[data-v="light"]').click(); await settle(page);
    await page.keyboard.press('Escape'); await settle(page);
    assert.equal(homeOf(await fe(page, 'deck'), nu.id).cards.length, 1);
    await toPanel(page);
    assert.deepEqual(await page.locator('[data-testid="home-tile"]').evaluateAll((t) => t.map((x) => x.dataset.id)), ids(h1));
    assert.equal(await fe(page, 'undo'), 2, 'add Home + add card = two steps');
  });

  it('rename, make start page, remove a Home (its page cards go with it), one Undo restores', async () => {
    await page.locator('[data-testid="home-tile"][data-id="kitchen"] .shot').click(); await settle(page);
    const name = page.locator('[data-testid="home-settings"] #p-home-name');
    await name.fill('Cook room'); await settle(page);
    assert.equal(homeOf(await fe(page, 'deck'), 'kitchen').name, 'Cook room');
    assert.ok(lower(await texts(page, 'Preview')).includes('cook room'), 'the Home redraws with its new name');
    await page.locator('[data-testid="home-start"]').click(); await settle(page);
    assert.equal((await fe(page, 'deck')).start, 'kitchen');
    assert.equal(await page.locator('[data-testid="home-start"]').isDisabled(), true);
    const before = await fe(page, 'deck');
    assert.ok(homeOf(before, 'living').cards.some((c) => c.page === 'kitchen'), 'fixture: Living room has a Kitchen card');
    await page.locator('[data-testid="home-remove"]').click(); await settle(page);
    const d = await fe(page, 'deck');
    assert.equal(homeOf(d, 'kitchen'), undefined);
    assert.equal(d.start, 'living', 'the start page moves to a Home that is left');
    assert.ok(!homeOf(d, 'living').cards.some((c) => c.page === 'kitchen'), 'no card opens a removed Home');
    assert.equal((await fe(page, 'flow')).step, 'panel');
    assert.equal((await fe(page, 'flow')).err, '', 'the engine took the deck');
    await page.keyboard.press('Control+z'); await settle(page);
    assert.deepEqual(await fe(page, 'deck'), before);
  });

  it('the last Home cannot be removed', async () => {
    await page.locator('[data-testid="home-tile"][data-id="kitchen"] .shot').click(); await settle(page);
    await page.locator('[data-testid="home-remove"]').click(); await settle(page);
    await page.locator('[data-testid="home-tile"][data-id="living"] .shot').click(); await settle(page);
    assert.equal(await page.locator('[data-testid="home-remove"]').isDisabled(), true);
  });

  it('Add screen from the gallery, then switch it off: the deck the panel gets follows', async () => {
    const s0 = (await fe(page, 'deck')).pages.map((p) => p.id);
    await page.locator('[data-testid="add-screen"]').click(); await settle(page);
    const pick = page.locator('#ed-dialog .gal .tile').first();
    const nm = (await pick.locator('.nm').innerText()).trim();
    await pick.click(); await settle(page);
    const d1 = (await fe(page, 'deck')).pages.map((p) => p.id), added = d1.find((x) => !s0.includes(x));
    assert.ok(added, `${nm} added`);
    const tile = page.locator(`[data-testid="screen-tile"][data-id="${added}"]`);
    assert.equal(await tile.locator('.nm').innerText(), nm);
    await tile.locator('[role="switch"]').click(); await settle(page);
    assert.ok(!(await fe(page, 'deck')).pages.some((p) => p.id === added), 'off = left out of the deck sent');
    assert.match(await tile.getAttribute('class'), /\boff\b/, 'the tile still lists it, marked off');
  });
});
after(close);
