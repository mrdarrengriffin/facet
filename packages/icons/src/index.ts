/**
 * Facet icons.
 *
 * UI icons are master SVGs in `svg/` (24 grid, 2 px round strokes in `currentColor`, a 20 px live area, drawn to
 * one of four keylines). `icon()` renders one as an inline SVG element, so it takes its colour from the text colour
 * around it. Weather icons are drawn in code (`weatherIcon()`) and can animate.
 */
import { glyphs, type Glyph, type IconName } from './glyphs.js';
import { svg } from './svg.js';

export { glyphs, type Glyph, type IconName };
export { svg };
export * from './weather.js';
export * from './moon.js';

/** Every UI icon name, sorted. */
export const iconNames = Object.keys(glyphs).sort() as IconName[];

/** Stroke width in screen pixels at the optical sizes: one master (24 grid), a stroke that stays readable. */
export const OPTICAL_STROKE: readonly (readonly [number, number])[] = [
  [16, 1.5],
  [24, 2],
  [32, 2.5],
  [48, 3],
];

/**
 * The stroke width (in 24-grid units) for an icon drawn at `size` px: the pixel stroke from `OPTICAL_STROKE`
 * (linear in between, clamped at the ends) scaled back to the grid. 2 at 24, 2.25 at 16, 1.875 at 32, 1.5 at 48.
 */
export function opticalStroke(size: number): number {
  let px = 3;
  let prev: readonly [number, number] | undefined;
  for (const [s, p] of OPTICAL_STROKE) {
    if (size <= s) {
      px = prev ? prev[1] + ((p - prev[1]) * (size - prev[0])) / (s - prev[0]) : p;
      break;
    }
    px = p;
    prev = [s, p];
  }
  return Math.round(((px * 24) / size) * 1000) / 1000;
}

/** Is this a known icon name? */
export const isIcon = (name: string): name is IconName => name in glyphs;

/**
 * Render an icon as an inline `<svg>`. Unknown names fall back to `close`, so a typo shows up rather than leaving a
 * gap. `cls` adds classes next to `facet-icon`.
 */
export function icon(name: IconName | string, size = 24, cls = ''): SVGElement {
  const g: Glyph = isIcon(name) ? glyphs[name] : glyphs.close;
  const path = g.stroke
    ? {
        d: g.d,
        fill: 'none',
        stroke: 'currentColor',
        'stroke-width': opticalStroke(size),
        'stroke-linecap': 'round',
        'stroke-linejoin': 'round',
      }
    : { d: g.d, fill: 'currentColor' };
  return svg(
    'svg',
    {
      class: `f-icon ${cls}`.trim(),
      viewBox: '0 0 24 24',
      width: size,
      height: size,
      'aria-hidden': 'true',
      'data-icon': name,
    },
    svg('path', path),
  );
}

import { screenGlyphs, type ScreenIconName } from './screen-glyphs.js';
export { screenGlyphs, type ScreenIconName };

/** Every screen icon name (home, agents, media, light, clock, github, energy, weather, lightning, timer, toasts, settings). */
export const screenIconNames = Object.keys(screenGlyphs).sort() as ScreenIconName[];

const withStroke = (inner: string, size: number) =>
  inner.replace('stroke-width="2"', `stroke-width="${opticalStroke(size)}"`);

/** Render a screen icon: 24 grid, 2 px round strokes in currentColor with one detail in the accent colour. */
export function screenIcon(name: ScreenIconName | string, size = 24, cls = ''): SVGElement {
  const el = svg('svg', {
    class: `f-icon f-screen-icon ${cls}`.trim(),
    viewBox: '0 0 24 24',
    width: size,
    height: size,
    'aria-hidden': 'true',
    'data-screen-icon': name,
  });
  el.innerHTML = withStroke(
    screenGlyphs[(name in screenGlyphs ? name : 'home') as ScreenIconName],
    size,
  );
  return el;
}

/** The raw SVG file text of a screen icon, for use outside the DOM (e.g. server-rendered pages). */
export function screenIconSvg(name: ScreenIconName | string, size = 24): string {
  const inner = withStroke(
    screenGlyphs[(name in screenGlyphs ? name : 'home') as ScreenIconName],
    size,
  );
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="${size}" height="${size}" aria-hidden="true" class="f-icon f-screen-icon">${inner}</svg>`;
}
