// API keys for the Facet panels (device side: renderer/core/keys.h, firmware/fw_device.h "API keys").
// The panel gates every POST /fw/*: only its own editor (same-origin browser) passes freely; anything else, the
// workbench included, needs "Allow API access" on AND `Authorization: Bearer <key>`. The workbench sends the
// key it was given for that panel: FACET_PANEL_KEYS = `<host>=<key>`, comma separated (a key comes from
// POST /fw/keys/request {name} on the panel, allowed with a tap on its screen; `curl` lines in /llms.txt). No key for a
// host = no header (the panel answers 401 with how to get one).
//   panelFetch(ip, path, init)  fetch http://<ip><path> with that panel's key
//   agentsOffMessage(body, name) the hint for a 403 {agents:"off"} or 401 {agents:"key"} reply (null otherwise).
const keys = (): Record<string, string> => Object.fromEntries((process.env.FACET_PANEL_KEYS ?? '').split(',').map((x) => x.trim().split('=')).filter((x) => x.length === 2 && x[0] && x[1]));

/** Headers for an agent request to the panel at `ip` ({} when the workbench has no key for it). */
export function panelAuthHeaders(ip: string): Record<string, string> {
  const k = keys()[ip];
  return k ? { Authorization: `Bearer ${k}` } : {};
}

/** fetch(`http://${ip}${path}`) as an agent, with that panel's key. */
export async function panelFetch(ip: string, path: string, init: RequestInit = {}): Promise<Response> {
  return fetch(`http://${ip}${path}`, { ...init, headers: { ...(init.headers as Record<string, string> | undefined), ...panelAuthHeaders(ip) } });
}

/** The workbench hint for a panel that refused the workbench (agents off, or no / a dead key), else null. */
export function agentsOffMessage(body: unknown, panel: string): string | null {
  const a = (body as { agents?: string } | null)?.agents;
  if (a === 'off') return `Turn on Allow API access on ${panel} (its Settings, its editor, or its switch in Home Assistant)`;
  if (a === 'key') return `The workbench has no working key for ${panel}: ask the panel for one (POST /fw/keys/request, tap Allow on it) and put it in FACET_PANEL_KEYS`;
  return null;
}
