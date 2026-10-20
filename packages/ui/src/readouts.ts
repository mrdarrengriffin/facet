// State readouts: Home items that report a state and open no screen (someone in the room, a door open, one
// temperature, who's home). The rule: a box is a button, no box is a readout. Cards are filled and pressable; a
// readout sits on the screen background with no fill, no press state, no sound and no chevron, and a tap does nothing.
// Two share one card slot (320 × 280): 136 + 8 + 136, one hairline between them.
import { icon } from '@facet/icons';
import { statusDot } from './atoms.js';
import { h } from './dom.js';
import { text } from './layout.js';
import type { ReadoutSource } from './types.js';

export type ReadoutTone = 'text' | 'accent' | 'warn' | 'mute';
export interface Readout {
  /** UI icon (24), e.g. `door`, `person`, `thermometer`, `house`. */
  icon: string;
  /** Caps label, e.g. "Back door". */
  label: string;
  /** The state, e.g. "Open", "21.4°". */
  state: string;
  /** How long or a detail, dim, e.g. "for 12 min", "Alex". */
  detail?: string;
  /** text (default) · accent = it matters (a dot too) · warn = out of range · mute = offline. */
  tone?: ReadoutTone;
}

/** One readout, 320 × 136: icon + caps label (+ dot when it matters), then the state (title) + detail (body, dim). */
export function readout(r: Readout) {
  const ic = h('span', { class: 'fs-ro-icon' }, icon(r.icon, 24));
  const lbl = text('label', 'fs-ro-label', r.label);
  const st = text('title', 'fs-ro-state', r.state);
  const det = text('body', 'fs-ro-detail', r.detail ?? '');
  const el = h(
    'div',
    { class: 'fs-ro' },
    h('div', { class: 'fs-ro-top' }, ic, lbl, statusDot('working')),
    h('div', { class: 'fs-ro-line' }, st, det),
  );
  const api = Object.assign(el, {
    set(s: Partial<Readout>) {
      if (s.icon !== undefined) ic.replaceChildren(icon(s.icon, 24));
      if (s.label !== undefined) lbl.textContent = s.label;
      if (s.state !== undefined) st.textContent = s.state;
      if (s.detail !== undefined) det.textContent = s.detail;
      if (s.tone !== undefined) el.dataset.tone = s.tone;
    },
  });
  api.set({ tone: r.tone ?? 'text' });
  return api;
}

/** One Home slot (a card's place) holding up to two readouts. */
export function readoutSlot(items: Readout[]) {
  const ros = items.slice(0, 2).map(readout);
  const el = h('div', { class: 'fs-roslot' }, ...ros);
  return Object.assign(el, { items: ros });
}

/** "just now" · "for 12 min" · "for 3 h" · "since Mon" */
export function sinceText(ms: number | undefined, now = Date.now()) {
  if (ms === undefined) return '';
  const m = Math.floor((now - ms) / 60_000);
  if (m < 1) return 'just now';
  if (m < 60) return `for ${m} min`;
  if (m < 24 * 60) return `for ${Math.floor(m / 60)} h`;
  return `since ${new Date(ms).toLocaleDateString('en-GB', { weekday: 'short' })}`;
}

const BINARY: Record<string, { icon: string; on: string; off: string; onTone?: ReadoutTone }> = {
  occupancy: { icon: 'person', on: 'Occupied', off: 'Clear' },
  presence: { icon: 'person', on: 'Someone in', off: 'Empty' },
  motion: { icon: 'person', on: 'Motion', off: 'Still' },
  door: { icon: 'door', on: 'Open', off: 'Closed', onTone: 'accent' },
  garage_door: { icon: 'door', on: 'Open', off: 'Closed', onTone: 'accent' },
  opening: { icon: 'door', on: 'Open', off: 'Closed', onTone: 'accent' },
  window: { icon: 'window', on: 'Open', off: 'Closed', onTone: 'accent' },
  moisture: { icon: 'alert', on: 'Wet', off: 'Dry', onTone: 'warn' },
  smoke: { icon: 'alert', on: 'Smoke', off: 'Clear', onTone: 'warn' },
};

/** A readout from its source: the wording per device class, how long, and the tone. */
export function readoutFrom(src: ReadoutSource, now = Date.now()): Readout {
  if (src.kind === 'binary') {
    const k = BINARY[src.deviceClass] ?? BINARY.occupancy!;
    const detail = sinceText(src.since, now);
    if (src.on === null)
      return { icon: k.icon, label: src.label, state: 'Offline', detail, tone: 'mute' };
    return {
      icon: k.icon,
      label: src.label,
      state: src.on ? k.on : k.off,
      detail,
      tone: src.on ? (k.onTone ?? 'text') : 'text',
    };
  }
  if (src.kind === 'value') {
    const ic = src.icon ?? 'thermometer';
    const v = src.value;
    if (v === null)
      return {
        icon: ic,
        label: src.label,
        state: 'Offline',
        detail: sinceText(src.since, now),
        tone: 'mute',
      };
    const unit = src.unit ?? '°C';
    const low = src.lo !== undefined && v < src.lo,
      high = src.hi !== undefined && v > src.hi;
    return {
      icon: ic,
      label: src.label,
      state: `${v.toFixed(1)}${unit.startsWith('°') ? '°' : ` ${unit}`}`,
      detail: low ? 'too cold' : high ? 'too warm' : '',
      tone: low || high ? 'warn' : 'text',
    };
  }
  const label = src.label ?? 'Home';
  const home = src.people.filter((p) => p.home);
  if (!home.length) {
    const left = Math.max(...src.people.map((p) => p.since ?? 0));
    return {
      icon: 'house',
      label,
      state: 'Nobody home',
      detail: left ? sinceText(left, now) : '',
      tone: 'text',
    };
  }
  const all = home.length === src.people.length;
  return {
    icon: 'house',
    label,
    state: all ? 'Everyone in' : `${home.length} home`,
    detail: all ? '' : home.map((p) => p.name.split(' ')[0]).join(', '),
    tone: 'text',
  };
}
