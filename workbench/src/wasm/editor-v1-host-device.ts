// Facet editor v1's host on the panel itself (plain HTTP, the panel's own origin: its writes need no key): the device
// bundle (scripts/editor-device.mjs) swaps this in for ./editor-v1-host. The panel's routes (firmware/fw_device.h):
// GET /fw/deck, GET|POST /fw/settings, GET /fw/link, GET /fw/entities (the pickers, through the panel's own Home
// Assistant link), GET /fw/ha?want= (live values for the previews), POST /fw/spec {kind: deck | revert}, POST /fw/act
// {do: ha.connect}.
import type { HaBridge } from './engine';
import type { V1Host, PanelState, SaveOut, Entities, Found } from './editor-v1-host';
export type * from './editor-v1-host';

const j = async (path: string, init?: RequestInit) => {
  const r = await fetch(path, { cache: 'no-store', ...init });
  const body = await r.json().catch(() => null);
  return { ok: r.ok, body };
};
const post = (path: string, body: unknown) => j(path, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
const ENTITY = /^[a-z_]+\.[a-z0-9_]+$/;
let inDeck = new Set<string>();
const entities = (v: unknown, out: Set<string>): Set<string> => {
  if (typeof v === 'string') { if (ENTITY.test(v)) out.add(v); }
  else if (Array.isArray(v)) for (const x of v) entities(x, out);
  else if (v && typeof v === 'object') for (const x of Object.values(v)) entities(x, out);
  return out;
};
// the panel's HA can't unsubscribe: ask only for the entities this deck names, never the sample screens' demo ones
const ha: HaBridge = {
  async states(all, attrs) {
    const ids = all.filter((id) => inDeck.has(id));
    if (!ids.length) return [];
    try {
      const q = `want=${encodeURIComponent(ids.join(','))}${attrs.length ? `&attrs=${encodeURIComponent(attrs.join(','))}` : ''}`;
      const { ok, body } = await j(`/fw/ha?${q}`);
      if (!ok || !body?.values) return null;
      const v = body.values as Record<string, string>;
      return ids.filter((id) => v[id] != null).map((id) => ({
        id, name: v[`${id}|friendly_name`] ?? id, state: v[id],
        attrs: Object.fromEntries(attrs.filter((a) => v[`${id}|${a}`] != null).map((a) => [a, v[`${id}|${a}`]])),
      }));
    } catch { return null; }
  },
  action: async () => undefined,
  ask: async () => null,
};
const said = (b: any, fallback: string) => b?.why ?? b?.error ?? b?.status ?? fallback;

export const host: V1Host = {
  panel: true,
  devices: [document.documentElement.dataset.den === '1.5x' ? 'waveshare' : 'guition'],
  wasmBase: (den) => `/ed/${den}/`,
  ha,
  async load(): Promise<PanelState | null> {
    try {
      const [d, s, l] = await Promise.all([j('/fw/deck'), j('/fw/settings'), j('/fw/link')]);
      if (!d.ok || !d.body?.deck) return null;
      return {
        deck: d.body.deck, settings: s.body?.values ?? {},
        ha: { url: l.body?.url ?? '', hint: l.body?.token ?? '', state: l.body?.state ?? 'off', why: l.body?.why || undefined },
      };
    } catch { return null; }
  },
  async entities(domains): Promise<Entities> {
    // a few domains at a time (a large house's whole list does not fit the 480 panel): per group the panel reads Home
    // Assistant (a few seconds; ask with nothing to send until it has them), then sends the lists and frees them
    const GROUPS = [['light', 'switch', 'media_player', 'cover', 'climate', 'fan', 'lock', 'vacuum'], ['sensor']];
    const groups = GROUPS.map((g) => g.filter((d) => domains.includes(d))).filter((g) => g.length);
    groups.push(domains.filter((d) => !GROUPS.flat().includes(d)));
    const lists: Record<string, { id: string; name: string }[]> = {};
    for (const g of groups.filter((x) => x.length)) {
      const q = encodeURIComponent(g.join(','));
      let st: any = null;
      for (let i = 0; i < 30 && (!st || st.state === 'loading'); i++) {
        if (i) await new Promise((r) => setTimeout(r, 1000));
        try { st = (await j(`/fw/entities?domains=${q}&send=`)).body; } catch { st = null; }
      }
      if (!st || st.state === 'loading') return { state: 'failed', why: 'Home Assistant took too long to answer', lists };
      if (st.state !== 'ok') return { state: st.state, why: st.why, lists };
      try { const { body } = await j(`/fw/entities?domains=${q}&send=${q}&drop=1`); Object.assign(lists, body?.lists ?? {}); } catch { /* that group stays empty */ }
    }
    return { state: 'ok', lists };
  },
  async search(domains, dc, q): Promise<Found> {
    // the panel reads Home Assistant once per search and keeps only the matches (never the whole list): ask until it has them
    const u = `/fw/entities?domains=${encodeURIComponent(domains.join(','))}&dc=${encodeURIComponent(dc)}&q=${encodeURIComponent(q)}`;
    for (let i = 0; i < 40; i++) {
      if (i) await new Promise((r) => setTimeout(r, 400));
      let b: any = null;
      try { b = (await j(u)).body; } catch { b = null; }
      if (b && b.state !== 'loading') return { state: b.state, why: b.why, found: b.found ?? [], more: b.more ?? 0 };
    }
    return { state: 'failed', why: 'Home Assistant took too long to answer', found: [], more: 0 };
  },
  async save(deck, settings, link): Promise<SaveOut> {
    try {
      const d = await post('/fw/spec', { kind: 'deck', spec: deck });
      if (!d.body?.ok) return d.body?.errors?.length ? { ok: false, errors: d.body.errors } : { ok: false, why: said(d.body, 'The panel did not answer.') };
      if (Object.keys(settings).length) {
        const s = await post('/fw/settings', { values: settings });
        if (!s.body?.ok) return { ok: false, why: `The screens are saved; the settings were not: ${said(s.body, 'no answer')}` };
      }
      if (link) {
        const a = await post('/fw/act', { do: 'ha.connect', url: link.url, token: link.token });
        if (!a.body?.ok) return { ok: false, why: `The rest is saved; Home Assistant's link was not: ${said(a.body, 'no answer')}` };
      }
      return { ok: true };
    } catch { return { ok: false, why: 'The panel did not answer.' }; }
  },
  async factory(): Promise<SaveOut> {
    try { const r = await post('/fw/spec', { kind: 'revert', id: 'deck' }); return r.body?.ok ? { ok: true } : { ok: false, why: said(r.body, 'The panel did not answer.') }; } catch { return { ok: false, why: 'The panel did not answer.' }; }
  },
  deckChanged(deck) { inDeck = entities(deck, new Set()); },
};
