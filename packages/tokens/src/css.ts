import {
  color,
  font,
  layout,
  motion,
  opacity,
  radius,
  screenType,
  size,
  space,
  stroke,
  type,
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
  ];
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
    '',
  ].join('\n\n');
}
