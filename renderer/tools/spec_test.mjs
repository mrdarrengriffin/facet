#!/usr/bin/env node
// Screen spec store test (core/spec.h): on the WASM build, headless. Power loss / reboot in the middle
// of an apply (the memory storage of core/main.cpp loses every write from the nth on; "torn" = that write lands cut in
// half), then a reboot: the live spec is always an intact one (the new, the last good or factory), it always builds,
// and a torn record never shows. Also: invalid specs are refused with their error codes, the same rev is unchanged.
// Usage: node tools/spec_test.mjs [--build <dir with 1x/ 1.5x/>]   exit 1 on a failure
import { readFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const HERE = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const env = (() => { try { return Object.fromEntries(readFileSync(join(HERE, '..', 'local.env'), 'utf8').split('\n').map((l) => l.match(/^([A-Z_]+)=(.*)$/)).filter(Boolean).map((m) => [m[1], m[2]])); } catch { return {}; } })();
const i = process.argv.indexOf('--build');
const BUILD = i > 0 ? process.argv[i + 1] : process.env.FW_OUT || env.FW_OUT || join(HERE, 'out');
const F = (await import(pathToFileURL(join(BUILD, '1.5x', 'fw.mjs')).href)).default;
const m = await F();
m._fw_init();
const stage = m._fw_stage(720, 720);
let t = 0;
const tick = (n = 10) => { for (let k = 0; k < n; k++) { t += 16; m._fw_tick(t); } m._fw_events(); };
const S = (p, f) => { const q = m.stringToNewUTF8(p); try { return f(q); } finally { m._free(q); } };
const call = (fn, c, a = '') => S(c, (pc) => S(a, (pa) => JSON.parse(m.UTF8ToString(fn(pc, pa)))));
const spec = (c, a) => call(m._fw_spec, c, a), store = (c, a) => call(m._fw_spec_storage, c, a);
const render = (id, props) => S(id, (pc) => S(JSON.stringify(props), (pp) => m.UTF8ToString(m._fw_render(stage, pc, pp, 0, -1))));
let fails = 0;
const ok = (cond, what) => { if (!cond) fails++; console.log(`${cond ? 'pass' : 'FAIL'}  ${what}`); };

const factory = JSON.parse(spec('list').screens.living_room);
const rev = (n) => JSON.stringify({ ...factory, rev: n, name: `Living room r${n}`, nodes: factory.nodes.slice(0, Math.max(1, factory.nodes.length - n + 1)) });
const live = () => JSON.parse(spec('list').screens.living_room);
const builds = () => render('device', { start: 'living_room' }) === '' && (tick(30), true);

// power loss at every write of an apply, both on a clean store (factory below) and over a stored rev 2 (last good)
for (const base of [0, 2]) for (const mode of ['drop', 'torn']) for (let n = 0; n < 4; n++) {
  store('reset');
  if (base) ok(spec('screen', rev(base)).status === 'applied', `base rev ${base} applied`);
  store('fault', `${n} ${mode}`);
  spec('screen', rev(3));
  const log = store('reboot').log ?? [];
  const r = live().rev;
  const want = [base || 1, 3];
  ok(want.includes(r) && builds(), `power lost at write ${n} (${mode}) over ${base ? 'rev 2' : 'factory'}: live rev ${r} (one of ${want.join(' / ')}), builds${log.length ? `; boot: ${log.join('; ')}` : ''}`);
}
// a torn current copy falls back to the last good one
store('reset');
spec('screen', rev(2));
spec('screen', rev(3));
store('fault', '0 torn');
spec('screen', rev(4));  // its first write (the last good copy = rev 3) lands torn; nothing after
store('reboot');
ok([3].includes(live().rev), `torn last-good write: live rev ${live().rev} (3)`);

// refusals and no-ops
store('reset');
const bad = (s) => spec('screen', JSON.stringify(s));
const codes = (r) => (r.errors ?? []).map((e) => e.code);
ok(codes(bad({ ...factory, rev: 9, colour: 'red' })).includes('style_key'), 'a style key is refused (style_key)');
ok(codes(bad({ ...factory, rev: 9, nodes: [{ c: 'no_such' }] })).includes('unknown_component'), 'an unknown component is refused');
ok(codes(bad({ ...factory, rev: 9, nodes: [{ c: 'list_row', on: { tap: { do: 'shell.run' } } }] })).includes('bad_action'), 'an action outside the whitelist is refused');
ok(codes(bad({ ...factory, rev: 9, nodes: [{ c: 'dial', on: { toggle: { do: 'nav.open', screen: 'x' } } }] })).includes('not_allowed'), 'an action the component event does not allow is refused');
ok(codes(bad({ ...factory, rev: 9, nodes: Array(60).fill({ c: 'list_row' }) })).includes('over_budget'), 'over the node budget is refused');
ok(codes(spec('screen', '{"v":1,')).includes('bad_json'), 'half a spec (cut JSON) is refused');
ok(live().rev === 1, 'after the refusals the factory spec is live');
ok(spec('screen', rev(2)).status === 'applied' && spec('screen', rev(2)).status === 'unchanged', 'the same spec again = unchanged');
ok(spec('screen', JSON.stringify({ ...JSON.parse(rev(2)), name: 'Edited, same rev' })).status === 'applied', 'an edit with the same rev applies');
ok(spec('deck', JSON.stringify({ v: 1, rev: 5, screens: [{ id: 'garage' }] })).status === 'refused', 'a deck naming an unknown screen is refused');
// links: more_row tap -> nav.open; any component's own "open" -> nav.open (screen from the event); not other actions
const linkSpec = (nodes) => JSON.stringify({ ...factory, rev: 20, nodes });
ok(spec('check', linkSpec([{ c: 'more_row', p: {}, on: { tap: { do: 'nav.open', screen: 'light' } } }])).ok, 'a more_row tap may open a screen');
ok(spec('check', linkSpec([{ c: 'card', p: {}, on: { open: { do: 'nav.open' } } }])).ok, 'any component\'s own open event may open the screen it names');
ok(codes(spec('check', linkSpec([{ c: 'card', p: {}, on: { open: { do: 'light.toggle', e: 'light.x' } } }]))).includes('not_allowed'), 'an open event allows nav.open only');
ok(codes(spec('check', linkSpec([{ c: 'more_row', p: {}, on: { tap: { do: 'light.toggle', e: 'light.x' } } }]))).includes('not_allowed'), 'a more_row tap allows navigation only');
ok(codes(spec('check', linkSpec([{ c: 'more_row', p: {}, on: { tap: { do: 'nav.open' } } }]))).includes('bad_screen'), 'a tap without a screen is refused (only open takes it from the event)');
// climate: thermostat_dial change / input -> climate.set_temperature on a climate entity; nothing else
const tSpec = (on) => JSON.stringify({ ...factory, rev: 30, nodes: [{ c: 'thermostat_dial', p: { target: { e: 'climate.test_room', a: 'temperature', fmt: 'num' } }, on }] });
ok(spec('check', tSpec({ change: { do: 'climate.set_temperature', e: 'climate.test_room' }, input: { do: 'climate.set_temperature', e: '$entity' } })).ok, 'a thermostat dial may set the temperature (live and on release), fmt num binds');
ok(codes(spec('check', tSpec({ change: { do: 'climate.set_temperature', e: 'light.x' } }))).includes('bad_entity'), 'climate actions need a climate entity');
ok(codes(spec('check', tSpec({ change: { do: 'light.brightness', e: 'light.x' } }))).includes('not_allowed'), 'a thermostat dial cannot dim a light');
ok(codes(spec('check', tSpec({ change: { do: 'climate.set_hvac_mode', e: 'climate.test_room' } }))).includes('not_allowed'), 'the dial sets the temperature only (modes come from the thermostat view)');
ok(codes(spec('check', JSON.stringify({ ...factory, rev: 31, nodes: [{ c: 'dial', on: { change: { do: 'climate.set_temperature', e: 'climate.test_room' } } }] }))).includes('not_allowed'), 'a light dial cannot set a thermostat');
console.log(fails ? `spec_test: ${fails} FAILED` : 'spec_test: all passed');
process.exit(fails ? 1 : 0);
