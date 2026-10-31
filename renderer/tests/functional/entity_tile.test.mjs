import fs from 'node:fs';
const CJ = JSON.parse(fs.readFileSync(new URL('../../tokens/colour.json', import.meta.url))), C = CJ.colours;
const is = (hex, tok) => !!hex && [C[tok].dark, C[tok].light].some((v) => v.toUpperCase() === String(hex).toUpperCase());
const inBox = (o, b) => o.x >= b.x && o.y >= b.y && o.x + o.w <= b.x + b.w && o.y + o.h <= b.y + b.h;
// Entity tile (components/entity_tile.cpp) on its own, and the Home light cards built from it (screens/home.cpp:
// status = "level" of the light, on = its state) driven through the fake Home Assistant: the state line must follow
// the live HA state after a tap, an outside change, unavailable and a reconnect.
const tile = (t, label) => t.part('entity_tile', label);
const status = (t, label) => { const p = tile(t, label); return p ? p.texts.filter((s) => s.toUpperCase() !== label.toUpperCase()) : null; };
const tileBg = (t, label) => { const p = tile(t, label); return p && t.objs().find((o) => o.box && o.x === p.x && o.y === p.y && o.w === p.w && o.h === p.h)?.bg; };
const HOME_LIGHTS = { Ceiling: 'light.living_room_ceiling', 'Sofa table': 'light.sofa_lamp', Chair: 'light.reading_lamp' };
export default {
  id: 'entity_tile', family: 'cards', about: 'Small card for one entity: name + state line, tap toggles or opens. Home light cards (device) included.',
  tests: [
    // ---- the component
    { name: 'shows its name and status', mount: ['entity_tile', { label: 'Sofa table', status: '64%', on: true }], run(t) {
      t.has(t.texts(), 'Sofa table', 'name'); t.has(status(t, 'Sofa table'), /^64%$/i, 'status');
    } },
    { name: 'tap on an on toggle tile: tap event + toggle-off', mount: ['entity_tile', { label: 'Sofa table', on: true }], run(t) {
      const ev = t.tap('Sofa table'); t.emitted(ev, 'tap', /Sofa table/); t.sounds(ev, ['toggle-off']);
    } },
    { name: 'tap on an off toggle tile: toggle-on', mount: ['entity_tile', { label: 'Chair', on: false, status: 'Off' }], run(t) {
      t.sounds(t.tap('Chair'), ['toggle-on']);
    } },
    { name: 'act open: open sound, one tap', mount: ['entity_tile', { label: 'Blind', act: 'open', on: false }], run(t) {
      const ev = t.tap('Blind'); t.sounds(ev, ['open']); t.eq(ev.filter((e) => e.name === 'tap').length, 1, 'tap count');
    } },
    { name: 'act none (sensor): a tap does nothing', mount: ['entity_tile', { label: 'Garage', act: 'none', on: false, status: '18.5 °C' }], run(t) {
      const ev = t.tap('Garage'); t.notEmitted(ev, 'tap'); t.silent(ev);
    } },
    { name: 'hold fires at most one tap', mount: ['entity_tile', { label: 'Sofa table' }], run(t) {
      t.ok(t.hold('Sofa table').filter((e) => e.name === 'tap').length <= 1, 'one tap at most');
    } },
    { name: 'unavailable: no tap, error only', mount: ['entity_tile', { label: 'Sofa table', state: 'unavailable' }], run(t) {
      const ev = t.tap('Sofa table'); t.notEmitted(ev, 'tap'); t.sounds(ev, ['error']);
    } },
    { name: 'update status + on: new line shows, silently', mount: ['entity_tile', { label: 'Sofa table', status: '64%', on: true }], run(t) {
      const ev = t.update({ status: 'Off', on: false }); t.silent(ev);
      t.has(status(t, 'Sofa table'), /^off$/i, 'status after update'); t.hasNot(status(t, 'Sofa table'), /64%/, 'old status');
    } },
    // the card keeps the surface: on shows in the icon (accent) and the state (accent-text), never a filled card
    { name: 'on and off: the card stays the surface', mount: ['entity_tile', { label: 'Sofa table', status: '64%', on: true }], run(t) {
      t.ok(is(tileBg(t, 'Sofa table'), 'surface'), `on: fill ${tileBg(t, 'Sofa table')}`);
      t.ok(!is(tileBg(t, 'Sofa table'), 'accent'), 'on: not the accent');
      const st = t.objs().find((o) => o.text === '64%' && inBox(o, tile(t, 'Sofa table')));
      t.ok(st && is(st.ink, 'accent-text'), `on: state ink ${st?.ink}`);
      t.update({ on: false, status: 'Off' });
      t.ok(is(tileBg(t, 'Sofa table'), 'surface'), `off: fill ${tileBg(t, 'Sofa table')}`);
      t.update({ on: true, status: '64%' });
      t.ok(is(tileBg(t, 'Sofa table'), 'surface'), `on again: fill ${tileBg(t, 'Sofa table')}`);
    } },
    { name: 'taps both ways keep the surface', mount: ['entity_tile', { label: 'Chair', on: false, status: 'Off' }], run(t) {
      t.ok(is(tileBg(t, 'Chair'), 'surface'), 'off: surface'); t.tap('Chair'); t.wait(600);
      t.ok(is(tileBg(t, 'Chair'), 'surface'), `on after a tap: ${tileBg(t, 'Chair')}`);
      t.tap('Chair'); t.wait(600);
      t.ok(is(tileBg(t, 'Chair'), 'surface'), `off after a tap: ${tileBg(t, 'Chair')}`);
    } },
    { name: 'sensor (act none) stays unfilled', mount: ['entity_tile', { label: 'Garage', act: 'none', on: false, status: '18.5 °C' }], run(t) {
      t.ok(is(tileBg(t, 'Garage'), 'surface'), 'surface');
    } },
    // ---- Home light cards on the device, live HA
    { name: 'Home: light cards show their HA level', async run(t) {
      await t.device('house');
      for (const [label, e] of Object.entries(HOME_LIGHTS)) {
        const on = t.ha.get(e) === 'on', want = on ? `${Math.round(t.ha.get(e, 'brightness') / 2.55)}%` : 'Off';
        t.has(status(t, label) || [], new RegExp(`^${want}$`, 'i'), `${label} state line`);
      }
    } },
    { name: 'Home: toggling Sofa table calls HA and the state line follows (facet-181)', async run(t) {
      await t.device('house');
      t.ha.set('light.sofa_lamp', 'on', { brightness: 115 });
      t.tap('Sofa table'); t.call({ service: 'light.toggle', entity: 'light.sofa_lamp' }, 'toggle call');
      t.has(status(t, 'Sofa table') || [], /^off$/i, 'after turning off');
      t.tap('Sofa table'); t.has(status(t, 'Sofa table') || [], /^45%$/, 'after turning back on');
    } },
    { name: 'Home: Chair off -> on shows its brightness, not Off (facet-181)', async run(t) {
      await t.device('house');
      t.ha.set('light.reading_lamp', 'off', { brightness: null });
      t.tap('Chair'); t.hasNot(status(t, 'Chair') || [], /^off$/i, 'state line after turning on');
      t.has(status(t, 'Chair') || [], /^(\d+%|on)$/i, 'shows a level');
    } },
    { name: 'Home: a light that turns off shows Off, not its last brightness (facet-181 ceiling 50%)', async run(t) {
      await t.device('house');
      t.ha.set('light.living_room_ceiling', 'on', { brightness: 128 }); t.has(status(t, 'Ceiling') || [], /^50%$/, 'on at 50%');
      t.ha.set('light.living_room_ceiling', 'off', { brightness: null }); t.has(status(t, 'Ceiling') || [], /^off$/i, 'off');
      t.ha.set('light.living_room_ceiling', 'off', { brightness: 128 }); t.has(status(t, 'Ceiling') || [], /^off$/i, 'off even if HA still reports a brightness');
    } },
    { name: 'Home: outside changes (HA, not a tap) update the line silently', async run(t) {
      await t.device('house');
      t.ha.set('light.sofa_lamp', 'off', { brightness: null }); let ev = t.wait(600);
      t.has(status(t, 'Sofa table') || [], /^off$/i, 'off'); t.silent(ev, 'outside off');
      t.ha.set('light.sofa_lamp', 'on', { brightness: 255 }); ev = t.wait(600);
      t.has(status(t, 'Sofa table') || [], /^100%$/, 'on 100%'); t.silent(ev, 'outside on');
    } },
    { name: 'Home: unavailable light: no HA call on tap, comes back with its level', async run(t) {
      await t.device('house');
      t.ha.unavailable('light.sofa_lamp'); const n = t.ha.calls.length;
      const ev = t.tap('Sofa table'); t.eq(t.ha.calls.length, n, 'HA calls while unavailable'); t.sounds(ev, ['error']);
      t.ha.set('light.sofa_lamp', 'on', { brightness: 115 }); t.has(status(t, 'Sofa table') || [], /^45%$/, 'back');
      t.emitted(t.tap('Sofa table'), 'ha', /light\.sofa_lamp/, 'taps again once back');
    } },
    { name: 'Home: the panel theme set unlike the stage: taps keep the panel\'s card surface (Guition black off card)', async run(t) {
      await t.device('house');
      const bg0 = String(tileBg(t, 'Sofa table') || '').toUpperCase();
      const stageLight = bg0 === C.surface.light.toUpperCase(), want = stageLight ? 'dark' : 'light';
      t.b.str('theme', (a) => t.b.str(want, (v) => t.b.m.UTF8ToString(t.b.m._fw_setting(a, v)))); t.wait(1200);
      const surf = C.surface[want].toUpperCase(), bg = () => String(tileBg(t, 'Sofa table') || '').toUpperCase();
      t.eq(bg(), surf, `${want} panel: card surface`);
      for (const s of ['on', 'off', 'on']) {
        t.ha.set('light.sofa_lamp', s === 'on' ? 'off' : 'on', s === 'on' ? { brightness: null } : { brightness: 115 }); t.wait(600);
        t.tap('Sofa table'); t.wait(600); t.eq(bg(), surf, `after a tap to ${s}`);
      }
    } },
    { name: 'Home: on -> off -> another accent -> on: the current accent; a tile lit before the change follows it', async run(t) {
      await t.device('house');
      const light = String(tileBg(t, 'Sofa table') || '').toUpperCase() === C.surface.light.toUpperCase();
      const set = CJ.accent.sets.find((a) => a.id !== (t.setting('accent') || CJ.accent.default));
      const ink = (label, re) => t.objs().find((o) => o.text && re.test(o.text) && inBox(o, tile(t, label)))?.ink;
      t.ha.set('light.living_room_ceiling', 'on', { brightness: 128 }); t.ha.set('light.sofa_lamp', 'on', { brightness: 115 }); t.wait(600);
      t.tap('Sofa table'); t.wait(600); t.has(status(t, 'Sofa table') || [], /^off$/i, 'sofa off');
      t.b.str('accent', (a) => t.b.str(set.id, (v) => t.b.m.UTF8ToString(t.b.m._fw_setting(a, v)))); t.wait(1500);
      const want = set[light ? 'light' : 'dark']['accent-text'].toUpperCase();
      t.eq(String(ink('Ceiling', /^50%$/) || '').toUpperCase(), want, `lit before the change: ${set.id} accent text`);
      t.tap('Sofa table'); t.wait(600);
      t.eq(String(ink('Sofa table', /^\d+%$/) || '').toUpperCase(), want, `on after the change: ${set.id} accent text`);
      t.eq(String(tileBg(t, 'Sofa table') || '').toUpperCase(), C.surface[light ? 'light' : 'dark'].toUpperCase(), 'still the surface');
    } },
    { name: 'Home: HA disconnect + reconnect: every light line comes back', async run(t) {
      await t.device('house');
      const before = Object.keys(HOME_LIGHTS).map((l) => (status(t, l) || []).join());
      t.ha.disconnect(); t.ha.reconnect(); t.wait(600);
      t.eq(Object.keys(HOME_LIGHTS).map((l) => (status(t, l) || []).join()), before, 'state lines after reconnect');
    } },
  ],
};
