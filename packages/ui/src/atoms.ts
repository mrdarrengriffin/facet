// Atoms: the smallest pieces. Labels, the 8 px content-sized box, focus ring, corner ticks, status dot and chip,
// and the plain mascot body used in previews (the animated one with moods is in mascot.ts).
import { size as Z, radius } from '@facet/tokens';
import { h } from './dom.js';
import type { Tone } from './types.js';

export type State = 'default' | 'pressed' | 'active' | 'disabled';
export const STATES: State[] = ['default', 'pressed', 'active', 'disabled'];

/** Mark an element as interactive: `.live` gives it the pressed look on `:active`. Taps are ignored while disabled. */
export function live<T extends HTMLElement>(el: T, onTap?: (e: Event) => void): T {
  el.classList.add('live');
  if (onTap)
    el.addEventListener('click', (e) => {
      if (el.dataset.state !== 'disabled') onTap(e);
    });
  return el;
}

/** Set the visual state of a component (and its children that carry a state). */
export function setState(el: HTMLElement, state: State) {
  el.dataset.state = state;
  el.querySelectorAll<HTMLElement>('[data-state]').forEach((c) => (c.dataset.state = state));
  if (el instanceof HTMLButtonElement) el.disabled = state === 'disabled';
}

// ---------------------------------------------------------------- 8 px grid: content-sized boxes
const G = 8;
const fitRO =
  typeof ResizeObserver === 'undefined'
    ? null
    : new ResizeObserver((entries) => {
        for (const e of entries) {
          const inner = e.target as HTMLElement;
          const box = inner.parentElement;
          if (!box || !inner.offsetWidth) continue;
          box.style.width = `${Math.ceil(inner.offsetWidth / G - 0.001) * G}px`;
        }
      });

/**
 * Content-sized text on the 8 px grid: the box is as wide as the text, rounded up to a multiple of 8, so whatever
 * follows it (gaps, chips, the header's right side) stays on the grid. The text moves into an inner `.f-fit` span;
 * change it with `setText()`.
 */
export function fit8<T extends HTMLElement>(el: T): T {
  const inner = h('span', { class: 'f-fit' }, ...Array.from(el.childNodes));
  el.append(inner);
  el.classList.add('f-fitbox');
  fitRO?.observe(inner);
  return el;
}

/** Set the text of a label, whether or not it's a `fit8` box. */
export function setText(el: Element, text: string) {
  (el.querySelector(':scope > .f-fit') ?? el).textContent = text;
}

/** A text label in one of the component type styles (`ft-<type>`). */
export function label(text: string, type = 'label', cls = '') {
  return h('span', { class: `f-label ft-${type} ${cls}` }, text);
}

/** A 1 px accent ring, `offset` px outside its parent; radius = the parent's radius + offset. */
export function focusRing(offset = Z['focus-offset'], r = radius.button) {
  return h('span', { class: 'f-ring', style: { '--ring-o': `${offset}px`, '--ring-r': `${r}px` } });
}

/** L-shaped corner ticks on the parent's corners: `all` (buttons) or `tr-bl` (cards and tiles). */
export function cornerTicks(
  corners: 'all' | 'tr-bl' = 'all',
  len = Z['btn-tick-len'],
  thick = Z['btn-tick-thick'],
) {
  const list = corners === 'all' ? ['tl', 'tr', 'bl', 'br'] : ['tr', 'bl'];
  return h(
    'span',
    { class: 'f-ticks', style: { '--tick-l': `${len}px`, '--tick-t': `${thick}px` } },
    ...list.map((c) => h('i', { class: c })),
  );
}

/** An 8 px status dot. */
export function statusDot(tone: Tone) {
  return h('span', { class: 'f-dot', 'data-tone': tone });
}

/** A tinted chip with a dot and caps text, e.g. "3 working". */
export function statusChip(tone: Tone, text: string) {
  return h(
    'span',
    { class: 'f-chip ft-caps-sm', 'data-tone': tone },
    statusDot(tone),
    fit8(h('span', { class: 'f-chip-text' }, text)),
  );
}

/** The plain mascot body (32 × 24 with two eyes), in a colour token. For moods and motion use `mascotMood()`. */
export function mascot(colour: string) {
  return h(
    'span',
    { class: 'f-mascot', style: { background: `var(--facet-color-${colour})` } },
    h('i'),
    h('i'),
  );
}
