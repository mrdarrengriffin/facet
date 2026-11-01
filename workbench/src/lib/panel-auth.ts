// AI agent auth for the Facet panels (device side: firmware/fw_device.h "AI agents").
// The panel gates every POST /fw/* and every GET /fw/ha: only its own editor (same-origin browser) passes freely;
// anything else, the workbench included, needs "Allow AI agents" on AND `Authorization: Bearer <token>`. The workbench
// acts as an agent with the token the panel publishes to HA (diagnostic text sensor "AI agent token",
// sensor.<panel>_ai_agent_token). The sensor is found by IP: its HA
// device's configuration_url is http://<ip>:80 (ESPHome). No sensor (older firmware) = no header, which old firmware
// does not need.
//   panelFetch(ip, path, init)  fetch http://<ip><path> with the token; on 403 {agents:"token"} drops the cache,
//                               re-reads the sensor and retries once (the token was rotated: "New AI agent token").
//   agentsOffMessage(body, name) the hint for a 403 {agents:"off"} reply (null otherwise).
import { configured, ha } from './ha';

type Hit = { id: string; token: string } | null;
const TTL_MS = 60_000;
const cache = new Map<string, { at: number; hit: Hit }>();

async function lookup(ip: string): Promise<Hit> {
  if (!/^[\d.]+$/.test(ip) || !configured()) return null;
  const template = `{% set ns = namespace(r=none) %}{% for s in states.sensor if s.entity_id.endswith('_ai_agent_token') %}`
    + `{% set u = device_attr(device_id(s.entity_id), 'configuration_url') or '' %}`
    + `{% if u.split('//')[-1].split('/')[0].split(':')[0] == '${ip}' %}{% set ns.r = {'id': s.entity_id, 'token': s.state} %}{% endif %}`
    + `{% endfor %}{{ ns.r | tojson }}`;
  try {
    const r = (await ha('template', { method: 'POST', body: { template }, timeoutMs: 4000 })) as Hit;
    return r && /^[0-9a-f]{32}$/i.test(r.token) ? r : null;  // unavailable / unknown = no header
  } catch {
    return null;  // HA down: try without a token (old firmware still answers)
  }
}

/** Headers for an agent request to the panel at `ip` ({} when it publishes no token). */
export async function panelAuthHeaders(ip: string, fresh = false): Promise<Record<string, string>> {
  const c = cache.get(ip);
  let hit: Hit;
  if (!fresh && c && Date.now() - c.at < TTL_MS) hit = c.hit;
  else { hit = await lookup(ip); cache.set(ip, { at: Date.now(), hit }); }
  return hit ? { Authorization: `Bearer ${hit.token}` } : {};
}

export function dropPanelAuth(ip: string) { cache.delete(ip); }

/** fetch(`http://${ip}${path}`) as an agent; one retry with a re-read token on 403 {agents:"token"}. */
export async function panelFetch(ip: string, path: string, init: RequestInit = {}): Promise<Response> {
  const go = async (fresh: boolean) => fetch(`http://${ip}${path}`, { ...init, headers: { ...(init.headers as Record<string, string> | undefined), ...(await panelAuthHeaders(ip, fresh)) } });
  const r = await go(false);
  if (r.status !== 403) return r;
  const body = await r.clone().json().catch(() => null) as { agents?: string } | null;
  if (body?.agents !== 'token') return r;
  dropPanelAuth(ip);
  return go(true);
}

/** The workbench hint for a panel that has AI agents off, else null. */
export function agentsOffMessage(body: unknown, panel: string): string | null {
  return (body as { agents?: string } | null)?.agents === 'off'
    ? `Turn on Allow AI agents on ${panel} (Settings > AI agents on the panel, or its Allow AI agents switch in Home Assistant)`
    : null;
}
