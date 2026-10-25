// Facet docs site: metadata extraction (run before every site build: `npm run extract`).
//
// Everything the site shows comes from what the renderer already declares; nothing is hand-written per page:
//   - the WASM build itself (fw.mjs runs in node: build.sh links with -sENVIRONMENT=web,node): fw_meta() = every
//     registered FW_COMPONENT (id, name, section, meta JSON: frame, props, variants), fw_tokens() = the colour tokens
//   - the .cpp sources: the header comment (notes), fw::emit / fw::sound calls (events + sounds; the fw::st:: names
//     resolved through tokens/states.json, the step ticks of core fw::step_ticks), FW_COMPONENT ids -> file
//   - README.md: the "Components" table row per id (description)
//   - tokens/*.json: colour (site theme + colour table), sound (sound names), each foundation's own token file
//   - git: WIP / Done / Done, edited per source file (same rule as the workbench's /api/wasm/status)
// Output: src/data/{site,components,foundations,tokens}.json + public/wasm/<density>/ (a copy of the build, so the
// site is self-contained wherever it is deployed).
//
// Build location: $FW_OUT (env, else local.env at the repo root, else renderer/out), densities 1.5x and 1x: the DOCS
// flavour of the build ($FW_OUT/docs/<density>, `FW_FEATURES=docs renderer/build.sh`): only the features shown in the
// docs (renderer/features.json, README "Feature flags") are in it, so hidden screens, their parts, deck pages and Home
// cards are not on the site at all. Hidden features are also left out of the roadmap and the Updates entries that
// name them (`feature:`), and a shown feature's status (done | wip | preview) is the one its manifest entry says.
import { execFileSync } from 'node:child_process';
import { copyFileSync, existsSync, mkdirSync, readdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { byId, hiddenIds, hiddenRoadmap, load as loadFeatures } from '../../renderer/tools/features.mjs';

const SITE = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const ROOT = resolve(SITE, '..');
const R = join(ROOT, 'renderer');
const REPO = 'https://github.com/mrdarrengriffin/facet/blob/wasm';
const DENS = ['1.5x', '1x'];
// meta "about" sentences are drafts (shown with a "draft" tag) until Darren's writing pass; set false then
const ABOUT_DRAFT = true;

// FW_OUT: env > local.env > renderer/out
let fwOut = process.env.FW_OUT;
if (!fwOut && existsSync(join(ROOT, 'local.env'))) fwOut = readFileSync(join(ROOT, 'local.env'), 'utf8').match(/^FW_OUT=(.*)$/m)?.[1]?.trim();
fwOut = join(resolve(ROOT, fwOut || 'renderer/out'), 'docs');
for (const d of DENS) if (!existsSync(join(fwOut, d, 'fw.mjs'))) { console.error(`extract: no docs build in ${fwOut}/${d} (run FW_FEATURES=docs renderer/build.sh)`); process.exit(1); }
const FEAT = loadFeatures(), FEAT_ID = byId(FEAT), HIDDEN = hiddenIds('docs', FEAT);
const shown = (id) => !HIDDEN.has(id) && FEAT_ID[id]?.docs !== false;

// 1. the WASM registry, read by running the build in node
const base = join(fwOut, '1.5x') + '/';
const { default: create } = await import(pathToFileURL(base + 'fw.mjs').href);
const m = await create({ locateFile: (f) => base + f, print: () => {}, printErr: () => {} });
m._fw_init();
const reg = JSON.parse(m.UTF8ToString(m._fw_meta()));
const fwTokens = JSON.parse(m.UTF8ToString(m._fw_tokens()));

// 2. sources: id -> file, header comment, events, sounds
const json = (p) => JSON.parse(readFileSync(join(R, p), 'utf8'));
const states = json('tokens/states.json').states;
const ST = {
  TAP_EVENT: states.rest.event, TAP_SOUND: states.rest.sound,
  BLOCKED_EVENT: states.disabled.event, BLOCKED_SOUND: states.disabled.sound,
  SWITCH_EVENT: states.on.event, ON_SOUND: states.on.sound, OFF_SOUND: states.on.off_sound,
};
const soundTok = json('tokens/sound.json');
const soundName = Object.fromEntries((soundTok.types ?? []).map((t) => [t.id, { name: t.name, for: t.for }]));

const git = (...a) => { try { return execFileSync('git', ['-C', ROOT, ...a], { encoding: 'utf8' }); } catch { return ''; } };
const tracked = new Set(git('ls-files', '--', 'renderer').split('\n').filter(Boolean));
const changed = new Set(git('status', '--porcelain', '--untracked-files=all', '--', 'renderer').split('\n').filter(Boolean).map((l) => l.slice(3)));
const one = (rel) => (!tracked.has(rel) ? 'wip' : changed.has(rel) ? 'edited' : 'done');
// the status the docs show: the feature's (features.json) when it says wip / preview, else git's (WIP / Done / edited)
const relStatus = (id, git) => (FEAT_ID[id] && FEAT_ID[id].status !== 'done' ? FEAT_ID[id].status : git);
const combine = (xs) => (xs.includes('wip') ? 'wip' : xs.includes('edited') ? 'edited' : 'done');

/** the leading // comment block of a source file, as paragraphs */
function headerComment(src) {
  const lines = [];
  for (const l of src.split('\n')) { if (!l.startsWith('//')) break; lines.push(l.replace(/^\/\/ ?/, '')); }
  return lines.join('\n').trim();
}
/** string-literal or fw::st:: names used as the 2nd argument of fn( */
function calls(src, fn) {
  const out = new Set();
  const re = new RegExp(`${fn}\\(\\s*[^,]+,\\s*([^,)]+(?:\\?[^:]+:[^,)]+)?)`, 'g');
  for (const mm of src.matchAll(re)) {
    const arg = mm[1];
    for (const s of arg.matchAll(/"([^"]+)"/g)) out.add(s[1]);
    for (const s of arg.matchAll(/fw::st::([A-Z_]+)|\bst::([A-Z_]+)/g)) { const v = ST[s[1] ?? s[2]]; if (v) out.add(v); }
  }
  return out;
}
const files = {};
for (const dir of ['components', 'foundations', 'screens']) {
  for (const f of readdirSync(join(R, dir)).filter((f) => f.endsWith('.cpp'))) {
    const rel = `renderer/${dir}/${f}`, src = readFileSync(join(ROOT, rel), 'utf8');
    const rels = [rel];
    const tok = `renderer/tokens/${f.replace(/\.cpp$/, '.json')}`;
    if (dir === 'foundations' && existsSync(join(ROOT, tok))) rels.push(tok);
    const events = calls(src, 'fw::emit'), sounds = calls(src, 'fw::sound');
    if (/st::blocked\(/.test(src)) { events.add(ST.BLOCKED_EVENT); sounds.add(ST.BLOCKED_SOUND); }
    if (/fw::step_ticks\(/.test(src)) { sounds.add('slider-tick'); sounds.add('slider-landmark'); }
    const uses = [...new Set([...src.matchAll(/\bfind\("([a-z0-9_]+)"\)/g)].map((x) => x[1]))];
    const info = { uses, files: rels, status: combine(rels.map(one)), notes: headerComment(src), events: [...events], sounds: [...sounds] };
    for (const mm of src.matchAll(/FW_COMPONENT\(\s*([A-Za-z0-9_]+)|fw::add\(\{\s*"([a-z0-9_]+)"/g)) files[mm[1] ?? mm[2]] = info;
  }
}

// 3. README "Components" rows: | `id` | description |
const readme = readFileSync(join(ROOT, 'README.md'), 'utf8');
const rows = {};
for (const mm of readme.matchAll(/^\| `([a-z0-9_]+)` \| (.+) \|$/gm)) rows[mm[1]] = mm[2];

// 4. one record per registered entry
/** drop "(docs ...)" / "(see ...)" references, parentheses balanced */
function stripRefs(s) {
  let out = '', i = 0;
  while (i < s.length) {
    const m = /\s*\((?:docs|see)\b/.exec(s.slice(i));
    if (!m) { out += s.slice(i); break; }
    out += s.slice(i, i + m.index);
    let j = i + m.index + m[0].length, depth = 1;
    while (j < s.length && depth) { if (s[j] === '(') depth++; else if (s[j] === ')') depth--; j++; }
    i = j;
  }
  return out;
}
// first clause of the README row / header comment: "Facet text button"
const lede = (s) => {
  const t = stripRefs((s ?? '').replace(/\s*\n\s*/g, ' ')).split(/[:;.]\s/)[0].replace(/,\s*WIP$/, '').replace(/`/g, '').trim();
  if (!t) return t;
  const u = t[0].toUpperCase() + t.slice(1);
  return u.length > 160 ? u.slice(0, 157) + '...' : u;
};
const entries = reg.filter((c) => shown(c.id)).map((c) => {
  const f = files[c.id] ?? { uses: [], files: [], status: 'wip', notes: '', events: [], sounds: [] };
  const desc = rows[c.id] ?? '';
  return {
    id: c.id, name: c.name, section: c.section, meta: c.meta,
    // public description: meta "about" (drafts until Darren's writing pass: ABOUT_DRAFT), else the first clause of the README row / header comment
    lede: c.meta.about || lede(desc || f.notes.replace(/^[^:]+:\s*/, '')),
    ledeKind: c.meta.about ? (ABOUT_DRAFT ? 'draft' : 'about') : 'fallback',
    description: desc,
    notes: f.notes,
    events: f.events,
    uses: f.uses.filter((u) => u !== c.id),
    sounds: f.sounds.map((id) => ({ id, ...(soundName[id] ?? { name: id, for: '' }) })),
    status: relStatus(c.id, f.status),
    source: f.files.map((p) => ({ path: p, url: `${REPO}/${p}` })),
  };
});
// public wording (Darren 2026-10-22): no internal references on the site: board ids "(facet-145)", "(lab)" tags,
// leading "Experiment:" / "WIP" markers in names, ledes and section texts
const pub = (t) => typeof t === 'string' ? t.replace(/\s*\((?:facet-\d+|lab)[^)]*\)/gi, '').replace(/\bfacet-\d+\b,?\s*/gi, '').replace(/,?\s*\bWIP( round \d+)?\b\.?/g, '').replace(/^\s*Experiment:\s*/i, '')
  .replace(/\s*by Darren \(\d{4}-\d{2}-\d{2}:[^)]*\)/g, '').replace(/\s*\(Darren \d{4}-\d{2}-\d{2}\)/g, '').replace(/,\s*Darren \d{4}-\d{2}-\d{2}/g, '').trim() : t;
// a shown component's variants / select options that name a hidden feature (Weather's lightning view while the
// Lightning feature is hidden: the docs build has no such view) are left out
const HIDDEN_FEATS = new Set(FEAT.features.filter((f) => !f.docs).map((f) => f.id));
for (const e of entries) {
  e.meta.variants = (e.meta.variants ?? []).filter((v) => !Object.values(v.props ?? {}).some((x) => HIDDEN_FEATS.has(x)));
  for (const pr of e.meta.props ?? []) if (pr.options) pr.options = pr.options.filter((o) => !HIDDEN_FEATS.has(o));
}
for (const e of entries) {
  e.name = pub(e.name);
  e.lede = pub(e.lede);
  e.lede = e.lede && e.lede[0].toUpperCase() + e.lede.slice(1);
  for (const v of e.meta.variants ?? []) { v.label = pub(v.label); if (v.about) v.about = pub(v.about); }
}
const comps = entries.filter((e) => e.section === 'components');
const founds = entries.filter((e) => e.section === 'foundations');
const others = entries.filter((e) => !['components', 'foundations', 'screens', 'bench'].includes(e.section));
if (others.length) console.warn(`extract: sections not on the site yet: ${[...new Set(others.map((e) => e.section))].join(', ')}`);

// 5. tokens for the site itself (theme CSS + colour table)
const colour = json('tokens/colour.json');
const tokens = { colour: { groups: colour.groups, colours: colour.colours, accent: colour.accent }, sets: fwTokens.accent?.sets ?? [] };

// 5b. the site's own CSS custom properties, generated from the same token files the panels use (one source):
// colour (both themes x every accent set), type styles, space / layout / radius / stroke / touch (720 values, plus
// -1x = the 480 values), motion durations + easings + opacities. Written to src/data/tokens.css (Base.astro imports
// it). Names: --facet-color-<name>, --facet-type-<style>-{family,size,line,weight,tracking,case}, --facet-space-<n>,
// --facet-layout-<name>, --facet-radius-<name>, --facet-stroke-<name>, --facet-touch-<name>, --facet-duration-<name>,
// --facet-easing-<name>, --facet-opacity-<name>.
function tokensCss() {
  const px = (v) => (typeof v === 'number' ? `${v}px` : v);
  const decl = (o) => Object.entries(o).map(([k, v]) => `--${k}:${v};`).join('');
  const root = {};
  const fam = json('tokens/type.json').families;
  const stack = { sans: `'${fam.sans.family}', system-ui, sans-serif`, mono: `'${fam.mono.family}', ui-monospace, monospace` };
  root['facet-font-sans'] = stack.sans;
  root['facet-font-mono'] = stack.mono;
  for (const [id, s] of Object.entries(json('tokens/type.json').styles)) {
    for (const [suf, v] of [['', s], ['-1x', { ...s, ...(s['1x'] ?? {}) }]]) {
      const n = `facet-type-${id}${suf}`;
      root[`${n}-family`] = `var(--facet-font-${s.font})`;
      root[`${n}-size`] = px(v.size);
      root[`${n}-line`] = px(v.box);
      root[`${n}-weight`] = v.weight;
      root[`${n}-tracking`] = px(v.letterSpacing ?? 0);
      root[`${n}-case`] = s.case === 'upper' ? 'uppercase' : 'none';
    }
  }
  const sp = json('tokens/space.json').tokens;
  const lay = json('tokens/layout.json').tokens;
  const both = (prefix, group) => {
    for (const [k, v] of Object.entries(group)) {
      if (typeof v !== 'object' || v === null || !('v' in v)) continue;
      const val = (x) => (prefix === 'radius' && k === 'round' ? '999px' : px(x));
      root[`facet-${prefix}-${k}`] = val(v.v);
      if ('1x' in v) root[`facet-${prefix}-${k}-1x`] = val(v['1x']);
    }
  };
  both('space', sp.space);
  both('layout', sp.layout);
  for (const g of Object.values(lay)) both('layout', g);
  both('radius', sp.radius);
  both('stroke', sp.stroke);
  both('touch', sp.touch);
  const mo = json('tokens/motion.json');
  for (const [k, v] of Object.entries(mo.durations)) root[`facet-duration-${k}`] = `${v.ms}ms`;
  for (const [k, v] of Object.entries(mo.easings)) root[`facet-easing-${k}`] = v.css;
  for (const [k, v] of Object.entries(mo.opacity ?? {})) root[`facet-opacity-${k}`] = v.value;
  // colour: dark is the default (as on the panels), light under data-theme=light; accent sets replace the 5 roles
  // + bg-565: the canvas colour as the panels show it (RGB565, what the WASM examples paint), for the areas around
  // an example so the canvas edge doesn't show (#F4F2EE paints as #F7F3EF)
  const q565 = (hex) => {
    const n = parseInt(hex.slice(1), 16), r = (n >> 19) & 31, g = (n >> 10) & 63, b = (n >> 3) & 31;
    const h = (v) => v.toString(16).padStart(2, '0');
    return `#${h((r << 3) | (r >> 2))}${h((g << 2) | (g >> 4))}${h((b << 3) | (b >> 2))}`.toUpperCase();
  };
  const cols = (t) => ({
    ...Object.fromEntries(Object.entries(colour.colours).map(([k, v]) => [`facet-color-${k}`, v[t]])),
    'facet-color-bg-565': q565(colour.colours.bg[t]),
  });
  const set = (s, t) => Object.fromEntries(Object.entries(s[t]).map(([k, v]) => [`facet-color-${k}`, v]));
  let css = `/* generated by scripts/extract.mjs from renderer/tokens/*.json: do not edit */\n`;
  css += `:root{${decl(root)}${decl(cols('dark'))}color-scheme:dark}\n`;
  css += `:root[data-theme=light]{${decl(cols('light'))}color-scheme:light}\n`;
  // a preview box's own theme (the Site / Dark / Light switch): the themed stage area takes that theme's colours
  css += `[data-preview-theme=dark] [data-themed]{${decl(cols('dark'))}color:var(--facet-color-text)}\n`;
  css += `[data-preview-theme=light] [data-themed]{${decl(cols('light'))}color:var(--facet-color-text)}\n`;
  for (const s of colour.accent.sets) {
    css += `:root[data-accent=${s.id}]{${decl(set(s, 'dark'))}}\n:root[data-theme=light][data-accent=${s.id}]{${decl(set(s, 'light'))}}\n`;
  }
  return css;
}

// 5c. icons for the site chrome: the WASM icon masters (renderer/icons, Facet's own SVGs, the set the renderer
// rasterises), inlined; ui + screen icons keep currentColor and the accent detail (var(--facet-color-accent)).
// Stroke = icons.json stroke.ui optical table at the size the site draws them (drawn by the page: Icon.astro).
function iconSet() {
  const ic = json('tokens/icons.json');
  const out = {};
  const read = (f) => readFileSync(join(R, 'icons', f), 'utf8').replace(/<!--[\s\S]*?-->/g, '').trim();
  for (const i of ic.icons) {
    const file = i.file ?? `${i.name}.svg`;
    if (!existsSync(join(R, 'icons', file))) continue;
    const svg = read(file);
    out[i.name] = { kind: i.kind, viewBox: svg.match(/viewBox="([^"]+)"/)?.[1] ?? '0 0 24 24', inner: svg.replace(/^[\s\S]*?<svg[^>]*>/, '').replace(/<\/svg>\s*$/, '').trim() };
  }
  return { stroke: ic.stroke.ui.table, icons: out };
}

// 5d. Roadmap: a public snapshot of the Facet scope list (what is done, being ported, planned). Source = the scope
// table kept next to the project ($FACET_SCOPE, env or local.env at the repo root, only on the build box); written to src/roadmap.json, which is checked in, so builds anywhere else use the last snapshot.
// Public-safe: only name, group, status and the short note; ideas / parked rows and notes that talk about people or
// internal tools are left out.
function roadmapSnapshot() {
  const env = existsSync(join(ROOT, 'local.env')) ? readFileSync(join(ROOT, 'local.env'), 'utf8') : '';
  const src = process.env.FACET_SCOPE ?? env.match(/^FACET_SCOPE=(.*)$/m)?.[1]?.trim();
  if (!src || !existsSync(src)) return null;
  const scope = JSON.parse(readFileSync(src, 'utf8'));
  // public wording only (Darren 2026-10-22): no internal workflow labels
  const STATUS = { done: 'done', wip: 'doing', 'to-port': 'planned', planned: 'planned' };
  const PRIVATE = /\b(darren|claude|board|lab|your|awaiting|rejected|not liked|approved|agent worktree|port|ported|porting|wasm|astro|parity|draft|old site|docs site|wip)\b/i;
  const tables = {};
  for (const [k, rows] of Object.entries(scope.tables ?? {})) {
    tables[k] = rows
      .filter((r) => STATUS[r.status])
      .map((r) => ({ name: r.name, group: r.group, status: STATUS[r.status], ...(r.note && !PRIVATE.test(r.note) ? { note: r.note } : {}) }));
  }
  return { updated: scope.updated ?? scope.generated, tables };
}
// hidden features' rows out of the roadmap (also out of a snapshot made before a flag changed)
const ROAD_HIDE = hiddenRoadmap(FEAT);
const roadFilter = (r) => ({ ...r, tables: Object.fromEntries(Object.entries(r.tables).map(([k, rows]) => [k, rows.filter((x) => !ROAD_HIDE.has(x.name))])) });

// 5e. Home page previews (Darren 2026-10-22): still PNGs rendered here by the WASM build itself (the 1.5x / 720
// build already running in node), both themes, so they stay current with the code. Foundations: one section each
// (cropped to its top), components: their first variant at the meta frame. Written to public/previews/<id>-<theme>.png
// + src/data/previews.json. Plus one or two grid rules quoted from layout.json rules (when present).
import { deflateSync } from 'node:zlib';
function png(w, h, rgba) {
  const crcT = new Int32Array(256).map((_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; return c; });
  const crc = (b) => { let c = -1; for (const x of b) c = crcT[(c ^ x) & 255] ^ (c >>> 8); return (c ^ -1) >>> 0; };
  const chunk = (type, data) => { const t = Buffer.from(type), len = Buffer.alloc(4), cr = Buffer.alloc(4); len.writeUInt32BE(data.length); cr.writeUInt32BE(crc(Buffer.concat([t, data]))); return Buffer.concat([len, t, data, cr]); };
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 2; // RGB
  const raw = Buffer.alloc((w * 3 + 1) * h);
  for (let y = 0; y < h; y++) { raw[y * (w * 3 + 1)] = 0; for (let x = 0; x < w; x++) { const i = (y * w + x) * 4, o = y * (w * 3 + 1) + 1 + x * 3; raw[o] = rgba[i]; raw[o + 1] = rgba[i + 1]; raw[o + 2] = rgba[i + 2]; } }
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', deflateSync(raw, { level: 9 })), chunk('IEND', Buffer.alloc(0))]);
}
function previews() {
  const out = join(SITE, 'public/previews');
  mkdirSync(out, { recursive: true });
  const S = (str, f) => { const p = m.stringToNewUTF8(str); try { return f(p); } finally { m._free(p); } };
  const defaults = (meta) => Object.fromEntries((meta.props ?? []).map((p) => [p.name, p.default ?? (p.type === 'bool' ? false : p.type === 'range' ? p.min ?? 0 : '')]));
  const shots = [
    ...[['layout', 'Screen overlay'], ['colour', ''], ['type', '']].map(([id, v]) => ({ id, section: 'foundations', variant: v, crop: 540 })),
    ...[['card', ''], ['tile', ''], ['button', ''], ['toggle', 'on'], ['slider', 'value · 40'], ['dial', '']].map(([id, v]) => ({ id, section: 'components', variant: v })),
  ];
  const done = [];
  let t = 0;
  for (const sh of shots) {
    const c = reg.find((x) => x.id === sh.id);
    if (!c) continue;
    // variant by label ('' = the first)
    const meta = c.meta, v = meta.variants?.find((x) => x.label === sh.variant) ?? meta.variants?.[0];
    const props = { ...defaults(meta), ...(v?.props ?? {}) };
    const auto = meta.frame[1] === 0;
    const w = sh.section === 'foundations' ? 720 : Math.round(meta.frame[0] * 1.5);
    let h = auto ? 1 : Math.round(meta.frame[1] * 1.5);
    const rec = { id: sh.id, section: sh.section, label: v?.label ?? c.name, name: c.name, w, h: 0 };
    for (const theme of ['dark', 'light']) {
      const id = m._fw_stage(w, h);
      const go = () => S(sh.id, (cp) => S(JSON.stringify(props), (pp) => m.UTF8ToString(m._fw_render(id, cp, pp, theme === 'light' ? 1 : 0, -1))));
      let err = go();
      if (!err && auto) { const ch = Math.max(1, m._fw_content_h(id)); m._fw_stage_resize(id, w, ch); h = ch; err = go(); }
      if (err) { console.warn(`extract: preview ${sh.id}: ${err}`); m._fw_stage_free(id); continue; }
      for (let k = 0; k < 120; k++) { t += 16; m._fw_tick(t); }  // let entry animations settle (~2 s)
      const p = m._fw_frame(id);
      const ph = Math.min(h, sh.crop ?? h);
      const rgba = Buffer.from(m.HEAPU8.subarray(p, p + w * ph * 4));
      writeFileSync(join(out, `${sh.id}-${theme}.png`), png(w, ph, rgba));
      rec.h = ph;
      m._fw_stage_free(id);
      if (auto) h = 1;
    }
    if (rec.h) done.push(rec);
  }
  // grid rules to quote (layout.json rules, first sentence, no names / dates in parentheses)
  const rules = json('tokens/layout.json').rules?.sections?.find((x) => x.id === 'grid')?.items ?? [];
  const quote = (s) => s.replace(/\s*\((?:Darren|see)[^)]*\)/g, '').replace(/`/g, '').split(/(?<=\.)\s/)[0];
  return { shots: done, rules: [rules[0], rules[2]].filter(Boolean).map(quote) };
}


// 5f. Screens (docs Screens section): every factory spec (renderer/specs/*.json) + every registered screen (section
// "screens" of the build, minus the workbench tools in SCREEN_SKIP). Per screen: what it shows (registered: meta
// about; spec: its main component's about), its typed parameters (core/spec.h "params"), its data sources
// (renderer/sources/sources.json: the screen's own entry, else its parent's: weather_days -> weather), the parts it is
// built from (spec nodes + the parts core/spec.h draws itself, e.g. the header; registered: its fw::find calls), the
// screens it opens / is opened from (nav.open actions), status (git rule as above). Public: entity defaults are never
// shown (they are the build's own panels' picks); the preview opens with sample entities (sampleParams), whose values
// come from site/src/lib/sample-ha.ts.
const SCREEN_SKIP = new Set(['spec', 'device']);
function screenSet() {
  const srcs = json('sources/sources.json');
  const chrome = [...new Set([...readFileSync(join(R, 'core/spec.h'), 'utf8').matchAll(/\bfind\("([a-z0-9_]+)"\)/g)].map((x) => x[1]))];
  const compById = Object.fromEntries(entries.map((e) => [e.id, e]));
  const sourcesOf = (id) => {
    const own = srcs.screens[id], parent = !own && id.includes('_') ? id.split('_')[0] : null;
    const e = own ?? (parent && srcs.screens[parent]);
    if (!e) return null;
    return {
      from: own ? null : parent, needs: e.needs,
      list: e.sources.map((x) => ({ id: x.id, kind: x.kind, kindLabel: srcs.kinds[x.kind] ?? x.kind, name: x.name, setup: x.setup ?? [],
        fills: Object.entries(x.fills ?? {}).map(([param, f]) => ({ param, domain: f.domain, integration: f.integration ?? null, list: !!f.list })) })),
    };
  };
  const sampleOf = (k, t) => {
    if (t.type === 'entity') return t.domain === 'sensor' ? '' : `${t.domain}.sample_1`;
    if (t.type === 'entities') return [1, 2, 3].slice(0, t.domain === 'calendar' ? 2 : 3).map((i) => `${t.domain}.sample_${i}`).join(',');
    return t.default ?? '';
  };
  const out = [];
  const opens = {};
  const walkOpen = (id, v) => {
    if (Array.isArray(v)) return v.forEach((x) => walkOpen(id, x));
    if (v && typeof v === 'object') {
      if (v.do === 'nav.open' && typeof v.screen === 'string') (opens[id] ??= new Set()).add(v.screen);
      Object.values(v).forEach((x) => walkOpen(id, x));
    }
  };
  for (const f of readdirSync(join(R, 'specs')).filter((f) => f.endsWith('.json')).sort()) {
    const spec = json(`specs/${f}`);
    if (!spec.id || !Array.isArray(spec.nodes)) continue;  // the deck, not a screen
    if (!shown(spec.id)) continue;  // a hidden feature (features.json)
    walkOpen(spec.id, spec.nodes);
    const params = Object.entries(spec.params ?? {}).map(([name, v]) => {
      const t = typeof v === 'object' && v ? v : { type: 'text', default: v };
      return { name, label: t.label ?? name, type: t.type ?? 'text', domain: t.domain ?? null, options: t.options ?? null,
        default: t.type === 'entity' || t.type === 'entities' ? null : (t.default ?? ''), sample: sampleOf(name, t) };
    }).filter((p) => p.type !== 'hidden');
    const nodes = spec.nodes.map((n) => n.c);
    const parts = [...new Set([...nodes, ...(spec.header ? chrome : [])])].filter((c) => compById[c]);
    const main = compById[nodes[0]];
    const rows = nodes.every((c) => c === 'list_row');
    const about = rows
      ? `A list of ${nodes.length} rows, one per device: each shows the device's state, and a tap switches it.`
      : main?.meta.about || main?.lede || '';
    const rel = `renderer/specs/${f}`;
    out.push({ id: spec.id, name: pub(spec.name), kind: 'spec', about: pub(about), layout: spec.layout, params,
      sampleProps: { screen: spec.id, ...Object.fromEntries(params.filter((p) => p.sample).map((p) => [p.name, p.sample])) },
      comp: 'spec', frame: [480, 480], parts, sources: sourcesOf(spec.id), status: relStatus(spec.id, one(rel)), source: [{ path: rel, url: `${REPO}/${rel}` }] });
  }
  for (const c of entries.filter((e) => e.section === 'screens' && !SCREEN_SKIP.has(e.id))) {
    const f = files[c.id] ?? { uses: [], files: [], status: 'wip' };
    const props = (c.meta.props ?? []).filter((p) => !(p.type === 'text' && !p.default));
    out.push({ id: c.id, name: c.name, kind: 'built', about: pub(c.meta.about ?? c.lede ?? ''), opening: c.meta.opening ? pub(c.meta.opening) : null,
      params: props.map((p) => ({ name: p.name, label: p.name.replace(/_/g, ' ').replace(/([a-z])([A-Z])/g, '$1 $2').toLowerCase().replace(/^./, (x) => x.toUpperCase()),
        type: p.type, domain: null, options: p.options ?? (p.type === 'range' ? [`${p.min ?? 0} to ${p.max}`] : null), default: p.default ?? '' })),
      sampleProps: Object.fromEntries((c.meta.props ?? []).map((p) => [p.name, p.default ?? (p.type === 'bool' ? false : p.type === 'range' ? p.min ?? 0 : '')])),
      comp: c.id, frame: c.meta.frame, parts: f.uses.filter((u) => compById[u] && u !== c.id), sources: sourcesOf(c.id), status: relStatus(c.id, f.status),
      source: f.files.map((p) => ({ path: p, url: `${REPO}/${p}` })) });
  }
  for (const s of out) {
    s.opens = [...(opens[s.id] ?? [])].filter((x) => out.some((o) => o.id === x));
    s.openedFrom = Object.entries(opens).filter(([, v]) => v.has(s.id)).map(([k]) => k);
  }
  return out;
}

const manifests = Object.fromEntries(DENS.map((d) => [d, JSON.parse(readFileSync(join(fwOut, d, 'manifest.json'), 'utf8'))]));
const motion = json('tokens/motion.json');
const site = {
  // a demo sample returns to its initial state this long after the last interaction (motion.json durations.demo-reset)
  demoResetMs: motion.durations?.['demo-reset']?.ms ?? 1800,
  built: new Date().toISOString(),
  commit: git('rev-parse', '--short', 'HEAD').trim(),
  repo: REPO,
  wasm: manifests,
};

const out = join(SITE, 'src/data');
mkdirSync(out, { recursive: true });
const w = (n, v) => writeFileSync(join(out, n), JSON.stringify(v, null, 1) + '\n');
w('site.json', site);
w('components.json', comps);
w('foundations.json', founds);
w('tokens.json', tokens);
w('icons.json', iconSet());
w('previews.json', previews());
const screens = screenSet();
w('screens.json', screens);
const road = roadmapSnapshot() ?? JSON.parse(readFileSync(join(SITE, 'src/roadmap.json'), 'utf8'));
writeFileSync(join(SITE, 'src/roadmap.json'), JSON.stringify(roadFilter(road), null, 1) + '\n');
// feature flags for the pages: hidden features (Updates entries tagged with one are left out) + the paths of hidden
// pages (links to them are dropped)
const hiddenFeatures = FEAT.features.filter((f) => !f.docs).map((f) => f.id);
const hiddenPaths = [...HIDDEN].flatMap((id) => [`screens/${id.replace(/_/g, '-')}/`, `components/${id.replace(/_/g, '-')}/`, `foundations/${id.replace(/_/g, '-')}/`])
  .filter((p) => !(p.startsWith('screens/') ? screens : entries).some((e) => p.endsWith(`/${e.id.replace(/_/g, '-')}/`)));
w('features.json', { hiddenFeatures, hiddenPaths, status: FEAT.man.status });
writeFileSync(join(out, 'tokens.css'), tokensCss());
// sound: the packs for the header picker; the whole sound.json is copied to public/ (fetched on the first sound)
w('sound-packs.json', { default: soundTok.default, variants: soundTok.variants.map((v) => ({ id: v.id, name: v.name, about: v.about })) });
mkdirSync(join(SITE, 'public'), { recursive: true });
copyFileSync(join(R, 'tokens/sound.json'), join(SITE, 'public/sound.json'));

// 6. copy the build into public/wasm/<density>/
for (const d of DENS) {
  const to = join(SITE, 'public/wasm', d);
  mkdirSync(to, { recursive: true });
  for (const f of ['fw.mjs', 'fw.wasm', 'manifest.json']) copyFileSync(join(fwOut, d, f), join(to, f));
}
console.log(`extract: ${comps.length} components, ${founds.length} foundations, ${screens.length} screens, build ${manifests['1.5x'].built} (${fwOut})`);
