// Release checklist dry-run (PRODUCT / plan 6.0, the lines an agent can run): every shipped screen on Home Assistant
// with its entities missing, with them unavailable, and with the network pulled then restored. Missing: no crash, no
// sample word. Unavailable / network pulled: none of the texts that came from the data stays on screen (no stale paint);
// restored: they come back without a rebuild. "From the data" = texts shown with the data that are not shown with
// the entities missing. No sample word with the entities missing or present is the leak rule's (tests/rules/leak.mjs,
// gate). Home Assistant is faked (tools/functional.mjs FakeHA): nothing real is touched.
import fs from 'node:fs';
import { DEMO } from '../../tools/demo_ha.mjs';
const SCHEMA = JSON.parse(fs.readFileSync(new URL('../../catalogue/schema.json', import.meta.url)));
const W = 'weather.test_home';  // the demo set has no weather entity: the test gives one (NOT a real entity)
// with entity data in the fake HA; weather_days (its forecast) and calendar (events) by a service reply, energy (the dashboard over a WebSocket),
// moon and timer (the panel's own) reach only their empty / loading states here: their recovery is their own tests'
const DATA = ['clock', 'weather', 'media', 'light', 'living_room'];
const SHIPPED = ['clock', 'weather', 'weather_days', 'calendar', 'media', 'energy', 'light', 'living_room', 'moon', 'timer'];
const props = (id) => {
  const p = { screen: id };
  const ps = SCHEMA.screens[id]?.params ?? {};
  if (ps.source?.options?.includes('ha')) p.source = 'ha';
  for (const [k, v] of Object.entries(ps)) {  // each entity param: the first demo entity of its domain (as the leak rule)
    if (v.type !== 'entity' && v.type !== 'entities') continue;
    const e = Object.keys(DEMO).find((x) => x.startsWith(`${v.domain}.`)) ?? (v.domain === 'weather' ? W : null);
    if (e) p[k] = e;
  }
  return p;
};
async function show(t, id, missing) {
  const get = t.ha.get.bind(t.ha);
  if (missing) t.ha.get = () => undefined;  // Home Assistant has none of the entities the screen asks for
  try { await t.mount('spec', props(id)); } finally { t.ha.get = get; }
  t.b.settle(1500);
  return t.texts();
}
const tests = SHIPPED.map((id) => ({
  name: `${id}: missing, unavailable, network pulled and restored`,
  async run(t) {
    const none = await show(t, id, true);  // built (a crash throws) and draws its empty / loading state
    t.ok(none.length > 0, `${id} missing: the screen draws (${none.join(' | ')})`);
    t.log.push(`${id} missing: ${none.join(' | ')}`);
    t.ha.s[W] = { '': 'cloudy', temperature: '10.2', humidity: '81', wind_speed: '12' };
    const ok = await show(t, id, false);
    const data = ok.filter((x) => !none.includes(x));
    const wants = [...new Set(t.ha.wants().map(([e]) => e))];
    if (DATA.includes(id)) t.ok(data.length > 0, `${id}: its data shows (else this test checks nothing)`);
    t.log.push(`${id}: ${wants.length} entities, data texts: ${data.join(' | ')}`);
    // unavailable: every entity it reads
    for (const e of wants) t.ha.unavailable(e);
    const un = t.texts();
    t.eq(data.filter((x) => un.includes(x)), [], `${id} unavailable: no stale paint`);
    for (const e of wants) if (e === W) t.ha.s[W] = { '': 'cloudy', temperature: '10.2', humidity: '81', wind_speed: '12' }; else delete t.ha.s[e];
    t.ha.push(); t.b.settle(1500);
    t.eq(data.filter((x) => !t.texts().includes(x)), [], `${id} available again: the data back`);
    // the network pulled, then restored
    t.ha.disconnect();
    const off = t.texts();
    t.eq(data.filter((x) => off.includes(x)), [], `${id} network pulled: no stale paint`);
    t.ha.reconnect();
    t.eq(data.filter((x) => !t.texts().includes(x)), [], `${id} network restored: the data back, no rebuild`);
  },
}));
export default { id: 'release_states', family: 'device', about: 'Release dry-run: every shipped screen with its entities missing, unavailable, the network pulled and restored.', tests };
