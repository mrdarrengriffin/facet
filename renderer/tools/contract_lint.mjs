#!/usr/bin/env node
// Contract lint (renderer/CONTRACT.md). A component whose meta says "contract": 2 is MIGRATED: it may not know the
// screen or card width, hold or resolve a colour, draw outside the pen, or set local styles / sizes (it fills its
// parent through core/fx.h). A violation in a migrated component fails (build.sh, facet-gate). Unmarked components are
// legacy: counted per rule = the migration backlog, never a failure. Rule parts: a migrated component's meta "parts" =
// the parts its code attaches / draws (C46).
//   node tools/contract_lint.mjs [--summary | --list]   (--list = the legacy backlog per component)
import { readFileSync, readdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
const DIR = join(dirname(fileURLToPath(import.meta.url)), '..', 'components');
const RULES = {
  width: /\blay::(screen_w|screen_h|card_w|content_w)\b/,
  // + the theme file engine is core only (C44): a component never sees a Skin, a theme file or the live theme
  theme: /\bfw::T\b|\bT->|\btheme_of\(|\bfw::Theme\b|\blv_color_(hex|make)\(|theme_file\.h|\bSkin\b|\bskin::|\btheme_(load|live|apply|facet|generation)\(/,
  draw: /\blv_draw_/,
  local: /\blv_obj_set_style_|\blv_style_init\(|\blv_obj_set_(size|width|height|pos|x|y)\(/,
  // Header round 3 (2026-10-27): no colour / font / symbol types (C16) and no fixed size numbers (sizes are token
  // NAMES through fx, so fx::tune and the Layout lab move them: C21, C23)
  font: /\blv_font_|\bLV_SYMBOL_|\blv_color_t\b|\blv_color_(white|black|mix)\(/,
  fixed: /\b(fw::)?(lay|sp|rad|stroke|touch|ic|ty)::[a-zA-Z_]/,
};
const mode = process.argv[2] || '--summary';
const bad = [], legacy = {}, perComp = [];
let migrated = 0;
for (const f of readdirSync(DIR).filter((x) => x.endsWith('.cpp')).sort()) {
  const src = readFileSync(join(DIR, f), 'utf8');
  const isV2 = /"contract":\s*2\b/.test(src);
  if (isV2) migrated++;
  const hits = {};
  src.split('\n').forEach((line, i) => {
    const code = line.replace(/\/\/.*$/, '');
    for (const [r, re] of Object.entries(RULES)) if (re.test(code)) {
      hits[r] = (hits[r] || 0) + 1;
      if (isV2) bad.push(`${f}:${i + 1}: ${r}: ${line.trim().slice(0, 120)}`);
    }
  });
  // parts (C46): the parts a migrated component names (fx::use "part:<p>", the Pen's knob / tick / arc parts) = its meta
  // "parts" list, so docs and the theme guards know which component has which part (text + icon parts are the core's)
  if (isV2) {
    const code = src.replace(/\/\/.*$/gm, ''), used = new Set([...code.matchAll(/"part:([a-z-]+)"/g)].map((m) => m[1]));
    if (/\.knob\(/.test(code) || /\.dot\([^;]*"knob"\)/.test(code)) used.add('knob');
    for (const a of ['track', 'track-fill']) if (new RegExp(`\\.line\\([^;]*"${a}"\\)`).test(code)) used.add(a);
    if (/\.tick\(/.test(code)) { used.add('tick'); if (/"tick-major"/.test(code)) used.add('tick-major'); }
    for (const a of ['arc', 'arc-fill']) if (new RegExp(`\\.arc\\([^;]*"${a}"\\)`).test(code)) used.add(a);
    const meta = src.match(/"parts":\s*\[([^\]]*)\]/);
    const said = new Set(meta ? [...meta[1].matchAll(/"([a-z-]+)"/g)].map((m) => m[1]) : []);
    const diff = [...used].filter((x) => !said.has(x)).concat([...said].filter((x) => !used.has(x)));
    if (!meta) bad.push(`${f}: parts: no meta "parts" list (C46)`);
    else if (diff.length) bad.push(`${f}: parts: meta "parts" [${[...said]}] != the parts it uses [${[...used]}]`);
  }
  if (!isV2 && Object.keys(hits).length) {
    perComp.push(`${f.replace('.cpp', '')} ${Object.entries(hits).map(([k, v]) => `${k}=${v}`).join(' ')}`);
    for (const [k, v] of Object.entries(hits)) { legacy[k] = legacy[k] || { comps: 0, lines: 0 }; legacy[k].comps++; legacy[k].lines += v; }
  }
}
const sum = Object.entries(legacy).map(([k, v]) => `${k} ${v.comps} comps/${v.lines} lines`).join(', ');
console.log(`contract lint: ${migrated} migrated, ${bad.length} violation(s); legacy backlog: ${perComp.length} components (${sum})`);
if (mode === '--list') perComp.forEach((l) => console.log('  ' + l));
bad.forEach((l) => console.log('FAIL ' + l));
process.exit(bad.length ? 1 : 0);
