// Covers: the window drawing for blinds and curtains, and the helpers that read a cover. Plain boxes only (fills, 1-2 px
// borders, radius), so the firmware can draw the same thing with LVGL objects. Positions are Home Assistant's:
// 0 = closed, 100 = open.
import { h } from './dom.js';
import { sweeper } from './controls.js';
import type { CoverData, CoverKind } from './types.js';

/** `supported_features` bits of a Home Assistant cover. */
export const COVER = {
  open: 1,
  close: 2,
  position: 4,
  stop: 8,
  tiltOpen: 16,
  tiltClose: 32,
  tiltStop: 64,
  tilt: 128,
} as const;

/** What a cover can do, from its feature bits (default 15: open, close, position, stop). */
export function coverCaps(c: Pick<CoverData, 'features'>) {
  const f = c.features ?? 15;
  return {
    open: !!(f & COVER.open),
    close: !!(f & COVER.close),
    position: !!(f & COVER.position),
    stop: !!(f & COVER.stop),
    tilt: !!(f & COVER.tilt),
  };
}

/** How to draw a cover: `kind` if set, else curtain for curtains, Venetian for anything that tilts, else a roller. */
export function coverKind(c: Pick<CoverData, 'kind' | 'deviceClass' | 'features'>): CoverKind {
  if (c.kind) return c.kind;
  if (c.deviceClass === 'curtain') return 'curtain';
  if (coverCaps(c).tilt) return 'venetian';
  return 'roller';
}

export const COVER_KIND_TEXT: Record<CoverKind, string> = {
  roller: 'roller blind',
  curtain: 'curtains',
  venetian: 'Venetian blind',
};

/** The position to draw: the real one, else 100 / 0 from the state. */
export const coverPos = (c: CoverData) =>
  c.position ?? (c.state === 'open' || c.state === 'closing' ? 100 : 0);

/** "Closed", "Open", "42%". */
export function coverValue(c: CoverData | null | undefined) {
  if (!c || c.state === 'unavailable') return '--';
  const p = coverPos(c);
  return p <= 0 ? 'Closed' : p >= 100 ? 'Open' : `${Math.round(p)}%`;
}

/** The status words: "opening", "closed", "42% open", "offline". */
export function coverStatus(c: CoverData) {
  if (c.state === 'unavailable') return 'offline';
  if (c.state === 'opening' || c.state === 'closing') return c.state;
  const p = coverPos(c);
  return p <= 0 ? 'closed' : p >= 100 ? 'open' : `${Math.round(p)}% open`;
}

export const coverMoving = (c: CoverData | null | undefined) =>
  c?.state === 'opening' || c?.state === 'closing';

const clamp = (v: number) => Math.round(Math.min(100, Math.max(0, v)));
const box = (cls: string) => h('i', { class: cls });

export interface CoverVisualProps {
  kind: CoverKind;
  width: number;
  height: number;
  /** 0-100, 0 = closed. */
  position: number;
  /** 0-100 (Venetian), 100 = slats flat (open). */
  tilt?: number;
  /** Drag the blind's edge (or a curtain's leading edge) to a position. Off when the cover has no position. */
  draggable?: boolean;
  onInput?: (v: number) => void;
  onChange?: (v: number) => void;
}

/**
 * A window with a cover in it. Roller: fabric drops from the head rail, the bottom bar is the handle. Venetian: slats
 * every 24 px down to the bottom rail; tilt makes them thinner (open) or thicker until they close up. Curtains: two
 * panels hang from a rod and part from the middle, pleats bunching as they open. Drag vertically on a blind (the edge
 * follows the finger), sideways on curtains. `set()` jumps or sweeps, `setTarget()` shows where a moving cover is going.
 */
export function coverVisual(o: CoverVisualProps) {
  const W = o.width,
    H = o.height,
    P = 16; // frame to glass
  const GW = W - 2 * P,
    GH = H - 2 * P;
  const SLAT = 24;
  const glass = h('div', { class: 'f-cover-glass' }, box('f-cover-mull-v'), box('f-cover-mull-h'));
  const cover = h('div', { class: 'f-cover-cover' });
  const target = box('f-cover-target');
  target.hidden = true;
  const el = h(
    'div',
    {
      class: 'f-cover',
      'data-kind': o.kind,
      style: { width: `${W}px`, height: `${H}px`, '--cv-pad': `${P}px` },
    },
    h('div', { class: 'f-cover-frame' }, glass),
    cover,
    target,
  );

  // the parts per kind
  let paint: (p: number) => void;
  let paintTilt: (t: number) => void = () => {};
  if (o.kind === 'curtain') {
    const MIN = 24; // a panel never folds narrower than this
    const pleats = (n: number) =>
      Array.from({ length: n }, (_, i) =>
        h('i', { class: 'f-cover-pleat', style: { left: `${((i + 1) / (n + 1)) * 100}%` } }),
      );
    const left = h('div', { class: 'f-cover-panel is-left' }, ...pleats(5), box('f-cover-edge'));
    const right = h('div', { class: 'f-cover-panel is-right' }, ...pleats(5), box('f-cover-edge'));
    cover.append(box('f-cover-rod'), left, right);
    paint = (p) => {
      const w = MIN + (W / 2 - MIN) * (1 - p / 100);
      left.style.width = right.style.width = `${w}px`;
    };
  } else {
    const fabric = h('div', { class: 'f-cover-fabric' });
    const bar = box('f-cover-bar');
    const slats: HTMLElement[] = [];
    if (o.kind === 'venetian') {
      fabric.append(box('f-cover-cord is-l'), box('f-cover-cord is-r'));
      for (let y = SLAT; y < GH; y += SLAT) {
        const s = h('i', { class: 'f-cover-slat', style: { top: `${y}px` } });
        slats.push(s);
        fabric.append(s);
      }
      paintTilt = (t) => {
        // flat = 4 px, shut = the full pitch (they overlap into a wall)
        const th = 4 + ((SLAT - 4) * (100 - t)) / 100;
        for (const s of slats) {
          s.style.height = `${th}px`;
          s.style.marginTop = `${-th / 2}px`;
        }
      };
    }
    cover.append(box('f-cover-rail'), fabric, bar);
    paint = (p) => {
      // closed = the fabric reaches the sill; a Venetian blind keeps a stack of slats under the rail
      const min = o.kind === 'venetian' ? 16 : 0;
      const fh = min + (GH - min) * (1 - p / 100);
      fabric.style.height = `${fh}px`;
      bar.style.top = `${P + fh}px`;
    };
  }

  const yOf = (p: number) => P + GH * (1 - p / 100);
  const xOf = (p: number) => 24 + (W / 2 - 24) * (1 - p / 100);
  let value = clamp(o.position),
    tilt = clamp(o.tilt ?? 50),
    dragging = false;
  const sw = sweeper(paint, value);
  const tw = sweeper(paintTilt, tilt);
  sw.jump(value);
  tw.jump(tilt);

  const fromEvent = (e: PointerEvent) => {
    const r = el.getBoundingClientRect();
    const sx = r.width / W,
      sy = r.height / H; // the screen may be scaled (docs previews)
    if (o.kind === 'curtain') {
      const d = Math.abs((e.clientX - r.left) / sx - W / 2);
      return clamp(((d - 24) / (W / 2 - 24)) * 100);
    }
    return clamp(100 - (((e.clientY - r.top) / sy - P) / GH) * 100);
  };
  if (o.draggable !== false) {
    el.classList.add('is-drag');
    el.addEventListener('pointerdown', (e) => {
      e.preventDefault();
      dragging = true;
      el.setPointerCapture(e.pointerId);
      value = fromEvent(e);
      sw.jump(value);
      o.onInput?.(value);
    });
    el.addEventListener('pointermove', (e) => {
      if (!dragging) return;
      value = fromEvent(e);
      sw.jump(value);
      o.onInput?.(value);
    });
    const end = () => {
      if (!dragging) return;
      dragging = false;
      o.onChange?.(value);
    };
    el.addEventListener('pointerup', end);
    el.addEventListener('pointercancel', end);
  }

  return Object.assign(el, {
    /** Show a position (and tilt): sweeps unless `instant`. Ignored mid-drag. */
    set(st: { position?: number; tilt?: number }, instant = false) {
      if (st.position !== undefined && !dragging) {
        const v = clamp(st.position);
        if (instant) sw.jump(v);
        else if (v !== value) sw.to(v);
        value = v;
      }
      if (st.tilt !== undefined) {
        const t = clamp(st.tilt);
        if (instant) tw.jump(t);
        else if (t !== tilt) tw.to(t);
        tilt = t;
      }
    },
    /** Where a moving cover is heading (a line across the glass, or a mark on the rod); null hides it. */
    setTarget(p: number | null) {
      target.hidden = p === null;
      if (p === null) return;
      if (o.kind === 'curtain') {
        target.style.left = `${xOf(p)}px`;
        target.style.width = `${W - 2 * xOf(p)}px`;
        target.style.top = '';
      } else {
        target.style.top = `${yOf(p)}px`;
        target.style.left = target.style.width = '';
      }
    },
    get dragging() {
      return dragging;
    },
  });
}

/** The 72 px Home card picture: a small window with a blind at a position. `set(pct)`. */
export function previewCover(v = 0) {
  const fabric = h('i', { class: 'f-pcover-fabric' });
  const bar = h('i', { class: 'f-pcover-bar' });
  const set = (p: number) => {
    const fh = 52 * (1 - clamp(p) / 100);
    fabric.style.height = `${fh}px`;
    bar.style.top = `${10 + fh}px`;
  };
  set(v);
  return Object.assign(
    h(
      'span',
      { class: 'f-pcover' },
      h('i', { class: 'f-pcover-frame' }),
      h('i', { class: 'f-pcover-rail' }),
      fabric,
      bar,
    ),
    { set },
  );
}
