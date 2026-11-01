// Workbench status per item (new / done), keyed "<section>/<id>": done = on the contract
// (renderer/catalogue/contract.json); src/lib/status.mjs (cached per HEAD).
// Each entry: { status, name?, file, contract, last: {sha, subject, at} | null }. Used by /wasm.
import type { APIRoute } from 'astro';
import { status } from '../../../lib/status.mjs';

export const prerender = false;
export const GET: APIRoute = async () => {
  const out = await status().catch((e) => ({ error: String(e) }));
  return new Response(JSON.stringify(out), { headers: { 'content-type': 'application/json', 'cache-control': 'no-store' } });
};
