// Home Assistant for the workbench, from the environment: HA_URL (http://homeassistant.local:8123) and HA_TOKEN (a
// long-lived access token). Neither set = no Home Assistant: the HA routes answer from the fake Home Assistant
// (renderer/tools/demo_ha.mjs), so the workbench works on any machine. The token stays on the server.
export function configured(): boolean { return !!(process.env.HA_URL && process.env.HA_TOKEN); }
export function env(): { url: string; token: string } {
  if (!configured()) throw new Error('no Home Assistant: set HA_URL and HA_TOKEN');
  return { url: process.env.HA_URL!.replace(/\/+$/, ''), token: process.env.HA_TOKEN! };
}
/** one REST call: `ha('states')`, `ha('services/light/turn_on', {method: 'POST', body})` */
export async function ha(path: string, init: { method?: string; body?: unknown; timeoutMs?: number } = {}) {
  const { url, token } = env();
  const r = await fetch(`${url}/api/${path}`, {
    method: init.method ?? 'GET',
    headers: { Authorization: `Bearer ${token}`, 'Content-Type': 'application/json' },
    body: init.body === undefined ? undefined : JSON.stringify(init.body),
    signal: AbortSignal.timeout(init.timeoutMs ?? 10000),
  });
  if (!r.ok) throw new Error(`Home Assistant answered ${r.status} for ${path}`);
  const t = await r.text();
  return t ? JSON.parse(t) : null;
}
/** WebSocket commands, one connection, results in order */
export async function haWs(cmds: Record<string, unknown>[], timeoutMs = 8000): Promise<unknown[]> {
  const { url, token } = env();
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(url.replace(/^http/, 'ws') + '/api/websocket');
    const out: unknown[] = [];
    const timer = setTimeout(() => { try { ws.close(); } catch {} reject(new Error('Home Assistant took too long')); }, timeoutMs);
    const end = (f: () => void) => { clearTimeout(timer); try { ws.close(); } catch {} f(); };
    ws.onerror = () => end(() => reject(new Error('Home Assistant not reached')));
    ws.onmessage = (ev) => {
      const m = JSON.parse(String(ev.data));
      if (m.type === 'auth_required') ws.send(JSON.stringify({ type: 'auth', access_token: token }));
      else if (m.type === 'auth_invalid') end(() => reject(new Error('Home Assistant refused the token')));
      else if (m.type === 'auth_ok') cmds.forEach((c, i) => ws.send(JSON.stringify({ ...c, id: i + 1 })));
      else if (m.type === 'result') {
        out[m.id - 1] = m.success ? m.result : null;
        if (out.filter((x) => x !== undefined).length === cmds.length) end(() => resolve(out));
      }
    };
  });
}
