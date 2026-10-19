// The four media visuals (cassette in a deck, CD, vinyl LP with tonearm, 7" reel-to-reel), each an SVG in a mm
// viewBox so every part keeps its real proportion (specs.ts); one shared rAF loop turns the small rotating layers
// (hubs, label, reels, disc art) from the real tape / disc speeds.
import { h, reducedMotion, s } from '../dom.js';
import {
  ARM,
  CASSETTE as C,
  CD,
  LP,
  PLACE,
  REEL as R,
  armAngle,
  degPerSec,
  fullRadius,
  packs,
  tangentPoint,
} from './specs.js';

export type VisualId = 'cassette' | 'cd' | 'vinyl' | 'reel';
/** What a visual shows: progress 0-1 of the track, whether it plays, the cover art URL ('' = none), the track and
 *  four colours from the art (the cassette label stripes). */
export type Feed = {
  progress: number;
  playing: boolean;
  art: string;
  title: string;
  artist: string;
  palette: string[];
};
export interface Visual {
  id: VisualId;
  el: HTMLElement; // positioned box (px), absolute inside a 720 frame (or a sheet frame)
  box: { x: number; y: number; w: number; h: number; k: number }; // screen px placement + px per mm
  feed(f: Feed): void;
  /** Move on by `dt` seconds. `still` = reduced motion: nothing turns. */
  frame(dt: number, still?: boolean): void;
}

let uid = 0;
const id = (p: string) => `f-mv-${p}-${++uid}`;
const f2 = (n: number) => +n.toFixed(3);
const ease = (cur: number, target: number, dt: number, tau: number) =>
  cur + (target - cur) * (1 - Math.exp(-dt / Math.max(0.001, tau)));
const annulus = (cx: number, cy: number, r1: number, r2: number) =>
  `M${cx + r2} ${cy}A${r2} ${r2} 0 1 0 ${cx - r2} ${cy}A${r2} ${r2} 0 1 0 ${cx + r2} ${cy}Z` +
  `M${cx + r1} ${cy}A${r1} ${r1} 0 1 1 ${cx - r1} ${cy}A${r1} ${r1} 0 1 1 ${cx + r1} ${cy}Z`;
const rr = (x: number, y: number, w: number, hh: number, r: number) =>
  `M${x + r} ${y}H${x + w - r}A${r} ${r} 0 0 1 ${x + w} ${y + r}V${y + hh - r}A${r} ${r} 0 0 1 ${x + w - r} ${y + hh}H${x + r}A${r} ${r} 0 0 1 ${x} ${y + hh - r}V${y + r}A${r} ${r} 0 0 1 ${x + r} ${y}Z`;
/** clockwise circle (same winding as rr(), so rect + circles = a union under nonzero) */
const circlePath = (cx: number, cy: number, r: number) =>
  `M${cx + r} ${cy}A${r} ${r} 0 1 1 ${cx - r} ${cy}A${r} ${r} 0 1 1 ${cx + r} ${cy}Z`;

function frameBox(
  vid: VisualId,
  x: number,
  y: number,
  w: number,
  hh: number,
  k: number,
  ...kids: Node[]
) {
  const el = h(
    'div',
    {
      class: `f-mv f-mv-${vid}`,
      'data-visual': vid,
      style: { left: `${f2(x)}px`, top: `${f2(y)}px`, width: `${f2(w)}px`, height: `${f2(hh)}px` },
    },
    ...kids,
  );
  return el;
}
const svgRoot = (vb: [number, number, number, number], w: number, hh: number) =>
  s('svg', {
    viewBox: vb.join(' '),
    width: f2(w),
    height: f2(hh),
    class: 'f-mv-svg',
    'aria-hidden': 'true',
  });
const artImage = (x: number, y: number, w: number, hh: number, clip?: string) =>
  s('image', {
    x,
    y,
    width: w,
    height: hh,
    preserveAspectRatio: 'xMidYMid slice',
    'clip-path': clip ? `url(#${clip})` : undefined,
    class: 'f-mv-art',
  });
const setArt = (img: SVGElement, url: string) => {
  if (img.getAttribute('href') !== url) img.setAttribute('href', url);
};

// ---------------------------------------------------------------- 1 Compact cassette (in the deck well)
/** Cassette: 100.4 x 63.8 mm at k = 256 / 63.8 = 4.01 px/mm (403 px wide); in the hero slot (right edge x 288, y 112..368): only the
 *  right ~56 mm shows (take-up spool, right screws, the right end of the label + window, the head area). */
export function cassette(): Visual {
  const k = PLACE.h / C.h,
    W = C.w * k,
    H = C.h * k;
  const box = { x: PLACE.right - W, y: PLACE.top + 16, w: W, h: H, k }; // 16 lower than the slot, so it sits clear of the header
  const svg = svgRoot([0, 0, C.w, C.h], W, H);
  const winId = id('win'),
    labId = id('lab'),
    openId = id('open');
  const hubX = [C.w / 2 - C.hubPitch / 2, C.w / 2 + C.hubPitch / 2];
  const win = C.window;
  const winPath = rr(win.x, win.y, win.w, win.h, win.r);
  const lab = C.label;
  const full = fullRadius(C.hubR, C.tapeLength, C.tapeThickness);
  const defs = s(
    'defs',
    null,
    s('clipPath', { id: winId }, s('path', { d: winPath })),
    s('clipPath', { id: labId }, s('path', { d: rr(lab.x, lab.y, lab.w, lab.h, lab.r) })),
    s(
      'clipPath',
      { id: openId },
      ...C.openings.map(([a, b]) =>
        s('rect', { x: a, y: C.h - C.openingDepth, width: b - a, height: C.openingDepth }),
      ),
    ),
  );
  // shell: smoke plastic, a 0.6 mm moulded rim, the raised head area (trapezoid) at the bottom
  const shell = s('path', { d: rr(0, 0, C.w, C.h, C.corner), class: 'f-mv-shell' });
  const rim = s('path', {
    d: rr(1.1, 1.1, C.w - 2.2, C.h - 2.2, C.corner - 1),
    class: 'f-mv-shell-rim',
  });
  const head = s('path', {
    d: `M${C.headArea.map((p) => p.join(' ')).join('L')}Z`,
    class: 'f-mv-shell-2',
  });
  // label: paper strip (writing lines + title), colour block from the art's dominant colours, 3 stripes (no album art)
  const paper = s('rect', { x: lab.x, y: lab.y, width: lab.w, height: lab.h, class: 'f-mv-paper' });
  const stripes = [0, 1, 2, 3].map((i) =>
    s('rect', { x: lab.x, y: 38.4 + i * 2.0, width: lab.w, height: 1.6, class: 'f-mv-stripe' }),
  );
  const rules = [12.2, 17.6].map((y) =>
    s('line', { x1: lab.x + 3, x2: lab.x + lab.w - 3, y1: y, y2: y, class: 'f-mv-rule' }),
  );
  const t1 = s('text', { x: 93.4, y: 11.6, 'text-anchor': 'end', class: 'f-mv-ink f-mv-ink-1' });
  const t2 = s('text', { x: 93.4, y: 17.0, 'text-anchor': 'end', class: 'f-mv-ink f-mv-ink-2' });
  const sideA = s('text', { x: 7.4, y: 16.6, class: 'f-mv-ink f-mv-side' }, 'A');
  const c60 = s(
    'text',
    { x: 93.4, y: 36.8, 'text-anchor': 'end', class: 'f-mv-ink f-mv-ink-c' },
    'C60',
  );
  const label = s(
    'g',
    { 'clip-path': `url(#${labId})` },
    paper,
    ...stripes,
    ...rules,
    t1,
    t2,
    sideA,
    c60,
  );
  // window: smoked clear plastic; tape packs (area-conserving) + the white hubs with 6 drive teeth (rotating)
  const winBg = s('path', { d: winPath, class: 'f-mv-win' });
  const packEls = hubX.map((x) =>
    s('circle', { cx: x, cy: C.hubY, r: C.hubR, class: 'f-mv-pack' }),
  );
  const sheenEls = hubX.map((x) =>
    s('circle', { cx: x, cy: C.hubY, r: C.hubR, class: 'f-mv-pack-sheen' }),
  );
  const hubs = hubX.map((x) => {
    const g = s(
      'g',
      { class: 'f-mv-rot' },
      s('path', {
        d: circlePath(x, C.hubY, C.hubR) + circlePath(x, C.hubY, C.boreR),
        'fill-rule': 'evenodd',
        class: 'f-mv-hub',
      }),
      ...Array.from({ length: C.teeth }, (_, i) =>
        s('rect', {
          x: x - C.toothW / 2,
          y: C.hubY - C.boreR - 0.2,
          width: C.toothW,
          height: C.toothLen + 0.2,
          rx: 0.25,
          class: 'f-mv-hub',
          transform: `rotate(${(360 / C.teeth) * i} ${x} ${C.hubY})`,
        }),
      ),
      s('circle', { cx: x, cy: C.hubY, r: C.hubR - 0.9, class: 'f-mv-hub-ring' }),
      s('rect', {
        x: x - 1.3,
        y: C.hubY - C.hubR - 0.1,
        width: 2.6,
        height: 1.5,
        rx: 0.3,
        class: 'f-mv-gap',
      }),
    ); // leader clamp slot
    return g;
  });
  const glare = s('path', {
    d: `M${win.x + 8} ${win.y}L${win.x + 15} ${win.y}L${win.x + 7} ${win.y + win.h}L${win.x} ${win.y + win.h}Z M${win.x + 44} ${win.y}L${win.x + 47} ${win.y}L${win.x + 39} ${win.y + win.h}L${win.x + 36} ${win.y + win.h}Z`,
    class: 'f-mv-glare',
  });
  const bores = hubX.map((x) => s('circle', { cx: x, cy: C.hubY, r: C.boreR, class: 'f-mv-gap' })); // the dark shell inside / deck spindle
  const winIn = s(
    'g',
    { 'clip-path': `url(#${winId})` },
    ...packEls,
    ...sheenEls,
    ...bores,
    ...hubs,
    glare,
  );
  const winEdge = s('path', { d: winPath, class: 'f-mv-win-edge' });
  // bottom edge: three openings (pinch rollers + head), the tape running across them, capstan + pin holes
  const openings = C.openings.map(([a, b]) =>
    s('rect', {
      x: a,
      y: C.h - C.openingDepth,
      width: b - a,
      height: C.openingDepth,
      class: 'f-mv-gap',
    }),
  );
  const tapeRun = s('rect', {
    x: 0,
    y: C.h - 1.6,
    width: C.w,
    height: 0.9,
    class: 'f-mv-tape-edge',
    'clip-path': `url(#${openId})`,
  });
  const holes = [
    ...C.capstanHoles.map(([x, y]) =>
      s('circle', { cx: x, cy: y, r: C.capstanR, class: 'f-mv-gap' }),
    ),
    ...C.pinHoles.map(([x, y]) =>
      s('rect', {
        x: x - C.pinSide / 2,
        y: y - C.pinSide / 2,
        width: C.pinSide,
        height: C.pinSide,
        rx: 0.3,
        class: 'f-mv-gap',
      }),
    ),
  ];
  const screws = C.screws.map(([x, y]) =>
    s(
      'g',
      null,
      s('circle', { cx: x, cy: y, r: C.screwR, class: 'f-mv-screw' }),
      s('path', { d: `M${x - 1} ${y}H${x + 1}M${x} ${y - 1}V${y + 1}`, class: 'f-mv-screw-x' }),
    ),
  );
  svg.append(
    defs,
    shell,
    rim,
    head,
    label,
    winBg,
    winIn,
    winEdge,
    ...openings,
    tapeRun,
    ...holes,
    ...screws,
  ); // (write-protect tabs are on the top face: not visible from the front)
  const el = frameBox('cassette', box.x, box.y, W, H, k, svg);

  const ang = [0, 0];
  let m = 0,
    feed: Feed | null = null,
    rad = { supply: full, takeup: C.hubR };
  return {
    id: 'cassette',
    el,
    box,
    feed(f) {
      feed = f;
      rad = packs(C.hubR, full, f.progress);
      packEls[0].setAttribute('r', String(f2(rad.supply)));
      packEls[1].setAttribute('r', String(f2(rad.takeup)));
      sheenEls[0].setAttribute('r', String(f2(Math.max(C.hubR, rad.supply - 0.7))));
      sheenEls[1].setAttribute('r', String(f2(Math.max(C.hubR, rad.takeup - 0.7))));
      const up = (v: string, n: number) =>
        (v.length > n ? `${v.slice(0, n - 1)}…` : v).toUpperCase();
      t1.textContent = up(f.title || 'Side A', 26);
      t2.textContent = up(f.artist || '', 30);
      f.palette.forEach((col, i) => {
        if (stripes[i]) stripes[i].style.fill = col;
      });
    },
    frame(dt, still) {
      // real speed: 47.6 mm/s at the pack edge; both spools turn counter-clockwise for side A (the tape leaves the
      // supply pack on its outer side towards the bottom-left guide and winds onto the take-up from the bottom-right)
      m = ease(m, feed?.playing ? 1 : 0, dt, feed?.playing ? 0.25 : 0.12);
      if (m < 0.0005 || still) return;
      ang[0] -= degPerSec(C.speed, rad.supply) * dt * m;
      ang[1] -= degPerSec(C.speed, rad.takeup) * dt * m;
      hubs.forEach((g, i) =>
        g.setAttribute('transform', `rotate(${f2(ang[i] % 360)} ${hubX[i]} ${C.hubY})`),
      );
    },
  };
}

// ---------------------------------------------------------------- 2 CD with the album art printed on the label side
/** CD: 120 mm at k = 256 / 120 = 2.13 px/mm; the hero slot (x 32..288, y 112..368), like the disc. Label side up: clear hub
 *  (15 mm hole, clamping area 26-33, stacking ring 34), a sliver of the metallised band from 44 mm, art printed
 *  46..117 mm, clear rim to 120. The art turns (clockwise from the label side = counter-clockwise from the laser);
 *  the iridescent sheen is a static overlay (the light does not turn with the disc). */
export function compactDisc(): Visual {
  const D = PLACE.h,
    k = D / CD.d,
    c = CD.d / 2;
  const box = { x: PLACE.right - D, y: PLACE.top, w: D, h: D, k };
  const svg = svgRoot([0, 0, CD.d, CD.d], D, D);
  const artId = id('cdart');
  const r = (d: number) => d / 2;
  const defs = s(
    'defs',
    null,
    s(
      'clipPath',
      { id: artId },
      s('path', { d: annulus(c, c, r(CD.printIn), r(CD.infoOut)), 'clip-rule': 'evenodd' }),
    ),
  );
  const base = s('path', {
    d: annulus(c, c, r(CD.hole), r(CD.d)),
    'fill-rule': 'evenodd',
    class: 'f-mv-cd-clear',
  });
  const metal = s('path', {
    d: annulus(c, c, r(CD.infoIn), r(CD.infoOut)),
    'fill-rule': 'evenodd',
    class: 'f-mv-cd-mirror',
  });
  const art = artImage(c - r(CD.infoOut), c - r(CD.infoOut), CD.infoOut, CD.infoOut, artId);
  const spin = s(
    'g',
    { class: 'f-mv-rot' },
    art,
    // a printed hairline + the moulded matrix code ring make the turn visible even on a flat cover
    s('path', { d: `M${c} ${c - r(CD.infoIn) - 0.4}v-0.9`, class: 'f-mv-cd-mark' }),
  );
  const clamp = s('path', {
    d: annulus(c, c, r(CD.clampIn), r(CD.clampOut)),
    'fill-rule': 'evenodd',
    class: 'f-mv-cd-clamp',
  });
  const stack = s('circle', { cx: c, cy: c, r: r(CD.stackRing), class: 'f-mv-cd-stack' });
  const printEdge = s('circle', { cx: c, cy: c, r: r(CD.printIn), class: 'f-mv-cd-edge' });
  const rimEdge = s('circle', { cx: c, cy: c, r: r(CD.d) - 0.15, class: 'f-mv-cd-rim' });
  const holeEdge = s('circle', { cx: c, cy: c, r: r(CD.hole) + 0.1, class: 'f-mv-cd-rim' });
  svg.append(defs, base, metal, spin, clamp, stack, printEdge, rimEdge, holeEdge);
  const sheen = h('i', {
    class: 'f-mv-cd-sheen',
    style: { '--hole': `${f2(r(CD.hole) * k)}px`, '--in': `${f2(r(CD.clampOut) * k)}px` },
  });
  const el = frameBox('cd', box.x, box.y, D, D, k, svg, sheen);
  let a = 0,
    m = 0,
    feed: Feed | null = null;
  return {
    id: 'cd',
    el,
    box,
    feed(f) {
      feed = f;
      setArt(art, f.art);
    },
    frame(dt, still) {
      // CLV 1.3 m/s: ~500 rpm at the program start (d 50) falling to ~214 rpm at d 116 as the track plays; / 16 for calm
      m = ease(m, feed?.playing ? 1 : 0, dt, feed?.playing ? 0.6 : 0.9);
      if (m < 0.0005 || still) return;
      const rr2 = r(CD.program) + (feed?.progress ?? 0) * (r(CD.programMax) - r(CD.program));
      a += (degPerSec(CD.scan, rr2) / 16) * dt * m;
      spin.setAttribute('transform', `rotate(${f2(a % 360)} ${c} ${c})`);
    },
  };
}

// ---------------------------------------------------------------- 3 Vinyl LP with the tonearm
/** 12" LP: 301.6 mm at k = 256 / 301.6 = 0.849 px/mm, the hero slot (centre 160, 240). Grooves are rotation
 *  symmetric, so only the 100 mm label (the album art) turns, at the real 33 1/3 rpm. Tonearm = Technics SL-1200
 *  geometry: pivot 215 mm from the spindle (16 px inside the slot's right edge, top-right), effective length 230; the stylus follows the
 *  progress from the first music groove (r 146.05) to the last (r 60.3); idle = arm on its rest. */
export function vinyl(): Visual {
  const D = PLACE.h,
    k = D / LP.d,
    R = LP.d / 2;
  const cx = PLACE.right - D / 2,
    cy = PLACE.top + PLACE.h / 2;
  const Px = (PLACE.right - 16 - cx) / k,
    Py = -Math.sqrt(ARM.pivot ** 2 - Px * Px); // pivot (mm, spindle = 0,0)
  const vb: [number, number, number, number] = [-R, Py - 34, R + Px + 36, R - (Py - 34)]; // x -R..pivot+36, y above the pivot..R
  const W = vb[2] * k,
    H = vb[3] * k;
  const box = { x: cx - R * k, y: cy + vb[1] * k, w: W, h: H, k };
  const svg = svgRoot(vb, W, H);
  const labId = id('vlab');
  const r = (d: number) => d / 2;
  const defs = s(
    'defs',
    null,
    s('clipPath', { id: labId }, s('circle', { cx: 0, cy: 0, r: r(LP.label) })),
  );
  const disc = s('path', {
    d: annulus(0, 0, r(LP.hole), R),
    'fill-rule': 'evenodd',
    class: 'f-mv-vinyl',
  });
  const edge = s('path', {
    d: annulus(0, 0, r(LP.outerGroove), R),
    'fill-rule': 'evenodd',
    class: 'f-mv-vinyl-edge',
  });
  const leadIn = s('path', {
    d: annulus(0, 0, r(LP.music), r(LP.outerGroove)),
    'fill-rule': 'evenodd',
    class: 'f-mv-vinyl-gloss',
  });
  const grooves = s('g', { class: 'f-mv-grooves' });
  for (let rg = r(LP.lastMusic) + 0.6; rg < r(LP.music); rg += 0.92)
    grooves.append(s('circle', { cx: 0, cy: 0, r: f2(rg) }));
  // band gaps between tracks (wider-pitch rings look glossier): decorative, the track list is not known
  const bands = [0.22, 0.47, 0.7].map((q) =>
    s('circle', {
      cx: 0,
      cy: 0,
      r: f2(r(LP.lastMusic) + q * (r(LP.music) - r(LP.lastMusic))),
      class: 'f-mv-band',
    }),
  );
  const runout = s('path', {
    d: annulus(0, 0, r(LP.lockGroove), r(LP.lastMusic)),
    'fill-rule': 'evenodd',
    class: 'f-mv-vinyl-gloss',
  });
  const lock = s('circle', { cx: 0, cy: 0, r: r(LP.lockGroove), class: 'f-mv-lock' });
  const art = artImage(-r(LP.label), -r(LP.label), LP.label, LP.label, labId);
  const label = s(
    'g',
    { class: 'f-mv-rot' },
    s('circle', { cx: 0, cy: 0, r: r(LP.label), class: 'f-mv-label-bg' }),
    art,
    s('circle', { cx: 0, cy: 0, r: r(LP.label) - 0.4, class: 'f-mv-label-edge' }),
  );
  const spindle = s('circle', { cx: 0, cy: 0, r: r(LP.hole) - 0.3, class: 'f-mv-spindle' });
  // tonearm (pivot-local: arm along +x), rotated about the pivot each frame
  const L = ARM.effective;
  const arm = s(
    'g',
    null,
    s('rect', {
      x: -ARM.weightTo,
      y: -ARM.weightW / 2,
      width: ARM.weightTo - ARM.weightFrom,
      height: ARM.weightW,
      rx: 3,
      class: 'f-mv-weight',
    }),
    s('line', {
      x1: -ARM.weightFrom,
      y1: 0,
      x2: L - 26,
      y2: 0,
      class: 'f-mv-arm',
      'stroke-width': ARM.tubeW,
    }),
    s(
      'g',
      { transform: `translate(${L} 0) rotate(${ARM.offset})` },
      s('rect', { x: -34, y: -8.5, width: 40, height: 17, rx: 2, class: 'f-mv-headshell' }),
      s('line', { x1: 2, y1: -8.5, x2: 10, y2: -16, class: 'f-mv-arm', 'stroke-width': 2.2 }),
      s('rect', { x: -6, y: -5, width: 10, height: 10, rx: 1, class: 'f-mv-cart' }),
    ),
  );
  const restAng = armAngle([Px, Py], L, ARM.restR, -1);
  const restXY = [
    Px + L * Math.cos((restAng * Math.PI) / 180),
    Py + L * Math.sin((restAng * Math.PI) / 180),
  ];
  const rest = s(
    'g',
    null,
    s('circle', {
      cx: f2(restXY[0] - 14 * Math.cos((restAng * Math.PI) / 180)),
      cy: f2(restXY[1] - 14 * Math.sin((restAng * Math.PI) / 180)),
      r: 4.5,
      class: 'f-mv-post',
    }),
  );
  const armG = s('g', null, arm);
  const base = s(
    'g',
    null,
    s('circle', { cx: Px, cy: Py, r: ARM.baseR, class: 'f-mv-arm-base' }),
    s('circle', { cx: Px, cy: Py, r: ARM.bearingR, class: 'f-mv-arm-bearing' }),
  );
  svg.append(
    defs,
    disc,
    edge,
    leadIn,
    grooves,
    ...bands,
    runout,
    lock,
    label,
    spindle,
    rest,
    base,
    armG,
  );
  const sheen = h('i', {
    class: 'f-mv-vinyl-sheen',
    style: {
      left: '0px',
      top: `${f2((-vb[1] - R) * k)}px`,
      width: `${D}px`,
      height: `${D}px`,
      '--in': `${f2(r(LP.label) * k + 2)}px`,
    },
  });
  const el = frameBox('vinyl', box.x, box.y, W, H, k, svg, sheen);
  let a = 0,
    m = 0,
    feed: Feed | null = null,
    armA = restAng,
    idle = true;
  const placeArm = () =>
    armG.setAttribute('transform', `translate(${f2(Px)} ${f2(Py)}) rotate(${f2(armA)})`);
  placeArm();
  return {
    id: 'vinyl',
    el,
    box,
    feed(f) {
      feed = f;
      setArt(art, f.art);
    },
    frame(dt, still) {
      m = ease(m, feed?.playing ? 1 : 0, dt, feed?.playing ? 0.5 : 0.7); // platter start ~0.7 s, brake ~1 s
      if (m > 0.0005 && !still) {
        a += LP.rpm * 6 * dt * m;
        label.setAttribute('transform', `rotate(${f2(a % 360)})`);
      }
      idle = !feed || (!feed.playing && feed.progress <= 0);
      const rs = r(LP.music) - (feed?.progress ?? 0) * (r(LP.music) - r(LP.lastMusic));
      const target = idle ? restAng : armAngle([Px, Py], L, rs, -1);
      const na = ease(armA, target, dt, 0.45);
      if (Math.abs(na - armA) > 0.0005) {
        armA = na;
        placeArm();
      }
    },
  };
}

// ---------------------------------------------------------------- 4 Reel-to-reel (two 7" reels, 1/4" tape)
/** Two 7" reels (177.8 mm) 230 mm apart, scaled to the hero slot height: the right reel's right edge on x 288, top 112; the left
 *  (supply) reel is mostly off the screen's left edge. Tape: supply pack -> guide -> heads (erase, record, play) ->
 *  capstan / pinch roller -> guide -> take-up pack; packs area-conserving from progress; both reels clockwise. */
export function reelToReel(): Visual {
  const hw = R.spacing / 2 + R.d / 2,
    top = R.d / 2,
    bottom = 160,
    k = PLACE.h / (top + bottom); // fills the hero slot
  const W = 2 * hw * k,
    H = (top + bottom) * k;
  const box = { x: PLACE.right - W, y: PLACE.top, w: W, h: H, k };
  const svg = svgRoot([-hw, -top, 2 * hw, top + bottom], W, H);
  const core = R.hubD / 2,
    full = fullRadius(core, R.tapeLength, R.tapeThickness);
  const cxs = [-R.spacing / 2, R.spacing / 2];
  const G = R.guide,
    HL = R.headLine;
  const packEls = cxs.map((x) => s('circle', { cx: x, cy: 0, r: core, class: 'f-mv-pack' }));
  const sheenEls = cxs.map((x) => s('circle', { cx: x, cy: 0, r: core, class: 'f-mv-pack-sheen' }));
  const tape = s('path', { class: 'f-mv-tape-line', 'stroke-width': 1.5 });
  const cut = (x: number, a0: number) => {
    const p = (rad: number, deg: number) =>
      `${f2(x + rad * Math.cos((deg * Math.PI) / 180))} ${f2(rad * Math.sin((deg * Math.PI) / 180))}`;
    const a1 = a0 + R.cutSpan;
    return `M${p(R.cutOut, a0)}A${R.cutOut} ${R.cutOut} 0 0 1 ${p(R.cutOut, a1)}L${p(R.cutIn, a1)}A${R.cutIn} ${R.cutIn} 0 0 0 ${p(R.cutIn, a0)}Z`;
  };
  const reels = cxs.map((x, i) =>
    s(
      'g',
      { class: 'f-mv-rot' },
      s('path', {
        d:
          circlePath(x, 0, R.d / 2) +
          [0, 120, 240].map((a) => cut(x, a + 21 + i * 40)).join('') +
          circlePath(x, 0, R.holeD / 2),
        'fill-rule': 'evenodd',
        class: 'f-mv-reel',
      }),
      s('circle', { cx: x, cy: 0, r: R.d / 2 - 1.2, class: 'f-mv-reel-rim' }),
      s('circle', { cx: x, cy: 0, r: core, class: 'f-mv-reel-hub' }),
      s('circle', { cx: x, cy: 0, r: 12, class: 'f-mv-reel-hub' }),
      ...[0, 120, 240].map((a) =>
        s('rect', {
          x: x - 1.2,
          y: -R.holeD / 2 - 2.2,
          width: 2.4,
          height: 2.6,
          class: 'f-mv-reel-key',
          transform: `rotate(${a + 81 + i * 40} ${x} 0)`,
        }),
      ),
    ),
  );
  const spindles = cxs.map((x) =>
    s('circle', { cx: x, cy: 0, r: R.holeD / 2 - 0.6, class: 'f-mv-spindle' }),
  );
  const guides = [-1, 1].map((sg) =>
    s(
      'g',
      null,
      s('circle', { cx: sg * G.x, cy: G.y, r: G.r, class: 'f-mv-metal' }),
      s('circle', { cx: sg * G.x, cy: G.y, r: 1.6, class: 'f-mv-gap' }),
    ),
  );
  const cover = s('path', { d: rr(-44, HL + 4, 80, 30, 4), class: 'f-mv-deck' });
  const heads = [-30, -10, 10].map((x) =>
    s('rect', { x: x - 6, y: HL - 0.6, width: 12, height: 9, rx: 1.4, class: 'f-mv-metal' }),
  );
  const capstan = s('circle', { cx: 46, cy: HL - 2.4, r: 2.4, class: 'f-mv-metal' });
  const pinch = s(
    'g',
    null,
    s('circle', { cx: 46, cy: HL + 10.6, r: 10, class: 'f-mv-rubber' }),
    s('circle', { cx: 46, cy: HL + 10.6, r: 3, class: 'f-mv-metal' }),
  );
  svg.append(
    cover,
    ...heads,
    ...packEls,
    ...sheenEls,
    ...reels,
    ...spindles,
    tape,
    ...guides,
    capstan,
    pinch,
  );
  const el = frameBox('reel', box.x, box.y, W, H, k, svg);
  const ang = [0, 0];
  let m = 0,
    feed: Feed | null = null,
    rad = { supply: full, takeup: core };
  const path = () => {
    // supply leaves its right side heading down to the left guide's outer (left) point, wraps to its bottom, runs along
    // the head line, wraps the right guide, and climbs to the take-up's left side
    const qL: [number, number] = [-G.x - G.r, G.y],
      qR: [number, number] = [G.x + G.r, G.y];
    const tL = tangentPoint([cxs[0], 0], rad.supply, qL, -1),
      tR = tangentPoint([cxs[1], 0], rad.takeup, qR, 1);
    tape.setAttribute(
      'd',
      `M${f2(tL[0])} ${f2(tL[1])}L${qL[0]} ${qL[1]}A${G.r} ${G.r} 0 0 0 ${-G.x} ${HL}L${G.x} ${HL}A${G.r} ${G.r} 0 0 0 ${qR[0]} ${qR[1]}L${f2(tR[0])} ${f2(tR[1])}`,
    );
  };
  path();
  return {
    id: 'reel',
    el,
    box,
    feed(f) {
      feed = f;
      rad = packs(core, full, f.progress);
      packEls[0].setAttribute('r', String(f2(rad.supply)));
      packEls[1].setAttribute('r', String(f2(rad.takeup)));
      sheenEls[0].setAttribute('r', String(f2(Math.max(core, rad.supply - 1.2))));
      sheenEls[1].setAttribute('r', String(f2(Math.max(core, rad.takeup - 1.2))));
      path();
    },
    frame(dt, still) {
      // 7.5 ips = 190.5 mm/s at the pack edge (w = v / r), shown at half speed for calm
      m = ease(m, feed?.playing ? 1 : 0, dt, feed?.playing ? 0.35 : 0.2);
      if (m < 0.0005 || still) return;
      ang[0] += degPerSec(R.speed, rad.supply) * 0.5 * dt * m;
      ang[1] += degPerSec(R.speed, rad.takeup) * 0.5 * dt * m;
      reels.forEach((g, i) =>
        g.setAttribute('transform', `rotate(${f2(ang[i] % 360)} ${cxs[i]} 0)`),
      );
    },
  };
}

export const MAKERS: Record<VisualId, () => Visual> = {
  cassette,
  cd: compactDisc,
  vinyl,
  reel: reelToReel,
};

// ---------------------------------------------------------------- the shared animation loop
// One requestAnimationFrame loop for every visual on the page. A visual only animates while it's on screen, and drops
// out of the loop once it has been removed from the page. With reduced motion, nothing turns; packs and the tonearm
// still follow the track.
type Live = { v: Visual; seen: boolean };
const live = new Map<Visual, Live>();
let last = 0,
  running = false;
function loop(t: number) {
  const dt = last ? Math.min(0.1, (t - last) / 1000) : 0;
  last = t;
  const still = reducedMotion();
  for (const l of live.values()) {
    if (!l.v.el.isConnected) {
      if (l.seen) live.delete(l.v);
      continue;
    }
    l.seen = true;
    if (l.v.el.getClientRects().length) l.v.frame(dt, still);
  }
  if (live.size) requestAnimationFrame(loop);
  else {
    running = false;
    last = 0;
  }
}
/** Add a visual to the shared loop (again, after it was taken off the page and put back). */
export function animate(v: Visual) {
  live.set(v, { v, seen: false });
  if (!running) {
    running = true;
    requestAnimationFrame(loop);
  }
  return v;
}
