// Toasts: a message over any screen, and the layer that stacks them.
import { icon } from '@facet/icons';
import { layout as G, motion, screen, toast as T } from '@facet/tokens';
import { live } from './atoms.js';
import { button } from './buttons.js';
import { h } from './dom.js';
import type { Tone } from './types.js';

export interface ToastData {
  id?: string;
  /** Icon name. */
  icon: string;
  tone: Tone;
  title: string;
  body?: string;
  /** An optional button, e.g. "Open". */
  action?: { label: string; href?: string };
  /** How long it stays (ms). Default `toast.life` (6 s). */
  duration?: number;
}

export type DismissReason = 'tap' | 'swipe' | 'timeout' | 'action';

/**
 * A 656 × 120 toast (padding 24): a 48 px icon in a tone-tinted square, a title (screen title, 40 box) over a body line
 * (screen body, 32 box), an optional Secondary action button, and a 2 px timer bar along the bottom that shrinks over
 * the toast's life (paused while pressed).
 * Tap to dismiss, swipe sideways 64 px to dismiss. `static` = no timer (for documentation).
 */
export function toast(
  t: ToastData,
  o: { onDismiss?: (how: DismissReason) => void; onAction?: () => void; static?: boolean } = {},
) {
  const life = t.duration ?? T.life;
  const bar = h('i', { class: 'f-toast-bar' });
  const act = t.action
    ? button({
        kind: 'secondary',
        text: t.action.label,
        onTap: (e) => {
          e.stopPropagation();
          o.onAction?.();
          done('action');
        },
      })
    : null;
  const el = h(
    'div',
    { class: 'f-toast', 'data-tone': t.tone, role: 'status' },
    h('span', { class: 'f-toast-icon' }, icon(t.icon, 32)),
    h(
      'span',
      { class: 'f-toast-text' },
      h('span', { class: 'f-toast-title fst-title' }, t.title),
      h('span', { class: 'f-toast-body fst-body' }, t.body ?? ''),
    ),
    act,
    bar,
  );
  live(el);
  let closed = false;
  const done = (how: DismissReason) => {
    if (closed) return;
    closed = true;
    timer?.cancel();
    o.onDismiss?.(how);
  };
  const timer = o.static
    ? null
    : bar.animate([{ transform: 'scaleX(1)' }, { transform: 'scaleX(0)' }], {
        duration: life,
        easing: 'linear',
        fill: 'forwards',
      });
  timer?.finished.then(
    () => done('timeout'),
    () => {},
  );
  let x0 = 0,
    dx = 0,
    down = false;
  el.addEventListener('pointerdown', (e) => {
    if ((e.target as Element).closest('.f-btn')) return;
    e.preventDefault(); // no text selection or native drag while swiping
    down = true;
    x0 = e.clientX;
    dx = 0;
    el.setPointerCapture(e.pointerId);
    timer?.pause();
  });
  el.addEventListener('pointermove', (e) => {
    if (!down) return;
    const k = el.getBoundingClientRect().width / el.offsetWidth || 1;
    dx = (e.clientX - x0) / k;
    el.style.transform = `translateX(${dx}px)`;
    el.style.opacity = String(Math.max(0.3, 1 - Math.abs(dx) / 300));
  });
  el.addEventListener('pointerup', () => {
    if (!down) return;
    down = false;
    if (Math.abs(dx) >= T.swipe) {
      el.dataset.out = dx > 0 ? 'right' : 'left';
      return done('swipe');
    }
    if (Math.abs(dx) < 8) return done('tap');
    el.style.transform = '';
    el.style.opacity = '';
    timer?.play();
  });
  el.addEventListener('pointercancel', () => {
    down = false;
    el.style.transform = '';
    el.style.opacity = '';
    timer?.play();
  });
  return el;
}

/**
 * The toast layer: covers the screen (only the toasts take pointer events) and stacks up to `toast.max` (2), newest
 * nearest the edge they enter from, 16 apart. `top` = the first at y 96, just under the header; `bottom` = the
 * first ends at y 688 (the bottom inset).
 * Toasts slide 16 px in and fade; a third pushes the oldest out.
 */
export function toastLayer(
  o: { position?: 'top' | 'bottom'; onShow?: (t: ToastData) => void } = {},
) {
  const D = motion.duration,
    TH = G['toast-h'],
    GUT = G.gutter;
  const top0 = G['toast-top'];
  const bottom0 = screen.height - G.inset - TH;
  let pos: 'top' | 'bottom' = o.position ?? T.position;
  const el = h('div', { class: 'f-toasts' });
  const shown: HTMLElement[] = []; // newest first
  const ease = motion.easing['ease-out'].css;
  const layout = () =>
    shown.forEach((t, i) => {
      t.style.top = `${pos === 'top' ? top0 + i * (TH + GUT) : bottom0 - i * (TH + GUT)}px`;
    });
  const remove = (t: HTMLElement, how?: string) => {
    const i = shown.indexOf(t);
    if (i < 0) return;
    shown.splice(i, 1);
    const dx = how === 'swipe' ? (t.dataset.out === 'right' ? 160 : -160) : 0;
    const dy = how === 'swipe' ? 0 : pos === 'top' ? -16 : 16;
    const from = t.style.transform || 'translate(0, 0)';
    t.style.pointerEvents = 'none';
    t.animate(
      [
        { transform: from, opacity: t.style.opacity || 1 },
        { transform: `translate(${dx}px, ${dy}px)`, opacity: 0 },
      ],
      { duration: D['toast-out'], easing: ease, fill: 'forwards' },
    ).finished.then(
      () => t.remove(),
      () => t.remove(),
    );
    layout();
  };
  return Object.assign(el, {
    /** Show a toast. */
    push(t: ToastData, opts: { onAction?: () => void } = {}) {
      const node: HTMLElement = toast(t, {
        onDismiss: (how) => remove(node, how),
        onAction: opts.onAction,
      });
      shown.unshift(node);
      el.append(node);
      while (shown.length > T.max) remove(shown[shown.length - 1]!);
      layout();
      node.animate(
        [
          { transform: `translateY(${pos === 'top' ? -16 : 16}px)`, opacity: 0 },
          { transform: 'translateY(0)', opacity: 1 },
        ],
        { duration: D['toast-in'], easing: ease },
      );
      o.onShow?.(t);
      return node;
    },
    /** Dismiss every toast. */
    clear() {
      [...shown].forEach((t) => remove(t));
    },
    setPosition(p: 'top' | 'bottom') {
      pos = p;
      layout();
    },
    get position() {
      return pos;
    },
  });
}
