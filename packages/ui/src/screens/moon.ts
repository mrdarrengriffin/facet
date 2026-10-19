// Moon: the moon itself as the hero (384 px, drawn from the phase) and one line under it: "Waxing gibbous · 78% ·
// full on Fri". "Details" opens the numbers: how much is lit, the age, the next moonrise and moonset, the next full and new
// moon, and the next four phases.
import { moonIcon } from '@facet/icons';
import { h } from '../dom.js';
import { hm } from '../format.js';
import {
  at,
  CH,
  CW,
  content,
  header,
  layout,
  moreRow,
  page,
  stat,
  subView,
  text,
} from '../layout.js';
import { MOON_PHASE_TEXT } from '../moon.js';
import type { MoonData, Tone } from '../types.js';
import { missing, type Screen, type ScreenContext, type ScreenOptions } from './common.js';

const DAY = 86_400_000;
// stat values in full text colour (the stat's tone styles only set dim)
const T = 'text' as Tone;
const FRAC = { new_moon: 0, first_quarter: 0.25, full_moon: 0.5, last_quarter: 0.75 } as const;

/** "tonight", "tomorrow", "Fri" within a week, else "24 Oct". */
export function moonDay(iso: string, now = Date.now()) {
  const t = new Date(iso),
    n = new Date(now);
  const days = Math.round(
    (new Date(t).setHours(0, 0, 0, 0) - new Date(n).setHours(0, 0, 0, 0)) / DAY,
  );
  if (days === 0) return t.getHours() >= 17 ? 'tonight' : 'today';
  if (days === 1) return 'tomorrow';
  if (days < 7) return t.toLocaleDateString('en-GB', { weekday: 'short' });
  return t.toLocaleDateString('en-GB', { day: 'numeric', month: 'short' });
}

/** The one line: "Waxing gibbous · 78% · full on Fri" (the next full or new moon, whichever comes first). */
export function moonLine(m: MoonData, now = Date.now()) {
  const full = Date.parse(m.next_full) < Date.parse(m.next_new);
  const next = full ? m.next_full : m.next_new;
  const when = moonDay(next, now);
  const on = /^(today|tonight|tomorrow)$/.test(when) ? when : `on ${when}`;
  return {
    name: MOON_PHASE_TEXT[m.phase],
    rest: `${Math.round(m.illumination)}% · ${full ? 'full' : 'new'} ${on}`,
  };
}

export function moonScreen(ctx: ScreenContext, o: ScreenOptions = {}): Screen {
  const S = 384;
  const hero = moonIcon(0.25, { size: S });
  hero.classList.add('fs-moon-hero');
  const name = text('title', 'fs-moon-name', 'Loading');
  const rest = text('title', 'fs-moon-rest', '');
  const line = h('div', { class: 'fs-moon-line' }, name, rest);
  const more = moreRow('details', () => {
    sumView.hidden = true;
    detView.hidden = false;
  });
  const sumView = subView(
    header({ label: 'Moon', onBack: o.onBack }),
    content(
      at((CW - S) / 2, 0, S, S, hero),
      at(0, S + 32, CW, 40, line),
      at(0, CH - layout['more-row-h'], CW, layout['more-row-h'], more),
    ),
  );

  const sLit = stat('lit'),
    sAge = stat('age'),
    sRise = stat('moonrise'),
    sSet = stat('moonset'),
    sFull = stat('next full'),
    sNew = stat('next new');
  const COL = CW / 4;
  const next = Array.from({ length: 4 }, () => ({
    ic: moonIcon(0, { size: 48 }),
    k: text('label', 'fs-mn-k', '--'),
    d: text('body', 'fs-mn-d', '--'),
  }));
  const strip = h(
    'div',
    { class: 'fs-mn' },
    ...next.map((c) =>
      h('div', { class: 'fs-mn-col', style: { width: `${COL}px` } }, c.ic, c.k, c.d),
    ),
  );
  const half = (CW - layout.gutter) / 2;
  const detView = subView(
    header({
      label: 'Moon details',
      onBack: () => {
        detView.hidden = true;
        sumView.hidden = false;
      },
    }),
    content(
      at(0, 0, half, 64, sLit),
      at(half + layout.gutter, 0, half, 64, sAge),
      at(0, 96, half, 64, sRise),
      at(half + layout.gutter, 96, half, 64, sSet),
      at(0, 192, half, 64, sFull),
      at(half + layout.gutter, 192, half, 64, sNew),
      at(0, 296, CW, 24, text('label', 'fs-dim', 'next phases')),
      at(0, 336, CW, 136, strip),
    ),
  );
  detView.hidden = true;

  let m: MoonData | null | undefined;
  const dayTime = (iso: string, now: number) => `${moonDay(iso, now)} · ${hm(iso)}`;
  const render = (now = Date.now()) => {
    if (!m) {
      name.textContent = missing(ctx.store) === 'loading' ? 'Loading' : 'No data';
      rest.textContent = '';
      return;
    }
    hero.set(m.frac, { south: m.south });
    const l = moonLine(m, now);
    name.textContent = l.name;
    rest.textContent = ` · ${l.rest}`;
    sLit.set(`${Math.round(m.illumination)}%`, T);
    sAge.set(`${m.age.toFixed(1)} days`, T);
    sRise.set(m.next_rise ? dayTime(m.next_rise, now) : '--', m.next_rise ? T : 'dim');
    sSet.set(m.next_set ? dayTime(m.next_set, now) : '--', m.next_set ? T : 'dim');
    sFull.set(dayTime(m.next_full, now), T);
    sNew.set(dayTime(m.next_new, now), T);
    const up = m.upcoming ?? [];
    next.forEach((c, i) => {
      const p = up[i];
      c.ic.set(p ? FRAC[p.phase] : 0, { south: m!.south });
      c.k.textContent = p ? MOON_PHASE_TEXT[p.phase].replace(' moon', '') : '--';
      c.d.textContent = p ? moonDay(p.t, now) : '--';
    });
  };
  return {
    el: page('fs-moon', sumView, detView),
    update(s) {
      m = s.moon;
      render();
    },
    leave() {
      detView.hidden = true;
      sumView.hidden = false;
    },
  };
}
