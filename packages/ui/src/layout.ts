// Screen building blocks, following the design principles: one hero per screen in a large light weight, everything
// else quiet; inset 32 on every side; rows separated by hairlines rather than boxes; the accent only for the one thing
// that matters. Geometry comes from `layout` in @facet/tokens, type from the screen scale (`.fst-*`).
import { icon, screenIcon } from '@facet/icons';
import { addScrollTracks } from './scrolltrack.js';
import { layout as G } from '@facet/tokens';
import { fit8, live, setText, statusDot } from './atoms.js';
import { backButton } from './buttons.js';
import { h } from './dom.js';
import { mascotMood, moodFor } from './mascot.js';
import type { Agent, Tone } from './types.js';

export const SCREEN = 720;
export { G as layout };
/** Content box: x 32..688, y 112..688 (656 × 576). */
export const CW = G.contentW,
  CH = G.contentH;

/** Text in a screen type role: `text('hero', 'my-class', '42')`. */
export const text = (role: string, cls: string, t = '') =>
  h('span', { class: `fst-${role} ${cls}` }, t);

/** An absolutely positioned box. */
export const at = (
  x: number,
  y: number,
  w: number | null,
  hh: number | null,
  ...kids: (Node | null)[]
) =>
  h(
    'div',
    {
      class: 'f-abs',
      style: {
        left: `${x}px`,
        top: `${y}px`,
        ...(w !== null ? { width: `${w}px` } : {}),
        ...(hh !== null ? { height: `${hh}px` } : {}),
      },
    },
    ...kids,
  );

/** The scrollable content box under the header. Children are positioned from its top-left. */
export const content = (...kids: (Node | null)[]) =>
  h(
    'div',
    {
      class: 'f-abs f-content f-scroll fs-content',
      // from the tokens' CSS vars, so the same screen fits any device (data-facet-device / data-facet-density)
      style: {
        left: 'var(--facet-layout-inset)',
        top: 'var(--facet-layout-top)',
        width: 'calc(var(--facet-screen-w) - 2 * var(--facet-layout-inset))',
        height: 'calc(var(--facet-screen-h) - var(--facet-layout-inset) - var(--facet-layout-top))',
      },
    },
    ...kids,
  );

/** A page (one screen), and a sub-view inside a page. */
export const page = (cls: string, ...kids: (Node | null)[]) => {
  const el = h('div', { class: `f-page fs ${cls}` }, ...kids);
  addScrollTracks(el); // every scrollable area gets a track in the margin
  return el;
};
export const subView = (...kids: (Node | null)[]) => h('div', { class: 'f-sub' }, ...kids);

export interface HeaderProps {
  /** The screen's name, in caps. */
  label: string;
  /** Show a back button. Leave it out on Home, or wherever going back makes no sense (like a docs preview). */
  onBack?: () => void;
  /** Items on the right, before the time. */
  right?: (Node | null)[];
  /** Show the time on the right. Default true. */
  clock?: boolean;
}

/**
 * The screen header: one 48 px row at y 32. Back button, the screen's label, then optional items and the time on the
 * right. The label only names the screen; the hero below says what it's about.
 */
export function header(o: HeaderProps) {
  return h(
    'div',
    { class: 'fs-header' },
    o.onBack ? backButton({ onTap: o.onBack }) : null,
    fit8(text('label', 'fs-hlabel', o.label)),
    h('span', { class: 'f-grow' }),
    ...(o.right ?? []),
    o.clock === false
      ? null
      : h('span', { class: 'fst-label fs-hclock', 'data-clock': '' }, '--:--'),
  );
}

export interface CardProps {
  /** Caps label at the top. */
  label: string;
  /** 72 px artwork, top right. */
  preview: Node;
  /** A screen icon name (24 px) before the label, e.g. `weather`. */
  icon?: string;
  /** One value (value 64), e.g. "18:56", "13°", "65%". */
  value?: string;
  /** Or a short text (title 28, two lines), e.g. a track name. */
  text?: string;
  /** One line under it. */
  sub?: string;
  onTap?: () => void;
}

/**
 * A Home card, 320 × 280: a screen icon and label (with a dot when it matters), the 72 px preview top right, one value or a
 * two-line text, and one line under it. `set({ value | text, sub, accent, tone })`.
 */
export function card(o: CardProps) {
  const dot = statusDot('working');
  const value = text('value', 'fs-cval', o.value ?? '');
  const textIn = text('title', '', o.text ?? '');
  const textBox = h('div', { class: 'fs-ctext' }, textIn);
  const sub = text('body', 'fs-csub', o.sub ?? '');
  const el = h(
    'div',
    { class: 'fs-card', role: 'button', tabindex: 0 },
    h(
      'div',
      { class: 'fs-clabel' },
      o.icon ? screenIcon(o.icon, 24, 'fs-cicon') : null,
      fit8(text('label', '', o.label)),
      dot,
    ),
    h('span', { class: 'fs-cprev' }, o.preview),
    value,
    textBox,
    sub,
  );
  live(el, () => o.onTap?.());
  const api = Object.assign(el, {
    set(st: { value?: string; text?: string; sub?: string; accent?: boolean; tone?: Tone }) {
      if (st.value !== undefined) {
        value.textContent = st.value;
        el.dataset.mode = 'value';
      }
      if (st.text !== undefined) {
        textIn.textContent = st.text;
        el.dataset.mode = 'text';
      }
      if (st.sub !== undefined) sub.textContent = st.sub;
      if (st.accent !== undefined) el.classList.toggle('is-accent', st.accent);
      if (st.tone) sub.dataset.tone = st.tone;
    },
  });
  api.set({ value: o.value, ...(o.text !== undefined ? { text: o.text } : {}) });
  return api;
}

/** A 112 px row: hairline on top, a caps label line with an optional status on the right, and a one-line title. */
export function row(o: {
  label: string;
  title: string;
  status?: { tone: Tone; text: string } | null;
  onTap?: () => void;
  chevron?: boolean;
}) {
  const el = h(
    'div',
    {
      class: 'fs-row',
      role: o.onTap ? 'button' : undefined,
      'data-snd': o.onTap ? 'open' : undefined,
    },
    h(
      'div',
      { class: 'fs-row-top' },
      text('label', 'fs-row-label', o.label),
      h('span', { class: 'f-grow' }),
      o.status
        ? h(
            'span',
            { class: 'fs-status', 'data-tone': o.status.tone },
            statusDot(o.status.tone),
            fit8(text('label', '', o.status.text)),
          )
        : null,
    ),
    h('div', { class: 'fs-row-title fst-title' }, o.title),
    o.chevron ? h('span', { class: 'fs-chev' }, icon('chevron', 24)) : null,
  );
  if (o.onTap) live(el, () => o.onTap!());
  return el;
}

/** A 64 px "more" row: hairline, caps label and a chevron, e.g. "All pull requests · 4 more". `set(text)`. */
export function moreRow(label: string, onTap?: () => void) {
  const lbl = text('label', 'fs-more-label', label);
  const el = h(
    'div',
    { class: 'fs-more', role: 'button', 'data-snd': 'open' },
    lbl,
    h('span', { class: 'f-grow' }),
    icon('chevron', 24),
  );
  live(el, () => onTap?.());
  return Object.assign(el, {
    set(t: string) {
      lbl.textContent = t;
    },
  });
}

/** A stat: caps label over a body line (24 + 8 + 32 = 64). `set(value, tone?, label?)`. */
export function stat(label: string) {
  const v = text('body', 'fs-stat-v', '--');
  const k = text('label', 'fs-stat-k', label);
  const el = h('div', { class: 'fs-stat' }, k, v);
  return Object.assign(el, {
    set(val: string, tone?: Tone, key?: string) {
      v.textContent = val;
      v.dataset.tone = tone ?? 'dim';
      if (key) k.textContent = key;
    },
  });
}

/** A hero block: a big number (hero 96) with a label over one line to its right. `accent` = it matters now. */
export function heroBlock(o: { label: string; role?: 'hero' | 'display' }) {
  const num = fit8(text(o.role ?? 'hero', 'fs-hero-n', '--'));
  const lbl = text('label', 'fs-hero-l', o.label);
  const line = text('body', 'fs-hero-s', '');
  const el = h('div', { class: 'fs-hero' }, num, h('div', { class: 'fs-hero-t' }, lbl, line));
  return Object.assign(el, {
    set(st: { n?: string; label?: string; line?: string; accent?: boolean }) {
      if (st.n !== undefined) setText(num, st.n);
      if (st.label !== undefined) lbl.textContent = st.label;
      if (st.line !== undefined) line.textContent = st.line;
      if (st.accent !== undefined) el.classList.toggle('is-accent', st.accent);
    },
  });
}

const TILE_COLOUR = { working: 'accent', needs: 'warn', idle: 'text-mute', done: 'steel' } as const;

/**
 * An agent tile, 208 × 208: the mascot in its mood, a two-line title, and a status row (the state word in its tone
 * on the left, the age or detail on the right).
 */
export function tile(a: Agent, delay = 0) {
  const colour = a.mascot ?? TILE_COLOUR[a.kind];
  const [state = '', detail = ''] = a.status.split(' · ');
  return h(
    'div',
    { class: `fs-tile fs-tile--${a.kind}` },
    mascotMood({ mood: moodFor(a.kind, a.status), colour, delay }),
    text('body', 'fs-tile-title', a.title),
    h(
      'div',
      { class: 'fs-tile-status' },
      text('label', 'fs-ts-k', state),
      text('label', 'fs-ts-v', detail.replace(/ (ctx|ago)$/, '')),
    ),
  );
}
