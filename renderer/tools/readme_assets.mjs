// README images (.github/assets/) rendered by the WASM renderer itself: no browser, no dependencies.
//   node tools/readme_assets.mjs --list                      the screens the docs build registers
//   node tools/readme_assets.mjs [--screens id,spec:weather,...] [--hero id,...]
// Reads the DOCS flavour build ($FW_OUT/docs/1.5x, `FW_FEATURES=docs ./build.sh`), renders each screen at 720 x 720 in
// dark and light, halves it (2 x 2 box filter) to 360 with rounded corners, and writes
//   .github/assets/screen-<id>-<theme>.png   one per screen (shown at 200 in the README)
//   .github/assets/screens-<theme>.png       the hero: the --hero screens in a 4-wide grid, transparent around them
import { readFileSync, writeFileSync, existsSync, mkdirSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { deflateSync } from 'node:zlib';
import { demoValue } from './demo_ha.mjs';

const HERE = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const ROOT = resolve(HERE, '..');
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const env = (() => { try { return Object.fromEntries(readFileSync(join(ROOT, 'local.env'), 'utf8').split('\n').map((l) => l.match(/^([A-Z_]+)=(.*)$/)).filter(Boolean).map((m) => [m[1], m[2]])); } catch { return {}; } })();
const BUILD = join(resolve(ROOT, process.env.FW_OUT || env.FW_OUT || join(HERE, 'out')), 'docs', '1.5x');
if (!existsSync(join(BUILD, 'fw.mjs'))) { console.error(`readme_assets: no docs build in ${BUILD} (FW_FEATURES=docs ./build.sh)`); process.exit(1); }
const OUT = join(ROOT, '.github', 'assets');
mkdirSync(OUT, { recursive: true });

const F = (await import(pathToFileURL(join(BUILD, 'fw.mjs')).href)).default;
const wasm = await WebAssembly.compile(readFileSync(join(BUILD, 'fw.wasm')));
const m = await F({ instantiateWasm: (imports, ok) => { WebAssembly.instantiate(wasm, imports).then((i) => ok(i, wasm)); return {}; }, print: () => {}, printErr: () => {} });
m._fw_init();
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
const S = 720, H = 360, R = 32;
let t = 0;
// "<id>" = a registered screen, "spec:<screen>" = the spec screen with that built-in spec
const file = (id) => id.replace(/^spec:/, '');
function render(id, light) {
  const [comp, props] = id.startsWith('spec:') ? ['spec', { screen: file(id) }] : [id, {}];
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
  // 2 x 2 box filter to 360, rounded corners (anti-aliased alpha)
  const o = new Uint8Array(H * H * 4);
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
function png(rgba, W, Hh) {
  const crcT = new Int32Array(256).map((_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; return c; });
  const crc = (buf) => { let c = -1; for (const x of buf) c = crcT[(c ^ x) & 255] ^ (c >>> 8); return (c ^ -1) >>> 0; };
  const chunk = (type, data) => { const l = Buffer.alloc(4); l.writeUInt32BE(data.length); const td = Buffer.concat([Buffer.from(type), data]); const c = Buffer.alloc(4); c.writeUInt32BE(crc(td)); return Buffer.concat([l, td, c]); };
  const ih = Buffer.alloc(13); ih.writeUInt32BE(W, 0); ih.writeUInt32BE(Hh, 4); ih[8] = 8; ih[9] = 6;
  const raw = Buffer.alloc((W * 4 + 1) * Hh);
  for (let y = 0; y < Hh; y++) Buffer.from(rgba.buffer, rgba.byteOffset + y * W * 4, W * 4).copy(raw, y * (W * 4 + 1) + 1);
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ih), chunk('IDAT', deflateSync(raw, { level: 9 })), chunk('IEND', Buffer.alloc(0))]);
}

const ids = arg('--screens') ? arg('--screens').split(',') : screens.map((c) => c.id);
const hero = arg('--hero') ? arg('--hero').split(',') : ids.slice(0, 8);
const shots = {};
for (const theme of ['dark', 'light']) {
  for (const id of ids) {
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
