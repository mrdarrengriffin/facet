/**
 * The knobs under each screen preview. Every change goes through the mock house: a store patch built by the scene
 * helpers in @facet/demo-data, a panel action, or (for the Clock) a remount with other options. Nothing pokes the DOM.
 * The first value of each control is what the mock house starts with.
 */
import {
  agentsWith,
  githubWith,
  mediaWith,
  timersWith,
  TOASTS,
  TRACKS,
  weatherWith,
  type LightningScene,
  type MockHouse,
  type RainScene,
  type TimerScene,
} from '@facet/demo-data';
import { WX_TEXT } from '@facet/icons';
import {
  CLOCK_FACES,
  type Band,
  type ClockFace,
  type MediaPlayback,
  type ToastData,
} from '@facet/ui';
import type { Control, Props } from './controls';

export interface ScreenOptionsContext {
  house: MockHouse;
  /** Mount the screen again with other screen options (the Clock face and style). */
  remount(o: { face?: ClockFace; font?: string; format?: string; date?: string }): void;
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
const BANDS = ['live', 'cheap', 'normal', 'peak'] as const;
const LIGHTNING: LightningScene[] = ['live', 'quiet', 'distant', 'near', 'overhead'];
const CONDITIONS = Object.keys(WX_TEXT);
const RAIN: RainScene[] = ['later', 'dry', 'soon', 'now'];
const TIMER: TimerScene[] = ['running', 'done', 'none'];

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

export const screenOptions: Record<string, ScreenOptions> = {
  home: {
    controls: [
      sel('state', 'Music', MEDIA_STATES),
      tog('on', 'Light on', true),
      sel('band', 'Price band', BANDS),
      sel('sky', 'Lightning', LIGHTNING),
    ],
    apply(key, p, ctx) {
      if (key === 'state') media({ ...p, track: TRACK_TITLES[0]!, volume: 42 }, ctx);
      else if (key === 'on') light({ ...p, brightness: 64 }, ctx);
      else if (key === 'band') band(p, ctx);
      else sky(p, ctx);
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
    ],
    apply: (_k, p, ctx) => media(p, ctx),
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
      sel('condition', 'Condition', CONDITIONS, 'partlycloudy'),
      sel('rain', 'Rain', RAIN),
    ],
    apply: (_k, p, ctx) =>
      ctx.house.store.patch({
        weather: weatherWith(Date.now(), String(p.condition), p.rain as RainScene),
      }),
  },
  lightning: {
    controls: [sel('sky', 'Storm', LIGHTNING)],
    apply: (_k, p, ctx) => sky(p, ctx),
  },
  timer: {
    controls: [sel('state', 'Timer', TIMER), range('minutes', 'Length (min)', 1, 60, 12)],
    apply: (_k, p, ctx) =>
      ctx.house.store.patch({
        timers: timersWith(Date.now(), p.state as TimerScene, Number(p.minutes)),
      }),
  },
  toasts: {
    controls: [sel('position', 'Position', ['top', 'bottom'])],
    apply: (_k, p, ctx) => ctx.toasts?.setPosition(p.position as 'top' | 'bottom'),
    buttons: TOASTS.map((t) => ({ label: t.name, run: (ctx) => ctx.toasts?.push(t) })),
  },
};
