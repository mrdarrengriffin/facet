// The clock family. Atoms: fixed-width digits with an optically raised colon, time text, date line, a seconds dot
// orbiting a ring, a seconds strip, analogue hands and a context slot. Faces (720 × 720): Hero, Ring, Analogue and
// Clock + context. Props: font 'mono' (default) or 'inter', format '24h' (default) or '12h', date 'short' or 'long'.
//
// Live atoms carry `data-clock` and a `tick(now)` method; call `tickClocks(root)` once a second (see startTicker).
// Seconds move with one rotated or scaled small element per clock (60 s linear, re-synced to the wall clock on every
// tick); on a panel that's an animation of the dot's angle or the bar's value.
import { motion, size as Z, type as TYPE } from '@facet/tokens';
import { statusDot } from './atoms.js';
import { tickRing } from './controls.js';
import { h } from './dom.js';
import type { CalendarEvent, Timer, Tone, WeatherData } from './types.js';
import { WX_TEXT } from '@facet/icons';

export type ClockFont = 'inter' | 'mono';
export type ClockFormat = '24h' | '12h';
export type DateStyle = 'short' | 'long';
export type ClockProps = { font: ClockFont; format: ClockFormat; date: DateStyle };
type Live = HTMLElement & { tick(now: Date): void };

const D = motion.duration;
const T = TYPE as Record<string, { size: number; digit?: number; colon?: number; colonY?: number }>;
const pad = (n: number) => String(n).padStart(2, '0');

function live<E extends HTMLElement>(el: E, tick: (now: Date) => void): E & Live {
  el.dataset.clock = '';
  return Object.assign(el, { tick });
}

/** Calls tick(now) on every live clock atom under `root`. Run it once a second (startTicker). */
export function tickClocks(root: ParentNode = document, now = new Date()) {
  root.querySelectorAll<HTMLElement>('[data-clock]').forEach((e) => (e as any).tick?.(now));
}

export function timeParts(now: Date, format: ClockFormat) {
  const hr = now.getHours(),
    m = pad(now.getMinutes());
  if (format === '24h') return { hh: pad(hr), mm: m, ampm: '' };
  return { hh: String(hr % 12 || 12), mm: m, ampm: hr < 12 ? 'AM' : 'PM' };
}
export const fmtTime = (d: Date, format: ClockFormat) => {
  const p = timeParts(d, format);
  return `${p.hh}:${p.mm}${p.ampm ? ` ${p.ampm}` : ''}`;
};

// ---------------------------------------------------------------- atoms
/** Digit string ("14:52", "4:07:30") in fixed-width cells (tabular on LVGL too: one label per cell), colon raised by the
 *  type token's colonY so it sits centred on the figures (Inter's colon rests on the baseline). set(text) updates. */
export function digits(type: string, text = '') {
  const t = T[type];
  const el = h('span', {
    class: `f-digits ft-${type}`,
    style: {
      '--dg': `${t.digit ?? 0.6}em`,
      '--colon': `${t.colon ?? 0.3}em`,
      '--colon-y': `${t.colonY ?? 0}em`,
    } as any,
  });
  let cur = '';
  const set = (s: string) => {
    if (s === cur) return;
    if (s.length !== cur.length || [...s].some((c, i) => (c === ':') !== (cur[i] === ':'))) {
      el.replaceChildren(
        ...[...s].map((c) => h('span', { class: c === ':' ? 'f-colon' : 'f-dg' }, c)),
      );
    } else {
      [...s].forEach((c, i) => {
        if (c !== cur[i]) el.children[i].textContent = c;
      });
    }
    cur = s;
  };
  set(text);
  return Object.assign(el, { set });
}

/** Time of day: digits + (12h) small AM/PM on the baseline. size hero 180 | mid 128 | compact (header). */
export function timeText(o: {
  size: 'hero' | 'mid' | 'compact';
  font: ClockFont;
  format: ClockFormat;
}) {
  const mono = o.font === 'mono' ? '-mono' : '';
  const dg = digits(`clock-${o.size}${mono}`);
  const ampmType = o.size === 'compact' ? 'clock-ampm-sm' : `clock-ampm${mono}`;
  const ap = o.format === '12h' ? h('span', { class: `f-ampm ft-${ampmType}` }) : null;
  const el = h('span', { class: `f-time f-time--${o.size}` }, dg, ap);
  return live(el, (now) => {
    const p = timeParts(now, o.format);
    dg.set(`${p.hh}:${p.mm}`);
    if (ap && ap.textContent !== p.ampm) ap.textContent = p.ampm;
  });
}

/** Date line: short = MON 19 OCT (JetBrains Mono caps, tracked) · long = Monday 19 October (Inter light). */
export function dateText(now: Date, style: DateStyle) {
  return style === 'short'
    ? now
        .toLocaleDateString('en-GB', { weekday: 'short', day: 'numeric', month: 'short' })
        .replace(',', '')
    : now
        .toLocaleDateString('en-GB', { weekday: 'long', day: 'numeric', month: 'long' })
        .replace(',', '');
}
export function dateLine(o: { style: DateStyle; tone?: string }) {
  const el = h('span', {
    class: `f-dateline ft-${o.style === 'short' ? 'date' : 'date-long'}`,
    style: { color: `var(--facet-color-${o.tone ?? 'text-dim'})` },
  });
  return live(el, (now) => {
    const s = dateText(now, o.style);
    if (el.textContent !== s) el.textContent = s;
  });
}

/** One full turn per minute, synced to the wall clock (seconds + ms). Re-sync on every tick if it drifted > 40 ms. */
function sweep(el: HTMLElement) {
  const dur = D['clock-sweep'];
  const a = el.animate([{ transform: 'rotate(0deg)' }, { transform: 'rotate(360deg)' }], {
    duration: dur,
    iterations: Infinity,
    easing: 'linear',
  });
  return () => {
    const n = new Date(),
      want = n.getSeconds() * 1000 + n.getMilliseconds();
    const cur = Number(a.currentTime ?? 0) % dur;
    const off = Math.abs(cur - want);
    if (off > 40 && off < dur - 40) a.currentTime = want;
  };
}

/** Seconds dot orbiting at radius r around its parent's centre (= rotated 0x0 container, only the dot repaints). */
export function orbitDot(o: { r: number; size?: number; colour?: string }) {
  const size = o.size ?? Z['clock-orbit-dot'];
  const arm = h(
    'i',
    { class: 'f-orbit' },
    h('i', {
      class: 'f-orbit-dot',
      style: {
        left: `${-size / 2}px`,
        top: `${-o.r - size / 2}px`,
        width: `${size}px`,
        height: `${size}px`,
        background: `var(--facet-color-${o.colour ?? 'accent'})`,
      },
    }),
  );
  const sync = sweep(arm);
  return live(arm, sync);
}

/** Seconds progress strip: 2 px track + accent indicator scaled 0..1 over each minute (transform on a thin strip). */
export function secondsBar(width: number) {
  const ind = h('i', { class: 'f-secbar-ind' });
  const el = h('span', { class: 'f-secbar', style: { width: `${width}px` } }, ind);
  const dur = D['clock-sweep'];
  const a = ind.animate([{ transform: 'scaleX(0)' }, { transform: 'scaleX(1)' }], {
    duration: dur,
    iterations: Infinity,
    easing: 'linear',
  });
  return live(el, () => {
    const n = new Date(),
      want = n.getSeconds() * 1000 + n.getMilliseconds();
    const off = Math.abs((Number(a.currentTime ?? 0) % dur) - want);
    if (off > 40 && off < dur - 40) a.currentTime = want;
  });
}

/** Analogue hands around the parent's centre: hour + minute (thin, text colour, short ease-out step), seconds (accent,
 *  tail + tip dot, smooth sweep), centre cap. */
export function hands() {
  const bar = (len: number, w: number, c: string, tail = 0) =>
    h('i', {
      class: 'f-hand-bar',
      style: {
        left: `${-w / 2}px`,
        top: `${-len}px`,
        width: `${w}px`,
        height: `${len + tail}px`,
        background: `var(--facet-color-${c})`,
      },
    });
  const hr = h('i', { class: 'f-hand f-hand--step' }, bar(Z['clock-hand-hour'], 4, 'text'));
  const mn = h('i', { class: 'f-hand f-hand--step' }, bar(Z['clock-hand-min'], 2, 'text'));
  const sec = h(
    'i',
    { class: 'f-hand' },
    bar(Z['clock-hand-sec'], 2, 'accent', Z['clock-hand-tail']),
    h('i', { class: 'f-hand-tip', style: { top: `${-Z['clock-hand-sec'] - 5}px` } }),
  );
  const cap = h('i', { class: 'f-hand-cap' });
  const el = h('span', { class: 'f-hands' }, hr, mn, sec, cap);
  const sync = sweep(sec);
  // cumulative angles so 59 -> 0 keeps turning clockwise
  const acc = new Map<HTMLElement, number>();
  const turn = (e: HTMLElement, deg: number) => {
    let last = acc.get(e);
    if (last !== undefined) {
      while (deg < last - 180) deg += 360;
    }
    acc.set(e, deg);
    e.style.transform = `rotate(${deg}deg)`;
  };
  return live(el, (now) => {
    const m = now.getMinutes() + now.getSeconds() / 60;
    turn(mn, m * 6);
    turn(hr, ((now.getHours() % 12) + m / 60) * 30);
    sync();
  });
}

/** Context slot row (Clock + context): dot · caps label over value · meta right · progress strip. */
export function contextSlot(o: { label: string; tone: Tone; value?: Node }) {
  const value = h('span', { class: 'f-slot-value ft-title' });
  if (o.value) value.append(o.value);
  const meta = h('span', { class: 'f-slot-meta ft-meta' });
  const bar = h('i', { class: 'f-slot-bar' });
  const dot = statusDot(o.tone);
  const el = h(
    'div',
    { class: 'f-slot' },
    dot,
    h(
      'div',
      { class: 'f-slot-main' },
      h('span', { class: 'f-slot-label ft-caps-sm' }, o.label),
      value,
    ),
    h('span', { class: 'f-grow' }),
    meta,
    bar,
  );
  return Object.assign(el, {
    value,
    set(st: { value?: string; meta?: string; progress?: number | null; tone?: Tone }) {
      if (st.value !== undefined && !o.value && value.textContent !== st.value)
        value.textContent = st.value;
      if (st.meta !== undefined && meta.textContent !== st.meta) meta.textContent = st.meta;
      if (st.tone) dot.dataset.tone = st.tone;
      bar.hidden = st.progress === null || st.progress === undefined;
      if (!bar.hidden) bar.style.transform = `scaleX(${Math.max(0, Math.min(1, st.progress!))})`;
    },
  });
}

// ---------------------------------------------------------------- composites (720x720 screens)
const screen = (...kids: (Node | null)[]) => h('div', { class: 'f-screen f-clockscreen' }, ...kids);
const at = (left: number, top: number, w: number, ...kids: (Node | null)[]) =>
  h(
    'div',
    { class: 'f-abs f-center', style: { left: `${left}px`, top: `${top}px`, width: `${w}px` } },
    ...kids,
  );

/** Clock / Hero: 180 px digits (~70 % of the width) + date line + seconds strip. */
export function clockHero(p: ClockProps) {
  return screen(
    at(0, 238, 720, timeText({ size: 'hero', font: p.font, format: p.format })),
    at(0, 446, 720, dateLine({ style: p.date })),
    at(0, 500, 720, secondsBar(64)),
  );
}

/** Clock / Ring: 60-dot ring (600) + 12 hour ticks (560) + orange seconds dot orbiting on the dots; time + date centred. */
export function clockRing(p: ClockProps) {
  const R = Z['clock-ring'],
    RI = Z['clock-ring-inner'];
  const dots = tickRing({
    size: R,
    count: 60,
    range: 354,
    rotation: 270,
    minor: { w: 3, len: 3, c: 'line-hi' },
  });
  const ticks = tickRing({
    size: RI,
    count: 12,
    range: 330,
    rotation: 270,
    minor: { w: 2, len: 16, c: 'text-mute' },
    major: { stride: 3, w: 2, len: 24, c: 'text-dim' },
  });
  return screen(
    h(
      'div',
      { class: 'f-abs', style: { left: `${(720 - R) / 2}px`, top: `${(720 - R) / 2}px` } },
      dots,
    ),
    h(
      'div',
      { class: 'f-abs', style: { left: `${(720 - RI) / 2}px`, top: `${(720 - RI) / 2}px` } },
      ticks,
    ),
    h(
      'div',
      { class: 'f-abs', style: { left: '360px', top: '360px' } },
      orbitDot({ r: R / 2 - 1.5 }),
    ),
    at(0, 296, 720, timeText({ size: 'mid', font: p.font, format: p.format })),
    at(0, 448, 720, dateLine({ style: p.date })),
  );
}

/** Clock / Analogue: thin 60-tick scale (like the dial), numerals 12 3 6 9 in the chosen font, thin hands,
 *  orange seconds hand + tip dot, date window under the centre. */
export function clockAnalogue(p: Omit<ClockProps, 'format'>) {
  const S = Z['clock-dial'],
    c = 360,
    off = (720 - S) / 2;
  const scale = tickRing({
    size: S,
    count: 60,
    range: 354,
    rotation: 270,
    minor: { w: 1, len: 10, c: 'line-hi' },
    major: { stride: 5, w: 2, len: 22, c: 'text-dim' },
  });
  const nr = S / 2 - 22 - 34;
  const num = (txt: string, deg: number) => {
    const a = (deg * Math.PI) / 180;
    return h(
      'span',
      {
        class: `f-abs f-numeral ft-clock-numeral${p.font === 'mono' ? '-mono' : ''}`,
        style: { left: `${c + nr * Math.cos(a)}px`, top: `${c + nr * Math.sin(a)}px` },
      },
      txt,
    );
  };
  return screen(
    h('div', { class: 'f-abs', style: { left: `${off}px`, top: `${off}px` } }, scale),
    num('12', 270),
    num('3', 0),
    num('6', 90),
    num('9', 180),
    at(0, 466, 720, dateLine({ style: p.date })),
    h('div', { class: 'f-abs', style: { left: `${c}px`, top: `${c}px` } }, hands()),
  );
}

/** Compact time for a header. */
export function clockCompact(o: { font?: ClockFont; format?: ClockFormat } = {}) {
  return timeText({ size: 'compact', font: o.font ?? 'mono', format: o.format ?? '24h' });
}

// ---- Clock + context
const relTime = (ms: number) => {
  const m = Math.round(ms / 60000);
  if (m < 1) return 'now';
  if (m < 60) return `in ${m} min`;
  const hh = Math.floor(m / 60),
    mm = m % 60;
  return hh < 24 ? `in ${hh} h${mm ? ` ${mm} m` : ''}` : '';
};
function eventMeta(ev: CalendarEvent, now: Date, format: ClockFormat) {
  const st = new Date(ev.allDay ? `${ev.start}T00:00:00` : ev.start),
    en = new Date(ev.allDay ? `${ev.end}T00:00:00` : ev.end);
  const day0 = new Date(now.getFullYear(), now.getMonth(), now.getDate()).getTime();
  const dd = Math.round(
    (new Date(st.getFullYear(), st.getMonth(), st.getDate()).getTime() - day0) / 86400000,
  );
  const day =
    dd <= 0
      ? 'today'
      : dd === 1
        ? 'tomorrow'
        : st.toLocaleDateString('en-GB', { weekday: 'short', day: 'numeric' });
  if (st.getTime() <= now.getTime())
    return ev.allDay ? 'today · all day' : `now · until ${fmtTime(en, format).toLowerCase()}`;
  if (ev.allDay) return `${day} · all day`;
  const rel = relTime(st.getTime() - now.getTime());
  return `${day} ${fmtTime(st, format).toLowerCase()}${rel ? ` · ${rel}` : ''}`;
}
/** "4:07" or "1:04:07". */
export const countdown = (s: number) => {
  s = Math.max(0, Math.ceil(s));
  const hh = Math.floor(s / 3600),
    mm = Math.floor((s % 3600) / 60),
    ss = s % 60;
  return hh ? `${hh}:${pad(mm)}:${pad(ss)}` : `${mm}:${pad(ss)}`;
};

/** What the context face shows: read on every tick. */
export interface ClockContextData {
  weather?: WeatherData | null;
  nextEvent?: CalendarEvent | null;
  timers?: Timer[];
}

/**
 * Clock + context: a mid-size clock and the date, then three rows: outside (weather), next (calendar) and a running
 * timer with a progress strip. Empty rows say so quietly.
 */
export function clockContext(p: ClockProps, get: () => ClockContextData) {
  const wx = contextSlot({ label: 'Outside', tone: 'info' });
  const ev = contextSlot({ label: 'Next', tone: 'ok' });
  const cd = digits(`clock-compact${p.font === 'mono' ? '-mono' : ''}`, '0:00');
  const tm = contextSlot({ label: 'Timer', tone: 'working', value: cd });
  const slots = h(
    'div',
    { class: 'f-abs f-slots', style: { left: '16px', top: '316px', width: '688px' } },
    wx,
    ev,
    tm,
  );
  const el = screen(
    at(0, 92, 720, timeText({ size: 'mid', font: p.font, format: p.format })),
    at(0, 236, 720, dateLine({ style: p.date })),
    slots,
  );
  const ticker = live(h('i', { hidden: true }), (now) => {
    const s = get();
    const c = s.weather?.current;
    if (c) {
      const cond = WX_TEXT[c.condition] ?? c.condition;
      wx.set({
        value: `${c.temp !== null ? `${Math.round(c.temp)}°  ` : ''}${cond}`,
        meta: c.feels !== null ? `feels ${Math.round(c.feels)}°` : '',
        progress: null,
      });
    } else wx.set({ value: '--', meta: '', progress: null });
    const e = s.nextEvent;
    if (e) ev.set({ value: e.summary, meta: eventMeta(e, now, p.format), progress: null });
    else ev.set({ value: 'Nothing coming up', meta: '', progress: null });
    const t = s.timers?.[0];
    if (t) {
      const left = (t.endsAt - now.getTime()) / 1000;
      cd.set(countdown(left));
      tm.set({
        meta: `${t.label.toLowerCase()} · of ${Math.round(t.total / 60)} min${s.timers!.length > 1 ? ` · +${s.timers!.length - 1}` : ''}`,
        progress: 1 - left / t.total,
      });
    } else {
      cd.set('0:00');
      tm.set({ meta: 'no timer running', progress: null });
    }
  });
  el.append(ticker);
  return el;
}

/** The clock faces, for pickers and docs. */
export const CLOCK_FACES = [
  {
    id: 'orbit',
    name: 'Orbit',
    what: 'The Clock screen: a tick ring with three dots orbiting at different speeds, the time, date and weather.',
  },
  {
    id: 'hero',
    name: 'Hero',
    what: 'Big digits across most of the width, the date and a seconds strip.',
  },
  {
    id: 'ring',
    name: 'Ring',
    what: 'A dotted ring with hour ticks; an orange dot orbits as the seconds hand.',
  },
  {
    id: 'analogue',
    name: 'Analogue',
    what: 'Thin hands and ticks like the dial, with an orange seconds hand.',
  },
  {
    id: 'context',
    name: 'Clock + context',
    what: 'A smaller clock with weather, the next calendar event and a running timer.',
  },
] as const;
export type ClockFace = (typeof CLOCK_FACES)[number]['id'];
