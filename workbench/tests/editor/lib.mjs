// Shared harness for the editor end-to-end tests (README.md here): one Chromium, the real workbench editor page
// (/wasm/editor, the real WASM engine), a fixed house (fixtures/sources.json in place of /api/wasm/sources) and a stubbed
// HA (entity states from the fixture's names; actions are recorded, never sent: a test must never switch a real light).
import { chromium } from 'playwright';
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import assert from 'node:assert/strict';

const HERE = dirname(fileURLToPath(import.meta.url));
export const HUB = process.env.WORKBENCH_URL ?? 'http://localhost:4322';
export const fixture = () => JSON.parse(readFileSync(join(HERE, 'fixtures/sources.json'), 'utf8'));
export const DEV = { guition: 480, waveshare: 720 };

let browser = null;
export async function launch() {
  browser ??= await chromium.launch({ executablePath: process.env.CHROMIUM_PATH ?? '/usr/bin/chromium', args: ['--enable-unsafe-swiftshader'] });
  return browser;
}
export async function close() { await browser?.close(); browser = null; }

const names = (src) => Object.fromEntries(Object.values(src.options).flat().map((o) => [o.id, o.name]));

/** open the workbench editor: device guition | waveshare, query = extra params (home=, screen=), src = the house (a fixture copy) */
export async function openEditor({ device = 'guition', query = '', src = fixture(), width = 1440, height = 1000 } = {}) {
  const b = await launch();
  const ctx = await b.newContext({ ignoreHTTPSErrors: true, viewport: { width, height }, reducedMotion: 'reduce' });
  const page = await ctx.newPage();
  const errors = [], actions = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  page.on('dialog', (d) => d.accept());
  const nm = names(src);
  await page.route('**/api/wasm/sources', (r) => r.fulfill({ json: src }));
  await page.route('**/api/wasm/show**', (r) => r.fulfill({ json: { online: true, ip: '192.0.2.1' } }));
  await page.route('**/api/panel/entities**', (r) => {
    const ids = (new URL(r.request().url()).searchParams.get('ids') ?? '').split(',').filter(Boolean);
    r.fulfill({ json: ids.map((id) => ({ id, name: nm[id] ?? id, state: 'on', attrs: {} })) });
  });
  await page.route('**/api/panel/action', (r) => { actions.push(r.request().postData()); r.fulfill({ json: { ok: true } }); });
  await page.goto(`${HUB}/wasm/editor?device=${device}&theme=dark${query ? `&${query}` : ''}`);
  await page.waitForFunction(() => window.__fe?.ready(), null, { timeout: 60000 });
  await settle(page);
  return { page, ctx, errors, actions, src, size: DEV[device], async done() { await ctx.close(); } };
}

/** let the engine paint (its rise animations) and the editor's 450 ms post-rise work run */
export const settle = (page, ms = 600) => page.waitForTimeout(ms);
export const fe = (page, f, ...a) => page.evaluate(([f, a]) => window.__fe[f](...a), [f, a]);
/** the label texts a stage drew, top to bottom (fw_audit) */
export const texts = async (page, label) => ((await fe(page, 'texts', label)) ?? []).map((o) => o.t);
export const homeOf = (deck, id) => deck.pages.find((p) => p.id === id);

/** the click point (page px) of a Home box on the Preview stage: card i, or -1 = the header actions */
export async function cardPoint(page, i) {
  const boxes = await fe(page, 'boxes', 'Preview');
  const b = boxes.find((x) => x.card === i);
  assert.ok(b, `card ${i} has a box on the preview`);
  const hits = page.locator('[data-testid="preview-hits"]');
  const r = await hits.boundingBox();
  const S = Number(await hits.getAttribute('data-size'));
  const k = r.width / S;
  return { x: r.x + (b.x + b.w / 2) * k, y: r.y + (b.y + b.h / 2) * k, b };
}
export async function clickCard(page, i) { const p = await cardPoint(page, i); await page.mouse.click(p.x, p.y); await settle(page); }
/** pick card `from` up and drop it over card `to` (mouse: lift after 6 px); esc = cancel before the drop */
export async function dragCard(page, from, to, { esc = false } = {}) {
  const a = await cardPoint(page, from), b = await cardPoint(page, to);
  await page.mouse.move(a.x, a.y); await page.mouse.down();
  for (let s = 1; s <= 8; s++) await page.mouse.move(a.x + ((b.x - a.x) * s) / 8, a.y + ((b.y - a.y) * s) / 8);
  await page.waitForTimeout(150);
  if (esc) await page.keyboard.press('Escape');
  await page.mouse.up(); await settle(page);
}
/** the same pick-up for an overview tile (data-testid home-tile / screen-tile) */
export async function dragTile(page, tid, fromId, toId, { esc = false } = {}) {
  const a = await page.locator(`[data-testid="${tid}"][data-id="${fromId}"] .shot`).boundingBox();
  const b = await page.locator(`[data-testid="${tid}"][data-id="${toId}"] .shot`).boundingBox();
  const ax = a.x + a.width / 2, ay = a.y + a.height / 2, bx = b.x + b.width / 2, by = b.y + b.height / 2;
  await page.mouse.move(ax, ay); await page.mouse.down();
  for (let s = 1; s <= 10; s++) { await page.mouse.move(ax + ((bx - ax) * s) / 10, ay + ((by - ay) * s) / 10); await page.waitForTimeout(30); }
  await page.waitForTimeout(150);
  if (esc) await page.keyboard.press('Escape');
  await page.mouse.up(); await settle(page);
}
export const ids = (xs) => xs.map((x) => x.id);
export const lower = (xs) => xs.map((x) => x.toLowerCase());
/** a modal is open on the page */
export const modalOpen = (page) => page.evaluate(() => document.getElementById('ed-dialog').open);

/** what the card shows in its own rendering: its label if it has one, else the entity's name / the page's name */
export function expectedTitle(card, src) {
  if (card.label) return card.label;
  if (card.t === 'page') return null;
  return names(src)[card.e] ?? null;
}
