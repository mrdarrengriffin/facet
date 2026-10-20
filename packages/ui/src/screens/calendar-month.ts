// The Calendar screen's month view (header Agenda | Month). Content 656 × 576: a month bar (title, Today when you're on
// another month, previous / next), the weekday row, then six weeks of 80 × 72 day cells (16 across, 8 down). A cell
// is the day number (today in an accent disc) and up to three calendar dots, or two and "+N". Days of other months
// are mute. Tap a day for a sheet with its events. Moving month calls `actions.loadCalendarMonth(month)`; the host
// answers with `calendarMonth`.
import { h } from '../dom.js';
import { iconButton } from '../buttons.js';
import { at, CW, text } from '../layout.js';
import { bottomSheet } from '../pickers.js';
import type { AgendaEvent, CalendarData, CalendarMonth } from '../types.js';
import { agendaRow, calMs } from './calendar.js';

const DAY = 86_400_000;
const midnight = (t: number) => new Date(t).setHours(0, 0, 0, 0);
const nextDay = (t: number) => midnight(t + DAY + 3_600_000); // DST-safe
/** "2026-10" */
export const monthId = (d: Date) =>
  `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}`;
export const addMonths = (month: string, n: number) => {
  const [y, m] = month.split('-').map(Number);
  return monthId(new Date(y!, m! - 1 + n, 1));
};
/** The grid's first day for a month: the Monday on or before the 1st (local midnight, ms). */
export function monthGridStart(month: string) {
  const [y, m] = month.split('-').map(Number);
  const first = new Date(y!, m! - 1, 1);
  return new Date(y!, m! - 1, 1 - ((first.getDay() + 6) % 7)).getTime();
}

/** Events per local day (midnight ms) from `from` for `days`; a multi-day event counts on each of its days. */
export function eventsByDay(events: AgendaEvent[], from: number, days = 42) {
  const by = new Map<number, AgendaEvent[]>();
  let last = from;
  for (let i = 0; i < days; i++) last = nextDay(last);
  for (const e of events) {
    const end = calMs(e, 'end');
    for (
      let t = Math.max(midnight(calMs(e, 'start')), from), i = 0;
      t < end && t < last && i < 42;
      t = nextDay(t), i++
    ) {
      if (!by.has(t)) by.set(t, []);
      by.get(t)!.push(e);
    }
  }
  return by;
}

export interface MonthCellProps {
  day: number;
  /** One colour token per event that day. */
  colours: string[];
  /** A day of another month. */
  out?: boolean;
  today?: boolean;
  selected?: boolean;
  onTap?: () => void;
}

/** A day cell, 80 × 72: the number in a 40 disc (accent today), then up to three dots, or two and "+N". */
export function monthCell(o: MonthCellProps) {
  const more = o.colours.length > 3 ? o.colours.length - 2 : 0;
  const dots = h(
    'span',
    { class: 'fs-calm-dots' },
    ...o.colours
      .slice(0, more ? 2 : 3)
      .map((c) =>
        h('i', { class: 'fs-cal-dot', style: { background: `var(--facet-color-${c})` } }),
      ),
    more ? text('label', 'fs-calm-more', `+${more}`) : null,
  );
  const cls = ['fs-calm-cell', o.out && 'is-out', o.today && 'is-today', o.selected && 'is-sel']
    .filter(Boolean)
    .join(' ');
  const el = h(
    'div',
    { class: cls, role: o.onTap ? 'button' : undefined, 'data-snd': o.onTap ? 'open' : undefined },
    h('span', { class: 'fs-calm-n' }, text('body', '', String(o.day))),
    dots,
  );
  if (o.onTap) el.addEventListener('click', o.onTap);
  return el;
}

export function monthView(o: { load?: (month: string) => void; month?: string }) {
  let month = o.month ?? monthId(new Date()),
    sel: number | null = null;
  let agenda: CalendarData | null | undefined, got: CalendarMonth | null | undefined;
  const title = text('title', 'fs-calm-title');
  const todayBtn = h('button', { class: 'fst-label fs-cal-fbtn', type: 'button' }, 'today');
  todayBtn.addEventListener('click', () => go(monthId(new Date())));
  const prev = iconButton({
    style: 'square',
    icon: 'chevron',
    title: 'Previous month',
    onTap: () => go(addMonths(month, -1)),
  });
  prev.classList.add('fs-calm-prev');
  const next = iconButton({
    style: 'square',
    icon: 'chevron',
    title: 'Next month',
    onTap: () => go(addMonths(month, 1)),
  });
  const bar = h(
    'div',
    { class: 'fs-calm-bar' },
    title,
    h('span', { class: 'f-grow' }),
    todayBtn,
    prev,
    next,
  );
  const wk = h(
    'div',
    { class: 'fs-calm-wk' },
    ...['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'].map((d) => text('label', '', d)),
  );
  const grid = h('div', { class: 'fs-calm-grid' });
  const sheet = bottomSheet({
    title: 'day',
    onClose: () => {
      sel = null;
      render();
    },
  });

  /** The data for the shown month: `calendarMonth` when it's this month, else whatever `calendar` covers. */
  const source = () => (got?.month === month ? got : o.load ? null : agenda);
  function go(m: string) {
    month = m;
    sel = null;
    o.load?.(m);
    render();
  }

  function render() {
    const [y, m] = month.split('-').map(Number);
    title.textContent = new Date(y!, m! - 1, 1).toLocaleDateString('en-GB', {
      month: 'long',
      year: 'numeric',
    });
    todayBtn.hidden = month === monthId(new Date());
    const d = source(),
      from = monthGridStart(month),
      today = midnight(Date.now());
    const colour = Object.fromEntries(
      (d?.calendars ?? []).map((c) => [c.id, c.colour ?? 'text-dim']),
    );
    const by = eventsByDay(d?.events ?? [], from);
    const cells: HTMLElement[] = [];
    for (let i = 0, t = from; i < 42; i++, t = nextDay(t)) {
      const day = new Date(t),
        evs = by.get(t) ?? [];
      cells.push(
        monthCell({
          day: day.getDate(),
          colours: evs.map((e) => (e.calendar && colour[e.calendar]) || 'text-dim'),
          out: day.getMonth() !== m! - 1,
          today: t === today,
          selected: t === sel,
          onTap: () => {
            sel = t;
            render();
            sheet.open();
          },
        }),
      );
    }
    grid.replaceChildren(...cells);
    grid.dataset.state = d ? 'ok' : 'loading';
    if (sel !== null) {
      // the day sheet follows the data: "Loading" until the month arrives
      const evs = by.get(sel) ?? [];
      sheet.setTitle(
        new Date(sel).toLocaleDateString('en-GB', {
          weekday: 'long',
          day: 'numeric',
          month: 'long',
        }),
      );
      sheet.list.replaceChildren(
        ...(evs.length
          ? evs.map((e) => agendaRow(e, e.calendar ? colour[e.calendar] : undefined))
          : [text('body', 'fs-empty fs-calm-none', d ? 'Nothing on this day.' : 'Loading')]),
      );
    }
  }

  const el = h(
    'div',
    { class: 'fs-calm' },
    at(0, 0, CW, 48, bar),
    at(0, 64, CW, 24, wk),
    at(0, 104, CW, 472, grid),
  );
  return Object.assign(el, {
    /** Put it on the page, over the header (like the screen's other sheets). */
    sheet,
    update(a: CalendarData | null | undefined, m: CalendarMonth | null | undefined) {
      agenda = a;
      got = m;
      render();
    },
    show() {
      go(month);
    },
    close() {
      sheet.close(true);
    },
  });
}
