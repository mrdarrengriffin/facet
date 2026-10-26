// Headless check of the built /install-test/ page (after `astro build`): it loads the vendored, patched ESP Web Tools
// (nothing from unpkg), the install button is defined, the patched dialog chunk + facet-flash.js import cleanly, the
// page is noindex, and every board's manifest and the images it names are served. Needs public/firmware/ (untracked).
// Run: node site/scripts/test-install-page.mjs   (PLAYWRIGHT=<path to playwright/index.mjs> if not installed here)
import { createServer } from 'node:http';
import { existsSync, readFileSync, statSync } from 'node:fs';
import { dirname, extname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const SITE = join(dirname(fileURLToPath(import.meta.url)), '..');
const DIST = join(SITE, 'dist');
const BASE = (process.env.FACET_DOCS_BASE ?? (readFileSync(join(SITE, 'astro.config.mjs'), 'utf8').match(/base:[^\n]*?['"]([^'"]+)['"]/) || [, '/'])[1]).replace(/\/?$/, '/');
const VENDOR = 'vendor/esp-web-tools-10.4.0-facet.1/';
const { chromium } = await import(process.env.PLAYWRIGHT || 'playwright');

const types = { '.html': 'text/html', '.js': 'text/javascript', '.json': 'application/json', '.css': 'text/css', '.bin': 'application/octet-stream' };
const server = createServer((req, res) => {
  let p = decodeURIComponent(new URL(req.url, 'http://x').pathname);
  if (!p.startsWith(BASE)) { res.writeHead(404).end(); return; }
  p = join(DIST, p.slice(BASE.length));
  if (existsSync(p) && statSync(p).isDirectory()) p = join(p, 'index.html');
  if (!existsSync(p)) { res.writeHead(404).end(); return; }
  res.writeHead(200, { 'content-type': types[extname(p)] || 'application/octet-stream' });
  res.end(req.method === 'HEAD' ? undefined : readFileSync(p));
}).listen(0, '127.0.0.1');
await new Promise((r) => server.once('listening', r));
const origin = `http://127.0.0.1:${server.address().port}`;

let fails = 0;
const ok = (cond, what) => { if (!cond) fails++; console.log(`${cond ? 'ok  ' : 'FAIL'} ${what}`); };
const browser = await chromium.launch(process.env.CHROMIUM ? { executablePath: process.env.CHROMIUM } : {});
try {
  const page = await browser.newPage();
  const requests = [];
  const errors = [];
  page.on('request', (r) => requests.push(r.url()));
  page.on('pageerror', (e) => errors.push(String(e)));
  await page.goto(`${origin}${BASE}install-test/`, { waitUntil: 'networkidle' });

  ok(requests.some((u) => u.endsWith(`${VENDOR}install-button.js`)), 'loads the vendored install-button.js');
  ok(!requests.some((u) => u.includes('unpkg.com') || u.includes('esp-web-tools@')), 'nothing from unpkg');
  ok(await page.evaluate(() => !!customElements.get('esp-web-install-button')), 'esp-web-install-button is defined');
  ok((await page.getAttribute('meta[name="robots"]', 'content'))?.includes('noindex'), 'page is noindex');
  // the patched dialog chunk imports facet-flash.js; loading it must not throw
  const chunk = await page.evaluate(async (v) => {
    const html = await (await fetch(v + 'install-button.js')).text();
    const name = html.match(/install-dialog-[\w-]+\.js/)?.[0];
    try { await import(v + name); const f = await import(v + 'facet-flash.js'); return { name, ok: typeof f.facetFlash === 'function' }; }
    catch (e) { return { name, ok: false, err: String(e) }; }
  }, `${origin}${BASE}${VENDOR}`);
  ok(chunk.ok, `patched ${chunk.name} + facet-flash.js import (${chunk.err || 'clean'})`);

  const manifests = await page.$$eval('[data-boards] input[name="board"]', (els) => els.map((e) => e.dataset.manifest));
  ok(manifests.length === 2, `two boards (${manifests.join(', ')})`);
  for (const m of manifests) {
    const res = await page.request.get(origin + m);
    const json = res.ok() ? await res.json() : null;
    ok(!!json?.builds?.length, `manifest ${m} ${json ? json.version : res.status()}`);
    for (const part of json?.builds?.[0]?.parts ?? []) {
      const r = await page.request.head(new URL(part.path, origin + m).toString());
      ok(r.ok(), `  ${part.path} served`);
    }
  }
  // picking the other board switches the button's manifest
  await page.check('[data-boards] input[value="guition-4848s040"]', { force: true });
  ok((await page.getAttribute('[data-install]', 'manifest'))?.includes('guition-4848s040'), 'board pick sets the manifest');
  ok(errors.length === 0, `no page errors ${errors.join(' | ')}`);
  // the live page still uses upstream (unchanged until Darren has tried the test page)
  const live = readFileSync(join(DIST, 'install/index.html'), 'utf8');
  ok(live.includes('unpkg.com/esp-web-tools@10.4.0') && !live.includes(VENDOR), 'live /install/ unchanged (upstream 10.4.0)');
} finally {
  await browser.close();
  server.close();
}
console.log(fails ? `${fails} FAILED` : 'all passed');
process.exit(fails ? 1 : 0);
