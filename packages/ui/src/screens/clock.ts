// Clock: a tick ring the height of the content with three dots orbiting at different speeds (60 s, 23 s reversed,
// 37 s), the time in the display style, the date and the weather. Other faces from the clock family can stand in.
import { WX_TEXT } from '@facet/icons';
import { frame as L, motion, screenType, size as Z, type as TYPE } from '@facet/tokens';
import {
  clockAnalogue,
  clockContext,
  clockHero,
  clockRing,
  type ClockFace,
  type ClockProps,
} from '../clocks.js';
import { tickRing } from '../controls.js';
import { h } from '../dom.js';
import { deg, hhmm } from '../format.js';
import { header, page } from '../layout.js';
import type { WeatherData } from '../types.js';
import type { Screen, ScreenContext, ScreenOptions } from './common.js';

export interface ClockOptions extends ScreenOptions {
  /** Which face. Default `orbit`. */
  face?: ClockFace;
  /** Font, format and date style for the clock-family faces. Default mono, 24 h, short date. */
  props?: Partial<ClockProps>;
  /** Hide the header (an idle / ambient clock). */
  bare?: boolean;
}

const weatherLine = (w: WeatherData | null | undefined) => {
  const c = w?.current;
  return c ? `${(WX_TEXT[c.condition] ?? c.condition).toLowerCase()} · ${deg(c.temp)}` : '';
};

/** The orbit face: the Clock screen's own layout. */
function orbitFace() {
  const D = motion.duration;
  const RG = Z['screen-clock-ring'],
    C = RG / 2;
  const ring = tickRing({
    size: RG,
    count: 120,
    range: 357,
    rotation: 270,
    minor: { w: 2, len: 2, c: 'line-hi' },
    major: { stride: 10, w: 3, len: 6, c: 'steel' },
  });
  const orbit = (r: number, size: number, c: string, ms: number, rev: boolean) =>
    h(
      'i',
      {
        class: 'f-abs',
        style: {
          left: `${C}px`,
          top: `${C}px`,
          width: '0',
          height: '0',
          animation: `${rev ? 'f-spin-rev' : 'f-spin-plain'} ${ms}ms linear infinite`,
        },
      },
      h('i', {
        class: 'f-abs',
        style: {
          left: `${-size / 2}px`,
          top: `${-r - size / 2}px`,
          width: `${size}px`,
          height: `${size}px`,
          borderRadius: '999px',
          background: `var(--facet-color-${c})`,
        },
      }),
    );
  const edge = { left: `${L.inset}px`, width: `${L.width}px`, textAlign: 'center' };
  const timeTop = L.cy - 104,
    dateTop = timeTop + screenType.display.box! + 16,
    wxTop = dateTop + TYPE.date.box! + 16;
  const time = h(
    'div',
    { class: 'f-abs fst-display fs-clock-time', style: { ...edge, top: `${timeTop}px` } },
    '--:--',
  );
  const date = h(
    'div',
    { class: 'f-abs ft-date fs-clock-date', style: { ...edge, top: `${dateTop}px` } },
    '',
  );
  const wx = h(
    'div',
    { class: 'f-abs ft-weather fs-clock-wx', style: { ...edge, top: `${wxTop}px` } },
    '',
  );
  const el = h(
    'div',
    { class: 'f-abs', style: { left: '0', top: '0', width: '720px', height: '720px' } },
    h(
      'div',
      {
        class: 'f-abs',
        style: { left: `${L.cx - C}px`, top: `${L.cy - C}px`, width: `${RG}px`, height: `${RG}px` },
      },
      ring,
      orbit(C, 14, 'accent', D['orbit-a'], false),
      orbit(C - 40, 8, 'blue', D['orbit-b'], true),
      orbit(C - 80, 6, 'steel', D['orbit-c'], false),
    ),
    time,
    date,
    wx,
  );
  return {
    el,
    set(now: Date, w: WeatherData | null | undefined) {
      time.textContent = hhmm(now);
      date.textContent = now
        .toLocaleDateString('en-GB', { weekday: 'short', day: '2-digit', month: 'short' })
        .replace(',', '');
      wx.textContent = weatherLine(w);
    },
  };
}

export function clockScreen(ctx: ScreenContext, o: ClockOptions = {}): Screen {
  const face = o.face ?? 'orbit';
  const p: ClockProps = { font: 'mono', format: '24h', date: 'short', ...o.props };
  const orbit = face === 'orbit' ? orbitFace() : null;
  const faceEl = orbit
    ? orbit.el
    : face === 'hero'
      ? clockHero(p)
      : face === 'ring'
        ? clockRing(p)
        : face === 'analogue'
          ? clockAnalogue(p)
          : clockContext(p, () => ctx.store.get());
  faceEl.classList.add('fs-clock-face');
  const el = page(
    `fs-clock fs-clock--${face}`,
    faceEl,
    o.bare ? null : header({ label: 'Clock', onBack: o.onBack, clock: false }),
  );
  return {
    el,
    tick(now) {
      orbit?.set(now, ctx.store.get().weather);
      el.querySelectorAll<HTMLElement & { tick?: (n: Date) => void }>('[data-clock]').forEach((e) =>
        e.tick?.(now),
      );
    },
  };
}
