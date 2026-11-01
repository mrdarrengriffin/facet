// /api/wasm/goldens: the golden screenshot review (page /wasm/goldens). Facet's tests write a candidate
// <suite>/<den>-<name>.new.png (+ .new.565.gz, the pixels the test compares) when there is no golden or it differs
// (renderer/tests/functional/_release.mjs golden()). A golden is its .565.gz alone; its PNG is rendered on demand
// (renderer/tools/golden_png.mjs). Accept = rename the candidate .565.gz to the golden and drop the candidate PNG (what
// FW_GOLDEN_ACCEPT=1 does); reject = delete the candidate (the next test run writes it again if it still differs).
//   GET            -> {items: [{suite, base, old}]}  (base = <den>-<name>; old = a golden exists)
//   GET ?img=<suite>/<base>[.new].png -> the PNG (the golden's rendered from its .565.gz)
//   POST {suite, base, action: 'accept' | 'reject'}
import type { APIRoute } from 'astro';
import { readdir, readFile, rename, unlink, access } from 'node:fs/promises';
import { join } from 'node:path';
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { REPO } from '../../../lib/repo';

export const prerender = false;
const RENDERER = `${REPO}/renderer`;
const DIR = `${RENDERER}/tests/golden`;
const run = promisify(execFile);
// the golden <suite>/<base> as a PNG, from its .565.gz
const render = async (key: string) => (await run('node', ['tools/golden_png.mjs', key, '-'], { cwd: RENDERER, encoding: 'buffer', maxBuffer: 64 << 20 })).stdout;
const SAFE = /^[a-z0-9_-]+$/i, BASE = /^[a-z0-9_.-]+$/i;
const json = (o: unknown, status = 200) => new Response(JSON.stringify(o), { status, headers: { 'content-type': 'application/json' } });
const has = (p: string) => access(p).then(() => true, () => false);

export const GET: APIRoute = async ({ url }) => {
  const img = url.searchParams.get('img');
  if (img) {
    const [suite, file] = img.split('/');
    if (!SAFE.test(suite ?? '') || !BASE.test(file ?? '') || !file.endsWith('.png') || file.includes('..')) return json({ error: 'bad path' }, 400);
    const p = join(DIR, suite, file), golden = !file.endsWith('.new.png') && !(await has(p));
    try { return new Response(golden ? await render(`${suite}/${file.slice(0, -'.png'.length)}`) : await readFile(p), { headers: { 'content-type': 'image/png', 'cache-control': 'no-cache' } }); }
    catch { return json({ error: 'not found' }, 404); }
  }
  const items: { suite: string; base: string; old: boolean }[] = [];
  for (const suite of (await readdir(DIR, { withFileTypes: true })).filter((d) => d.isDirectory()).map((d) => d.name).sort()) {
    const files = await readdir(join(DIR, suite));
    const set = new Set(files);
    for (const f of files.filter((x) => x.endsWith('.new.png')).sort()) {
      const base = f.slice(0, -'.new.png'.length);
      items.push({ suite, base, old: set.has(`${base}.565.gz`) });
    }
  }
  return json({ items });
};

export const POST: APIRoute = async ({ request }) => {
  const { suite, base, action } = await request.json().catch(() => ({}));
  if (!SAFE.test(suite ?? '') || !BASE.test(base ?? '') || base.includes('..') || !['accept', 'reject'].includes(action)) return json({ error: 'bad request' }, 400);
  const p = join(DIR, suite, base);
  if (!(await has(`${p}.new.png`))) return json({ error: 'no candidate' }, 404);
  if (action === 'accept' && (await has(`${p}.new.565.gz`))) await rename(`${p}.new.565.gz`, `${p}.565.gz`);
  for (const ext of ['.565.gz', '.png']) if (await has(`${p}.new${ext}`)) await unlink(`${p}.new${ext}`);
  return json({ ok: true });
};
