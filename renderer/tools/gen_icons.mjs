#!/usr/bin/env node
// tokens/icons.json + the masters in icons/ -> two C++ headers (README "Icons"):
//   node tools/gen_icons.mjs --meta <icons_gen.h>             density-free: fw::ic::Size + px per density, the icon
//                                                             table (name, kind, group, keyline, sizes, use), page
//                                                             sections; included by fw.h
//   node tools/gen_icons.mjs --data <1x|1.5x> <facet_icons.h> this density's pixels: every icon x every size it ships
//                                                             in, rasterised at that exact pixel size (never scaled on
//                                                             the device), one A8 alpha mask per colour layer, cropped
//                                                             to its ink; included by core/icons.cpp only
// Rasteriser: resvg (tools/package.json: `cd renderer/tools && npm install`). Strokes follow Facet's optical sizes:
// UI + screen icons opticalStroke(px) (1.5 / 2 / 2.5 / 3 px at 16 / 24 / 32 / 48, linear, clamped), weather 2 px
// (3 px from 96 px). Layers: ui = one mask (ink = the caller's colour); screen = ink + accent; weather = ink (the
// master's text colour) + text-dim / accent / blue / steel, the cloud's bg fill baked in as an occluder (each layer is
// rendered white with every other colour black, so what is hidden behind a cloud is not in the mask).
// FW_ICONS=used (env or local.env): only the icon@size pairs components declare in their meta ("icons": ["plus@icon"])
// = the small build for a panel that does not carry the Icons page's full set. Writes only when the output changes.
import { createHash } from 'node:crypto';
import { existsSync, readdirSync, readFileSync, renameSync, writeFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
const R = join(HERE, '..');
const t = JSON.parse(readFileSync(join(R, 'tokens', 'icons.json'), 'utf8'));
const die = (m) => { console.error(`tokens/icons.json: ${m}`); process.exit(1); };
const cstr = (s) => JSON.stringify(String(s ?? ''));
const cid = (s) => s.replace(/-/g, '_');
const write = (out, text) => {
  if (existsSync(out) && readFileSync(out, 'utf8') === text) return;
  writeFileSync(`${out}.tmp`, text);
  renameSync(`${out}.tmp`, out);
};

// ---- checks (both modes)
const SIZES = Object.keys(t.sizes);
for (const [k, s] of Object.entries(t.sizes)) {
  if (!/^[a-z][a-z0-9]*$/.test(k)) die(`size ${k}: bad name (a C++ identifier)`);
  for (const d of ['v', '1x']) if (!Number.isInteger(s[d]) || s[d] % 4) die(`size ${k} ${d} = ${s[d]}: not a multiple of 4 (4 px grid)`);
}
const GROUPS = t.groups.map((g) => g.id);
const KINDS = ['ui', 'screen', 'weather'];
const names = new Set();
for (const i of t.icons) {
  if (!/^[a-z0-9][a-z0-9-]*$/.test(i.name)) die(`${i.name}: bad name (kebab-case)`);
  if (names.has(i.name)) die(`${i.name}: twice`);
  names.add(i.name);
  if (!KINDS.includes(i.kind)) die(`${i.name}: kind ${i.kind}`);
  if (!GROUPS.includes(i.group)) die(`${i.name}: group ${i.group} not in groups`);
  if (!existsSync(join(R, 'icons', i.file))) die(`${i.name}: no master icons/${i.file}`);
  if (!i.sizes.length) die(`${i.name}: no sizes`);
  for (const s of i.sizes) if (!SIZES.includes(s)) die(`${i.name}: size ${s} not in sizes`);
  if (i.turn && i.kind !== 'ui') die(`${i.name}: turn needs kind ui`);
}
// turn icons: the master's one filled path as an outline on its 24 grid (M/L/H/V/C/Z, absolute or relative; a cubic
// = 8 segments), so the device fills it at any heading (fw::icon_turned) instead of resampling a turned mask
const outline = (name, src) => {
  const ds = [...src.matchAll(/<path[^>]*\sd="([^"]+)"/g)].map((m) => m[1]);
  if (ds.length !== 1 || !/fill="currentColor"/.test(src) || /stroke="currentColor"/.test(src)) die(`${name}: turn needs one filled path`);
  const tk = ds[0].match(/[a-zA-Z]|-?(?:\d*\.\d+|\d+\.?\d*)(?:e[-+]?\d+)?/g);
  const pts = [];
  let k = 0, cmd = '', x = 0, y = 0, closed = false;
  const num = () => parseFloat(tk[k++]);
  while (k < tk.length) {
    if (/[a-zA-Z]/.test(tk[k])) cmd = tk[k++];
    const rel = cmd === cmd.toLowerCase(), C = cmd.toUpperCase();
    if (closed && C !== 'Z') die(`${name}: turn needs one subpath`);
    if (C === 'Z') { closed = true; continue; }
    if (C === 'M' || C === 'L') { const nx = num(), ny = num(); x = rel ? x + nx : nx; y = rel ? y + ny : ny; if (C === 'M' && pts.length) die(`${name}: turn needs one subpath`); pts.push([x, y]); if (C === 'M') cmd = rel ? 'l' : 'L'; }
    else if (C === 'H') { const n = num(); x = rel ? x + n : n; pts.push([x, y]); }
    else if (C === 'V') { const n = num(); y = rel ? y + n : n; pts.push([x, y]); }
    else if (C === 'C') {
      const c = [num(), num(), num(), num(), num(), num()].map((v, j) => (rel ? v + (j % 2 ? y : x) : v));
      for (let j = 1; j <= 8; j++) {
        const t = j / 8, u = 1 - t, a = u * u * u, b = 3 * u * u * t, cc = 3 * u * t * t, d = t * t * t;
        pts.push([a * x + b * c[0] + cc * c[2] + d * c[4], a * y + b * c[1] + cc * c[3] + d * c[5]]);
      }
      x = c[4], y = c[5];
    } else die(`${name}: path command ${cmd} not supported for turn`);
  }
  const [f, l] = [pts[0], pts[pts.length - 1]];
  if (Math.hypot(f[0] - l[0], f[1] - l[1]) < 1e-6) pts.pop();
  return pts;
};
for (const n of t.compare.icons) if (!names.has(n)) die(`compare: ${n} is not an icon`);
const ROLES = ['ink', 'accent', 'text-dim', 'blue', 'steel', 'occluder'];  // ink = the caller's colour; occluder = an
// animated weather part's bg fill, drawn in the colour behind the icon (icons.json animated)

// ---- meta header
if (process.argv[2] === '--meta') {
  const out = process.argv[3];
  if (!out) { console.error('usage: gen_icons.mjs --meta <out.h>'); process.exit(2); }
  const L = ['// GENERATED by tools/gen_icons.mjs from tokens/icons.json: edit the JSON, not this file.', '#pragma once', '#include <cstdint>', '', 'namespace fw {', 'namespace ic {'];
  L.push(`enum Size : uint8_t { ${SIZES.join(', ')}, N_SIZES };`);
  L.push(`constexpr int PX_V[N_SIZES] = {${SIZES.map((s) => t.sizes[s].v).join(', ')}};`);
  L.push(`constexpr int PX_1X[N_SIZES] = {${SIZES.map((s) => t.sizes[s]['1x']).join(', ')}};`);
  L.push('#ifdef FW_1X', 'constexpr const int *PX = PX_1X;', '#else', 'constexpr const int *PX = PX_V;', '#endif');
  L.push(`constexpr const char *SIZE_NAMES[N_SIZES] = {${SIZES.map(cstr).join(', ')}};`);
  L.push(`constexpr const char *SIZE_USE[N_SIZES] = {${SIZES.map((s) => cstr(t.sizes[s].use)).join(', ')}};`);
  L.push(`enum Role : uint8_t { ${ROLES.map((r) => `R_${cid(r)}`).join(', ')} };`);
  L.push('struct Group { const char *id, *label; };', `constexpr Group GROUPS[] = {${t.groups.map((g) => `{${cstr(g.id)}, ${cstr(g.label)}}`).join(', ')}};`);
  L.push(`constexpr int N_GROUPS = ${t.groups.length};`);
  L.push('// one per master; sizes = bit mask of Size (what the docs ship it in; a build may carry fewer, see fw::icon_has)');
  L.push('struct Info { const char *name, *kind, *group, *keyline, *use; uint16_t sizes; };', 'constexpr Info ICONS[] = {');
  for (const i of t.icons) L.push(`  {${cstr(i.name)}, ${cstr(i.kind)}, ${cstr(i.group)}, ${cstr(i.keyline)}, ${cstr(i.use)}, ${i.sizes.reduce((m, s) => m | (1 << SIZES.indexOf(s)), 0)}},`);
  L.push('};', `constexpr int N = ${t.icons.length};`);
  L.push(`constexpr const char *COMPARE[] = {${t.compare.icons.map(cstr).join(', ')}};`, `constexpr int N_COMPARE = ${t.compare.icons.length};`);
  L.push(`constexpr Size COMPARE_SIZES[] = {${t.compare.sizes.join(', ')}};`, `constexpr int N_COMPARE_SIZES = ${t.compare.sizes.length};`);
  L.push(`constexpr float STROKE_TABLE[][2] = {${t.stroke.ui.table.map(([a, b]) => `{${a}, ${b}}`).join(', ')}};`);
  L.push(`constexpr float WX_STROKE = ${t.stroke.weather.px}, WX_STROKE_96 = ${t.stroke.weather.from96};  // weather + moon phase stroke px (from96 at 96 px and up)`);
  L.push(`constexpr uint16_t ANIM_SIZES = ${(t.animated?.sizes ?? []).reduce((m, s) => m | (1 << SIZES.indexOf(s)), 0)};  // sizes the animated weather parts ship in (icons.json animated)`);
  const turns = t.icons.map((i, ii) => [i, ii]).filter(([i]) => i.turn);
  const tp = [], tt = [];
  for (const [i, ii] of turns) {
    const o = outline(i.name, readFileSync(join(R, 'icons', i.file), 'utf8').replace(/<!--[\s\S]*?-->/g, ''));
    tt.push(`{${ii}, ${tp.length}, ${o.length}}`);
    tp.push(...o);
  }
  L.push('// turn icons (icons.json turn): the master\'s filled outline on its 24 grid, curves flattened (gen_icons.mjs outline);');
  L.push('// fw::icon_turned fills it at any heading on the device (exact span coverage, no resampled mask)');
  L.push('struct Turn { uint16_t icon, first, n; };', `constexpr Turn TURNS[${Math.max(1, tt.length)}] = {${tt.join(', ') || '{0, 0, 0}'}};`, `constexpr int N_TURNS = ${tt.length};`);
  L.push(`constexpr float TURN_PTS[${Math.max(1, tp.length)}][2] = {${tp.map((q) => `{${q.map((v) => `${+v.toFixed(4)}`.replace(/^(-?\d+)$/, '$1.') + 'f').join(', ')}}`).join(', ') || '{0, 0}'}};`);
  L.push('}  // namespace ic', '}  // namespace fw');
  // Icons page sections (FW_COMPONENT meta): id list + variants
  const sections = [['compare', 'Size comparison'], ...t.groups.map((g) => [g.id, g.label]), ['animated', 'Animated weather'], ['moon', 'Moon phase'], ['all', 'Every icon'], ['memory', 'Flash']];
  L.push(`#define FW_ICON_SECTION_IDS ${cstr(JSON.stringify(sections.map((s) => s[0])))}`);
  L.push(`#define FW_ICON_SECTIONS ${cstr(JSON.stringify(sections.map(([id, label]) => ({ label, props: { section: id } }))))}`);
  L.push(`#define FW_ICON_GROUP_IDS ${cstr(JSON.stringify(['all', ...GROUPS]))}`);
  L.push(`#define FW_ICON_SIZE_IDS ${cstr(JSON.stringify(['all', ...SIZES]))}`);
  L.push(`#define FW_WEATHER_IDS ${cstr(JSON.stringify(t.icons.filter((i) => i.kind === 'weather').map((i) => i.name)))}`);
  L.push(`#define FW_WEATHER_SIZE_IDS ${cstr(JSON.stringify(SIZES.filter((s) => t.icons.some((i) => i.kind === 'weather' && i.sizes.includes(s)))))}`);
  write(out, `${L.join('\n')}\n`);
  process.exit(0);
}

// ---- data header
if (process.argv[2] !== '--data') { console.error('usage: gen_icons.mjs --meta <out.h> | --data <1x|1.5x> <out.h>'); process.exit(2); }
const den = process.argv[3], out = process.argv[4];
if (!['1x', '1.5x'].includes(den) || !out) { console.error('usage: gen_icons.mjs --data <1x|1.5x> <out.h>'); process.exit(2); }
const env = {};
const envFile = join(R, '..', 'local.env');
if (existsSync(envFile)) for (const l of readFileSync(envFile, 'utf8').split('\n')) { const m = l.match(/^([A-Z_]+)=(.*)$/); if (m) env[m[1]] = m[2]; }
const used = (process.env.FW_ICONS ?? env.FW_ICONS ?? 'all') === 'used';
// which icon@size pairs this build carries
const want = new Map();  // name -> Set(size)
if (used) {
  const dir = join(R, 'components');
  for (const f of existsSync(dir) ? readdirSync(dir).filter((f) => f.endsWith('.cpp')) : [])
    for (const m of readFileSync(join(dir, f), 'utf8').matchAll(/"icons"\s*:\s*\[([^\]]*)\]/g))
      for (const p of m[1].matchAll(/"([a-z0-9-]+)@([a-z0-9]+)"/g)) {
        if (!names.has(p[1]) || !SIZES.includes(p[2])) die(`components/${f}: icons "${p[1]}@${p[2]}" is not a shipped icon@size`);
        (want.get(p[1]) ?? want.set(p[1], new Set()).get(p[1])).add(p[2]);
      }
} else {
  for (const i of t.icons) want.set(i.name, new Set(i.sizes));
  for (const n of t.compare.icons) for (const s of t.compare.sizes) want.get(n).add(s);  // compare-only extras
}
// skip the rasterising when nothing it reads changed
const self = readFileSync(fileURLToPath(import.meta.url));
const masters = t.icons.map((i) => readFileSync(join(R, 'icons', i.file)));
const stamp = createHash('sha1').update(self).update(readFileSync(join(R, 'tokens', 'icons.json'))).update(readFileSync(join(R, 'tokens', 'motion.json'))).update(den).update(JSON.stringify([...want].map(([k, v]) => [k, [...v]])));
for (const m of masters) stamp.update(m);
const STAMP = stamp.digest('hex').slice(0, 12);
if (existsSync(out) && readFileSync(out, 'utf8').split('\n', 1)[0].endsWith(STAMP)) process.exit(0);

let Resvg;
try { ({ Resvg } = createRequire(import.meta.url)('@resvg/resvg-js')); }
catch { console.error('gen_icons.mjs: @resvg/resvg-js missing: cd renderer/tools && npm install'); process.exit(1); }

const opticalPx = (size) => {
  const tb = t.stroke.ui.table;
  if (size <= tb[0][0]) return tb[0][1];
  for (let k = 1; k < tb.length; k++) if (size <= tb[k][0]) return tb[k - 1][1] + ((tb[k][1] - tb[k - 1][1]) * (size - tb[k - 1][0])) / (tb[k][0] - tb[k - 1][0]);
  return tb[tb.length - 1][1];
};
// the master with one layer white, every other colour black (occluders), strokes at this size's width
const prepare = (src, kind, role, S) => {
  let s = src;
  const grid = kind === 'weather' ? 48 : 24;
  const pxStroke = kind === 'weather' ? (S >= 96 ? t.stroke.weather.from96 : t.stroke.weather.px) : opticalPx(S);
  const units = Math.round(((pxStroke * grid) / S) * 1000) / 1000;
  s = s.replace(/stroke-width="2"/g, `stroke-width="${units}"`).replace(/vector-effect="[^"]*"/g, '');
  const col = (r) => (r === role ? '#ffffff' : '#000000');
  if (kind === 'weather') {
    s = s.replace(/var\(--facet-color-([a-z-]+)\)/g, (_, c) => (c === 'bg' ? '#000000' : col(c === 'text' ? 'ink' : c)));
  } else {
    s = s.replace(/var\(--facet-color-accent[^)]*\)/g, col('accent')).replace(/currentColor/g, col('ink'));
  }
  if (/var\(/.test(s)) die(`unmapped colour in a ${kind} master: ${s.match(/var\([^)]*\)/)[0]}`);
  return s;
};
const roles = (src, kind) => {
  if (kind === 'ui') return ['ink'];
  if (kind === 'screen') return ['ink', 'accent'];
  const r = new Set();
  for (const m of src.matchAll(/var\(--facet-color-([a-z-]+)\)/g)) if (m[1] !== 'bg') r.add(m[1] === 'text' ? 'ink' : m[1]);
  for (const x of r) if (!ROLES.includes(x)) die(`weather colour ${x} has no role`);
  return ROLES.filter((x) => r.has(x));
};

const bytes = [];  // the A8 pixels of every layer, back to back
const layers = [];  // {role, x, y, w, h, off}
const entries = [];  // {icon, size, first, n, px}
t.icons.forEach((ic, ii) => {
  const sizes = want.get(ic.name);
  if (!sizes) return;
  const src = masters[ii].toString().replace(/<!--[\s\S]*?-->/g, '');
  for (const sz of SIZES) {
    if (!sizes.has(sz)) continue;
    const S = t.sizes[sz][den === '1x' ? '1x' : 'v'];
    const first = layers.length;
    for (const role of roles(src, ic.kind)) {
      const img = new Resvg(prepare(src, ic.kind, role, S), { fitTo: { mode: 'width', value: S }, background: 'rgba(0,0,0,0)', shapeRendering: 2, font: { loadSystemFonts: false } }).render();
      if (img.width !== S || img.height !== S) die(`${ic.name}@${S}: rendered ${img.width}x${img.height}`);
      const px = img.pixels;  // premultiplied RGBA: R = this layer's coverage
      let x0 = S, y0 = S, x1 = -1, y1 = -1;
      for (let y = 0; y < S; y++) for (let x = 0; x < S; x++) if (px[(y * S + x) * 4] > 0) { x0 = Math.min(x0, x); y0 = Math.min(y0, y); x1 = Math.max(x1, x); y1 = Math.max(y1, y); }
      if (x1 < 0) continue;  // nothing of this colour at this size
      const off = bytes.length;
      for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) bytes.push(px[(y * S + x) * 4]);
      layers.push({ role, x: x0, y: y0, w: x1 - x0 + 1, h: y1 - y0 + 1, off });
    }
    entries.push({ icon: ii, size: SIZES.indexOf(sz), first, n: layers.length - first, px: S });
  }
});

// ---- animated weather parts (icons.json animated, motion.json weather): each master cut into parts = its drawn
// elements (a group with a motion class, e.g. the rays, is one part; other groups are opened), in document order; each
// part rasterised ALONE (nothing cut out by a cloud: the cloud's bg fill is its own occluder mask, drawn first in the
// colour behind the icon), one mask per colour, at the animated sizes. motion = index into fw::mo::wx::PARTS (255 =
// still), inst = the master's --i, pivot = the rays' transform-origin in px from the mask's top-left.
const WX = JSON.parse(readFileSync(join(R, 'tokens', 'motion.json'), 'utf8')).weather ?? { parts: {} };
const WXP = Object.entries(WX.parts);
const ASIZES = (t.animated?.sizes ?? []);
for (const s of ASIZES) if (!SIZES.includes(s)) die(`animated: size ${s} not in sizes`);
const parseSvg = (src) => {  // tiny XML reader for the masters: {tag, attrs (raw string), kids, raw}
  const root = { tag: '#root', kids: [] }, stack = [root];
  const re = /<(\/?)([a-zA-Z]+)([^>]*?)(\/?)>/g;
  let m, last = 0;
  while ((m = re.exec(src))) {
    if (m[1]) { const el = stack.pop(); el.raw = src.slice(el.start, m.index + m[0].length); continue; }
    const el = { tag: m[2], attrs: m[3], kids: [], start: m.index };
    stack[stack.length - 1].kids.push(el);
    if (m[4]) el.raw = m[0]; else stack.push(el);
    last = re.lastIndex;
  }
  return root.kids.find((k) => k.tag === 'svg');
};
const attr = (el, a) => (el.attrs.match(new RegExp(`(?:^|\\s)${a}="([^"]*)"`)) ?? [])[1];
const motionOf = (icon, el) => {
  const cls = (attr(el, 'class') ?? '').split(/\s+/).filter((c) => c.startsWith('fwx-')).map((c) => c.slice(4));
  let hit = -1;
  WXP.forEach(([, p], k) => { if (cls.includes(p.class) && (!p.icons || p.icons.includes(icon)) && (hit < 0 || (p.icons && !WXP[hit][1].icons))) hit = k; });
  return hit;
};
const partsOf = (icon, svg) => {
  const out = [];
  const walk = (el) => {
    for (const k of el.kids) {
      const mo = motionOf(icon, k);
      if (k.tag === 'g' && mo < 0) { walk(k); continue; }
      const i = ((attr(k, 'style') ?? '').match(/--i:\s*(\d+)/) ?? [])[1];
      const o = ((attr(k, 'style') ?? '').match(/transform-origin:\s*([\d.]+)px\s+([\d.]+)px/) ?? []);
      const lines = k.tag === 'g' ? k.kids.filter((c) => c.tag === 'line').map((c) => ['x1', 'y1', 'x2', 'y2'].map((a) => +attr(c, a))) : [];
      out.push({ raw: k.raw, motion: mo, inst: i ? +i : 0, pivot: o[1] ? [+o[1], +o[2]] : null, lines });
    }
  };
  walk(svg);
  return out;
};
const alayers = [];  // {role, motion, inst, x, y, w, h, off, px, py}
const rays = [];  // {part, cx, cy, rin, rout, a0, n, w}: px in the S box
const aentries = [];  // {icon, size, first, n, px}
t.icons.forEach((ic, ii) => {
  if (ic.kind !== 'weather' || !want.get(ic.name)) return;
  const src = masters[ii].toString().replace(/<!--[\s\S]*?-->/g, '');
  const svg = parseSvg(src);
  const head = src.slice(src.indexOf('<svg'), src.indexOf('>', src.indexOf('<svg')) + 1);
  const parts = partsOf(ic.name, svg);
  for (const sz of ASIZES) {
    if (!want.get(ic.name).has(sz)) continue;
    const S = t.sizes[sz][den === '1x' ? '1x' : 'v'];
    const first = alayers.length;
    for (const p of parts) {
      const cols = [...new Set([...p.raw.matchAll(/var\(--facet-color-([a-z-]+)\)/g)].map((m) => m[1]))];
      const rs = [];
      if (cols.includes('bg')) rs.push('occluder');
      for (const r of ROLES) if (r !== 'occluder' && cols.some((c) => (c === 'text' ? 'ink' : c) === r)) rs.push(r);
      for (const role of rs) {
        let el = p.raw;
        if (role === 'occluder') el = el.replace(/stroke="var\([^)]*\)"/g, 'stroke="none"');
        const doc = prepare(`${head}${el}</svg>`, 'weather', role === 'occluder' ? 'bg-as-white' : role, S)
        const img = new Resvg(role === 'occluder' ? doc.replace(/fill="#000000"/g, 'fill="#ffffff"') : doc, { fitTo: { mode: 'width', value: S }, background: 'rgba(0,0,0,0)', shapeRendering: 2, font: { loadSystemFonts: false } }).render();
        const px = img.pixels;
        let x0 = S, y0 = S, x1 = -1, y1 = -1;
        for (let y = 0; y < S; y++) for (let x = 0; x < S; x++) if (px[(y * S + x) * 4] > 0) { x0 = Math.min(x0, x); y0 = Math.min(y0, y); x1 = Math.max(x1, x); y1 = Math.max(y1, y); }
        if (x1 < 0) continue;
        const off = bytes.length;
        for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) bytes.push(px[(y * S + x) * 4]);
        const pv = p.pivot ? [Math.round((p.pivot[0] * S) / 48) - x0, Math.round((p.pivot[1] * S) / 48) - y0] : [0, 0];
        alayers.push({ role, motion: p.motion < 0 ? 255 : p.motion, inst: p.inst, x: x0, y: y0, w: x1 - x0 + 1, h: y1 - y0 + 1, off, px: pv[0], py: pv[1] });
        if (p.pivot && p.lines.length) {  // turning rays: drawn on the device from their geometry (no mask rotation)
          const [cx, cy] = p.pivot, [x1, y1, x2, y2] = p.lines[0], k = S / 48;
          const w = S >= 96 ? t.stroke.weather.from96 : t.stroke.weather.px;
          rays.push({ part: alayers.length - 1, cx: cx * k, cy: cy * k, rin: Math.hypot(x1 - cx, y1 - cy) * k, rout: Math.hypot(x2 - cx, y2 - cy) * k, a0: (Math.atan2(y1 - cy, x1 - cx) * 180) / Math.PI, n: p.lines.length, w });
        }
      }
    }
    aentries.push({ icon: ii, size: SIZES.indexOf(sz), first, n: alayers.length - first, px: S });
  }
});
const aBytes = alayers.reduce((n, l) => n + l.w * l.h, 0);
const tableBytes = entries.length * 8 + layers.length * 16 + aentries.length * 8 + alayers.length * 16;
const L = [`// GENERATED by tools/gen_icons.mjs (${den}${used ? ', FW_ICONS=used' : ''}) from tokens/icons.json + icons/: do not edit. stamp ${STAMP}`, '#pragma once', '#include <cstdint>', 'namespace fw {', 'namespace icd {'];
L.push(`constexpr bool USED_ONLY = ${used};`);
L.push('struct Layer { uint8_t role; uint8_t x, y; uint8_t w, h; uint32_t off; };  // A8 rows of w bytes at DATA + off, drawn at (x, y) in the S x S box');
L.push('struct Entry { uint16_t icon; uint8_t size; uint8_t n; uint16_t first; uint16_t px; };');
L.push(`constexpr int N_ENTRIES = ${entries.length}, N_LAYERS = ${layers.length}, DATA_BYTES = ${bytes.length}, TABLE_BYTES = ${tableBytes};`);
L.push('constexpr Entry ENTRIES[] = {');
for (const e of entries) L.push(`  {${e.icon}, ${e.size}, ${e.n}, ${e.first}, ${e.px}},  // ${t.icons[e.icon].name} ${SIZES[e.size]}`);
L.push('};', 'constexpr Layer LAYERS[] = {');
for (let k = 0; k < layers.length; k += 4) L.push(`  ${layers.slice(k, k + 4).map((l) => `{${ROLES.indexOf(l.role)}, ${l.x}, ${l.y}, ${l.w}, ${l.h}, ${l.off}}`).join(', ')},`);
L.push('};', 'alignas(4) static const uint8_t DATA[] = {');
for (let k = 0; k < bytes.length; k += 40) L.push(`  ${bytes.slice(k, k + 40).join(',')},`);
L.push('};');
L.push('struct APart { uint8_t role, motion, inst; uint8_t x, y, w, h; int16_t px, py; uint32_t off; };  // animated weather part mask: motion = fw::mo::wx::PARTS index (255 = still), pivot (px, py) from the mask origin');
L.push(`constexpr int N_AENTRIES = ${aentries.length}, N_APARTS = ${alayers.length}, ANIM_BYTES = ${aBytes};`);
L.push(`constexpr Entry AENTRIES[${Math.max(1, aentries.length)}] = {`);
for (const e of aentries) L.push(`  {${e.icon}, ${e.size}, ${e.n}, ${e.first}, ${e.px}},  // ${t.icons[e.icon].name} ${SIZES[e.size]} animated`);
L.push('};', `constexpr APart APARTS[${Math.max(1, alayers.length)}] = {`);
for (let k = 0; k < alayers.length; k += 3) L.push(`  ${alayers.slice(k, k + 3).map((l) => `{${ROLES.indexOf(l.role)}, ${l.motion}, ${l.inst}, ${l.x}, ${l.y}, ${l.w}, ${l.h}, ${l.px}, ${l.py}, ${l.off}}`).join(', ')},`);
L.push('};');
L.push('// turning sun rays (the rays part of an APART): n round-capped strokes of width w px from rin to rout around (cx, cy),');
L.push('// the first at a0 deg; drawn on the device at each step of the turn (core/icons.cpp); the APART mask = the rest frame');
L.push('struct Rays { uint16_t part; float cx, cy, rin, rout, a0; uint8_t n; float w; };');
L.push(`constexpr int N_RAYS = ${rays.length};`, `constexpr Rays RAYS[${Math.max(1, rays.length)}] = {`);
for (const r of rays) L.push(`  {${r.part}, ${r.cx.toFixed(3)}f, ${r.cy.toFixed(3)}f, ${r.rin.toFixed(3)}f, ${r.rout.toFixed(3)}f, ${r.a0.toFixed(3)}f, ${r.n}, ${r.w}},`);
L.push('};', '}  // namespace icd', '}  // namespace fw');
write(out, `${L.join('\n')}\n`);
const kb = (n) => `${(n / 1024).toFixed(1)} KB`;
console.log(`[${den}] icons: ${new Set(entries.map((e) => e.icon)).size} icons, ${entries.length} icon x size, ${layers.length} masks + ${alayers.length} animated weather part masks (${kb(aBytes)}), ${kb(bytes.length + tableBytes)} flash (A8 ${kb(bytes.length)} + tables ${kb(tableBytes)})${used ? ' [used only]' : ''}`);
