/**
 * The knobs under each screen preview. Every change goes through the mock house: a store patch built by the scene
 * helpers in @facet/demo-data, a panel action, or (for the Clock) a remount with other options. Nothing pokes the DOM.
 * The first value of each control is what the mock house starts with.
 */
import {
  agentsWith,
  githubWith,
  mediaWith,
  printWith,
  switchWith,
  thermostatWith,
  coversWith,
  timersWith,
  TOASTS,
  TRACKS,
  weatherCardWith,
  weatherWith,
  type LightningScene,
  type MoonScene,
  type MockHouse,
  type PrintScene,
  type RainScene,
  type SwitchScene,
  type ThermostatScene,
  type CoverScene,
  type OverheadScene,
  type TimerScene,
  type WeatherCardScene,
} from '@facet/demo-data';
import { WX_TEXT } from '@facet/icons';
import {
  CLOCK_FACES,
  MEDIA_VISUALS,
  type Band,
  type ClockFace,
  type MediaPlayback,
  type MediaVisualId,
  type ToastData,
} from '@facet/ui';
import type { Control, Props } from './controls';

export interface ScreenOptionsContext {
  house: MockHouse;
  /** Mount the screen again with other screen options (the Clock face and style). */
  remount(o: { face?: ClockFace; font?: string; format?: string; date?: string }): void;
  /** Swap what sits in the Media screen's hero slot (the disc or a visual). */
  mediaVisual(id: MediaVisualId): void;
  /** The toast layer, on the Toasts preview. */
  toasts?: { push(t: ToastData): void; setPosition(p: 'top' | 'bottom'): void };
}

export interface ScreenOptions {
  controls: Control[];
  /** Apply one change (`key` = the control that changed, `p` = every control's value). */
  apply(key: string, p: Props, ctx: ScreenOptionsContext): void;
  /** Extra buttons, e.g. to fire a toast. */
  buttons?: { label: string; run(ctx: ScreenOptionsContext): void }[];
}

const sel = (
  key: string,
  label: string,
  options: readonly string[],
  value = options[0]!,
): Control => ({
  key,
  label,
  type: 'select',
  options,
  value,
});
const range = (
  key: string,
  label: string,
  min: number,
  max: number,
  value: number,
  step = 1,
): Control => ({ key, label, type: 'range', min, max, step, value });
const tog = (key: string, label: string, value: boolean): Control => ({
  key,
  label,
  type: 'toggle',
  value,
});

const MEDIA_STATES = ['playing', 'paused', 'idle'] as const;
const TRACK_TITLES = TRACKS.map((t) => t.title);
const VISUAL_NAMES = MEDIA_VISUALS.map((v) => v.name);
const BANDS = ['live', 'cheap', 'normal', 'peak'] as const;
const LIGHTNING: LightningScene[] = ['live', 'quiet', 'distant', 'near', 'overhead'];
const CONDITIONS = Object.keys(WX_TEXT);
const RAIN: RainScene[] = ['later', 'dry', 'soon', 'now'];
// Home's Weather card: storm > rain within the hour > night (the moon) > day
const WX_CARD: WeatherCardScene[] = ['live', 'day', 'night', 'rain', 'storm', 'loading', 'offline'];
const TIMER: TimerScene[] = ['running', 'done', 'none'];
const OVERHEAD: OverheadScene[] = ['live', 'overhead', 'emergency', 'quiet'];
// 'tonight' = the real moon; the rest jump to the next time the moon is in that phase
const MOON_PHASES = [
  'tonight',
  'new moon',
  'waxing crescent',
  'first quarter',
  'waxing gibbous',
  'full moon',
  'waning gibbous',
  'last quarter',
  'waning crescent',
];

const media = (p: Props, ctx: ScreenOptionsContext) =>
  ctx.house.store.patch({
    media: mediaWith(Date.now(), {
      state: p.state as MediaPlayback,
      track: Math.max(0, TRACK_TITLES.indexOf(String(p.track))),
      volume: Number(p.volume),
    }),
  });
const light = (p: Props, ctx: ScreenOptionsContext) =>
  ctx.house.store.patch({ light: { on: !!p.on, brightness: Number(p.brightness) } });
const band = (p: Props, ctx: ScreenOptionsContext) =>
  ctx.house.setScene({ band: p.band as Band | 'live' });
const sky = (p: Props, ctx: ScreenOptionsContext) =>
  ctx.house.setScene({ lightning: p.sky as LightningScene });
const wxCard = (p: Props, ctx: ScreenOptionsContext) => {
  const s = weatherCardWith(Date.now(), p.wx as WeatherCardScene);
  ctx.house.setScene({ lightning: s.lightning });
  ctx.house.store.patch({ weather: s.weather });
};

export const screenOptions: Record<string, ScreenOptions> = {
  home: {
    controls: [
      sel('state', 'Music', MEDIA_STATES),
      tog('on', 'Light on', true),
      sel('band', 'Price band', BANDS),
      sel('wx', 'Weather card', WX_CARD),
    ],
    apply(key, p, ctx) {
      if (key === 'state') media({ ...p, track: TRACK_TITLES[0]!, volume: 42 }, ctx);
      else if (key === 'on') light({ ...p, brightness: 64 }, ctx);
      else if (key === 'band') band(p, ctx);
      else wxCard(p, ctx);
    },
  },
  agents: {
    controls: [
      range('needs', 'Need you', 0, 6, 2),
      range('working', 'Working', 0, 6, 2),
      range('done', 'Done', 0, 6, 5),
      range('idle', 'Idle', 0, 6, 1),
    ],
    apply: (_k, p, ctx) =>
      ctx.house.store.patch({
        agents: agentsWith({
          needs: Number(p.needs),
          working: Number(p.working),
          done: Number(p.done),
          idle: Number(p.idle),
        }),
      }),
  },
  media: {
    controls: [
      sel('state', 'State', MEDIA_STATES),
      sel('track', 'Track', TRACK_TITLES),
      range('volume', 'Volume', 0, 100, 42),
      sel('visual', 'Visual', VISUAL_NAMES),
    ],
    apply(key, p, ctx) {
      if (key === 'visual')
        ctx.mediaVisual(MEDIA_VISUALS.find((v) => v.name === p.visual)?.id ?? 'disc');
      else media(p, ctx);
    },
  },
  light: {
    controls: [tog('on', 'On', true), range('brightness', 'Brightness', 1, 100, 64)],
    apply(key, p, ctx) {
      // dragging the brightness of a light that's off turns it on, as the dial does
      light(key === 'brightness' ? { ...p, on: true } : p, ctx);
    },
  },
  clock: {
    controls: [
      sel(
        'face',
        'Face',
        CLOCK_FACES.map((f) => f.id),
      ),
      sel('font', 'Font', ['mono', 'inter']),
      sel('format', 'Format', ['24h', '12h']),
      sel('date', 'Date', ['short', 'long']),
    ],
    apply: (_k, p, ctx) =>
      ctx.remount({
        face: p.face as ClockFace,
        font: String(p.font),
        format: String(p.format),
        date: String(p.date),
      }),
  },
  github: {
    controls: [range('needs', 'Need you', 0, 3, 3), range('mine', 'Yours', 0, 3, 3)],
    apply: (_k, p, ctx) =>
      ctx.house.store.patch({ github: githubWith(Date.now(), Number(p.needs), Number(p.mine)) }),
  },
  energy: {
    controls: [sel('band', 'Price band now', BANDS)],
    apply: (_k, p, ctx) => band(p, ctx),
  },
  weather: {
    controls: [
      sel('wx', 'State', WX_CARD),
      sel('condition', 'Condition', CONDITIONS, 'partlycloudy'),
      sel('rain', 'Rain', RAIN),
      sel('sky', 'Lightning', LIGHTNING),
    ],
    apply(key, p, ctx) {
      if (key === 'wx') wxCard(p, ctx);
      else if (key === 'sky') sky(p, ctx);
      else
        ctx.house.store.patch({
          weather: weatherWith(Date.now(), String(p.condition), p.rain as RainScene),
        });
    },
  },
  lightning: {
    controls: [sel('sky', 'Storm', LIGHTNING)],
    apply: (_k, p, ctx) => sky(p, ctx),
  },
  moon: {
    controls: [
      sel('phase', 'Phase', MOON_PHASES),
      range('days', 'Days ahead', 0, 29, 0),
      tog('south', 'Southern hemisphere', false),
    ],
    apply: (_k, p, ctx) =>
      ctx.house.setScene({
        moon: {
          phase: (p.phase === 'tonight'
            ? 'live'
            : String(p.phase).replace(' ', '_')) as MoonScene['phase'],
          days: Number(p.days),
          south: !!p.south,
        } as MoonScene,
      }),
  },
  overhead: {
    controls: [
      sel('sky', 'Sky', OVERHEAD),
      sel('distance', 'Distance', ['km', 'mi']),
      sel('speed', 'Speed', ['mph', 'kmh', 'kn']),
    ],
    apply(key, p, ctx) {
      if (key === 'sky') ctx.house.setScene({ overhead: p.sky as OverheadScene });
      // the units are settings (Settings > Units), so they go through the house's setSetting
      else
        ctx.house.actions.setSetting(
          key === 'distance' ? 'distanceUnit' : 'speedUnit',
          String(p[key]),
        );
    },
  },
  timer: {
    controls: [sel('state', 'Timer', TIMER), range('minutes', 'Length (min)', 1, 60, 12)],
    apply: (_k, p, ctx) =>
      ctx.house.store.patch({
        timers: timersWith(Date.now(), p.state as TimerScene, Number(p.minutes)),
      }),
  },
  thermostat: {
    controls: [
      sel('state', 'Thermostat', ['live', 'heating', 'idle', 'eco', 'off', 'none']),
      range('current', 'Room (°C)', 10, 26, 19.4, 0.1),
    ],
    apply: (_k, p, ctx) =>
      ctx.house.store.patch({
        thermostat:
          p.state === 'none'
            ? null
            : thermostatWith(Date.now(), p.state as ThermostatScene, Number(p.current)),
      }),
  },
  switch: {
    controls: [
      sel('state', 'Switch', ['on', 'off', 'unavailable', 'none']),
      sel('kind', 'Kind', ['plug', 'light', 'switch']),
      range('minutes', 'In this state (min)', 0, 3000, 134, 1),
    ],
    apply: (_k, p, ctx) =>
      ctx.house.store.patch({
        switch:
          p.state === 'none'
            ? null
            : switchWith(
                Date.now(),
                p.state as SwitchScene,
                Number(p.minutes),
                p.kind as 'plug' | 'light' | 'switch',
              ),
      }),
  },
  covers: {
    controls: [
      sel('state', 'Covers', [
        'live',
        'roller',
        'curtain',
        'venetian',
        'moving',
        'offline',
        'none',
      ]),
      range('position', 'Position (%)', 0, 100, 60),
      range('tilt', 'Slats (%)', 0, 100, 60),
    ],
    apply: (k, p, ctx) =>
      ctx.house.store.patch({
        covers: coversWith(
          p.state as CoverScene,
          k === 'state' ? undefined : Number(p.position),
          k === 'state' ? undefined : Number(p.tilt),
        ),
      }),
  },
  print: {
    controls: [
      sel('state', 'Printer', ['printing', 'paused', 'preparing', 'done', 'none']),
      range('progress', 'Progress (%)', 0, 100, 45),
    ],
    apply: (_k, p, ctx) =>
      ctx.house.store.patch({
        print: printWith(Date.now(), p.state as PrintScene, Number(p.progress) / 100),
      }),
  },
  toasts: {
    controls: [sel('position', 'Position', ['top', 'bottom'])],
    apply: (_k, p, ctx) => ctx.toasts?.setPosition(p.position as 'top' | 'bottom'),
    buttons: TOASTS.map((t) => ({ label: t.name, run: (ctx) => ctx.toasts?.push(t) })),
  },
};
