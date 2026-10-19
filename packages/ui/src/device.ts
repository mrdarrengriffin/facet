// The device: every screen in one 720 × 720 frame, with navigation (tap a Home card to open its screen, Back returns
// Home), a toast layer over everything, and an idle clock after a while without a touch. Screens switch instantly.
import { toast as TOAST } from '@facet/tokens';
import { h } from './dom.js';
import { hhmm } from './format.js';
import { startTicker } from './store.js';
import { toastLayer, type ToastData } from './toast.js';
import { agentsScreen } from './screens/agents.js';
import { clockScreen, type ClockOptions } from './screens/clock.js';
import type { Screen, ScreenContext } from './screens/common.js';
import { energyScreen } from './screens/energy.js';
import { githubScreen } from './screens/github.js';
import { homeScreen, type CardId } from './screens/home.js';
import { lightScreen } from './screens/light.js';
import { lightningScreen } from './screens/lightning.js';
import { mediaScreen } from './screens/media.js';
import { timerScreen } from './screens/timer.js';
import { weatherScreen } from './screens/weather.js';

export type ScreenId = 'home' | CardId | 'timer';
export const SCREEN_IDS: ScreenId[] = [
  'home',
  'agents',
  'media',
  'light',
  'clock',
  'github',
  'energy',
  'weather',
  'lightning',
  'timer',
];

/** Build one screen by id. `onBack` shows the back button; leave it out for a standalone preview. */
export function createScreen(
  id: ScreenId,
  ctx: ScreenContext,
  o: { onBack?: () => void; open?: (id: CardId) => void; clock?: ClockOptions } = {},
): Screen {
  switch (id) {
    case 'home':
      return homeScreen(ctx, { open: o.open });
    case 'agents':
      return agentsScreen(ctx, o);
    case 'media':
      return mediaScreen(ctx, o);
    case 'light':
      return lightScreen(ctx, o);
    case 'clock':
      return clockScreen(ctx, { ...o.clock, onBack: o.onBack });
    case 'github':
      return githubScreen(ctx, o);
    case 'energy':
      return energyScreen(ctx, o);
    case 'weather':
      return weatherScreen(ctx, o);
    case 'lightning':
      return lightningScreen(ctx, o);
    case 'timer':
      return timerScreen(ctx, o);
  }
}

/** Run a screen's clocks: the header time and anything with `data-clock` gets the time once a second. */
function tickClocks(root: HTMLElement, now: Date) {
  root
    .querySelectorAll<HTMLElement & { tick?: (n: Date) => void }>('[data-clock]')
    .forEach((e) => (e.tick ? e.tick(now) : (e.textContent = hhmm(now))));
}

export interface PreviewOptions {
  onBack?: () => void;
  clock?: ClockOptions;
}

/**
 * One screen in its own 720 × 720 frame, bound to the store and ticking: what the docs use for live previews.
 * Without `onBack` there's no back button. Call `destroy()` when you're done with it.
 */
export function mountScreen(id: ScreenId, ctx: ScreenContext, o: PreviewOptions = {}) {
  const frame = h('div', { class: 'f-screen', 'data-screen': id });
  const scr = createScreen(id, ctx, o);
  frame.append(scr.el);
  const unsub = ctx.store.subscribe((s) => scr.update?.(s));
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
  /** Face of the idle clock. Default `ring`. */
  idleFace?: ClockOptions['face'];
  /** Called after every navigation. */
  onNavigate?: (id: ScreenId | 'idle') => void;
}

/** The whole panel: every screen, navigation, toasts and the idle clock. */
export function createDevice(ctx: ScreenContext, o: DeviceOptions = {}) {
  const frame = h('div', { class: 'f-screen f-device' });
  const pages = new Map<ScreenId | 'idle', Screen>();
  const back = () => go('home');
  for (const id of SCREEN_IDS)
    pages.set(
      id,
      createScreen(id, ctx, { onBack: id === 'home' ? undefined : back, open: (c) => go(c) }),
    );
  pages.set('idle', clockScreen(ctx, { face: o.idleFace ?? 'ring', bare: true }));
  for (const p of pages.values()) {
    p.el.hidden = true;
    frame.append(p.el);
  }
  const toasts = toastLayer({ position: o.toastPosition ?? TOAST.position, onShow: o.onToast });
  frame.append(toasts);
  let cur: ScreenId | 'idle' = o.start ?? 'home';
  pages.get(cur)!.el.hidden = false;
  frame.dataset.page = cur;

  function go(id: ScreenId | 'idle') {
    if (id === cur) return;
    const from = pages.get(cur)!,
      to = pages.get(id)!;
    from.leave?.();
    from.el.hidden = true;
    to.el.hidden = false;
    cur = id;
    frame.dataset.page = id;
    to.enter?.();
    o.onNavigate?.(id);
  }

  // idle: after a while without a touch, the clock; the next touch wakes it back to Home
  const idleAfter = o.idleAfter ?? 60_000;
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

  const unsub = ctx.store.subscribe((s) => pages.forEach((p) => p.update?.(s)));
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
    toast: (t: ToastData, opts?: { onAction?: () => void }) => toasts.push(t, opts),
    toasts,
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
