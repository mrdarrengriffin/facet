// The renderer build the workbench shows: $FW_OUT/<path> (lib/repo.ts) at /facet-wasm/<path>: <density>/fw.mjs,
// fw.wasm, manifest.json, features.json, and the visual report (visual/<density>/). manifest.json is never cached; the
// engine asks for fw.mjs / fw.wasm with ?v=<built>.
import type { APIRoute } from 'astro';
import { readFile, stat } from 'node:fs/promises';
import { extname, join, normalize } from 'node:path';
import { FW_OUT } from '../../lib/repo';

export const prerender = false;
const TYPES: Record<string, string> = { '.mjs': 'text/javascript', '.js': 'text/javascript', '.wasm': 'application/wasm', '.json': 'application/json', '.html': 'text/html; charset=utf-8', '.png': 'image/png', '.css': 'text/css' };
export const GET: APIRoute = async ({ params }) => {
  let rel = normalize(params.file ?? '').replace(/^(\.\.(\/|$))+/, '');
  let p = join(FW_OUT, rel);
  if (!p.startsWith(FW_OUT)) return new Response('not found', { status: 404 });
  try {
    if ((await stat(p)).isDirectory()) { p = join(p, 'index.html'); rel = join(rel, 'index.html'); }
    const body = await readFile(p);
    const cache = /manifest\.json$|index\.html$/.test(rel) ? 'no-cache' : 'public, max-age=31536000, immutable';
    return new Response(body, { headers: { 'content-type': TYPES[extname(p)] ?? 'application/octet-stream', 'cache-control': cache } });
  } catch {
    return new Response('not found', { status: 404 });
  }
};
