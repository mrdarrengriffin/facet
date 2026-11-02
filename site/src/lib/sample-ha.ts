// The docs' fake Home Assistant. The site has no Home Assistant, so every (entity, attribute) pair a built screen binds
// (fw_ha_wants, core/ha.h) gets a made-up value here, by domain. The previews open with sample entities
// (domain.sample_N, scripts/extract.mjs sampleParams); nothing here belongs to a real home. A tap does what it does on a
// panel: the screen sends an action (event "ha": service, entity, data) and HA changes the entity, which every bound
// component then shows; a screen's own events (a ticked checkbox, a mode pick) change what its REACTIONS say (data next
// to the demo values, renderer/tools/demo_ha_examples.mjs). The state lasts while the page is open (no reset).
import type { Engine } from './engine';
// the README images' demo values (one list for both): named demo entities (the docs Home's cards) and by entity name
import { DEMO, demoValue, REACTIONS } from '../../../renderer/tools/demo_ha.mjs';
// the answers a panel gets from Home Assistant (forecasts, calendar events, Energy's reads): the tests' fake uses the same
import { demoGrid, demoReply, demoWs } from '../../../renderer/tools/demo_ha_replies.mjs';

const NAMES: Record<string, string[]> = {
  light: ['Ceiling', 'Lamp', 'Desk light'],
  media_player: ['Kitchen speaker', 'Lounge speaker', 'Study speaker'],
  cover: ['Study blind', 'Bedroom curtain', 'Landing blind'],
  climate: ['Thermostat'],
  weather: ['Home'],
  calendar: ['Family', 'Work'],
};
const VALUES: Record<string, Record<string, string | (() => string)>> = {
  light: { '': 'on', brightness: '166' },
  media_player: { '': 'playing', media_title: 'Slow Tide', media_artist: 'Harbour Lights', volume_level: '0.45', media_duration: '236', media_position: '71', media_position_updated_at: () => new Date().toISOString() },
  cover: { '': 'open', current_position: '60', current_tilt_position: '40', device_class: 'blind' },
  climate: { '': 'heat', current_temperature: '20.5', temperature: '21', hvac_action: 'heating', preset_mode: 'none', min_temp: '7', max_temp: '30', target_temp_step: '0.5' },
  weather: { '': 'partlycloudy', temperature: '14', apparent_temperature: '12', humidity: '71', wind_speed: '14', wind_speed_unit: 'km/h', pressure: '1016', temperature_unit: '°C' },
  calendar: { '': 'off' },
  sensor: {},
};
// Energy's dashboard (demoWs) names these: its price and live power are plain entity states
const G = demoGrid();
const ENERGY: Record<string, Record<string, string>> = {
  [G.price]: { '': '0.275', unit_of_measurement: 'GBP/kWh' }, [G.power]: { '': '420', unit_of_measurement: 'W', device_class: 'power' },
};

// what taps changed: "entity|attr" -> value, for this page (a new engine, e.g. the 720 / 480 switch, is fed these)
const changed = new Map<string, string>();
let live: Engine | null = null;
function value(entity: string, attr: string): string | undefined {
  return changed.get(`${entity}|${attr}`) ?? first(entity, attr);
}
/** HA changes (entity, attr): kept, and put into the engine so every bound component updates */
function put(entity: string, attr: string, v: string) {
  changed.set(`${entity}|${attr}`, v);
  const e = live, m = e?.m as unknown as { _fw_ha_put?: (a: number, b: number, c: number) => void } | undefined;
  if (e && m?._fw_ha_put) e.withStr(entity, (pe) => e.withStr(attr, (pa) => e.withStr(v, (pv) => m._fw_ha_put!(pe, pa, pv))));
}
/** an action a screen sent (core/ha.h act: the whitelisted services) done as HA would */
function call(svc: string, ent: string, data: Record<string, number | string>) {
  const verb = svc.slice(svc.indexOf('.') + 1), s = value(ent, '') ?? '';
  const n = (k: string) => Number(data[k]);
  switch (verb) {
    case 'toggle': put(ent, '', s === 'on' ? 'off' : 'on'); break;
    case 'turn_on':
      put(ent, '', 'on');
      if (data.brightness_pct !== undefined) { put(ent, 'brightness', String(Math.round(n('brightness_pct') * 2.55))); put(ent, 'brightness_pct', String(n('brightness_pct'))); }
      break;
    case 'turn_off': put(ent, '', 'off'); break;
    case 'media_play_pause': put(ent, '', s === 'playing' ? 'paused' : 'playing'); break;
    case 'volume_set': put(ent, 'volume_level', String(n('volume_level'))); break;
    case 'open_cover': put(ent, '', 'open'); put(ent, 'current_position', '100'); break;
    case 'close_cover': put(ent, '', 'closed'); put(ent, 'current_position', '0'); break;
    case 'set_cover_position': put(ent, '', n('position') > 0 ? 'open' : 'closed'); put(ent, 'current_position', String(n('position'))); break;
    case 'set_cover_tilt_position': put(ent, 'current_tilt_position', String(n('tilt_position'))); break;
    case 'set_temperature': put(ent, 'temperature', String(n('temperature'))); break;
    case 'set_hvac_mode': put(ent, '', String(data.hvac_mode)); break;
    case 'set_preset_mode': put(ent, 'preset_mode', String(data.preset_mode)); break;
    case 'lock': put(ent, '', 'locked'); break;
    case 'unlock': put(ent, '', 'unlocked'); break;
    case 'open': put(ent, '', 'open'); break;
    case 'start': put(ent, '', 'cleaning'); break;
    case 'pause': put(ent, '', 'paused'); break;
    case 'return_to_base': put(ent, '', 'returning'); break;
    case 'set_fan_speed': put(ent, 'fan_speed', String(data.fan_speed)); break;
    case 'select_option': put(ent, '', String(data.option)); break;
  }
}
/** what taps changed so far (tests: window.__facetDocs.ha()) */
export const fakeHaState = () => Object.fromEntries(changed);
type Reaction = { ev: string; is?: string; set?: Record<string, string>; add?: Record<string, number> };
const split = (k: string) => { const i = k.indexOf('|'); return i < 0 ? [k, ''] : [k.slice(0, i), k.slice(i + 1)]; };
/** an event from a stage showing screen `screen`: an action goes to the fake HA, then the screen's reactions */
export function fakeHaEvent(screen: string, name: string, detail: string) {
  if (name === 'ha') {
    try { const a = JSON.parse(detail) as { service: string; entity: string; data?: Record<string, number | string> | null }; call(a.service, a.entity, a.data ?? {}); } catch { /* not an action */ }
  }
  for (const r of ((REACTIONS as Record<string, Reaction[]>)[screen] ?? [])) {
    if (r.ev !== name || (r.is && !new RegExp(`^(?:${r.is})$`).test(detail))) continue;
    for (const [k, v] of Object.entries(r.set ?? {})) { const [en, a] = split(k); put(en, a, v.replace(/\$/g, detail)); }
    for (const [k, d] of Object.entries(r.add ?? {})) { const [en, a] = split(k); put(en, a, String(Math.max(0, (parseFloat(value(en, a) ?? '0') || 0) + d))); }
  }
}

function first(entity: string, attr: string): string | undefined {
  const [domain, obj = ''] = entity.split('.');
  if (attr === 'friendly_name') {
    if (!/^sample_\d+$/.test(obj)) return obj.replace(/_/g, ' ').replace(/^./, (x) => x.toUpperCase());  // light.sofa_lamp: Sofa lamp
    const names = NAMES[domain] ?? [];
    const i = Math.max(0, (parseInt(obj.replace(/\D+/g, ''), 10) || 1) - 1);
    return names[i % Math.max(1, names.length)] ?? obj.replace(/_/g, ' ');
  }
  if (ENERGY[entity]) return ENERGY[entity][attr];
  if (!/^sample_\d+$/.test(obj)) {  // a named demo entity (the demo deck, a spec's default): the shared demo values
    const d = demoValue(entity, attr);
    if (d !== undefined) return d;
  }
  const v = VALUES[domain]?.[attr];
  return typeof v === 'function' ? v() : v;
}

/** put sample values for every pair the engine's screens want, as they ask (each pair once per engine) */
/** the link's /api/states: the demo entities, as HA lists them */
const STATES = () => [...Object.keys(DEMO), ...Object.keys(ENERGY), 'weather.home', 'calendar.family']
  .map((id) => ({ entity_id: id, state: first(id, '') ?? 'unknown', attributes: { friendly_name: first(id, 'friendly_name') } }));
type HaMod = {
  _fw_ha_wants?: () => number; _fw_ha_put: (a: number, b: number, c: number) => void;
  _fw_ha_asks?: () => number; _fw_ha_answer: (id: number, ok: number, s: number) => void;
  _fw_ha_gets?: () => number; _fw_ha_chunk: (id: number, s: number, n: number) => void; _fw_ha_got: (id: number, ok: number, s: number) => void;
};
/** the replies a screen waits for: HA actions (a forecast, a calendar's events) and the link's reads (Energy) */
function answer(e: Engine, m: HaMod) {
  for (const a of JSON.parse(e.str(m._fw_ha_asks!()) || '[]') as { id: number; service: string; data?: Record<string, unknown> }[]) {
    const r = demoReply(a.service, a.data ?? {});
    e.withStr(r ? JSON.stringify(r) : '', (p) => m._fw_ha_answer(a.id, r ? 1 : 0, p));
  }
  if (!m._fw_ha_gets) return;
  for (const g of JSON.parse(e.str(m._fw_ha_gets()) || '[]') as (number | { id: number; ws?: unknown })[]) {
    const id = typeof g === 'number' ? g : g.id, ws = typeof g === 'number' ? null : g.ws;
    // a plain id = the link's /api/states read (how many entities it sees); ws = a WebSocket read; get = other REST
    const body = typeof g === 'number' ? STATES() : ws ? demoWs(typeof ws === 'string' ? JSON.parse(ws) : ws) : null;
    if (body) { const b = JSON.stringify(body); e.withStr(b, (p) => m._fw_ha_chunk(id, p, new TextEncoder().encode(b).length)); }
    e.withStr(body ? '' : 'not found', (p) => m._fw_ha_got(id, body ? 1 : 0, p));
  }
}

export function feedSampleHa(e: Engine) {
  const m = e.m as unknown as HaMod;
  if (!m._fw_ha_wants) return;
  live = e;
  // the panel's own Home Assistant link (Energy reads over it): a made-up address, answered here, never fetched
  try { e.withStr('act', (pc) => e.withStr(JSON.stringify({ do: 'ha.connect', url: 'http://homeassistant.local:8123', token: 'demo-token-0000' }), (pa) => (e.m as unknown as { _fw_spec: (a: number, b: number) => number })._fw_spec(pc, pa))); } catch { /* an older build */ }
  const done = new Set<string>();
  const tick = () => {
    if (m._fw_ha_asks) answer(e, m);
    const pairs = JSON.parse(e.str(m._fw_ha_wants!())) as [string, string][];
    for (const [en, a] of pairs) {
      const k = `${en}|${a}`;
      if (done.has(k)) continue;
      done.add(k);
      const v = value(en, a);
      if (v !== undefined) e.withStr(en, (pe) => e.withStr(a, (pa) => e.withStr(v, (pv) => m._fw_ha_put(pe, pa, pv))));
    }
  };
  const t = window.setInterval(() => { try { tick(); } catch { window.clearInterval(t); } }, 250);
  const dispose = e.dispose.bind(e);
  e.dispose = () => { window.clearInterval(t); if (live === e) live = null; dispose(); };
}
