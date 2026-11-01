// Editor lab, the new editor's Home cards: click a card on the glass = it is selected
// (outlined) and its settings open in the side panel (a bottom sheet on a phone) while the whole Home stays visible and
// live; every change (entity, tap, label, size, duplicate, remove, add) is staged and kept only on Save, Cancel drops
// it. The entity picker opens like a dropdown. Ported from the old editor's card modal (card-edit.test.mjs = that one).
import { describe, it, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { launch, close, fixture, HUB, settle } from './lib.mjs';

const lab = (page, f, ...a) => page.evaluate(([f, a]) => window.lab[f](...a), [f, a]);
const lower = (xs) => (xs ?? []).map((x) => String(x).toLowerCase());
const drawn = async (page) => lower(await lab(page, 'texts'));

async function openLab({ width = 1440, height = 1000 } = {}) {
  const b = await launch();
  const ctx = await b.newContext({ ignoreHTTPSErrors: true, viewport: { width, height }, reducedMotion: 'reduce' });
  const page = await ctx.newPage();
  const errors = [], actions = [], src = fixture();
  const nm = Object.fromEntries(Object.values(src.options).flat().map((o) => [o.id, o.name]));
  page.on('pageerror', (e) => errors.push(String(e)));
  page.on('dialog', (d) => d.accept());
  await page.route('**/api/wasm/sources', (r) => r.fulfill({ json: src }));
  await page.route('**/api/panel/entities**', (r) => {
    const ids = (new URL(r.request().url()).searchParams.get('ids') ?? '').split(',').filter(Boolean);
    r.fulfill({ json: ids.map((id) => ({ id, name: nm[id] ?? id, state: 'on', attrs: {} })) });
  });
  await page.route('**/api/panel/action', (r) => { actions.push(r.request().postData()); r.fulfill({ json: { ok: true } }); });
  await page.route('**/api/wasm/spec', (r) => (r.request().method() === 'POST' ? r.fulfill({ json: { error: 'tests never send' } }) : r.continue()));
  await page.goto(`${HUB}/wasm/editor-lab`);
  await page.waitForFunction(() => window.lab?.boxes().length > 0, null, { timeout: 60000 });
  await settle(page);
  return { page, ctx, errors, actions, src, async done() { await ctx.close(); } };
}

for (const [view, width, height] of [['desktop', 1440, 1000], ['phone', 390, 844]]) {
  describe(`editor lab: Home card settings (${view})`, () => {
    let L, page;
    before(async () => { L = await openLab({ width, height }); page = L.page; });
    after(async () => { await L?.done(); });

    it('click a card = selected on the glass, its settings in the side panel, no modal', async () => {
      assert.equal(await page.locator('#ov .ov-card').count(), (await lab(page, 'cards')).length);
      await page.locator('#ov .ov-card[data-card="0"]').click();
      await settle(page, 300);
      assert.equal(await lab(page, 'selected'), 0);
      assert.ok(await page.locator('#ov .ov-card.on[data-card="0"]').isVisible(), 'outlined on the glass');
      assert.ok(await page.locator('#side [data-testid="card-state"]').isVisible());
      assert.equal(await page.locator('dialog[open]').count(), 0, 'no modal');
      assert.ok(await page.locator('#side .cs-save').isDisabled(), 'nothing to save yet');
    });

    it('an edit is staged: drawn live, not kept until Save', async () => {
      const kept = (await lab(page, 'cards'))[0];
      await page.locator('#side input[aria-label="Label"]').fill('Zebra lamp');
      await settle(page, 300);
      assert.ok((await drawn(page)).some((t) => t.includes('zebra lamp')), 'the Home preview shows it');
      assert.equal((await lab(page, 'cards'))[0].label, kept.label, 'not kept yet');
      assert.ok(await lab(page, 'dirty'));
      assert.match(await page.locator('#side [data-testid="card-state"]').textContent(), /Unsaved/);
      assert.ok(await page.locator('#side .cs-save').isEnabled());
      const other = (await page.locator('#side [data-testid="size"] .chip').evaluateAll((bs) => bs.map((b) => b.dataset.v))).find((v) => v !== kept.size);
      if (other) await page.locator(`#side [data-testid="size"] .chip[data-v="${other}"]`).click();
      await page.locator('#side .cs-save').click();
      await settle(page, 300);
      const now = (await lab(page, 'cards'))[0];
      assert.equal(now.label, 'Zebra lamp');
      if (other) assert.equal(now.size, other);
      assert.equal(await lab(page, 'selected'), -1, 'Save closes the settings');
      assert.ok(await page.locator('#side [data-testid="saved"]').isVisible(), 'says it is saved');
    });

    it('Cancel drops the change', async () => {
      await page.locator('#ov .ov-card[data-card="0"]').click();
      await page.locator('#side input[aria-label="Label"]').fill('Not kept');
      await settle(page, 200);
      await page.locator('#side .cs-cancel').click();
      await settle(page, 300);
      assert.equal((await lab(page, 'cards'))[0].label, 'Zebra lamp');
      assert.ok(!(await drawn(page)).some((t) => t.includes('not kept')), 'the preview is back');
    });

    it('add a light is staged; the entity picker opens like a dropdown, picking closes it', async () => {
      const n = (await lab(page, 'cards')).length;
      await page.locator('#side select').last().selectOption('type:light');
      await settle(page, 300);
      assert.equal((await lab(page, 'cards')).length, n, 'not kept yet');
      assert.equal((await lab(page, 'draft')).length, n + 1);
      assert.equal(await lab(page, 'selected'), n);
      const ep = page.locator('#side [data-testid="entity-picker"]');
      assert.equal(await ep.locator('input').count(), 0, 'closed: only the current entity');
      await ep.locator('.ep-cur').click();
      const q = ep.locator('input');
      await q.fill('pantry');
      const ids = await ep.locator('.ep-list button').evaluateAll((bs) => bs.map((b) => b.dataset.id));
      assert.ok(ids.length > 0 && ids.every((id) => /^(light|switch)\./.test(id) && id.includes('pantry')), `filtered: ${ids}`);
      await q.fill('binary_sensor.back_door_opening');
      assert.equal(await ep.locator('.ep-list button').count(), 0, 'a wrong domain is never offered');
      await q.fill('pantry');
      await q.press('Enter');
      await settle(page, 300);
      assert.equal((await lab(page, 'draft'))[n].e, ids[0]);
      assert.equal(await ep.locator('input').count(), 0, 'closed again');
      assert.equal(await ep.locator('.ep-cur .id').textContent(), ids[0]);
      await page.locator('#side [data-testid="tap"] .chip[data-v="detail"]').click();
      await page.locator('#side .cs-save').click();
      await settle(page, 300);
      const c = (await lab(page, 'cards'))[n];
      assert.deepEqual([c.t, c.e, c.tap], ['light', ids[0], 'detail']);
    });

    it('duplicate and remove are staged too', async () => {
      const n = (await lab(page, 'cards')).length;
      await page.locator(`#ov .ov-card[data-card="${n - 1}"]`).click();
      await page.locator('#side .cm-acts button', { hasText: 'Duplicate' }).click();
      assert.equal((await lab(page, 'draft')).length, n + 1);
      await page.locator('#side .cm-acts button', { hasText: 'Remove' }).click();
      await page.locator(`#ov .ov-card[data-card="0"]`).click();
      await page.locator('#side .cm-acts button', { hasText: 'Remove' }).click();
      assert.equal((await lab(page, 'draft')).length, n - 1);
      assert.equal((await lab(page, 'cards')).length, n, 'not kept yet');
      await page.locator('#side .cs-cancel').click();
      assert.equal((await lab(page, 'cards')).length, n);
    });

    it('step 5: one Entity tile for any entity; its taps follow the domain (like HA tile card); no Ask first', async () => {
      const n = (await lab(page, 'cards')).length;
      await page.locator('#side select').last().selectOption('type:entity');
      await settle(page, 300);
      const ep = page.locator('#side [data-testid="entity-picker"]');
      const taps = async () => page.locator('#side [data-testid="tap"] .chip').evaluateAll((bs) => bs.map((b) => b.dataset.v));
      const pickE = async (id) => { await ep.locator('.ep-cur').click(); await ep.locator('input').fill(id); await ep.locator(`.ep-list button[data-id="${id}"]`).click(); await settle(page, 200); };
      await ep.locator('.ep-cur').click();
      const doms = new Set(await ep.locator('.ep-list button').evaluateAll((bs) => bs.map((b) => b.dataset.id.split('.')[0])));
      assert.ok(doms.size >= 5, `any domain: ${[...doms]}`);
      await ep.locator('.ep-cur').click();
      await pickE('light.pantry_light');
      assert.deepEqual(await taps(), ['toggle', 'detail', 'none'], 'a light: toggle (default), its screen, nothing');
      assert.equal((await lab(page, 'draft'))[n].tap, 'toggle');
      await pickE('automation.lights_out');
      assert.deepEqual(await taps(), ['toggle', 'run', 'none'], 'an automation: on / off or trigger');
      await pickE('sensor.kitchen_dishwasher_plug_power');
      assert.equal((await lab(page, 'draft'))[n].tap, 'none', 'a sensor: nothing (no generic screen)');
      await pickE('script.good_night');
      assert.deepEqual(await taps(), ['run', 'none']);
      assert.equal(await page.locator('#side [data-testid="ask"]').count(), 0, 'no Ask first (removed)');
      await page.locator('#side .cs-save').click();
      await settle(page, 400);
      const c = (await lab(page, 'cards'))[n];
      assert.deepEqual([c.t, c.e, c.tap, c.ask, c.size], ['entity', 'script.good_night', 'run', undefined, 's']);
      assert.ok((await drawn(page)).includes('run'), 'the tile says Run');
    });

    it('step 4: the header end slot offers square icon buttons (no segmented), no debug props, and a tap action', async () => {
      await page.locator('#screen').selectOption('new');
      await settle(page, 600);
      await page.locator('#ov .ov-slot[title="Add to end"]').click();
      const offered = await page.locator('#side .mini .nm').allTextContents();
      assert.ok(offered.includes('Icon button'), `offered: ${offered}`);
      assert.ok(!offered.some((x) => /segmented/i.test(x)), 'no segmented control');
      await page.locator('#side .mini', { hasText: 'Icon button' }).first().click();
      await settle(page, 400);
      const node = (await page.evaluate(() => window.lab.spec())).nodes.at(-1);
      assert.equal(node.c, 'icon_button');
      assert.equal(node.p.style, 'square', 'pinned to square');
      const labels = await page.locator('#side .prop .pl').allTextContents();
      assert.ok(!labels.includes('State'), `no debug State prop: ${labels}`);
      const style = await page.locator('#side .prop', { has: page.locator('.pl', { hasText: /^Style$/ }) }).locator('.chip').allTextContents();
      assert.deepEqual(style, ['Square']);
      await page.locator('#side [data-testid="on-tap"] select').selectOption('scene.activate');
      await settle(page, 300);
      const tap = (await page.evaluate(() => window.lab.spec())).nodes.at(-1).on?.tap;
      assert.equal(tap?.do, 'scene.activate');
      assert.match(tap?.e ?? '', /^scene\./);
      assert.equal(await page.locator('#side [data-testid="ask"]').count(), 0, 'no Ask first');
    });

    it('a custom screen = a root Column: remove every part, its top + foot still take parts; the Column has no Remove', async () => {
      const spec = () => page.evaluate(() => window.lab.spec());
      const nm = (id) => page.evaluate((id) => window.lab.engine.comps.find((c) => c.id === id)?.name ?? id, id);
      assert.deepEqual([(await spec()).nodes[0].c, (await spec()).nodes[0].in], ['column', undefined], 'the root is a Column');
      for (let guard = 0; guard < 10; guard++) {
        const n = (await spec()).nodes.find((x) => x.in === 0);
        if (!n) break;
        await page.locator(`#ov .ov-node[title="${await nm(n.c)}"]`).first().click();
        await settle(page, 200);
        await page.locator('#side button.danger', { hasText: 'Remove' }).click();
        await settle(page, 300);
      }
      assert.equal((await spec()).nodes.filter((x) => x.in !== 'header').length, 1, 'only the Column is left');
      await page.locator(`#ov .ov-node[title="${await nm('column')}"]`).click({ position: { x: 20, y: 140 } });
      await settle(page, 200);
      assert.equal(await page.locator('#side button.danger', { hasText: 'Remove' }).count(), 0, 'no Remove on the root Column');
      for (const t of ['top', 'foot']) assert.ok((await page.locator(`#ov .ov-slot[title="Add to ${t}"]`).boundingBox()).height > 16, `a visible + for the empty ${t}`);
      await page.locator('#ov .ov-slot[title="Add to foot"]').click();
      await settle(page, 200);
      const foot = await page.locator('#side .group').first().locator('.mini .nm').allTextContents();
      assert.ok(foot.includes(await nm('button')), `foot suggests buttons: ${foot}`);
      await page.locator('#ov .ov-slot[title="Add to top"]').click();
      await settle(page, 200);
      const top = await page.locator('#side .group').first().locator('.mini .nm').allTextContents();
      assert.ok(top.includes(await nm('cells')), `top suggests Cells: ${top}`);
      await page.locator('#side .group').first().locator('.mini', { hasText: await nm('cells') }).click();
      await settle(page, 400);
      const last = (await spec()).nodes.at(-1);
      assert.deepEqual([last.c, last.in, last.slot], ['cells', 0, 0], 'Cells back in the top slot');
      assert.ok((await page.locator('#ov .ov-slot[title="Add to cell"]').count()) > 0, 'its cells are slots again');
      assert.ok(!(await drawn(page)).includes('top'), 'no workbench outline labels on the screen');
    });

    it('a locked screen (every factory one, spec "locked") shows no slots or parts to edit', async () => {
      await page.locator('#screen').selectOption('built:vacuum');
      await settle(page, 600);
      assert.equal((await page.evaluate(() => window.lab.spec())).locked, true);
      assert.equal(await page.locator('#ov .ov-slot, #ov .ov-node').count(), 0);
    });

    it('nothing reached Home Assistant and no page errors', () => {
      assert.deepEqual(L.actions, []);
      assert.deepEqual(L.errors, []);
    });
  });
}

// pick-up drag: drag a Home card onto another = the new order is staged (Save / Cancel), a tap still opens
for (const [view, width, height] of [['desktop', 1440, 1000], ['phone', 390, 844]]) {
  describe(`editor lab: drag to reorder Home cards (${view})`, () => {
    let L, page;
    before(async () => { L = await openLab({ width, height }); page = L.page; });
    after(async () => { await L?.done(); });

    it('reorder moves one item, the others shift', async () => {
      assert.deepEqual(await lab(page, 'reorder', ['a', 'b', 'c', 'd'], 0, 2), ['b', 'c', 'a', 'd']);
      assert.deepEqual(await lab(page, 'reorder', ['a', 'b', 'c', 'd'], 3, 1), ['a', 'd', 'b', 'c']);
    });

    it('drag card 1 onto card 2 = swapped in the draft, not kept, Cancel puts it back', async () => {
      const kept = await lab(page, 'cards');
      const box = async (i) => { const b = await page.locator(`#ov .ov-card[data-card="${i}"]`).boundingBox(); return [b.x + b.width / 2, b.y + b.height / 2]; };
      const [ax, ay] = await box(0), [bx, by] = await box(1);
      await page.mouse.move(ax, ay);
      await page.mouse.down();
      await page.mouse.move(ax + 10, ay + 4, { steps: 2 });
      assert.equal(await page.locator('.ed-lift').count(), 1, 'lifted');
      await page.mouse.move(bx, by, { steps: 8 });
      await page.mouse.up();
      await settle(page, 400);
      assert.equal(await page.locator('.ed-lift, .ed-lay').count(), 0, 'landed');
      assert.deepEqual(await lab(page, 'draft'), [kept[1], kept[0], ...kept.slice(2)]);
      assert.deepEqual(await lab(page, 'cards'), kept, 'not kept until Save');
      assert.equal(await lab(page, 'selected'), 1, 'the moved card stays picked');
      assert.match(await page.locator('#side [data-testid="card-state"]').textContent(), /Unsaved/);
      await page.locator('#side .cs-cancel').click();
      await settle(page, 300);
      assert.equal(await lab(page, 'draft'), null);
      assert.deepEqual(await lab(page, 'cards'), kept);
    });

    it('a tap without a drag still opens the card', async () => {
      await page.locator('#ov .ov-card[data-card="1"]').click();
      await settle(page, 300);
      assert.equal(await lab(page, 'selected'), 1);
      assert.equal(await lab(page, 'dirty'), false);
      assert.deepEqual(L.actions, []);
      assert.deepEqual(L.errors, []);
    });
  });
}

// header actions per Home: tap the header = its end buttons in the side panel; each opens a screen (its icon);
// add / reorder / remove staged (Save / Cancel); how many = the Header's end slot max less its own Settings button
for (const [view, width, height] of [['desktop', 1440, 1000], ['phone', 390, 844]]) {
  describe(`editor lab: header buttons per Home (${view})`, () => {
    let L, page;
    before(async () => { L = await openLab({ width, height }); page = L.page; });
    after(async () => { await L?.done(); });
    const rows = () => page.locator('#side [data-testid="head-buttons"] .hb-row');

    it('tap the header = selected, its button list in the side panel, Add capped by the slot max', async () => {
      await page.locator('#ov .ov-head').click();
      await settle(page, 300);
      assert.ok(await page.locator('#ov .ov-head.on').isVisible(), 'outlined on the glass');
      assert.equal(await rows().count(), 0);
      const r = await lab(page, 'headRule');
      assert.ok(r.max > 0, `the slot declares a max: ${JSON.stringify(r)}`);
      assert.equal(r.room, r.max - r.own);
      for (let k = 0; k < r.room; k++) { await page.locator('#side [data-testid="head-add"]').click(); await settle(page, 200); }
      assert.equal(await rows().count(), r.room);
      assert.ok(await page.locator('#side [data-testid="head-add"]').isDisabled(), 'Add disabled at the max');
      assert.ok(await lab(page, 'dirty'));
      assert.deepEqual(await lab(page, 'head'), [], 'not kept until Save');
      assert.equal((await lab(page, 'deck')).pages[0].header_actions.buttons.length, r.room, 'the preview deck has them');
    });

    it('a button: opens a screen (no tap kinds, no icon or style picker), its icon from the screen; reorder; remove; Save', async () => {
      await rows().nth(0).locator('.hb-head').click();
      await settle(page, 200);
      for (const gone of ['select[aria-label="Icon"]', '[data-testid="btn-kind"]', '[data-testid="btn-tap"]', '[data-testid="entity-picker"]'])
        assert.equal(await page.locator(`#side ${gone}`).count(), 0, `no ${gone}`);
      assert.ok(!(await page.locator('#side .prop .pl').allTextContents()).some((t) => /style|icon/i.test(t)), 'no style or icon option');
      await page.locator('#side select[aria-label="Screen"]').selectOption('weather');
      await settle(page, 200);
      let d = await lab(page, 'headDraft');
      assert.deepEqual(d[0], { t: 'page', page: 'weather' });
      assert.ok(d.every((b) => b.t === 'page' && b.page && !('icon' in b)), 'every button opens a screen');
      assert.match(await rows().nth(0).locator('.hb-head').textContent(), /Opens Weather/);
      await rows().nth(1).locator('.hb-head').click();
      await settle(page, 200);
      await page.locator('#side button', { hasText: 'Move up' }).click();
      await settle(page, 200);
      d = await lab(page, 'headDraft');
      assert.equal(d[1].page, 'weather', 'moved up');
      await page.locator('#side .cm-acts .danger').click();
      await settle(page, 200);
      assert.equal(await rows().count(), (await lab(page, 'headRule')).room - 1);
      assert.ok(await page.locator('#side [data-testid="head-add"]').isEnabled(), 'room again');
      await page.locator('#side .cs-save').click();
      await settle(page, 400);
      assert.ok((await lab(page, 'head')).some((b) => b.page === 'weather'));
      assert.equal(await lab(page, 'draft'), null);
    });

    it('drag a row to reorder (staged), Cancel puts it back', async () => {
      const kept = await lab(page, 'head');
      await page.locator('#ov .ov-head').click();
      await settle(page, 300);
      const a = await rows().nth(0).locator('.hb-head').boundingBox(), b = await rows().nth(1).locator('.hb-head').boundingBox();
      await page.mouse.move(a.x + 20, a.y + a.height / 2);
      await page.mouse.down();
      await page.mouse.move(a.x + 24, a.y + a.height / 2 + 10, { steps: 2 });
      await page.mouse.move(b.x + 20, b.y + b.height / 2 + 4, { steps: 8 });
      await page.mouse.up();
      await settle(page, 300);
      assert.deepEqual(await lab(page, 'headDraft'), [kept[1], kept[0], ...kept.slice(2)]);
      await page.locator('#side .cs-cancel').click();
      await settle(page, 300);
      assert.deepEqual(await lab(page, 'head'), kept);
      assert.deepEqual(L.actions, []);
      assert.deepEqual(L.errors, []);
    });
  });
}

after(close);
