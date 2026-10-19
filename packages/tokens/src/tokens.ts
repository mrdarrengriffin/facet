/**
 * Facet design tokens.
 *
 * `tokens.json` is the single source of truth. This module re-exports it with types, plus a few derived values
 * (the screen layout box) that components use instead of literal numbers.
 */
import data from './tokens.json' with { type: 'json' };

export type ThemeName = 'dark' | 'light';
export const themes: readonly ThemeName[] = ['dark', 'light'];

/** A colour with a value for each theme. */
export interface ThemedColor {
  dark: string;
  light: string;
  /** What the colour is for. */
  use: string;
}

export interface FontMetrics {
  unitsPerEm: number;
  ascender: number;
  descender: number;
  capHeight: number;
  xHeight: number;
}

export interface FontToken {
  family: string;
  fallback: string;
  metrics: FontMetrics;
}

export interface TypeStyle {
  font: 'sans' | 'mono';
  /** Font size in px. */
  size: number;
  weight: number;
  lineHeight: number;
  /** Line box in px (a multiple of 8). Text is centred on its cap height inside it. */
  box?: number;
  /** Letter spacing in px. */
  letterSpacing: number;
  case: 'none' | 'upper';
  use: string;
  /** Font id on the firmware side. */
  lvgl: string;
  /** Clock digits only: digit cell width, colon cell width and colon lift, in em. */
  digit?: number;
  colon?: number;
  colonY?: number;
}

export interface Easing {
  css: string;
  lvgl: string;
}

const strip = <T extends object>(o: T) =>
  Object.fromEntries(Object.entries(o).filter(([k]) => !k.startsWith('$'))) as Omit<T, '$comment'>;

export const screen = data.screen;
export const color = strip(data.color) as Record<
  keyof Omit<typeof data.color, '$comment'>,
  ThemedColor
>;
export type ColorToken = keyof typeof color;

export const font = { sans: data.font.sans, mono: data.font.mono } as Record<
  'sans' | 'mono',
  FontToken
>;

/** The screen type scale: display 144, hero 96, value 64, headline 40, title 28, body 22, label 18. */
export const screenType = strip(data.screenType) as Record<
  keyof Omit<typeof data.screenType, '$comment'>,
  TypeStyle
>;
export type ScreenTypeToken = keyof typeof screenType;

/** The component type scale: the smaller text inside components (lists, chips, the dial, clocks). */
export const type = strip(data.type) as Record<keyof Omit<typeof data.type, '$comment'>, TypeStyle>;
export type TypeToken = keyof typeof type;

export const space = data.space;
export const radius = data.radius;
export const stroke = data.stroke;
export const size = data.size;
export const opacity = data.opacity;
export const motion = data.motion as {
  duration: Record<keyof typeof data.motion.duration, number>;
  easing: Record<keyof typeof data.motion.easing, Easing>;
  transition: { default: 'instant' };
};
export const toast = strip(data.toast) as {
  position: 'top' | 'bottom';
  max: number;
  life: number;
  'life-urgent': number;
  swipe: number;
  sound: string;
};
export const sound = strip(data.sound) as {
  chosen: string;
  peakDb: number;
  loudnessDb: number;
  fadeMs: number;
  sampleRates: number[];
};

/**
 * Screen layout. Every x / y / w / h is a multiple of 8. One inset (32) on all four sides, a 48 px header row at
 * y 32, and the content box from y 112 to the bottom inset.
 */
const L = strip(data.layout);
export const layout = {
  ...L,
  grid: 8,
  /** Content box (x, y, w, h) below the header. */
  contentX: L.inset,
  contentY: L.top,
  contentW: data.screen.width - 2 * L.inset,
  contentH: data.screen.height - L.inset - L.top,
};

/**
 * The compact frame used by overlays and dense views (toasts, bottom sheets, list and detail views): a 16 px inset
 * and an 80 px header band, content from y 96.
 */
const IN = data.space.screen;
const TOP = data.size['header-h'] + data.space.gutter;
export const frame = {
  inset: IN,
  gutter: data.space.gutter,
  headerH: data.size['header-h'],
  top: TOP,
  width: data.screen.width - 2 * IN,
  right: data.screen.width - IN,
  bottom: data.screen.height - IN,
  height: data.screen.height - IN - TOP,
  cx: data.screen.width / 2,
  cy: (TOP + data.screen.height - IN) / 2,
};

export const tokens = {
  screen,
  color,
  font,
  screenType,
  type,
  layout,
  frame,
  space,
  radius,
  stroke,
  size,
  opacity,
  motion,
  toast,
  sound,
} as const;
export type Tokens = typeof tokens;
