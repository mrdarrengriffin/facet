/**
 * Tokens → ESPHome / LVGL YAML, for building panel firmware from the same tokens as the web components.
 *
 * The output is meant to be included as an ESPHome package (`packages: { tokens: !include tokens.yaml }`):
 *
 * - `substitutions`: `c_<colour>` (dark) and `cl_<colour>` (light) as `0xRRGGBB`, plus `r_` radius, `w_` stroke,
 *   `z_` size, `s_` space, `l_` screen layout, `d_` duration and `o_` opacity values.
 * - `font`: one entry per firmware font id used by the two type scales (Inter and JetBrains Mono via Google Fonts).
 * - `lvgl.style_definitions`: text styles, buttons (kind × state), icon buttons, card, tile, chip and focus ring.
 *
 * Nothing in the firmware uses this yet. It's here so the web and the panels can share one set of numbers.
 */
import {
  color,
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

export interface EsphomeOptions {
  /** Which colour set the style definitions use. Substitutions always carry both. Default `dark`. */
  theme?: ThemeName;
}

const FAMILY = { sans: 'Inter', mono: 'JetBrains Mono' } as const;
const ident = (s: string) => s.replace(/-/g, '_');
const hex = (v: string) => `0x${v.replace('#', '').toUpperCase()}`;
const q = (v: string | number) => `"${v}"`;

export function toEsphome(options: EsphomeOptions = {}): string {
  const theme = options.theme ?? 'dark';
  const C = Object.fromEntries(Object.entries(color).map(([k, v]) => [k, hex(v[theme])])) as Record<
    keyof typeof color,
    string
  >;
  const out: string[] = [
    '# Generated from @facet/tokens. Do not edit by hand.',
    `# Colour set used by style_definitions: ${theme}.`,
    'substitutions:',
  ];
  for (const [k, v] of Object.entries(color)) out.push(`  c_${ident(k)}: ${q(hex(v.dark))}`);
  for (const [k, v] of Object.entries(color)) out.push(`  cl_${ident(k)}: ${q(hex(v.light))}`);
  const groups: [string, Record<string, number>][] = [
    ['r', radius],
    ['w', stroke],
    ['z', size],
    ['s', space],
    ['l', layout],
  ];
  for (const [pre, group] of groups)
    for (const [k, v] of Object.entries(group)) out.push(`  ${pre}_${ident(k)}: ${q(v)}`);
  for (const [k, v] of Object.entries(motion.duration)) out.push(`  d_${ident(k)}: ${q(`${v}ms`)}`);
  for (const [k, v] of Object.entries(opacity))
    out.push(`  o_${ident(k)}: ${q(`${Math.round(v * 100)}%`)}`);

  // Fonts: one per firmware id. The same id must always mean the same family, weight and size.
  const fonts = new Map<string, { family: string; weight: number; size: number }>();
  const styles: [string, TypeStyle][] = [...Object.entries(screenType), ...Object.entries(type)];
  for (const [name, s] of styles) {
    const f = { family: FAMILY[s.font], weight: s.weight, size: s.size };
    const seen = fonts.get(s.lvgl);
    if (seen && (seen.family !== f.family || seen.weight !== f.weight || seen.size !== f.size)) {
      throw new Error(
        `type ${name}: font id ${s.lvgl} is already used for ${seen.family} ${seen.weight} ${seen.size}`,
      );
    }
    fonts.set(s.lvgl, f);
  }
  out.push('font:');
  for (const [id, f] of fonts) {
    out.push(
      `  - id: ${id}`,
      `    file: {type: gfonts, family: ${f.family}, weight: ${f.weight}}`,
      `    size: ${f.size}`,
      '    bpp: 4',
    );
    out.push(
      ...(f.size >= 100
        ? ['    glyphs: ["0123456789:%-°"]']
        : ['    glyphsets: [GF_Latin_Core]', '    ignore_missing_glyphs: true']),
    );
  }

  type Style = Record<string, string | number>;
  const S: [string, Style][] = [];
  for (const [name, s] of Object.entries(screenType))
    S.push([
      `st_${ident(name)}`,
      { text_font: s.lvgl, ...(s.letterSpacing ? { text_letter_space: s.letterSpacing } : {}) },
    ]);
  for (const [name, s] of Object.entries(type))
    S.push([
      `t_${ident(name)}`,
      { text_font: s.lvgl, ...(s.letterSpacing ? { text_letter_space: s.letterSpacing } : {}) },
    ]);
  const btn: Style = {
    radius: radius.button,
    height: size['button-h'],
    pad_left: size['button-pad-x'],
    pad_right: size['button-pad-x'],
    border_width: stroke.hair,
    shadow_width: 0,
    text_font: type.label.lvgl,
  };
  const kinds: Record<string, [string, Style][]> = {
    primary: [
      [
        '',
        { bg_color: C.accent, bg_opa: 'COVER', border_opa: 'TRANSP', text_color: C['on-accent'] },
      ],
      ['pressed', { bg_color: C['accent-press'] }],
      ['disabled', { bg_color: C['accent-dim'], text_color: C['text-mute'] }],
    ],
    secondary: [
      ['', { bg_color: C.surface, bg_opa: 'COVER', border_color: C.line, text_color: C.text }],
      ['pressed', { bg_color: C['surface-press'], border_color: C['line-hi'] }],
      ['disabled', { text_color: C['text-mute'] }],
    ],
    outline: [
      ['', { bg_opa: 'TRANSP', border_color: C.accent, text_color: C['accent-text'] }],
      ['pressed', { bg_color: C['accent-dim'], bg_opa: 'COVER' }],
      ['checked', { bg_color: C.accent, bg_opa: 'COVER', text_color: C['on-accent'] }],
      ['disabled', { border_color: C['accent-dim'], text_color: C['accent-dim'] }],
    ],
    ghost: [
      ['', { bg_opa: 'TRANSP', border_opa: 'TRANSP', text_color: C['text-dim'] }],
      ['pressed', { bg_color: C['surface-press'], bg_opa: 'COVER', text_color: C.text }],
      ['checked', { text_color: C['accent-text'] }],
      ['disabled', { text_color: C['text-mute'] }],
    ],
  };
  for (const [k, states] of Object.entries(kinds)) {
    for (const [st, d] of states)
      S.push([`btn_${k}${st ? `_${st}` : ''}`, { ...(st ? {} : btn), ...d }]);
  }
  const ibtns: [string, number, number][] = [
    ['play', size['icon-play'], 999],
    ['round', size['icon-round'], 999],
    ['square', size['icon-square'], radius.square],
  ];
  for (const [k, sz, rad] of ibtns) {
    const base: Style = {
      width: sz,
      height: sz,
      radius: rad,
      pad_all: 0,
      shadow_width: 0,
      border_width: k === 'play' ? 0 : stroke.hair,
    };
    const col: Style =
      k === 'play'
        ? { bg_color: C.accent, bg_opa: 'COVER', text_color: C['on-accent'] }
        : { bg_color: C.surface, bg_opa: 'COVER', border_color: C.line, text_color: C.text };
    S.push([`ibtn_${k}`, { ...base, ...col }]);
    S.push([
      `ibtn_${k}_pressed`,
      k === 'play'
        ? { bg_color: C['accent-press'] }
        : { bg_color: C['surface-press'], border_color: C['line-hi'] },
    ]);
  }
  S.push(
    [
      'focus_ring',
      { outline_width: stroke.focus, outline_color: C.accent, outline_pad: size['focus-offset'] },
    ],
    [
      'card',
      {
        radius: radius.card,
        bg_color: C.surface,
        bg_opa: 'COVER',
        border_width: stroke.hair,
        border_color: C.line,
        pad_all: 0,
        shadow_width: 0,
      },
    ],
    ['card_pressed', { bg_color: C['surface-press'], border_color: C['line-hi'] }],
    [
      'tile',
      {
        radius: radius.tile,
        bg_color: C.surface,
        bg_opa: 'COVER',
        border_width: stroke.hair,
        border_color: C.line,
        pad_all: 0,
        shadow_width: 0,
      },
    ],
    [
      'chip',
      {
        height: size['chip-h'],
        radius: radius.chip,
        pad_left: space['2'],
        pad_right: space['2'],
        bg_opa: `${Math.round(opacity['chip-bg'] * 100)}%`,
        border_width: 0,
        text_font: type['caps-sm'].lvgl,
      },
    ],
  );
  out.push('lvgl:', '  style_definitions:');
  for (const [id, d] of S) {
    out.push(`    - id: ${id}`);
    for (const [k, v] of Object.entries(d))
      out.push(`      ${k}: ${typeof v === 'string' && v.startsWith('0x') ? q(v) : v}`);
  }
  return out.join('\n') + '\n';
}
