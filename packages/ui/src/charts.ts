// Charts, in the panel-safe subset: SVG rects, lines and arcs with solid token colours; motion is transform or
// opacity on the marks. Each maps to an LVGL widget: bar chart = lv_chart bars (or rects), sparkline = a line series
// with an end dot, big number = a label (count-up = an animation on its value), rate strip = a row of rects, donut =
// one arc per part, axis = labels at fixed x. Colours are token names ('accent', 'blue', 'band-normal' ...). Text
// never takes a series colour.
import { motion } from '@facet/tokens';
import { fit8, setText, statusChip } from './atoms.js';
import { arcPath, h, reducedMotion, s } from './dom.js';
import type { Band, Tone } from './types.js';

export type { Band };
const EASE = motion.easing['ease-out'].css;

/** Rounded-top bar path (data end rounded, baseline square), x/y = top-left. */
function barPath(x: number, y: number, w: number, hh: number, r: number) {
  if (hh <= 0) return '';
  const rr = Math.min(r, w / 2, hh);
  return `M${x} ${y + hh} V${y + rr} Q${x} ${y} ${x + rr} ${y} H${x + w - rr} Q${x + w} ${y} ${x + w} ${y + rr} V${y + hh} Z`;
}

export type Bar = { v: number | null; c: string; ghost?: boolean };

/** Bar chart: equal slots across `w`, bars anchored to the baseline at the bottom, `gap` px between bars (2 = the
 *  surface spacer), radius on the data end. `null` = no data (empty slot); ghost = outline-only placeholder
 *  (e.g. future slots). `now` = index of a dotted marker line. play() = bars grow from the baseline with a stagger.
 *  onTap(i) with a slot-wide, full-height hit target (bigger than the mark). */
export function barChart(o: {
  w: number;
  h: number;
  bars: Bar[];
  max?: number;
  gap?: number;
  radius?: number;
  now?: number | null;
  baseline?: boolean;
  onTap?: (i: number) => void;
  minH?: number;
  group?: number;
  groupGap?: number;
}) {
  const svg = s('svg', {
    class: 'fch-bars',
    width: o.w,
    height: o.h,
    viewBox: `0 0 ${o.w} ${o.h}`,
    'aria-hidden': 'true',
  });
  const g = s('g');
  svg.append(g);
  let bars = o.bars,
    sel = -1;
  const marks: SVGElement[] = [];
  const render = () => {
    g.replaceChildren();
    marks.length = 0;
    // grouped bars (e.g. pairs this week / last week): `group` bars per group, `groupGap` px between groups
    const n = Math.max(1, bars.length),
      gs = o.group ?? 1,
      ng = Math.ceil(n / gs),
      GG = gs > 1 ? (o.groupGap ?? 8) : 0;
    const unit = (o.w - (ng - 1) * GG) / ng,
      slot = unit / gs,
      gap = o.gap ?? 2,
      bw = Math.max(1, slot - gap);
    const xOf = (i: number) => Math.floor(i / gs) * (unit + GG) + (i % gs) * slot;
    const max = o.max ?? Math.max(1e-9, ...bars.map((b) => b.v ?? 0));
    if (o.baseline !== false)
      g.append(
        s('line', {
          x1: 0,
          x2: o.w,
          y1: o.h - 0.5,
          y2: o.h - 0.5,
          stroke: 'var(--facet-color-line)',
          'stroke-width': 1,
        }),
      );
    bars.forEach((b, i) => {
      const x = xOf(i) + gap / 2;
      if (b.v === null) {
        marks.push(s('g'));
        return;
      }
      const hh = Math.max(o.minH ?? 2, (b.v / max) * (o.h - 1));
      const p = b.ghost
        ? s('path', {
            class: 'fch-bar fch-bar--ghost',
            d: barPath(x + 0.5, o.h - 1 - hh + 0.5, bw - 1, hh - 0.5, o.radius ?? 2),
            fill: 'none',
            stroke: `var(--facet-color-${b.c})`,
            'stroke-width': 1,
            'stroke-dasharray': '2 2',
          })
        : s('path', {
            class: 'fch-bar',
            d: barPath(x, o.h - 1 - hh, bw, hh, o.radius ?? 2),
            fill: `var(--facet-color-${b.c})`,
          });
      if (i === sel) p.classList.add('is-sel');
      g.append(p);
      marks.push(p);
    });
    if (o.now !== undefined && o.now !== null && o.now >= 0) {
      const x = Math.round(xOf(o.now) + slot / 2) + 0.5;
      g.append(
        s('line', {
          class: 'fch-now',
          x1: x,
          x2: x,
          y1: 0,
          y2: o.h - 1,
          stroke: 'var(--facet-color-text-dim)',
          'stroke-width': 1,
          'stroke-dasharray': '1 3',
        }),
      );
    }
    if (o.onTap)
      bars.forEach((_, i) => {
        const r = s('rect', {
          class: 'fch-hit',
          x: xOf(i),
          y: 0,
          width: slot,
          height: o.h,
          fill: 'transparent',
        });
        r.addEventListener('click', (e) => {
          e.stopPropagation();
          sel = sel === i ? -1 : i;
          marks.forEach((m, k) => m.classList.toggle('is-sel', k === sel));
          o.onTap!(sel);
        });
        g.append(r);
      });
  };
  render();
  return Object.assign(svg, {
    set(next: Bar[], now?: number | null) {
      bars = next;
      if (now !== undefined) o.now = now;
      render();
    },
    /** grow from the baseline, `stagger` ms apart (only the bars move: transform on small objects) */
    play(stagger = 12, duration = 420) {
      if (reducedMotion()) return;
      marks.forEach((m, i) =>
        m.animate?.([{ transform: 'scaleY(0)' }, { transform: 'scaleY(1)' }], {
          duration,
          delay: i * stagger,
          easing: EASE,
          fill: 'backwards',
        }),
      );
    },
  });
}

/** Sparkline: 2 px line over `values` (oldest first), 8 px dot on the last point; optional faint baseline. */
export function sparkline(o: {
  w: number;
  h: number;
  values: number[];
  c?: string;
  min?: number;
  max?: number;
  dot?: boolean;
}) {
  const svg = s('svg', {
    class: 'fch-spark',
    width: o.w,
    height: o.h,
    viewBox: `0 0 ${o.w} ${o.h}`,
    'aria-hidden': 'true',
  });
  const line = s('polyline', {
    fill: 'none',
    stroke: `var(--facet-color-${o.c ?? 'blue'})`,
    'stroke-width': 2,
    'stroke-linejoin': 'round',
    'stroke-linecap': 'round',
  });
  const dot = s('circle', { r: 4, fill: `var(--facet-color-${o.c ?? 'blue'})` });
  svg.append(
    s('line', {
      x1: 0,
      x2: o.w,
      y1: o.h - 0.5,
      y2: o.h - 0.5,
      stroke: 'var(--facet-color-line)',
      'stroke-width': 1,
      'stroke-dasharray': '1 3',
    }),
    line,
  );
  if (o.dot !== false) svg.append(dot);
  const set = (vals: number[]) => {
    if (vals.length < 2) {
      line.setAttribute('points', '');
      dot.setAttribute('r', '0');
      return;
    }
    const lo = o.min ?? Math.min(...vals),
      hi = o.max ?? Math.max(...vals),
      span = hi - lo || 1,
      pad = 4;
    const pts = vals.map((v, i) => [
      pad + (i / (vals.length - 1)) * (o.w - 2 * pad),
      pad + (1 - (v - lo) / span) * (o.h - 2 * pad),
    ]);
    line.setAttribute('points', pts.map((p) => p.map((n) => n.toFixed(1)).join(',')).join(' '));
    const [x, y] = pts[pts.length - 1];
    dot.setAttribute('cx', String(x));
    dot.setAttribute('cy', String(y));
    dot.setAttribute('r', '4');
  };
  set(o.values);
  return Object.assign(svg, { set });
}

/** Big number: caps label (16 box) + value in `type` (default 'number') + unit; count-up from 0 on play(). */
export function bigNumber(o: {
  label: string;
  value: number | null;
  decimals?: number;
  unit?: string;
  prefix?: string;
  type?: string;
  width: number;
}) {
  const val = fit8(h('span', { class: `fch-big-v ft-${o.type ?? 'number'}` }, '--'));
  const unit = h('span', { class: 'fch-big-u ft-subtitle' }, o.unit ?? '');
  const el = h(
    'div',
    { class: 'fch-big', style: { width: `${o.width}px` } },
    h('span', { class: 'fch-big-l ft-caps-sm' }, o.label),
    h('div', { class: 'fch-big-row' }, val, unit),
  );
  let cur = o.value,
    anim = 0;
  const fmt = (v: number | null) =>
    v === null ? '--' : `${o.prefix ?? ''}${v.toFixed(o.decimals ?? 0)}`;
  const show = (v: number | null) => {
    setText(val, fmt(v));
  };
  show(cur);
  return Object.assign(el, {
    set(v: number | null, u?: string) {
      cur = v;
      if (!anim) show(v);
      if (u !== undefined) unit.textContent = u;
    },
    /** count up 0 -> value (LVGL: lv_anim on the label's value, ease-out) */
    play(duration = 700) {
      if (cur === null || reducedMotion()) return show(cur);
      cancelAnimationFrame(anim);
      const t0 = performance.now(),
        target = cur;
      const step = (t: number) => {
        const k = Math.min(1, (t - t0) / duration),
          e = 1 - Math.pow(1 - k, 3);
        show(target * e);
        if (k < 1) anim = requestAnimationFrame(step);
        else {
          anim = 0;
          show(cur);
        }
      };
      anim = requestAnimationFrame(step);
    },
  });
}

/** Delta chip: ▲/▼ + percentage vs a reference, tone by whether up is good. */
export function deltaChip(o: { pct: number | null; ref: string; upIsGood?: boolean }) {
  const chip = statusChip('idle', '--');
  const set = (pct: number | null, ref = o.ref) => {
    let tone: Tone = 'idle',
      txt = `-- ${ref}`;
    if (pct !== null && Number.isFinite(pct)) {
      const up = pct >= 0.5,
        down = pct <= -0.5;
      tone = !up && !down ? 'idle' : up === !!o.upIsGood ? 'ok' : 'warn';
      txt = `${up ? '▲' : down ? '▼' : '='} ${Math.abs(Math.round(pct))}% ${ref}`;
    }
    chip.dataset.tone = tone;
    chip.querySelector('.f-dot')!.setAttribute('data-tone', tone);
    const t = chip.querySelector('.f-chip-text .f-fit') ?? chip.querySelector('.f-chip-text')!;
    t.textContent = txt;
  };
  set(o.pct);
  return Object.assign(chip, { set });
}

export const BAND_COLOUR: Record<Band, string> = {
  cheap: 'blue',
  normal: 'band-normal',
  peak: 'accent',
};

/** Rate band strip: one rect per run of equal band across `w` (slots equal width), 2 px gap between runs. */
export function rateStrip(o: { w: number; h?: number; bands: Band[]; now?: number | null }) {
  const H = o.h ?? 8;
  const svg = s('svg', {
    class: 'fch-strip',
    width: o.w,
    height: H,
    viewBox: `0 0 ${o.w} ${H}`,
    'aria-hidden': 'true',
  });
  const set = (bands: Band[], now?: number | null) => {
    svg.replaceChildren();
    const n = Math.max(1, bands.length),
      slot = o.w / n;
    let i = 0;
    while (i < bands.length) {
      let j = i;
      while (j + 1 < bands.length && bands[j + 1] === bands[i]) j++;
      const x0 = i * slot + (i ? 1 : 0),
        x1 = (j + 1) * slot - (j < bands.length - 1 ? 1 : 0);
      svg.append(
        s('rect', {
          x: x0,
          y: 0,
          width: Math.max(1, x1 - x0),
          height: H,
          rx: 2,
          fill: `var(--facet-color-${BAND_COLOUR[bands[i]]})`,
        }),
      );
      i = j + 1;
    }
    if (now !== undefined && now !== null && now >= 0)
      svg.append(
        s('rect', {
          class: 'fch-strip-now',
          x: (now + 0.5) * slot - 1,
          y: 0,
          width: 2,
          height: H,
          fill: 'var(--facet-color-text)',
        }),
      );
  };
  set(o.bands, o.now);
  return Object.assign(svg, { set });
}

/** Donut / ring: one arc per part (LVGL lv_arc each), clockwise from 12 o'clock, 3 deg gaps, thickness `t`. */
export function donut(o: {
  size: number;
  t?: number;
  parts: { v: number; c: string }[];
  track?: boolean;
}) {
  const T = o.t ?? 16,
    C = o.size / 2,
    R = C - T / 2;
  const svg = s('svg', {
    class: 'fch-donut',
    width: o.size,
    height: o.size,
    viewBox: `0 0 ${o.size} ${o.size}`,
    'aria-hidden': 'true',
  });
  const set = (parts: { v: number; c: string }[]) => {
    svg.replaceChildren();
    if (o.track !== false)
      svg.append(
        s('circle', {
          cx: C,
          cy: C,
          r: R,
          fill: 'none',
          stroke: 'var(--facet-color-line)',
          'stroke-width': 1,
        }),
      );
    const total = parts.reduce((a, p) => a + Math.max(0, p.v), 0);
    if (!total) return;
    const live = parts.filter((p) => p.v > 0),
      GAP = live.length > 1 ? 3 : 0;
    let a = 270;
    for (const p of live) {
      const sweep = (p.v / total) * 360;
      const d = arcPath(C, C, R, a + GAP / 2, a + Math.max(GAP / 2 + 0.5, sweep - GAP / 2));
      svg.append(
        s('path', {
          class: 'fch-arc',
          d,
          fill: 'none',
          stroke: `var(--facet-color-${p.c})`,
          'stroke-width': T,
        }),
      );
      a += sweep;
    }
  };
  set(o.parts);
  return Object.assign(svg, {
    set,
    /** arcs fade in one after another (opacity only) */
    play(stagger = 90) {
      if (reducedMotion()) return;
      svg.querySelectorAll('.fch-arc').forEach((p, i) =>
        (p as SVGElement).animate([{ opacity: 0 }, { opacity: 1 }], {
          duration: 240,
          delay: 200 + i * stagger,
          easing: EASE,
          fill: 'backwards',
        }),
      );
    },
  });
}

/** Axis labels: text at fractions 0..1 of `w` (centred on the point; the first/last are edge-aligned). */
export function axis(o: { w: number; ticks: { at: number; text: string }[]; type?: string }) {
  const el = h('div', {
    class: `fch-axis ft-${o.type ?? 'meta-sm'}`,
    style: { width: `${o.w}px` },
  });
  for (const t of o.ticks) {
    const edge = t.at <= 0 ? 'start' : t.at >= 1 ? 'end' : 'mid';
    el.append(
      h(
        'span',
        { class: `fch-tick fch-tick--${edge}`, style: { left: `${t.at * o.w}px` } },
        t.text,
      ),
    );
  }
  return el;
}

/** Legend row: swatch (8 px square, radius 2) + text in text tokens, one per entry. */
export function legend(items: { c: string; text: string; ghost?: boolean }[]) {
  // each entry is content-sized on the 8 px grid (fit8), so a right-aligned legend keeps every edge on the grid
  return h(
    'div',
    { class: 'fch-legend ft-meta-sm' },
    ...items.map((it) =>
      fit8(
        h(
          'span',
          { class: 'fch-leg' },
          h('i', {
            class: `fch-sw ${it.ghost ? 'fch-sw--ghost' : ''}`,
            style: { '--sw': `var(--facet-color-${it.c})` } as any,
          }),
          it.text,
        ),
      ),
    ),
  );
}

/** Price band names, for labels. Bands always come with a legend or a label, never colour alone. */
export const BAND_NAME: Record<Band, string> = { cheap: 'cheap', normal: 'normal', peak: 'peak' };
