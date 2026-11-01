// Facet editor host on the panel itself (plain HTTP; Guition 1x, Waveshare 1.5x): the device bundle
// (scripts/editor-device.mjs --device <id>) swaps this in for ./editor-host and marks the page <html data-device="<id>">,
// so one bundle serves either panel. The panel serves the editor + its own renderer (/ed/<den>/) from flash (facet-wasm
// firmware/fw_device.h "web UI": /, /ed/*), its deck = GET /fw/deck, Save = POST /fw/spec {kind: deck} (validated,
// stored in NVS with the last good copy, Home rebuilt). No HA yet: no entity options, no found sources ("set up
// later"), the preview's bound components keep their sample data.
import type { EditorHost } from './editor-host';
export type { Option, Deck, DeckPage, DeckEntry, Discovery, PanelStatus, SaveResult, EditorHost } from './editor-host';

const j = async (path: string, init?: RequestInit) => {
  const r = await fetch(path, { cache: 'no-store', ...init });
  const body = await r.json().catch(() => null);
  return { ok: r.ok, body };
};

// the entities the panel's deck names (cards' e, screens' params, any entity-id string in it): the only ones the editor
// asks the panel for. Every built screen binds its spec's defaults too (a page with no params = the sample screen's demo
// entities: light.living_room_ceiling, weather.forecast_home...), and the panel subscribes to each wanted pair for good
// (HA can't unsubscribe), so those stay out.
const ENTITY = /^[a-z_]+\.[a-z0-9_]+$/;
let inDeck = new Set<string>();
const entities = (v: unknown, out: Set<string>): Set<string> => {
  if (typeof v === 'string') { if (ENTITY.test(v)) out.add(v); }
  else if (Array.isArray(v)) for (const x of v) entities(x, out);
  else if (v && typeof v === 'object') for (const x of Object.values(v)) entities(x, out);
  return out;
};

// the preview size: the panel's density (the page's data-den; the board id is data-device)
const here = typeof document !== 'undefined' && document.documentElement.dataset.den === '1.5x' ? 'waveshare' : 'guition';

export const deviceHost: EditorHost = {
  devices: [here as EditorHost['devices'][number]],
  live: true,
  saveLabel: 'Save to panel',
  wasmBase: (den) => `/ed/${den}/`,
  // live values from the panel's own HA subscriptions (GET /fw/ha?want= subscribes first; a new entity fills in a poll later)
  ha: {
    pollMs: 5000,
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
  },
  async discover() {
    try {
      const { ok, body } = await j('/fw/deck');
      if (!ok || !body?.deck) return null;
      const hi = await j('/fw/spec').catch(() => ({ ok: false, body: null as any }));
      const sent = Object.entries((hi.ok && hi.body?.screens) || {}).filter(([, v]: [string, any]) => v?.sent).map(([id, v]: [string, any]) => ({ id, name: v.name ?? id, role: v.role }));
      return { deck: body.deck, options: {}, screens: {}, sent };
    } catch { return null; }
  },
  async status() {
    try { const { ok } = await j('/fw/info'); return { online: ok, ip: location.host }; } catch { return { online: false, ip: location.host }; }
  },
  async save(_device, deck) {
    try {
      const { ok, body } = await j('/fw/spec', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ kind: 'deck', spec: deck }) });
      if (ok && body?.ok) return { sent: true, note: body.status === 'unchanged' ? 'The panel already had this deck.' : `Applied: rev ${body.rev}.` };
      const e = body?.errors?.[0];
      return { sent: false, note: e ? `Refused: ${e.msg ?? e.message ?? e.code} (${e.path ?? ''})` : (body?.error ?? body?.status ?? 'The panel did not answer.') };
    } catch { return { sent: false, note: 'The panel did not answer.' }; }
  },
  async removeSent(_device, id) {
    try { const { ok, body } = await j('/fw/spec', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ kind: 'revert', id }) }); return ok && !!body?.ok; } catch { return false; }
  },
  deckChanged(deck) { inDeck = entities(deck, new Set()); },
  saveNote: 'Sends the deck to this panel: applied in one step, kept across reboots, the last one kept to roll back.',
};

export const host: EditorHost = deviceHost;
