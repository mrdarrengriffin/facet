// Media visuals: the real-world dimensions (mm) every visual is drawn from, with their sources, plus the shared maths
// (tape pack radius from progress, angular speed from linear tape speed, tonearm geometry).
// Inside each visual everything keeps its real proportions (each SVG uses a mm viewBox); only the box it's placed in
// on the screen is on the 8 px grid.
import { layout } from '@facet/tokens';

/** Compact Cassette (Philips 1963, standardised as IEC 60094-7 / DIN 45516). Origin = top-left of the shell, tape
 *  opening at the bottom, side A facing you. IEC 60094-7 itself is paywalled: the shell outline, label recess, window,
 *  screws and head-area openings follow the Philips drawing as reproduced in walldye.com/cassette (design.py), the
 *  hub spacing is the commonly quoted 42.5 mm (sources quote 42.0-42.6; the walldye drawing 42.6). The label recess
 *  starts at y 6 here (not 4.4) so the top screws sit outside it. */
export const CASSETTE = {
  w: 100.4,
  h: 63.8,
  corner: 3,
  hubPitch: 42.5, // hub centre to hub centre
  hubY: 28.9, // hub centres from the top edge (3.0 above the shell centre line)
  hubR: 8.5, // tape core: hub OD 17
  boreR: 6.2, // drive bore 12.4; 6 teeth point inwards from r 6.2 to 4.6 (the deck spindle has 6 keys)
  teeth: 6,
  toothLen: 1.6,
  toothW: 1.4,
  label: { x: 5.2, y: 6.0, w: 90, h: 40.4, r: 2 },
  window: { x: 20.2, y: 20.4, w: 60, h: 17, r: 2.2 }, // 17 tall = the hub OD: the hubs just fill the window height
  screws: [
    [4.2, 3.9],
    [96.2, 3.9],
    [3.2, 49.9],
    [97.2, 49.9],
    [50.2, 48.9],
  ] as [number, number][],
  screwR: 1.7,
  headArea: [
    [12.7, 63.8],
    [19.2, 53.4],
    [81.2, 53.4],
    [87.7, 63.8],
  ] as [number, number][],
  openings: [
    [30.7, 37.7],
    [44.2, 56.2],
    [62.7, 69.7],
  ] as [number, number][], // bottom-edge openings, 3.6 deep
  openingDepth: 3.6,
  capstanHoles: [
    [34.2, 57.9],
    [66.2, 57.9],
  ] as [number, number][],
  capstanR: 1.8,
  pinHoles: [
    [24.2, 58.4],
    [76.2, 58.4],
  ] as [number, number][],
  pinSide: 2.6,
  tabs: [
    [6, 12],
    [88.4, 94.4],
  ] as [number, number][], // write-protect tabs on the top edge (3 deep)
  tapeWidth: 3.81, // 0.150 in
  speed: 47.625, // mm/s = 1 7/8 ips
  // C60: 30 min a side = 85.7 m at 4.76 cm/s + leaders = ~90 m of ~16 um tape (Wikipedia: C60 15-16 um)
  tapeLength: 90_000,
  tapeThickness: 0.016,
};

/** Compact Disc: ECMA-130 2nd ed. (1996) = ISO/IEC 10149, the data side of Red Book (IEC 60908). Diameters, mm. */
export const CD = {
  d: 120.0, // d10 120.0 +-0.3
  hole: 15.0, // d1 15.0 +0.1/-0
  clampIn: 26, // d2 max: clamping area 26..33 (min)
  clampOut: 33,
  stackRing: 34, // the moulded stacking ring just outside the clamping area (label side, typical)
  infoIn: 44, // d4 max: information area starts (metallised layer)
  leadIn: 46, // d6 max: inner buffer / lead-in
  program: 50, // d7 50.0 -0.4: user data (program) starts
  programMax: 116, // d8 max
  infoOut: 117, // metallised data band edge used here (d5 <= 118 max, outer buffer 1 mm after lead-out)
  rimIn: 118, // d5 max: rim area to the edge
  printIn: 46, // label print from the lead-in (here), to infoOut
  scan: 1300, // mm/s, CLV scanning velocity 1.20..1.40 m/s (sec. 11.4): ~500 rpm at d 50, ~214 rpm at d 116
};

/** 12" LP: RIAA Bulletin E4 (1978) "33 1/3 RPM records for home use", = IEC 60098. Diameters, mm. */
export const LP = {
  d: 301.6, // 11.875 in +-0.031
  hole: 7.26, // 0.286 in +0.001/-0.002 (IEC 60098 states 7.24 +0.09/-0)
  outerGroove: 297.6, // outermost groove at recording pitch (lead-in), 11.719 in
  music: 292.1, // recording groove contour limit = first music groove, 11.500 in max
  lastMusic: 120.6, // minimum inside diameter of recording, 4.750 in
  lockGroove: 106.4, // stopping groove (closed concentric circle), 4.187 in
  label: 100, // label 100 mm (3.94 in), inside the 106.4 stopping groove (industry standard)
  rpm: 100 / 3, // clockwise when facing the side being played
};
/** 7" single (RIAA E4, 45 rpm): future option. */
export const SINGLE7 = {
  d: 174.6,
  hole: 38.2,
  label: 88.9,
  music: 168.3,
  lastMusic: 108,
  lockGroove: 98.4,
  rpm: 45,
};

/** Tonearm: Technics SL-1200 MK2 (the reference turntable): effective length 230 mm, overhang 15 mm
 *  -> pivot-to-spindle 215 mm, headshell offset angle 22 deg. */
export const ARM = {
  effective: 230,
  overhang: 15,
  pivot: 215,
  offset: 22,
  baseR: 20,
  bearingR: 9,
  tubeW: 7.5,
  weightFrom: 22,
  weightTo: 50,
  weightW: 30,
  restR: 168,
};

/** 7" reel-to-reel (EIA/"cine" 7 in plastic reel, three cut-outs, three-key 8 mm centre on a splined spindle):
 *  flange 177.8 mm (7 in); small-hub core ~57 mm (2.25 in, typical blank-tape reel); 1/4 in tape (6.35 mm);
 *  1800 ft (548.6 m) long-play tape ~35 um thick; 7.5 ips = 190.5 mm/s. 10.5" NAB reels (267 mm, 3 in / 76 mm NAB
 *  centre) are the studio size: too big for this layout. Spindle spacing is the deck's choice, not a standard. */
export const REEL = {
  d: 177.8,
  hubD: 57.2,
  holeD: 8,
  keys: 3,
  cutIn: 33,
  cutOut: 81,
  cutSpan: 78, // three cut-outs (annular sectors), 120 deg apart
  tapeWidth: 6.35,
  tapeLength: 548_640,
  tapeThickness: 0.035,
  speed: 190.5,
  spacing: 230, // spindle centre to centre on this deck (7" deck, gap ~52 mm)
  guide: { x: 60, y: 110, r: 6 },
  headLine: 116,
};
export const NAB = { d: 266.7, hub: 76.2 };

// ---------------------------------------------------------------- maths
/** Full tape pack radius: pack area pi (R^2 - r^2) = length x thickness. */
export const fullRadius = (core: number, length: number, thick: number) =>
  Math.sqrt(core * core + (length * thick) / Math.PI);

/** Pack radii from progress p (0..1 of the side): area-conserving, r = sqrt(r_core^2 + k * length). */
export function packs(core: number, full: number, p: number) {
  const q = Math.min(1, Math.max(0, p)),
    A = full * full - core * core;
  return { supply: Math.sqrt(core * core + (1 - q) * A), takeup: Math.sqrt(core * core + q * A) };
}

/** Angular speed (deg/s) of a pack of radius r (mm) at linear tape speed v (mm/s): w = v / r. */
export const degPerSec = (v: number, r: number) => (v / r) * (180 / Math.PI);

/** Tonearm: stylus on radius r (mm) from the spindle (0,0), pivot P (mm), effective length L: the arm angle (deg,
 *  LVGL/SVG convention, 0 = +x, clockwise) from the pivot to the stylus. side -1/1 picks the intersection. */
export function armAngle(P: [number, number], L: number, r: number, side = 1) {
  const d = Math.hypot(P[0], P[1]);
  const cosA = Math.max(-1, Math.min(1, (L * L + d * d - r * r) / (2 * L * d)));
  const toSpindle = Math.atan2(-P[1], -P[0]);
  return (toSpindle + side * Math.acos(cosA)) * (180 / Math.PI);
}

/** Tangent point on circle (C, R) for a line from the outside point Q; which = +1 / -1 picks the side. */
export function tangentPoint(C: [number, number], R: number, Q: [number, number], which: 1 | -1) {
  const dx = Q[0] - C[0],
    dy = Q[1] - C[1],
    d = Math.hypot(dx, dy);
  const a = Math.atan2(dy, dx) + which * Math.acos(Math.min(1, R / d));
  return [C[0] + R * Math.cos(a), C[1] + R * Math.sin(a)] as [number, number];
}

/**
 * Where the visuals sit on the Media screen: the disc's slot (right edge x 288, y 112..368), cropped by the screen's
 * left edge. From `layout.mv-*` in @facet/tokens.
 */
export const PLACE = { right: layout['mv-right'], top: layout['mv-top'], h: layout['mv-h'] };
