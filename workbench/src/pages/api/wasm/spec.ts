import type { APIRoute } from 'astro';
import { readFileSync, readdirSync, existsSync } from 'node:fs';
import { join } from 'node:path';
import { spawn } from 'node:child_process';
import { panels } from '../../../lib/panels';
import { panelFetch } from '../../../lib/panel-auth';
import { REPO } from '../../../lib/repo';

export const prerender = false;

/**
 * Screen specs (renderer/core/spec.h; the workbench's Specs page and editor lab).
 * GET  /api/wasm/spec                  -> {factory: {id: spec}, deck, catalogue, targets}
 *   factory = renderer/specs/*.json (compiled into the firmware), deck = renderer/specs/deck.json
 * GET  /api/wasm/spec?target=<panel>   -> the panel's hello (GET /fw/spec: protocol, catalogue, components, screens, heap)
 * POST /api/wasm/spec {target, kind: screen|deck|revert|check, spec?, id?, dev?}
 *   dev = the Specs page: may replace a built-in screen (kind screen_dev); agents send without it, so a built-in id is
 *   refused (core/spec.h no_built_in)
 *   screen / deck: 1) dry run in WASM at the panel's density (tools/spec.mjs: the panel's own validation + build);
 *   2) the panel's hello: a component the spec uses that the panel lacks = 409 needs_firmware; 3) POST /fw/spec on the
 *   panel (it validates again, builds once off screen, stores, swaps) -> its answer. revert: {kind: revert, id}.
 * Panels = FACET_PANELS (lib/panels.ts); the panel's HTTP API with its key (FACET_PANEL_KEYS, lib/panel-auth.ts).
 */
const R = join(REPO, 'renderer');
const json = (o: unknown, status = 200) => new Response(JSON.stringify(o), { status, headers: { 'content-type': 'application/json', 'cache-control': 'no-store' } });

function dryRun(text: string, den: string, known: string[] = [], dev = false): Promise<any> {
  return new Promise((resolve) => {
    const c = spawn(process.execPath, [join(R, 'tools', 'spec.mjs'), ...(dev ? ['--dev'] : []), '--den', den, '--known', known.join(',')], { cwd: R });
    let out = '', err = '';
    c.stdout.on('data', (d) => (out += d));
    c.stderr.on('data', (d) => (err += d));
    const t = setTimeout(() => c.kill(), 30000);
    c.on('close', () => { clearTimeout(t); try { resolve(JSON.parse(out)); } catch { resolve({ ok: false, errors: [{ code: 'dry_run', msg: (err || out).slice(0, 400) }] }); } });
    c.stdin.end(text);
  });
}
async function panel(host: string, body?: unknown) {
  const r = await panelFetch(host, '/fw/spec', body === undefined ? {} : { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
  const t = await r.text();
  try { return JSON.parse(t); } catch { throw new Error(`panel answered ${r.status}: ${t.slice(0, 160)}`); }
}

export const GET: APIRoute = async ({ url }) => {
  const P = panels(), target = url.searchParams.get('target');
  if (target) {
    const p = P[target];
    if (!p) return json({ error: 'unknown target' }, 400);
    try { return json(await panel(p.host)); } catch (e: any) { return json({ error: String(e?.message ?? e) }, 502); }
  }
  const dir = join(R, 'specs'), factory: Record<string, unknown> = {};
  let deck: unknown = null;
  for (const f of existsSync(dir) ? readdirSync(dir).filter((x) => x.endsWith('.json')) : []) {
    const s = JSON.parse(readFileSync(join(dir, f), 'utf8'));
    if (f === 'deck.json') deck = s; else factory[f.replace(/\.json$/, '')] = s;
  }
  let catalogue: any = null;
  try { const c = JSON.parse(readFileSync(join(R, 'catalogue', 'catalogue.json'), 'utf8')); catalogue = { hash: c.hash, components: Object.keys(c.components), actions: c.actions, budget: c.budget }; } catch { /* no catalogue yet */ }
  return json({ factory, deck, examples: {}, catalogue, targets: Object.fromEntries(Object.values(P).map((p) => [p.name, p.title])) });
};

export const POST: APIRoute = async ({ request }) => {
  let b: any;
  try { b = await request.json(); } catch { return json({ error: 'bad json' }, 400); }
  const kind = String(b?.kind ?? ''), P = panels(), p = P[b?.target];
  if (!p) return json({ error: `target: ${Object.keys(P).join(' | ') || 'none (set FACET_PANELS)'}` }, 400);
  try {
    if (kind === 'revert') return json({ panel: await panel(p.host, { kind: 'revert', id: String(b?.id ?? 'all') }) });
    if (kind !== 'screen' && kind !== 'deck' && kind !== 'check') return json({ error: 'kind: screen | deck | check | revert' }, 400);
    const text = typeof b.spec === 'string' ? b.spec : JSON.stringify(b.spec);
    let hello: any = null;
    try { hello = await panel(p.host); } catch (e) { if (kind !== 'check') throw e; }
    const dev = b?.dev === true && kind !== 'deck';
    const dry = await dryRun(text, p.density, Object.keys(hello?.screens ?? {}), dev);  // a deck may name screens the panel has
    if (kind === 'check') return json({ stage: 'dry_run', dry }, dry.ok ? 200 : 422);
    if (!dry.ok) return json({ stage: 'dry_run', dry }, 422);
    const missing = (dry.components ?? []).filter((c: string) => !(hello?.components ?? []).includes(c));
    if (missing.length) return json({ stage: 'hello', error: 'needs_firmware', missing, dry }, 409);
    const sent = await panel(p.host, { kind: kind === 'deck' ? 'deck' : dev ? 'screen_dev' : 'screen', spec: JSON.parse(text) });
    return json({ stage: 'sent', dry, panel: sent, catalogue: { panel: hello?.catalogue } }, sent?.ok ? 200 : 422);
  } catch (e: any) {
    return json({ stage: 'transport', error: String(e?.message ?? e) }, 502);
  }
};
