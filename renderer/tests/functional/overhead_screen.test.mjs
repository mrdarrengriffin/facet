// Overhead screen (specs/overhead.json: header Now | Radar; Now = the Flight hero, also up there, all aircraft; Radar =
// the big Sky radar; All aircraft list; the detail view), on the flights feed panel.flights (core/flights_source.cpp,
// CONTRACT.md C51 / C51a) and its action flights.select, and its Home card (C52). Sources: FlightRadar24 and an ADS-B
// receiver through the fake Home Assistant (tools/lib/fakeha.mjs FakeHA), adsb.lol through the fake web (FakeWeb): never
// a real Home Assistant or the real internet. S / R are not real sensors. Approved original: Astro
// packages/ui/src/screens/overhead.ts.
const S = 'sensor.test_fr24', R = 'sensor.test_adsb';
const HOME = [52.0, 0.0];
// a point km away on a bearing from home (the flat-earth step is plenty within 100 km)
const at = (km, brg) => {
  const a = (brg * Math.PI) / 180, dlat = (km * Math.cos(a)) / 111.2, dlon = (km * Math.sin(a)) / (111.2 * Math.cos((HOME[0] * Math.PI) / 180));
  return [HOME[0] + dlat, HOME[1] + dlon];
};
// a FlightRadar24 flight (the integration's attribute fields)
const fr = (o) => {
  const [lat, lon] = at(o.km, o.brg);
  return { id: o.hex + 'x', aircraft_icao_24bit: o.hex, callsign: o.cs, flight_number: o.fn ?? '', aircraft_code: o.type ?? 'A20N', aircraft_model: o.model ?? 'Airbus A320neo',
    aircraft_registration: o.reg ?? 'G-TEST', airline: o.airline ?? 'Test Air', airline_short: o.airline ?? 'Test Air', airport_origin_city: o.from ?? null,
    airport_destination_city: o.to ?? null, latitude: lat, longitude: lon, altitude: o.alt ?? 30000, ground_speed: o.gs ?? 450, vertical_speed: o.vs ?? 0,
    heading: o.track ?? 90, squawk: o.squawk ?? null, on_ground: o.ground ? 1 : 0, distance: o.km };
};
const A = { hex: 'aaa111', cs: 'AAA1', km: 6, brg: 300, alt: 29000, from: 'Amsterdam', to: 'Edinburgh', model: 'Airbus A320neo', airline: 'easyJet', reg: 'G-UZHA' };
const B = { hex: 'bbb222', cs: 'BBB2', km: 12.5, brg: 90, alt: 32000, vs: -1200, from: 'Kraków', to: 'Leeds', type: 'B38M', model: 'Boeing 737 MAX 8', airline: 'Ryanair', reg: 'EI-IHR', track: 302 };
const C = { hex: 'ccc333', cs: 'CCC3', km: 30, brg: 180, alt: 18000, squawk: '7700', type: 'A332', model: 'Airbus Voyager KC2', airline: "O'Neill Air" };
const D = { hex: 'ddd444', cs: 'DDD4', km: 70, brg: 0, alt: 36000 };  // past 50 km
const LOW = { hex: 'eee555', cs: 'EEE5', km: 4, brg: 45, alt: 1500, type: 'C172', model: 'Cessna 172' };
// Home Assistant sends a list attribute as Python text: the adapter reads it
const py = (v) => JSON.stringify(v).replace(/"([^"]*)"/g, (_, s) => (s.includes("'") ? `"${s}"` : `'${s}'`)).replace(/null/g, 'None');
const home = (t) => { t.ha.s['zone.home'] = { '': 'zoning', latitude: String(HOME[0]), longitude: String(HOME[1]) }; };
const sky = (t, list, { asJson = false } = {}) => { home(t); t.ha.s[S] = { '': String(list.length), flights: asJson ? JSON.stringify(list.map(fr)) : py(list.map(fr)) }; };
async function open(t, params = {}) { await t.mount('spec', { screen: 'overhead', source: 'fr24', sensor: S, ...params }); return t; }
const routeOf = (t) => t.texts().find((x) => /→|^[A-Z]{3}\d$/.test(x));
const radar = (t) => t.comps().find((x) => x.ids.includes('sky_radar') && t.visible(x));
// a plane on the big radar (its box; rmax = half the box - half a plane: layout sky-plane-lg 32 / 24; square-root scale)
const plane = (t, km, brg, edge = 50) => {
  const c = radar(t), R0 = c.w / 2, glyph = t.b.S > 600 ? 32 : 24;
  const r = (R0 - glyph / 2) * Math.sqrt(km / edge), a = (brg * Math.PI) / 180;
  return { at: [c.x + R0 + r * Math.sin(a), c.y + R0 - r * Math.cos(a)] };
};
const more = (t) => { const o = t.find(/all aircraft · \d/i); return { at: [o.x + o.w / 2, o.y - 8] }; };
const set = (t, id, v) => t.b.str(id, (p) => t.b.str(v, (q) => t.b.m._fw_setting(p, q)));
const lab = (t, cmd, arg = '') => t.b.str(cmd, (c) => t.b.str(arg, (a) => t.b.m.UTF8ToString(t.b.m._fw_lab(t.b.stage, c, a))));
const feed = (t, a) => lab(t, 'value', `panel.flights|${a}`);
const planes = (t) => JSON.parse(feed(t, 'planes') || '[]');
const deck = (params) => ({ v: 2, rev: 9, start: 'h', pages: [
  { id: 'h', kind: 'home', name: 'Test', cards: [{ t: 'page', page: 'o', size: 'm' }] },
  { id: 'o', kind: 'overhead', params }] });
// readsb's JSON (adsb.lol / a receiver's aircraft.json)
const rs = (o) => { const [lat, lon] = at(o.km, o.brg); return { hex: o.hex, flight: (o.cs + '        ').slice(0, 8), r: o.reg ?? '', t: o.type ?? 'A20N', alt_baro: o.ground ? 'ground' : o.alt ?? 30000, gs: o.gs ?? 450, track: o.track ?? 90, baro_rate: o.vs ?? 0, squawk: o.squawk ?? '2000', emergency: o.em ?? 'none', category: o.cat ?? 'A3', dbFlags: o.mil ? 1 : 0, lat, lon, dst: o.km / 1.852, dir: o.brg }; };

export default {
  id: 'overhead_screen', family: 'screens', about: 'The Overhead screen on the flights feed: many, none, a tap on a flight, range and altitude, the sources, the feed states, units, the Home card.',
  tests: [
    { name: 'many flights (FlightRadar24 as Python text): the closest leads, the next two, all aircraft · 3', async run(t) {
      sky(t, [B, C, A, D]); await open(t);
      t.eq(routeOf(t), 'Amsterdam → Edinburgh', 'the closest leads');
      t.has(t.texts(), /^also up there$/i, 'also up there'); t.has(t.texts(), 'Kraków → Leeds', 'the next one'); t.has(t.texts(), 'CCC3', 'no route: the callsign');
      t.has(t.texts(), /7700 emergency/i, 'the emergency on its row'); t.has(t.texts(), /all aircraft · 3/i, 'the More row counts (70 km is past the 50 km circle)');
      t.has(t.texts(), /^easyJet · AAA1 · Airbus A320neo$/, 'who flies it');
      t.eq(planes(t).length, 3, 'three on the radar'); t.eq(planes(t).at(-1)[3] & 1, 1, 'the lead is drawn last');
      t.eq(t.ha.calls.length, 0, 'nothing goes to Home Assistant');
    } },
    { name: 'the same list as JSON, with a name that has an apostrophe', async run(t) {
      sky(t, [C], { asJson: true }); await open(t);
      t.eq(routeOf(t), 'CCC3', 'read'); t.has(t.texts(), /O'Neill Air/, 'the apostrophe kept');
      sky(t, [C]); await open(t);
      t.has(t.texts(), /O'Neill Air/, 'and from Python text');
    } },
    { name: 'no flights: clear skies on the screen and the card', async run(t) {
      sky(t, []); await open(t);
      t.has(t.texts(), 'Clear skies', 'quiet'); t.has(t.texts(), 'Nothing flying within 50 km right now.', 'quiet words');
      t.ok(!t.texts().some((x) => /also up there/i.test(x)), 'no rows');
      await t.device(deck({ source: 'fr24', sensor: S }));
      const c = t.part('card'); t.ok(c && c.texts.some((x) => x === 'Clear skies'), 'the card says so');
    } },
    { name: 'a tap on a flight: a row selects it, the hero again opens its details, Back returns', async run(t) {
      sky(t, [A, B, C]); await open(t);
      t.tap('Kraków → Leeds'); t.wait(300);
      t.eq(routeOf(t), 'Kraków → Leeds', 'the tapped one leads');
      t.has(t.texts(), 'Amsterdam → Edinburgh', 'the old lead is a row now');
      t.has(t.texts(), /^descending$/i, 'its vertical speed reads descending');
      t.tap('Kraków → Leeds'); t.wait(300);
      t.has(t.texts(), /^BBB2$/, 'the detail header is the callsign');
      for (const [l, v] of [['registration', 'EI-IHR'], ['operator', 'Ryanair'], ['vertical speed', '−1,200 ft/min'], ['heading', '302° NW'], ['aircraft', /^Boeing 737/]]) {
        t.has(t.texts(), new RegExp(`^${l}$`, 'i'), `fact ${l}`); t.has(t.texts(), v, `${l} value`);
      }
      t.has(t.texts(), /^BBB222 · data FlightRadar24$/i, 'the source line');
      t.tap({ clk: 0 }); t.wait(300);
      t.has(t.texts(), /^now$/i, 'Back: the Now | Radar tabs again'); t.eq(routeOf(t), 'Kraków → Leeds', 'the selection stays');
    } },
    { name: 'a tap on a plane on the radar: picked by hex, again = its details', async run(t) {
      sky(t, [A, B, C]); await open(t);
      t.tap(/^radar$/i); t.wait(300);
      t.ok(radar(t) && radar(t).w === (t.b.S > 600 ? 576 : 384), 'the big radar (576 / 384)');
      const ev = t.tap(plane(t, 12.5, 90)); t.wait(300);
      t.emitted(ev, 'pick', 'bbb222', 'the plane picked by hex');
      t.tap(plane(t, 12.5, 90)); t.wait(300);
      t.has(t.texts(), /^BBB2$/, 'the selected plane again: its details');
      t.tap({ clk: 0 }); t.wait(300); t.has(t.texts(), /^radar$/i, 'Back: the radar view');
    } },
    { name: 'range change: 10 km keeps only what is inside it; the rings follow', async run(t) {
      sky(t, [A, B, C]); await open(t, { range: '10' });
      t.eq(planes(t).length, 1, 'only the one 6 km out'); t.eq(feed(t, 'range'), '10', 'the radar is told');
      t.ok(!t.texts().some((x) => /also up there/i.test(x)), 'no others');
      await open(t, { range: '100' });
      t.eq(planes(t).length, 3, '100 km: all three');
      t.tap(more(t)); t.wait(300); t.has(t.texts(), /^closest first · within 100 km$/i, 'the list says how far');
    } },
    { name: 'lowest altitude: lower aircraft and those on the ground are left out', async run(t) {
      sky(t, [A, LOW, { ...B, ground: true }]); await open(t);
      t.eq(routeOf(t), 'EEE5', 'all flying: the low one is closest');
      t.ok(!planes(t).some((p) => p[5] === 'bbb222'), 'on the ground: never shown');
      await open(t, { min_alt: '3000' });
      t.eq(routeOf(t), 'Amsterdam → Edinburgh', '3,000 ft: the low one is gone');
    } },
    { name: 'adsb.lol: the panel reads only the circle round home, every 10 s; offline keeps the last aircraft', async run(t) {
      home(t);
      t.web.serve(/^https:\/\/api\.adsb\.lol\/v2\/point\//, { ac: [rs(A), rs({ ...C, em: 'general' }), rs(LOW)] });
      await open(t, { source: 'adsb_lol', range: '25' });
      t.wait(300);
      t.ok(t.web.asked.length >= 1, 'it asked');
      t.eq(t.web.asked[0], 'https://api.adsb.lol/v2/point/52.000/0.000/14', 'home and a radius of 25 km in nautical miles, rounded up');
      t.eq(routeOf(t), 'EEE5', 'the closest leads (no route from this source: the callsign)');
      t.ok(!planes(t).some((p) => p[5] === 'ccc333'), '30 km is outside 25 km');
      const n = t.web.asked.length; t.wait(10500);
      t.ok(t.web.asked.length === n + 1, `one more read after 10 s (${t.web.asked.length - n})`);
      t.web.status(/adsb\.lol/, 'timeout'); t.wait(10500);
      t.has(t.texts(), /offline · last known/i, 'the stale tag'); t.eq(routeOf(t), 'EEE5', 'the last aircraft stay');
    } },
    { name: 'an ADS-B receiver in Home Assistant (a REST sensor on aircraft.json)', async run(t) {
      home(t); t.ha.s[R] = { '': 'ok', aircraft: JSON.stringify([rs(A), rs({ ...B, cat: 'A7' })]) };
      await open(t, { source: 'adsb_ha', sensor: R });
      t.eq(routeOf(t), 'AAA1', 'read'); t.tap(/^BBB2$/); t.wait(300); t.has(t.texts(), /^helicopter$/i, 'category A7 = a helicopter');
    } },
    { name: 'feed states: loading, not found, unavailable, no home, all in the feed words', async run(t) {
      home(t); t.ha.absent.add(S); await open(t);
      t.has(t.texts(), 'Loading', 'no answer yet');
      t.ha.absent.delete(S); t.ha.set(S, ''); t.wait(300); t.has(t.texts(), 'Not found', 'no such sensor');
      t.ha.set(S, 'unavailable'); t.wait(300); t.has(t.texts(), 'Unavailable', 'there, no state');
      sky(t, [A]); t.ha.s['zone.home'] = { '': 'zoning', latitude: '', longitude: '' }; await open(t);
      t.has(t.texts(), 'Offline', 'nowhere to centre the radar: not set up');
      await open(t, { lat: '52.0', lon: '0.0' }); t.eq(routeOf(t), 'Amsterdam → Edinburgh', 'the settings\' place works without HA\'s home');
    } },
    { name: 'units: miles move the words and the circle', async run(t) {
      sky(t, [A, B, C]); await open(t);
      set(t, 'distanceUnit', 'mi'); set(t, 'speedUnit', 'kn'); t.wait(300);
      t.has(t.texts(), /^3\.7 mi NW$/, 'distance in miles'); t.has(t.texts(), /^450 kn$/, 'speed in knots');
      t.eq(feed(t, 'unit'), 'mi', 'the radar in miles'); t.eq(planes(t).length, 3, '50 mi holds all three');
      set(t, 'distanceUnit', 'km'); set(t, 'speedUnit', 'mph');
    } },
    { name: 'Home card (C52): the lead, its height and where, the radar on the same feed', async run(t) {
      sky(t, [A, B]);
      await t.device(deck({ source: 'fr24', sensor: S }));
      const c = t.part('card');
      t.ok(c, 'the page card'); t.ok(c.texts.some((x) => x === 'Amsterdam → Edinburgh'), 'the route');
      t.ok(c.texts.some((x) => /^29,000 ft · /.test(x)), 'height and where');
      t.ok(t.comps().some((x) => x.ids.includes('sky_radar')), 'the radar on the card');
    } },
  ],
};
