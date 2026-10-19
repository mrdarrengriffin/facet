// Buttons: text buttons in four kinds, icon buttons in three sizes, and the back button.
import { icon } from '@facet/icons';
import { size as Z, radius } from '@facet/tokens';
import { cornerTicks, fit8, focusRing, label, live, setState, type State } from './atoms.js';
import { h } from './dom.js';

export type ButtonKind = 'primary' | 'secondary' | 'outline' | 'ghost';
export const BUTTON_KINDS: ButtonKind[] = ['primary', 'secondary', 'outline', 'ghost'];

export interface ButtonProps {
  kind: ButtonKind;
  text: string;
  state?: State;
  /** Icon name shown before the label. */
  icon?: string;
  /** A fixed width in px (label centred). Without it the button fits its label on the 8 px grid. */
  width?: number;
  onTap?: (e: Event) => void;
}

/**
 * A 64 px text button. Primary = orange fill (active adds a focus ring), Secondary = surface with corner ticks when
 * active, Outline = orange outline (active fills it), Ghost = text only (active underlines it).
 */
export function button(o: ButtonProps) {
  const lb = label(o.text, 'label', 'f-btn-label');
  const b = h(
    'button',
    {
      class: `f-btn f-btn--${o.kind} ${o.width ? 'f-btn--fixed' : ''}`,
      type: 'button',
      style: o.width ? { width: `${o.width}px` } : null,
    },
    o.icon ? icon(o.icon, 24) : null,
    o.width ? lb : fit8(lb),
    o.kind === 'primary' ? focusRing() : null,
    o.kind === 'secondary' ? cornerTicks('all') : null,
    o.kind === 'ghost' ? h('span', { class: 'f-underline' }) : null,
  );
  setState(live(b, o.onTap), o.state ?? 'default');
  return b;
}

export type IconButtonStyle = 'play' | 'round' | 'square';
export const ICON_BUTTON_STYLES: IconButtonStyle[] = ['play', 'round', 'square'];
const ISTYLE = {
  play: {
    size: Z['icon-play'],
    glyph: Z['glyph-play'],
    radius: Z['icon-play'] / 2,
    ring: Z['focus-offset-play'],
  },
  round: { size: Z['icon-round'], glyph: Z['glyph-round'], radius: Z['icon-round'] / 2, ring: 0 },
  square: { size: Z['icon-square'], glyph: Z['glyph-square'], radius: radius.square, ring: 0 },
};

export interface IconButtonProps {
  style: IconButtonStyle;
  icon: string;
  state?: State;
  onTap?: (e: Event) => void;
  /** Accessible name; defaults to the icon name. */
  title?: string;
  /** `span` = visual only (used inside the back button). */
  as?: 'span';
}

/** Icon button: Play 96 (orange; active shows pause and a focus ring), Round 80, Square 48. */
/** The play button's ring, from the size vars so it follows the screen's density. */
function playRing() {
  const r = focusRing(ISTYLE.play.ring, ISTYLE.play.radius);
  r.style.setProperty('--ring-o', 'var(--facet-size-focus-offset-play)');
  r.style.setProperty('--ring-r', 'calc(var(--facet-size-icon-play) / 2)');
  return r;
}

export function iconButton(o: IconButtonProps) {
  const st = ISTYLE[o.style];
  const glyph = o.style === 'play' && o.state === 'active' && o.icon === 'play' ? 'pause' : o.icon;
  const el = h(
    o.as ?? 'button',
    {
      class: `f-ibtn f-ibtn--${o.style}`,
      type: o.as ? undefined : 'button',
      'aria-label': o.as ? undefined : (o.title ?? o.icon),
    },
    icon(glyph, st.glyph),
    o.style === 'play' ? playRing() : null,
  ) as HTMLElement;
  if (o.as) el.dataset.state = o.state ?? 'default';
  else setState(live(el, o.onTap), o.state ?? 'default');
  return el;
}

/** Swap the glyph of an icon button, e.g. play ↔ pause. */
export function setGlyph(btn: HTMLElement, name: string) {
  const old = btn.querySelector('.f-icon');
  if (old?.getAttribute('data-icon') === name) return;
  const size = Number(old?.getAttribute('width') || 24);
  old?.replaceWith(icon(name, size));
}

/** The back button: a 48 px square icon button with an arrow and no label. The header puts the screen name next to it. */
export function backButton(o: { state?: State; onTap?: (e: Event) => void } = {}) {
  const b = h(
    'button',
    { class: 'f-back', type: 'button', 'aria-label': 'Back' },
    iconButton({ style: 'square', icon: 'back', as: 'span', state: o.state }),
  );
  setState(live(b, o.onTap), o.state ?? 'default');
  return b;
}
