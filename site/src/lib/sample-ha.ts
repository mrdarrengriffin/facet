// The docs' fake Home Assistant. The site has no Home Assistant, so every (entity, attribute) pair a built screen binds
// (fw_ha_wants, core/ha.h) gets a made-up value here, by domain. The previews open with sample entities
// (domain.sample_N, scripts/extract.mjs sampleParams); nothing here belongs to a real home. A tap does what it does on a
// panel: the screen sends an action (event "ha": service, entity, data) and HA changes the entity, which every bound
// component then shows; a screen's own events (a ticked checkbox, a mode pick) change what its REACTIONS say (data next
// to the demo values, renderer/tools/demo_ha_examples.mjs). The state lasts while the page is open (no reset).
import type { Engine } from './engine';
// the README images' demo values (one list for both): named demo entities (the docs Home's cards) and by entity name
import { demoValue, REACTIONS } from '../../../renderer/tools/demo_ha.mjs';

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
  weather: { '': 'partlycloudy', temperature: '14', apparent_temperature: '12' },
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
  if (!/^sample_\d+$/.test(obj)) {  // a named demo entity (the demo deck, a spec's default): the shared demo values
    const d = demoValue(entity, attr);
    if (d !== undefined) return d;
  }
  const v = VALUES[domain]?.[attr];
  return typeof v === 'function' ? v() : v;
}

/** put sample values for every pair the engine's screens want, as they ask (each pair once per engine) */
export function feedSampleHa(e: Engine) {
  const m = e.m as unknown as { _fw_ha_wants?: () => number; _fw_ha_put: (a: number, b: number, c: number) => void };
  if (!m._fw_ha_wants) return;
  live = e;
  const done = new Set<string>();
  const tick = () => {
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
