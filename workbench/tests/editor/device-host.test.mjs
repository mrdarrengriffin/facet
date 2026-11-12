// Editor v1 served BY a panel (scripts/editor-device.mjs bundle, editor-v1-host-device.ts) against a stubbed panel:
// GET /fw/deck, /fw/settings, /fw/link, /fw/ha?want=, /fw/entities?index=1, POST /fw/spec. The panel subscribes to
// every entity it is asked for, for good, so the editor may only ask for the entities its deck names (never the sample
// screens' demo ids); Save posts exactly the edited deck; a refused Save keeps the edits.
import { describe, it, after } from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { mkdtempSync, readFileSync, existsSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, extname } from 'node:path';
import { launch, close, settle } from './lib.mjs';

const WBDIR = new URL('../..', import.meta.url).pathname;
// the renderer build: FW_OUT (env or the repo's local.env), else renderer/out
const localEnv = join(WBDIR, '..', 'local.env');
const FW_OUT = process.env.FW_OUT ?? (existsSync(localEnv) ? readFileSync(localEnv, 'utf8').match(/^FW_OUT=(.*)$/m)?.[1]?.trim() : undefined) ?? join(WBDIR, '..', 'renderer', 'out');
const ENTITY = /^[a-z_]+\.[a-z0-9_]+$/;
const named = (v, out = new Set()) => { if (typeof v === 'string') { if (ENTITY.test(v)) out.add(v); } else if (v && typeof v === 'object') for (const x of Object.values(v)) named(x, out); return out; };
const TYPES = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.wasm': 'application/wasm', '.woff2': 'font/woff2' };

// the panel's deck: two Homes of the panel's own (anonymous entities), the Clock opened from a card
const DECK = { v: 2, rev: 1, start: 'living', pages: [
  { id: 'living', kind: 'home', name: 'Living room', cards: [
    { t: 'light', e: 'light.test_ceiling', size: '1x1', label: 'Ceiling' },
    { t: 'sensor', e: 'sensor.test_hall', size: 'half' },
    { t: 'page', page: 'clock', size: '1x1' }] },
  { id: 'kitchen', kind: 'home', name: 'Kitchen', cards: [{ t: 'light', e: 'light.test_pantry', size: '1x1', label: 'Pantry' }] },
  { id: 'clock', kind: 'clock' }] };
// what the panel's Home Assistant has (the add list's index): the deck's and one more light
const HA = [['light.test_ceiling', 'Ceiling', '', 'on', ''], ['light.test_pantry', 'Pantry', '', 'off', ''], ['light.test_chair', 'Chair light', '', 'on', ''],
  ['sensor.test_hall', 'Hall', 'temperature', '21.5', '°C']];
const SETTINGS = Object.fromEntries(JSON.parse(readFileSync(join(WBDIR, '..', 'renderer/tokens/settings.json'), 'utf8')).settings.map((x) => [x.id, x.default]));
const NAMES = Object.fromEntries(HA.map(([id, n]) => [id, n]));

const dists = {};
function bundle(device) {
  if (dists[device]) return dists[device];
  const den = { guition: '1x', waveshare: '1.5x' }[device];
  const d = mkdtempSync(join(tmpdir(), `fe-e2e-${device}-`));
  const board = { guition: 'guition-4848s040', waveshare: 'waveshare-p4-86' }[device];
  execFileSync('node', ['scripts/editor-device.mjs', '--wasm', join(FW_OUT, den), '--device', board, '--den', den, '--dist', d], { cwd: WBDIR, stdio: 'pipe' });
  return (dists[device] = d);
}

/** a panel: its deck, its HA values, Save answered by `answer(body)` */
async function openPanel(device, { answer = () => ({ ok: true, status: 'applied', rev: 2 }) } = {}) {
  const dist = bundle(device), deck = structuredClone(DECK);
  const ctx = await (await launch()).newContext({ viewport: { width: 1440, height: 1000 }, reducedMotion: 'reduce' });
  const page = await ctx.newPage();
  const P = { page, wants: [], posts: [], errors: [], deck, async done() { await ctx.close(); } };
  page.on('pageerror', (e) => P.errors.push(String(e)));
  page.on('dialog', (d) => d.accept());
  await page.route('http://panel.test/**', async (r) => {
    const u = new URL(r.request().url());
    if (u.pathname === '/fw/deck') return r.fulfill({ json: { deck } });
    if (u.pathname === '/fw/settings') return r.fulfill({ json: { ok: true, values: SETTINGS } });
    if (u.pathname === '/fw/link') return r.fulfill({ json: { url: 'http://ha.test:8123', token: '…abcd', state: 'linked' } });
    if (u.pathname === '/fw/info') return r.fulfill({ json: { ok: true } });
    if (u.pathname === '/fw/ha') {
      const want = (u.searchParams.get('want') ?? '').split(',').filter(Boolean);
      P.wants.push(...want);
      return r.fulfill({ json: { values: Object.fromEntries(want.filter((id) => !id.startsWith('panel.')).flatMap((id) => [[id, 'on'], [`${id}|friendly_name`, NAMES[id] ?? id]])) } });
    }
    if (u.pathname === '/fw/entities') {
      if (u.searchParams.get('index')) {
        const ds = (u.searchParams.get('domains') ?? '').split(',');
        return r.fulfill({ body: HA.filter((x) => ds.includes(x[0].split('.')[0])).map((x) => x.join('\t')).join('\n') + '\n#ok\t0\n', contentType: 'text/plain' });
      }
      const ds = (u.searchParams.get('send') ?? '').split(',').filter(Boolean);
      return r.fulfill({ json: { state: 'ok', lists: Object.fromEntries(ds.map((d) => [d, HA.filter((x) => x[0].startsWith(`${d}.`)).map(([id, name]) => ({ id, name }))])) } });
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
  await page.waitForFunction(() => window.__v1?.engine(), null, { timeout: 60000 });
  await settle(page, 800);
  return P;
}
/** the label texts the live panel drew (fw_audit), lower case */
const drawn = (page) => page.evaluate(() => {
  const e = window.__v1.engine(), st = [...e.views].find((s) => s.label === 'Live panel') ?? [...e.views][0];
  return JSON.parse(e.str(e.m._fw_audit(st.id))).filter((o) => o.text).map((o) => o.text.toLowerCase());
});
/** add a 1 x 1 light card for `q` on the first empty square */
async function addLight(page, q) {
  await page.locator('.v1-cell.add:not(.half)').first().click(); await settle(page, 200);
  await page.locator('.v1-cardlist .v1-opt', { hasText: 'Light' }).click(); await settle(page, 300);
  await page.fill('.v1-side .v1-search input', q); await settle(page, 150);
  await page.locator('.v1-cardlist .v1-opt').first().click(); await settle(page, 400);
}
const homeCards = (deck, id) => deck.pages.find((p) => p.id === id).cards;

for (const device of ['guition', 'waveshare']) {
  describe(`device host (${device})`, () => {
    it('asks the panel only for the entities its deck names, across its Homes', async () => {
      const P = await openPanel(device);
      assert.ok((await drawn(P.page)).some((t) => t.includes('ceiling')), 'the Living room Home draws');
      await P.page.locator('.v1-tab', { hasText: 'Kitchen' }).click(); await settle(P.page);
      await P.page.waitForTimeout(5500);  // one more HA poll
      const inDeck = named(P.deck);
      assert.ok(P.wants.length > 0, 'the editor asked for live values');
      const extra = [...new Set(P.wants)].filter((id) => !inDeck.has(id) && !id.startsWith('panel.'));
      assert.deepEqual(extra, [], 'no demo / sample entity asked for');
      assert.ok((await drawn(P.page)).some((t) => t.includes('pantry')), 'the Kitchen Home draws');
      assert.deepEqual(P.errors, []);
      await P.done();
    });

    it('adding a card shows that card, and Save posts exactly the edited deck', async () => {
      const P = await openPanel(device);
      const n = homeCards(await P.page.evaluate(() => window.__v1.deck()), 'living').length;
      await addLight(P.page, 'chair');
      const shown = await P.page.evaluate(() => window.__v1.deck());
      const card = homeCards(shown, 'living').at(-1);
      assert.equal(homeCards(shown, 'living').length, n + 1);
      assert.deepEqual([card.t, card.e], ['light', 'light.test_chair']);
      assert.ok((await drawn(P.page)).some((t) => t.includes('chair')), 'the live panel draws the new card');
      await P.page.locator('#v1-save').click(); await settle(P.page, 200);
      await P.page.locator('.v1-acts .btn.primary', { hasText: 'Save' }).click(); await settle(P.page);
      const decks = P.posts.filter((p) => p.kind === 'deck');
      assert.equal(decks.length, 1);
      assert.deepEqual(decks[0].spec, { ...shown, rev: shown.rev + 1 });
      assert.equal((await P.page.evaluate(() => window.__v1.changes())).length, 0, 'saved');
      assert.deepEqual(P.errors, []);
      await P.done();
    });
  });
}

describe('device host: a refused Save', () => {
  it('says why and keeps the edits unsaved', async () => {
    const P = await openPanel('guition', { answer: () => ({ ok: false, errors: [{ msg: 'too many cards', path: '.pages[1].cards[0]', code: 'too_many' }] }) });
    await addLight(P.page, 'chair');
    await P.page.locator('#v1-save').click(); await settle(P.page, 200);
    await P.page.locator('.v1-acts .btn.primary', { hasText: 'Save' }).click(); await settle(P.page);
    const side = await P.page.locator('.v1-side').innerText();
    assert.match(side, /the panel refused it · 1/i);
    assert.match(side, /too many cards/);
    assert.match(side, /Kitchen · Pantry card/, 'said against its screen and card');
    assert.equal((await P.page.evaluate(() => window.__v1.changes())).length, 1, 'the edit is kept');
    assert.match(await P.page.locator('#v1-save').textContent(), /^Save · 1$/);
    await P.done();
  });
});
after(async () => { await close(); for (const d of Object.values(dists)) rmSync(d, { recursive: true, force: true }); });
