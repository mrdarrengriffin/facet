// "Show on device" for the workbench (src/wasm/app.ts): the workbench firmware on a panel (esphome/; device side
// firmware/fw_device.h).
//   GET  /api/wasm/show?device=<panel>  -> {device, ip, online, info?, error?, stale: {id: reason}}
//        stale = components whose code differs from what the firmware was built from (core changed, file changed,
//        or not compiled in): the page shows "needs firmware build" for those instead of sending them.
//        Hashes of the current sources come from firmware/bundle.mjs --hashes, the firmware's from GET /fw/info.
//   POST /api/wasm/show?device=<panel>  body {id, props, theme, accent, sound}  -> the panel's POST /fw/show reply
//   device=all (GET and POST): every panel at once in parallel; POST skips the ones that cannot show the item (reasons).
// Panels = FACET_PANELS (lib/panels.ts).
import type { APIRoute } from 'astro';
import { agentsOffMessage, panelFetch } from '../../../lib/panel-auth';
import { panels } from '../../../lib/panels';
import { REPO } from '../../../lib/repo';
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';

export const prerender = false;
const run = promisify(execFile);
type Hashes = { core: string; corefiles?: Record<string, string>; glue?: string; files: Record<string, string>; ids: Record<string, string> };
const json = (o: unknown, status = 200) => new Response(JSON.stringify(o), { status, headers: { 'content-type': 'application/json', 'cache-control': 'no-store' } });

// per item (component / foundation id of the CURRENT sources): exactly the files compiled into it = its own source +
// the core (fw.h, fw_core.cpp, fonts + font header, tokens, colour generator). Device glue (fw_device.h, bundler) is
// not part of an item: a glue change never makes the device show old pixels, so it never blocks the button.
function staleness(cur: Hashes, fw: Hashes) {
  const out: Record<string, string> = {};
  let coreDiff: string[] = [];
  if (fw.core !== cur.core) {
    const a = cur.corefiles ?? {}, b = fw.corefiles ?? {};
    coreDiff = fw.corefiles ? [...new Set([...Object.keys(a), ...Object.keys(b)])].filter((f) => a[f] !== b[f]) : ['core (firmware predates per-file hashes)'];
  }
  for (const [id, file] of Object.entries(cur.ids)) {
    const why: string[] = [];
    if (!fw.ids?.[id]) why.push(`${id} is not compiled into the firmware`);
    else {
      if (fw.ids[id] !== file) why.push(`${id} moved to ${file}`);
      if (fw.files?.[file] !== cur.files[file]) why.push(`${file} changed`);
      if (coreDiff.length) why.push(`${coreDiff.join(', ')} changed`);
    }
    if (why.length) out[id] = why.join('; ');
  }
  return out;
}

const TARGETS = () => Object.keys(panels()).join(', ') || 'none (set FACET_PANELS)';

// status of one target (the GET reply for one device)
async function statusOf(device: string): Promise<{ code: number; body: Record<string, unknown> }> {
  const ok = (body: Record<string, unknown>, code = 200) => ({ code, body });
  const p = panels()[device];
  if (!p) return ok({ device, online: false, error: `not a configured panel (${TARGETS()})`, stale: {} });
  const ip = p.host;
  const [cur, info] = await Promise.all([
    run('node', [`${REPO}/firmware/bundle.mjs`, '--all', '--density', p.density, '--hashes']).then((r) => JSON.parse(r.stdout) as Hashes).catch(() => null),
    fetch(`http://${ip}/fw/info`, { signal: AbortSignal.timeout(2500) }).then((r) => (r.ok ? r.json() : Promise.reject(new Error(`HTTP ${r.status}`)))).catch((e) => ({ error: String(e?.message ?? e) })),
  ]);
  if (!cur) return ok({ device, ip, online: false, error: 'could not hash the sources' }, 500);
  if ('error' in info) return ok({ device, ip, online: false, error: `no workbench firmware answering (${info.error})`, stale: Object.fromEntries(Object.keys(cur.ids).map((id) => [id, 'device offline / not on the workbench firmware'])) });
  return ok({ device, ip, online: true, info: { board: info.board, density: info.density, built: info.built, core: info.core, glue_current: info.glue === cur.glue, shown: info.shown, heap: info.heap, uptime_s: info.uptime_s }, stale: staleness(cur, info) });
}
const ALL = () => Object.keys(panels());
// device=all: every target in parallel, {device: 'all', all: true, devices: {<target>: <its GET reply>}}
export const GET: APIRoute = async ({ url }) => {
  const device = url.searchParams.get('device') ?? '';
  if (device === 'all') {
    const r = await Promise.all(ALL().map(async (d) => [d, (await statusOf(d)).body] as const));
    return json({ device: 'all', all: true, devices: Object.fromEntries(r) });
  }
  const r = await statusOf(device);
  return json(r.body, r.code);
};

// device=all: fan out in parallel to every target that can show `id` now (online, firmware current for it; browser
// targets while a device page is open); the rest are skipped with their reason. {ok: shown on at least one, results: {<target>: {ok, ms?,
// clients?, error?, skipped?}}}. One stale or offline panel never blocks the others.
async function showAll(body: string) {
  let id = '';
  try { id = String(JSON.parse(body)?.id ?? ''); } catch { return json({ ok: false, error: 'bad JSON' }, 400); }
  if (!id) return json({ ok: false, error: 'id required' }, 400);
  const results = Object.fromEntries(await Promise.all(ALL().map(async (d) => {
    const st = (await statusOf(d)).body as { online?: boolean; browser?: boolean; error?: string; stale?: Record<string, string>; info?: { clients?: number } };
    if (!st.online) return [d, { ok: false, skipped: `offline: ${st.error ?? 'no answer'}` }] as const;
    if (st.stale?.[id]) return [d, { ok: false, skipped: `needs firmware build (${st.stale[id]})` }] as const;
    const r = await POST({ url: new URL(`http://workbench/api/wasm/show?device=${d}`), request: new Request('http://workbench/', { method: 'POST', body }) } as Parameters<APIRoute>[0]);
    return [d, await (r as Response).json()] as const;
  })));
  return json({ ok: Object.values(results).some((r) => (r as { ok?: boolean }).ok), results });
}

export const POST: APIRoute = async ({ url, request }) => {
  const device = url.searchParams.get('device') ?? '';
  if (device === 'all') return showAll(await request.text());
  const p = panels()[device];
  if (!p) return json({ ok: false, error: `device must be one of ${TARGETS()}` }, 400);
  const ip = p.host;
  try {
    // agent auth: the panel's AI agent token from HA; 403 agents:"off" -> say how to turn it on
    const r = await panelFetch(ip!, '/fw/show', { method: 'POST', headers: { 'content-type': 'application/json' }, body: await request.text(), signal: AbortSignal.timeout(8000) });
    const body = await r.json();
    const off = agentsOffMessage(body, p.title);
    return json(off ? { ...body, ok: false, error: off } : body, r.status);
  } catch (e) {
    return json({ ok: false, error: `device ${device} (${ip}) did not answer: ${String((e as Error)?.message ?? e)}` }, 502);
  }
};
