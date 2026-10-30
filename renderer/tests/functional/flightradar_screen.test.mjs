// Flight radar screen (specs/flightradar.json: header Now | Radar; Now = the Flight hero, also up there, all aircraft;
// Radar = the big Sky radar; All aircraft list; the detail view), on the flights feed panel.flights
// (core/flights_source.cpp, CONTRACT.md C51) and its action flights.select. Home Assistant is faked (tools/functional.mjs
// FakeHA); S is NOT a real sensor. Approved original: Astro packages/ui/src/screens/overhead.ts.
const S = 'sensor.test_overhead';
// [hex, callsign, type, alt, vs, gs, track, km*10, brg, elev*10, squawk, em, kind, mil, route, who, reg, model, operator]
const A = ['aaa111', 'AAA1', 'A20N', 29000, 0, 460, 330, 60, 300, 550, '2000', '', 'jet', 0, 'Amsterdam → Edinburgh', 'easyJet · U245UK · Airbus A320neo', 'G-UZHA', 'Airbus A320neo', 'easyJet'];
const B = ['bbb222', 'BBB2', 'B38M', 32000, -1200, 452, 302, 125, 90, 220, '2000', '', 'jet', 0, 'Kraków → Leeds', 'Ryanair · FR6BA · Boeing 737 MAX 8', 'EI-IHR', 'Boeing 737 MAX 8', 'Ryanair'];
const C = ['ccc333', 'CCC3', 'A332', 18000, 0, 330, 75, 300, 180, 100, '7700', 'general', 'jet', 1, '', 'Royal Air Force · Airbus Voyager KC2', 'ZZ336', 'Airbus Voyager KC2', 'Royal Air Force'];
const sky = (t, rows, extra = {}) => { t.ha.s[S] = { '': String(rows.length), a: 'J' + JSON.stringify(rows), ...extra }; };
async function open(t, params = {}) { await t.mount('spec', { screen: 'flightradar', source: 'ha', sensor: S, ...params }); return t; }
const route = (t) => t.texts().find((x) => /→|^[A-Z]{3}\d$/.test(x));
// a plane on the big radar (its box; rmax = half the box - half a plane: layout sky-plane-lg 32 / 24; square-root
// scale to 50 km)
const plane = (t, km, brg) => {
  const c = t.comps().find((x) => x.ids.includes('sky_radar') && t.visible(x)), R = c.w / 2, glyph = t.b.S > 600 ? 32 : 24;
  const r = (R - glyph / 2) * Math.sqrt(km / 50), a = (brg * Math.PI) / 180;
  return { at: [c.x + R + r * Math.sin(a), c.y + R - r * Math.cos(a)] };
};
// the More row's visible top (its lower part is under the body's bottom inset until the list scrolls)
const more = (t) => { const o = t.find(/all aircraft · \d/i); return { at: [o.x + o.w / 2, o.y - 8] }; };
const set = (t, id, v) => t.b.str(id, (p) => t.b.str(v, (q) => t.b.m._fw_setting(p, q)));

export default {
  id: 'flightradar_screen', family: 'screens', about: 'The Flight radar on the flights feed: views, selection, detail, list, empty states, units.',
  tests: [
    { name: 'now: the closest leads; the next two below; all aircraft · 3', async run(t) {
      sky(t, [B, C, A]); await open(t);
      t.eq(route(t), 'Amsterdam → Edinburgh', 'the closest leads');
      t.has(t.texts(), /^also up there$/i, 'also up there'); t.has(t.texts(), 'Kraków → Leeds', 'the next one'); t.has(t.texts(), 'CCC3', 'no route: the callsign');
      t.has(t.texts(), /7700 emergency/i, 'the emergency on its row'); t.has(t.texts(), /all aircraft · 3/i, 'the More row counts');
      t.has(t.texts(), /^descending$|^altitude$/i, 'the altitude stat');
    } },
    { name: 'a row selects; the hero again opens the detail; Back returns to Now', async run(t) {
      sky(t, [A, B, C]); await open(t);
      t.tap('Kraków → Leeds'); t.wait(300);
      t.eq(route(t), 'Kraków → Leeds', 'the tapped one leads');
      t.has(t.texts(), 'Amsterdam → Edinburgh', 'the old lead is a row now');
      t.has(t.texts(), /^descending$/i, 'its vertical speed reads descending');
      t.tap('Kraków → Leeds'); t.wait(300);
      t.has(t.texts(), /^BBB2$/, 'the detail header is the callsign');
      for (const [l, v] of [['registration', 'EI-IHR'], ['operator', 'Ryanair'], ['vertical speed', '−1,200 ft/min'], ['heading', '302° NW'], ['aircraft', /^Boeing 737/]]) {
        t.has(t.texts(), new RegExp(`^${l}$`, 'i'), `fact ${l}`); t.has(t.texts(), v, `${l} value`);
      }
      t.has(t.texts(), /^BBB222 · data Home Assistant$/i, 'the source line');
      t.tap({ clk: 0 }); t.wait(300);
      t.has(t.texts(), /^now$/i, 'Back: the Now | Radar tabs again'); t.eq(route(t), 'Kraków → Leeds', 'the selection stays');
    } },
    { name: 'radar: tap a plane = select it, again = its detail', async run(t) {
      sky(t, [A, B, C]); await open(t);
      t.tap(/^radar$/i); t.wait(300);
      t.ok(t.comps().some((c) => c.ids.includes('sky_radar') && c.w === (t.b.S > 600 ? 576 : 384)), 'the big radar (576 / 384)');
      const ev = t.tap(plane(t, 12.5, 90)); t.wait(300);
      t.emitted(ev, 'pick', 'bbb222', 'the plane picked by hex');
      t.tap(plane(t, 12.5, 90)); t.wait(300);
      t.has(t.texts(), /^BBB2$/, 'the selected plane again: its detail');
      t.tap({ clk: 0 }); t.wait(300); t.has(t.texts(), /^radar$/i, 'Back: the radar view');
    } },
    { name: 'all aircraft: closest first; a row selects and returns to Now', async run(t) {
      sky(t, [C, B, A]); await open(t);
      const ev = t.tap(more(t)); t.wait(300);
      t.emitted(ev, 'param', 'view=list', 'the list view opens');
      t.has(t.texts(), /^all aircraft$/i, 'the list header'); t.has(t.texts(), /^closest first · within 50 km$/i, 'the list heading');
      t.tap('CCC3'); t.wait(300);
      t.eq(route(t), 'CCC3', 'the tapped one leads'); t.has(t.texts(), /^7700 emergency$/i, 'its status by the look line');
    } },
    { name: 'empty: clear skies, offline, loading; stale tag with last known aircraft', async run(t) {
      sky(t, []); await open(t);
      t.has(t.texts(), 'Clear skies', 'quiet'); t.has(t.texts(), 'Nothing flying within 50 km right now.', 'quiet words');
      t.ha.set(S, 'unavailable', { a: 'J[]' }); t.wait(300); t.has(t.texts(), 'Feed offline', 'offline');
      sky(t, [A], { error: 'timeout' }); await open(t);
      t.has(t.texts(), /offline · last known/i, 'the stale tag'); t.eq(route(t), 'Amsterdam → Edinburgh', 'the last aircraft stays');
      delete t.ha.s[S]; await open(t, { sensor: 'sensor.nothing_here' }); t.has(t.texts(), 'Loading', 'no state yet');
    } },
    { name: 'units: miles move the words and the circle', async run(t) {
      sky(t, [A, B, C]); await open(t);
      set(t, 'distanceUnit', 'mi'); set(t, 'speedUnit', 'kn'); t.wait(300);
      t.has(t.texts(), /^3\.7 mi NW$/, 'distance in miles'); t.has(t.texts(), /^460 kn$/, 'speed in knots');
      t.tap(more(t)); t.wait(300); t.has(t.texts(), /^closest first · within 30 mi$/i, 'the 30 mi circle');
      set(t, 'distanceUnit', 'km'); set(t, 'speedUnit', 'mph');
    } },
  ],
};
