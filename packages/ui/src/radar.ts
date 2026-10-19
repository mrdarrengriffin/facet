// Lightning radar and helpers. Distances use a square-root scale so the near rings get room; ring radii are snapped
// to the 8 px grid and strikes are placed piecewise-linearly between the drawn rings, so dots always agree with them.
// Strikes are 8 px dots coloured by age (< 1 min white with an orange edge and a flash ring, < 5 min orange,
// < 15 min faded orange, older faded steel). The nearest strike of the last 15 minutes gets a 24 px ring.
import { motion, size as Z } from '@facet/tokens';
import { h, s } from './dom.js';
import type { LightningData, Strike, Tone } from './types.js';

const DIRS = ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW'];
/** Bearing in degrees → "NE". */
export const compass = (az: number) =>
  DIRS[Math.floor((((az % 360) + 360 + 22.5) % 360) / 45) % 8]!;
/** Age class of a strike: 0 < 1 min, 1 < 5, 2 < 15, 3 older. */
const ageClass = (t: number, now: number) => {
  const m = (now - t) / 60_000;
  return m < 1 ? 0 : m < 5 ? 1 : m < 15 ? 2 : 3;
};

/** One line for a Home card or chip: quiet unless there were strikes in the last 15 minutes. */
export function lightningSummary(d: LightningData | null | undefined): {
  status: string;
  tone: Tone;
  active: boolean;
} {
  if (!d) return { status: 'loading', tone: 'dim', active: false };
  if (d.error && !d.strikes.length) return { status: 'offline', tone: 'error', active: false };
  const n = d.nearest;
  if (!n)
    return {
      status: d.counts.m60
        ? `quiet · ${d.counts.m60} strikes in the last hour`
        : `quiet · nothing within ${d.radius} km`,
      tone: 'dim',
      active: false,
    };
  const where = `${n.d} km ${n.dir}`;
  if (d.trend.state === 'approaching')
    return { status: `approaching · ${where} · ~${d.trend.kmh} km/h`, tone: 'warn', active: true };
  if (n.d <= 25)
    return { status: `nearby · ${where} · ${d.counts.m15} in 15 min`, tone: 'warn', active: true };
  return { status: `${where} · ${d.counts.m15} in 15 min`, tone: 'info', active: d.storm };
}

/** The 72 px Home card preview: two rings (100 / 25 km), home, and the last 15 minutes of strikes as 4 px dots. */
export function previewLightning() {
  const S = Z.preview,
    C = S / 2;
  const dots = h('i', {
    class: 'f-abs',
    style: { left: '0', top: '0', width: `${S}px`, height: `${S}px` },
  });
  const ring = (d: number, c: string) =>
    h('i', {
      class: 'f-abs',
      style: {
        left: `${C - d / 2}px`,
        top: `${C - d / 2}px`,
        width: `${d}px`,
        height: `${d}px`,
        borderRadius: '999px',
        border: `1px solid var(--facet-color-${c})`,
      },
    });
  const el = h(
    'span',
    null,
    ring(S - 4, 'line-hi'),
    ring(36, 'groove'),
    h('i', {
      class: 'f-abs',
      style: {
        left: `${C - 3}px`,
        top: `${C - 3}px`,
        width: '6px',
        height: '6px',
        borderRadius: '999px',
        background: 'var(--facet-color-blue)',
      },
    }),
    dots,
  );
  const set = (d: LightningData | null | undefined) => {
    const now = Date.now();
    const list = (d?.strikes ?? []).filter((x) => now - x.t < 15 * 60_000).slice(0, 24);
    const R = d?.radius ?? 100;
    dots.replaceChildren(
      ...list.map((x) => {
        const r = (C - 4) * Math.sqrt(Math.min(Math.max(x.d, 0), R) / R);
        const a = (x.az * Math.PI) / 180;
        return h('i', {
          class: 'f-abs f-lx-mini',
          'data-age': String(ageClass(x.t, now)),
          style: { left: `${C + r * Math.sin(a) - 2}px`, top: `${C - r * Math.cos(a) - 2}px` },
        });
      }),
    );
  };
  return Object.assign(el, { set });
}

export interface RadarProps {
  /** Square size in px. Default 384. */
  size?: number;
  /** [km, px] per ring, innermost first. Default 10 / 25 / 50 / 100 km at 64 / 96 / 136 / 192. */
  rings?: [number, number][];
}

/**
 * The radar: rings, 30° ticks, ring labels just right of the south axis, an N marker, home in the centre, strike
 * dots and the nearest-strike ring. `render(data, visible)`: new strikes flash only while `visible`.
 */
export function lightningRadar(o: RadarProps = {}) {
  const RAD = o.size ?? 384,
    R = RAD / 2;
  const RINGS = o.rings ?? [
    [10, 64],
    [25, 96],
    [50, 136],
    [100, 192],
  ];
  const rPx = (d: number) => {
    let pd = 0,
      pr = 0;
    for (const [km, px] of RINGS) {
      if (d <= km) return pr + ((d - pd) / (km - pd)) * (px - pr);
      pd = km;
      pr = px;
    }
    return R;
  };
  const outer = RINGS[RINGS.length - 1]![0];
  const svg = s('svg', {
    class: 'f-abs f-lx-svg',
    width: RAD,
    height: RAD,
    viewBox: `0 0 ${RAD} ${RAD}`,
    'aria-hidden': 'true',
    style: { left: '0', top: '0' },
  });
  for (const [km, px] of RINGS)
    svg.append(
      s('circle', {
        cx: R,
        cy: R,
        r: px - (km === outer ? 1 : 0.5),
        fill: 'none',
        class: km === 25 ? 'f-lx-ring f-lx-ring--alert' : 'f-lx-ring',
      }),
    );
  for (let a = 0; a < 360; a += 30) {
    const major = a % 90 === 0,
      len = major ? 16 : 8,
      rad = (a * Math.PI) / 180;
    svg.append(
      s('line', {
        x1: R + (R - 1) * Math.sin(rad),
        y1: R - (R - 1) * Math.cos(rad),
        x2: R + (R - 1 - len) * Math.sin(rad),
        y2: R - (R - 1 - len) * Math.cos(rad),
        class: major ? 'f-lx-tick f-lx-tick--major' : 'f-lx-tick',
      }),
    );
  }
  const B = 24;
  const lbl = (text: string, x: number, y: number, w: number, cls: string, align = 'center') =>
    h(
      'span',
      {
        class: `f-abs ${cls}`,
        style: { left: `${x}px`, top: `${y}px`, width: `${w}px`, textAlign: align },
      },
      text,
    );
  const ringLbls = RINGS.map(([km, px]) =>
    lbl(String(km), R + 8, R + px - 8 - B, 88, 'f-lx-rlbl fst-label', 'left'),
  );
  const north = lbl('N', R - B / 2, 24, B, 'f-lx-cmp fst-label');
  const home = h('i', {
    class: 'f-abs f-lx-home',
    style: { left: `${R - 8}px`, top: `${R - 8}px` },
  });
  const dots = h('div', {
    class: 'f-abs f-lx-dots',
    style: { left: '0', top: '0', width: `${RAD}px`, height: `${RAD}px` },
  });
  const fx = h('div', {
    class: 'f-abs',
    style: { left: '0', top: '0', width: `${RAD}px`, height: `${RAD}px`, pointerEvents: 'none' },
  });
  const nearRing = h('i', { class: 'f-abs f-lx-near', hidden: true });
  const el = h(
    'div',
    { class: 'f-lx-radar', style: { width: `${RAD}px`, height: `${RAD}px` } },
    svg,
    ...ringLbls,
    north,
    dots,
    nearRing,
    fx,
    home,
  );
  const seen = new Set<string>();
  const nodes = new Map<string, HTMLElement>();
  const pos = (x: { d: number; az: number }) => {
    const r = rPx(x.d),
      a = (x.az * Math.PI) / 180;
    return { x: R + r * Math.sin(a), y: R - r * Math.cos(a) };
  };
  const flash = (x: Strike) => {
    const p = pos(x);
    const ring = h('i', {
      class: 'f-abs f-lx-flash',
      style: { left: `${p.x - 4}px`, top: `${p.y - 4}px` },
    });
    fx.append(ring);
    ring
      .animate(
        [
          { transform: 'scale(1)', opacity: 1 },
          { transform: 'scale(6)', opacity: 0 },
        ],
        { duration: 600, easing: motion.easing['ease-out'].css },
      )
      .finished.then(
        () => ring.remove(),
        () => ring.remove(),
      );
  };
  function render(d: LightningData | null | undefined, visible: boolean, now = Date.now()) {
    const list = d?.strikes ?? [];
    const ids = new Set(list.map((x) => x.id));
    for (const [id, n] of nodes)
      if (!ids.has(id)) {
        n.remove();
        nodes.delete(id);
      }
    let fresh = 0;
    for (const x of [...list].reverse()) {
      // oldest first, so the newest paint on top
      let n = nodes.get(x.id);
      if (!n) {
        const p = pos(x);
        n = h('i', {
          class: 'f-abs f-lx-dot',
          style: { left: `${p.x - 4}px`, top: `${p.y - 4}px` },
        });
        nodes.set(x.id, n);
        dots.append(n);
        if (!seen.has(x.id)) {
          seen.add(x.id);
          if (visible && now - x.t < 90_000 && fresh++ < 6) flash(x);
        }
      }
      n.dataset.age = String(ageClass(x.t, now));
    }
    if (seen.size > 4000) {
      seen.clear();
      list.forEach((x) => seen.add(x.id));
    }
    const n = d?.nearest;
    nearRing.hidden = !n;
    if (n) {
      const p = pos(n);
      nearRing.style.left = `${p.x - 12}px`;
      nearRing.style.top = `${p.y - 12}px`;
    }
  }
  return { el, render };
}
