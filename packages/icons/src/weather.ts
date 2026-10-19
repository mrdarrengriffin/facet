// Weather icons, drawn in code on a 48 grid with 2 px round strokes (3 px from 96 px up). Everything is built from
// shapes an LVGL panel can draw: circles and arcs (sun, cloud lobes, moon, hail), lines (rays, drops, fog, the bolt)
// and small filled dots (stars, snow).
//
// Colours: sun and bolt = accent, rain and hail = blue, cloud and snow = text, back cloud and fog = text-dim,
// stars = steel. The front cloud is filled with --fwx-mask (the canvas by default; cards set the surface) so it
// hides what's behind it.
//
// Animation (`animated: true`, weather.css) only moves a few small parts with transform and opacity: rays turn once
// every 24 s, clouds drift 1-2 px, drops fall and fade, snow drifts, hail bounces, the bolt flashes twice every 4 s,
// fog and wind lines slide 2 px and stars twinkle. It switches off with prefers-reduced-motion.
import { svg as s } from './svg.js';

export const WX_ICONS = [
  'clear-day',
  'clear-night',
  'partly-day',
  'partly-night',
  'cloudy',
  'fog',
  'rain-light',
  'rain-heavy',
  'showers-day',
  'showers-night',
  'snow',
  'sleet',
  'thunder',
  'thunder-rain',
  'hail',
  'wind',
  'exceptional',
] as const;
export type WxIcon = (typeof WX_ICONS)[number];

export const WX_LABEL: Record<WxIcon, string> = {
  'clear-day': 'Clear',
  'clear-night': 'Clear night',
  'partly-day': 'Partly cloudy',
  'partly-night': 'Partly cloudy (night)',
  cloudy: 'Cloudy',
  fog: 'Fog',
  'rain-light': 'Light rain',
  'rain-heavy': 'Heavy rain',
  'showers-day': 'Showers',
  'showers-night': 'Showers (night)',
  snow: 'Snow',
  sleet: 'Sleet',
  thunder: 'Thunder',
  'thunder-rain': 'Thunder + rain',
  hail: 'Hail',
  wind: 'Wind',
  exceptional: 'Exceptional',
};

/** HA weather condition (+ day/night, precipitation mm) -> icon id. */
export function wxIcon(condition: string | null | undefined, day = true, precip?: number): WxIcon {
  switch (condition) {
    case 'sunny':
      return day ? 'clear-day' : 'clear-night';
    case 'clear-night':
      return 'clear-night';
    case 'partlycloudy':
      return day ? 'partly-day' : 'partly-night';
    case 'cloudy':
      return 'cloudy';
    case 'fog':
      return 'fog';
    case 'rainy':
      return precip !== undefined && precip > 0 && precip < 0.3
        ? day
          ? 'showers-day'
          : 'showers-night'
        : 'rain-light';
    case 'pouring':
      return 'rain-heavy';
    case 'snowy':
      return 'snow';
    case 'snowy-rainy':
      return 'sleet';
    case 'lightning':
      return 'thunder';
    case 'lightning-rainy':
      return 'thunder-rain';
    case 'hail':
      return 'hail';
    case 'windy':
    case 'windy-variant':
      return 'wind';
    case 'exceptional':
      return 'exceptional';
    default:
      return 'cloudy';
  }
}

export const WX_TEXT: Record<string, string> = {
  sunny: 'Sunny',
  'clear-night': 'Clear',
  partlycloudy: 'Partly cloudy',
  cloudy: 'Cloudy',
  fog: 'Fog',
  rainy: 'Rain',
  pouring: 'Heavy rain',
  snowy: 'Snow',
  'snowy-rainy': 'Sleet',
  lightning: 'Thunder',
  'lightning-rainy': 'Thunderstorms',
  hail: 'Hail',
  windy: 'Windy',
  'windy-variant': 'Windy, cloudy',
  exceptional: 'Exceptional',
};

// ---------------------------------------------------------------- geometry
type C = { x: number; y: number; r: number };
/** Upper intersection point of two circles (smaller y). */
function meet(a: C, b: C) {
  const dx = b.x - a.x,
    dy = b.y - a.y,
    d = Math.hypot(dx, dy);
  const l = (a.r * a.r - b.r * b.r + d * d) / (2 * d),
    hh = Math.sqrt(Math.max(0, a.r * a.r - l * l));
  const mx = a.x + (l * dx) / d,
    my = a.y + (l * dy) / d;
  const p1 = { x: mx + (hh * dy) / d, y: my - (hh * dx) / d },
    p2 = { x: mx - (hh * dy) / d, y: my + (hh * dx) / d };
  return p1.y < p2.y ? p1 : p2;
}
const ang = (c: C, p: { x: number; y: number }) =>
  (Math.atan2(p.y - c.y, p.x - c.x) * 180) / Math.PI;
/** SVG arc segment on circle c from p to q, clockwise on screen (sweep 1). */
function arcTo(c: C, p: { x: number; y: number }, q: { x: number; y: number }) {
  const sw = (ang(c, q) - ang(c, p) + 360) % 360;
  return `A${c.r} ${c.r} 0 ${sw > 180 ? 1 : 0} 1 ${q.x.toFixed(2)} ${q.y.toFixed(2)}`;
}
const f = (n: number) => n.toFixed(2);

/** Cloud outline = 3 arcs (left lobe, top lobe, right lobe) + a flat base: lv_arc x3 + lv_line. */
function cloudPath(dx = 0, dy = 0, k = 1) {
  const T = (c: C): C => ({ x: 24 + (c.x - 24) * k + dx, y: 35 + (c.y - 35) * k + dy, r: c.r * k });
  const L = T({ x: 15, y: 29, r: 6 }),
    M = T({ x: 25, y: 23.5, r: 9 }),
    R = T({ x: 34, y: 29, r: 6 });
  const p0 = { x: L.x, y: L.y + L.r },
    p1 = meet(L, M),
    p2 = meet(M, R),
    p3 = { x: R.x, y: R.y + R.r };
  return `M${f(p0.x)} ${f(p0.y)} ${arcTo(L, p0, p1)} ${arcTo(M, p1, p2)} ${arcTo(R, p2, p3)} Z`;
}

/** Crescent moon: outer circle arc + inner (offset) circle arc. */
function moonPath(cx: number, cy: number, r: number) {
  const O = { x: cx, y: cy, r },
    I = { x: cx + r * 0.62, y: cy - r * 0.42, r: r * 0.86 };
  // the two intersections of O and I
  const dx = I.x - O.x,
    dy = I.y - O.y,
    d = Math.hypot(dx, dy);
  const l = (O.r * O.r - I.r * I.r + d * d) / (2 * d),
    hh = Math.sqrt(O.r * O.r - l * l);
  const mx = O.x + (l * dx) / d,
    my = O.y + (l * dy) / d;
  const a = { x: mx + (hh * dy) / d, y: my - (hh * dx) / d },
    b = { x: mx - (hh * dy) / d, y: my + (hh * dx) / d };
  // outer: from a the long way round (counter-clockwise on screen) to b; inner: back from b to a
  return `M${f(a.x)} ${f(a.y)} A${O.r} ${O.r} 0 1 0 ${f(b.x)} ${f(b.y)} A${f(I.r)} ${f(I.r)} 0 0 1 ${f(a.x)} ${f(a.y)} Z`;
}

const ST = (c: string, extra: Record<string, unknown> = {}) => ({
  fill: 'none',
  stroke: `var(--facet-color-${c})`,
  'stroke-width': 'var(--fwx-w)',
  'stroke-linecap': 'round',
  'stroke-linejoin': 'round',
  'vector-effect': 'non-scaling-stroke',
  ...extra,
});

function sun(cx: number, cy: number, r: number, rin: number, rout: number, n = 8) {
  const rays = s('g', { class: 'fwx-rays', style: { transformOrigin: `${cx}px ${cy}px` } });
  for (let i = 0; i < n; i++) {
    const a = (i * 2 * Math.PI) / n - Math.PI / 2;
    rays.append(
      s('line', {
        x1: f(cx + rin * Math.cos(a)),
        y1: f(cy + rin * Math.sin(a)),
        x2: f(cx + rout * Math.cos(a)),
        y2: f(cy + rout * Math.sin(a)),
        ...ST('accent'),
      }),
    );
  }
  return s('g', { class: 'fwx-sun' }, rays, s('circle', { cx, cy, r, ...ST('accent') }));
}
const moon = (cx: number, cy: number, r: number) =>
  s('path', { class: 'fwx-moon', d: moonPath(cx, cy, r), ...ST('text') });
const star = (x: number, y: number, i: number) =>
  s('circle', {
    class: 'fwx-star',
    cx: x,
    cy: y,
    r: 1.25,
    fill: 'var(--facet-color-steel)',
    style: { '--i': i },
  });
const cloud = (cls: string, c: string, dx = 0, dy = 0, k = 1, mask = true) =>
  s('path', {
    class: `fwx-cloud ${cls}`,
    d: cloudPath(dx, dy, k),
    ...ST(c, { fill: mask ? 'var(--fwx-mask)' : 'none' }),
  });
const drop = (x: number, y: number, i: number, len = 5) =>
  s('line', {
    class: 'fwx-drop',
    x1: x,
    y1: y,
    x2: x - len * 0.4,
    y2: y + len,
    ...ST('blue'),
    style: { '--i': i },
  });
const flake = (x: number, y: number, i: number) =>
  s('circle', {
    class: 'fwx-flake',
    cx: x,
    cy: y,
    r: 1.75,
    fill: 'var(--facet-color-text)',
    style: { '--i': i },
  });
const hailstone = (x: number, y: number, i: number) =>
  s('circle', { class: 'fwx-hail', cx: x, cy: y, r: 2, ...ST('blue'), style: { '--i': i } });
const bolt = () =>
  s('polyline', { class: 'fwx-bolt', points: '27,31 22,37 26,37 22,43', ...ST('accent') });
const line = (cls: string, x1: number, y: number, x2: number, c: string, i: number) =>
  s('line', { class: cls, x1, y1: y, x2, y2: y, ...ST(c), style: { '--i': i } });

function body(id: WxIcon): SVGElement[] {
  const front = (dx = 0, dy = 0) => cloud('fwx-cloud-front', 'text', dx, dy);
  switch (id) {
    case 'clear-day':
      return [sun(24, 24, 8, 13, 18, 8)];
    case 'clear-night':
      return [moon(23, 25, 11), star(35, 12, 0), star(39, 21, 1), star(31, 6, 2)];
    case 'partly-day':
      return [sun(17, 17, 6, 9.5, 12), front(3, 2)];
    case 'partly-night':
      return [moon(16, 16, 7.5), star(32, 8, 0), star(38, 15, 1), front(3, 2)];
    case 'cloudy':
      return [cloud('fwx-cloud-back', 'text-dim', 7, -7, 0.72), front(-2, 1)];
    case 'fog':
      return [
        cloud('fwx-cloud-back', 'text-dim', 0, -6, 0.9, false),
        line('fwx-fog', 8, 33, 34, 'text-dim', 0),
        line('fwx-fog', 14, 38, 40, 'text-dim', 1),
        line('fwx-fog', 10, 43, 30, 'text-dim', 2),
      ];
    case 'rain-light':
      return [front(0, -5), drop(18, 34, 0), drop(26, 36, 1), drop(34, 34, 2)];
    case 'rain-heavy':
      return [
        front(0, -6),
        drop(14, 33, 0, 8),
        drop(20, 35, 1, 8),
        drop(26, 33, 2, 8),
        drop(32, 35, 3, 8),
        drop(38, 33, 4, 8),
      ];
    case 'showers-day':
      return [sun(15, 15, 5, 7.5, 10), front(3, -3), drop(22, 36, 0), drop(31, 36, 1)];
    case 'showers-night':
      return [moon(14, 13, 6.5), front(3, -3), drop(22, 36, 0), drop(31, 36, 1)];
    case 'snow':
      return [
        front(0, -5),
        flake(17, 36, 0),
        flake(25, 39, 1),
        flake(33, 36, 2),
        flake(21, 42, 3),
        flake(30, 42, 4),
      ];
    case 'sleet':
      return [front(0, -5), drop(17, 34, 0), flake(25, 38, 1), drop(33, 34, 2), flake(29, 42, 3)];
    case 'thunder':
      return [front(0, -6), bolt()];
    case 'thunder-rain':
      return [front(0, -6), bolt(), drop(15, 33, 0), drop(35, 33, 1), drop(32, 38, 2)];
    case 'hail':
      return [front(0, -5), hailstone(17, 37, 0), hailstone(25, 41, 1), hailstone(33, 37, 2)];
    case 'wind':
      return [
        s('path', {
          class: 'fwx-wind',
          d: 'M6 18 H27 A4.5 4.5 0 1 0 22.5 13.5',
          ...ST('text'),
          style: { '--i': 0 },
        }),
        s('path', {
          class: 'fwx-wind',
          d: 'M6 26 H35 A4.5 4.5 0 1 1 30.5 30.5',
          ...ST('blue'),
          style: { '--i': 1 },
        }),
        s('path', {
          class: 'fwx-wind',
          d: 'M10 34 H22 A3.5 3.5 0 1 1 18.5 37.5',
          ...ST('text-dim'),
          style: { '--i': 2 },
        }),
      ];
    case 'exceptional':
      return [
        s('circle', { cx: 24, cy: 24, r: 15, ...ST('accent') }),
        s('line', { x1: 24, y1: 16, x2: 24, y2: 26, ...ST('accent') }),
        s('circle', { cx: 24, cy: 31.5, r: 1.5, fill: 'var(--facet-color-accent)' }),
      ];
  }
}

/** Weather icon component: id from WX_ICONS (or wxIcon(condition)), size px, animated variant on/off. */
export function weatherIcon(
  id: WxIcon,
  o: { size?: number; animated?: boolean; stroke?: number } = {},
) {
  const size = o.size ?? 48;
  const el = s(
    'svg',
    {
      class: `fwx ${o.animated ? 'is-anim' : ''}`,
      viewBox: '0 0 48 48',
      width: size,
      height: size,
      'aria-hidden': 'true',
      'data-fwx': id,
      style: { '--fwx-w': `${o.stroke ?? (size >= 96 ? 3 : 2)}px` },
    },
    ...body(id),
  );
  return Object.assign(el, {
    set(next: WxIcon) {
      if (el.getAttribute('data-fwx') === next) return;
      el.replaceChildren(...body(next));
      el.setAttribute('data-fwx', next);
    },
    setAnimated(on: boolean) {
      el.classList.toggle('is-anim', on);
    },
  });
}
