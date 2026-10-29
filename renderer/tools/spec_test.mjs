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
  if (base) ok(spec('screen_dev', rev(base)).status === 'applied', `base rev ${base} applied`);
  store('fault', `${n} ${mode}`);
  spec('screen_dev', rev(3));
  const log = store('reboot').log ?? [];
  const r = live().rev;
  const want = [base || 1, 3];
  ok(want.includes(r) && builds(), `power lost at write ${n} (${mode}) over ${base ? 'rev 2' : 'factory'}: live rev ${r} (one of ${want.join(' / ')}), builds${log.length ? `; boot: ${log.join('; ')}` : ''}`);
}
// a torn current copy falls back to the last good one
store('reset');
spec('screen_dev', rev(2));
spec('screen_dev', rev(3));
store('fault', '0 torn');
spec('screen_dev', rev(4));  // its first write (the last good copy = rev 3) lands torn; nothing after
store('reboot');
ok([3].includes(live().rev), `torn last-good write: live rev ${live().rev} (3)`);

// refusals and no-ops
store('reset');
const bad = (s) => spec('screen_dev', JSON.stringify(s));
const codes = (r) => (r.errors ?? []).map((e) => e.code);
ok(codes(spec('screen', JSON.stringify({ ...factory, rev: 9 }))).includes('built_in') && spec('screen', JSON.stringify({ ...factory, id: 'mine', rev: 9 })).ok, 'a sent screen may not replace a built-in (built_in); under its own id it applies (facet-209)');
ok(codes(bad({ ...factory, rev: 9, colour: 'red' })).includes('style_key'), 'a style key is refused (style_key)');
ok(codes(bad({ ...factory, rev: 9, nodes: [{ c: 'no_such' }] })).includes('unknown_component'), 'an unknown component is refused');
ok(codes(bad({ ...factory, rev: 9, nodes: [{ c: 'list_row', on: { tap: { do: 'shell.run' } } }] })).includes('bad_action'), 'an action outside the whitelist is refused');
ok(codes(bad({ ...factory, rev: 9, nodes: [{ c: 'dial', on: { toggle: { do: 'nav.open', screen: 'x' } } }] })).includes('not_allowed'), 'an action the component event does not allow is refused');
ok(codes(bad({ ...factory, rev: 9, nodes: Array(60).fill({ c: 'list_row' }) })).includes('over_budget'), 'over the node budget is refused');
ok(codes(spec('screen_dev', '{"v":1,')).includes('bad_json'), 'half a spec (cut JSON) is refused');
ok(live().rev === 1, 'after the refusals the factory spec is live');
ok(spec('screen_dev', rev(2)).status === 'applied' && spec('screen_dev', rev(2)).status === 'unchanged', 'the same spec again = unchanged');
ok(spec('screen_dev', JSON.stringify({ ...JSON.parse(rev(2)), name: 'Edited, same rev' })).status === 'applied', 'an edit with the same rev applies');
// deck v2 (screen model): pages, a start page, Home cards checked against the card types; v1 decks refused
const D = (pages, extra = {}) => JSON.stringify({ v: 2, rev: 5, start: 'h', pages: [{ id: 'h', kind: 'home', cards: [] }, ...pages], ...extra });
const dc = (d) => codes(spec('check_dev', d));
ok(spec('check_dev', D([{ id: 'media', kind: 'media', name: 'Speaker' }])).ok, 'a v2 deck with a Home and a page passes');
ok(dc(JSON.stringify({ v: 1, rev: 5, screens: [{ id: 'clock', card: true }] })).includes('bad_version'), 'a v1 deck is refused (bad_version)');
ok(spec('deck', D([{ id: 'g', kind: 'garage' }])).status === 'refused', 'a deck naming an unknown screen is refused');
ok(dc(D([{ id: 'l', kind: 'light' }])).includes('not_a_page'), 'a detail screen cannot be a page');
ok(dc(D([{ id: 's', kind: 'settings' }])).includes('not_a_page'), 'a system screen (Settings) cannot be a page');
ok(dc(D([{ id: 'h', kind: 'clock' }])).includes('bad_id'), 'two pages with one id are refused');
ok(dc(D([], { start: 'nope' })).includes('bad_start'), 'start names a page of the deck');
ok(dc(D([{ id: 'c', kind: 'clock', cards: [] }])).includes('bad_deck'), 'only a Home has cards');
const H = (cards) => JSON.stringify({ v: 2, rev: 5, start: 'h', pages: [{ id: 'h', kind: 'home', cards }, { id: 'w', kind: 'weather' }] });
ok(spec('check_dev', H([{ t: 'light', e: 'light.x', size: 's', tap: 'toggle' }, { t: 'page', page: 'w', size: 'l' }])).ok, 'light and page cards pass');
ok(dc(H([{ t: 'light', e: 'cover.x', size: 's' }])).includes('bad_entity'), 'a card takes only its domains');
ok(dc(H([{ t: 'climate', e: 'climate.x', size: 's' }])).includes('bad_size'), 'a card takes only its sizes');
ok(dc(H([{ t: 'cover', e: 'cover.x', size: 'm', tap: 'page' }])).includes('bad_tap'), 'a card takes only its taps');
ok(dc(H([{ t: 'page', page: 'nope', size: 'm' }])).includes('bad_page'), 'a page card names a page of the deck');
ok(dc(H([{ t: 'light', e: 'light.x', size: 's', x: 10 }])).includes('style_key'), 'a card has no position (style_key)');
ok(dc(H(Array(25).fill({ t: 'light', e: 'light.x', size: 's' }))).includes('over_budget'), 'over the card budget is refused');
ok(codes(bad({ ...factory, rev: 9, role: 'hero' })).includes('bad_role'), 'a spec role is page, detail or both');
// links: more_row tap -> nav.open; any component's own "open" -> nav.open (screen from the event); not other actions
const linkSpec = (nodes) => JSON.stringify({ ...factory, rev: 20, nodes });
ok(spec('check_dev', linkSpec([{ c: 'more_row', p: {}, on: { tap: { do: 'nav.open', screen: 'light' } } }])).ok, 'a more_row tap may open a screen');
ok(spec('check_dev', linkSpec([{ c: 'card', p: {}, on: { open: { do: 'nav.open' } } }])).ok, 'any component\'s own open event may open the screen it names');
ok(codes(spec('check_dev', linkSpec([{ c: 'card', p: {}, on: { open: { do: 'light.toggle', e: 'light.x' } } }]))).includes('not_allowed'), 'an open event allows nav.open only');
ok(codes(spec('check_dev', linkSpec([{ c: 'more_row', p: {}, on: { tap: { do: 'light.toggle', e: 'light.x' } } }]))).includes('not_allowed'), 'a more_row tap allows navigation only');
ok(codes(spec('check_dev', linkSpec([{ c: 'more_row', p: {}, on: { tap: { do: 'nav.open' } } }]))).includes('bad_screen'), 'a tap without a screen is refused (only open takes it from the event)');
// climate: thermostat_dial change / input -> climate.set_temperature on a climate entity; nothing else
const tSpec = (on) => JSON.stringify({ ...factory, rev: 30, nodes: [{ c: 'thermostat_dial', p: { target: { e: 'climate.test_room', a: 'temperature', fmt: 'num' } }, on }] });
ok(spec('check_dev', tSpec({ change: { do: 'climate.set_temperature', e: 'climate.test_room' }, input: { do: 'climate.set_temperature', e: '$entity' } })).ok, 'a thermostat dial may set the temperature (live and on release), fmt num binds');
ok(codes(spec('check_dev', tSpec({ change: { do: 'climate.set_temperature', e: 'light.x' } }))).includes('bad_entity'), 'climate actions need a climate entity');
ok(codes(spec('check_dev', tSpec({ change: { do: 'light.brightness', e: 'light.x' } }))).includes('not_allowed'), 'a thermostat dial cannot dim a light');
ok(codes(spec('check_dev', tSpec({ change: { do: 'climate.set_hvac_mode', e: 'climate.test_room' } }))).includes('not_allowed'), 'the dial sets the temperature only (modes come from the thermostat view)');
ok(codes(spec('check_dev', JSON.stringify({ ...factory, rev: 31, nodes: [{ c: 'dial', on: { change: { do: 'climate.set_temperature', e: 'climate.test_room' } } }] }))).includes('not_allowed'), 'a light dial cannot set a thermostat');
// pages from components (2026-10-23): layout slots (in / slot), repeated nodes (each / sample), bind maps, screen.toggle
const P = (nodes) => JSON.stringify({ ...factory, rev: 40, params: { entity: '', areas: '', rooms: { type: 'hidden', default: '' } }, nodes });
ok(spec('check_dev', P([{ c: 'split', p: {} }, { c: 'status_block', in: 0, slot: 2, p: {} }])).ok, 'a node may go into a layout node\'s slot');
ok(codes(spec('check_dev', P([{ c: 'card', p: {} }, { c: 'status_block', in: 0, p: {} }]))).includes('bad_in'), 'in: only a layout (a component with slots)');
ok(codes(spec('check_dev', P([{ c: 'status_block', in: 1, p: {} }, { c: 'split', p: {} }]))).includes('bad_in'), 'in: only an earlier node');
ok(codes(spec('check_dev', P([{ c: 'split', p: {} }, { c: 'status_block', in: 0, slot: 4, p: {} }]))).includes('bad_in'), 'slot: within the layout\'s slots');
ok(codes(spec('check_dev', P([{ c: 'group', p: {}, each: 'areas' }, { c: 'room_tile', in: 0, p: {} }]))).includes('bad_in'), 'a repeated node holds no others');
ok(codes(spec('check_dev', P([{ c: 'room_tile', p: {}, sample: [{}] }]))).includes('bad_each'), 'sample goes with each');
ok(codes(spec('check_dev', P([{ c: 'room_tile', p: {}, each: 'areas', sample: [{ nope: 1 }] }]))).includes('unknown_prop'), 'sample props are checked like props');
ok(codes(spec('check_dev', P([{ c: 'status_block', p: { value: { e: '$entity', map: 'x' } } }]))).includes('bad_bind'), 'a bind map is {value: text}');
ok(spec('check_dev', P([{ c: 'status_block', p: { value: { e: '$entity', s: 'docked', map: { docked: 'Docked' }, else: '', tpl: '{}' } } }])).ok, 'a bind may carry a sample, a map and a template');
ok(codes(spec('check_dev', P([{ c: 'room_tile', p: {}, on: { pick: { do: 'screen.toggle' } } }]))).includes('bad_action'), 'screen.toggle names its parameter');
// behaviour on the factory vacuum: three sample rooms, a pick goes into $rooms, Clean sends just those segments
const H2 = () => JSON.parse(m.UTF8ToString(m._fw_hits(stage)));
const step = (n) => { for (let k = 0; k < n; k++) { t += 16; m._fw_tick(t); } };  // ticks that keep the events (tick() drains them)
const tapAt = (x, y) => { m._fw_touch(stage, x, y, 1); step(4); m._fw_touch(stage, x, y, 0); step(6); };
const evs = () => { const e = m.UTF8ToString(m._fw_events()); return e ? JSON.parse(e) : []; };
S('vacuum.test', (e) => S('', (a) => S('docked', (v) => m._fw_ha_put(e, a, v))));
ok(render('spec', { screen: 'vacuum' }) === '' && (tick(40), true), 'the vacuum spec builds');
let tiles = H2().filter((h) => h.c === 'room_tile' && !h.prop);
ok(tiles.length === 3, `no rooms from a host = the three sample rooms (${tiles.length})`);
ok(render('spec', { screen: 'vacuum', entity: 'vacuum.test', areas: 'Kitchen:3, Hall:1' }) === '' && (tick(40), true), 'the vacuum spec builds on a vacuum');
tiles = H2().filter((h) => h.c === 'room_tile' && !h.prop).sort((a, b) => a.x - b.x);
ok(tiles.length === 2, `a room per item of areas (${tiles.length})`);
evs();
tapAt(tiles[0].x + 20, tiles[0].y + 20); tick(10); evs();
for (let y = 600; y >= 100; y -= 25) { m._fw_touch(stage, 360, y, 1); tick(1); }  // scroll the list to its foot
m._fw_touch(stage, 360, 100, 0); tick(60); evs();
const clean = H2().filter((h) => h.c === 'button' && !h.prop).sort((a, b) => b.x - a.x)[0];
tapAt(clean.x + 20, clean.y + 10);
const sent = evs().filter((e) => e.name === 'ha').map((e) => e.detail).join(' ');
ok(/app_segment_clean/.test(sent) && /"segments":\[3\]/.test(sent), `a picked room, then Clean = clean just that room (${sent.slice(0, 120)})`);
console.log(fails ? `spec_test: ${fails} FAILED` : 'spec_test: all passed');
process.exit(fails ? 1 : 0);
