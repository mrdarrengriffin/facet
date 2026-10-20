// Controls: value sweeps, slider, tick ring, arc, the light dial and the media disc. SVG is only used for what an
// LVGL panel draws natively (arcs and scale ticks); angles follow LVGL (0° = 3 o'clock, clockwise).
import { motion, screenType, size as Z, type as TYPE } from '@facet/tokens';
import { button } from './buttons.js';
import { arcPath, h, polar, reducedMotion, s } from './dom.js';

// ---------------------------------------------------------------- value sweep
/** CSS cubic-bezier(0, 0, 0.58, 1), the same curve as LVGL's ease-out path. */
const easeOut = (() => {
  const bx = (t: number) => 3 * (1 - t) * t * t * 0.58 + t * t * t;
  const by = (t: number) => 3 * (1 - t) * t * t + t * t * t;
  return (x: number) => {
    let lo = 0,
      hi = 1;
    for (let i = 0; i < 24; i++) {
      const m = (lo + hi) / 2;
      if (bx(m) < x) lo = m;
      else hi = m;
    }
    return by((lo + hi) / 2);
  };
})();

/**
 * An animated value. A change that isn't the user's own drag (an update from elsewhere, ON/OFF, opening the screen)
 * sweeps from the shown value to the new one, ease-out over `motion.duration.dial-sweep` (400 ms). A new target
 * mid-sweep carries on from where it is. `jump()` skips the animation (drags follow the finger).
 */
export function sweeper(render: (v: number) => void, initial = 0) {
  let shown = initial;
  let raf = 0;
  return {
    get value() {
      return shown;
    },
    jump(v: number) {
      cancelAnimationFrame(raf);
      raf = 0;
      shown = v;
      render(Math.round(v));
    },
    to(v: number, from?: number) {
      cancelAnimationFrame(raf);
      raf = 0;
      if (from !== undefined) shown = from;
      const a = shown;
      const dur = motion.duration['dial-sweep'];
      if (a === v || reducedMotion() || typeof requestAnimationFrame === 'undefined') {
        shown = v;
        render(Math.round(v));
        return;
      }
      const t0 = performance.now();
      const step = (t: number) => {
        const k = Math.min(1, (t - t0) / dur);
        shown = a + (v - a) * easeOut(k);
        render(Math.round(shown));
        raf = k < 1 ? requestAnimationFrame(step) : 0;
      };
      raf = requestAnimationFrame(step);
    },
  };
}

// ---------------------------------------------------------------- slider
export interface SliderProps {
  width: number;
  /** 0-100. */
  value: number;
  /** While dragging. */
  onInput?: (v: number) => void;
  /** On release. */
  onChange?: (v: number) => void;
  /** Every frame of a sweep (to keep a readout in step). */
  onSweep?: (v: number) => void;
}

/** Horizontal slider, 48 tall: a 4 px track, blue indicator and a 24 px orange knob. */
export function slider(o: SliderProps) {
  const ind = h('span', { class: 'f-slider-ind' });
  const knob = h('span', { class: 'f-slider-knob' });
  const el = h(
    'div',
    { class: 'f-slider', style: { width: `${o.width}px` } },
    h('span', { class: 'f-slider-track' }, ind),
    knob,
  );
  let value = o.value;
  let dragging = false;
  const paint = (v: number) => {
    ind.style.width = `${v}%`;
    knob.style.left = `${v}%`;
  };
  const sw = sweeper((v) => {
    paint(v);
    o.onSweep?.(v);
  }, o.value);
  const render = () => sw.jump(value);
  const fromEvent = (e: PointerEvent) => {
    const r = el.getBoundingClientRect();
    return Math.round(Math.min(100, Math.max(0, ((e.clientX - r.left) / r.width) * 100)));
  };
  el.addEventListener('pointerdown', (e) => {
    e.preventDefault(); // no text selection or native drag (a page selection over the slider would cancel the drag)
    dragging = true;
    el.setPointerCapture(e.pointerId);
    value = fromEvent(e);
    render();
    o.onInput?.(value);
  });
  el.addEventListener('pointermove', (e) => {
    if (!dragging) return;
    value = fromEvent(e);
    render();
    o.onInput?.(value);
  });
  const end = () => {
    if (!dragging) return;
    dragging = false;
    o.onChange?.(value);
  };
  el.addEventListener('pointerup', end);
  el.addEventListener('pointercancel', end);
  render();
  return Object.assign(el, {
    /** Jump to a value (no animation). */
    set(v: number) {
      if (!dragging) {
        value = v;
        render();
      }
    },
    /** Animate to a value changed elsewhere. */
    sweep(v: number) {
      if (!dragging && v !== value) {
        value = v;
        sw.to(v);
      }
    },
    get dragging() {
      return dragging;
    },
  });
}

// ---------------------------------------------------------------- tick ring and arc
export interface TickStyle {
  w: number;
  len: number;
  c: string;
}

/** A ring of ticks (LVGL scale ticks): `count` ticks over `range` degrees from `rotation`, drawn inward from the edge. */
export function tickRing(o: {
  size: number;
  count: number;
  range: number;
  rotation: number;
  minor: TickStyle;
  major?: TickStyle & { stride: number; labels?: (i: number) => string; gap?: number };
}) {
  const c = o.size / 2;
  const r = c;
  const svg = s('svg', {
    class: 'f-ticksvg',
    width: o.size,
    height: o.size,
    viewBox: `0 0 ${o.size} ${o.size}`,
    'aria-hidden': 'true',
  });
  const step = o.count > 1 ? o.range / (o.count - 1) : 0;
  for (let i = 0; i < o.count; i++) {
    const d = o.rotation + i * step;
    const maj = o.major && i % o.major.stride === 0;
    const t = maj ? o.major! : o.minor;
    const [x1, y1] = polar(c, c, r, d);
    const [x2, y2] = polar(c, c, r - t.len, d);
    svg.append(
      s('line', { x1, y1, x2, y2, stroke: `var(--facet-color-${t.c})`, 'stroke-width': t.w }),
    );
    if (maj && o.major!.labels) {
      const [lx, ly] = polar(c, c, r - t.len - (o.major!.gap ?? 12) - 10, d);
      svg.append(
        s(
          'text',
          {
            x: lx,
            y: ly,
            class: 'f-ticklabel ft-meta',
            'text-anchor': 'middle',
            'dominant-baseline': 'central',
          },
          o.major!.labels(i / o.major!.stride),
        ),
      );
    }
  }
  return svg;
}

/** An arc (LVGL `lv_arc`): optional track, an indicator with round caps, and an optional knob. `set(pct)`. */
export function arc(o: {
  size: number;
  start: number;
  end: number;
  track?: { w: number; c: string } | null;
  ind: { w: number; c: string };
  value: number;
  knob?: { size: number; border: number };
}) {
  const c = o.size / 2;
  const iw = o.ind.w;
  const tw = o.track?.w ?? 0;
  const svg = s('svg', {
    class: 'f-arc',
    width: o.size,
    height: o.size,
    viewBox: `0 0 ${o.size} ${o.size}`,
    'aria-hidden': 'true',
  });
  const span = (o.end - o.start + 360) % 360 || 360;
  if (o.track)
    svg.append(
      s('path', {
        d: arcPath(c, c, c - tw / 2, o.start, o.end === o.start ? o.start - 0.01 : o.end),
        fill: 'none',
        stroke: `var(--facet-color-${o.track.c})`,
        'stroke-width': tw,
      }),
    );
  const ind = s('path', {
    fill: 'none',
    stroke: `var(--facet-color-${o.ind.c})`,
    'stroke-width': iw,
    'stroke-linecap': 'round',
  });
  svg.append(ind);
  const knob = o.knob
    ? s('circle', {
        r: o.knob.size / 2 - o.knob.border / 2,
        fill: 'var(--facet-color-accent)',
        stroke: 'var(--facet-color-bg)',
        'stroke-width': o.knob.border,
      })
    : null;
  if (knob) svg.append(knob);
  const set = (v: number) => {
    const end = o.start + (span * Math.max(0, Math.min(100, v))) / 100;
    ind.setAttribute('d', v > 0 ? arcPath(c, c, c - iw / 2, o.start, end) : '');
    if (knob) {
      const [x, y] = polar(c, c, c - iw / 2, end);
      knob.setAttribute('cx', String(x));
      knob.setAttribute('cy', String(y));
    }
  };
  set(o.value);
  return Object.assign(svg, { set });
}

// ---------------------------------------------------------------- dial
export interface DialProps {
  /** 0-100. */
  value: number;
  on: boolean;
  /** Caption above the value. Default "Brightness". */
  caption?: string;
  /** Unit under the value. Default "%". */
  unit?: string;
  onInput?: (v: number) => void;
  onChange?: (v: number) => void;
  onToggle?: () => void;
}

/**
 * The light dial: a 560 square with a 0-100 scale, an adjustable 432 arc in the middle, and the caption, value
 * (display 144, light), unit and an ON/OFF button stacked on the centre. Drag the ring to change the value; the
 * centre stays free for the button. Changes from elsewhere sweep the arc.
 */
export function dial(o: DialProps) {
  const START = 135,
    RANGE = 270;
  const S = Z['dial-scale'],
    A = Z['dial-arc'],
    C = S / 2;
  const scale = tickRing({
    size: S,
    count: 51,
    range: RANGE,
    rotation: START,
    minor: { w: 2, len: 10, c: 'line-hi' },
    major: { stride: 5, w: 2, len: 18, c: 'blue', labels: (i) => String(i * 10), gap: 12 },
  });
  const a = arc({
    size: A,
    start: START,
    end: START + RANGE,
    track: { w: 2, c: 'line' },
    ind: { w: 6, c: 'accent' },
    value: o.value,
    knob: { size: 26, border: 4 },
  });
  const VAL = screenType.display.box!,
    valTop = C - VAL / 2;
  const row = (top: number, hh: number) => ({
    left: '0px',
    width: `${S}px`,
    top: `${top}px`,
    height: `${hh}px`,
  });
  const val = h(
    'span',
    { class: 'f-dial-value fst-display', style: row(valTop, VAL) },
    String(o.value),
  );
  const TW = Z['dial-toggle-w'],
    TH = Z['button-h'];
  const toggle = button({
    kind: o.on ? 'outline' : 'secondary',
    text: o.on ? 'ON' : 'OFF',
    width: TW,
    onTap: () => o.onToggle?.(),
  });
  const CAP = TYPE.meta.box!,
    UNIT = TYPE.subtitle.box!;
  const el = h(
    'div',
    { class: 'f-dial', style: { width: `${S}px`, height: `${S}px` } },
    h('div', { class: 'f-dial-scale' }, scale),
    h(
      'div',
      {
        class: 'f-dial-arc',
        style: {
          left: `${(S - A) / 2}px`,
          top: `${(S - A) / 2}px`,
          width: `${A}px`,
          height: `${A}px`,
        },
      },
      a,
    ),
    h(
      'span',
      { class: 'f-dial-cap', style: row(valTop - 8 - 2 * CAP, CAP) },
      o.caption ?? 'Brightness',
    ),
    val,
    h('span', { class: 'f-dial-unit', style: row(valTop + VAL + 8, UNIT) }, o.unit ?? '%'),
    h(
      'div',
      {
        class: 'f-dial-toggle',
        style: {
          left: `${(S - TW) / 2}px`,
          top: `${valTop + VAL + 8 + UNIT + 32}px`,
          width: `${TW}px`,
          height: `${TH}px`,
        },
      },
      toggle,
    ),
  );
  const hit = el.querySelector<HTMLElement>('.f-dial-arc')!;
  let value = o.value;
  let dragging = false;
  const sw = sweeper((v) => {
    a.set(v);
    val.textContent = String(v);
  }, o.value);
  const fromEvent = (e: PointerEvent) => {
    const r = hit.getBoundingClientRect();
    const k = r.width / A;
    const dx = (e.clientX - r.left) / k - A / 2,
      dy = (e.clientY - r.top) / k - A / 2;
    const dist = Math.hypot(dx, dy);
    let rel = ((Math.atan2(dy, dx) * 180) / Math.PI - START + 720) % 360;
    if (rel > RANGE) rel = rel > RANGE + (360 - RANGE) / 2 ? 0 : RANGE;
    return { v: Math.round((rel / RANGE) * 100), dist };
  };
  hit.addEventListener('pointerdown', (e) => {
    const p = fromEvent(e);
    if (p.dist < 150) return; // the centre stays free for the value and the ON/OFF button
    e.preventDefault(); // no text selection or native drag, as on the slider
    dragging = true;
    hit.setPointerCapture(e.pointerId);
    value = p.v;
    sw.jump(value);
    o.onInput?.(value);
  });
  hit.addEventListener('pointermove', (e) => {
    if (!dragging) return;
    value = fromEvent(e).v;
    sw.jump(value);
    o.onInput?.(value);
  });
  const end = () => {
    if (!dragging) return;
    dragging = false;
    o.onChange?.(value);
  };
  hit.addEventListener('pointerup', end);
  hit.addEventListener('pointercancel', end);
  return Object.assign(el, {
    /** A change from elsewhere: sweeps to the value (off sweeps down to 0). `fromZero` = opening the screen. */
    set(st: { value: number; on: boolean }, fromZero = false) {
      if (!dragging && (st.value !== value || fromZero)) {
        value = st.value;
        sw.to(value, fromZero ? 0 : undefined);
      }
      toggle.className = `f-btn f-btn--${st.on ? 'outline' : 'secondary'} f-btn--fixed live`;
      toggle.querySelector('.f-btn-label')!.textContent = st.on ? 'ON' : 'OFF';
    },
    get dragging() {
      return dragging;
    },
  });
}

// ---------------------------------------------------------------- thermostat dial
export interface ThermostatDialProps {
  min: number;
  max: number;
  step: number;
  target: number | null;
  current: number | null;
  /** Dragging the ring: every step. */
  onInput?: (t: number) => void;
  /** The drag ended. */
  onChange?: (t: number) => void;
}

/**
 * The thermostat ring: a 480 square with a tick a degree (longer every 5), a 416 arc track, the set point as the
 * knob and the measured temperature as a long tick in the text colour. While it heats, the arc from the measured
 * temperature up to the set point is orange (cooling: blue, from the set point up). Drag the ring to move the set
 * point in `step`s; the centre (r < 150) stays free for the screen's text and buttons. Off = no knob.
 */
export function thermostatDial(o: ThermostatDialProps) {
  const START = 135,
    RANGE = 270;
  const S = Z['thermo-scale'],
    A = Z['thermo-arc'],
    C = S / 2,
    IW = 6,
    R = A / 2 - IW / 2;
  const deg = (t: number) =>
    START + (RANGE * (Math.min(o.max, Math.max(o.min, t)) - o.min)) / (o.max - o.min);
  const svg = s('svg', {
    class: 'f-thermo-svg',
    width: S,
    height: S,
    viewBox: `0 0 ${S} ${S}`,
    'aria-hidden': 'true',
  });
  // a tick a degree, drawn inward from the edge; whole multiples of 5 are majors
  for (let t = Math.ceil(o.min); t <= o.max; t++) {
    const maj = t % 5 === 0;
    const [x1, y1] = polar(C, C, C, deg(t));
    const [x2, y2] = polar(C, C, C - (maj ? 18 : 10), deg(t));
    svg.append(
      s('line', {
        x1,
        y1,
        x2,
        y2,
        stroke: `var(--facet-color-${maj ? 'blue' : 'line-hi'})`,
        'stroke-width': 2,
      }),
    );
  }
  svg.append(
    s('path', {
      d: arcPath(C, C, A / 2 - 1, START, START + RANGE),
      fill: 'none',
      stroke: 'var(--facet-color-line)',
      'stroke-width': 2,
    }),
  );
  const span = s('path', { fill: 'none', 'stroke-width': IW, 'stroke-linecap': 'round' });
  const now = s('line', {
    stroke: 'var(--facet-color-text)',
    'stroke-width': 4,
    'stroke-linecap': 'round',
  });
  const knob = s('circle', {
    r: 13 - 2,
    fill: 'var(--facet-color-accent)',
    stroke: 'var(--facet-color-bg)',
    'stroke-width': 4,
  });
  svg.append(span, now, knob);
  const el = h('div', { class: 'f-thermo', style: { width: `${S}px`, height: `${S}px` } }, svg);

  let st: { target: number | null; current: number | null; active: 'heat' | 'cool' | null } = {
    target: o.target,
    current: o.current,
    active: null,
  };
  // the knob sweeps in whole steps (the sweeper works in integers)
  const idx = (t: number) => Math.round((t - o.min) / o.step);
  const val = (i: number) => +(o.min + i * o.step).toFixed(2);
  let shown = st.target;
  const draw = () => {
    const t = shown,
      c = st.current;
    knob.style.display = t === null ? 'none' : '';
    if (t !== null) {
      const [x, y] = polar(C, C, R, deg(t));
      knob.setAttribute('cx', String(x));
      knob.setAttribute('cy', String(y));
    }
    now.style.display = c === null ? 'none' : '';
    if (c !== null) {
      const [x1, y1] = polar(C, C, C, deg(c));
      const [x2, y2] = polar(C, C, C - 30, deg(c));
      Object.entries({ x1, y1, x2, y2 }).forEach(([k, v]) => now.setAttribute(k, String(v)));
    }
    const heat = st.active === 'heat' && t !== null && c !== null && t > c;
    const cool = st.active === 'cool' && t !== null && c !== null && t < c;
    span.setAttribute(
      'd',
      heat ? arcPath(C, C, R, deg(c!), deg(t!)) : cool ? arcPath(C, C, R, deg(t!), deg(c!)) : '',
    );
    span.setAttribute('stroke', `var(--facet-color-${heat ? 'accent' : 'blue'})`);
  };
  const sw = sweeper(
    (i) => {
      shown = val(i);
      draw();
    },
    idx(o.target ?? o.min),
  );
  draw();

  let dragging = false,
    value = o.target ?? o.min;
  const fromEvent = (e: PointerEvent) => {
    const r = svg.getBoundingClientRect();
    const k = r.width / S;
    const dx = (e.clientX - r.left) / k - C,
      dy = (e.clientY - r.top) / k - C;
    let rel = ((Math.atan2(dy, dx) * 180) / Math.PI - START + 720) % 360;
    if (rel > RANGE) rel = rel > RANGE + (360 - RANGE) / 2 ? 0 : RANGE;
    return {
      t: val(Math.round(((rel / RANGE) * (o.max - o.min)) / o.step)),
      dist: Math.hypot(dx, dy),
    };
  };
  el.addEventListener('pointerdown', (e) => {
    const p = fromEvent(e);
    if (p.dist < 150 || st.target === null) return; // the centre stays free; off = nothing to set
    e.preventDefault();
    dragging = true;
    el.setPointerCapture(e.pointerId);
    value = p.t;
    sw.jump(idx(value));
    o.onInput?.(value);
  });
  el.addEventListener('pointermove', (e) => {
    if (!dragging) return;
    const t = fromEvent(e).t;
    if (t === value) return;
    value = t;
    sw.jump(idx(value));
    o.onInput?.(value);
  });
  const end = () => {
    if (!dragging) return;
    dragging = false;
    o.onChange?.(value);
  };
  el.addEventListener('pointerup', end);
  el.addEventListener('pointercancel', end);
  return Object.assign(el, {
    /** New state from elsewhere: the knob sweeps to the set point (`fromMin` = opening the screen). */
    set(next: typeof st, fromMin = false) {
      const moved = next.target !== st.target;
      st = next;
      if (dragging) return draw();
      if (next.target === null) {
        shown = null;
        return draw();
      }
      if (shown === null) shown = next.target;
      if (moved || fromMin) sw.to(idx(next.target), fromMin ? 0 : idx(shown));
      else draw();
      value = next.target;
    },
    get dragging() {
      return dragging;
    },
  });
}

// ---------------------------------------------------------------- media disc
/**
 * The media disc: a dotted ring (72 dots), an orange progress arc from 12 o'clock, a disc face with two grooves, an
 * orange label spot with a hole, and a blue marker circling the face (8 s a turn, only while playing).
 * `size` is a multiple of 16 so every ring lands on the grid (default 320; screens use 256).
 */
export function disc(o: { progress: number; playing: boolean; size?: number }) {
  const D = o.size ?? Z.disc;
  const k = D / Z.disc;
  const r16 = (v: number) => Math.round(v / 16) * 16;
  const PR = D - (Z.disc - Z['disc-prog']);
  const F = o.size ? r16(Z['disc-face'] * k) : Z['disc-face'];
  const SPOT = o.size ? r16(64 * k) : 64;
  const ORB = F / 2 - 16;
  const dots = tickRing({
    size: D,
    count: 72,
    range: 355,
    rotation: 270,
    minor: { w: 3, len: 3, c: 'line-hi' },
  });
  const prog = arc({
    size: PR,
    start: 270,
    end: 269,
    track: null,
    ind: { w: 3, c: 'accent' },
    value: o.progress,
  });
  const el = h(
    'div',
    { class: 'f-disc', style: { width: `${D}px`, height: `${D}px` } },
    h('div', { class: 'f-disc-dots' }, dots),
    h(
      'div',
      { class: 'f-disc-prog', style: { left: `${(D - PR) / 2}px`, top: `${(D - PR) / 2}px` } },
      prog,
    ),
    h(
      'div',
      {
        class: 'f-disc-face',
        style: {
          left: `${(D - F) / 2}px`,
          top: `${(D - F) / 2}px`,
          width: `${F}px`,
          height: `${F}px`,
        },
      },
      h('i', { class: 'f-disc-groove', style: { width: `${F - 48}px`, height: `${F - 48}px` } }),
      h('i', { class: 'f-disc-groove', style: { width: `${F - 96}px`, height: `${F - 96}px` } }),
      h(
        'i',
        {
          class: 'f-disc-spot',
          style: { width: `${SPOT}px`, height: `${SPOT}px`, '--hole': `${(SPOT - 16) / 2}px` },
        },
        h('i', { class: 'f-disc-hole' }),
      ),
      h(
        'i',
        { class: 'f-disc-orbit' },
        h('i', { class: 'f-disc-marker', style: { top: `${-ORB - 5}px` } }),
      ),
    ),
  );
  el.classList.toggle('is-playing', o.playing);
  return Object.assign(el, {
    set(st: { progress: number; playing: boolean }) {
      prog.set(st.progress);
      el.classList.toggle('is-playing', st.playing);
    },
  });
}
