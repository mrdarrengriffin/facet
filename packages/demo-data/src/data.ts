// Fictional data for the mock house. No real people, places, repositories or devices.
import type {
  Agent,
  Band,
  CalendarEvent,
  EnergyData,
  EnergySlot,
  GithubData,
  LightningData,
  Player,
  PullRequest,
  Strike,
  ToastData,
  WeatherData,
} from '@facet/ui';

const HOUR = 3600_000;
const iso = (t: number) => new Date(t).toISOString();
const startOfDay = (now: number) => {
  const d = new Date(now);
  d.setHours(0, 0, 0, 0);
  return d.getTime();
};

export const ROOM = 'Study';
export const PLACE = 'Larkfield';
export const LIGHT_NAME = 'Desk lamp';

// ---------------------------------------------------------------- agents
export const AGENTS: Agent[] = [
  { title: 'Photo backup', status: 'working · 42% ctx', kind: 'working' },
  { title: 'Recipe importer', status: 'needs you · 12m', kind: 'needs' },
  { title: 'Garden sensors', status: 'done · 1h ago', kind: 'done' },
  { title: 'Firmware build', status: 'working · 18% ctx', kind: 'working' },
  { title: 'Calendar tidy-up', status: 'done · 5h ago', kind: 'done' },
  { title: 'Invoice sorter', status: 'needs you · 2h', kind: 'needs' },
  { title: 'Bird feeder camera', status: 'done · 1d ago', kind: 'done' },
  { title: 'Plant watering plan', status: 'idle · 1w ago', kind: 'idle' },
  { title: 'Docs spellcheck', status: 'done · 3d ago', kind: 'done', mascot: 'blue' },
];

// ---------------------------------------------------------------- media
export interface Track {
  title: string;
  artist: string;
  duration: number;
}
export const TRACKS: Track[] = [
  { title: 'Paper Lanterns', artist: 'The Quiet Hours', duration: 214 },
  { title: 'Northbound', artist: 'Marlow & Finch', duration: 187 },
  { title: 'Low Tide Static', artist: 'Juniper Signal', duration: 243 },
  { title: 'A Room With Two Windows', artist: 'Odette Vale', duration: 198 },
];
export const PLAYERS: Player[] = [
  { id: 'study', name: 'Study speaker', state: 'playing', kind: 'speaker' },
  { id: 'kitchen', name: 'Kitchen speaker', state: 'idle', kind: 'speaker' },
  { id: 'lounge-tv', name: 'Lounge TV', state: 'off', kind: 'tv' },
  { id: 'downstairs', name: 'Downstairs', state: 'idle', kind: 'group', members: 3 },
  { id: 'stream', name: 'Streaming account', state: 'paused', kind: 'stream', source: 'Laptop' },
];

// ---------------------------------------------------------------- calendar
export function nextEvent(now: number): CalendarEvent {
  const start =
    startOfDay(now) + 17.5 * HOUR > now + 20 * 60_000
      ? startOfDay(now) + 17.5 * HOUR
      : now + 50 * 60_000;
  return {
    summary: 'Pottery class',
    start: iso(start),
    end: iso(start + 1.5 * HOUR),
    allDay: false,
  };
}

// ---------------------------------------------------------------- weather
export function weather(now: number): WeatherData {
  const day0 = startOfDay(now);
  const hourly = Array.from({ length: 12 }, (_, i) => {
    const t = Math.floor(now / HOUR) * HOUR + i * HOUR;
    const hr = new Date(t).getHours();
    const temp = 11 + 5 * Math.sin(((hr - 9) / 24) * 2 * Math.PI);
    const condition = i >= 4 && i <= 6 ? 'rainy' : i >= 3 && i <= 7 ? 'cloudy' : 'partlycloudy';
    return {
      t: iso(t),
      condition,
      temp: Math.round(temp),
      precip: condition === 'rainy' ? 0.6 : 0,
    };
  });
  const conds = ['partlycloudy', 'rainy', 'cloudy', 'sunny', 'lightning-rainy'];
  const daily = conds.map((condition, i) => ({
    t: iso(day0 + i * 24 * HOUR + 12 * HOUR),
    condition,
    hi: [16, 13, 14, 18, 15][i]!,
    lo: [8, 9, 7, 9, 10][i]!,
    precip: [0, 4.2, 0.4, 0, 6.8][i]!,
  }));
  return {
    current: {
      condition: hourly[0]!.condition,
      temp: hourly[0]!.temp,
      feels: hourly[0]!.temp - 2,
      humidity: 72,
      wind: 14,
      wind_bearing: 240,
      uv: 2,
      unit: '°C',
      wind_unit: 'km/h',
    },
    sun: {
      next_rising: iso(day0 + 7 * HOUR + 12 * 60_000),
      next_setting: iso(day0 + 18 * HOUR + 4 * 60_000),
      up: true,
    },
    next_hour: { steps: [0, 1, 2, 3].map((i) => ({ t: iso(now + i * 15 * 60_000), mm: 0 })) },
    next_rain: { t: hourly[4]!.t, mm: 0.6 },
    hourly,
    daily,
  };
}

// ---------------------------------------------------------------- energy (a time-of-use tariff with three bands)
/** Cheap 04-07, 13-16 and 22-24; peak 16-19; normal the rest of the day. */
export const bandAt = (hr: number): Band =>
  (hr >= 4 && hr < 7) || (hr >= 13 && hr < 16) || hr >= 22
    ? 'cheap'
    : hr >= 16 && hr < 19
      ? 'peak'
      : 'normal';
export const RATES: Record<Band, number> = { cheap: 0.135, normal: 0.262, peak: 0.394 };

export function energy(now: number, watts = 420): EnergyData {
  const day0 = startOfDay(now);
  const slots: EnergySlot[] = Array.from({ length: 48 }, (_, i) => {
    const start = day0 + i * 30 * 60_000;
    const band = bandAt(i / 2);
    const past = start + 30 * 60_000 <= now;
    const base =
      0.12 +
      0.18 * Math.abs(Math.sin(i * 0.61)) +
      (band === 'cheap' ? 0.32 : 0) +
      (i >= 14 && i <= 17 ? 0.25 : 0);
    const kwh = past ? Math.round(base * 100) / 100 : null;
    return {
      start: iso(start),
      kwh,
      rate: RATES[band],
      cost: kwh === null ? null : kwh * RATES[band],
      band,
    };
  });
  const used = slots.filter((s) => s.kwh !== null);
  const kwh = used.reduce((a, s) => a + s.kwh!, 0);
  const cost = used.reduce((a, s) => a + s.cost!, 0) + 0.48; // + a standing charge
  const hr = new Date(now).getHours() + new Date(now).getMinutes() / 60;
  const band = bandAt(hr);
  const edges = [4, 7, 13, 16, 19, 22, 24];
  const until = edges.find((e) => e > hr) ?? 24;
  const nextBand = bandAt(until % 24);
  return {
    today: { kwh: Math.round(kwh * 10) / 10, cost, slots },
    rate: {
      now: RATES[band],
      band,
      until: iso(day0 + until * HOUR),
      next: RATES[nextBand],
      next_band: nextBand,
    },
    power: { w: watts },
    yesterday_to_now: Math.round(kwh * 1.12 * 10) / 10,
  };
}

// ---------------------------------------------------------------- GitHub
const pr = (
  o: Partial<PullRequest> & Pick<PullRequest, 'id' | 'repo' | 'number' | 'title'>,
  now: number,
  ageH: number,
): PullRequest => ({
  repoFull: `orchard-labs/${o.repo}`,
  url: `https://example.com/orchard-labs/${o.repo}/pull/${o.number}`,
  author: 'sam-okafor',
  created: iso(now - ageH * HOUR),
  updated: iso(now - Math.min(ageH, 2) * HOUR),
  draft: false,
  head: 'feature',
  base: 'main',
  additions: 120,
  deletions: 14,
  files: 6,
  body: '',
  review: 'pending',
  ci: 'pass',
  checks: [
    { name: 'build', state: 'pass' },
    { name: 'lint', state: 'pass' },
    { name: 'test', state: 'pass' },
  ],
  ...o,
});

export function github(now: number): GithubData {
  const needs: PullRequest[] = [
    pr(
      {
        id: 'n1',
        repo: 'lantern',
        number: 412,
        title: 'Add a quiet hours schedule to the notification router',
        reason: 'review',
        head: 'quiet-hours',
        body: 'Holds non-urgent notifications between 22:00 and 07:00.\nUrgent ones still come through.\nAdds a settings screen and tests.',
        checks: [
          { name: 'build', state: 'pass' },
          { name: 'lint', state: 'pass' },
          { name: 'test', state: 'pass' },
          { name: 'preview', state: 'pending' },
        ],
        ci: 'pending',
      },
      now,
      26,
    ),
    pr(
      {
        id: 'n2',
        repo: 'hearth-ui',
        number: 88,
        title: 'Card grid: snap scrolling one row at a time',
        reason: 'assigned',
        author: 'ada-lindqvist',
        head: 'snap-rows',
        additions: 64,
        deletions: 9,
        files: 3,
        body: 'Snaps the Home grid per row instead of free scrolling.',
      },
      now,
      5,
    ),
    pr(
      {
        id: 'n3',
        repo: 'seedling',
        number: 1290,
        title: 'Fix the sunrise time drifting by a minute each week',
        reason: 'both',
        author: 'jo-marsh',
        ci: 'fail',
        head: 'sunrise-drift',
        checks: [
          { name: 'build', state: 'pass' },
          { name: 'test', state: 'fail' },
          { name: 'lint', state: 'pass' },
        ],
        body: 'Uses the solar noon from the table instead of adding 24 h.',
      },
      now,
      72,
    ),
  ];
  const mine: PullRequest[] = [
    pr(
      {
        id: 'm1',
        repo: 'lantern',
        number: 409,
        title: 'Docs: getting started on a Raspberry Pi',
        review: 'approved',
        author: 'you',
        head: 'pi-docs',
      },
      now,
      50,
    ),
    pr(
      {
        id: 'm2',
        repo: 'hearth-ui',
        number: 86,
        title: 'Theme tokens for the light mode charts',
        review: 'changes',
        author: 'you',
        head: 'light-charts',
      },
      now,
      120,
    ),
    pr(
      {
        id: 'm3',
        repo: 'tidepool',
        number: 37,
        title: 'Experiment: ambient screen with drifting dots',
        draft: true,
        author: 'you',
        ci: 'none',
        checks: [],
      },
      now,
      300,
    ),
  ];
  return {
    at: iso(now - 2 * 60_000),
    needs,
    mine,
    counts: { review: 2, assigned: 2, needs: needs.length, mine: mine.length },
  };
}

// ---------------------------------------------------------------- lightning: a storm passing from the west
const DIRS = ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW'];
const dirOf = (az: number) => DIRS[Math.floor((((az % 360) + 360 + 22.5) % 360) / 45) % 8]!;

/** A synthetic storm: a cell that crosses the area once an hour, from the west at about 50 km/h. */
export function lightning(now: number, quiet = false): LightningData {
  if (quiet)
    return {
      radius: 100,
      strikes: [],
      counts: { m5: 0, m15: 0, m60: 0, near25: 0 },
      nearest: null,
      last: null,
      trend: { state: 'none', kmh: 0 },
      storm: false,
    };
  const strikes: Strike[] = [];
  let seed = Math.floor(now / 20_000);
  const rnd = () => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return ((seed >>> 8) & 0xffff) / 0xffff;
  };
  for (let k = 0; k < 90; k++) {
    const t = now - k * 40_000 - rnd() * 20_000;
    const phase = (t % HOUR) / HOUR; // 0..1 across the hour
    const cx = -90 + phase * 160,
      cy = -10 + phase * 18; // km east / north of home
    const x = cx + (rnd() - 0.5) * 22,
      y = cy + (rnd() - 0.5) * 22;
    const d = Math.hypot(x, y);
    if (d > 100) continue;
    const az = ((Math.atan2(x, y) * 180) / Math.PI + 360) % 360;
    strikes.push({
      id: `s${Math.round(t)}`,
      d: Math.round(d),
      az: Math.round(az),
      t: Math.round(t),
    });
  }
  strikes.sort((a, b) => b.t - a.t);
  const within = (m: number) => strikes.filter((s) => now - s.t < m * 60_000);
  const m15 = within(15);
  const near = m15.reduce<Strike | null>((a, s) => (!a || s.d < a.d ? s : a), null);
  const approaching = (now % HOUR) / HOUR < 0.55;
  return {
    radius: 100,
    strikes,
    counts: {
      m5: within(5).length,
      m15: m15.length,
      m60: within(60).length,
      near25: m15.filter((s) => s.d <= 25).length,
    },
    nearest: near ? { d: near.d, az: near.az, dir: dirOf(near.az), t: near.t } : null,
    last: strikes[0]?.t ?? null,
    trend: { state: approaching ? 'approaching' : 'receding', kmh: approaching ? 48 : -44 },
    storm: m15.length > 10,
  };
}

// ---------------------------------------------------------------- toasts
export const TOASTS: (ToastData & { key: string; name: string })[] = [
  {
    key: 'timer',
    name: 'Timer done',
    icon: 'timer',
    tone: 'ok',
    title: 'Pasta is ready',
    body: 'Your 9 min timer has finished.',
  },
  {
    key: 'agent',
    name: 'Agent needs you',
    icon: 'agent',
    tone: 'warn',
    title: 'Recipe importer needs you',
    body: 'Two recipes have the same name. Keep both?',
    action: { label: 'Open' },
  },
  {
    key: 'doorbell',
    name: 'Someone at the door',
    icon: 'person',
    tone: 'warn',
    title: 'Someone at the front door',
    body: 'Person on the doorstep',
    action: { label: 'View' },
    duration: 12000,
  },
  {
    key: 'washer',
    name: 'Washing done',
    icon: 'washer',
    tone: 'info',
    title: 'Washing machine done',
    body: 'Took 1 h 4 min.',
  },
  {
    key: 'vacuum',
    name: 'Vacuum docked',
    icon: 'vacuum',
    tone: 'ok',
    title: 'Back on the dock',
    body: 'Kitchen and hallway cleaned in 23 min.',
  },
  {
    key: 'lightning',
    name: 'Lightning nearby',
    icon: 'bolt',
    tone: 'warn',
    title: 'Lightning 9 km W',
    body: '6 strikes within 25 km, closing at 48 km/h',
    action: { label: 'Radar' },
  },
  {
    key: 'error',
    name: 'Something offline',
    icon: 'alert',
    tone: 'error',
    title: 'Garden sensor offline',
    body: 'No reading for 30 min',
  },
];
