// Readout (components/readout.cpp): the live sensor readout (label set) and the Home sensor card (half slot, bound
// to the entity's state, tone mute when unavailable). Values must follow HA, survive unavailable / empty / reconnect.
const DECK = { v: 2, rev: 9, start: 't', pages: [{ id: 't', kind: 'home', name: 'Test', cards: [
  { t: 'readout', e: 'binary_sensor.back_door', size: 'half', label: 'Back door' },
  { t: 'readout', e: 'sensor.garage_temperature', size: 'half', label: 'Garage' },
  { t: 'light', e: 'light.sofa_lamp', size: 's', tap: 'toggle', label: 'Sofa table' }] }] };
const val = (t, label) => { const p = t.part('readout', new RegExp(`^${label}$`, 'i')); return p ? p.texts.filter((s) => s.toUpperCase() !== label.toUpperCase()) : []; };
const inside = (o, b) => o.x >= b.x && o.y >= b.y && o.x + o.w <= b.x + b.w && o.y + o.h <= b.y + b.h;
import { frame565, diff565 } from './_release.mjs';
const stalePx = (t) => { const a = frame565(t); t.b.m._fw_redraw(t.b.stage); const b = frame565(t); return diff565(a, b); };
const CARD_H = { '1.5x': 280, '1x': 184 };  // lay::card_h (tokens/layout.json)
export default {
  id: 'readout', family: 'cards', about: 'Sensor readout: label + live value; the Home sensor card is two of these in one slot.',
  tests: [
    // C8c: the State readout declares its Home sizes (meta sizes: half only); the deck, the spec check and the editor's
    // schema all follow it
    { name: 'Home sizes: half only (meta sizes): a readout card at another size is refused, half is taken', async run(t) {
      await t.device(DECK);
      const deck = (size) => ({ ...DECK, rev: DECK.rev + 1, pages: [{ ...DECK.pages[0], cards: [{ t: 'readout', e: 'sensor.garage_temperature', size }] }] });
      for (const z of ['1x1', '2x1', '2x2', 's', 'l']) {  // a new type: no old sizes
        const r = JSON.parse(t.b.spec('deck', deck(z)) || '{}');
        t.ok(!r.ok && JSON.stringify(r.errors ?? '').includes('bad_size'), `size ${z} refused (bad_size): ${JSON.stringify(r.errors ?? r).slice(0, 160)}`);
      }
      const ok = JSON.parse(t.b.spec('deck', { ...deck('half'), rev: DECK.rev + 2 }) || '{}');
      t.ok(ok.ok, `half taken: ${JSON.stringify(ok.errors ?? '')}`);
    } },
    { name: 'Home sizes: a readout node in a half-row grid takes a half span only', mount: ['readout', {}], run(t) {
      const check = (span) => JSON.stringify(JSON.parse(t.b.spec('check', { v: 1, id: 'rsz', rev: 1, name: 'Sizes', layout: 'list',
        nodes: [{ c: 'grid', p: { cols: 3, aspect: 'half' } }, { c: 'readout', p: { label: 'Hall', state: '21°' }, in: 0, slot: 0, span }] }) || '{}'));
      for (const sp of [[1, 2], [2, 2], [2, 4]]) { const r = check(sp); t.ok(r.includes('bad_slot') && r.includes('.span'), `span ${sp} refused: ${r.slice(0, 160)}`); }
      const r = check([1, 1]);
      t.ok(!r.includes('bad_slot'), `span 1x1 half row taken: ${r.slice(0, 160)}`);
    } },
    // #62 (not reproduced yet): a value bound to the Home sensor card's readout vanished on a panel and never came back.
    // Ruled out here: rapid updates, unavailable / empty / reconnect, a hidden kept Home (data only, widgets wake on
    // show), look changes mid-burst, screen changes mid-update, and stale pixels (a full redraw = the partial one).
    // Seen on a Waveshare: Home sensor values vanished and never came back. HA values
    // reached the panel store, so suspect = the fade-on-update when updates arrive quicker than the fade.
    ...[[20, 'every 20 ms'], [80, 'every 80 ms'], [150, 'every 150 ms']].map(([gap, how]) => ({ name: `Home: rapid HA updates (${how}) end on the last value, fully drawn`, async run(t) {
      await t.device(DECK);
      const ref = t.objs().find((o) => o.text && /°|^\d/.test(o.text) && t.part('readout', /^garage$/i) && inside(o, t.part('readout', /^garage$/i)));
      for (let i = 0; i < 30; i++) {
        const v = i % 7 === 3 ? 'unavailable' : (18 + i / 10).toFixed(1);
        t.b.haPut('sensor.garage_temperature', '', v); t.b.haPut('binary_sensor.back_door', '', i % 2 ? 'on' : 'off');
        t.wait(gap);
      }
      t.b.haPut('sensor.garage_temperature', '', '21.4'); t.b.haPut('binary_sensor.back_door', '', 'off'); t.wait(2000);
      const g = t.part('readout', /^garage$/i), d = t.part('readout', /^back door$/i);
      if (!t.ok(g && d, 'both readouts there')) return;
      t.has(g.texts, /21\.4/, 'garage shows the last value');
      t.ok(d.texts.length > 1, `back door shows a value: ${JSON.stringify(d.texts)}`);
      const labs = t.objs().filter((o) => o.text && t.visible(o) && (inside(o, g) || inside(o, d)));
      t.eq(labs.filter((o) => o.topa !== undefined || o.opa < 255).map((o) => `${o.text} topa ${o.topa} opa ${o.opa}`), [], 'no text left faded');
      if (ref) { const now = labs.find((o) => /21\.4/.test(o.text)); t.eq(now && now.ink, ref.ink, 'value ink = the ink before the burst'); }
    } })),
    { name: 'live readout shows label and state', mount: ['readout', { label: 'Dishwasher', icon: 'bolt', state: '0.8 W', half: true }], run(t) {
      t.has(t.texts(), /dishwasher/i, 'label'); t.has(val(t, 'Dishwasher'), '0.8 W', 'state');
    } },
    { name: 'Home: two half readouts share one card slot', async run(t, { den }) {
      await t.device(DECK);
      const a = t.part('readout', /^back door$/i), b = t.part('readout', /^garage$/i);
      if (!t.ok(a && b, 'both readouts found')) return;
      t.eq([a.x, a.w, a.h], [b.x, b.w, b.h], 'same column and size'); t.ok(a.h % 4 === 0, `height ${a.h} on the grid`);
      const gap = b.y - (a.y + a.h); t.ok(gap > 0 && gap % 4 === 0, `gap ${gap} between the halves`);
      t.ok(b.y + b.h - a.y <= CARD_H[den], `pair ${b.y + b.h - a.y} px taller than a card slot ${CARD_H[den]}`);
    } },
    { name: 'update: new value shows, silently', mount: ['readout', { label: 'Dishwasher', state: '0.8 W', half: true }], run(t) {
      const ev = t.update({ state: '1.2 kW' }); t.silent(ev); t.has(val(t, 'Dishwasher'), '1.2 kW', 'new value');
    } },
    { name: 'update to empty then back: the value returns', mount: ['readout', { label: 'Dishwasher', state: '0.8 W', half: true }], run(t) {
      t.update({ state: '' }); t.update({ state: '3 W' }); t.has(val(t, 'Dishwasher'), '3 W', 'value after an empty update');
    } },
    { name: 'a tap does nothing (read-only)', mount: ['readout', { label: 'Dishwasher', state: '0.8 W', half: true }], run(t) {
      const ev = t.tap(/dishwasher/i); t.eq(ev.filter((e) => e.kind === 'event' && e.name !== 'screen').length, 0, 'events'); t.silent(ev);
    } },
    // ---- Home sensor cards, live HA
    { name: 'Home: sensor cards show HA values', async run(t) {
      await t.device(DECK);
      t.has(val(t, 'Back door'), /closed/i, 'door'); t.has(val(t, 'Garage'), /18[.,]5/, 'temperature');
    } },
    { name: 'Home: values follow HA changes, silently', async run(t) {
      await t.device(DECK);
      t.ha.set('binary_sensor.back_door', 'on'); t.ha.set('sensor.garage_temperature', '21.0'); const ev = t.wait(800);
      t.has(val(t, 'Back door'), /open/i, 'door open'); t.has(val(t, 'Garage'), /21/, 'new temperature'); t.silent(ev);
    } },
    { name: 'Home: unavailable then back: the value returns', async run(t) {
      await t.device(DECK);
      t.ha.unavailable('sensor.garage_temperature'); t.wait(800);
      t.ok(val(t, 'Garage').length > 0, 'unavailable shows a word, not a blank');
      t.ha.set('sensor.garage_temperature', '19.0'); t.wait(800); t.has(val(t, 'Garage'), /19/, 'value back');
    } },
    { name: 'Home: empty state then a value: it returns', async run(t) {
      await t.device(DECK);
      t.ha.set('sensor.garage_temperature', ''); t.wait(800); t.ha.set('sensor.garage_temperature', '17.5'); t.wait(800);
      t.has(val(t, 'Garage'), /17[.,]5/, 'value after an empty state');
    } },
    { name: 'Home: HA disconnect + reconnect: sensor values come back', async run(t) {
      await t.device(DECK);
      const before = [val(t, 'Back door').join(), val(t, 'Garage').join()];
      t.ha.disconnect(); t.wait(800); t.ha.reconnect(); t.wait(800);
      t.eq([val(t, 'Back door').join(), val(t, 'Garage').join()], before, 'values after reconnect');
    } },
    ...[
      ['plain', (t) => { t.ha.set('sensor.garage_temperature', '22.5'); t.wait(800); }],
      ['unavailable then value', (t) => { t.ha.unavailable('sensor.garage_temperature'); t.wait(800); t.ha.set('sensor.garage_temperature', '22.5'); t.wait(800); }],
      ['disconnect reconnect then value', (t) => { t.ha.disconnect(); t.wait(800); t.ha.reconnect(); t.wait(800); t.ha.set('sensor.garage_temperature', '22.5'); t.wait(800); }],
      ['look change then value', (t) => { t.b.test('setting', 'textSize=large'); t.wait(800); t.ha.set('sensor.garage_temperature', '22.5'); t.wait(800); }],
      ['theme then value', (t) => { t.b.test('setting', 'theme=light'); t.wait(800); t.ha.set('sensor.garage_temperature', '22.5'); t.wait(800); }],
      ['burst', (t) => { for (let i = 0; i < 30; i++) { t.ha.set('sensor.garage_temperature', i % 5 === 2 ? 'unavailable' : String(10 + i)); t.wait(30); } t.ha.set('sensor.garage_temperature', '22.5'); t.wait(800); }],
    ].map(([how, step]) => ({ name: `#62 Home kept and hidden (another screen shows): ${how}, then back: the value is there, fully drawn`, async run(t) {
      await t.device({ ...DECK, pages: [...DECK.pages, { id: 'clock', kind: 'clock', params: { source: 'sample' } }],
        start: 't' });
      t.b.test('keep', '8'); t.b.tick(300);
      t.b.spec('act', { do: 'nav.open', screen: 'clock' }); t.b.tick(10); t.b.settle(800);
      step(t);
      t.b.spec('act', { do: 'nav.back' }); t.b.tick(10); t.b.settle(800);
      t.has(val(t, 'Garage'), /22[.,]5/, `value back on Home: ${JSON.stringify(val(t, 'Garage'))}`);
      t.eq(stalePx(t), 0, 'no stale pixels');
      t.ok(val(t, 'Back door').length > 0, `door: ${JSON.stringify(val(t, 'Back door'))}`);
    } })),
    ...['theme=light', 'textSize=large'].map((set) => ({ name: `#62 Home shown: a look change (${set}) inside a burst of updates, then a trip away: the values follow`, async run(t) {
      await t.device({ ...DECK, pages: [...DECK.pages, { id: 'clock', kind: 'clock', params: { source: 'sample' } }] });
      t.b.test('keep', '8'); t.b.tick(300);
      for (let i = 0; i < 30; i++) {
        if (i === 10) t.b.test('setting', set);
        t.ha.set('sensor.garage_temperature', i % 6 === 4 ? 'unavailable' : String(10 + i)); t.ha.set('binary_sensor.back_door', i % 2 ? 'on' : 'off');
        t.wait(40);
      }
      t.ha.set('sensor.garage_temperature', '22.5'); t.wait(2500);
      t.has(val(t, 'Garage'), /22[.,]5/, `value: ${JSON.stringify(val(t, 'Garage'))}`);
      t.b.spec('act', { do: 'nav.open', screen: 'clock' }); t.b.tick(10); t.b.settle(800);
      t.ha.set('sensor.garage_temperature', '23.5'); t.wait(800);
      t.b.spec('act', { do: 'nav.back' }); t.b.tick(10); t.b.settle(800);
      t.has(val(t, 'Garage'), /23[.,]5/, `after a trip away: ${JSON.stringify(val(t, 'Garage'))}`);
      t.eq(stalePx(t), 0, 'no stale pixels');
    } })),
    { name: '#62 updates while the screen changes (Home <-> Clock): the last value, fully drawn', async run(t) {
      await t.device({ ...DECK, pages: [...DECK.pages, { id: 'clock', kind: 'clock', params: { source: 'sample' } }] });
      t.b.test('keep', '8'); t.b.tick(300);
      for (let i = 0; i < 6; i++) {
        t.b.spec('act', { do: 'nav.open', screen: 'clock' }); t.b.tick(2); t.ha.set('sensor.garage_temperature', String(30 + i)); t.b.tick(3);
        t.b.spec('act', { do: 'nav.back' }); t.b.tick(2); t.ha.set('sensor.garage_temperature', String(40 + i)); t.b.tick(3);
      }
      t.b.settle(1500);
      t.has(val(t, 'Garage'), /45/, `the last value: ${JSON.stringify(val(t, 'Garage'))}`);
      t.eq(stalePx(t), 0, 'no stale pixels');
    } },
    { name: 'Home: many quick updates: the last value wins', async run(t) {
      await t.device(DECK);
      for (let i = 0; i < 20; i++) { t.ha.set('sensor.garage_temperature', String(10 + i)); t.wait(32); }
      t.wait(1500); t.has(val(t, 'Garage'), /29/, 'last value');
    } },
  ],
};
