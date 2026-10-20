import {
  color,
  density,
  devices,
  font,
  layout,
  motion,
  opacity,
  radius,
  screen,
  screenType,
  size,
  space,
  stroke,
  type,
  screenTypeAt,
  type DensityId,
  type ThemeName,
  type TypeStyle,
} from './tokens.js';

const PREFIX = 'facet';

/** The CSS custom property for a token, e.g. `cssVar('color', 'accent')` → `--facet-color-accent`. */
export function cssVar(group: string, name: string): string {
  return `--${PREFIX}-${group}-${name}`;
}

const px = (n: number) => `${n}px`;

function vars(
  group: string,
  obj: Record<string, number | string>,
  unit: (n: number) => string = px,
): string[] {
  return Object.entries(obj)
    .filter(([k, v]) => !k.startsWith('$') && typeof v === 'number')
    .map(([k, v]) => `${cssVar(group, k)}: ${unit(v as number)};`);
}

function colorVars(theme: ThemeName): string[] {
  return [
    'color-scheme: ' + theme + ';',
    ...Object.entries(color).map(([name, value]) => `${cssVar('color', name)}: ${value[theme]};`),
  ];
}

function staticVars(): string[] {
  return [
    `${cssVar('font', 'sans')}: '${font.sans.family}', ${font.sans.fallback};`,
    `${cssVar('font', 'mono')}: '${font.mono.family}', ${font.mono.fallback};`,
    ...vars('space', space),
    ...vars('radius', radius),
    ...vars('stroke', stroke),
    ...vars('size', size),
    ...vars('layout', layout),
    ...vars('duration', motion.duration, (n) => `${n}ms`),
    ...Object.entries(motion.easing).map(([k, v]) => `${cssVar('easing', k)}: ${v.css};`),
    ...vars('opacity', opacity, (n) => String(n)),
    ...boxVars(screenType),
    `--${PREFIX}-scale: 1;`,
    `--${PREFIX}-screen-w: ${screen.width}px;`,
    `--${PREFIX}-screen-h: ${screen.height}px;`,
  ];
}

/** `--facet-fst-<role>-box` / `-weight`: each screen type role's line box and weight, for parts that borrow them. */
function boxVars(styles: Record<string, TypeStyle>): string[] {
  return Object.entries(styles)
    .filter(([, s]) => s.box)
    .flatMap(([k, s]) => [
      `${cssVar('fst', k + '-box')}: ${s.box}px;`,
      `${cssVar('fst', k + '-weight')}: ${s.weight};`,
    ]);
}

/** A density's overrides: its layout / size vars and its screen type scale, under `[data-facet-density]`. */
function densityBlocks(): string[] {
  return (Object.keys(density) as (DensityId | 'reference')[])
    .filter((d): d is DensityId => d !== 'reference' && d !== density.reference)
    .flatMap((d) => {
      const t = density[d];
      const sel = `[data-facet-density="${d}"]`;
      const st = screenTypeAt(d);
      return [
        block(sel, [
          `--${PREFIX}-scale: ${t.scale};`,
          ...vars('layout', (t.layout ?? {}) as Record<string, number>),
          ...vars('size', (t.size ?? {}) as Record<string, number>),
          ...boxVars(st),
        ]),
        // :where() keeps the specificity of the plain class, so component CSS that sets a line height still wins
        ...typeClasses('fst', st).map((c) => `:where(${sel}) ${c}`),
      ];
    });
}

/** `[data-facet-device="<id>"]`: the device's screen size. */
function deviceBlocks(): string[] {
  return Object.entries(devices).map(([id, d]) =>
    block(`[data-facet-device="${id}"]`, [
      `--${PREFIX}-screen-w: ${d.w}px;`,
      `--${PREFIX}-screen-h: ${d.h}px;`,
    ]),
  );
}

/** One class per type style. Styles with a `box` use it as the line height, so text boxes sit on the 8 px grid. */
function typeClasses(prefix: string, styles: Record<string, TypeStyle>): string[] {
  return Object.entries(styles).map(([name, s]) => {
    const lh = s.box ? `${s.box}px` : String(s.lineHeight);
    return (
      `.${prefix}-${name} { font-family: var(${cssVar('font', s.font)}); font-size: ${s.size}px; font-weight: ${s.weight}; ` +
      `line-height: ${lh}; letter-spacing: ${s.letterSpacing}px; text-transform: ${s.case === 'upper' ? 'uppercase' : 'none'}; }`
    );
  });
}

function block(selector: string, lines: string[], indent = ''): string {
  const body = lines.map((line) => `${indent}  ${line}`).join('\n');
  return `${indent}${selector} {\n${body}\n${indent}}`;
}

/**
 * Render every token as CSS.
 *
 * - Custom properties: `--facet-color-*` (per theme), `--facet-space-*`, `--facet-radius-*`, `--facet-stroke-*`,
 *   `--facet-size-*`, `--facet-layout-*`, `--facet-duration-*`, `--facet-easing-*`, `--facet-opacity-*`,
 *   `--facet-font-sans|mono`.
 * - Type classes: `.fst-<role>` for the screen scale, `.ft-<name>` for the component scale.
 *
 * Dark is the default. Light applies when the OS prefers it, and either theme can be forced with
 * `data-facet-theme="dark|light"` on any element, so a preview can differ from the page around it.
 */
export function toCss(): string {
  return [
    '/* Generated from @facet/tokens. Do not edit by hand. */',
    block(':root', staticVars()),
    block(':root,\n[data-facet-theme="dark"]', colorVars('dark')),
    `@media (prefers-color-scheme: light) {\n${block(':root:not([data-facet-theme="dark"])', colorVars('light'), '  ')}\n}`,
    block('[data-facet-theme="light"]', colorVars('light')),
    ...typeClasses('fst', screenType),
    ...typeClasses('ft', type),
    ...deviceBlocks(),
    ...densityBlocks(),
    '',
  ].join('\n\n');
}
