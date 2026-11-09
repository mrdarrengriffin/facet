// Facet editor v1's host on the panel itself (plain HTTP, the panel's own origin: its writes need no key): the device
// bundle (scripts/editor-device.mjs) swaps this in for ./editor-v1-host. The panel's routes (firmware/fw_device.h):
// GET /fw/deck, GET|POST /fw/settings, GET /fw/link, GET /fw/entities (the pickers, through the panel's own Home
// Assistant link), GET /fw/ha?want= (live values for the previews), POST /fw/spec {kind: deck | revert}, POST /fw/act
// {do: ha.connect}.
import type { HaBridge } from './engine';
import type { V1Host, PanelState, SaveOut, Entities, Index, Row } from './editor-v1-host';
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
  mirror: true,  // the panel's own feeds and timers (panel.*): what the glass shows, never fetched here
  async states(all, attrs) {
    const ids = all.filter((id) => inDeck.has(id) || id.startsWith('panel.'));
    if (!ids.length) return [];
    try {
      const q = `want=${encodeURIComponent(ids.join(','))}${attrs.length ? `&attrs=${encodeURIComponent(attrs.join(','))}` : ''}`;
      const { ok, body } = await j(`/fw/ha?${q}`);
      if (!ok || !body?.values) return null;
      const v = body.values as Record<string, string>;
      // a panel.* source: every field the panel holds for it; an entity: the attributes asked for
      const fields = (id: string) => id.startsWith('panel.')
        ? Object.keys(v).filter((k) => k.startsWith(`${id}|`) && k !== `${id}|friendly_name`).map((k) => k.slice(id.length + 1))
        : attrs.filter((a) => v[`${id}|${a}`] != null);
      return ids.filter((id) => v[id] != null).map((id) => ({  // a source the panel has not run yet: still loading
        id, name: v[`${id}|friendly_name`] ?? id, state: v[id],
        attrs: Object.fromEntries(fields(id).map((a) => [a, v[`${id}|${a}`]])),
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
  async index(domains): Promise<Index> {
    // the panel reads Home Assistant once (a few seconds in a large house) and sends one line per entity, then keeps
    // nothing: "id\tname\tdc\tstate\tunit" lines, then "#ok\t<left out>"; "#loading" = ask again
    const u = `/fw/entities?index=1&domains=${encodeURIComponent(domains.join(','))}`;
    for (let i = 0; i < 40; i++) {
      if (i) await new Promise((r) => setTimeout(r, 500));
      let t = '';
      try { t = await (await fetch(u, { cache: 'no-store' })).text(); } catch { continue; }
      if (t.startsWith('#loading')) continue;
      if (t.startsWith('#off')) return { state: 'off', rows: [], left: 0 };
      if (t.startsWith('#failed')) return { state: 'failed', why: t.split('\t')[1]?.trim() || 'no answer', rows: [], left: 0 };
      const lines = t.split('\n'), rows: Row[] = [];
      let left = 0;
      for (const l of lines) {
        if (!l) continue;
        if (l.startsWith('#ok')) { left = Number(l.split('\t')[1]) || 0; continue; }
        const [id, name, dc, st, unit] = l.split('\t');
        rows.push({ id, name: name || id, dc: dc || undefined, st, unit: unit || undefined });
      }
      return { state: 'ok', rows, left };
    }
    return { state: 'failed', why: 'Home Assistant took too long to answer', rows: [], left: 0 };
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
