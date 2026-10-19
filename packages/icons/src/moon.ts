// Moon phase glyph, drawn in code on the weather icons' 48 grid: a disc (r 20) with a 2 px outline (3 px from 96 px
// up), the dark side filled with --facet-color-moon-dark and the lit part with --facet-color-moon-lit. The lit part is
// one path: the lit limb (a half circle) and the terminator (half an ellipse, rx = r × |2k − 1| for illumination k), so
// a panel can draw it as an arc plus a precomputed polyline, or as one of a few dozen alpha-mask images.
//
// `frac` = how far through the synodic month: 0 new, 0.25 first quarter, 0.5 full, 0.75 last quarter. North of the
// equator the lit side grows from the right; `south: true` mirrors it.
import { svg as s } from './svg.js';

const R = 20,
  C = 24;
const f = (n: number) => n.toFixed(2);

/** Lit fraction of the disc (0-1) at `frac` through the month. */
export const moonIllumination = (frac: number) => (1 - Math.cos(2 * Math.PI * frac)) / 2;

/** The lit part's path for `frac` on the 48 grid (empty near new moon). */
export function moonLitPath(frac: number, south = false) {
  const p = ((frac % 1) + 1) % 1;
  const k = moonIllumination(p);
  if (k < 0.005) return '';
  if (k > 0.995)
    return `M${C} ${C - R} A${R} ${R} 0 1 1 ${C} ${C + R} A${R} ${R} 0 1 1 ${C} ${C - R} Z`;
  // waxing = lit on the right (north); waning or south mirrors it
  const right = p < 0.5 !== south;
  const rx = R * Math.abs(2 * k - 1);
  // limb: top -> bottom round the lit side; terminator: bottom -> top, bulging towards the lit side for a crescent
  // and away from it for a gibbous moon
  const limb = right ? 1 : 0;
  const term = k < 0.5 === right ? 0 : 1;
  return `M${C} ${C - R} A${R} ${R} 0 0 ${limb} ${C} ${C + R} A${f(rx)} ${R} 0 0 ${term} ${C} ${C - R} Z`;
}

function body(frac: number, south: boolean): SVGElement[] {
  const d = moonLitPath(frac, south);
  return [
    s('circle', { class: 'fmoon-dark', cx: C, cy: C, r: R, fill: 'var(--facet-color-moon-dark)' }),
    d ? s('path', { class: 'fmoon-lit', d, fill: 'var(--facet-color-moon-lit)' }) : null,
    s('circle', {
      class: 'fmoon-edge',
      cx: C,
      cy: C,
      r: R,
      fill: 'none',
      stroke: 'var(--facet-color-moon-edge)',
      'stroke-width': 'var(--fmoon-w)',
      'vector-effect': 'non-scaling-stroke',
    }),
  ].filter((x): x is SVGElement => !!x);
}

/** Moon phase glyph: `frac` 0-1 through the month, size px. `set(frac)` redraws it. */
export function moonIcon(
  frac: number,
  o: { size?: number; stroke?: number; south?: boolean } = {},
) {
  const size = o.size ?? 48;
  let south = !!o.south;
  const el = s(
    'svg',
    {
      class: 'fmoon',
      viewBox: '0 0 48 48',
      width: size,
      height: size,
      'aria-hidden': 'true',
      style: { '--fmoon-w': `${o.stroke ?? (size >= 96 ? 3 : 2)}px` },
    },
    ...body(frac, south),
  );
  let cur = frac;
  return Object.assign(el, {
    set(next: number, opts: { south?: boolean } = {}) {
      const ns = opts.south ?? south;
      if (Math.abs(next - cur) < 0.0005 && ns === south) return;
      cur = next;
      south = ns;
      el.replaceChildren(...body(next, south));
    },
  });
}
