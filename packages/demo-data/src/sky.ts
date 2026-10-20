// The mock house's sky: made-up flights crossing the 50 km circle on straight lines, so the Overhead screen moves.
// Each flight passes home at its closest distance `pass` (km, + = to the right of its track) and loops every `period`
// seconds; only the ones inside the circle are listed. Airlines are fictional; the cities are just places.
import type { Aircraft, Airport, OverheadData } from '@facet/ui';

/** Pin the sky: `live` (default) = flights crossing, `overhead` = a jet right overhead, `emergency` = a 7700 in
 *  range, `quiet` = nothing within the circle. */
export type OverheadScene = 'live' | 'overhead' | 'emergency' | 'quiet';

const ap = (iata: string, city: string): Airport => ({ iata, icao: '', name: city, city });
type Plan = Partial<Aircraft> & {
  hex: string;
  alt: number;
  gs: number;
  track: number;
  pass: number;
  phase: number;
};
const PLANS: Plan[] = [
  {
    hex: '40a1c2',
    callsign: 'NWD214',
    airline: 'Northwind',
    flightNo: 'NW214',
    type: 'A20N',
    model: 'Airbus A320neo',
    from: ap('AMS', 'Amsterdam'),
    to: ap('EDI', 'Edinburgh'),
    alt: 34000,
    gs: 450,
    track: 320,
    pass: 6,
    phase: 0,
  },
  {
    hex: '40b7d0',
    callsign: 'SKC88',
    airline: 'Skyline Cargo',
    flightNo: 'SK88',
    type: 'B763',
    model: 'Boeing 767-300F',
    from: ap('BRU', 'Brussels'),
    to: ap('EMA', 'East Midlands'),
    alt: 21000,
    gs: 380,
    track: 290,
    pass: -18,
    phase: 0.35,
    vs: -1400,
  },
  {
    hex: '40c311',
    callsign: 'LRK902',
    airline: 'Lark Air',
    flightNo: 'LK902',
    type: 'B38M',
    model: 'Boeing 737 MAX 8',
    from: ap('KRK', 'Kraków'),
    to: ap('LBA', 'Leeds'),
    alt: 32000,
    gs: 455,
    track: 302,
    pass: 22,
    phase: 0.6,
  },
  {
    hex: '40d9e4',
    callsign: 'GOLKA',
    reg: 'G-OLKA',
    type: 'C172',
    model: 'Cessna 172',
    kind: 'light',
    alt: 2300,
    gs: 95,
    track: 20,
    pass: 4,
    phase: 0.2,
  },
  {
    hex: '40e012',
    callsign: 'MED21',
    reg: 'G-MEDX',
    type: 'EC35',
    model: 'Airbus H135',
    kind: 'heli',
    operator: 'County air ambulance',
    alt: 1500,
    gs: 120,
    track: 75,
    pass: -9,
    phase: 0.8,
  },
  {
    hex: '40f5a8',
    callsign: 'NWD77',
    airline: 'Northwind',
    flightNo: 'NW77',
    type: 'A21N',
    model: 'Airbus A321neo',
    from: ap('MAN', 'Manchester'),
    to: ap('FUE', 'Fuerteventura'),
    alt: 11700,
    gs: 300,
    track: 150,
    pass: 30,
    phase: 0.5,
    vs: 1800,
  },
];
const SPAN = 120; // km of track per loop (60 either side of the closest point)

function at(p: Plan, now: number, phaseShift = 0): Aircraft {
  const v = (p.gs * 1.852) / 3600, // km/s
    period = SPAN / v,
    t = (((now / 1000 / period + p.phase + phaseShift) % 1) + 1) % 1,
    along = t * SPAN - SPAN / 2;
  const th = (p.track * Math.PI) / 180,
    ux = Math.sin(th),
    uy = Math.cos(th);
  const x = p.pass * uy + along * ux,
    y = -p.pass * ux + along * uy;
  const km = Math.hypot(x, y),
    brg = ((Math.atan2(x, y) * 180) / Math.PI + 360) % 360;
  const elev = (Math.atan2((p.alt * 0.3048) / 1000, Math.max(km, 0.05)) * 180) / Math.PI;
  const { pass: _p, phase: _ph, ...a } = p;
  return {
    callsign: null,
    reg: null,
    type: null,
    model: null,
    operator: null,
    airline: null,
    flightNo: null,
    from: null,
    to: null,
    vs: 0,
    squawk: '2000',
    emergency: null,
    kind: 'jet',
    military: false,
    ...a,
    km: Math.round(km * 10) / 10,
    brg: Math.round(brg),
    elev: Math.round(elev * 10) / 10,
  };
}

export function skyWith(now: number, scene: OverheadScene = 'live'): OverheadData {
  const base = { at: new Date(now).toISOString(), radiusKm: 50, source: 'mock house' };
  if (scene === 'quiet') return { ...base, aircraft: [] };
  let list = PLANS.map((p) => at(p, now)).filter((a) => a.km <= 50);
  if (scene === 'overhead') list.push({ ...at(PLANS[0]!, 0), km: 3.2, brg: 300, elev: 72.7 });
  if (scene === 'emergency')
    list.push({
      ...at(PLANS[2]!, now),
      hex: '40c3ee',
      callsign: 'LRK31',
      flightNo: 'LK31',
      from: ap('STN', 'London'),
      to: ap('GDN', 'Gdańsk'),
      alt: 9800,
      vs: -2400,
      km: 28,
      brg: 175,
      elev: 6.1,
      squawk: '7700',
      emergency: 'general',
    });
  list = list.sort((a, b) => Number(!!b.emergency) - Number(!!a.emergency) || b.elev - a.elev);
  return { ...base, aircraft: list };
}
