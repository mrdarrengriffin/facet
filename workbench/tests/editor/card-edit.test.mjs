// Editing a Home card in place: click = its modal (isolated preview + settings), every change = the preview
// redrawn + one Undo step; Esc / backdrop / Close = deselect; the header actions; duplicate, remove, move.
import { describe, it, before, after, beforeEach, afterEach } from 'node:test';
import assert from 'node:assert/strict';
import { openEditor, close, fe, texts, settle, homeOf, lower, modalOpen, clickCard, cardPoint } from './lib.mjs';

const kitchen = async (page) => homeOf(await fe(page, 'deck'), 'kitchen').cards;
const drawn = async (page, label = 'Card preview') => lower(await texts(page, label));

describe('edit a Home card', () => {
  let E, page;
  beforeEach(async () => { E = await openEditor({ query: 'home=kitchen' }); page = E.page; });
  afterEach(async () => { assert.deepEqual(E.errors, [], 'no page errors'); assert.deepEqual(E.actions, [], 'nothing switched in the house'); await E.done(); });

  it('clicking a card opens THAT card, alone, in the modal', async () => {
    const cs = await kitchen(page);
    for (const i of [0, 1, 3]) {
      await clickCard(page, i);
      assert.equal(await modalOpen(page), true);
      assert.equal((await fe(page, 'flow')).modal, i);
      assert.equal(await page.locator('[data-testid="card-modal"] h2').innerText(), cs[i].label);
      const d = await drawn(page);
      assert.ok(d.includes(cs[i].label.toLowerCase()), `draws ${cs[i].label}`);
      for (const [j, c] of cs.entries()) if (j !== i && c.label && c.label !== cs[i].label && !cs[i].label.includes(c.label)) assert.ok(!d.includes(c.label.toLowerCase()), `no ${c.label}`);
      await page.keyboard.press('Escape');
      await settle(page);
    }
  });

  it('Esc, the backdrop and Close all deselect; no edit is made', async () => {
    const before = await fe(page, 'deck');
    for (const how of ['esc', 'backdrop', 'close']) {
      await clickCard(page, 0);
      assert.equal(await modalOpen(page), true);
      if (how === 'esc') await page.keyboard.press('Escape');
      else if (how === 'backdrop') await page.mouse.click(5, 5);
      else await page.locator('[data-testid="card-modal"] .cm-head button', { hasText: 'Close' }).click();
      await settle(page, 300);
      assert.equal(await modalOpen(page), false, how);
      const f = await fe(page, 'flow');
      assert.equal(f.drawer, null, how); assert.equal(f.card, -1, how); assert.equal(f.modal, -1, how);
    }
    assert.deepEqual(await fe(page, 'deck'), before);
    assert.equal(await fe(page, 'undo'), 0);
  });

  it('the header actions: click selects, click again deselects; a shortcut lands in the deck', async () => {
    const hp = await cardPoint(page, -1);
    await page.mouse.click(hp.x, hp.y); await settle(page, 300);
    assert.equal((await fe(page, 'flow')).drawer, 'home');
    assert.match(await page.locator('#ed-crumbs').innerText(), /Header actions/);
    await page.mouse.click(hp.x, hp.y); await settle(page, 300);
    assert.equal((await fe(page, 'flow')).drawer, null);
    await page.locator('#p-ha-button').selectOption('calendar');
    await settle(page);
    assert.equal(homeOf(await fe(page, 'deck'), 'kitchen').header_actions?.button, 'calendar');
  });

  it('label: typing redraws the card live and is ONE Undo step; empty = the entity name', async () => {
    await clickCard(page, 0);
    const inp = page.locator('[data-testid="card-modal"] #p-label');
    await inp.fill('');
    await inp.pressSequentially('Larder', { delay: 20 });
    await settle(page);
    assert.equal((await kitchen(page))[0].label, 'Larder');
    assert.ok((await drawn(page)).includes('larder'));
    assert.ok((await drawn(page, 'Preview')).includes('larder'), 'the Home behind redraws too');
    const u = await fe(page, 'undo');
    await page.keyboard.press('Escape'); await settle(page, 300);
    await page.locator('#ed-undo').click(); await settle(page);
    assert.equal(await fe(page, 'undo'), u - 1);
    assert.equal((await kitchen(page))[0].label, 'Pantry', 'one Undo = the whole typed label');
  });

  it('entity: a light of the house redraws the card; a wrong domain is refused', async () => {
    await clickCard(page, 0);
    const lbl = page.locator('[data-testid="card-modal"] #p-label');
    await lbl.fill(''); await settle(page);  // no label: the card shows its entity's name
    const ent = page.locator('#hc-entity');
    await ent.fill('light.sofa_table_light'); await ent.dispatchEvent('change'); await settle(page);
    assert.equal((await kitchen(page))[0].e, 'light.sofa_table_light');
    const name = E.src.options.light.find((o) => o.id === 'light.sofa_table_light').name;
    assert.ok((await drawn(page)).includes(name.toLowerCase()), `draws ${name}`);
    const before = await fe(page, 'deck');
    await ent.fill('sensor.kitchen_dishwasher_plug_power'); await ent.dispatchEvent('change'); await settle(page);
    assert.deepEqual(await fe(page, 'deck'), before, 'a sensor is not a light: nothing changes');
  });

  it('size, tap, duplicate, move and remove each change the deck and the drawing', async () => {
    let cs = await kitchen(page);
    const i = cs.findIndex((c) => c.t === 'light');
    await clickCard(page, i);
    const w0 = (await fe(page, 'boxes', 'Card preview')).find((b) => b.card === 0).w;
    const other = cs[i].size === 's' ? 'm' : 's';
    await page.locator(`[data-testid="card-modal"] .seg button[data-v="${other}"]`).click(); await settle(page, 900);
    assert.equal((await kitchen(page))[i].size, other);
    assert.notEqual((await fe(page, 'boxes', 'Card preview')).find((b) => b.card === 0).w, w0, 'the card is drawn at its new size');
    await page.locator('[data-testid="card-modal"] .seg button[data-v="detail"]').click(); await settle(page);
    assert.equal((await kitchen(page))[i].tap, 'detail');
    await page.locator('[data-testid="card-modal"] button', { hasText: 'Duplicate' }).click(); await settle(page);
    cs = await kitchen(page);
    assert.deepEqual(cs[i + 1], cs[i]);
    const n = cs.length, moving = cs[i];
    await page.locator('[data-testid="card-modal"] button', { hasText: 'Move later' }).click(); await settle(page);
    assert.equal((await fe(page, 'flow')).modal, i + 1, 'the modal follows the card');
    await page.keyboard.press('Alt+ArrowLeft'); await settle(page);
    assert.equal((await fe(page, 'flow')).modal, i);
    assert.deepEqual((await kitchen(page))[i], moving);
    await page.locator('[data-testid="card-modal"] button', { hasText: 'Remove' }).click(); await settle(page);
    assert.equal(await modalOpen(page), false, 'removing closes the modal');
    assert.equal((await kitchen(page)).length, n - 1);
  });

  it('remove drops the card from the Home preview; Undo / Redo walk it back and forth', async () => {
    const cs = await kitchen(page), i = cs.findIndex((c) => c.label === 'Fridge');
    const deck0 = await fe(page, 'deck');
    await clickCard(page, i);
    await page.locator('[data-testid="card-modal"] button', { hasText: 'Remove' }).click(); await settle(page);
    assert.ok(!(await drawn(page, 'Preview')).includes('fridge'), 'Fridge gone from the preview');
    const deck1 = await fe(page, 'deck');
    await page.keyboard.press('Control+z'); await settle(page);
    assert.deepEqual(await fe(page, 'deck'), deck0);
    assert.ok((await drawn(page, 'Preview')).includes('fridge'), 'back after Undo');
    assert.equal(await page.locator('#ed-redo').isEnabled(), true);
    await page.keyboard.press('Control+Shift+z'); await settle(page);
    assert.deepEqual(await fe(page, 'deck'), deck1);
    assert.ok(!(await drawn(page, 'Preview')).includes('fridge'), 'gone again after Redo');
    assert.match(await page.locator('#ed-dirty').innerText(), /^1 unsaved change$/i);
  });
});
after(close);
