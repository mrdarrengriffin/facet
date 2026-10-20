// Moon: the moon itself as the hero (384 px, drawn from the phase) and one line under it: "Waxing gibbous · 78% ·
// full on Fri". "Details" opens the numbers: how much is lit, the age, the next moonrise and moonset, the next full and new
// moon, and the next four phases. From there "next 75 days" opens a bar chart of the evening moon, one bar per day.
import { moonIcon } from '@facet/icons';
import { barChart, type Bar } from '../charts.js';
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
import { MOON_PHASE_TEXT, moonDays, moonPhaseName, type MoonDay } from '../moon.js';
import type { MoonData, Tone } from '../types.js';
import { missing, type Screen, type ScreenContext, type ScreenOptions } from './common.js';

const DAY = 86_400_000;
// stat values in full text colour (the stat's tone styles only set dim)
const T = 'text' as Tone;
const FRAC = { new_moon: 0, first_quarter: 0.25, full_moon: 0.5, last_quarter: 0.75 } as const;
const EVENT = {
  new: 'new_moon',
  first: 'first_quarter',
  full: 'full_moon',
  last: 'last_quarter',
} as const;
/** Days the long-range chart shows before today. */
export const MOON_RANGE_BACK = 7;

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

/**
 * The long-range chart: one 8 px slot per day across `w` (bar 6 + gap 2), bar height = the evening moon's
 * illumination. Full-moon days in accent, days before `today` in line-hi, the rest steel; a dotted line on today.
 * A 24 px moon sits above every new and full moon, month names under the 1st. `onTap(i)` = a day (-1 = none).
 */
export function moonRangeChart(o: {
  w: number;
  h: number;
  days: MoonDay[];
  today: number;
  south?: boolean;
  onTap?: (i: number) => void;
}) {
  const colour = (d: MoonDay, i: number, today: number) =>
    d.event === 'full' ? 'accent' : i < today ? 'line-hi' : 'steel';
  const bars = (days: MoonDay[], today: number): Bar[] =>
    days.map((d, i) => ({ v: d.illumination, c: colour(d, i, today) }));
  const chart = barChart({
    w: o.w,
    h: o.h,
    bars: bars(o.days, o.today),
    max: 100,
    gap: 2,
    radius: 2,
    now: o.today,
    onTap: o.onTap,
  });
  const marks = h('div', { class: 'fs-mr-marks', style: { width: `${o.w}px` } });
  const axis = h('div', { class: 'fs-mr-axis', style: { width: `${o.w}px` } });
  const set = (days: MoonDay[], today: number, south = false) => {
    const slot = o.w / Math.max(1, days.length);
    chart.set(bars(days, today), today);
    marks.replaceChildren(
      ...days.flatMap((d, i) => {
        if (d.event !== 'new' && d.event !== 'full') return [];
        const g = moonIcon(d.event === 'full' ? 0.5 : 0, { size: 24, south });
        return [
          h(
            'span',
            { class: 'fs-mr-mark', style: { left: `${Math.round(i * slot + slot / 2 - 12)}px` } },
            g,
          ),
        ];
      }),
    );
    // month names at the left edge of the 1st's bar (none in the last 48 px, they would run off)
    axis.replaceChildren(
      ...days.flatMap((d, i) => {
        const t = new Date(d.t);
        if (t.getDate() !== 1 || i * slot > o.w - 48) return [];
        const el = text('label', 'fs-mr-tick', t.toLocaleDateString('en-GB', { month: 'short' }));
        el.style.left = `${Math.round(i * slot)}px`;
        return [el];
      }),
    );
  };
  set(o.days, o.today, o.south);
  return { chart, marks, axis, set };
}

/**
 * The Moon's parts, for the Moon screen and the Weather screen's Moon view: the moon (`size`) centred at `top`, the
 * line under it and the "details" row (`nodes`, for a content box), and the details and next-75-days sub-views
 * (`views`, page children; `openDetails()` / `closeDetails()` show and hide them).
 */
export function moonParts(
  ctx: ScreenContext,
  o: { top: number; size: number; onDetails: () => void; onDetailsBack: () => void },
) {
  const S = o.size;
  const hero = moonIcon(0.25, { size: S });
  hero.classList.add('fs-moon-hero');
  const name = text('title', 'fs-moon-name', 'Loading');
  const rest = text('title', 'fs-moon-rest', '');
  const line = h('div', { class: 'fs-moon-line' }, name, rest);
  const nodes = [
    at((CW - S) / 2, o.top, S, S, hero),
    at(0, o.top + S + 32, CW, 40, line),
    at(0, CH - layout['more-row-h'], CW, layout['more-row-h'], moreRow('details', o.onDetails)),
  ];

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
  const rangeMore = moreRow(`next ${Math.floor(CW / 8) - MOON_RANGE_BACK} days`, () => {
    fillRange(south);
    showDay(); // a day tapped earlier stays picked (the chart keeps its highlight)
    detView.hidden = true;
    rangeView.hidden = false;
    range.chart.play(6, 360);
  });
  const detView = subView(
    header({ label: 'Moon details', onBack: o.onDetailsBack }),
    content(
      at(0, 0, half, 64, sLit),
      at(half + layout.gutter, 0, half, 64, sAge),
      at(0, 96, half, 64, sRise),
      at(half + layout.gutter, 96, half, 64, sSet),
      at(0, 192, half, 64, sFull),
      at(half + layout.gutter, 192, half, 64, sNew),
      at(0, 296, CW, 24, text('label', 'fs-dim', 'next phases')),
      at(0, 336, CW, 136, strip),
      at(0, CH - layout['more-row-h'], CW, layout['more-row-h'], rangeMore),
    ),
  );
  detView.hidden = true;

  // next N days: one 8 px slot per day across the content, MOON_RANGE_BACK of them before today
  const N = Math.floor(CW / 8),
    BACK = MOON_RANGE_BACK;
  const rName = text('title', 'fs-moon-name', 'Loading');
  const rRest = text('title', 'fs-moon-rest', '');
  let days = moonDays(Date.now() - BACK * DAY, N),
    sel = -1,
    dayKey = '',
    south = false;
  const showDay = () => {
    const d = days[sel >= 0 ? sel : BACK]!;
    rName.textContent = MOON_PHASE_TEXT[d.event ? EVENT[d.event] : moonPhaseName(d.frac)];
    rRest.textContent = ` · ${d.illumination}% · ${sel >= 0 ? moonDay(new Date(d.t).toISOString()) : 'tonight'}`;
  };
  const range = moonRangeChart({
    w: CW,
    h: 360,
    days,
    today: BACK,
    onTap: (i) => {
      sel = i;
      showDay();
    },
  });
  const rangeView = subView(
    header({
      label: `Next ${N - BACK} days`,
      onBack: () => {
        rangeView.hidden = true;
        detView.hidden = false;
      },
    }),
    content(
      at(0, 0, CW, 40, h('div', { class: 'fs-moon-line' }, rName, rRest)),
      at(0, 72, CW, 24, range.marks),
      at(0, 112, CW, 360, range.chart),
      at(0, 488, CW, 24, range.axis),
    ),
  );
  rangeView.hidden = true;
  // new bars once a day (and when the hemisphere changes)
  const fillRange = (sth: boolean) => {
    const key = new Date().toDateString();
    if (key === dayKey && sth === south) return;
    dayKey = key;
    south = sth;
    sel = -1;
    days = moonDays(Date.now() - BACK * DAY, N);
    range.set(days, BACK, south);
  };

  const dayTime = (iso: string, now: number) => `${moonDay(iso, now)} · ${hm(iso)}`;
  const render = (m: MoonData | null | undefined, now = Date.now()) => {
    if (!m) {
      name.textContent = missing(ctx.store) === 'loading' ? 'Loading' : 'No data';
      rest.textContent = '';
      return;
    }
    hero.set(m.frac, { south: m.south });
    fillRange(!!m.south);
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
      c.ic.set(p ? FRAC[p.phase] : 0, { south: m.south });
      c.k.textContent = p ? MOON_PHASE_TEXT[p.phase].replace(' moon', '') : '--';
      c.d.textContent = p ? moonDay(p.t, now) : '--';
    });
  };
  return {
    nodes,
    views: [detView, rangeView],
    render,
    openDetails() {
      rangeView.hidden = true;
      detView.hidden = false;
    },
    closeDetails() {
      detView.hidden = true;
      rangeView.hidden = true;
    },
  };
}

export function moonScreen(ctx: ScreenContext, o: ScreenOptions = {}): Screen {
  const parts = moonParts(ctx, {
    top: 0,
    size: 384,
    onDetails: () => {
      sumView.hidden = true;
      parts.openDetails();
    },
    onDetailsBack: () => {
      parts.closeDetails();
      sumView.hidden = false;
    },
  });
  const sumView = subView(header({ label: 'Moon', onBack: o.onBack }), content(...parts.nodes));
  return {
    el: page('fs-moon', sumView, ...parts.views),
    update(s) {
      parts.render(s.moon);
    },
    leave() {
      parts.closeDetails();
      sumView.hidden = false;
    },
  };
}
