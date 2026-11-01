// The editor served BY a panel (scripts/editor-device.mjs bundle, editor-host-device.ts) against a stubbed panel:
// GET /fw/deck, /fw/spec, /fw/info, /fw/ha?want=, POST /fw/spec. the panel subscribes to every entity it is
// asked for, for good, so the editor may only ask for the entities its deck names (never the sample screens' demo ids).
import { describe, it, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { mkdtempSync, readFileSync, existsSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, extname } from 'node:path';
import { launch, close, fixture, fe, texts, settle, homeOf, lower } from './lib.mjs';

const WBDIR = new URL('../..', import.meta.url).pathname;
// the renderer build: FW_OUT (env or the repo's local.env), else renderer/out
const localEnv = join(WBDIR, '..', 'local.env');
const FW_OUT = process.env.FW_OUT ?? (existsSync(localEnv) ? readFileSync(localEnv, 'utf8').match(/^FW_OUT=(.*)$/m)?.[1]?.trim() : undefined) ?? join(WBDIR, '..', 'renderer', 'out');
const ENTITY = /^[a-z_]+\.[a-z0-9_]+$/;
const named = (v, out = new Set()) => { if (typeof v === 'string') { if (ENTITY.test(v)) out.add(v); } else if (v && typeof v === 'object') for (const x of Object.values(v)) named(x, out); return out; };
const TYPES = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.wasm': 'application/wasm', '.woff2': 'font/woff2' };

const dists = {};
function bundle(device) {
  if (dists[device]) return dists[device];
  const den = { guition: '1x', waveshare: '1.5x' }[device];
  const d = mkdtempSync(join(tmpdir(), `fe-e2e-${device}-`));
  const board = { guition: 'guition-4848s040', waveshare: 'waveshare-p4-86' }[device];
  execFileSync('node', ['scripts/editor-device.mjs', '--wasm', join(FW_OUT, den), '--device', board, '--den', den, '--dist', d], { cwd: WBDIR, stdio: 'pipe' });
  return (dists[device] = d);
}

/** a panel: its deck, its HA values (friendly names from the fixture), Save answered by `answer(body)` */
async function openPanel(device, { answer = () => ({ ok: true, status: 'applied', rev: 2 }) } = {}) {
  const dist = bundle(device), src = fixture(), deck = src.deck;
  const names = Object.fromEntries(Object.values(src.options).flat().map((o) => [o.id, o.name]));
  const ctx = await (await launch()).newContext({ viewport: { width: 1440, height: 1000 }, reducedMotion: 'reduce' });
  const page = await ctx.newPage();
  const P = { page, wants: [], posts: [], errors: [], deck, async done() { await ctx.close(); } };
  page.on('pageerror', (e) => P.errors.push(String(e)));
  await page.route('http://panel.test/**', async (r) => {
    const u = new URL(r.request().url());
    if (u.pathname === '/fw/deck') return r.fulfill({ json: { deck } });
    if (u.pathname === '/fw/info') return r.fulfill({ json: { ok: true } });
    if (u.pathname === '/fw/ha') {
      const want = (u.searchParams.get('want') ?? '').split(',').filter(Boolean);
      P.wants.push(...want);
      return r.fulfill({ json: { values: Object.fromEntries(want.flatMap((id) => [[id, 'on'], [`${id}|friendly_name`, names[id] ?? id]])) } });
    }
    if (u.pathname === '/fw/spec') {
      if (r.request().method() === 'GET') return r.fulfill({ json: { screens: {} } });
      const body = JSON.parse(r.request().postData());
      P.posts.push(body);
      return r.fulfill({ json: answer(body) });
    }
    const f = join(dist, u.pathname === '/' ? 'index.html' : u.pathname);
    if (!existsSync(f)) return r.fulfill({ status: 404, body: '' });
    return r.fulfill({ body: readFileSync(f), contentType: TYPES[extname(f)] ?? 'application/octet-stream' });
  });
  await page.goto('http://panel.test/');
  await page.waitForFunction(() => window.__fe?.ready(), null, { timeout: 60000 });
  await settle(page);
  return P;
}

for (const device of ['guition', 'waveshare']) {
  describe(`device host (${device})`, () => {
    it('asks the panel only for the entities its deck names, across the overview and a Home', async () => {
      const P = await openPanel(device);
      await P.page.locator('[data-testid="home-tile"][data-id="living"] .shot').click(); await settle(P.page);
      await P.page.waitForTimeout(5500);  // one more HA poll
      const inDeck = named(P.deck);
      assert.ok(P.wants.length > 0, 'the editor asked for live values');
      const extra = [...new Set(P.wants)].filter((id) => !inDeck.has(id));
      assert.deepEqual(extra, [], 'no demo / sample entity asked for');
      assert.ok(lower(await texts(P.page, 'Preview')).includes('ceiling'), 'the Living room Home draws');
      assert.deepEqual(P.errors, []);
      await P.done();
    });

    it('adding a card shows that card, and Save posts exactly the edited deck', async () => {
      const P = await openPanel(device);
      await P.page.locator('[data-testid="home-tile"][data-id="kitchen"] .shot').click(); await settle(P.page);
      await P.page.locator('[data-testid="add-card"] button[data-v="cover"]').click(); await settle(P.page, 900);
      const card = homeOf(await fe(P.page, 'deck'), 'kitchen').cards.at(-1);
      assert.equal(card.t, 'cover');
      assert.equal((await fe(P.page, 'flow')).err, '');
      const drawn = lower(await texts(P.page, 'Card preview'));
      assert.ok(drawn.includes(card.e.toLowerCase()) || drawn.some((t) => t.includes('cover')), `card preview draws the new cover (${drawn.join(' | ')})`);
      assert.ok(!drawn.includes('pantry') && !drawn.includes('ceiling'), 'only that card');
      await P.page.keyboard.press('Escape'); await settle(P.page, 300);
      const shown = await fe(P.page, 'deck');
      await P.page.locator('#ed-save').click(); await settle(P.page, 300);
      await P.page.locator('#ed-dialog .btn.primary').click(); await settle(P.page);
      assert.equal(P.posts.length, 1);
      assert.equal(P.posts[0].kind, 'deck');
      assert.deepEqual(P.posts[0].spec, { ...shown, rev: shown.rev + 1 });
      assert.equal(await P.page.locator('#ed-dirty').isHidden(), true, 'saved');
      await P.done();
    });
  });
}

describe('device host: a refused Save', () => {
  it('says why and keeps the edits unsaved', async () => {
    const P = await openPanel('guition', { answer: () => ({ ok: false, errors: [{ msg: 'too many cards', path: 'pages[1].cards' }] }) });
    await P.page.locator('[data-testid="home-tile"][data-id="kitchen"] .shot').click(); await settle(P.page);
    await P.page.locator('[data-testid="add-card"] button[data-v="light"]').click(); await settle(P.page);
    await P.page.keyboard.press('Escape'); await settle(P.page, 300);
    await P.page.locator('#ed-save').click(); await settle(P.page, 300);
    await P.page.locator('#ed-dialog .btn.primary').click(); await settle(P.page);
    assert.match(await P.page.locator('#ed-dialog .note:not([hidden])').last().innerText(), /Refused: too many cards \(pages\[1\]\.cards\)/);
    assert.equal(await P.page.locator('#ed-dialog .btn.primary').isEnabled(), true, 'can try again');
    await P.page.keyboard.press('Escape'); await settle(P.page, 300);
    assert.match(await P.page.locator('#ed-dirty').textContent(), /^1 unsaved change$/);
    await P.done();
  });
});
after(async () => { await close(); for (const d of Object.values(dists)) rmSync(d, { recursive: true, force: true }); });
