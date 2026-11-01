import type { APIRoute } from 'astro';
import { configured, env } from '../../../lib/ha';

export const prerender = false;

/**
 * GET /api/panel/ha-states: Home Assistant's GET /api/states, streamed through as it arrives, for the WASM workbench
 * (core/device.h ha_link: the panel's own link; the workbench fetches with its own token, so no token reaches the
 * browser). The module reads it piece by piece, as the panel's HTTP task does. Read-only.
 */
export const GET: APIRoute = async () => {
  if (!configured()) return new Response('[]', { headers: { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' } });  // no Home Assistant: no states
  const { url, token } = env();
  try {
    const r = await fetch(`${url}/api/states`, { headers: { Authorization: `Bearer ${token}` }, signal: AbortSignal.timeout(20000) });
    if (!r.ok || !r.body) return new Response(`HA ${r.status}`, { status: 502 });
    return new Response(r.body, { headers: { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' } });
  } catch (e: any) { return new Response(String(e?.message || e), { status: 502 }); }
};
