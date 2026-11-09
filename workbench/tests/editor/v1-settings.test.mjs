// Editor v1, part 3 (workbench/EDITOR-V1.md): one Settings list as the panel's own: Panel (Screens, Home Assistant),
// then the schema's groups. Screens turns a built-in screen off / on and opens its options; Home Assistant takes the
// address and a token (never shown back whole); a setting reaches the live panel as it changes; the page's own light /
// dark toggle turns the live panel with it.
import { describe, it, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { launch, close, HUB, settle } from './lib.mjs';

describe('Editor v1 Settings', () => {
  let page, ctx, errors = [];
  before(async () => {
    const b = await launch();
    ctx = await b.newContext({ viewport: { width: 1440, height: 1000 }, colorScheme: 'dark', reducedMotion: 'reduce' });
    page = await ctx.newPage();
    page.on('pageerror', (e) => errors.push(String(e)));
    await page.goto(`${HUB}/wasm/editor-v1?device=guition`);
    await page.waitForFunction(() => window.__v1?.engine(), null, { timeout: 60000 });
    await settle(page, 300);
  });
  after(async () => { assert.deepEqual(errors, []); await ctx.close(); await close(); });

  const corner = () => page.evaluate(() => { const st = [...window.__v1.engine().views].find((s) => s.label === 'Live panel'); return st.canvas.getContext('2d').getImageData(4, 4, 1, 1).data[0]; });
  const pages = () => page.evaluate(() => window.__v1.draft().pages.map((p) => p.kind));

  it('the page toggle turns the page and the live panel light / dark together', async () => {
    assert.ok((await corner()) < 40, 'dark browser: dark panel');
    await page.click('#v1-look'); await settle(page, 300);
    assert.equal(await page.evaluate(() => document.documentElement.dataset.theme), 'light');
    assert.ok((await corner()) > 200, 'the live panel turned light');
    await page.click('#v1-look'); await settle(page, 300);
    assert.ok((await corner()) < 40);
  });

  it('the tabs are your own screens only; built-in ones are in Settings', async () => {
    const owns = await page.evaluate(() => window.__v1.draft().pages.filter((p) => p.kind === 'home').map((p) => p.name || 'Home'));
    const tabs = (await page.locator('#v1-tabs .v1-tab').allTextContents()).map((t) => t.replace('★', ''));
    assert.deepEqual(tabs, owns);
  });

  it('Settings opens on Panel: Screens and Home Assistant, then the groups', async () => {
    await page.click('#v1-settings'); await settle(page, 200);
    const rows = await page.locator('.v1-nav .v1-ftxt').allTextContents();
    assert.deepEqual(rows, ['Screens', 'Home Assistant']);
    const caps = await page.locator('.v1-side h2.cap').allTextContents();
    assert.equal(caps[0], 'Panel');
    assert.ok(caps.includes('Look') && caps.includes('Units'));
  });

  it('Screens: a row opens its screen on the live panel; off and on again puts it at the end of the swipe', async () => {
    await page.locator('.v1-nav', { hasText: 'Screens' }).click(); await settle(page, 200);
    const was = await pages();
    const row = page.locator('.v1-side .v1-nav').filter({ hasText: 'On' }).nth(1);
    const name = (await row.locator('.v1-ftxt').textContent()).trim();
    await row.click(); await settle(page, 300);
    assert.equal((await page.locator('.v1-sidetitle b').textContent()).trim(), name);
    assert.match(await page.textContent('#v1-main .v1-stage .v1-meta'), new RegExp(`Live panel · ${name}`), 'the live panel shows that screen');
    await page.locator('.v1-side .v1-switch').click(); await settle(page, 200);
    const off = await pages();
    assert.equal(off.length, was.length - 1);
    assert.match(await page.textContent('#v1-main .v1-stage .v1-meta'), new RegExp(`Live panel · ${name}`), 'still shown while off');
    const gone = was.find((k) => !off.includes(k));
    await page.locator('.v1-side .v1-switch').click(); await settle(page, 200);
    assert.equal((await pages()).at(-1), gone, 'on again = the end of the swipe');
    assert.ok((await page.locator('.v1-side .v1-field').count()) >= 2, 'on, and its name (and options) show');
    await page.locator('.v1-sidehead .btn.icon').first().click(); await settle(page, 200);  // back to Screens
    await page.locator('.v1-sidehead .btn.icon').first().click(); await settle(page, 200);  // back to Settings
  });

  it('Home Assistant: the address and the token; the token shows only its last four', async () => {
    await page.locator('.v1-nav', { hasText: 'Home Assistant' }).click(); await settle(page, 200);
    const url = page.locator('.v1-side .v1-input').nth(0), tok = page.locator('.v1-side .v1-input').nth(1);
    await url.fill('http://ha.example:8123'); await url.press('Tab'); await settle(page, 200);
    await tok.fill('abcdefgh1234'); await tok.press('Tab'); await settle(page, 200);
    assert.match(await page.textContent('.v1-sidetitle'), /Set, ha\.example/);
    const ch = await page.evaluate(() => window.__v1.changes());
    const t = ch.find((c) => c.what === 'Token');
    assert.equal(t.now, '…1234');
    assert.ok(!JSON.stringify(ch).includes('abcdefgh'), 'the token is never said whole');
    await page.locator('.v1-sidehead .btn.icon').first().click(); await settle(page, 200);
  });

  it('a setting reaches the live panel as it changes', async () => {
    await page.locator('.v1-field', { hasText: 'Text size' }).locator('.seg button').last().click(); await settle(page, 200);
    const v = await page.evaluate(() => { const e = window.__v1.engine(); return e.withStr('textSize', (pi) => e.str(e.m._fw_setting(pi, 0))); });
    assert.equal(v, 'large');
    assert.ok((await page.evaluate(() => window.__v1.check())).ok);
  });
});
