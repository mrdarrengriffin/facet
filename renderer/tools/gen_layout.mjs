#!/usr/bin/env node
// tokens/layout.json -> C++ header: fw::lay:: screen geometry (inset, gutter, group, top, header row, section gap, more
// row, sheet / toast positions, plus the derived screen_w/h, content_w/h and header_gap) as device px for this density
// (#ifdef FW_1X), and fw::lyt:: the tables for the Layout page (tokens at both densities, devices, size classes,
// densities) and its sections. Component sizes (card, tile, list row, dial) stay in space.json, also under fw::lay::.
// Run by build.sh / firmware/bundle.mjs: `node tools/gen_layout.mjs <out.h>`; writes only when the output changes.
import { readFileSync, writeFileSync, existsSync, renameSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
const t = JSON.parse(readFileSync(join(HERE, '..', 'tokens', 'layout.json'), 'utf8'));
const space = JSON.parse(readFileSync(join(HERE, '..', 'tokens', 'space.json'), 'utf8'));
const motion = JSON.parse(readFileSync(join(HERE, '..', 'tokens', 'motion.json'), 'utf8'));
const sound = JSON.parse(readFileSync(join(HERE, '..', 'tokens', 'sound.json'), 'utf8'));
const out = process.argv[2];
if (!out) { console.error('usage: gen_layout.mjs <out.h>'); process.exit(2); }
const die = (m) => { console.error(`tokens/layout.json: ${m}`); process.exit(1); };
const cstr = (s) => JSON.stringify(String(s ?? ''));
const field = (n) => n.replace(/-/g, '_');
const flt = (x) => `${Number(x).toFixed(4).replace(/0+$/, '0')}f`;

const rows = [];
for (const g of t.groups) {
  if (!t.tokens[g.id]) die(`group ${g.id} has no tokens`);
  for (const [n, v] of Object.entries(t.tokens[g.id])) {
    if (!/^[a-z0-9][a-z0-9-]*$/.test(n)) die(`${g.id}.${n}: bad name`);
    if (!Number.isInteger(v.v) || !Number.isInteger(v['1x'])) die(`${g.id}.${n}: v and 1x must be integers`);
    rows.push({ g: g.id, n, ...v });
  }
}
for (const g of Object.keys(t.tokens)) if (!t.groups.some((x) => x.id === g)) die(`tokens.${g}: not in groups`);
// one name, one home: fw::lay:: is shared with space.json's layout group (component sizes)
const spaceLay = new Set(Object.keys(space.tokens.layout ?? {}));
const seen = new Set();
for (const r of rows) {
  if (seen.has(r.n)) die(`${r.n}: defined twice`);
  if (spaceLay.has(r.n)) die(`${r.n}: also in space.json layout (one home per token)`);
  seen.add(r.n);
}
const V = (n, d) => { const r = rows.find((x) => x.n === n); if (!r) die(`missing token ${n}`); return d === '1x' ? r['1x'] : r.v; };

// the grid rule (Darren 2026-10-21): every value a multiple of 4 at both densities, at least 4
const bad = [];
for (const r of rows)
  for (const [d, x] of [['720 (v)', r.v], ['480 (1x)', r['1x']]])
    if (x < 4 || x % 4) bad.push(`${r.g}.${r.n} ${d} = ${x}: ${x < 4 ? 'below 4' : 'not a multiple of 4'}${x % 2 ? ' (odd)' : ''}`);
// the frame adds up at both densities: header row inside the header zone, content area on the grid
const dens = ['1.5x', '1x'];
const derived = {};
for (const d of dens) {
  const S = t.screen[d];
  if (!S) die(`screen.${d} missing`);
  const gap = V('top', d) - V('inset', d) - V('row', d);
  const cw = S.w - 2 * V('inset', d), ch = S.h - V('inset', d) - V('top', d);
  if (gap < 4) bad.push(`${d}: top must leave a header gap below the header row (top - inset - row = ${gap})`);
  for (const [k, x] of [['header gap', gap], ['content width', cw], ['content height', ch], ['screen width', S.w], ['screen height', S.h]])
    if (x % 4) bad.push(`${d}: ${k} ${x} not a multiple of 4`);
  if (V('header-time-w', d) + V('row', d) + 2 * V('gutter', d) > cw) bad.push(`${d}: header items do not fit the content width`);
  if (V('toast-top', d) + V('toast-h', d) > S.h - V('inset', d)) bad.push(`${d}: a toast at toast-top runs past the bottom inset`);
  derived[d] = { screen_w: S.w, screen_h: S.h, content_w: cw, content_h: ch, header_gap: gap };
}
// toast behaviour group (docs tokens.json `toast`): references checked against motion.json / sound.json, distances on the grid
const TO = t.toast;
if (!TO) die('toast group missing');
if (!(TO.position?.options ?? []).includes(TO.position?.value)) die('toast.position: value must be one of its options');
if (!Number.isInteger(TO.max?.value) || TO.max.value < 1) die('toast.max: a whole number >= 1');
for (const k of ['life', 'life-urgent']) if (!motion.durations?.[TO[k]?.motion]) die(`toast.${k}: motion.json has no duration ${TO[k]?.motion}`);
if (!sound.types.some((x) => x.id === TO.sound?.sound)) die(`toast.sound: sound.json has no type ${TO.sound?.sound}`);
const TD = ['swipe', 'swipe-out', 'tap-slop'];
for (const k of TD)
  for (const [d, x] of [['720 (v)', TO[k]?.v], ['480 (1x)', TO[k]?.['1x']]])
    if (!Number.isInteger(x) || x < 4 || x % 4) bad.push(`toast.${k} ${d} = ${x}: not a multiple of 4`);
if (bad.length) die(`layout rule:\n  ${bad.join('\n  ')}`);

// densities, size classes, devices: each device's class = the largest class whose min fits its dp size
const D = Object.entries(t.density).filter(([k]) => !k.startsWith('$'));
const C = Object.entries(t.sizeClass).filter(([k]) => !k.startsWith('$'));
const DV = Object.entries(t.devices).filter(([k]) => !k.startsWith('$'));
for (const [id, d] of D) if (!(d.d > 0) || !Number.isInteger(d.grid) || d.grid % 4) die(`density ${id}: d > 0 and grid a multiple of 4`);
const fits = (c, w, h) => w >= c.min[0] && h >= c.min[1];
for (const [id, v] of DV) {
  const den = t.density[v.density];
  if (!den) die(`device ${id}: unknown density ${v.density}`);
  const w = v.w / den.d, h = v.h / den.d;
  if (!Number.isInteger(w) || !Number.isInteger(h)) die(`device ${id}: ${v.w} x ${v.h} / ${den.d} is not whole dp`);
  const cls = C.filter(([, c]) => fits(c, w, h)).sort((a, b) => b[1].min[0] * b[1].min[1] - a[1].min[0] * a[1].min[1])[0];
  if (!cls) die(`device ${id}: ${w} x ${h} dp fits no size class`);
  if (cls[0] !== v.class) die(`device ${id}: ${w} x ${h} dp is class ${cls[0]}, not ${v.class}`);
}

const L = ['// GENERATED by tools/gen_layout.mjs from tokens/layout.json: edit the JSON, not this file.', '#pragma once', '', 'namespace fw {'];
const block = (d) => {
  const B = ['namespace lay {'];
  for (const r of rows) B.push(`constexpr int ${field(r.n)} = ${d === '1x' ? r['1x'] : r.v};`);
  for (const [k, x] of Object.entries(derived[d])) B.push(`constexpr int ${k} = ${x};  // derived`);
  B.push('}  // namespace lay', 'namespace toast {  // toast behaviour (layout.json toast)');
  for (const k of TD) B.push(`constexpr int ${field(k)} = ${d === '1x' ? TO[k]['1x'] : TO[k].v};  // ${TO[k].use}`);
  B.push('}  // namespace toast');
  return B;
  return B;
};
L.push('#ifdef FW_1X', ...block('1x'), '#else', ...block('1.5x'), '#endif');
L.push('namespace toast {',
  `constexpr bool top = ${TO.position.value === 'top'};  // ${TO.position.use}`,
  `constexpr int max = ${TO.max.value};  // ${TO.max.use}`,
  `constexpr uint32_t life = ${motion.durations[TO.life.motion].ms};  // motion ${TO.life.motion}: ${TO.life.use}`,
  `constexpr uint32_t life_urgent = ${motion.durations[TO['life-urgent'].motion].ms};  // motion ${TO['life-urgent'].motion}`,
  `constexpr const char *sound = ${cstr(TO.sound.sound)};  // ${TO.sound.use}`,
  '}  // namespace toast');
L.push('namespace lyt {  // the Layout page tables: both densities, device px');
L.push('struct Group { const char *id, *label, *about; };', 'constexpr Group GROUPS[] = {');
for (const g of t.groups) L.push(`  {${cstr(g.id)}, ${cstr(g.label)}, ${cstr(g.about)}},`);
L.push('};', `constexpr int N_GROUPS = ${t.groups.length};`);
L.push('struct Token { const char *group, *name, *use; int v, v1x; };', 'constexpr Token TOKENS[] = {');
for (const r of rows) L.push(`  {${cstr(r.g)}, ${cstr(r.n)}, ${cstr(r.use)}, ${r.v}, ${r['1x']}},`);
L.push('};', `constexpr int N_TOKENS = ${rows.length};`);
L.push('struct Derived { const char *name, *how; int v, v1x; };', 'constexpr Derived DERIVED[] = {');
const how = { screen_w: 'the screen', screen_h: 'the screen', content_w: 'screen - 2 x inset', content_h: 'screen - top - inset', header_gap: 'top - inset - row' };
for (const k of Object.keys(derived['1.5x'])) L.push(`  {${cstr(k)}, ${cstr(how[k])}, ${derived['1.5x'][k]}, ${derived['1x'][k]}},`);
L.push('};', `constexpr int N_DERIVED = ${Object.keys(derived['1.5x']).length};`);
L.push('struct Density { const char *id; float d; int grid; const char *role, *optical; };', 'constexpr Density DENSITIES[] = {');
for (const [id, d] of D) L.push(`  {${cstr(id)}, ${flt(d.d)}, ${d.grid}, ${cstr(d.role)}, ${cstr(d.optical)}},`);
L.push('};', `constexpr int N_DENSITIES = ${D.length};`);
L.push('struct SizeClass { const char *id; int min_w, min_h, home_cols; const char *use; };', 'constexpr SizeClass CLASSES[] = {');
for (const [id, c] of C) L.push(`  {${cstr(id)}, ${c.min[0]}, ${c.min[1]}, ${c.homeCols}, ${cstr(c.use)}},`);
L.push('};', `constexpr int N_CLASSES = ${C.length};`);
L.push('struct Device { const char *id, *name; int w, h; float inches; const char *density; float d; const char *cls, *status, *role, *runs, *mount; };', 'constexpr Device DEVICES[] = {');
for (const [id, v] of DV) L.push(`  {${cstr(id)}, ${cstr(v.name)}, ${v.w}, ${v.h}, ${flt(v.inches)}, ${cstr(v.density)}, ${flt(t.density[v.density].d)}, ${cstr(v.class)}, ${cstr(v.status)}, ${cstr(v.role)}, ${cstr(v.runs)}, ${cstr(v.mount)}},`);
L.push('};', `constexpr int N_DEVICES = ${DV.length};`);
L.push(`constexpr const char *SCREEN_ABOUT = ${cstr(t.screen.about)};`);
// rules (layout.json `rules`, Darren 2026-10-22): plain-words sections (heading + one or two sentences) that are the
// page's section labels + abouts below (the docs site reads the same keys); `components` is checked, not emitted
const R = t.rules ?? die('rules: missing');
const RS = Object.fromEntries((R.sections ?? []).map((s) => [s.id, s]));
for (const s of R.sections ?? []) if (!s.id || !s.title || !s.text) die(`rules.sections ${s.id}: needs id, title, text`);
for (const c of R.components ?? []) {
  if (!existsSync(join(HERE, '..', 'components', `${c.id}.cpp`))) die(`rules.components ${c.id}: no components/${c.id}.cpp`);
  if (!c.size) die(`rules.components ${c.id}: no size line`);
}
L.push('}  // namespace lyt');
const sec = (id, label, about) => ({ label, about, props: { section: id } });
const rule = (id) => { const s = RS[id] ?? die(`rules.sections: no ${id}`); return sec(id, s.title, s.text); };
const doc = [
  sec('screen', 'The screen, layer by layer', 'A real Home screen at true size with the layout drawn over it: the grid, the margin around the edge, the header, the content area and the gaps between cards. Tap a chip under the screen to hide or show a layer.'),
  rule('grid'), rule('sizes'), rule('spacing'), rule('rhythm'), rule('frame'), rule('touch'), rule('corners'), rule('breaks'),
  sec('overlay', 'Sheets and toasts', 'Some things slide in over the screen. A sheet comes up from below and keeps the same margin inside; a toast is as wide as the content and appears just under the header.'),
  sec('devices', 'Devices', "The panels Facet is built for today, and the sizes planned next, each drawn to scale."),
  sec('classes', 'Screen shapes', 'Square, tall, wide and large screens each get their own layout; the same content is arranged to fit. Each shape is sketched with its Home columns.'),
];
L.push(`#define FW_LAYOUT_SECTIONS ${cstr(JSON.stringify(doc))}`);
L.push(`#define FW_LAYOUT_SECTION_IDS ${cstr(JSON.stringify(doc.map((d) => d.props.section)))}`);
L.push('}  // namespace fw', '');
const text = L.join('\n');
if (existsSync(out) && readFileSync(out, 'utf8') === text) process.exit(0);
writeFileSync(`${out}.${process.pid}`, text);
renameSync(`${out}.${process.pid}`, out);
console.log(`gen layout_gen.h (${rows.length} tokens, ${DV.length} devices)`);
