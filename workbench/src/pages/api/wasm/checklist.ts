// The release checklist's ticks (/wasm/checklist; the lines in lib/checklist.ts), kept on this machine so a run
// survives a reload and can be ticked from a phone at the panel.
//   GET    /api/wasm/checklist                         -> {ticks: {<key>: <ISO time>}, notes: {<key>: text}}
//   POST   /api/wasm/checklist  {key, on?, note?}       -> the same; on true ticks (now), false clears; note sets text
//   DELETE /api/wasm/checklist                         -> a new run: every tick and note cleared
// Stored in $FW_CACHE/checklist.json (never in the tree).
import type { APIRoute } from 'astro';
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { KEYS } from '../../../lib/checklist';
import { FW_CACHE } from '../../../lib/repo';

export const prerender = false;
type State = { ticks: Record<string, string>; notes: Record<string, string> };
const FILE = join(FW_CACHE, 'checklist.json');
const json = (o: unknown, status = 200) => new Response(JSON.stringify(o), { status, headers: { 'content-type': 'application/json', 'cache-control': 'no-store' } });
const load = (): State => {
  try { const s = JSON.parse(readFileSync(FILE, 'utf8')); return { ticks: s.ticks ?? {}, notes: s.notes ?? {} }; } catch { return { ticks: {}, notes: {} }; }
};
const save = (s: State) => { if (!existsSync(FW_CACHE)) mkdirSync(FW_CACHE, { recursive: true }); writeFileSync(FILE, JSON.stringify(s, null, 2) + '\n'); return s; };

export const GET: APIRoute = () => json(load());

export const POST: APIRoute = async ({ request }) => {
  const b = await request.json().catch(() => null) as { key?: string; on?: boolean; note?: string } | null;
  if (!b?.key || !KEYS.has(b.key)) return json({ error: 'unknown line' }, 400);
  const s = load();
  if (b.on === true) s.ticks[b.key] = new Date().toISOString();
  if (b.on === false) delete s.ticks[b.key];
  if (typeof b.note === 'string') { if (b.note.trim()) s.notes[b.key] = b.note.trim().slice(0, 500); else delete s.notes[b.key]; }
  return json(save(s));
};

export const DELETE: APIRoute = () => json(save({ ticks: {}, notes: {} }));
