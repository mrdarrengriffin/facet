// Demo Home Assistant values, shared by the README images (tools/readme_assets.mjs) and the docs site's previews
// (site/src/lib/sample-ha.ts): neither has a Home Assistant, so every (entity, attribute) a screen binds gets a
// believable value (a track playing, the ceiling light at 70 %, temperatures, doors closed, battery levels). Made-up
// devices, not a real home. Plain JS (one sibling import): node and the site's bundler both load it.
import { EXAMPLES } from './demo_ha_examples.mjs';

/** values for the demo deck's own entities (renderer/specs/deck.json and the specs' defaults) */
export const DEMO = {
  ...EXAMPLES,  // the docs' "Make screens with AI" examples (renderer/specs/ai-examples)
  'light.living_room_ceiling': { '': 'on', brightness: '179' },
  'light.sofa_lamp': { '': 'on', brightness: '115' },
  'light.reading_lamp': { '': 'off' },
  'light.pantry': { '': 'on', brightness: '230' },
  'light.pantry_strip': { '': 'off' },
  'media_player.sitting_room_speaker': { '': 'playing', media_title: 'Harvest Moon', media_artist: 'Neil Young', volume_level: '0.45', media_position: '118', media_duration: '303' },
  'media_player.kitchen_hub': { '': 'paused', media_title: 'Morning Edition', media_artist: 'Radio 4', volume_level: '0.3', media_position: '640', media_duration: '1800' },
  'binary_sensor.fridge_door': { '': 'off', device_class: 'door' },
  'sensor.dishwasher_power': { '': '412', unit_of_measurement: 'W', device_class: 'power' },
};

/** the demo value of (entity, attribute), else undefined (by entity name: temperature, humidity, battery, power,
 *  door / window; then by domain) */
export function demoValue(e, a) {
  const d = DEMO[e]?.[a];
  if (d !== undefined) return d;
  if (a === 'media_position_updated_at') return new Date().toISOString();
  if (/temperature/.test(e)) return { '': '18.5', unit_of_measurement: '°C', device_class: 'temperature' }[a];
  if (/humidity/.test(e)) return { '': '52', unit_of_measurement: '%', device_class: 'humidity' }[a];
  if (/battery/.test(e)) return { '': '82', unit_of_measurement: '%', device_class: 'battery' }[a];
  if (/power/.test(e)) return { '': '412', unit_of_measurement: 'W', device_class: 'power' }[a];
  if (/door|window/.test(e)) return { '': 'off', device_class: /window/.test(e) ? 'window' : 'door' }[a];
  if (/^(light|switch|fan)\./.test(e)) return { '': 'on', brightness: '204' }[a];
  if (/^cover\./.test(e)) return { '': 'open', current_position: '60' }[a];
  if (/^climate\./.test(e)) return { '': 'heat', temperature: '21', current_temperature: '19.5' }[a];
  return undefined;
}
