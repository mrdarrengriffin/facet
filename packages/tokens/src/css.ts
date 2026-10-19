import { color, font, motion, radius, space, type, type ThemeName } from './tokens.js';

const PREFIX = 'facet';

/** The CSS custom property name for a token, e.g. `cssVar('color', 'accent')` → `--facet-color-accent`. */
export function cssVar(group: string, name: string): string {
  return `--${PREFIX}-${group}-${name}`;
}

function colorVars(theme: ThemeName): string[] {
  return Object.entries(color).map(([name, value]) => `${cssVar('color', name)}: ${value[theme]};`);
}

function staticVars(): string[] {
  const lines: string[] = [];

  for (const [name, value] of Object.entries(font)) {
    lines.push(`${cssVar('font', name)}: '${value.family}', ${value.fallback};`);
  }
  for (const [name, style] of Object.entries(type)) {
    lines.push(`${cssVar('type', `${name}-font`)}: var(${cssVar('font', style.font)});`);
    lines.push(`${cssVar('type', `${name}-size`)}: ${style.size}px;`);
    lines.push(`${cssVar('type', `${name}-weight`)}: ${style.weight};`);
    lines.push(`${cssVar('type', `${name}-line-height`)}: ${style.lineHeight};`);
    lines.push(`${cssVar('type', `${name}-letter-spacing`)}: ${style.letterSpacing}px;`);
  }
  for (const [name, value] of Object.entries(space)) {
    lines.push(`${cssVar('space', name)}: ${value}px;`);
  }
  for (const [name, value] of Object.entries(radius)) {
    lines.push(`${cssVar('radius', name)}: ${value}px;`);
  }
  for (const [name, value] of Object.entries(motion.duration)) {
    lines.push(`${cssVar('duration', name)}: ${value}ms;`);
  }
  for (const [name, value] of Object.entries(motion.easing)) {
    lines.push(`${cssVar('easing', name)}: ${value};`);
  }
  return lines;
}

function block(selector: string, lines: string[], indent = ''): string {
  const body = lines.map((line) => `${indent}  ${line}`).join('\n');
  return `${indent}${selector} {\n${body}\n${indent}}`;
}

/**
 * Render every token as CSS custom properties.
 *
 * Dark is the default. Light applies when the OS prefers it, and either theme can be forced
 * with `data-facet-theme="dark|light"` on any element.
 */
export function toCss(): string {
  return [
    '/* Generated from @facet/tokens. Do not edit by hand. */',
    block(':root', staticVars()),
    block(':root,\n[data-facet-theme="dark"]', colorVars('dark')),
    `@media (prefers-color-scheme: light) {\n${block(':root:not([data-facet-theme="dark"])', colorVars('light'), '  ')}\n}`,
    block('[data-facet-theme="light"]', colorVars('light')),
    '',
  ].join('\n\n');
}
