/**
 * The moon, worked out locally (no API): phase, illumination, age, the next new and full moons, and today's moonrise
 * and moonset. Phase times use Meeus (Astronomical Algorithms, ch. 49) and land within a couple of minutes; positions
 * use the main lunar terms (about 0.2°), so rise and set times are good to a few minutes.
 *
 * ```ts
 * store.patch({ moon: moonData(Date.now(), { lat: 52.66, lon: 0.16 }) });
 * ```
 */
import { moonIllumination } from '@facet/icons';
import type { MoonData, MoonPhase, MoonPhaseEvent } from './types.js';

const RAD = Math.PI / 180;
const DAY = 86_400_000;
/** Mean length of the synodic month, days. */
export const SYNODIC = 29.530588861;
const J2000 = 2451545;
const toJd = (ms: number) => ms / DAY + 2440587.5;
const fromJd = (jd: number) => (jd - 2440587.5) * DAY;
const norm = (deg: number) => ((deg % 360) + 360) % 360;

// ---------------------------------------------------------------- phase times (Meeus ch. 49)
type Kind = 'new' | 'first' | 'full' | 'last';
const OFFSET: Record<Kind, number> = { new: 0, first: 0.25, full: 0.5, last: 0.75 };

/** Time (ms) of lunation `k` + the phase's offset; k = 0 is the new moon of 6 January 2000. */
function phaseTime(k: number, kind: Kind) {
  k += OFFSET[kind];
  const T = k / 1236.85;
  let jde =
    2451550.09766 + SYNODIC * k + 0.00015437 * T * T - 0.00000015 * T ** 3 + 0.00000000073 * T ** 4;
  const E = 1 - 0.002516 * T - 0.0000074 * T * T;
  const M = (2.5534 + 29.1053567 * k - 0.0000014 * T * T) * RAD;
  const Mp = (201.5643 + 385.81693528 * k + 0.0107582 * T * T + 0.00001238 * T ** 3) * RAD;
  const F = (160.7108 + 390.67050284 * k - 0.0016118 * T * T - 0.00000227 * T ** 3) * RAD;
  const O = (124.7746 - 1.56375588 * k + 0.0020672 * T * T) * RAD;
  const sin = Math.sin,
    cos = Math.cos;
  if (kind === 'new' || kind === 'full') {
    const n = kind === 'new';
    jde +=
      (n ? -0.4072 : -0.40614) * sin(Mp) +
      (n ? 0.17241 : 0.17302) * E * sin(M) +
      (n ? 0.01608 : 0.01614) * sin(2 * Mp) +
      (n ? 0.01039 : 0.01043) * sin(2 * F) +
      (n ? 0.00739 : 0.00734) * E * sin(Mp - M) -
      (n ? 0.00514 : 0.00515) * E * sin(Mp + M) +
      (n ? 0.00208 : 0.00209) * E * E * sin(2 * M) -
      0.00111 * sin(Mp - 2 * F) -
      0.00057 * sin(Mp + 2 * F) +
      0.00056 * E * sin(2 * Mp + M) -
      0.00042 * sin(3 * Mp) +
      0.00042 * E * sin(M + 2 * F) +
      0.00038 * E * sin(M - 2 * F) -
      0.00024 * E * sin(2 * Mp - M) -
      0.00017 * sin(O) -
      0.00007 * sin(Mp + 2 * M) +
      0.00004 * sin(2 * Mp - 2 * F) +
      0.00004 * sin(3 * M) +
      0.00003 * sin(Mp + M - 2 * F) +
      0.00003 * sin(2 * Mp + 2 * F) -
      0.00003 * sin(Mp + M + 2 * F) +
      0.00003 * sin(Mp - M + 2 * F) -
      0.00002 * sin(Mp - M - 2 * F) -
      0.00002 * sin(3 * Mp + M) +
      0.00002 * sin(4 * Mp);
  } else {
    jde +=
      -0.62801 * sin(Mp) +
      0.17172 * E * sin(M) -
      0.01183 * E * sin(Mp + M) +
      0.00862 * sin(2 * Mp) +
      0.00804 * sin(2 * F) +
      0.00454 * E * sin(Mp - M) +
      0.00204 * E * E * sin(2 * M) -
      0.0018 * sin(Mp - 2 * F) -
      0.0007 * sin(Mp + 2 * F) -
      0.0004 * sin(3 * Mp) -
      0.00034 * E * sin(2 * Mp - M) +
      0.00032 * E * sin(M + 2 * F) +
      0.00032 * E * sin(M - 2 * F) -
      0.00028 * E * E * sin(Mp + 2 * M) +
      0.00027 * E * sin(2 * Mp + M) -
      0.00017 * sin(O);
    const W =
      0.00306 -
      0.00038 * E * cos(M) +
      0.00026 * cos(Mp) -
      0.00002 * cos(Mp - M) +
      0.00002 * cos(Mp + M) +
      0.00002 * cos(2 * F);
    jde += kind === 'first' ? W : -W;
  }
  // terrestrial time -> UTC (ΔT ≈ 70 s in the 2020s)
  return fromJd(jde) - 70_000;
}

/** The principal phases (new, first quarter, full, last quarter) from just before `from` on, in order. */
function phasesAround(from: number, count: number) {
  const k0 = Math.floor((toJd(from) - 2451550.09766) / SYNODIC) - 1;
  const out: { kind: Kind; t: number }[] = [];
  for (let k = k0; out.length < count + 8; k++)
    for (const kind of ['new', 'first', 'full', 'last'] as Kind[])
      out.push({ kind, t: phaseTime(k, kind) });
  return out;
}

// ---------------------------------------------------------------- positions
/** Sun's ecliptic longitude (rad) at `d` days since J2000. */
function sunLon(d: number) {
  const M = (357.5291 + 0.98560028 * d) * RAD;
  const C = (1.9148 * Math.sin(M) + 0.02 * Math.sin(2 * M) + 0.0003 * Math.sin(3 * M)) * RAD;
  return M + C + 102.9372 * RAD + Math.PI;
}

/** Moon's ecliptic longitude and latitude (rad) at `d` days since J2000, with the main periodic terms. */
function moonEcl(d: number) {
  const L = 218.316 + 13.176396 * d;
  const Mp = (134.963 + 13.064993 * d) * RAD;
  const M = (357.5291 + 0.98560028 * d) * RAD;
  const F = (93.272 + 13.22935 * d) * RAD;
  const D = (297.8502 + 12.19074912 * d) * RAD;
  const lon =
    L +
    6.289 * Math.sin(Mp) +
    1.274 * Math.sin(2 * D - Mp) +
    0.658 * Math.sin(2 * D) +
    0.214 * Math.sin(2 * Mp) -
    0.186 * Math.sin(M) -
    0.114 * Math.sin(2 * F);
  const lat = 5.128 * Math.sin(F) + 0.28 * Math.sin(Mp + F) + 0.277 * Math.sin(Mp - F);
  return { lon: lon * RAD, lat: lat * RAD };
}

/** Moon's geocentric altitude (rad) at `ms` from latitude/longitude (degrees, east positive). */
function moonAlt(ms: number, lat: number, lon: number) {
  const d = toJd(ms) - J2000;
  const { lon: l, lat: b } = moonEcl(d);
  const e = 23.4397 * RAD;
  const ra = Math.atan2(Math.sin(l) * Math.cos(e) - Math.tan(b) * Math.sin(e), Math.cos(l));
  const dec = Math.asin(Math.sin(b) * Math.cos(e) + Math.cos(b) * Math.sin(e) * Math.sin(l));
  const H = (280.16 + 360.9856235 * d + lon) * RAD - ra;
  const phi = lat * RAD;
  return Math.asin(Math.sin(phi) * Math.sin(dec) + Math.cos(phi) * Math.cos(dec) * Math.cos(H));
}

/** How far through the synodic month (0-1) the moon is at `ms`, from the sun-moon elongation. */
export function moonFrac(ms: number) {
  const d = toJd(ms) - J2000;
  return norm((moonEcl(d).lon - sunLon(d)) / RAD) / 360;
}

/** A principal phase that falls in a day of `moonDays`. */
export type MoonDayEvent = 'new' | 'first' | 'full' | 'last';

/** One local day of the long-range chart: the moon at 21:00 and the principal phase that falls in that day, if any. */
export interface MoonDay {
  /** 21:00 local on that day, ms. */
  t: number;
  frac: number;
  /** 0-100, whole per cent. */
  illumination: number;
  event: MoonDayEvent | null;
}

/** `n` local days from the day containing `start`: the evening moon (21:00) and any new, quarter or full moon in it. */
export function moonDays(start: number, n: number): MoonDay[] {
  const d0 = new Date(start);
  d0.setHours(0, 0, 0, 0);
  const out: MoonDay[] = [];
  for (let i = 0; i < n; i++) {
    const a = new Date(d0);
    a.setDate(d0.getDate() + i);
    const b = new Date(a);
    b.setDate(a.getDate() + 1);
    const eve = new Date(a);
    eve.setHours(21);
    const f0 = moonFrac(a.getTime()),
      f1 = moonFrac(b.getTime()),
      frac = moonFrac(eve.getTime());
    const across = (x: number) => f0 < x && f1 >= x;
    const event: MoonDayEvent | null =
      f1 < f0
        ? 'new'
        : across(0.25)
          ? 'first'
          : across(0.5)
            ? 'full'
            : across(0.75)
              ? 'last'
              : null;
    out.push({
      t: eve.getTime(),
      frac,
      illumination: Math.round(moonIllumination(frac) * 100),
      event,
    });
  }
  return out;
}

/**
 * Moonrise and moonset on the local day containing `ms` (the device's time zone). Either can be null: on some days
 * the moon doesn't rise or doesn't set. Altitude threshold +0.125° (parallax minus refraction and semi-diameter).
 */
export function moonTimes(ms: number, lat: number, lon: number) {
  const start = new Date(ms);
  start.setHours(0, 0, 0, 0);
  const t0 = start.getTime();
  const h0 = 0.125 * RAD;
  const STEP = 10 * 60_000;
  let rise: number | null = null,
    set: number | null = null;
  let prev = moonAlt(t0, lat, lon) - h0;
  for (let t = t0 + STEP; t <= t0 + DAY; t += STEP) {
    const cur = moonAlt(t, lat, lon) - h0;
    if (prev < 0 !== cur < 0) {
      const at = t - STEP + (STEP * prev) / (prev - cur);
      if (cur > 0 && rise === null) rise = at;
      if (cur < 0 && set === null) set = at;
    }
    prev = cur;
  }
  return { rise, set };
}

// ---------------------------------------------------------------- the lot
const WITHIN = 12 * 3600_000;

/** Phase name, using Home Assistant's `moon` sensor states: a principal phase within 12 hours of its exact time. */
export function moonPhaseName(frac: number, near?: Kind | null): MoonPhase {
  if (near)
    return near === 'new'
      ? 'new_moon'
      : near === 'full'
        ? 'full_moon'
        : near === 'first'
          ? 'first_quarter'
          : 'last_quarter';
  if (frac < 0.25) return 'waxing_crescent';
  if (frac < 0.5) return 'waxing_gibbous';
  if (frac < 0.75) return 'waning_gibbous';
  return 'waning_crescent';
}

/** "Waxing gibbous". */
export const MOON_PHASE_TEXT: Record<MoonPhase, string> = {
  new_moon: 'New moon',
  waxing_crescent: 'Waxing crescent',
  first_quarter: 'First quarter',
  waxing_gibbous: 'Waxing gibbous',
  full_moon: 'Full moon',
  waning_gibbous: 'Waning gibbous',
  last_quarter: 'Last quarter',
  waning_crescent: 'Waning crescent',
};

const EVENT: Record<Kind, MoonPhaseEvent['phase']> = {
  new: 'new_moon',
  first: 'first_quarter',
  full: 'full_moon',
  last: 'last_quarter',
};

/**
 * Everything the Moon screen shows, for `now` at a place. Without a place there are no rise and set times, and the
 * glyph assumes the northern hemisphere.
 */
export function moonData(
  now: number | Date = Date.now(),
  at?: { lat: number; lon: number },
): MoonData {
  const ms = typeof now === 'number' ? now : now.getTime();
  const all = phasesAround(ms, 4);
  const lastNew = [...all].reverse().find((p) => p.kind === 'new' && p.t <= ms)!;
  const nextNew = all.find((p) => p.kind === 'new' && p.t > ms)!;
  const nextFull = all.find((p) => p.kind === 'full' && p.t > ms)!;
  const near = all.find((p) => Math.abs(p.t - ms) < WITHIN)?.kind ?? null;
  const frac = moonFrac(ms);
  const times = at ? moonTimes(ms, at.lat, at.lon) : { rise: null, set: null };
  // the next rise and set from now: today's if still to come, else tomorrow's (or the day after)
  const ahead = at
    ? [times, moonTimes(ms + DAY, at.lat, at.lon), moonTimes(ms + 2 * DAY, at.lat, at.lon)]
    : [];
  const next = (k: 'rise' | 'set') =>
    ahead.map((x) => x[k]).find((t) => t !== null && t > ms) ?? null;
  const iso = (t: number | null) => (t === null ? null : new Date(t).toISOString());
  return {
    phase: moonPhaseName(frac, near),
    frac,
    illumination: Math.round(moonIllumination(frac) * 1000) / 10,
    age: Math.round(((ms - lastNew.t) / DAY) * 10) / 10,
    next_new: iso(nextNew.t)!,
    next_full: iso(nextFull.t)!,
    upcoming: all
      .filter((p) => p.t > ms)
      .slice(0, 4)
      .map((p) => ({ phase: EVENT[p.kind], t: iso(p.t)! })),
    rise: iso(times.rise),
    set: iso(times.set),
    next_rise: iso(next('rise')),
    next_set: iso(next('set')),
    south: at ? at.lat < 0 : false,
  };
}
