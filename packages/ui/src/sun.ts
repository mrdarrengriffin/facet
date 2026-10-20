/**
 * The sun, worked out locally (no API): today's sunrise and sunset, first and last light (civil twilight, -6°), the
 * day length and how it changed since yesterday, and tomorrow's sunrise. The sunrise equation (NOAA, simplified) with
 * -0.833° for refraction and the sun's radius lands within about a minute.
 *
 * ```ts
 * store.patch({ sun: sunData(Date.now(), { lat: 52.66, lon: 0.16 }) });
 * ```
 */
import type { SunData } from './types.js';

const RAD = Math.PI / 180;
const DAY = 86_400_000;
const J2000 = 2451545;
const toJd = (ms: number) => ms / DAY + 2440587.5;
const fromJd = (jd: number) => (jd - 2440587.5) * DAY;

/** Sunrise and sunset (ms) of the local day that holds `ms`, for the sun at `alt` degrees; null = it doesn't cross. */
export function sunTimes(ms: number, lat: number, lon: number, alt = -0.833) {
  const noon = new Date(ms);
  noon.setHours(12, 0, 0, 0);
  const n = Math.round(toJd(noon.getTime()) - J2000);
  const js = n - lon / 360; // mean solar noon (east positive)
  const mDeg = (357.5291 + 0.98560028 * js) % 360,
    m = mDeg * RAD;
  const c = 1.9148 * Math.sin(m) + 0.02 * Math.sin(2 * m) + 0.0003 * Math.sin(3 * m);
  const lam = ((mDeg + c + 180 + 102.9372) % 360) * RAD;
  const jt = J2000 + js + 0.0053 * Math.sin(m) - 0.0069 * Math.sin(2 * lam);
  const dec = Math.asin(Math.sin(lam) * Math.sin(23.4397 * RAD));
  const cosW =
    (Math.sin(alt * RAD) - Math.sin(lat * RAD) * Math.sin(dec)) /
    (Math.cos(lat * RAD) * Math.cos(dec));
  if (cosW < -1 || cosW > 1) return { rise: null, set: null, noon: fromJd(jt) };
  const w = Math.acos(cosW) / RAD / 360;
  return { rise: fromJd(jt - w), set: fromJd(jt + w), noon: fromJd(jt) };
}

const iso = (t: number | null) => (t === null ? null : new Date(t).toISOString());
const len = (t: { rise: number | null; set: number | null }) =>
  t.rise !== null && t.set !== null ? t.set - t.rise : null;

/** Today's sun at a place. */
export function sunData(now: number, at: { lat: number; lon: number }): SunData {
  const t = sunTimes(now, at.lat, at.lon),
    y = sunTimes(now - DAY, at.lat, at.lon),
    tm = sunTimes(now + DAY, at.lat, at.lon),
    c = sunTimes(now, at.lat, at.lon, -6);
  const length = len(t),
    ylen = len(y);
  return {
    rise: iso(t.rise),
    set: iso(t.set),
    dawn: iso(c.rise),
    dusk: iso(c.set),
    length,
    delta: length !== null && ylen !== null ? length - ylen : null,
    next_rise: iso(tm.rise),
  };
}

/** The next sunrise or sunset from `now`, and how far through the day it is (0-1; null while the sun is down). */
export function sunNow(s: SunData, now = Date.now()) {
  const rise = s.rise ? Date.parse(s.rise) : null,
    set = s.set ? Date.parse(s.set) : null,
    tom = s.next_rise ? Date.parse(s.next_rise) : null;
  const next =
    rise !== null && now < rise
      ? { kind: 'sunrise' as const, t: rise }
      : set !== null && now < set
        ? { kind: 'sunset' as const, t: set }
        : tom !== null
          ? { kind: 'sunrise' as const, t: tom }
          : null;
  const frac =
    rise !== null && set !== null && now >= rise && now < set ? (now - rise) / (set - rise) : null;
  return { next, frac };
}

/** "10 h 42 min". */
export const dayLength = (ms: number) => {
  const m = Math.round(ms / 60_000);
  return `${Math.floor(m / 60)} h ${String(m % 60).padStart(2, '0')} min`;
};
/** "in 25 min", "in 2 h 14 min". */
export const inTime = (ms: number) => {
  const m = Math.max(0, Math.round(ms / 60_000));
  return m < 60 ? `in ${m} min` : `in ${Math.floor(m / 60)} h ${m % 60} min`;
};
/** "2 min 10 s shorter" / "longer" than yesterday. */
export const dayDelta = (ms: number) => {
  const s = Math.round(Math.abs(ms) / 1000),
    m = Math.floor(s / 60);
  return `${m ? `${m} min ` : ''}${s % 60} s ${ms < 0 ? 'shorter' : 'longer'}`;
};
