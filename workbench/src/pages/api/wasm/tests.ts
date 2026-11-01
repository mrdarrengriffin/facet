// Functional tests, seen (/wasm/tests). Runs renderer/tools/functional.mjs (renderer/tests/functional/README.md)
// against the build the workbench shows (FW_OUT) and keeps the last run in $FW_CACHE/workbench-tests/ (results.json +
// png/). The whole suite takes a few seconds, so POST runs it inline.
//   GET  /api/wasm/tests                  -> results.json of the last run (+ {tree, running}) or {results: null}
//   GET  /api/wasm/tests?png=<id>/<name>  -> a test's screenshot
//   POST /api/wasm/tests {ids?}           -> runs the suite (both sizes, PNG per test), returns the new results
import type { APIRoute } from 'astro';
import { spawnSync } from 'node:child_process';
import { readFileSync, existsSync, mkdirSync } from 'node:fs';
import { join } from 'node:path';
import { FW_CACHE, FW_OUT, REPO } from '../../../lib/repo';

export const prerender = false;
const DATA = join(FW_CACHE, 'workbench-tests');
const TREE = REPO;
const RUNNER = join(TREE, 'renderer', 'tools', 'functional.mjs');
const json = (o: unknown, status = 200) => new Response(JSON.stringify(o), { status, headers: { 'content-type': 'application/json', 'cache-control': 'no-store' } });
const g = globalThis as unknown as { __testsRunning?: boolean };
const last = () => { try { return JSON.parse(readFileSync(join(DATA, 'results.json'), 'utf8')); } catch { return null; } };

export const GET: APIRoute = ({ url }) => {
  const png = url.searchParams.get('png');
  if (png) {
    const f = /^[a-z0-9_]+\/[A-Za-z0-9.=_-]+\.png$/.test(png) ? join(DATA, 'png', png) : '';
    if (!f || !existsSync(f)) return new Response('not found', { status: 404 });
    return new Response(readFileSync(f), { headers: { 'content-type': 'image/png', 'cache-control': 'no-cache' } });
  }
  return json({ tree: TREE, runner: existsSync(RUNNER), running: !!g.__testsRunning, results: last() });
};

export const POST: APIRoute = async ({ request }) => {
  if (!existsSync(RUNNER)) return json({ error: `no functional test runner in ${TREE} yet` }, 404);
  if (g.__testsRunning) return json({ error: 'a run is already going' }, 409);
  const ids = ((await request.json().catch(() => ({}))) as { ids?: string[] }).ids;
  const args = [RUNNER, '--png', 'all', '--soft', '--out', DATA, ...(ids?.length ? ['--ids', ids.filter((i) => /^[a-z0-9_]+$/.test(i)).join(',')] : [])];
  mkdirSync(DATA, { recursive: true });
  g.__testsRunning = true;
  try {
    const r = spawnSync(process.execPath, args, { cwd: TREE, encoding: 'utf8', timeout: 120_000, env: { ...process.env, FW_OUT } });
    if (r.status !== 0) return json({ error: (r.stderr || r.stdout || 'run failed').slice(-600) }, 500);
  } finally { g.__testsRunning = false; }
  return json({ tree: TREE, runner: true, running: false, results: last() });
};
