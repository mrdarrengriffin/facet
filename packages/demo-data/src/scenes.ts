// Scenes: variations on the mock house for playgrounds and tests. Each function returns plain data to patch into the
// store, so a screen can be shown playing or paused, wet or dry, quiet or mid-storm without touching the DOM.
import type {
  Agent,
  AgentKind,
  Band,
  EnergyData,
  GithubData,
  LightningData,
  MediaPlayback,
  MediaState,
  Strike,
  MoonData,
  MoonPhase,
  Timer,
  WeatherData,
} from '@facet/ui';
import { moonData, moonFrac, SYNODIC } from '@facet/ui';
import * as D from './data.js';

const iso = (t: number) => new Date(t).toISOString();

// ---------------------------------------------------------------- media
export interface MediaScene {
  state?: MediaPlayback;
  /** Index into TRACKS. */
  track?: number;
  /** 0-100. */
  volume?: number;
}
/** Now-playing state for a track, at the start of its second minute. */
export function mediaWith(now: number, o: MediaScene = {}): MediaState {
  const t = D.TRACKS[(o.track ?? 0) % D.TRACKS.length]!;
  const idle = o.state === 'idle' || o.state === 'off';
  return {
    state: o.state ?? 'playing',
    title: idle ? null : t.title,
    artist: idle ? null : t.artist,
    art: idle ? null : t.art,
    duration: idle ? null : t.duration,
    position: idle ? null : 74,
    positionAt: now,
    volume: (o.volume ?? 42) / 100,
  };
}

// ---------------------------------------------------------------- agents
const EXTRA: Record<AgentKind, { title: string; status: string }[]> = {
  working: [
    { title: 'Photo backup', status: 'working · 42% ctx' },
    { title: 'Firmware build', status: 'working · 18% ctx' },
    { title: 'Podcast transcripts', status: 'working · 67% ctx' },
    { title: 'Map tiles refresh', status: 'working · 9% ctx' },
    { title: 'Inbox triage', status: 'working · 55% ctx' },
    { title: 'Seed catalogue', status: 'working · 23% ctx' },
  ],
  needs: [
    { title: 'Recipe importer', status: 'needs you · 12m' },
    { title: 'Invoice sorter', status: 'needs you · 2h' },
    { title: 'Holiday planner', status: 'needs you · 5m' },
    { title: 'Library returns', status: 'needs you · 1d' },
    { title: 'Bike service log', status: 'needs you · 3h' },
    { title: 'Guest Wi-Fi notes', status: 'needs you · 40m' },
  ],
  done: [
    { title: 'Garden sensors', status: 'done · 1h ago' },
    { title: 'Calendar tidy-up', status: 'done · 5h ago' },
    { title: 'Bird feeder camera', status: 'done · 1d ago' },
    { title: 'Docs spellcheck', status: 'done · 3d ago' },
    { title: 'Receipt scanner', status: 'done · 20m ago' },
    { title: 'Playlist mixer', status: 'done · 2d ago' },
  ],
  idle: [
    { title: 'Plant watering plan', status: 'idle · 1w ago' },
    { title: 'Old photo tagging', status: 'idle · 2w ago' },
    { title: 'Energy report', status: 'idle · 4d ago' },
    { title: 'Bookshelf index', status: 'idle · 1mo ago' },
    { title: 'Recipe scaler', status: 'idle · 6d ago' },
    { title: 'Weather log', status: 'idle · 3w ago' },
  ],
};
/** A list of agents with this many in each mood (up to 6 each). */
export function agentsWith(counts: Partial<Record<AgentKind, number>>): Agent[] {
  const out: Agent[] = [];
  for (const kind of ['needs', 'working', 'done', 'idle'] as const) {
    const n = Math.max(0, Math.min(EXTRA[kind].length, Math.round(counts[kind] ?? 0)));
    out.push(...EXTRA[kind].slice(0, n).map((a) => ({ ...a, kind })));
  }
  return out;
}

// ---------------------------------------------------------------- timer
export type TimerScene = 'running' | 'done' | 'none';
/** No timer, one running with `minutes` in total (a third gone), or one that has just finished. */
export function timersWith(now: number, state: TimerScene, minutes = 12, label = 'Bread'): Timer[] {
  const total = Math.max(1, minutes) * 60;
  if (state === 'none') return [];
  if (state === 'done') return [{ label, endsAt: now - 5_000, total }];
  return [{ label, endsAt: now + total * 1000 * (2 / 3), total }];
}

// ---------------------------------------------------------------- GitHub
/** The mock pull requests, cut down to `needs` that need you and `mine` of your own (0-3 each). */
export function githubWith(now: number, needs: number, mine: number): GithubData {
  const g = D.github(now);
  const n = g.needs.slice(0, Math.max(0, needs));
  const m = g.mine.slice(0, Math.max(0, mine));
  return {
    ...g,
    needs: n,
    mine: m,
    counts: {
      review: n.filter((p) => p.reason === 'review' || p.reason === 'both').length,
      assigned: n.filter((p) => p.reason === 'assigned' || p.reason === 'both').length,
      needs: n.length,
      mine: m.length,
    },
  };
}

// ---------------------------------------------------------------- energy
/** The mock tariff day, but with the price band right now forced to `band`. */
export function energyWith(now: number, band: Band, watts = 420): EnergyData {
  const e = D.energy(now, watts);
  const next: Band = band === 'peak' ? 'normal' : band === 'cheap' ? 'normal' : 'peak';
  return {
    ...e,
    rate: {
      now: D.RATES[band],
      band,
      until: iso(now + 75 * 60_000),
      next: D.RATES[next],
      next_band: next,
    },
  };
}

// ---------------------------------------------------------------- weather
export type RainScene = 'dry' | 'later' | 'soon' | 'now';
/** The mock forecast with the current condition and the next hour's rain set. */
export function weatherWith(
  now: number,
  condition: string,
  rain: RainScene = 'later',
): WeatherData {
  const w = D.weather(now);
  const mm =
    rain === 'now' ? [1.2, 0.9, 0.5, 0.2] : rain === 'soon' ? [0, 0, 0.4, 0.8] : [0, 0, 0, 0];
  return {
    ...w,
    current: { ...w.current!, condition },
    next_hour: { steps: mm.map((x, i) => ({ t: iso(now + i * 15 * 60_000), mm: x })) },
    next_rain: rain === 'dry' ? null : w.next_rain,
  };
}

/** Home's Weather card states (`weatherState` in @facet/ui), plus `live` = the mock house as it is. */
export type WeatherCardScene = 'live' | 'day' | 'night' | 'rain' | 'storm' | 'loading' | 'offline';
/** The weather and the sky that put Home's Weather card (and the Weather screen) in a state. */
export function weatherCardWith(
  now: number,
  scene: WeatherCardScene,
): { weather: WeatherData | null; lightning: LightningScene } {
  const sky: LightningScene = scene === 'storm' ? 'near' : scene === 'live' ? 'live' : 'quiet';
  if (scene === 'live') return { weather: D.weather(now), lightning: sky };
  if (scene === 'loading') return { weather: null, lightning: sky };
  if (scene === 'offline') return { weather: { error: 'offline' }, lightning: sky };
  const cond =
    scene === 'night'
      ? 'clear-night'
      : scene === 'rain'
        ? 'cloudy'
        : scene === 'storm'
          ? 'lightning-rainy'
          : 'partlycloudy';
  const w = weatherWith(now, cond, scene === 'rain' ? 'soon' : scene === 'storm' ? 'now' : 'dry');
  return { weather: { ...w, sun: { ...w.sun!, up: scene !== 'night' } }, lightning: sky };
}

// ---------------------------------------------------------------- lightning
/** `live` = the storm that crosses once an hour; the rest hold still. */
export type LightningScene = 'live' | 'quiet' | 'distant' | 'near' | 'overhead';

const CELLS: Record<
  Exclude<LightningScene, 'live' | 'quiet'>,
  { x: number; y: number; n: number; spread: number }
> = {
  distant: { x: -68, y: 14, n: 14, spread: 18 },
  near: { x: -16, y: 6, n: 40, spread: 16 },
  overhead: { x: 2, y: -3, n: 80, spread: 22 },
};
/** Lightning data for a scene. */
export function lightningWith(now: number, scene: LightningScene): LightningData {
  if (scene === 'live') return D.lightning(now);
  if (scene === 'quiet') return D.lightning(now, true);
  const c = CELLS[scene];
  let seed = 7;
  const rnd = () => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return ((seed >>> 8) & 0xffff) / 0xffff;
  };
  const strikes: Strike[] = [];
  for (let k = 0; k < c.n; k++) {
    const t = now - (k * (40 * 60_000)) / c.n - rnd() * 20_000;
    const x = c.x + (rnd() - 0.5) * c.spread,
      y = c.y + (rnd() - 0.5) * c.spread;
    const d = Math.hypot(x, y);
    if (d > 100) continue;
    strikes.push({
      id: `s${k}`,
      d: Math.round(d),
      az: Math.round(((Math.atan2(x, y) * 180) / Math.PI + 360) % 360),
      t: Math.round(t),
    });
  }
  strikes.sort((a, b) => b.t - a.t);
  const trend: LightningData['trend'] =
    scene === 'overhead' ? { state: 'steady', kmh: 0 } : { state: 'approaching', kmh: 46 };
  return D.summariseStrikes(now, strikes, trend);
}

// ---------------------------------------------------------------- moon
/** A moon to show: `phase` jumps to the next time the moon is in that phase (`live` = now), `days` moves on from
 *  there, `south` mirrors the glyph. */
export interface MoonScene {
  phase?: 'live' | MoonPhase;
  days?: number;
  south?: boolean;
}
const PHASE_FRAC: Record<MoonPhase, number> = {
  new_moon: 0,
  waxing_crescent: 0.125,
  first_quarter: 0.25,
  waxing_gibbous: 0.375,
  full_moon: 0.5,
  waning_gibbous: 0.625,
  last_quarter: 0.75,
  waning_crescent: 0.875,
};
/**
 * The moon for a scene, told as if it were now: worked out for the shifted moment, then every time moved back by the
 * same amount, so "full on Fri" and the rise and set times read naturally.
 */
export function moonWith(now: number, s: MoonScene = {}): MoonData {
  const DAY = 86_400_000;
  let shift = (s.days ?? 0) * DAY;
  if (s.phase && s.phase !== 'live') {
    const ahead = (PHASE_FRAC[s.phase] - moonFrac(now) + 1) % 1;
    shift += ahead * SYNODIC * DAY;
  }
  const m = moonData(now + shift, D.PLACE_AT);
  const back = (t?: string | null) => (t ? iso(Date.parse(t) - shift) : t);
  return {
    ...m,
    next_new: back(m.next_new)!,
    next_full: back(m.next_full)!,
    upcoming: m.upcoming?.map((p) => ({ ...p, t: back(p.t)! })),
    rise: back(m.rise),
    set: back(m.set),
    next_rise: back(m.next_rise),
    next_set: back(m.next_set),
    south: !!s.south,
  };
}
