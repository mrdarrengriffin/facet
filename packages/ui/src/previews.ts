// The 72 px artwork on Home cards. Plain shapes only (circles, rings, arcs), on the 72 box.
import { mascot } from './atoms.js';
import { barChart, BAND_COLOUR } from './charts.js';
import { arc } from './controls.js';
import { h } from './dom.js';
import type { EnergyData } from './types.js';

export { previewLightning } from './radar.js';

const P = (style: Record<string, string>, cls = 'f-abs') => h('i', { class: cls, style });
const circle = (x: number, y: number, d: number, extra: Record<string, string>) =>
  P({
    left: `${x}px`,
    top: `${y}px`,
    width: `${d}px`,
    height: `${d}px`,
    borderRadius: '999px',
    ...extra,
  });
const c = (name: string) => `var(--facet-color-${name})`;

/** Four mascots. */
export function previewAgents() {
  const place = (m: HTMLElement, x: number, y: number) => {
    m.classList.add('f-abs');
    m.style.left = `${x}px`;
    m.style.top = `${y}px`;
    return m;
  };
  return h(
    'span',
    null,
    place(mascot('accent'), 0, 8),
    place(mascot('steel'), 40, 8),
    place(mascot('warn'), 0, 40),
    place(mascot('accent'), 40, 40),
  );
}

/** A small record with its label spot and marker. */
export function previewMedia() {
  return h(
    'span',
    null,
    circle(2, 2, 68, { background: c('disc'), border: `1px solid ${c('line-hi')}` }),
    circle(14, 14, 44, { border: `1px solid ${c('groove')}` }),
    circle(25, 25, 22, { background: c('accent') }),
    circle(34, 34, 4, { background: c('bg') }),
    circle(56, 7, 8, { background: c('blue') }),
  );
}

/** A small dial. `set(pct)`. */
export function previewDial(v = 0) {
  const a = arc({
    size: 68,
    start: 135,
    end: 45,
    track: { w: 2, c: 'line' },
    ind: { w: 5, c: 'accent' },
    value: v,
  });
  a.classList.add('f-abs');
  a.style.left = '2px';
  a.style.top = '2px';
  return Object.assign(h('span', null, a, circle(32, 32, 8, { background: c('blue') })), {
    set: (x: number) => a.set(x),
  });
}

/** A toggle: a pill with its knob, filled accent while on. `set(on)`; null = unavailable (dim, no knob). */
export function previewSwitch(on: boolean | null = null) {
  const pill = P({
    left: '2px',
    top: '18px',
    width: '68px',
    height: '36px',
    borderRadius: '999px',
    boxSizing: 'border-box',
  });
  const knob = circle(8, 24, 24, { transition: 'transform 150ms ease-out' });
  const el = Object.assign(h('span', null, pill, knob), {
    set(v: boolean | null) {
      pill.style.background = v ? c('accent') : 'transparent';
      pill.style.border = `2px solid ${v ? c('accent') : v === false ? c('line-hi') : c('line')}`;
      knob.style.background = v ? c('on-accent') : c('steel');
      knob.style.transform = v ? 'translateX(32px)' : 'none';
      knob.style.visibility = v === null ? 'hidden' : 'visible';
    },
  });
  el.set(on);
  return el;
}

/** Rings with orbiting dots. */
export function previewClock() {
  return h(
    'span',
    null,
    circle(2, 2, 68, { border: `1px solid ${c('line-hi')}` }),
    circle(13, 13, 46, { border: `1px solid ${c('groove')}` }),
    circle(29, -5, 14, { background: c('accent') }),
    circle(53, 41, 8, { background: c('blue') }),
    circle(19, 52, 6, { background: c('steel') }),
  );
}

/** A ring that empties as the timer runs, with a dot at its centre. `set(pct left)`. */
export function previewTimer(v = 100) {
  const a = arc({
    size: 68,
    start: 270,
    end: 269,
    track: { w: 2, c: 'line' },
    ind: { w: 5, c: 'accent' },
    value: v,
  });
  a.classList.add('f-abs');
  a.style.left = '2px';
  a.style.top = '2px';
  return Object.assign(h('span', null, a, circle(32, 32, 8, { background: c('blue') })), {
    set: (x: number) => a.set(x),
  });
}

/** Two commits and a branch merging into an orange dot. */
export function previewGithub() {
  return h(
    'span',
    null,
    circle(4, 4, 20, { border: `2px solid ${c('steel')}` }),
    circle(4, 48, 20, { border: `2px solid ${c('steel')}` }),
    P({ left: '13px', top: '24px', width: '2px', height: '24px', background: c('steel') }),
    P({
      left: '24px',
      top: '13px',
      width: '34px',
      height: '35px',
      borderTop: `2px solid ${c('blue')}`,
      borderRight: `2px solid ${c('blue')}`,
      borderTopRightRadius: '20px',
    }),
    circle(48, 48, 20, { background: c('accent') }),
  );
}

/** The last 12 half-hours as bars coloured by price band. `set(energy)`. */
export function previewEnergy() {
  const ch = barChart({ w: 72, h: 64, bars: [], gap: 2, radius: 1, baseline: true });
  const el = h('span', null, h('i', { class: 'f-abs', style: { left: '0', top: '8px' } }, ch));
  return Object.assign(el, {
    set(d: EnergyData | null | undefined) {
      const sl = d?.today?.slots ?? [];
      const ni = nowIndex(sl);
      const past = sl
        .slice(0, ni < 0 ? sl.length : ni)
        .filter((s) => s.kwh !== null)
        .slice(-12);
      ch.set(past.map((s) => ({ v: s.kwh, c: BAND_COLOUR[s.band] })));
    },
  });
}

/** Index of the half-hour slot that contains `now`, or -1. */
export function nowIndex(slots: { start: string }[], now = Date.now()) {
  return slots.findIndex((s, i) => {
    const a = Date.parse(s.start);
    const b = slots[i + 1] ? Date.parse(slots[i + 1]!.start) : a + 1800_000;
    return now >= a && now < b;
  });
}
