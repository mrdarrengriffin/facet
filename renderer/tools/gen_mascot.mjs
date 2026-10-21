#!/usr/bin/env node
// tokens/mascot.json -> C++ header (README "Mascot tokens"): fw::mas:: the mascot's geometry for both densities (GEO_V /
// GEO_1X, `G` = this build's), the eye shapes, brows, extras ("!", z, progress dots), the moods (colour token as a
// Theme offset, eyes, brows, pose, extras, motion patterns), the motion patterns as keyframe tracks, the instance
// delay, and the tables + sections of the Mascots foundation page. Runtime: core/mascot.cpp (fw::mascot).
// mascot.json names tokens only: colours are checked against colour.json, easings + reduced modes against motion.json,
// the build fails on a bad reference, an off-grid body box or sub-pixel artwork.
// Run by build.sh / firmware/bundle.mjs: `node tools/gen_mascot.mjs <out.h>`; writes only when the output changes.
import { readFileSync, writeFileSync, existsSync, renameSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
const read = (f) => JSON.parse(readFileSync(join(HERE, '..', 'tokens', f), 'utf8'));
const t = read('mascot.json'), colour = read('colour.json'), motion = read('motion.json'), space = read('space.json');
const out = process.argv[2];
if (!out) { console.error('usage: gen_mascot.mjs <out.h>'); process.exit(2); }
const die = (m) => { console.error(`tokens/mascot.json: ${m}`); process.exit(1); };
const cstr = (s) => JSON.stringify(String(s ?? ''));
const field = (n) => n.replace(/-/g, '_');
const D = ['v', '1x'];
const int = (where, v, min = -Infinity) => { if (!Number.isInteger(v) || v < min) die(`${where} = ${v}: whole px${min > -Infinity ? ` >= ${min}` : ''} (no sub-pixel artwork)`); return v; };
// token references ("stroke.edge", "radius.round") in any per-density value -> that density's space.json value
// (radius.round = 999, the sentinel LVGL clamps to fully round); refs[] keeps them for the page
const refs = [];
const resolve = (o, d, where) => {
  if (typeof o === 'string') {
    const [g, k] = o.split('.');
    if (!['stroke', 'radius'].includes(g) || space.tokens?.[g]?.[k]?.[d] === undefined) die(`${where}: "${o}" is not a space.json stroke / radius token`);
    refs.push(`${where}=${o}`);
    return space.tokens[g][k][d];
  }
  if (o && typeof o === 'object' && !Array.isArray(o)) for (const [k, v] of Object.entries(o)) o[k] = resolve(v, d, `${where}.${k}`);
  return o;
};
const walk = (o, where) => {
  if (!o || typeof o !== 'object' || Array.isArray(o)) return;
  if ('v' in o && '1x' in o) { for (const d of D) o[d] = resolve(o[d], d, `${where}.${d}`); return; }
  for (const [k, v] of Object.entries(o)) walk(v, where ? `${where}.${k}` : k);
};
walk(t, '');
// the 4 px grid where the mascot follows it (grid.checked: body box + everything outside the body)
const at = (path) => path.split('.').reduce((o, k) => o?.[k], t);
for (const path of t.grid.checked) {
  const vals = path.startsWith('motion.') ? at(path.replace(/\.amp$/, '')).tracks.filter((x) => x.amp).flatMap((x) => D.map((d) => x.amp[d]))
    : path.split('.').length === 2 ? D.map((d) => at(path)[d]) : D.map((d) => at(path.replace(/\.(\w+)$/, `.${d}.$1`)));
  for (const v of vals) if (!Number.isInteger(v) || v % 4) die(`${path} = ${v}: not on the 4 px grid (grid.checked)`);
}
const colourRef = (where, n) => { if (!colour.colours[n]) die(`${where}: unknown colour token "${n}" (colour.json colours)`); return n; };

// body: box on the 4 px grid; its radius is artwork (docs 7 / firmware 5), whole px
const body = {};
for (const d of D) {
  body[d] = { w: int(`body.w.${d}`, t.body.w[d], 4), h: int(`body.h.${d}`, t.body.h[d], 4), r: int(`body.r.${d}`, t.body.r[d], 0) };
  if (body[d].w % 4 || body[d].h % 4) die(`body ${d} ${body[d].w} x ${body[d].h}: not on the 4 px grid`);
}
colourRef('eye-colour', t['eye-colour'].token);
const blink = Object.fromEntries(D.map((d) => [d, int(`blink-h.${d}`, t['blink-h'][d], 1)]));

const eyes = Object.entries(t.eyes).filter(([k]) => !k.startsWith('$')).map(([id, e]) => {
  if (!/^[a-z][a-z0-9-]*$/.test(id)) die(`eyes.${id}: bad name`);
  for (const d of D) {
    const s = e[d];
    for (const k of ['x', 'y', 'w', 'h', 'r']) int(`eyes.${id}.${d}.${k}`, s[k], 0);
    int(`eyes.${id}.${d}.arch`, s.arch ?? 0, 0);
    s.rx = int(`eyes.${id}.${d}.rx`, s.rx ?? body[d].w - s.x - s.w, 0);
    if (s.rx < body[d].w / 2 || s.rx + s.w > body[d].w) die(`eyes.${id}.${d}.rx: the right eye must sit in the right half`);
    if (s.w < 1 || s.h < 1) die(`eyes.${id}.${d}: empty`);
    if (s.x + s.w > body[d].w / 2) die(`eyes.${id}.${d}: the left eye crosses the body centre`);
    if (s.y + s.h > body[d].h) die(`eyes.${id}.${d}: below the body`);
  }
  return { id, ...e };
});
const eyeIds = eyes.map((e) => e.id);
for (const d of D) { const b = t.brow[d]; for (const k of ['x', 'w', 'h', 'r']) int(`brow.${d}.${k}`, b[k], 0); }

const X = t.extras;
colourRef('extras.bang.colour', X.bang.colour);
colourRef('extras.z.colour', X.z.colour);
if (X.dots.colour !== 'mood') colourRef('extras.dots.colour', X.dots.colour);
for (const d of D) {
  for (const k of ['x', 'y', 'w', 'stem', 'gap', 'dot', 'r']) int(`extras.bang.${d}.${k}`, X.bang[d][k]);
  for (const k of ['x', 'y', 'size', 'stroke']) int(`extras.z.${d}.${k}`, X.z[d][k]);
  if (X.z[d].stroke < 1 || X.z[d].stroke > 2) die(`extras.z.${d}.stroke: 1 or 2 px`);
  for (const k of ['x', 'y', 'size', 'pitch', 'r']) int(`extras.dots.${d}.${k}`, X.dots[d][k]);
}
const opacity = Object.fromEntries(Object.entries(t.opacity).map(([n, o]) => {
  if (typeof o.value !== 'number' || o.value < 0 || o.value > 1) die(`opacity.${n}: 0..1`);
  return [n, o.value];
}));

// motion patterns: keyframe tracks
const EASE = Object.keys(motion.easings), MODES = Object.keys(motion.reduced.modes);
const PARTS = { body: 'PART_BODY', face: 'PART_FACE', eyes: 'PART_EYES', bang: 'PART_BANG', z: 'PART_Z', dot: 'PART_DOT' };
const PROPS = { x: 'PROP_X', y: 'PROP_Y', h: 'PROP_H', opa: 'PROP_OPA', rot: 'PROP_ROT', close: 'PROP_CLOSE', scale: 'PROP_SCALE' };
const PX = new Set(['x', 'y', 'h']);
const pats = Object.entries(t.motion).filter(([k]) => !k.startsWith('$')).map(([id, p]) => {
  const w = `motion.${id}`;
  if (!/^[a-z][a-z0-9-]*$/.test(id)) die(`${w}: bad name`);
  int(`${w}.ms`, p.ms, 1);
  if (!EASE.includes(p.easing)) die(`${w}.easing: unknown easing "${p.easing}" (motion.json)`);
  if (!MODES.includes(p.reduced)) die(`${w}.reduced: one of ${MODES.join(', ')}`);
  if (p.reduced === 'still' && !p.loop) die(`${w}: reduced "still" is for loops`);
  int(`${w}.rest`, p.rest ?? 0, 0);
  const tracks = p.tracks.map((tr, i) => {
    const tw = `${w}.tracks[${i}]`;
    if (!PARTS[tr.part]) die(`${tw}.part: one of ${Object.keys(PARTS).join(', ')}`);
    if (!PROPS[tr.prop]) die(`${tw}.prop: one of ${Object.keys(PROPS).join(', ')}`);
    if (PX.has(tr.prop) !== !!tr.amp) die(`${tw}: x / y / h tracks need amp {v, 1x} (art px), others none`);
    const amp = tr.amp ? { v: int(`${tw}.amp.v`, tr.amp.v, 1), '1x': int(`${tw}.amp.1x`, tr.amp['1x'], 1) } : { v: 1, '1x': 1 };
    let last = -1;
    const keys = tr.keys.map(([k, v], j) => {
      if (!Number.isInteger(k) || k <= last || k > 1000) die(`${tw}.keys[${j}]: t must rise, 0..1000 permille`);
      last = k;
      if (typeof v === 'string') { if (opacity[v] === undefined) die(`${tw}.keys[${j}]: unknown opacity "${v}"`); v = opacity[v]; }
      if (typeof v !== 'number') die(`${tw}.keys[${j}]: value must be a number or an opacity name`);
      return [k, v];
    });
    if (keys[0][0] !== 0 || keys[keys.length - 1][0] !== 1000) die(`${tw}: keys must start at 0 and end at 1000`);
    return { ...tr, amp, keys };
  });
  return { id, ...p, tracks, stagger: p.stagger ?? 0, rest: p.rest ?? 0 };
});
const patIds = pats.map((p) => p.id);

const EXTRAS = { bang: 'X_BANG', z: 'X_Z', dots: 'X_DOTS' };
const moods = Object.entries(t.moods).map(([id, m]) => {
  const w = `moods.${id}`;
  colourRef(`${w}.colour`, m.colour);
  if (!eyeIds.includes(m.eyes)) die(`${w}.eyes: one of ${eyeIds.join(', ')}`);
  for (const p of m.motion ?? []) if (!patIds.includes(p)) die(`${w}.motion: unknown pattern "${p}"`);
  for (const x of m.extras ?? []) if (!EXTRAS[x]) die(`${w}.extras: one of ${Object.keys(EXTRAS).join(', ')}`);
  if (m.brows) for (const d of D) int(`${w}.brows.${d}.y`, m.brows[d].y, 0);
  return { id, ...m };
});
const delay = int('delay.instance-offset.ms', t.delay['instance-offset'].ms, 1);
for (const p of pats) if (p.desync && p.loop && p.ms % delay === 0) die(`motion.${p.id}: ${p.ms} ms is a multiple of instance-offset (blinks would line up)`);
const work = t.work;
if (!work.variants[work.default]) die('work.default: not a variant');

// ---------------------------------------------------------------- header
const L = ['// GENERATED by tools/gen_mascot.mjs from tokens/mascot.json: edit the JSON, not this file.', '#pragma once', '#include <cstddef>', '#include <cstdint>', '', 'namespace fw::mas {'];
L.push(`enum Mood { ${moods.map((m) => field(m.id)).join(', ')}, N_MOODS };`);
L.push(`enum Eye { ${eyes.map((e) => `E_${field(e.id)}`).join(', ')}, N_EYES };`);
L.push(`enum Pat { ${pats.map((p) => `P_${field(p.id)}`).join(', ')}, N_PATS };`);
L.push('enum Part : uint8_t { PART_BODY, PART_FACE, PART_EYES, PART_BANG, PART_Z, PART_DOT };  // face = the eye + brow group');
L.push('enum Prop : uint8_t { PROP_X, PROP_Y, PROP_H, PROP_OPA, PROP_ROT, PROP_CLOSE, PROP_SCALE };  // scale: z line points only');
L.push('enum Extra : uint8_t { X_BANG = 1, X_Z = 2, X_DOTS = 4 };');
L.push('struct Shape { int16_t x, y, w, h, r, arch, rx; };  // left eye at x, right eye at rx; arch > 0 = lv_arc half ring of that width in a w x w box');
L.push('struct Brow { int16_t x, w, h, r; };');
L.push('struct Bang { int16_t x, y, w, stem, gap, dot, r; };');
L.push('struct Zs { int16_t x, y, size, stroke; };');
L.push('struct Dots { int16_t x, y, size, pitch, r; };');
L.push('struct Geo { int16_t body_w, body_h, body_r, blink_h; Shape eyes[N_EYES]; Brow brow; Bang bang; Zs z; Dots dots; int16_t brow_y[N_MOODS]; };');
const sh = (s) => `{${s.x}, ${s.y}, ${s.w}, ${s.h}, ${s.r}, ${s.arch ?? 0}, ${s.rx}}`;
const geo = (d) => {
  const b = t.brow[d], bg = X.bang[d], z = X.z[d], dt = X.dots[d];
  return `{${body[d].w}, ${body[d].h}, ${body[d].r}, ${blink[d]}, {${eyes.map((e) => sh(e[d])).join(', ')}}, {${b.x}, ${b.w}, ${b.h}, ${b.r}}, {${bg.x}, ${bg.y}, ${bg.w}, ${bg.stem}, ${bg.gap}, ${bg.dot}, ${bg.r}}, {${z.x}, ${z.y}, ${z.size}, ${z.stroke}}, {${dt.x}, ${dt.y}, ${dt.size}, ${dt.pitch}, ${dt.r}}, {${moods.map((m) => (m.brows ? m.brows[d].y : 0)).join(', ')}}}`;
};
L.push(`constexpr Geo GEO_V = ${geo('v')};  // 720 px panels`, `constexpr Geo GEO_1X = ${geo('1x')};  // 480 px panels`);
L.push('#ifdef FW_1X', 'inline constexpr const Geo &G = GEO_1X;', '#else', 'inline constexpr const Geo &G = GEO_V;', '#endif');
L.push(`constexpr int Z_COUNT = ${X.z.count}, DOT_COUNT = ${X.dots.count};`);
L.push(`constexpr float DOT_DIM = ${opacity['dot-dim']}f;  // ${t.opacity['dot-dim'].about}`);
L.push(`constexpr uint32_t INSTANCE_OFFSET_MS = ${delay};  // ${t.delay['instance-offset'].about}`);
L.push(`constexpr const char *EYE_COLOUR = ${cstr(t['eye-colour'].token)}, *BANG_COLOUR = ${cstr(X.bang.colour)}, *Z_COLOUR = ${cstr(X.z.colour)};`);
L.push(`constexpr size_t EYE_OFF = offsetof(Theme, ${field(t['eye-colour'].token)}), BANG_OFF = offsetof(Theme, ${field(X.bang.colour)}), Z_OFF = offsetof(Theme, ${field(X.z.colour)});`);
L.push('struct EyeDef { const char *id, *label, *use; };', 'constexpr EyeDef EYE_DEFS[N_EYES] = {');
for (const e of eyes) L.push(`  {${cstr(e.id)}, ${cstr(e.label)}, ${cstr(e.use)}},`);
L.push('};');

L.push('struct Key { uint16_t t; float v; };');
L.push('struct Track { Part part; Prop prop; int16_t amp_v, amp_1x; const Key *keys; uint8_t n; };');
for (const p of pats) p.tracks.forEach((tr, i) => L.push(`constexpr Key K_${field(p.id)}_${i}[] = {${tr.keys.map(([k, v]) => `{${k}, ${Number.isInteger(v) ? `${v}.0` : v}f}`).join(', ')}};`));
for (const p of pats) L.push(`constexpr Track T_${field(p.id)}[] = {${p.tracks.map((tr, i) => `{${PARTS[tr.part]}, ${PROPS[tr.prop]}, ${tr.amp.v}, ${tr.amp['1x']}, K_${field(p.id)}_${i}, ${tr.keys.length}}`).join(', ')}};`);
L.push('#ifdef FW_1X', 'inline int amp(const Track &t) { return t.amp_1x; }', '#else', 'inline int amp(const Track &t) { return t.amp_v; }', '#endif');
L.push('struct Pattern { const char *id, *label, *use, *animates, *easing_name, *reduced_name; uint32_t ms; mo::Easing easing; bool loop, desync; uint32_t stagger; mo::Reduced reduced; uint16_t rest; const Track *tracks; uint8_t n_tracks; };');
L.push('constexpr Pattern PATTERNS[N_PATS] = {');
for (const p of pats) L.push(`  {${cstr(p.id)}, ${cstr(p.label)}, ${cstr(p.use)}, ${cstr(p.animates)}, ${cstr(p.easing)}, ${cstr(p.reduced)}, ${p.ms}, mo::E_${field(p.easing)}, ${!!p.loop}, ${!!p.desync}, ${p.stagger}, mo::R_${p.reduced}, ${p.rest}, T_${field(p.id)}, ${p.tracks.length}},`);
L.push('};');

L.push('struct MoodDef { const char *id, *label, *colour, *what; size_t colour_off; Eye eye; bool brows; int16_t brow_angle; int16_t pose_y, pose_rot; uint8_t extras; uint32_t pats; };  // pats = bit per Pat');
L.push('constexpr MoodDef MOODS[N_MOODS] = {');
for (const m of moods) {
  const ex = (m.extras ?? []).map((x) => EXTRAS[x]).join(' | ') || '0';
  const pm = (m.motion ?? []).map((p) => `(1u << P_${field(p)})`).join(' | ') || '0u';
  L.push(`  {${cstr(m.id)}, ${cstr(m.label)}, ${cstr(m.colour)}, ${cstr(m.what)}, offsetof(Theme, ${field(m.colour)}), E_${field(m.eyes)}, ${!!m.brows}, ${m.brows?.angle ?? 0}, ${m.pose?.y ?? 0}, ${m.pose?.rot ?? 0}, ${ex}, ${pm}},`);
}
L.push('};');
L.push('struct Work { const char *id, *label, *status, *what; };', 'constexpr Work WORK[] = {');
for (const [id, v] of Object.entries(work.variants)) L.push(`  {${cstr(id)}, ${cstr(v.label)}, ${cstr(v.status)}, ${cstr(v.what)}},`);
L.push('};', `constexpr int N_WORK = ${Object.keys(work.variants).length};`, `constexpr const char *WORK_DEFAULT = ${cstr(work.default)};`);
L.push(`constexpr const char *GRID_RULE = ${cstr(t.grid.rule)};`);
L.push(`constexpr const char *FOUNDATIONS[] = {${t.grid.foundations.map(cstr).join(', ')}};`, `constexpr int N_FOUNDATIONS = ${t.grid.foundations.length};`);
L.push(`constexpr const char *CHANGES[] = {${t.grid.changes.map(cstr).join(', ')}};`, `constexpr int N_CHANGES = ${t.grid.changes.length};`);
L.push(`constexpr const char *WHERE[] = {${t.usage.where.map(cstr).join(', ')}};`, `constexpr int N_WHERE = ${t.usage.where.length};`);
L.push(`constexpr const char *RULES[] = {${t.usage.rules.map(cstr).join(', ')}};`, `constexpr int N_RULES = ${t.usage.rules.length};`);

const doc = [
  { label: 'Moods', about: `${moods.length} moods, each at true size and 4x (the same code, every part x 4), on the canvas and on a tile surface, all of their motion live. Tap a mood to replay its arrival (the hop for done).`, props: { section: 'moods' } },
  { label: 'Motion', about: `${pats.length} motion patterns, each alone on the mood that uses it, with its timing, easing and reduced-motion rule. Tap a sample to replay it from the start.`, props: { section: 'motion' } },
  { label: 'Reduced motion', about: 'The moods with reduced motion on (the reduced_motion setting): every loop stops on its rest frame, so the pose still shows the mood (the z\'s stay up, one dot lit), and the hop is skipped.', props: { section: 'moods', reduced_motion: true } },
  { label: 'Desync', about: 'Six idle mascots side by side, each with the instance delay (n x instance-offset): their blinks and glances never line up. Tap to restart them together.', props: { section: 'desync' } },
  { label: 'Tokens', about: 'How the mascot follows the foundations, then every size, colour and timing token at both densities (mascot.json), and what changed from the old mascot.', props: { section: 'tokens' } },
  { label: 'Usage', about: 'Where mascots appear and the rules, with an Agents tile and an empty state as examples (their components are not built yet: plain outlines).', props: { section: 'usage' } },
];
L.push(`#define FW_MASCOT_SECTIONS ${cstr(JSON.stringify(doc))}`);
L.push(`#define FW_MASCOT_SECTION_IDS ${cstr(JSON.stringify([...new Set(doc.map((d) => d.props.section))]))}`);
L.push(`#define FW_MASCOT_MOOD_IDS ${cstr(JSON.stringify(moods.map((m) => m.id)))}`);
L.push(`#define FW_MASCOT_COLOURS ${cstr(JSON.stringify(['mood', ...[...new Set(moods.map((m) => m.colour))]]))}`);
L.push('}  // namespace fw::mas', '');
const text = L.join('\n');
if (existsSync(out) && readFileSync(out, 'utf8') === text) process.exit(0);
writeFileSync(`${out}.${process.pid}`, text);
renameSync(`${out}.${process.pid}`, out);
console.log(`gen mascot_gen.h (${moods.length} moods, ${eyes.length} eye shapes, ${pats.length} patterns)`);
