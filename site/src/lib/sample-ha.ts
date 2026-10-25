// Sample Home Assistant data for the screen previews: the site has no Home Assistant, so every (entity, attribute)
// pair a built screen binds (fw_ha_wants, core/ha.h) gets a made-up value here, by domain. The previews open with
// sample entities (domain.sample_N, scripts/extract.mjs sampleParams); nothing here belongs to a real home.
import type { Engine } from './engine';
// the README images' demo values (one list for both): named demo entities (the docs Home's cards) and by entity name
import { demoValue } from '../../../renderer/tools/demo_ha.mjs';

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

function value(entity: string, attr: string): string | undefined {
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
  e.dispose = () => { window.clearInterval(t); dispose(); };
}
