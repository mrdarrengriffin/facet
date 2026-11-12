// README images (.github/assets/) rendered by the WASM renderer itself: no browser, no dependencies.
//   node tools/readme_assets.mjs --list                      the screens the docs build registers
//   node tools/readme_assets.mjs [--screens id,spec:weather,spec:weather?source=sample,...] [--hero id,...]
// Without --screens: the screens the root README shows (its screen-<id>-*.png), a spec screen with its sample source
// when it has one; the hero: HERO below. A screen that is not Stable (features.json, RELEASING.md "Screen channels")
// gets its channel's chip in the bottom-right corner (tools/lib/chip.mjs: BETA, COMING SOON), drawn before the halving.
// Reads the DOCS flavour build ($FW_OUT/docs/1.5x, `FW_FEATURES=docs ./build.sh`), renders each screen at 720 x 720 in
// dark and light, halves it (2 x 2 box filter) to 360 with rounded corners, and writes
//   .github/assets/screen-<id>-<theme>.png   one per screen (shown at 200 in the README)
//   .github/assets/screens-<theme>.png       the hero: the --hero screens in a 4-wide grid, transparent around them
// except screen-media-<theme>.png (left alone unless --screens names it): Media with album art, which the sample
// source has none of, so it is the Media panel of release_shots.mjs' `art` picture (720:@media), cropped out of its
// frame (26 px) and rounded the same way (radius 32)
//   node tools/readme_assets.mjs --ai-examples   instead: each "Make screens with AI" example (specs/ai-examples, installed as a
//   panel installs a spec, demo HA values) -> site/public/ai/shots/<id>-<theme>.png (the tiles on the docs /ai/ page)
//   node tools/readme_assets.mjs --coming-soon   instead: the root README's Coming soon strip (no build needed: the screens
//   are on the wip branch, features.json wip): each picture's site/public/coming-soon/<id>-720.png with the COMING SOON
//   chip, halved to 360 with rounded corners -> .github/assets/coming-soon-<id>.png (dark only: the pictures are)
import { readFileSync, writeFileSync, existsSync, mkdirSync, readdirSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { deflateSync, inflateSync } from 'node:zlib';
import { demoValue } from './demo_ha.mjs';
import { channels } from './features.mjs';
import { drawChip } from './lib/chip.mjs';

const HERE = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const ROOT = resolve(HERE, '..');
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const env = (() => { try { return Object.fromEntries(readFileSync(join(ROOT, 'local.env'), 'utf8').split('\n').map((l) => l.match(/^([A-Z_]+)=(.*)$/)).filter(Boolean).map((m) => [m[1], m[2]])); } catch { return {}; } })();
if (process.argv.includes('--coming-soon')) {
  // the README strip (features.json wip has them all); top: the chip top-right, where bottom-right would hide a value
  const STRIP = [['f1-live'], ['tennis-live'], ['plants', 'top'], ['blinds']];
  for (const [id, corner] of STRIP) {
    const { W, H: h, rgba } = unpng(readFileSync(join(ROOT, 'site', 'public', 'coming-soon', `${id}-720.png`)));
    if (W !== 720 || h !== 720) throw new Error(`coming soon ${id}: ${W} x ${h}, want 720 x 720`);
    writeFileSync(join(ROOT, '.github', 'assets', `coming-soon-${id}.png`), png(halve(drawChip(rgba, 720, 'soon', 'dark', corner), 720), 360, 360));
  }
  console.log(`readme_assets: ${STRIP.length} coming soon pictures -> .github/assets/coming-soon-*.png`);
  process.exit(0);
}
const BUILD = join(resolve(ROOT, process.env.FW_OUT || env.FW_OUT || join(HERE, 'out')), 'docs', '1.5x');
if (!existsSync(join(BUILD, 'fw.mjs'))) { console.error(`readme_assets: no docs build in ${BUILD} (FW_FEATURES=docs ./build.sh)`); process.exit(1); }
const OUT = join(ROOT, '.github', 'assets');
mkdirSync(OUT, { recursive: true });

const F = (await import(pathToFileURL(join(BUILD, 'fw.mjs')).href)).default;
const wasm = await WebAssembly.compile(readFileSync(join(BUILD, 'fw.wasm')));
const m = await F({ instantiateWasm: (imports, ok) => { WebAssembly.instantiate(wasm, imports).then((i) => ok(i, wasm)); return {}; }, print: () => {}, printErr: () => {} });
m._fw_init();
// Home's cards as on a panel with nothing linked yet would read Home Assistant ("loading"): for the README the docs
// site's deck (site/src/data/site-deck.json, from site/scripts/extract.mjs: a Home with every card size, filling the
// grid), else the deck with every screen, with every page that has a sample source on it (Weather's sample forecast)
{
  const site = new URL('../../site/src/data/site-deck.json', import.meta.url);
  const deck = JSON.parse(readFileSync(existsSync(site) ? site : new URL('../tests/fixtures/deck-full.json', import.meta.url), 'utf8'));
  for (const p of deck.pages) {
    let sp = null;
    try { sp = JSON.parse(readFileSync(new URL(`../specs/${p.kind}.json`, import.meta.url), 'utf8')); } catch {}
    if (sp?.params?.source?.options?.includes('sample')) p.params = { ...(p.params ?? {}), source: 'sample' };
  }
  deck.rev = (deck.rev ?? 1) + 1;
  const s1 = m.stringToNewUTF8('deck'), s2 = m.stringToNewUTF8(JSON.stringify(deck));
  const r = m.UTF8ToString(m._fw_spec(s1, s2));
  m._free(s1), m._free(s2);
  if (!JSON.parse(r).ok) throw new Error(`readme deck refused: ${r}`);
}
const meta = JSON.parse(m.UTF8ToString(m._fw_meta())).map((c) => ({ ...c, meta: typeof c.meta === 'string' ? JSON.parse(c.meta) : c.meta }));
const screens = meta.filter((c) => c.section === 'screens');
if (process.argv.includes('--list')) { for (const c of screens) console.log(c.id, '|', c.meta?.title || c.title || '', '|', c.meta?.desc || c.meta?.description || ''); process.exit(0); }

const str = (s, f) => { const p = m.stringToNewUTF8(s); try { return f(p); } finally { m._free(p); } };
// Demo HA values so the images look alive (tools/demo_ha.mjs, shared with the docs site's previews)
const put = new Set();
const demo = demoValue;
function feed() {
  for (const [e, a] of JSON.parse(m.UTF8ToString(m._fw_ha_wants()) || '[]')) {
    const k = `${e}|${a}`, v = demo(e, a);
    if (put.has(k) || v === undefined) continue;
    put.add(k);
    str(e, (ep) => str(a, (ap) => str(v, (vp) => m._fw_ha_put(ep, ap, vp))));
  }
}
const S = 720, H = 360;
let t = 0;
// "<id>" = a registered screen, "spec:<screen>" = the spec screen with that built-in spec
const file = (id) => id.replace(/^spec:/, '').replace(/\?.*$/, '');  // spec:weather?source=sample -> weather
const CHANNEL = channels();
const chipOf = (id) => (CHANNEL[file(id)]?.channel ?? 'stable');
function render(id, light) {
  // spec:<id>?k=v&...: the screen with those params (a README shot of a Home Assistant screen with its sample data)
  const params = Object.fromEntries(new URLSearchParams(id.includes('?') ? id.slice(id.indexOf('?') + 1) : ''));
  // home = the device (Home as the panel shows it: the stack's root, no Back)
  const [comp, props] = id.startsWith('spec:') ? ['spec', { screen: file(id), ...params }] : id === 'home' ? ['device', {}] : [id, {}];
  const st = m._fw_stage(S, S);
  const tick = (n) => { for (let i = 0; i < n; i++) { t += 16; m._fw_tick(t); } m._fw_events(); };
  tick(2);
  const e = str(comp, (cp) => str(JSON.stringify(props), (pp) => m.UTF8ToString(m._fw_render(st, cp, pp, light ? 1 : 0, 0))));
  if (e) { m._fw_stage_free(st); throw new Error(`${id}: ${e}`); }
  feed();  // the demo HA values for what this screen binds (moves through the update path)
  tick(120);  // ~2 s: entrance motion settled
  m._fw_refresh(st);
  const p = m._fw_frame(st), b = m.HEAPU8.slice(p, p + S * S * 4);
  m._fw_stage_free(st);
  if (chipOf(id) !== 'stable') drawChip(b, S, chipOf(id), light ? 'light' : 'dark');
  return halve(b, S);
}
/** an S x S RGBA frame -> S / 2 by a 2 x 2 box filter, rounded corners (radius 32, anti-aliased alpha) */
function halve(b, S) {
  const H = S / 2, R = 32, o = new Uint8Array(H * H * 4);
  for (let y = 0; y < H; y++) for (let x = 0; x < H; x++) {
    const i = (y * H + x) * 4;
    for (let c = 0; c < 3; c++) { let s = 0; for (const [dx, dy] of [[0, 0], [1, 0], [0, 1], [1, 1]]) s += b[((2 * y + dy) * S + 2 * x + dx) * 4 + c]; o[i + c] = (s + 2) >> 2; }
    const cx = Math.min(x + 0.5, H - x - 0.5), cy = Math.min(y + 0.5, H - y - 0.5);
    let a = 1;
    if (cx < R && cy < R) a = Math.max(0, Math.min(1, R - Math.hypot(R - cx, R - cy) + 0.5));
    o[i + 3] = Math.round(a * 255);
  }
  return o;
}
/** a PNG (8-bit RGB or RGBA, not interlaced) -> { W, H, rgba } (opaque when RGB) */
function unpng(b) {
  let p = 8, W = 0, Hh = 0, n = 4;
  const idat = [];
  while (p < b.length) {
    const len = b.readUInt32BE(p), type = b.toString('latin1', p + 4, p + 8), d = b.subarray(p + 8, p + 8 + len);
    if (type === 'IHDR') {
      W = d.readUInt32BE(0); Hh = d.readUInt32BE(4);
      if (d[8] !== 8 || (d[9] !== 2 && d[9] !== 6) || d[12]) throw new Error('unpng: 8-bit RGB or RGBA, not interlaced');
      n = d[9] === 6 ? 4 : 3;
    }
    if (type === 'IDAT') idat.push(d);
    p += 12 + len;
  }
  const raw = inflateSync(Buffer.concat(idat)), st = W * n, row = new Uint8Array(W * Hh * n), out = new Uint8Array(W * Hh * 4);
  for (let y = 0; y < Hh; y++) {
    const f = raw[y * (st + 1)];
    for (let x = 0; x < st; x++) {
      const v = raw[y * (st + 1) + 1 + x], a = x >= n ? row[y * st + x - n] : 0, u = y ? row[(y - 1) * st + x] : 0, c = x >= n && y ? row[(y - 1) * st + x - n] : 0;
      const pa = Math.abs(u - c), pb = Math.abs(a - c), pc = Math.abs(a + u - 2 * c);
      row[y * st + x] = (v + [0, a, u, (a + u) >> 1, pa <= pb && pa <= pc ? a : pb <= pc ? u : c][f]) & 255;
    }
  }
  for (let i = 0; i < W * Hh; i++) { for (let c = 0; c < 3; c++) out[i * 4 + c] = row[i * n + c]; out[i * 4 + 3] = n === 4 ? row[i * n + 3] : 255; }
  return { W, H: Hh, rgba: out };
}
function png(rgba, W, Hh) {
  const crcT = new Int32Array(256).map((_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; return c; });
  const crc = (buf) => { let c = -1; for (const x of buf) c = crcT[(c ^ x) & 255] ^ (c >>> 8); return (c ^ -1) >>> 0; };
  const chunk = (type, data) => { const l = Buffer.alloc(4); l.writeUInt32BE(data.length); const td = Buffer.concat([Buffer.from(type), data]); const c = Buffer.alloc(4); c.writeUInt32BE(crc(td)); return Buffer.concat([l, td, c]); };
  const ih = Buffer.alloc(13); ih.writeUInt32BE(W, 0); ih.writeUInt32BE(Hh, 4); ih[8] = 8; ih[9] = 6;
  const raw = Buffer.alloc((W * 4 + 1) * Hh);
  for (let y = 0; y < Hh; y++) Buffer.from(rgba.buffer, rgba.byteOffset + y * W * 4, W * 4).copy(raw, y * (W * 4 + 1) + 1);
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ih), chunk('IDAT', deflateSync(raw, { level: 9 })), chunk('IEND', Buffer.alloc(0))]);
}

if (process.argv.includes('--ai-examples')) {
  const dir = join(HERE, 'specs', 'ai-examples'), out = join(ROOT, 'site', 'public', 'ai', 'shots');
  mkdirSync(out, { recursive: true });
  // only the examples the docs list (site/src/lib/ai-examples.json): a held one (unreleased component) gets no shot
  const ex = JSON.parse(readFileSync(join(ROOT, 'site', 'src', 'lib', 'ai-examples.json'), 'utf8')).map((e) => e.id).sort();
  for (const id of ex) {
    const r = JSON.parse(str('screen_dev', (kp) => str(readFileSync(join(dir, `${id}.json`), 'utf8'), (sp) => m.UTF8ToString(m._fw_spec(kp, sp)))));
    if (!r.ok) throw new Error(`${id}: spec ${JSON.stringify(r.errors ?? r.status)}`);
    for (const theme of ['dark', 'light']) writeFileSync(join(out, `${id}-${theme}.png`), png(render(`spec:${id}`, theme === 'light'), H, H));
  }
  console.log(`readme_assets: ${ex.length} AI examples x 2 themes -> site/public/ai/shots/`);
  process.exit(0);
}
// a README screen id as rendered: registered screens as they are, a spec screen with its sample source when it has one
const asShot = (id) => {
  if (screens.some((c) => c.id === id)) return id;
  const sp = JSON.parse(readFileSync(join(HERE, 'specs', `${id}.json`), 'utf8'));
  return sp.params?.source?.options?.includes('sample') ? `spec:${id}?source=sample` : `spec:${id}`;
};
const README_IDS = [...new Set([...readFileSync(join(ROOT, 'README.md'), 'utf8').matchAll(/assets\/screen-([a-z0-9_]+)-dark\.png/g)].map((x) => x[1]))];
const HERO = ['home', 'media', 'weather', 'clock', 'light', 'energy', 'calendar', 'timer'];  // the README's alt text names them
const ids = arg('--screens') ? arg('--screens').split(',') : README_IDS.map(asShot);
const hero = arg('--hero') ? arg('--hero').split(',') : HERO.map(asShot);
const shots = {};
for (const theme of ['dark', 'light']) {
  for (const id of ids) {
    if (file(id) === 'media' && !arg('--screens')) continue;  // album art: see the header
    shots[`${id}-${theme}`] = render(id, theme === 'light');
    writeFileSync(join(OUT, `screen-${file(id)}-${theme}.png`), png(shots[`${id}-${theme}`], H, H));
  }
  const cols = 4, gap = 24, rows = Math.ceil(hero.length / cols), W = cols * H + (cols - 1) * gap, HH = rows * H + (rows - 1) * gap;
  const g = new Uint8Array(W * HH * 4);
  hero.forEach((id, k) => {
    const s = shots[`${id}-${theme}`] || render(id, theme === 'light'), ox = (k % cols) * (H + gap), oy = Math.floor(k / cols) * (H + gap);
    for (let y = 0; y < H; y++) g.set(s.subarray(y * H * 4, (y + 1) * H * 4), ((oy + y) * W + ox) * 4);
  });
  writeFileSync(join(OUT, `screens-${theme}.png`), png(g, W, HH));
}
console.log(`readme_assets: ${ids.length} screens x 2 themes + hero (${hero.join(', ')}) -> .github/assets/`);
