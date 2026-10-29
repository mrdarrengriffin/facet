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
  // chart.json geometry is a size token too (chart:bar-gap ...): fx::size, never the compiled fw::ch:: number
  chart: new RegExp(`\\b(fw::)?ch::(${Object.keys(JSON.parse(readFileSync(join(DIR, '..', 'tokens', 'chart.json'), 'utf8')).geometry).map((n) => n.replace(/-/g, '_')).join('|')})\\b`),
};
// the CORE code a migrated component's sizes come from (2026-10-28: Toast, Bottom sheet, Bar chart and Sparkline were
// migrated while the core placed / drew them at compiled sizes, so the Layout lab had no slider for them): here a
// size is read live (fx::size("layout:inset"), re-placed on LV_EVENT_STYLE_CHANGED), never a compiled constant.
// [file, first line of the section | null = whole file, the next section's line]
const CORE = [['core/toasts.cpp'], ['core/sheet.cpp'], ['core/bins_art.cpp'],
  ['foundations/chart.cpp', '// ---------------------------------------------------------------- the line chart', '// ---------------------------------------------------------------- bars'],
  ['foundations/chart.cpp', '// ---------------------------------------------------------------- token bars', '// ---------------------------------------------------------------- page sections']];
const SIZE_NS = /\b(fw::)?(lay|sp|rad|stroke|touch)::[a-zA-Z_]/;  // sizes (type and icon styles are not slider sizes)
const coreBad = [];
for (const [f, from, to] of CORE) {
  const all = readFileSync(join(DIR, '..', f), 'utf8');
  const a = from ? all.indexOf(from) : 0, b = to ? all.indexOf(to, a) : all.length;
  if (a < 0 || b < 0) { coreBad.push(`${f}: core section "${from}" .. "${to}" not found (renamed? update CORE in tools/contract_lint.mjs)`); continue; }
  const first = all.slice(0, a).split('\n').length;
  all.slice(a, b).split('\n').forEach((line, i) => {
    const code = line.replace(/\/\/.*$/, '');
    for (const [r, re] of [['fixed', SIZE_NS], ['chart', RULES.chart]]) if (re.test(code)) coreBad.push(`${f}:${first + i}: core ${r}: a compiled size where a migrated component's size comes from; fx::size("<token>"): ${line.trim().slice(0, 100)}`);
  });
}
const mode = process.argv[2] || '--summary';
const bad = [...coreBad], debts = [], legacy = {}, perComp = [];
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
    // C8b: a tap target inside a component is another component (fw::find), never a hand-built one: a box this file
    // makes (fx::box) that takes taps or a press look is a copied control. A real part of one control (a Segmented's
    // segment) says so on that line: `// part: <why>`.
    const boxes = new Set([...code.matchAll(/\b(\w+)\s*=\s*fx::box\(/g)].map((m) => m[1]));
    src.split('\n').forEach((line, i) => {
      if (/\/\/\s*part:/.test(line)) return;
      const debt = /\/\/\s*debt:/.test(line);  // a known C8b gap: listed on every run, never hidden
      const c = line.replace(/\/\/.*$/, '');
      const m = c.match(/fw::st::on_tap\((\w+)|lv_obj_(?:add|set)_flag\((\w+),[^;]*CLICKABLE|fx::use\((\w+), "(?:bg:pressed-tint|fade-press[a-z-]*)", LV_STATE_PRESSED/);
      const v = m && (m[1] || m[2] || m[3]);
      if (v && boxes.has(v)) (debt ? debts : bad).push(`${f}:${i + 1}: control: a hand-built tap target (${v}); use the component (fw::find), C8b: ${line.trim().slice(0, 100)}`);
    });
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
debts.forEach((l) => console.log('DEBT ' + l));
bad.forEach((l) => console.log('FAIL ' + l));
process.exit(bad.length ? 1 : 0);
