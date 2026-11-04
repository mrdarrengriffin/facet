import type { APIRoute } from 'astro';
import { configured, env } from '../../../lib/ha';

export const prerender = false;

/** The WebSocket commands the panel may send: reads only (the Energy screen: HA's Energy dashboard + its statistics). */
const READS = new Set(['energy/get_prefs', 'recorder/statistics_during_period', 'recorder/list_statistic_ids',
  'config/entity_registry/list_for_display', 'config/device_registry/list', 'config/area_registry/list']);

/**
 * POST /api/panel/ha-ws: one Home Assistant WebSocket read for the WASM workbench (core/device.h
 * ha_link::ws: the panel's own link; the workbench connects with its own token, so no token reaches the browser). Body = the command ({type, ...}, no id). Answers HA's result message as HA sent it ({id, type: "result",
 * success, result | error}); the module reads it piece by piece, as the panel would. Only the commands in READS.
 */
export const POST: APIRoute = async ({ request }) => {
  let cmd: Record<string, unknown>;
  try { cmd = await request.json(); } catch { return new Response('not JSON', { status: 400 }); }
  if (!cmd || typeof cmd.type !== 'string' || !READS.has(cmd.type)) return new Response('not a read the panel may make', { status: 403 });
  // no Home Assistant: HA's own answer to a command it cannot run, so the module handles it as the panel would
  if (!configured()) return new Response(JSON.stringify({ id: 1, type: 'result', success: false, error: { code: 'not_configured', message: 'no Home Assistant (HA_URL, HA_TOKEN)' } }), { headers: { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' } });
  const { url, token } = env();
  try {
    const raw = await new Promise<string>((resolve, reject) => {
      const ws = new WebSocket(url.replace(/^http/, 'ws') + '/api/websocket');
      const timer = setTimeout(() => { try { ws.close(); } catch {} reject(new Error('Home Assistant took too long')); }, 20000);
      const end = (f: () => void) => { clearTimeout(timer); try { ws.close(); } catch {} f(); };
      ws.onerror = () => end(() => reject(new Error('Home Assistant not reached')));
      ws.onmessage = (ev) => {
        const text = String(ev.data);
        const m = JSON.parse(text);
        if (m.type === 'auth_required') ws.send(JSON.stringify({ type: 'auth', access_token: token }));
        else if (m.type === 'auth_ok') ws.send(JSON.stringify({ ...cmd, id: 1 }));
        else if (m.type === 'auth_invalid') end(() => reject(new Error('Home Assistant refused the token')));
        else if (m.type === 'result' && m.id === 1) end(() => resolve(text));
      };
    });
    return new Response(raw, { headers: { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' } });
  } catch (e: any) { return new Response(String(e?.message || e), { status: 502 }); }
};
