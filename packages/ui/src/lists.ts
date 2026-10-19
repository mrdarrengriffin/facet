// Lists: count badge, section header, list row, state chips, pull request row, check row, empty state, QR block.
// Used by the GitHub list and detail views, and handy anywhere a screen needs a list.
import { icon } from '@facet/icons';
import { size as Z } from '@facet/tokens';
import { fit8, label, live, setText, statusChip, statusDot, type State } from './atoms.js';
import { h, s } from './dom.js';
import { qrEncode } from './qr.js';
import type { Tone } from './types.js';

/** A 32 px tinted pill with a number. `set(n, tone?)`. */
export function countBadge(n: number | string, tone: Tone = 'idle') {
  const txt = fit8(h('span', { class: 'f-badge-text' }, String(n)));
  const el = h('span', { class: 'f-badge ft-caps-sm', 'data-tone': tone }, txt);
  return Object.assign(el, {
    set(v: number | string, t?: Tone) {
      setText(txt, String(v));
      if (t) el.dataset.tone = t;
    },
  });
}

/** A 48 px section header: caps title, an optional count badge, and optional meta on the right. */
export function sectionHeader(o: { title: string; count?: number; tone?: Tone; right?: string }) {
  const badge = o.count === undefined ? null : countBadge(o.count, o.tone);
  const right = fit8(h('span', { class: 'f-section-meta ft-meta' }, o.right ?? ''));
  const el = h(
    'div',
    { class: 'f-section' },
    fit8(label(o.title, 'caps-sm', 'f-section-title')),
    badge,
    h('span', { class: 'f-grow' }),
    right,
  );
  return Object.assign(el, {
    set(st: { count?: number; tone?: Tone; right?: string }) {
      if (badge && st.count !== undefined) badge.set(st.count, st.tone);
      if (st.right !== undefined) setText(right, st.right);
    },
  });
}

/** A full-width surface row (16 px padding) holding a column of children. Tappable when `onTap` is set. */
export function listRow(o: {
  kids: (Node | null)[];
  onTap?: (e: Event) => void;
  state?: State;
  height?: number;
  cls?: string;
}) {
  const el = h(
    'div',
    {
      class: `f-row ${o.cls ?? ''}`,
      role: o.onTap ? 'button' : undefined,
      tabindex: o.onTap ? 0 : undefined,
      style: o.height ? { height: `${o.height}px` } : null,
    },
    ...o.kids,
  );
  if (o.onTap) live(el, o.onTap);
  el.dataset.state = o.state ?? 'default';
  return el;
}

/** Chip kinds for pull requests: review state, CI and why it needs you. */
export type ChipKind =
  | 'approved'
  | 'changes'
  | 'pending'
  | 'draft'
  | 'ci-pass'
  | 'ci-fail'
  | 'ci-pending'
  | 'review'
  | 'assigned';
export const CHIP: Record<ChipKind, { tone: Tone; text: string }> = {
  approved: { tone: 'ok', text: 'approved' },
  changes: { tone: 'error', text: 'changes requested' },
  pending: { tone: 'idle', text: 'review pending' },
  draft: { tone: 'idle', text: 'draft' },
  'ci-pass': { tone: 'ok', text: 'ci pass' },
  'ci-fail': { tone: 'error', text: 'ci fail' },
  'ci-pending': { tone: 'working', text: 'ci running' },
  review: { tone: 'warn', text: 'review' },
  assigned: { tone: 'info', text: 'assigned' },
};
export const stateChip = (k: ChipKind) => statusChip(CHIP[k].tone, CHIP[k].text);

export interface PrRowProps {
  repo: string;
  number: number;
  title: string;
  /** Compact age, e.g. "3d". */
  age: string;
  chips: ChipKind[];
}

/** A 144 px pull request row: repo · #number … age, a title of up to two lines, and a row of chips. */
export function prRow(p: PrRowProps, onTap?: (e: Event) => void, state?: State) {
  return listRow({
    cls: 'f-prrow',
    height: Z['pr-row-h'],
    onTap,
    state,
    kids: [
      h(
        'div',
        { class: 'f-prrow-meta ft-meta' },
        fit8(h('span', { class: 'f-prrow-repo' }, p.repo)),
        fit8(h('span', { class: 'f-prrow-num' }, `#${p.number}`)),
        h('span', { class: 'f-grow' }),
        fit8(h('span', { class: 'f-prrow-age' }, p.age)),
      ),
      h('div', { class: 'f-prrow-title ft-tile' }, p.title),
      h('div', { class: 'f-chips' }, ...p.chips.map(stateChip)),
    ],
  });
}

/** A 32 px check row: status dot, name and state, e.g. one CI check. */
export function checkRow(name: string, state: 'pass' | 'fail' | 'pending' | 'skipped') {
  const tone: Tone =
    state === 'pass' ? 'ok' : state === 'fail' ? 'error' : state === 'pending' ? 'working' : 'idle';
  return h(
    'div',
    { class: 'f-check', 'data-tone': tone },
    statusDot(tone),
    h('span', { class: 'f-check-name ft-meta' }, name),
    h('span', { class: 'f-check-state ft-caps-sm' }, state),
  );
}

/** A 136 px empty state: a 48 px icon in the tone colour, a title and a line of body text, centred. */
export function emptyState(o: { icon: string; title: string; body?: string; tone?: Tone }) {
  return h(
    'div',
    { class: 'f-empty', 'data-tone': o.tone ?? 'ok' },
    h('span', { class: 'f-empty-icon' }, icon(o.icon, Z['empty-icon'])),
    h('span', { class: 'f-empty-title ft-tile' }, o.title),
    h('span', { class: 'f-empty-body ft-meta' }, o.body ?? ''),
  );
}

/**
 * A QR block (208 wide): the code on a light surface in both themes (scanners want dark on light) plus a caption.
 * On a panel this maps to LVGL's QR widget with the same text. `set(text)` re-encodes.
 */
export function qrBlock(o: { text: string; caption?: string }) {
  const S = Z.qr;
  const box = h('span', { class: 'f-qr-code', style: { width: `${S}px`, height: `${S}px` } });
  const cap = h('span', { class: 'f-qr-cap ft-caps-sm' }, o.caption ?? 'open on your phone');
  const draw = (text: string) => {
    box.textContent = '';
    if (!text) return;
    const q = qrEncode(text);
    const Q = 2;
    const n = q.size + 2 * Q;
    let d = '';
    q.modules.forEach((row, y) =>
      row.forEach((on, x) => {
        if (on) d += `M${x + Q} ${y + Q}h1v1h-1z`;
      }),
    );
    box.append(
      s(
        'svg',
        {
          viewBox: `0 0 ${n} ${n}`,
          width: S,
          height: S,
          'shape-rendering': 'crispEdges',
          'aria-label': `QR code: ${text}`,
        },
        s('rect', { width: n, height: n, fill: 'var(--facet-color-qr-bg)' }),
        s('path', { d, fill: 'var(--facet-color-qr-fg)' }),
      ),
    );
  };
  draw(o.text);
  const el = h('div', { class: 'f-qr' }, box, cap);
  return Object.assign(el, { set: draw });
}
