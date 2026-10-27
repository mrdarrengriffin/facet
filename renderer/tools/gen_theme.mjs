#!/usr/bin/env node
// Theme schema generator (renderer/CONTRACT.md section 10 + Appendix A; design notes "theme engine" 3.1).
// tokens/theme.json (the rules) + colour.json, space.json, layout.json, type.json + catalogue/contract.json (which
// components are themeable) ->
//   themes/facet.json   the schema WITH Facet's values: every key a theme may set, at Facet's value. Every theme inherits
//                       it; never hand-edited (`--check` fails when it is out of date: build.sh runs that)
//   <out.h>             theme_gen.h: the same tables for core/theme_file.cpp (density-free: both densities' values, so a
//                       1x build refuses exactly what a 1.5x build refuses) + every built-in theme's text (themes/*.json)
// Usage: node tools/gen_theme.mjs <out.h>       write both (each only when it changes)
//        node tools/gen_theme.mjs --check       exit 1 if themes/facet.json is not what the tokens say
// Also imported (facetTheme(), schema()) by tests/functional/theme_*.test.mjs.
import { readFileSync, writeFileSync, existsSync, readdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { isLook, lookRange } from './lib/looks.mjs';

const R = join(dirname(fileURLToPath(import.meta.url)), '..');
const rd = (f) => JSON.parse(readFileSync(join(R, f), 'utf8'));
export const THEMES = join(R, 'themes');
const die = (m) => { throw new Error(`gen_theme: ${m}`); };

export function schema() {
  const th = rd('tokens/theme.json'), col = rd('tokens/colour.json'), sp = rd('tokens/space.json'), lay = rd('tokens/layout.json');
  const ty = rd('tokens/type.json'), contract = rd('catalogue/contract.json');
  const roles = col.accent.roles;
  const colours = Object.keys(col.colours);
  for (const c of colours) for (const m of th.modes) if (!/^#[0-9A-Fa-f]{6}$/.test(col.colours[c][m] ?? '')) die(`colour ${c}.${m} is not #RRGGBB`);
  // sizes: space.json (space, layout, radius, stroke, touch) + layout.json (screen geometry, all "layout:<name>")
  const sizes = new Map();
  const add = (n, v, v1) => {
    if (!Number.isInteger(v) || !Number.isInteger(v1)) die(`size ${n} is not whole px`);
    if (sizes.has(n)) die(`size ${n} is defined twice (space.json and layout.json)`);
    const [lo, hi] = lookRange(n, v), [lo1, hi1] = lookRange(n, v1);
    sizes.set(n, { name: n, look: isLook(n), v: [v, v1], lo: [lo, lo1], hi: [hi, hi1] });
  };
  for (const g of sp.groups.map((x) => x.id)) for (const [n, t] of Object.entries(sp.tokens[g])) add(`${g}:${n}`, t.v, t['1x']);
  for (const grp of Object.values(lay.tokens)) for (const [n, t] of Object.entries(grp)) if (typeof t?.v === 'number') add(`layout:${n}`, t.v, t['1x']);
  const sizeList = [...sizes.values()].sort((a, b) => (a.name < b.name ? -1 : 1));
  // type: the styles a theme may size (theme.json), with Facet's size and box at both densities
  const types = Object.entries(th.type.styles).map(([n, r]) => {
    const s = ty.styles[n] ?? die(`type style ${n} (theme.json) is not in type.json`);
    const size = [s.size, s['1x']?.size ?? s.size], box = [s.box, s['1x']?.box ?? s.box];
    for (const d of [0, 1]) if (size[d] < r.floor[d] || size[d] > box[d]) die(`type ${n}: Facet's own size ${size[d]} is outside [floor ${r.floor[d]}, box ${box[d]}]`);
    if (!th.roles.list.includes(r.role)) die(`type ${n}: role ${r.role} is not a font role`);
    return { name: n, size, box, floor: r.floor, role: r.role, font: s.font };
  });
  // font families: type.json families, by id (lower case, dashes); Facet's family per role = the one its styles use
  const famId = (f) => f.toLowerCase().replace(/[^a-z0-9]+/g, '-');
  // + the theme families of the font registry (fonts/fonts.json, C49): built in where a built-in theme names them
  const reg = rd('fonts/fonts.json');
  const families = [...Object.entries(ty.families).map(([k, f]) => ({ key: k, id: famId(f.family), name: f.family })),
    ...Object.entries(reg).filter(([k]) => k !== 'about' && !k.startsWith('$')).map(([k, f]) => ({ key: k, id: k, name: f.family }))];
  const fontOf = {};
  for (const r of th.roles.list) {
    const t = types.find((x) => x.role === r);
    fontOf[r] = famId(ty.families[t ? t.font : r === 'mono' ? 'mono' : 'sans'].family);
  }
  // mixes (colour.json): tint (optionally moved toward `toward` by desat) over `over` at opa: component opacities fixed
  const opa = (ref) => { const [c, k] = ref.split('.'); const v = col.components?.[c]?.[k]?.value; if (typeof v !== 'number') die(`mix opacity ${ref}`); return Math.round(v * 255); };
  const mixes = Object.entries(col.mixes).map(([n, m]) => ({ name: n, tint: m.tint, over: m.over, toward: m.toward ?? null, desat: m.toward ? opa(m.desat) : 0, opa: opa(m.opa) }));
  // the contrast pairs the guard checks: foundation sections + every contract component / screen's own section
  const ids = Object.keys(contract).map((k) => k.split('/')[1]);
  const sections = [...th.contrast.sections, ...ids];
  const kinds = col.contrast.kinds;
  const pairs = [];
  for (const p of col.contrast.pairs) if (sections.includes(p.section)) for (const bg of [p.bg].flat()) pairs.push({ section: p.section, fg: p.fg, bg, kind: p.kind, min: kinds[p.kind].min });
  const known = (n) => colours.includes(n) || mixes.some((m) => m.name === n);
  for (const p of pairs) for (const k of [p.fg, p.bg]) if (!known(k)) die(`contrast pair ${p.section}: unknown token ${k}`);
  // parts
  const parts = Object.entries(th.parts).filter(([k]) => k !== 'about').map(([n, p]) => {
    const keys = th.keys[p.keys] ?? die(`part ${n}: unknown key set ${p.keys}`);
    for (const t of [p.radius, p.height, p.room?.pad, p.room?.height, p.room?.stroke]) if (t && !sizes.has(t)) die(`part ${n}: unknown size token ${t}`);
    if (p.room?.less && !types.some((x) => x.name === p.room.less)) die(`part ${n}: room.less ${p.room.less} is not a themeable type style`);
    for (const c of [p.sits_on, p.face, ...p.texts]) if (c && !colours.includes(c)) die(`part ${n}: unknown colour ${c}`);
    return { name: n, ...p, keys };
  });
  const sets = col.accent.sets.map((s) => ({ id: s.id, label: s.label, dark: s.dark, light: s.light }));
  for (const s of sets) for (const m of th.modes) for (const r of roles) if (!/^#[0-9A-Fa-f]{6}$/.test(s[m]?.[r] ?? '')) die(`accent ${s.id}.${m}.${r}`);
  return { th, colours, roles, sizeList, types, families, fontOf, mixes, pairs, sections, kinds, parts, sets };
}

// themes/facet.json: the schema with Facet's values (compact: one entry per line)
export function facetTheme(S = schema()) {
  const t = {
    schema: S.th.schema, id: 'facet', label: 'Facet',
    about: 'Facet itself: the schema every theme inherits (GENERATED by tools/gen_theme.mjs from renderer/tokens; edit the tokens, not this file).',
    modes: S.th.modes,
    colour: Object.fromEntries(S.colours.filter((c) => !S.roles.includes(c)).map((c) => { const v = rd('tokens/colour.json').colours[c]; return [c, { dark: v.dark.toUpperCase(), light: v.light.toUpperCase() }]; })),
    accents: S.sets.map((s) => ({ id: s.id, label: s.label, ...Object.fromEntries(S.th.modes.map((m) => [m, Object.fromEntries(S.roles.map((r) => [r, s[m][r].toUpperCase()]))])) })),
    size: Object.fromEntries(S.sizeList.filter((s) => s.look).map((s) => [s.name, s.v])),
    type: Object.fromEntries(S.types.map((x) => [x.name, x.size])),
    font: S.fontOf,
    shape: { ...Object.fromEntries(Object.entries(S.th.shape).filter(([k]) => k !== 'about').map(([k, v]) => [k, v[0]])), snap: false },
    parts: {},
  };
  // one entry per line: readable, diffable, well under the file cap
  const lines = ['{'];
  const top = Object.entries(t);
  top.forEach(([k, v], i) => {
    const end = i < top.length - 1 ? ',' : '';
    if (v && typeof v === 'object' && !Array.isArray(v) && Object.keys(v).length && k !== 'font' && k !== 'shape') {
      lines.push(` ${JSON.stringify(k)}: {`);
      const es = Object.entries(v);
      es.forEach(([a, b], j) => lines.push(`  ${JSON.stringify(a)}: ${JSON.stringify(b)}${j < es.length - 1 ? ',' : ''}`));
      lines.push(` }${end}`);
    } else if (k === 'accents') {
      lines.push(` "accents": [`);
      v.forEach((a, j) => lines.push(`  ${JSON.stringify(a)}${j < v.length - 1 ? ',' : ''}`));
      lines.push(` ]${end}`);
    } else lines.push(` ${JSON.stringify(k)}: ${JSON.stringify(v)}${end}`);
  });
  lines.push('}', '');
  return lines.join('\n');
}

const cs = (s) => JSON.stringify(s);  // a C string literal (ASCII JSON escapes are valid C escapes for our data)
const hex = (h) => `0x${h.slice(1).toUpperCase()}`;
function cRaw(s) {  // any text as a C++ raw string (themes are JSON: no )" + delimiter clash with this delimiter)
  if (s.includes(')FXTHEME"')) die('a theme file contains the raw-string delimiter');
  return `R"FXTHEME(${s})FXTHEME"`;
}

export function header(S = schema()) {
  const { th, colours, roles, sizeList, types, families, fontOf, mixes, pairs, parts, sets } = S;
  const col = rd('tokens/colour.json');
  const inkIdx = (n) => { const c = colours.indexOf(n); if (c >= 0) return c; const m = mixes.findIndex((x) => x.name === n); if (m < 0) die(`ink ${n}`); return colours.length + m; };
  const sIdx = (n) => (n ? sizeList.findIndex((s) => s.name === n) : -1);
  const kindsList = Object.keys(S.kinds);
  const keyBits = ['face', 'radius', 'pad', 'edges', 'shadow', 'text', 'weight', 'ink', 'shape', 'buttons', 'w', 'lane', 'arrows', 'flush'];
  const mask = (ks) => ks.reduce((m, k) => { const i = keyBits.indexOf(k); if (i < 0) die(`key ${k}`); return m | (1 << i); }, 0);
  const lim = th.limits;
  const builtins = readdirSync(THEMES).filter((f) => f.endsWith('.json')).sort((a, b) => (a === 'facet.json' ? -1 : b === 'facet.json' ? 1 : a < b ? -1 : 1));
  const L = [
    '// GENERATED by tools/gen_theme.mjs from tokens/theme.json, colour.json, space.json, layout.json, type.json,',
    '// catalogue/contract.json and themes/*.json: edit those, not this file. Pure data (no LVGL): core/theme_file.cpp.',
    '#pragma once', '#include <stdint.h>', 'namespace fx { namespace tgen {',
    `constexpr int SCHEMA = ${th.schema};`,
    `constexpr int MAX_BYTES = ${lim.bytes}, MAX_DEPTH = ${lim.depth}, MAX_ERRORS = ${lim.errors}, MAX_VALUES = ${lim.values}, MAX_EDGES = ${lim.edges_per_block}, MAX_EDGES_TOTAL = ${lim.edges_total};`,
    `constexpr int ACCENTS_MIN = ${lim.accents_min}, ACCENTS_MAX = ${lim.accents_max}, ID_CHARS = ${lim.id_chars}, LABEL_CHARS = ${lim.label_chars}, ABOUT_CHARS = ${lim.about_chars}, MAX_BUTTONS = ${lim.buttons};`,
    `constexpr const char *MODES[] = {${th.modes.map(cs).join(', ')}};  // [0] dark, [1] light`,
    `constexpr int N_COLOURS = ${colours.length};`,
    `constexpr const char *COLOURS[] = {${colours.map(cs).join(', ')}};`,
    `constexpr uint32_t COLOUR_V[][2] = {${colours.map((c) => `{${hex(col.colours[c].dark)}, ${hex(col.colours[c].light)}}`).join(', ')}};`,
    `constexpr int N_ROLES = ${roles.length};`,
    `constexpr const char *ROLES[] = {${roles.map(cs).join(', ')}};`,
    `constexpr int ROLE_COLOUR[] = {${roles.map((r) => colours.indexOf(r)).join(', ')}};  // the colour token each accent role is (-1 = none)`,
    `constexpr int N_SETS = ${sets.length};`,
    'struct Set { const char *id, *label; uint32_t v[2][' + roles.length + ']; };',
    `constexpr Set SETS[] = {${sets.map((s) => `{${cs(s.id)}, ${cs(s.label)}, {{${roles.map((r) => hex(s.dark[r])).join(', ')}}, {${roles.map((r) => hex(s.light[r])).join(', ')}}}}`).join(',\n  ')}};`,
    '// mixes: ink index space = [0, N_COLOURS) colour tokens, [N_COLOURS, + N_MIXES) these; toward < 0 = no desaturation',
    `constexpr int N_MIXES = ${mixes.length};`,
    'struct Mix { const char *name; int tint, over, toward, desat, opa; };',
    `constexpr Mix MIXES[] = {${mixes.map((m) => `{${cs(m.name)}, ${inkIdx(m.tint)}, ${inkIdx(m.over)}, ${m.toward ? inkIdx(m.toward) : -1}, ${m.desat}, ${m.opa}}`).join(',\n  ')}};`,
    '// every size token, sorted by name: look = a theme may set it within lo..hi; v / lo / hi at [1.5x, 1x]',
    `constexpr int N_SIZES = ${sizeList.length};`,
    'struct Size { const char *name; bool look; int16_t v[2], lo[2], hi[2]; };',
    `constexpr Size SIZES[] = {${sizeList.map((s) => `{${cs(s.name)}, ${s.look}, {${s.v}}, {${s.lo}}, {${s.hi}}}`).join(',\n  ')}};`,
    `constexpr int N_FONT_ROLES = ${th.roles.list.length};`,
    `constexpr const char *FONT_ROLES[] = {${th.roles.list.map(cs).join(', ')}};`,
    `constexpr int N_FAMILIES = ${families.length};`,
    `constexpr const char *FAMILIES[] = {${families.map((f) => cs(f.id)).join(', ')}};  // the families this build has (type.json)`,
    `constexpr int FONT_DEFAULT[] = {${th.roles.list.map((r) => families.findIndex((f) => f.id === fontOf[r])).join(', ')}};  // Facet's family per role`,
    '// the type styles a theme may size: Facet size, line box, floor at [1.5x, 1x]; role = FONT_ROLES index',
    `constexpr int N_TYPES = ${types.length};`,
    'struct Type { const char *name; int16_t size[2], box[2], floor[2]; int role; };',
    `constexpr Type TYPES[] = {${types.map((t) => `{${cs(t.name)}, {${t.size}}, {${t.box}}, {${t.floor}}, ${th.roles.list.indexOf(t.role)}}`).join(',\n  ')}};`,
    `constexpr int N_STATES = ${th.states.list.length};`,
    `constexpr const char *STATES[] = {${th.states.list.map(cs).join(', ')}};  // merge order; bit i = 1 << i`,
    `constexpr const char *KEYS[] = {${keyBits.map(cs).join(', ')}};  // style block keys: bit i`,
    `constexpr int N_KEYS = ${keyBits.length};`,
    `constexpr int BASE_ONLY = ${mask(th.keys.base_only)};`,
    '// parts: keys mask; colour indexes (-1 none); texts up to 3; press = key mask; size indexes (-1 none); kind;',
    '// room = how far in edge layers may reach: ROOM_PAD (room_size), ROOM_HEIGHT (room_size less type room_less box,',
    '// halved), ROOM_STROKE (room_size less a pixel cell, halved), ROOM_BUTTONS, ROOM_WIDTH, ROOM_NONE (EDGE_PX); pad = may be padded',
    'enum Room { ROOM_NONE, ROOM_PAD, ROOM_HEIGHT, ROOM_STROKE, ROOM_BUTTONS, ROOM_WIDTH };',
    `constexpr int N_PARTS = ${parts.length};`,
    'struct Part { const char *name; const char *kind; int keys, sits_on, face, texts[3], n_texts, press, radius, height; Room room; int room_size, room_less; bool pad; };',
    `constexpr Part PARTS[] = {${parts.map((p) => { const r = p.room || {}, k = r.pad ? 'ROOM_PAD' : r.height ? 'ROOM_HEIGHT' : r.stroke ? 'ROOM_STROKE' : r.buttons ? 'ROOM_BUTTONS' : r.width ? 'ROOM_WIDTH' : 'ROOM_NONE';
      return `{${cs(p.name)}, ${cs(Object.entries(th.parts).find(([q]) => q === p.name)[1].keys)}, ${mask(p.keys)}, ${colours.indexOf(p.sits_on)}, ${p.face ? colours.indexOf(p.face) : -1}, {${[0, 1, 2].map((i) => (p.texts[i] ? colours.indexOf(p.texts[i]) : -1))}}, ${p.texts.length}, ${mask(p.press)}, ${sIdx(p.radius)}, ${sIdx(p.height)}, ${k}, ${sIdx(r.pad || r.height || r.stroke)}, ${r.less ? types.findIndex((x) => x.name === r.less) : -1}, ${!!p.pad}}`; }).join(',\n  ')}};`,
    `constexpr int EDGE_PX[2] = {${lim.edge_px}}, SHADOW_PX[2] = {${lim.shadow_px}};`,
    '// the names the parser itself relies on, resolved here (the generator fails if one goes): never looked up at run time',
    ...[['SZ_PIXEL', sIdx('stroke:pixel')], ['SZ_EDGE', sIdx('stroke:edge')], ['SZ_INDICATOR', sIdx('stroke:indicator')], ['SZ_ROW', sIdx('layout:row')], ['SZ_TOUCH', sIdx('touch:min')], ['SZ_TOP', sIdx('layout:top')], ['SZ_INSET', sIdx('layout:inset')], ['SZ_CARD_PAD', sIdx('layout:card-pad')], ['SZ_HAIR', sIdx('stroke:hair')],
      ['C_BG', colours.indexOf('bg')], ['C_SURFACE', colours.indexOf('surface')], ['C_TEXT_DIM', colours.indexOf('text-dim')], ['C_TEXT_MUTE', colours.indexOf('text-mute')],
      ['R_ACCENT', roles.indexOf('accent')], ...['card', 'control', 'knob', 'tick', 'tick-major', 'scrollbar', 'caption', 'header', 'switch', 'page', 'card-head', 'segment'].map((n) => [`P_${n.toUpperCase().replace('-', '_')}`, parts.findIndex((x) => x.name === n)]),
      ...th.states.list.map((s) => [`ST_${s.toUpperCase()}`, th.states.list.indexOf(s)])].map(([n, v]) => { if (v < 0) die(`the parser needs ${n}`); return `constexpr int ${n} = ${v};`; }),
    `constexpr const char *OUTER[] = {${th.contrast.outer.map(cs).join(', ')}};`,
    `constexpr int N_OUTER = ${th.contrast.outer.length};`,
    `constexpr double MIN_TEXT = ${th.contrast.text}, MIN_LARGE = ${th.contrast.large}, MIN_UI = ${th.contrast.ui}, MIN_DISABLED = ${th.contrast.disabled};`,
    '// declared contrast pairs in the theme scope (colour.json contrast.pairs of the sections below): fg / bg ink index',
    `constexpr const char *SECTIONS = ${cs(S.sections.join(', '))};`,
    `constexpr const char *KINDS[] = {${kindsList.map(cs).join(', ')}};`,
    `constexpr int N_PAIRS = ${pairs.length};`,
    'struct Pair { const char *section; int fg, bg, kind; double min; };',
    `constexpr Pair PAIRS[] = {${pairs.map((p) => `{${cs(p.section)}, ${inkIdx(p.fg)}, ${inkIdx(p.bg)}, ${kindsList.indexOf(p.kind)}, ${p.min}}`).join(',\n  ')}};`,
  ];
  for (const [k, v] of Object.entries(th.shape)) if (k !== 'about') L.push(`constexpr const char *SHAPE_${k.toUpperCase()}[] = {${v.map(cs).join(', ')}};`, `constexpr int N_SHAPE_${k.toUpperCase()} = ${v.length};`);
  for (const [k, v] of Object.entries(th.values)) if (k !== 'about') L.push(`constexpr const char *${k.toUpperCase()}[] = {${v.map(cs).join(', ')}};`, `constexpr int N_${k.toUpperCase()} = ${v.length};`);
  // pixel icons (shape.icons pixel): tokens/icons.json "pixel" = 8 rows of hex, high bit = left
  const px = rd('tokens/icons.json').icons.filter((i) => i.pixel);
  for (const i of px) if (i.pixel.length !== 8 || i.pixel.some((r) => !/^[0-9A-Fa-f]{2}$/.test(r))) die(`icons.json ${i.name}: pixel = 8 rows of 2 hex digits`);
  L.push('// pixel icons (shape.icons pixel, tokens/icons.json "pixel"): 8 x 8, rows top down, bit 7 = left; sorted by name',
    'struct PixelIcon { const char *name; uint8_t rows[8]; };',
    `constexpr PixelIcon PIXEL_ICONS[] = {${[...px].sort((a, b) => (a.name < b.name ? -1 : 1)).map((i) => `{${cs(i.name)}, {${i.pixel.map((r) => '0x' + r.toUpperCase()).join(', ')}}}`).join(',\n  ')}};`,
    `constexpr int N_PIXEL_ICONS = ${px.length};`);
  L.push('// the built-in themes (themes/*.json, facet first): each is validated when the build runs (tools/theme_check.mjs)',
    `constexpr int N_BUILTINS = ${builtins.length};`, 'struct Builtin { const char *id; const char *json; };',
    `constexpr Builtin BUILTINS[] = {${builtins.map((f) => `{${cs(f.replace(/\.json$/, ''))}, ${cRaw(readFileSync(join(THEMES, f), 'utf8'))}}`).join(',\n  ')}};`,
    '}}  // namespace fx::tgen', '');
  return L.join('\n');
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  try {
    const S = schema(), f = join(THEMES, 'facet.json'), want = facetTheme(S);
    if (process.argv[2] === '--check') {
      if (!existsSync(f) || readFileSync(f, 'utf8') !== want) { console.error('gen_theme: themes/facet.json is out of date: run node tools/gen_theme.mjs <out.h>'); process.exit(1); }
      process.exit(0);
    }
    const out = process.argv[2];
    if (!out) { console.error('usage: gen_theme.mjs <out.h> | --check'); process.exit(2); }
    if (!existsSync(f) || readFileSync(f, 'utf8') !== want) writeFileSync(f, want);
    const h = header(S);
    if (!existsSync(out) || readFileSync(out, 'utf8') !== h) writeFileSync(out, h);
  } catch (e) { console.error(e.message); process.exit(1); }
}
