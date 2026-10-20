// The device: every screen in one 720 × 720 frame, with navigation (tap a Home card to open its screen, Back returns
// Home), a toast layer over everything, and an idle clock after a while without a touch.
// Transition (shared with the firmware): `rise` (default, `motion.transition`) fades the new screen's rows in, header
// first, from 8 px below (above when going Home); `instant` just swaps. The idle clock always fades in.
// Idle rule (shared with the firmware): after `idleAfter` without a touch, the clock without its header and the screen
// dimmed to `idleBrightness`; the next touch goes to Home and does nothing else.
// Settings: Home's gear opens the Settings screen. `data.settings` (from @facet/settings) styles the frame (theme,
// accent, text size) and, when a value changes, sets the transition, toast position, idle time, brightness and face.
import type { Values } from '@facet/settings';
import { devices, motion, toast as TOAST, type DeviceId, type TransitionId } from '@facet/tokens';
import { h } from './dom.js';
import { hhmm } from './format.js';
import { startTicker } from './store.js';
import { applySettings } from './settings.js';
import { toastLayer, type ToastData } from './toast.js';
import { agentsScreen } from './screens/agents.js';
import { calendarScreen } from './screens/calendar.js';
import { clockScreen, type ClockOptions } from './screens/clock.js';
import type { Screen, ScreenContext } from './screens/common.js';
import { energyScreen } from './screens/energy.js';
import { githubScreen } from './screens/github.js';
import { homeScreen, type CardId } from './screens/home.js';
import { lightScreen } from './screens/light.js';
import { lightningScreen } from './screens/lightning.js';
import { mediaScreen } from './screens/media.js';
import type { MediaVisualId } from './media-visuals/index.js';
import { moonScreen } from './screens/moon.js';
import { printScreen } from './screens/print.js';
import { settingsScreen } from './screens/settings.js';
import { switchScreen } from './screens/switch.js';
import { thermostatScreen } from './screens/thermostat.js';
import { coversScreen } from './screens/covers.js';
import { timerScreen } from './screens/timer.js';
import { weatherScreen, type WeatherView } from './screens/weather.js';

/** `lightning` and `moon` are views of Weather on Home, and still screens of their own. */
export type ScreenId =
  'home' | Exclude<CardId, 'readouts'> | 'lightning' | 'moon' | 'timer' | 'print' | 'settings';
export const SCREEN_IDS: ScreenId[] = [
  'home',
  'agents',
  'media',
  'light',
  'thermostat',
  'covers',
  'switch',
  'clock',
  'github',
  'energy',
  'weather',
  'lightning',
  'moon',
  'calendar',
  'timer',
  'print',
  'settings',
];

/** Build one screen by id. `onBack` shows the back button; leave it out for a standalone preview. */
export function createScreen(
  id: ScreenId,
  ctx: ScreenContext,
  o: {
    onBack?: () => void;
    open?: (id: CardId) => void;
    /** Home: show the Settings button, which calls this. */
    openSettings?: () => void;
    clock?: ClockOptions;
    /** Media screen: what sits in the hero slot. */
    mediaVisual?: MediaVisualId;
    /** Weather screen: the view it opens on. */
    weatherView?: WeatherView;
  } = {},
): Screen {
  switch (id) {
    case 'home':
      return homeScreen(ctx, { open: o.open, openSettings: o.openSettings });
    case 'agents':
      return agentsScreen(ctx, o);
    case 'media':
      return mediaScreen(ctx, { onBack: o.onBack, visual: o.mediaVisual });
    case 'light':
      return lightScreen(ctx, o);
    case 'thermostat':
      return thermostatScreen(ctx, o);
    case 'switch':
      return switchScreen(ctx, o);
    case 'covers':
      return coversScreen(ctx, o);
    case 'clock':
      return clockScreen(ctx, { ...o.clock, onBack: o.onBack });
    case 'github':
      return githubScreen(ctx, o);
    case 'energy':
      return energyScreen(ctx, o);
    case 'weather':
      return weatherScreen(ctx, { onBack: o.onBack, view: o.weatherView });
    case 'lightning':
      return lightningScreen(ctx, o);
    case 'moon':
      return moonScreen(ctx, o);
    case 'calendar':
      return calendarScreen(ctx, o);
    case 'timer':
      return timerScreen(ctx, o);
    case 'print':
      return printScreen(ctx, o);
    case 'settings':
      return settingsScreen(ctx, o);
  }
}

/** Put a device's size and density on a frame (the tokens' CSS follows them). */
function setDevice(frame: HTMLElement, id?: DeviceId) {
  const d = id ? devices[id] : undefined;
  if (!d) return;
  frame.dataset.facetDevice = id;
  frame.dataset.facetDensity = d.density;
}

/** Run a screen's clocks: the header time and anything with `data-clock` gets the time once a second. */
function tickClocks(root: HTMLElement, now: Date) {
  root
    .querySelectorAll<HTMLElement & { tick?: (n: Date) => void }>('[data-clock]')
    .forEach((e) => (e.tick ? e.tick(now) : (e.textContent = hhmm(now))));
}

export interface PreviewOptions {
  onBack?: () => void;
  /** The screen to draw for (size + density, from `devices` in @facet/tokens). Default the 720 × 720 reference. */
  device?: DeviceId;
  clock?: ClockOptions;
  /** Media screen: what sits in the hero slot. Default `disc`. */
  mediaVisual?: MediaVisualId;
  /** Weather screen: the view it opens on (default Weather, or Lightning during a storm). */
  weatherView?: WeatherView;
}

/**
 * One screen in its own 720 × 720 frame, bound to the store and ticking: what the docs use for live previews.
 * Without `onBack` there's no back button. Call `destroy()` when you're done with it.
 */
export function mountScreen(id: ScreenId, ctx: ScreenContext, o: PreviewOptions = {}) {
  const frame = h('div', { class: 'f-screen', 'data-screen': id });
  setDevice(frame, o.device);
  if (o.device) ctx = { ...ctx, device: o.device };
  const scr = createScreen(id, ctx, o);
  frame.append(scr.el);
  const unsub = ctx.store.subscribe((s) => {
    applySettings(frame, s.settings); // the look settings (theme, accent, text size) show in a preview too
    scr.update?.(s);
  });
  const stop = startTicker((now) => {
    scr.tick?.(now);
    tickClocks(frame, now);
  });
  scr.enter?.();
  return {
    el: frame,
    screen: scr,
    destroy() {
      scr.leave?.();
      unsub();
      stop();
    },
  };
}

export interface DeviceOptions {
  /** The screen to draw for (size + density, from `devices` in @facet/tokens). Default the 720 × 720 reference. */
  device?: DeviceId;
  /** Screen to start on. Default `home`. */
  start?: ScreenId;
  /** Home cards, in order. */
  cards?: CardId[];
  /** Toast position. Default `toast.position` (top). */
  toastPosition?: 'top' | 'bottom';
  /** Called when a toast shows (e.g. to play the Notify sound). */
  onToast?: (t: ToastData) => void;
  /** Show the idle clock after this many ms without a touch; 0 = never. Default 60 s. */
  idleAfter?: number;
  /** Screen brightness while idle, 0.1 to 1 (the device dims its backlight; here a CSS filter). Default 0.7. */
  idleBrightness?: number;
  /** Media screen: what sits in the hero slot. Default `disc`. */
  mediaVisual?: MediaVisualId;
  /** Face of the idle clock. Default `ring`. */
  idleFace?: ClockOptions['face'];
  /** Screen transition. Default `motion.transition.default` (rise); use `instant` on slow devices. */
  transition?: TransitionId;
  /** Called after every navigation. */
  onNavigate?: (id: ScreenId | 'idle') => void;
}

/** The whole panel: every screen, navigation, toasts and the idle clock. */
export function createDevice(ctx: ScreenContext, o: DeviceOptions = {}) {
  const frame = h('div', { class: 'f-screen f-device' });
  setDevice(frame, o.device);
  if (o.device) ctx = { ...ctx, device: o.device };
  const pages = new Map<ScreenId | 'idle', Screen>();
  const back = () => go('home');
  for (const id of SCREEN_IDS)
    pages.set(
      id,
      createScreen(id, ctx, {
        onBack: id === 'home' ? undefined : back,
        open: (c) => c !== 'readouts' && go(c),
        openSettings: () => go('settings'),
        mediaVisual: o.mediaVisual,
      }),
    );
  let idleFace = o.idleFace ?? 'ring';
  pages.set('idle', clockScreen(ctx, { face: idleFace, bare: true }));
  for (const p of pages.values()) {
    p.el.hidden = true;
    frame.append(p.el);
  }
  /** A new idle clock face: rebuild the idle page in place. */
  function setIdleFace(face: NonNullable<ClockOptions['face']>) {
    if (face === idleFace) return;
    idleFace = face;
    const old = pages.get('idle')!;
    const next = clockScreen(ctx, { face, bare: true });
    next.el.hidden = old.el.hidden;
    old.leave?.();
    old.el.replaceWith(next.el);
    pages.set('idle', next);
    next.update?.(ctx.store.get());
    if (cur === 'idle') next.enter?.();
  }
  const toasts = toastLayer({ position: o.toastPosition ?? TOAST.position, onShow: o.onToast });
  frame.append(toasts);
  let cur: ScreenId | 'idle' = o.start ?? 'home';
  pages.get(cur)!.el.hidden = false;
  frame.dataset.page = cur;

  let transition: TransitionId = o.transition ?? motion.transition.default;
  let rising: Animation[] = [];
  /** Content rise: the page's blocks grouped into rows by their top, header first, each row a step later. */
  function rise(page: HTMLElement, dy: number) {
    const r = motion.transition.rise;
    const hd = page.querySelector<HTMLElement>(':scope > .fs-header');
    const blocks: HTMLElement[] = [];
    for (const c of Array.from(page.children) as HTMLElement[]) {
      if (c === hd || c.hidden || c.classList.contains('f-track')) continue;
      if (c.classList.contains('fs-content') && c.children.length > 1)
        blocks.push(...(Array.from(c.children) as HTMLElement[]));
      else blocks.push(c);
    }
    const byTop = new Map<number, HTMLElement[]>();
    for (const b of blocks) {
      const t = Math.round(b.getBoundingClientRect().top);
      (byTop.get(t) ?? byTop.set(t, []).get(t)!).push(b);
    }
    const rows = [...byTop.entries()].sort((a, b) => a[0] - b[0]).map((e) => e[1]);
    if (hd) rows.unshift([hd]);
    rising = rows.flatMap((row, i) =>
      row.map((b) =>
        b.animate(
          [
            { opacity: 0, transform: `translateY(${dy}px)` },
            { opacity: 1, transform: 'none' },
          ],
          {
            duration: r.duration,
            delay: Math.min(i, r['max-blocks'] - 1) * r.stagger,
            easing: motion.easing[r.easing].css,
            fill: 'backwards',
          },
        ),
      ),
    );
  }

  function go(id: ScreenId | 'idle') {
    if (id === cur) return;
    rising.forEach((a) => a.finish());
    rising = [];
    const from = pages.get(cur)!,
      to = pages.get(id)!;
    from.leave?.();
    from.el.hidden = true;
    to.el.hidden = false;
    cur = id;
    frame.dataset.page = id;
    if (id === 'idle') frame.dataset.dimmed = '';
    else delete frame.dataset.dimmed;
    to.enter?.();
    if (transition === 'rise' && id !== 'idle')
      rise(to.el, id === 'home' ? -motion.transition.rise.offset : motion.transition.rise.offset);
    o.onNavigate?.(id);
  }

  // idle: after a while without a touch, the clock, dimmed; the next touch goes back to Home
  let idleAfter = o.idleAfter ?? 60_000;
  const setIdleBrightness = (b: number) =>
    frame.style.setProperty('--f-idle-brightness', String(Math.min(1, Math.max(0.1, b))));
  setIdleBrightness(o.idleBrightness ?? 0.7);
  let idleTimer: ReturnType<typeof setTimeout> | undefined;
  const arm = () => {
    clearTimeout(idleTimer);
    if (idleAfter > 0) idleTimer = setTimeout(() => go('idle'), idleAfter);
  };
  frame.addEventListener(
    'pointerdown',
    (e) => {
      if (cur === 'idle') {
        e.preventDefault();
        e.stopPropagation();
        go('home');
      }
      arm();
    },
    true,
  );
  arm();

  // personalisation settings from the store: the look on the frame; behaviour only when a value changes, so the
  // options above (and setTransition) hold until the store says otherwise
  const applied: Partial<Values> = {};
  function applyStoreSettings(st: Partial<Values>) {
    applySettings(frame, st);
    const changed = (id: string) => {
      if (!(id in st) || st[id] === applied[id]) return false;
      applied[id] = st[id];
      return true;
    };
    if (changed('transition')) transition = st.transition as TransitionId;
    if (changed('toastPosition')) toasts.setPosition(st.toastPosition as 'top' | 'bottom');
    if (changed('idleAfter')) {
      idleAfter = Number(st.idleAfter) * 1000;
      arm();
    }
    if (changed('idleBrightness')) setIdleBrightness(Number(st.idleBrightness) / 100);
    if (changed('clockFace')) setIdleFace(st.clockFace as NonNullable<ClockOptions['face']>);
  }

  const unsub = ctx.store.subscribe((s) => {
    applyStoreSettings(s.settings ?? {});
    pages.forEach((p) => p.update?.(s));
  });
  const stop = startTicker((now) => {
    pages.forEach((p) => p.tick?.(now));
    tickClocks(frame, now);
  });
  pages.get(cur)!.enter?.();

  return {
    el: frame,
    go,
    /** Show the idle clock now. */
    sleep: () => go('idle'),
    /** Show a toast; it also wakes a dimmed screen to full brightness (it stays on the idle clock). */
    toast: (t: ToastData, opts?: { onAction?: () => void }) => {
      delete frame.dataset.dimmed;
      arm();
      return toasts.push(t, opts);
    },
    toasts,
    /** Change the screen transition. */
    setTransition(t: TransitionId) {
      transition = t;
    },
    get transition() {
      return transition;
    },
    get current() {
      return cur;
    },
    destroy() {
      clearTimeout(idleTimer);
      unsub();
      stop();
    },
  };
}
export type Device = ReturnType<typeof createDevice>;
