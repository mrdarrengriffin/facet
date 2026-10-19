/**
 * The data Facet's screens show. Everything comes in through these shapes, so the UI never needs to know where it came
 * from: a Home Assistant adapter, a mock house, a test. Times are ISO strings unless they say otherwise.
 */

/** Status colours. `dim` = secondary text, no status. */
export type Tone = 'ok' | 'warn' | 'error' | 'working' | 'info' | 'idle' | 'dim';

// ---------------------------------------------------------------- media
export type MediaPlayback = 'playing' | 'paused' | 'idle' | 'off' | 'unavailable';

export interface MediaState {
  state: MediaPlayback;
  title?: string | null;
  artist?: string | null;
  /** Cover art: an image URL or a data URI. The Media screen's CD and vinyl show it, and the cassette label takes
   *  its colours. */
  art?: string | null;
  /** Track length in seconds. */
  duration?: number | null;
  /** Position in seconds, as of `positionAt`. */
  position?: number | null;
  /** When `position` was read (ms since the epoch). While playing, the screen moves the position on from here. */
  positionAt?: number | null;
  /** 0-1. */
  volume?: number | null;
}

/** A speaker (or group, TV, streaming account) the panel can send media to. */
export interface Player {
  id: string;
  name: string;
  state: MediaPlayback;
  kind: 'speaker' | 'group' | 'tv' | 'stream';
  title?: string | null;
  artist?: string | null;
  /** Speakers in a group. */
  members?: number;
  /** For a streaming account: the device it's playing on. */
  source?: string | null;
}

/** Where media controls go: `auto` follows whatever is playing, `manual` sticks to one player. */
export interface MediaTarget {
  mode: 'auto' | 'manual';
  id: string;
  name: string;
}

// ---------------------------------------------------------------- light
export interface LightState {
  on: boolean;
  /** 0-100. */
  brightness: number;
}

// ---------------------------------------------------------------- agents
export type AgentKind = 'done' | 'working' | 'needs' | 'idle';

/** A background job or assistant task shown as a tile with a mascot. */
export interface Agent {
  title: string;
  kind: AgentKind;
  /** "<state> · <detail>", e.g. "working · 31% ctx", "done · 4h ago", "needs you · 20m". */
  status: string;
  /** Override the mascot colour (a colour token name). */
  mascot?: string;
}

// ---------------------------------------------------------------- timers and calendar
export interface Timer {
  label: string;
  /** When it ends (ms since the epoch). */
  endsAt: number;
  /** Total length in seconds. */
  total: number;
}

export interface CalendarEvent {
  summary: string;
  /** ISO date-time, or a date (YYYY-MM-DD) for all-day events. */
  start: string;
  end: string;
  allDay: boolean;
}

// ---------------------------------------------------------------- weather
/** Weather conditions, using Home Assistant's names. */
export type WeatherCondition =
  | 'sunny'
  | 'clear-night'
  | 'partlycloudy'
  | 'cloudy'
  | 'fog'
  | 'rainy'
  | 'pouring'
  | 'snowy'
  | 'snowy-rainy'
  | 'lightning'
  | 'lightning-rainy'
  | 'hail'
  | 'windy'
  | 'windy-variant'
  | 'exceptional';

export interface WeatherData {
  current?: {
    condition: WeatherCondition | string;
    temp: number | null;
    /** Feels-like temperature. */
    feels: number | null;
    humidity: number | null;
    wind: number | null;
    wind_bearing: number | null;
    uv: number | null;
    unit: string;
    wind_unit: string;
  };
  /** Next sunrise and sunset, used for day and night icons. */
  sun?: { next_rising: string; next_setting: string; up: boolean } | null;
  /** Rain in the next hour, in 15-minute steps (mm per step). */
  next_hour?: { steps: { t: string; mm: number }[] } | null;
  /** The next time rain is forecast, if not within the hour. */
  next_rain?: { t: string; mm: number } | null;
  hourly?: { t: string; condition: WeatherCondition | string; temp: number; precip: number }[];
  daily?: {
    t: string;
    condition: WeatherCondition | string;
    hi: number;
    lo: number | null;
    precip: number;
  }[];
  error?: string;
}

// ---------------------------------------------------------------- energy
export type Band = 'cheap' | 'normal' | 'peak';

export interface EnergySlot {
  /** Start of the half-hour. */
  start: string;
  kwh: number | null;
  /** Price per kWh in pounds. */
  rate: number;
  cost: number | null;
  band: Band;
}

export interface EnergyData {
  today?: { kwh: number | null; cost: number | null; slots: EnergySlot[] };
  rate?: {
    now: number | null;
    band: Band | null;
    until: string | null;
    next: number | null;
    next_band: Band | null;
  };
  /** Live demand in watts. */
  power?: { w: number | null } | null;
  /** Use by this time yesterday, for the comparison line. */
  yesterday_to_now?: number | null;
  /** Daily totals, oldest first, today last (partial). 14 days = this week and the week before. */
  days?: EnergyDay[];
  error?: string;
}

export interface EnergyDay {
  /** Local date, `YYYY-MM-DD`. */
  date: string;
  kwh: number | null;
  cost: number | null;
}

// ---------------------------------------------------------------- GitHub
export interface Check {
  name: string;
  state: 'pass' | 'fail' | 'pending' | 'skipped';
}

export interface PullRequest {
  id: string;
  /** Short repository name. */
  repo: string;
  /** owner/name. */
  repoFull: string;
  number: number;
  title: string;
  url: string;
  author: string;
  created: string;
  updated: string;
  draft: boolean;
  head: string;
  base: string;
  additions: number;
  deletions: number;
  files: number;
  body: string;
  review: 'approved' | 'changes' | 'pending';
  ci: 'pass' | 'fail' | 'pending' | 'none';
  checks: Check[];
  /** Why it needs you: a review request, an assignment, or both. */
  reason?: 'review' | 'assigned' | 'both';
}

export interface GithubData {
  /** When the data was fetched. */
  at: string;
  /** Pull requests that need you: review requests and assignments. */
  needs: PullRequest[];
  /** Your own open pull requests. */
  mine: PullRequest[];
  counts: { review: number; assigned: number; needs: number; mine: number };
  error?: string;
  stale?: boolean;
}

// ---------------------------------------------------------------- lightning
export interface Strike {
  id: string;
  /** Distance in km. */
  d: number;
  /** Bearing from home in degrees (0 = north, clockwise). */
  az: number;
  /** When (ms since the epoch). */
  t: number;
}

export interface LightningData {
  /** Radius covered, km. */
  radius: number;
  /** Strikes, newest first. */
  strikes: Strike[];
  counts: { m5: number; m15: number; m60: number; near25: number };
  /** Nearest strike in the last 15 minutes. */
  nearest: { d: number; az: number; dir: string; t: number } | null;
  /** Time of the latest strike (ms). */
  last: number | null;
  trend: { state: 'approaching' | 'receding' | 'steady' | 'none'; kmh: number };
  storm: boolean;
  error?: string;
}

// ---------------------------------------------------------------- the whole panel
/** Everything the screens read. Every key is optional: a screen shows a quiet empty state for missing data. */
export interface PanelData {
  /** Room name: the Home screen's header label. */
  room?: string;
  /** Place name for the weather screen. */
  place?: string;
  media?: MediaState | null;
  players?: Player[];
  mediaTarget?: MediaTarget | null;
  light?: LightState | null;
  /** Name of the light on the Light screen and its Home card. */
  lightName?: string;
  agents?: Agent[];
  weather?: WeatherData | null;
  energy?: EnergyData | null;
  github?: GithubData | null;
  lightning?: LightningData | null;
  timers?: Timer[];
  nextEvent?: CalendarEvent | null;
}

/** What the screens can ask for. An adapter implements these (and updates the store when things change). */
export interface PanelActions {
  mediaPlayPause?(): void;
  mediaNext?(): void;
  mediaPrevious?(): void;
  /** 0-100. */
  mediaVolume?(volume: number): void;
  setMediaTarget?(mode: 'auto' | 'manual', id?: string): void;
  /** 0-100; 0 = off. */
  lightBrightness?(brightness: number): void;
  lightToggle?(): void;
  refreshGithub?(): void;
  /** Start a timer of `seconds`. */
  startTimer?(seconds: number, label: string): void;
  /** Add time to the running timer. */
  extendTimer?(seconds: number): void;
  cancelTimer?(): void;
}
