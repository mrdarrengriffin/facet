/**
 * Facet design tokens.
 *
 * This is a starter set: enough to build the docs site and the first components. The full
 * token set (worked out on real hardware) will replace it, so expect names and values to
 * change before 1.0.
 */

export type ThemeName = 'dark' | 'light';

/** A colour with a value for each theme. */
export interface ThemedColor {
  dark: string;
  light: string;
  /** What the colour is for. Shown on the design system page. */
  use: string;
}

export const color = {
  bg: { dark: '#000000', light: '#F4F2EE', use: 'Canvas' },
  surface: { dark: '#0A0B0E', light: '#FFFFFF', use: 'Cards, tiles, secondary buttons' },
  'surface-press': { dark: '#15171C', light: '#E9E6E0', use: 'Pressed surface' },
  line: { dark: '#22262D', light: '#DDD9D2', use: 'Hairline outlines and dividers' },
  'line-strong': { dark: '#3A404A', light: '#B9B4AB', use: 'Active outlines, ticks' },
  text: { dark: '#F2F1EE', light: '#16181C', use: 'Primary text' },
  'text-dim': { dark: '#8A919B', light: '#5C636D', use: 'Secondary text' },
  'text-mute': { dark: '#565C66', light: '#9A9FA6', use: 'Meta text, disabled' },
  accent: { dark: '#E8542A', light: '#E8542A', use: 'Signature orange: fills, rings, markers' },
  'accent-press': { dark: '#C2441F', light: '#C2441F', use: 'Pressed accent' },
  'accent-text': { dark: '#E8542A', light: '#B8401A', use: 'Accent used for text and icons' },
  'on-accent': { dark: '#0A0B0E', light: '#16181C', use: 'Text on the accent' },
  info: { dark: '#9CC3E6', light: '#2F6A9E', use: 'Secondary accent' },
  ok: { dark: '#5DBE7E', light: '#2E8A50', use: 'Status: fine' },
  warn: { dark: '#E8B33A', light: '#9A6B00', use: 'Status: needs attention' },
  error: { dark: '#E5484D', light: '#C9302C', use: 'Status: something is wrong' },
} as const satisfies Record<string, ThemedColor>;

export type ColorToken = keyof typeof color;

export const font = {
  sans: { family: 'Inter', fallback: 'system-ui, sans-serif' },
  mono: { family: 'JetBrains Mono', fallback: 'ui-monospace, monospace' },
} as const;

export type FontToken = keyof typeof font;

export interface TypeStyle {
  font: FontToken;
  /** Font size in px. */
  size: number;
  weight: number;
  lineHeight: number;
  /** Letter spacing in px. */
  letterSpacing: number;
  uppercase: boolean;
  use: string;
}

/**
 * Type scale. One large, light hero per screen; everything else stays quiet. Sizes are in px
 * at the reference panel size (720 x 720).
 */
export const type = {
  display: {
    font: 'sans',
    size: 144,
    weight: 200,
    lineHeight: 1,
    letterSpacing: -2,
    uppercase: false,
    use: 'The hero of a full screen: time, temperature',
  },
  hero: {
    font: 'sans',
    size: 96,
    weight: 200,
    lineHeight: 1,
    letterSpacing: -1,
    uppercase: false,
    use: 'Big numbers on a screen',
  },
  value: {
    font: 'sans',
    size: 64,
    weight: 200,
    lineHeight: 1,
    letterSpacing: -1,
    uppercase: false,
    use: 'The one value on a card',
  },
  headline: {
    font: 'sans',
    size: 40,
    weight: 300,
    lineHeight: 1.2,
    letterSpacing: 0,
    uppercase: false,
    use: 'Track title, weather condition',
  },
  title: {
    font: 'sans',
    size: 28,
    weight: 400,
    lineHeight: 1.25,
    letterSpacing: 0,
    uppercase: false,
    use: 'Row and card titles',
  },
  body: {
    font: 'sans',
    size: 22,
    weight: 400,
    lineHeight: 1.3,
    letterSpacing: 0,
    uppercase: false,
    use: 'Secondary lines',
  },
  label: {
    font: 'mono',
    size: 18,
    weight: 500,
    lineHeight: 1,
    letterSpacing: 2,
    uppercase: true,
    use: 'Labels, chips, buttons: the smallest text',
  },
} as const satisfies Record<string, TypeStyle>;

export type TypeToken = keyof typeof type;

/** Spacing in px, on an 8 px grid (with 4 for tight spots). */
export const space = {
  '1': 4,
  '2': 8,
  '3': 16,
  '4': 24,
  '5': 32,
  '6': 48,
  '7': 64,
} as const;

export type SpaceToken = keyof typeof space;

/** Corner radius in px. */
export const radius = {
  none: 0,
  sm: 4,
  md: 8,
  lg: 16,
  pill: 999,
} as const;

export type RadiusToken = keyof typeof radius;

/** Motion. Keep animation to moving or fading small things, so it also works on the panels. */
export const motion = {
  duration: {
    instant: 0,
    fast: 120,
    base: 200,
    slow: 320,
  },
  easing: {
    standard: 'cubic-bezier(0.2, 0, 0, 1)',
    out: 'cubic-bezier(0, 0, 0, 1)',
    in: 'cubic-bezier(0.3, 0, 1, 1)',
  },
} as const;

export const tokens = { color, font, type, space, radius, motion } as const;

export type Tokens = typeof tokens;
