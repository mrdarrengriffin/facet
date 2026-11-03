// Pictures for the site (npm run extract runs it after scripts/extract.mjs): every screen and component picture on the
// site is a PNG made here by the renderer itself. The live renderer (src/scripts/live.ts) runs only as a page's demo
// (a screen page, a component's playground, the foundations); cards, lists and grids are these pictures.
//   main     each screen with a page, as its page's demo starts (src/pages/screens/[id].astro): the docs build's module
//            in node, fed by the site's own sample Home Assistant (src/lib/sample-ha.ts sampleFeeder: the same values,
//            replies and site deck as the live demo), at 720 (1.5x) and 480 (1x), dark and light; half size (-half) for
//            the grids (homepage, Screens overview), full size only for the homepage panel (FULL)
//   comps    each component with its defaults and with each variant (src/pages/components/: the index cards, the
//            variants grid), at its meta frame (auto height when h = 0), cropped to what it draws plus an even margin
//   examples each "Make screens with AI" example (src/lib/ai-examples.json), its spec installed as a panel installs one,
//            on the sample Home Assistant, dark and light (the example page's pair)
// Output: public/shots/*.png (palette PNG, sharp) + src/data/shots.json (the paths and sizes the pages use;
// a copy stays in public/shots/ with the pictures, so a cached folder alone, as CI restores it, is enough).
// Cache: public/shots/key.txt = a hash of the two docs builds' fw.wasm and every input here (this script, the sample HA,
// the screens and the site deck); the same key = nothing rendered. FACET_SHOTS=fresh renders anyway.
import { createHash } from 'node:crypto';
import { copyFileSync, existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { performance } from 'node:perf_hooks';
import sharp from 'sharp';

const SITE = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const ROOT = resolve(SITE, '..');
const R = join(ROOT, 'renderer');
const OUT = join(SITE, 'public/shots');
const DATA = join(SITE, 'src/data/shots.json');
const DENS = [['1.5x', 720], ['1x', 480]];
const THEMES = ['dark', 'light'];
const FULL = new Set(['home']);  // screens shown full size (the homepage panel); the rest only in the grids
const MARGIN = 12;               // dp around a component's picture, every side
const t0 = performance.now();

// the docs build, as scripts/extract.mjs finds it: FW_OUT (env > local.env > renderer/out) + /docs
let fwOut = process.env.FW_OUT;
if (!fwOut && existsSync(join(ROOT, 'local.env'))) fwOut = readFileSync(join(ROOT, 'local.env'), 'utf8').match(/^FW_OUT=(.*)$/m)?.[1]?.trim();
const BUILD = join(resolve(ROOT, fwOut || 'renderer/out'), 'docs');

const rel = (p) => join(ROOT, p);
const INPUTS = [
  ...DENS.map(([d]) => join(BUILD, d, 'fw.wasm')), fileURLToPath(import.meta.url),
  rel('site/src/lib/sample-ha.ts'), rel('site/src/data/site-deck.json'),
  rel('renderer/tools/demo_ha.mjs'), rel('renderer/tools/demo_ha_examples.mjs'), rel('renderer/tools/demo_ha_replies.mjs'),
  rel('renderer/tools/lib/fwbench.mjs'),
  rel('site/src/lib/ai-examples.json'),
];
const EX_DIR = rel('renderer/specs/ai-examples');
// the screens with a page, and only what a picture depends on (screens.json also carries git's status)
const screens = JSON.parse(readFileSync(join(SITE, 'src/data/screens.json'), 'utf8')).filter((s) => s.page);
const h = createHash('sha256');
for (const f of INPUTS) h.update(readFileSync(f));
h.update(JSON.stringify(screens.map((s) => [s.id, s.comp, s.sampleProps, s.frame])));
const comps = JSON.parse(readFileSync(join(SITE, 'src/data/components.json'), 'utf8'));
h.update(JSON.stringify(comps.map((c) => [c.id, c.meta.frame, c.meta.props, c.meta.variants])));
const examples = JSON.parse(readFileSync(rel('site/src/lib/ai-examples.json'), 'utf8')).map((e) => ({ id: e.id, spec: JSON.stringify(JSON.parse(readFileSync(join(EX_DIR, `${e.id}.json`), 'utf8'))) }));
h.update(JSON.stringify(examples));
const KEY = h.digest('hex').slice(0, 16);
const KEYFILE = join(OUT, 'key.txt'), KEPT = join(OUT, 'shots.json');
if (process.env.FACET_SHOTS !== 'fresh' && existsSync(KEPT) && existsSync(KEYFILE) && readFileSync(KEYFILE, 'utf8').trim() === KEY) {
  mkdirSync(dirname(DATA), { recursive: true });
  copyFileSync(KEPT, DATA);
  console.log(`shots: cached (${KEY}), nothing rendered`);
  process.exit(0);
}

const { instance } = await import(pathToFileURL(join(R, 'tools/lib/fwbench.mjs')).href);
const { sampleFeeder, demoNow } = await import(pathToFileURL(join(SITE, 'src/lib/sample-ha.ts')).href);
const realNow = Date.now;  // the panel's time() reads Date.now in node: the demo clock (mid-afternoon) for main, comps, examples

rmSync(OUT, { recursive: true, force: true });
mkdirSync(OUT, { recursive: true });
let files = 0, bytes = 0;
/** an S x S RGBA frame -> public/shots/<name>.png at width w (palette PNG: the panel's flat colours, small) */
async function save(name, rgba, S, w) {
  let img = sharp(Buffer.from(rgba), { raw: { width: S, height: S, channels: 4 } });
  if (w !== S) img = img.resize(w, w, { kernel: 'lanczos3' });
  return write(name, img);
}
async function write(name, img) {
  const buf = await img.png({ palette: true, quality: 95, effort: 10, compressionLevel: 9 }).toBuffer();
  writeFileSync(join(OUT, `${name}.png`), buf);
  files++, bytes += buf.length;
  return `shots/${name}.png`;
}
/** a W x H RGBA frame cropped to what differs from its background (the commonest edge colour) plus m px every side
 *  (past the stage edge filled with the background) -> { src, w, h } */
async function saveCrop(name, px, W, H, m) {
  const n = new Map();
  const at = (x, y) => { const i = (y * W + x) * 4; return (px[i] << 16) | (px[i + 1] << 8) | px[i + 2]; };
  for (let x = 0; x < W; x++) for (const y of [0, H - 1]) n.set(at(x, y), (n.get(at(x, y)) ?? 0) + 1);
  for (let y = 0; y < H; y++) for (const x of [0, W - 1]) n.set(at(x, y), (n.get(at(x, y)) ?? 0) + 1);
  const bg = [...n].sort((a, b) => b[1] - a[1])[0][0], br = bg >> 16, bgG = (bg >> 8) & 255, bb = bg & 255;
  let x0 = W, y0 = H, x1 = -1, y1 = -1;
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
    const i = (y * W + x) * 4;
    if (Math.abs(px[i] - br) + Math.abs(px[i + 1] - bgG) + Math.abs(px[i + 2] - bb) <= 6) continue;
    if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y;
  }
  if (x1 < 0) x0 = 0, y0 = 0, x1 = W - 1, y1 = H - 1;
  const w = x1 - x0 + 1, h = y1 - y0 + 1;
  const img = sharp(Buffer.from(px), { raw: { width: W, height: H, channels: 4 } }).extract({ left: x0, top: y0, width: w, height: h })
    .extend({ top: m, bottom: m, left: m, right: m, background: { r: br, g: bgG, b: bb, alpha: 1 } });
  return { src: await write(name, img), w: w + 2 * m, h: h + 2 * m };
}
const frame = (m, st, S, H = S) => { m._fw_refresh(st); const p = m._fw_frame(st), px = m.HEAPU8.slice(p, p + S * H * 4); for (let i = 3; i < px.length; i += 4) px[i] = 255; return px; };

// ---- main: the screen page's demo (the same comp / props as [id].astro), on a fresh module per picture
const demoOf = (s) => (s.comp === 'spec' ? ['spec', { ...s.sampleProps, live: true }]
  : s.comp === 'home' || s.comp === 'settings' ? ['device', { start: s.comp }] : [s.comp, s.sampleProps]);
async function mainShot(s, den, S, theme, spec) {
  Date.now = demoNow;
  const m = await instance(den, BUILD);
  const host = { m, str: (p) => m.UTF8ToString(p), withStr: (x, f) => { const p = m.stringToNewUTF8(x); try { return f(p); } finally { m._free(p); } } };
  const feed = sampleFeeder(host);
  const [comp, props] = spec ? ['spec', { screen: s.id, live: true }] : demoOf(s);
  if (spec) {  // an AI example: its spec installed first, as a panel installs one (live.ts install)
    const r = JSON.parse(host.withStr('screen', (a) => host.withStr(spec, (b) => m.UTF8ToString(m._fw_spec(a, b)))));
    if (!r.ok) throw new Error(`shots: example ${s.id}: ${JSON.stringify(r.errors ?? r.status)}`);
  }
  // a whole panel (the Device, a live spec stack) takes its look from the panel's Theme setting, as live.ts pushes it
  if (m._fw_setting) host.withStr('theme', (a) => host.withStr(theme, (b) => m._fw_setting(a, b)));
  const st = m._fw_stage(S, S);
  let t = 0;
  const tick = (n) => { for (let i = 0; i < n; i++) { t += 16; m._fw_tick(t); if (i % 16 === 0) feed?.tick(); m._fw_events(); } };
  tick(2);
  const err = host.withStr(comp, (c) => host.withStr(JSON.stringify(props), (p) => m.UTF8ToString(m._fw_render(st, c, p, theme === 'light' ? 1 : 0, -1))));
  if (err) throw new Error(`shots: ${s.id} ${den} ${theme}: ${err}`);
  tick(190);  // ~3 s: the sample values and replies in, entrance motion settled
  for (let k = 0; k < 120 && m._fw_anims(); k++) tick(4);
  const px = frame(m, st, S);
  feed?.stop();
  return px;
}

const out = {};
for (const s of screens) {
  const rec = (out[s.id] = { main: {}, half: {} });
  for (const [den, S] of DENS) {
    rec.main[S] = {}, rec.half[S] = {};
    for (const theme of THEMES) {
      const px = await mainShot(s, den, S, theme);
      if (FULL.has(s.id)) rec.main[S][theme] = await save(`${s.id}-${S}-${theme}`, px, S, S);
      rec.half[S][theme] = await save(`${s.id}-${S}-${theme}-half`, px, S, S / 2);
    }
  }
}

// ---- examples: each AI example's spec on the sample HA, full size, dark and light
const ex = {};
for (const e of examples) {
  ex[e.id] = {};
  for (const [den, S] of DENS) {
    ex[e.id][S] = {};
    for (const theme of THEMES) ex[e.id][S][theme] = await save(`ex-${e.id}-${S}-${theme}`, await mainShot({ id: e.id }, den, S, theme, e.spec), S, S);
  }
}

// ---- comps: one module per size; each picture a fresh stage at the component's frame, settled, then cropped
Date.now = demoNow;
const defaults = (meta) => Object.fromEntries((meta.props ?? []).map((p) => [p.name, p.default ?? (p.type === 'bool' ? false : p.type === 'range' ? p.min ?? 0 : '')]));
const cp = {};
for (const c of comps) cp[c.id] = { base: {}, variants: (c.meta.variants ?? []).map(() => ({})) };
for (const [den, S] of DENS) {
  const m = await instance(den, BUILD), d = S / 480;
  const withStr = (x, f) => { const p = m.stringToNewUTF8(x); try { return f(p); } finally { m._free(p); } };
  let t = 0;
  const tick = (n) => { for (let i = 0; i < n; i++) { t += 16; m._fw_tick(t); m._fw_events(); } };
  async function compShot(c, props, light, name) {
    const W = Math.round(c.meta.frame[0] * d), auto = c.meta.frame[1] === 0;
    let H = auto ? 1 : Math.round(c.meta.frame[1] * d);
    const st = m._fw_stage(W, H);
    const go = () => withStr(c.id, (a) => withStr(JSON.stringify(props), (b) => m.UTF8ToString(m._fw_render(st, a, b, light ? 1 : 0, -1))));
    let err = go();
    if (!err && auto) { const h = Math.max(1, m._fw_content_h(st)); if (h !== H) { m._fw_stage_resize(st, W, h); H = h; err = go(); } }
    if (err) throw new Error(`shots: ${name}: ${err}`);
    tick(40);  // entrance motion settled (a looping one is caught where it is: always the same frame)
    for (let k = 0; k < 40 && m._fw_anims(); k++) tick(4);
    const px = frame(m, st, W, H);
    m._fw_stage_free(st);
    return saveCrop(name, px, W, H, Math.round(MARGIN * d));
  }
  for (const c of comps) {
    const base = defaults(c.meta);
    for (const theme of THEMES) {
      const light = theme === 'light';
      (cp[c.id].base[S] ??= {})[theme] = await compShot(c, base, light, `c-${c.id}-${S}-${theme}`);
      for (const [i, v] of (c.meta.variants ?? []).entries()) (cp[c.id].variants[i][S] ??= {})[theme] = await compShot(c, { ...base, ...v.props }, light, `c-${c.id}-v${i}-${S}-${theme}`);
    }
  }
}

Date.now = realNow;
writeFileSync(DATA, JSON.stringify({ screens: out, examples: ex, comps: cp }) + '\n');
copyFileSync(DATA, KEPT);
writeFileSync(KEYFILE, KEY + '\n');
const sec = ((performance.now() - t0) / 1000).toFixed(1);
console.log(`shots: ${files} pictures, ${(bytes / 1024).toFixed(0)} KB, ${sec} s (${KEY})`);
