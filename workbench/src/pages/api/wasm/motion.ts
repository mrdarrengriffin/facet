// Workbench: the motion tokens (renderer/tokens/motion.json), read fresh on every
// request. Used by src/wasm/app.ts for durations the page itself needs (demo-reset: samples return to their initial state).
import type { APIRoute } from 'astro';
import { readFile } from 'node:fs/promises';
import { REPO } from '../../../lib/repo';

export const prerender = false;
const WASM_DIR = REPO;

export const GET: APIRoute = async () => {
  try {
    const body = await readFile(`${WASM_DIR}/renderer/tokens/motion.json`, 'utf8');
    return new Response(body, { headers: { 'content-type': 'application/json', 'cache-control': 'no-store' } });
  } catch (e) {
    return new Response(JSON.stringify({ error: String(e) }), { status: 500, headers: { 'content-type': 'application/json' } });
  }
};
