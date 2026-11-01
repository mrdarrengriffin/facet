// Workbench: the sound list (single source = renderer/tokens/sound.json), for the
// Foundations > Sound page (src/wasm/sound.ts). Read fresh on every request, so an edit shows on reload.
// GET  -> sound.json + `generated` = "what uses what" from the code (renderer/tools/gen_sound_usage.mjs --json, run on
//         every request so the table always matches the components; null + `generatedError` when the script fails).
// Read only: the page's pitch range controls are a play-only preview (changing the token stays a
// code edit he asks for), so nothing here writes sound.json.
import type { APIRoute } from 'astro';
import { readFile } from 'node:fs/promises';
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { REPO } from '../../../lib/repo';

export const prerender = false;
const WASM_DIR = REPO;
const FILE = `${WASM_DIR}/renderer/tokens/sound.json`;
const json = (o: unknown, status = 200) => new Response(JSON.stringify(o), { status, headers: { 'content-type': 'application/json', 'cache-control': 'no-store' } });

export const GET: APIRoute = async () => {
  try {
    const doc = JSON.parse(await readFile(FILE, 'utf8'));
    try {
      const { stdout } = await promisify(execFile)('node', [`${WASM_DIR}/renderer/tools/gen_sound_usage.mjs`, '--json'], { timeout: 10000 });
      doc.generated = JSON.parse(stdout);
    } catch (e) { doc.generated = null; doc.generatedError = String(e).slice(0, 400); }
    return json(doc);
  } catch (e) {
    return json({ error: String(e) }, 500);
  }
};
