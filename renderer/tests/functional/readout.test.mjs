// Readout (components/readout.cpp): the live sensor readout (label set) and the Home sensor card (half slot, bound
// to the entity's state, tone mute when unavailable). Values must follow HA, survive unavailable / empty / reconnect.
const DECK = { v: 2, rev: 9, start: 't', pages: [{ id: 't', kind: 'home', name: 'Test', cards: [
  { t: 'sensor', e: 'binary_sensor.back_door', size: 'm', label: 'Back door' },
  { t: 'sensor', e: 'sensor.garage_temperature', size: 'm', label: 'Garage' },
  { t: 'light', e: 'light.sofa_lamp', size: 's', tap: 'toggle', label: 'Sofa table' }] }] };
const val = (t, label) => { const p = t.part('readout', new RegExp(`^${label}$`, 'i')); return p ? p.texts.filter((s) => s.toUpperCase() !== label.toUpperCase()) : []; };
const CARD_H = { '1.5x': 280, '1x': 184 };  // lay::card_h (tokens/layout.json)
export default {
  id: 'readout', family: 'cards', about: 'Sensor readout: label + live value; the Home sensor card is two of these in one slot.',
  tests: [
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
    { name: 'update to empty then back: the value returns (facet-182)', mount: ['readout', { label: 'Dishwasher', state: '0.8 W', half: true }], run(t) {
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
    { name: 'Home: unavailable then back: the value returns (facet-182)', async run(t) {
      await t.device(DECK);
      t.ha.unavailable('sensor.garage_temperature'); t.wait(800);
      t.ok(val(t, 'Garage').length > 0, 'unavailable shows a word, not a blank');
      t.ha.set('sensor.garage_temperature', '19.0'); t.wait(800); t.has(val(t, 'Garage'), /19/, 'value back');
    } },
    { name: 'Home: empty state then a value: it returns (facet-182)', async run(t) {
      await t.device(DECK);
      t.ha.set('sensor.garage_temperature', ''); t.wait(800); t.ha.set('sensor.garage_temperature', '17.5'); t.wait(800);
      t.has(val(t, 'Garage'), /17[.,]5/, 'value after an empty state');
    } },
    { name: 'Home: HA disconnect + reconnect: sensor values come back (facet-182)', async run(t) {
      await t.device(DECK);
      const before = [val(t, 'Back door').join(), val(t, 'Garage').join()];
      t.ha.disconnect(); t.wait(800); t.ha.reconnect(); t.wait(800);
      t.eq([val(t, 'Back door').join(), val(t, 'Garage').join()], before, 'values after reconnect');
    } },
    { name: 'Home: many quick updates: the last value wins', async run(t) {
      await t.device(DECK);
      for (let i = 0; i < 20; i++) { t.ha.set('sensor.garage_temperature', String(10 + i)); t.wait(32); }
      t.wait(1500); t.has(val(t, 'Garage'), /29/, 'last value');
    } },
  ],
};
