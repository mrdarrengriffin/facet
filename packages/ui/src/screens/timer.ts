// Timer: the time left in the display style inside a ring that empties as it runs, the timer's name above it, and
// two buttons (+1 min, Cancel). With no timer running it offers one-tap presets.
import { button } from '../buttons.js';
import { countdown } from '../clocks.js';
import { arc } from '../controls.js';
import { h } from '../dom.js';
import { at, CH, CW, content, header, page, text } from '../layout.js';
import type { Timer } from '../types.js';
import type { Screen, ScreenContext, ScreenOptions } from './common.js';

export interface TimerOptions extends ScreenOptions {
  /** Presets offered when nothing is running, in minutes. Default 3, 5, 10, 20. */
  presets?: number[];
}

export function timerScreen(ctx: ScreenContext, o: TimerOptions = {}): Screen {
  const A = ctx.actions;
  const S = 432;
  const ring = arc({
    size: S,
    start: 270,
    end: 269,
    track: { w: 2, c: 'line' },
    ind: { w: 6, c: 'accent' },
    value: 0,
  });
  const lbl = text('label', 'fs-t-label', 'timer');
  const left = text('display', 'fs-t-left', '0:00');
  const sub = text('body', 'fs-t-sub', '');
  const mid = h('div', { class: 'fs-t-mid' }, lbl, left, sub);
  const running = h(
    'div',
    { class: 'fs-t-btns' },
    button({ kind: 'secondary', text: '+1 min', onTap: () => A?.extendTimer?.(60) }),
    button({ kind: 'outline', text: 'Cancel', onTap: () => A?.cancelTimer?.() }),
  );
  const presets = h(
    'div',
    { class: 'fs-t-btns' },
    ...(o.presets ?? [3, 5, 10, 20]).map((m) =>
      button({
        kind: m === (o.presets ?? [3, 5, 10, 20])[1] ? 'primary' : 'secondary',
        text: `${m} min`,
        onTap: () => A?.startTimer?.(m * 60, `${m} min timer`),
      }),
    ),
  );
  const top = (CH - S - 96) / 2;
  const el = page(
    'fs-timer',
    header({ label: 'Timer', onBack: o.onBack }),
    content(at((CW - S) / 2, top, S, S, ring, mid), at(0, top + S + 32, CW, 64, running, presets)),
  );
  let t: Timer | undefined;
  const render = (now = Date.now()) => {
    const on = !!t && t.endsAt > now;
    running.hidden = !on;
    presets.hidden = on;
    if (!t || !on) {
      lbl.textContent = t ? 'done' : 'timer';
      left.textContent = '0:00';
      sub.textContent = t ? t.label : 'pick a length';
      ring.set(0);
      el.classList.toggle('is-done', !!t);
      return;
    }
    el.classList.remove('is-done');
    const s = (t.endsAt - now) / 1000;
    lbl.textContent = t.label;
    left.textContent = countdown(s);
    sub.textContent = `of ${Math.round(t.total / 60)} min · ends ${new Date(t.endsAt).toLocaleTimeString('en-GB', { hour: '2-digit', minute: '2-digit' })}`;
    ring.set((s / t.total) * 100);
  };
  return {
    el,
    update(s) {
      const next = s.timers?.[0];
      // a timer that ran out stays as "done" until the next one; one that vanishes early was cancelled
      if (next) t = next;
      else if (t && t.endsAt > Date.now()) t = undefined;
      render();
    },
    tick() {
      render();
    },
  };
}
