// Calendar: the agenda. The hero is the next event: when it starts as the big number ("16:50", "Now" while it runs,
// else the day), its title and one quiet line. Below it, the days: a caps day label ("Today · Tue 20 Oct") and a 64 px
// row per event (time, the calendar's dot, the title). Finished events go dim. With `actions.setCalendar`, the header
// gets a calendars button (icon + how many are on) that opens a sheet to show or hide each calendar. Agenda | Month in
// the header switches to the month view (calendar-month.ts); `view` / `month` pick where it starts.
import { h } from '../dom.js';
import { fit8, live, setText, statusDot } from '../atoms.js';
import { checkbox } from '../checkbox.js';
import { hhmm } from '../format.js';
import { at, CW, content, header, page, text } from '../layout.js';
import { bottomSheet } from '../pickers.js';
import { screenIcon } from '@facet/icons';
import { segmented } from '../settings.js';
import { monthView } from './calendar-month.js';
import type { AgendaEvent, Calendar, CalendarData } from '../types.js';
import { missing, type Screen, type ScreenContext, type ScreenOptions } from './common.js';

const DAY = 86_400_000;
/** Starting within this long counts as "now" for the accent. */
export const CAL_SOON = 15 * 60_000;

// ---------------------------------------------------------------- times and wording (local time)
/** Start or end in ms; an all-day date counts from local midnight. */
export const calMs = (e: AgendaEvent, k: 'start' | 'end') =>
  e.allDay ? Date.parse(`${e[k]}T00:00:00`) : Date.parse(e[k]);
const midnight = (t: number) => new Date(t).setHours(0, 0, 0, 0);
const dayDiff = (t: number, now: number) => Math.round((midnight(t) - midnight(now)) / DAY);
const hm = (t: number) => hhmm(new Date(t));
const wd = (t: number) => new Date(t).toLocaleDateString('en-GB', { weekday: 'short' });
const dm = (t: number) =>
  new Date(t).toLocaleDateString('en-GB', { day: 'numeric', month: 'short' });
const longDay = (t: number) =>
  new Date(t).toLocaleDateString('en-GB', { weekday: 'short', day: 'numeric', month: 'short' });

/** True while the event is on. */
export const isRunning = (e: AgendaEvent, now = Date.now()) =>
  calMs(e, 'start') <= now && calMs(e, 'end') > now;

/**
 * The hero (and Home card) event: the running or next timed event, or an all-day event from tomorrow on. Today's
 * all-day events stay in the list, so they don't hide the day's appointments.
 */
export function nextAgendaEvent(d: CalendarData | null | undefined, now = Date.now()) {
  return (
    d?.events.find((e) => calMs(e, 'end') > now && (!e.allDay || calMs(e, 'start') > now)) ?? null
  );
}

/** "now", "in 25 min", "in 2h 15m" (under 3 hours), "today 16:50", "tomorrow 09:00", "Thu 19:30", "24 Oct 19:30". All-day: "today", "tomorrow", "Thu", "24 Oct". */
export function calWhen(e: AgendaEvent, now = Date.now()) {
  const s = calMs(e, 'start'),
    dd = dayDiff(s, now);
  if (isRunning(e, now)) return 'now';
  const day = dd === 0 ? 'today' : dd === 1 ? 'tomorrow' : dd < 7 ? wd(s) : dm(s);
  if (e.allDay) return day;
  const min = Math.ceil((s - now) / 60_000);
  if (min < 60) return `in ${min} min`;
  if (dd === 0 && min < 180)
    return `in ${Math.floor(min / 60)}h ${String(min % 60).padStart(2, '0')}m`;
  return `${day} ${hm(s)}`;
}

/** "16:50 – 17:50", "all day", "all day · 3 days". */
export function calTime(e: AgendaEvent) {
  if (e.allDay) {
    const n = Math.round((calMs(e, 'end') - calMs(e, 'start')) / DAY);
    return n > 1 ? `all day · ${n} days` : 'all day';
  }
  return `${hm(calMs(e, 'start'))} – ${hm(calMs(e, 'end'))}`;
}

/** The Home card's line: "in 25 min · 16:50", "today 16:50", "now · until 17:50". */
export function calCardLine(e: AgendaEvent, now = Date.now()) {
  if (isRunning(e, now)) return `now · until ${hm(calMs(e, 'end'))}`;
  const when = calWhen(e, now);
  return !e.allDay && when.startsWith('in ') ? `${when} · ${hm(calMs(e, 'start'))}` : when;
}

/** The events by local day (midnight ms), from today; an event lasting several days is on each of them. */
export function agendaDays(events: AgendaEvent[], now = Date.now()) {
  const by = new Map<number, AgendaEvent[]>();
  for (const e of events) {
    const end = calMs(e, 'end');
    for (
      let t = Math.max(midnight(calMs(e, 'start')), midnight(now)), i = 0;
      t < end && i < 31;
      t = midnight(t + DAY + 3_600_000), i++
    ) {
      if (!by.has(t)) by.set(t, []);
      by.get(t)!.push(e);
    }
  }
  return [...by.entries()].sort((a, b) => a[0] - b[0]);
}

// ---------------------------------------------------------------- parts
const dot = (colour = 'text-dim') =>
  h('i', { class: 'fs-cal-dot', style: { background: `var(--facet-color-${colour})` } });

/** An agenda row (64, hairline on top): time 112 (body; "now" in the accent, "all day") · the calendar's dot · title. */
export function agendaRow(e: AgendaEvent, colour?: string, now = Date.now()) {
  const past = calMs(e, 'end') <= now,
    on = isRunning(e, now);
  const time = e.allDay ? 'all day' : on ? 'now' : hm(calMs(e, 'start'));
  return h(
    'div',
    { class: `fs-cal-row${past ? ' is-past' : ''}${on ? ' is-now' : ''}` },
    text('body', 'fs-cal-time', time),
    dot(colour),
    text('title', 'fs-cal-title', e.summary),
  );
}

/** The Home card preview (72): a desk calendar page, the weekday (caps) over the day number. `set(ms)`. */
export function previewCalendar(size = 72) {
  const k = text('label', 'fs-calt-k'),
    n = text('title', 'fs-calt-n');
  const el = h(
    'span',
    { class: 'fs-calt', style: { width: `${size}px`, height: `${size}px` } },
    k,
    n,
  );
  const set = (t = Date.now()) => {
    k.textContent = wd(t);
    n.textContent = String(new Date(t).getDate());
  };
  set();
  return Object.assign(el, { set });
}

// ---------------------------------------------------------------- the screen
export type CalendarView = 'agenda' | 'month';
export interface CalendarOptions extends ScreenOptions {
  /** Which view it opens on. Default `agenda`. */
  view?: CalendarView;
  /** The month view's first month (YYYY-MM). Default this month. */
  month?: string;
}

export function calendarScreen(ctx: ScreenContext, o: CalendarOptions = {}): Screen {
  // hero = the heroBlock layout with the title in title type (not caps)
  const hN = fit8(text('hero', 'fs-hero-n', '--')),
    hTitle = text('title', 'fs-cal-htitle', ''),
    hLine = text('body', 'fs-hero-s', '');
  const heroEl = h(
    'div',
    { class: 'fs-hero fs-cal-hero' },
    hN,
    h('div', { class: 'fs-hero-t' }, hTitle, hLine),
  );
  const hero = {
    set(st: { n: string; label: string; line: string; accent: boolean }) {
      setText(hN, st.n);
      hTitle.textContent = st.label;
      hLine.textContent = st.line;
      heroEl.classList.toggle('is-accent', st.accent);
    },
  };
  const days = h('div', { class: 'fs-cal-days' });

  const setCal = ctx.actions?.setCalendar;
  const sheet = bottomSheet({ title: 'calendars' });
  const fCount = text('label', '', '--');
  const fBtn = setCal
    ? h(
        'button',
        {
          class: 'fs-cal-fbtn fs-cal-fbtn--icon',
          type: 'button',
          'data-snd': 'open',
          'aria-label': 'Calendars',
        },
        screenIcon('calendar', 24),
        fCount,
      )
    : null;
  if (fBtn) live(fBtn, () => sheet.open());
  const sheetRow = (c: Calendar, n: number) => {
    const box = checkbox({ state: c.on ? 'done' : 'open' });
    const el = h(
      'div',
      { class: 'fs-cal-srow', role: 'button', 'data-snd': 'tap' },
      dot(c.on ? c.colour : 'text-mute'),
      h(
        'span',
        { class: 'fs-cal-stext' },
        text('title', 'fs-cal-sname', c.name),
        text('label', 'fs-cal-sstat', c.on ? `${n} event${n === 1 ? '' : 's'}` : 'hidden'),
      ),
      box,
    );
    live(el, () => {
      box.set(c.on ? 'open' : 'done');
      setCal?.(c.id, !c.on);
    });
    return el;
  };

  let d: CalendarData | null | undefined,
    lastMin = -1;
  const render = (now = Date.now()) => {
    lastMin = Math.floor(now / 60_000);
    const colour = Object.fromEntries((d?.calendars ?? []).map((c) => [c.id, c.colour]));
    const nOn = d?.calendars.filter((c) => c.on).length ?? 0;
    fCount.textContent = d ? String(nOn) : '--';
    const counts: Record<string, number> = {};
    for (const e of d?.events ?? [])
      if (e.calendar) counts[e.calendar] = (counts[e.calendar] ?? 0) + 1;
    sheet.list.replaceChildren(
      ...[...(d?.calendars ?? [])]
        .sort((a, b) => Number(b.on) - Number(a.on))
        .map((c) => sheetRow(c, counts[c.id] ?? 0)),
    );

    const e = nextAgendaEvent(d, now);
    if (!d)
      hero.set({
        n: '--',
        label: missing(ctx.store) === 'loading' ? 'Loading' : 'No data',
        line: '',
        accent: false,
      });
    else if (!e)
      hero.set({
        n: '--',
        label: d.error ? 'Calendar offline' : 'Nothing coming up',
        line: d.error ? '' : `in the next ${d.days} days`,
        accent: false,
      });
    else {
      // the number = when: the start time today, "Now" while it runs, else the day
      const s = calMs(e, 'start'),
        dd = dayDiff(s, now),
        on = isRunning(e, now);
      const when = on
        ? `until ${hm(calMs(e, 'end'))}`
        : dd === 0
          ? 'today'
          : e.allDay
            ? calTime(e)
            : hm(s);
      const cal = d.calendars.find((c) => c.id === e.calendar)?.name;
      hero.set({
        n: on ? 'Now' : dd === 0 && !e.allDay ? hm(s) : dd === 1 ? 'Tmrw' : dd < 7 ? wd(s) : dm(s),
        label: e.summary,
        line: [when, e.location, cal].filter(Boolean).join(' · '),
        accent: on || s - now <= CAL_SOON,
      });
    }

    const groups = agendaDays(d?.events ?? [], now);
    days.replaceChildren(
      ...groups.flatMap(([t, evs]) => {
        const dd = dayDiff(t, now);
        const name =
          dd === 0 ? `today · ${longDay(t)}` : dd === 1 ? `tomorrow · ${longDay(t)}` : longDay(t);
        return [
          h('div', { class: 'fs-sec' }, statusDot('working'), text('label', '', name)),
          h(
            'div',
            { class: 'fs-rows fs-cal-rows' },
            ...evs.map((ev) => agendaRow(ev, ev.calendar ? colour[ev.calendar] : undefined, now)),
          ),
        ];
      }),
    );
    if (d && !d.error && !groups.length)
      days.replaceChildren(text('body', 'fs-empty', 'Nothing on the calendars that are on.'));
  };

  // ---- views: Agenda (hero + days) | Month
  let view: CalendarView = o.view ?? 'agenda';
  const month = monthView({ load: ctx.actions?.loadCalendarMonth, month: o.month });
  const agendaBox = content(at(0, 0, CW, 96, heroEl), at(0, 96 + 48, CW, null, days));
  const monthBox = content(month);
  monthBox.classList.remove('f-scroll'); // the grid fits
  const seg = segmented({
    options: [
      { id: 'agenda', label: 'Agenda' },
      { id: 'month', label: 'Month' },
    ],
    value: view,
    onPick: (v) => setView(v as CalendarView),
  });
  seg.classList.add('fs-cal-seg');
  const setView = (v: CalendarView) => {
    view = v;
    seg.set(v);
    agendaBox.hidden = v !== 'agenda';
    monthBox.hidden = v !== 'month';
    if (v === 'month') month.show();
    else month.close();
  };
  setView(view);

  return {
    el: page(
      'fs-cal',
      header({ label: 'Calendar', onBack: o.onBack, right: fBtn ? [seg, fBtn] : [seg] }),
      agendaBox,
      monthBox,
      sheet,
      month.sheet,
    ),
    update(s) {
      d = s.calendar;
      render();
      month.update(s.calendar, s.calendarMonth);
    },
    tick(now) {
      if (Math.floor(now.getTime() / 60_000) !== lastMin) render(now.getTime());
    },
    leave() {
      sheet.close(true);
      month.close();
    },
  };
}
