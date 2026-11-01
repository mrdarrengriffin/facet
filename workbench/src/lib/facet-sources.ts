// Screen sources: discovery against this Home Assistant (Settings > Screens in the workbench). The public source
// catalogue (renderer/sources/sources.json: per screen its sources, how to detect each, which params each fills) + the
// Home Assistant's states and entity registry -> per screen: which sources are found, the source in use (the first
// found one), the params it fills (entity lists, one entity), the entity options per domain for pickers, and the deck =
// the factory deck with each screen's source + params. No Home Assistant configured: built-in sources only.
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { configured, ha, haWs } from './ha';
import { REPO } from './repo';

const R = join(REPO, 'renderer');
type Fill = { domain: string; integration?: string; list?: boolean };
type Source = { id: string; kind: 'builtin' | 'ha' | 'direct'; name: string; detect?: { integration?: string; domain?: string; energy?: boolean }; setup?: string[]; fills?: Record<string, Fill> };
type Catalogue = { kinds: Record<string, string>; screens: Record<string, { needs: string | null; sources: Source[] }> };
type State = { entity_id: string; state: string; attributes: Record<string, any> };
export type Found = { found: boolean; evidence: string; count?: number };
export type ScreenSources = { source: string | null; found: Record<string, Found>; params: Record<string, string>; fills: Record<string, Record<string, string>> };
export type Discovery = { at: string; screens: Record<string, ScreenSources>; options: Record<string, { id: string; name: string }[]>; deck: unknown; specs: Record<string, unknown> };

const readJson = (p: string) => JSON.parse(readFileSync(p, 'utf8'));
let reg: { at: number; list: { entity_id: string; platform: string; disabled_by: string | null }[] } = { at: 0, list: [] };
async function registry() {
  if (configured() && Date.now() - reg.at > 10 * 60_000) {
    try { const [list] = (await haWs([{ type: 'config/entity_registry/list' }])) as any[][]; reg = { at: Date.now(), list }; } catch { /* keep */ }
  }
  return reg.list;
}
let cache: { at: number; value: Discovery | null } = { at: 0, value: null };

export async function discover(): Promise<Discovery> {
  if (cache.value && Date.now() - cache.at < 30_000) return cache.value;
  const cat = readJson(join(R, 'sources', 'sources.json')) as Catalogue;
  const states = configured() ? ((await ha('states', { timeoutMs: 8000 })) as State[]) : [];
  const by = Object.fromEntries(states.map((s) => [s.entity_id, s]));
  const live = (id: string) => by[id] && !['unavailable', 'unknown'].includes(by[id].state);
  const regs = (await registry()).filter((e) => !e.disabled_by);
  let energy = false;
  if (configured()) try { const [p] = (await haWs([{ type: 'energy/get_prefs' }])) as any[]; energy = !!p?.energy_sources?.length; } catch { /* no energy */ }
  const match = (f: { domain?: string; integration?: string }) => {
    const ids = f.integration ? regs.filter((e) => e.platform === f.integration).map((e) => e.entity_id) : states.map((s) => s.entity_id);
    return ids.filter((id) => (!f.domain || id.startsWith(`${f.domain}.`)) && live(id)).sort((a, b) => String(by[a]?.attributes?.friendly_name ?? a).localeCompare(String(by[b]?.attributes?.friendly_name ?? b)));
  };
  const factory: Record<string, any> = {};
  for (const id of Object.keys(cat.screens)) { try { factory[id] = readJson(join(R, 'specs', `${id}.json`)); } catch { /* not ported */ } }
  const screens: Record<string, ScreenSources> = {};
  for (const [id, sc] of Object.entries(cat.screens)) {
    const found: Record<string, Found> = {};
    for (const s of sc.sources) {
      if (s.kind === 'builtin') found[s.id] = { found: true, evidence: 'built in' };
      else if (s.kind === 'direct') found[s.id] = s.setup?.length ? { found: false, evidence: `needs ${s.setup.join(' + ')}` } : { found: true, evidence: 'no setup' };
      else if (s.detect?.energy) found[s.id] = { found: energy, evidence: energy ? 'Energy dashboard set up' : 'no Energy dashboard' };
      else { const m = match(s.detect ?? {}); found[s.id] = { found: m.length > 0, count: m.length, evidence: m.length ? `found ${m.length}` : 'not in Home Assistant' }; }
    }
    // per found source, the params it fills (the editor refills them when that source is picked)
    const fills: Record<string, Record<string, string>> = {};
    for (const s of sc.sources) {
      if (!found[s.id]?.found || !s.fills) continue;
      const p: Record<string, string> = {};
      for (const [k, f] of Object.entries(s.fills)) {
        const m = match(f);
        if (!m.length) continue;
        if (f.list) { p[k] = m.join(','); continue; }
        const def = factory[id]?.params?.[k]?.default ?? factory[id]?.params?.[k];
        p[k] = typeof def === 'string' && m.includes(def) ? def : m[0];
      }
      fills[s.id] = p;
    }
    const use = sc.sources.find((s) => found[s.id]?.found) ?? null;
    const params: Record<string, string> = { ...(use ? fills[use.id] : {}) };
    screens[id] = { source: use?.id ?? null, found, params, fills };
  }
  // entity options per domain (the workbench's pickers), for every typed entity / entities param
  const domains = new Set<string>();
  for (const f of Object.values(factory)) for (const p of Object.values(f?.params ?? {}) as any[]) if (p?.domain) domains.add(p.domain);
  for (const id of Object.keys(by)) domains.add(id.split('.')[0]);  // Home card pickers: every domain (the Entity tile takes any entity)
  const options: Discovery['options'] = {};
  for (const d of domains) options[d] = match({ domain: d }).map((id) => ({ id, name: String(by[id]?.attributes?.friendly_name ?? id) }));
  // the deck: the factory deck + each page's source and params (deck v2: pages by kind; v1: screens by id)
  const deck = readJson(join(R, 'specs', 'deck.json'));
  const specs: Record<string, unknown> = {};
  for (const e of (deck.pages ?? deck.screens) as any[]) {
    const s = screens[e.kind ?? e.id];
    if (!s) continue;
    if (s.source) e.source = s.source;
    if (Object.keys(s.params).length) e.params = s.params;
  }
  const value: Discovery = { at: new Date().toISOString(), screens, options, deck, specs };
  cache = { at: Date.now(), value };
  return value;
}
