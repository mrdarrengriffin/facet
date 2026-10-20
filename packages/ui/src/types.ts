/**
 * The data Facet's screens show. Everything comes in through these shapes, so the UI never needs to know where it came
 * from: a Home Assistant adapter, a mock house, a test. Times are ISO strings unless they say otherwise.
 */
import type { Values, Value } from '@facet/settings';
import type { Grid } from './print3d/grid.js';

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

/** One calendar on the Calendar screen. */
export interface Calendar {
  id: string;
  name: string;
  /** Whether its events are shown. */
  on: boolean;
  /** Its dot colour, a colour token name (`blue`, `ok`, `warn`...). Default `text-dim`. */
  colour?: string;
}

/** An event on the agenda: a calendar event plus which calendar it's from. */
export interface AgendaEvent extends CalendarEvent {
  /** The calendar's `id`. */
  calendar?: string;
  location?: string;
}

/** The agenda: every event from the calendars that are on, from today's midnight for `days` days, by start. */
export interface CalendarData {
  /** When it was fetched. */
  at: string;
  days: number;
  calendars: Calendar[];
  events: AgendaEvent[];
  error?: string;
}

/** The Calendar screen's month view: the six weeks shown for `month` (from the Monday before the 1st, 42 days). */
export interface CalendarMonth extends CalendarData {
  /** YYYY-MM */
  month: string;
  /** The grid's first day, a Monday (YYYY-MM-DD). */
  start: string;
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

// ---------------------------------------------------------------- moon
/** Moon phases, using Home Assistant's `moon` sensor states. */
export type MoonPhase =
  | 'new_moon'
  | 'waxing_crescent'
  | 'first_quarter'
  | 'waxing_gibbous'
  | 'full_moon'
  | 'waning_gibbous'
  | 'last_quarter'
  | 'waning_crescent';

/** A principal phase and when it happens. */
export interface MoonPhaseEvent {
  phase: 'new_moon' | 'first_quarter' | 'full_moon' | 'last_quarter';
  t: string;
}

/** The moon now. `moonData(now, { lat, lon })` works all of it out locally. */
export interface MoonData {
  phase: MoonPhase;
  /** How far through the month, 0-1: 0 new, 0.25 first quarter, 0.5 full, 0.75 last quarter. Draws the glyph. */
  frac: number;
  /** Lit part of the disc, 0-100. */
  illumination: number;
  /** Days since the last new moon. */
  age: number;
  next_new: string;
  next_full: string;
  /** The next four principal phases. */
  upcoming?: MoonPhaseEvent[];
  /** Today's moonrise and moonset (local day); null when it doesn't rise or set today, or there's no location. */
  rise?: string | null;
  set?: string | null;
  /** The next moonrise and moonset from now (within two days). */
  next_rise?: string | null;
  next_set?: string | null;
  /** Southern hemisphere: the glyph's lit side is mirrored. */
  south?: boolean;
}

/** Today's sun. `sunData(now, { lat, lon })` works it out locally. Times are ISO; null = the sun doesn't rise or set. */
export interface SunData {
  rise: string | null;
  set: string | null;
  /** First and last light (civil twilight, sun 6° below the horizon). */
  dawn: string | null;
  dusk: string | null;
  /** Day length (ms), and the change since yesterday (ms, negative = shorter). */
  length: number | null;
  delta: number | null;
  /** Tomorrow's sunrise, the next event after sunset. */
  next_rise: string | null;
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
  moon?: MoonData | null;
  /** Sunrise, sunset and day length for the Weather screen's Sun view. */
  sun?: SunData | null;
  print?: PrintData | null;
  thermostat?: ThermostatData | null;
  /** Blinds and curtains, in the order the Covers screen offers them. */
  covers?: CoverData[] | null;
  switch?: SwitchData | null;
  timers?: Timer[];
  nextEvent?: CalendarEvent | null;
  calendar?: CalendarData | null;
  /** The month the Calendar's month view asked for (`loadCalendarMonth`). */
  calendarMonth?: CalendarMonth | null;
  /** Personalisation settings (`@facet/settings` ids → values); a setting left out = its default. The device applies
   *  the ones that are set (theme, accent, text size, idle, transition, toasts) and the Settings screen shows them. */
  settings?: Partial<Values> | null;
}

/** A 3D print in progress (a Bambu printer through Home Assistant's bambu_lab integration, or anything similar). */
export interface PrintData {
  status: 'printing' | 'paused' | 'preparing' | 'done' | 'failed' | 'idle';
  /** What the printer is doing while preparing ("auto bed levelling"). */
  stage?: string | null;
  task?: string | null;
  layer: number;
  layers: number;
  /** The printer's own %, shown as the hero (it matches the printer's app). */
  progress: number;
  remainingMin?: number | null;
  /** Unix seconds. */
  endTs?: number | null;
  /** The filament colour ("#RRGGBB"), for `solid: 'filament'`. */
  filament?: string | null;
  /** The model: `fromGcode` / `fromStl` / `fromShape` from @facet/ui. Null = no model yet (the screen shows text only). */
  model?: Grid | null;
}

/** Home Assistant's HVAC modes (`climate` entity `hvac_modes`). */
export type HvacMode = 'heat' | 'cool' | 'heat_cool' | 'auto' | 'dry' | 'fan_only' | 'off';

/** A thermostat (a Home Assistant `climate` entity). Temperatures in the entity's unit (°C in the examples). */
export interface ThermostatData {
  /** The room or zone it controls ("Hallway"). */
  name?: string | null;
  /** The measured temperature. */
  current: number | null;
  /** The set point. Null in a mode without one (off, fan). */
  target: number | null;
  /** The dial's range and step (`min_temp`, `max_temp`, `target_temp_step`). */
  min: number;
  max: number;
  step: number;
  mode: HvacMode;
  /** The modes offered, in order. */
  modes: HvacMode[];
  /** What it is doing right now (`hvac_action`). */
  action?: 'heating' | 'cooling' | 'idle' | 'off' | null;
  preset?: string | null;
  /** Preset names (`preset_modes`); empty or missing = no presets button. */
  presets?: string[];
  humidity?: number | null;
  /** The schedule's next change, if the thermostat (or a schedule helper) knows it. */
  next?: { at: string; target: number } | null;
}

/** What a switch is: decides the icon. */
export type SwitchKind = 'switch' | 'plug' | 'light' | 'fan';

/** Anything with an on and an off (a Home Assistant `switch`, `light`, `fan`, `input_boolean` ...). */
export interface SwitchData {
  /** What it powers ("Dehumidifier"). */
  name?: string | null;
  kind?: SwitchKind | null;
  /** On or off; null = unavailable. */
  on: boolean | null;
  /** Power draw in W, if it measures it (a smart plug's power sensor). */
  power?: number | null;
  /** When it last changed (`last_changed`). */
  changedAt?: string | null;
}

/** How a cover is drawn: curtains (two panels part sideways), a roller blind (fabric drops), Venetian (slats tilt). */
export type CoverKind = 'curtain' | 'roller' | 'venetian';

/** What a cover is doing (a Home Assistant `cover` entity's state). */
export type CoverState = 'open' | 'closed' | 'opening' | 'closing' | 'unavailable';

/**
 * A blind or curtain (a Home Assistant `cover` entity). Positions are Home Assistant's: 0 = closed, 100 = open.
 * Leave `kind` out and it comes from `deviceClass` and `features` (see `coverKind`).
 */
export interface CoverData {
  /** The entity id (what the actions get back). */
  id: string;
  /** "Office blind". */
  name: string;
  kind?: CoverKind | null;
  /** `device_class`: curtain, shade, blind, shutter, awning, window ... */
  deviceClass?: string | null;
  state: CoverState;
  /** `current_position`; null when the cover only knows open / closed. */
  position: number | null;
  /** `current_tilt_position` (Venetian slats): 0 = shut, 100 = open (flat). */
  tilt?: number | null;
  /** `supported_features` bits: 1 open, 2 close, 4 set position, 8 stop, 16/32/64/128 the same for tilt. Default 15. */
  features?: number;
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
  /** Show or hide a calendar's events on the Calendar screen. Leave it out and the screen has no calendar filter. */
  setCalendar?(id: string, on: boolean): void;
  /** The month view moved to `month` (YYYY-MM): put its events in `calendarMonth`. Without it, the month view shows what
   *  `calendar` has. */
  loadCalendarMonth?(month: string): void;
  /** Thermostat set point (`climate.set_temperature`). */
  climateTarget?(target: number): void;
  /** `climate.set_hvac_mode`. */
  climateMode?(mode: HvacMode): void;
  /** `climate.set_preset_mode`. */
  climatePreset?(preset: string): void;
  /** `cover.set_cover_position` (0 = closed, 100 = open). */
  coverPosition?(id: string, position: number): void;
  /** `cover.set_cover_tilt_position`. */
  coverTilt?(id: string, tilt: number): void;
  /** `cover.open_cover` / `close_cover` / `stop_cover`. */
  coverCommand?(id: string, command: 'open' | 'close' | 'stop'): void;
  /** Turn the switch on or off (`homeassistant.turn_on` / `turn_off`). */
  switchSet?(on: boolean): void;
  /** Change a personalisation setting (an `@facet/settings` id; the value is already valid for it). */
  setSetting?(id: string, value: Value): void;
}
