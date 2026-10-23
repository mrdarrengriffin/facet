#!/usr/bin/env node
// Visual checks (2026-10-22, after Darren's batch E review: "some of [the card previews] are broken", "does the lightning
// radar actually work in terms of animations?"). The audit (tools/audit.mjs) proves geometry: nothing clipped, on the
// grid, no overlap, symmetric marks, motion paths, update animations. It cannot tell whether a component LOOKS like it
// should or whether its motion is visible. These checks look at the pixels, per component x variant x theme, one density:
//   blank      degenerate render, inside the component's own box (its roots): (almost) no ink (pixels that differ from
//              the stage background) or ink in a sliver (error: under 16 px or 0.05 %: nothing there; review: under 2 %, or
//              the ink's own box under a tenth of it)
//   one-colour review: 97 %+ of the ink is one colour (a picture that lost its parts / colours)
//   animates   meta "motion": "<what moves>" (or a running LVGL animation at the build): the frames over 3 s must change
//              (3+ distinct frames); a declared motion that never shows is an error; meta "motion_off": "<bool prop>" =
//              variants with that prop false are still (a pinned demo)
//   reference  ported components (tests/references.json): the Astro original (docs explorer, captured headless with
//              --capture into tests/ref/, kept in the repo) against the WASM render at 720 (1.5x), same box: structural similarity
//              (SSIM on luminance minus the background, 8 px windows; 0.8) and ink overlap (share of ink within 2 px of the other's; 0.6). Under the thresholds = flagged for review, never
//              a failure (the Facet port differs on purpose: the grid, tokens), with both images side by side
// Report: $FW_OUT/visual/<den>/index.html (the hub serves it at /facet-wasm/visual/1.5x/), every flagged case + picture.
//   node tools/visual.mjs [--den 1x|1.5x] [--comp id,id] [--capture] [--json out.json] [--dump dir: every render as PNG]
// Exit 1 on an error (blank, a declared motion that does not show); review flags only print.
import { readFileSync, writeFileSync, existsSync, mkdirSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { createHash } from 'node:crypto';

const HERE = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const env = {};
const envFile = join(HERE, '..', 'local.env');
if (existsSync(envFile)) for (const l of readFileSync(envFile, 'utf8').split('\n')) { const m = l.match(/^(\w+)=(.*)$/); if (m) env[m[1]] = m[2]; }
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const flag = (k) => process.argv.includes(k);
const OUT = process.env.FW_OUT || env.FW_OUT, CACHE = process.env.FW_CACHE || env.FW_CACHE;
const BUILD = arg('--build', OUT);
const den = arg('--den', '1.5x');
const only = arg('--comp', '') ? arg('--comp').split(',') : null;
// PNG in / out without dependencies (8-bit RGBA / RGB, non-interlaced: what Chromium writes)
import { deflateSync, inflateSync } from 'node:zlib';
const CRC = new Int32Array(256).map((_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; return c; });
const crc = (b) => { let c = -1; for (const x of b) c = CRC[(c ^ x) & 255] ^ (c >>> 8); return (c ^ -1) >>> 0; };
function encodePng(rgba, W, H) {
  const raw = Buffer.alloc((W * 4 + 1) * H);
  for (let y = 0; y < H; y++) { raw[y * (W * 4 + 1)] = 0; Buffer.from(rgba.buffer, rgba.byteOffset + y * W * 4, W * 4).copy(raw, y * (W * 4 + 1) + 1); }
  const chunk = (t, d) => { const l = Buffer.alloc(4); l.writeUInt32BE(d.length); const td = Buffer.concat([Buffer.from(t), d]); const c = Buffer.alloc(4); c.writeUInt32BE(crc(td)); return Buffer.concat([l, td, c]); };
  const ih = Buffer.alloc(13); ih.writeUInt32BE(W, 0); ih.writeUInt32BE(H, 4); ih[8] = 8; ih[9] = 6;
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ih), chunk('IDAT', deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
}
function decodePng(buf) {
  let p = 8, W = 0, H = 0, ct = 0; const idat = [];
  while (p < buf.length) { const l = buf.readUInt32BE(p), t = buf.toString('ascii', p + 4, p + 8), d = buf.subarray(p + 8, p + 8 + l); if (t === 'IHDR') { W = d.readUInt32BE(0); H = d.readUInt32BE(4); ct = d[9]; } else if (t === 'IDAT') idat.push(d); p += 12 + l; }
  const bpp = ct === 6 ? 4 : 3, raw = inflateSync(Buffer.concat(idat)), out = new Uint8Array(W * H * 4), line = W * bpp, prev = new Uint8Array(line), cur = new Uint8Array(line);
  for (let y = 0; y < H; y++) {
    const f = raw[y * (line + 1)], s = raw.subarray(y * (line + 1) + 1, (y + 1) * (line + 1));
    for (let i = 0; i < line; i++) { const a = i >= bpp ? cur[i - bpp] : 0, b = prev[i], c = i >= bpp ? prev[i - bpp] : 0; let v = s[i];
      if (f === 1) v += a; else if (f === 2) v += b; else if (f === 3) v += (a + b) >> 1; else if (f === 4) { const pp = a + b - c, pa = Math.abs(pp - a), pb = Math.abs(pp - b), pc = Math.abs(pp - c); v += pa <= pb && pa <= pc ? a : pb <= pc ? b : c; }
      cur[i] = v & 255; }
    for (let x = 0; x < W; x++) { out[(y * W + x) * 4] = cur[x * bpp]; out[(y * W + x) * 4 + 1] = cur[x * bpp + 1]; out[(y * W + x) * 4 + 2] = cur[x * bpp + 2]; out[(y * W + x) * 4 + 3] = 255; }
    prev.set(cur);
  }
  return { W, H, px: out };
}
// nearest-neighbour resize to W x H (references are captured at the same 720 size, so this is normally a copy)
function fit(img, W, H) {
  if (img.W === W && img.H === H) return img.px;
  const o = new Uint8Array(W * H * 4);
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) { const sx = Math.floor((x * img.W) / W), sy = Math.floor((y * img.H) / H); o.set(img.px.subarray((sy * img.W + sx) * 4, (sy * img.W + sx) * 4 + 4), (y * W + x) * 4); }
  return o;
}
const TICK = 16;

async function load(d) {
  const dir = join(BUILD, d);
  const F = (await import(pathToFileURL(join(dir, 'fw.mjs')).href)).default;
  const wasm = await WebAssembly.compile(readFileSync(join(dir, 'fw.wasm')));
  const m = await F({ instantiateWasm: (imports, ok) => { WebAssembly.instantiate(wasm, imports).then((i) => ok(i, wasm)); return {}; } });
  m._fw_init();
  return m;
}
const str = (m, s, f) => { const p = m.stringToNewUTF8(s); try { return f(p); } finally { m._free(p); } };

// one render: RGBA frames of the stage at the build, then every `every` ticks for `ms`
export function shoot(m, d, c, props, light, { ms = 0, every = 2 } = {}) {
  const D = d === '1x' ? 1 : 1.5, S = d === '1x' ? 480 : 720;
  const screen = c.section === 'screens';
  const [fw, fh] = c.meta.frame || [S / D, S / D];
  let W = screen ? S : Math.round(fw * D), H = screen ? S : (fh ? Math.round(fh * D) : S);
  const id = m._fw_stage(W, H);
  let t = Math.round(m.__t || 0);
  const tick = (n = 1) => { for (let i = 0; i < n; i++) { t += TICK; m._fw_tick(t); } m.__t = t; m._fw_events(); };
  tick(2);
  const e = str(m, c.id, (cp) => str(m, JSON.stringify(props), (pp) => m.UTF8ToString(m._fw_render(id, cp, pp, light ? 1 : 0, 0))));
  if (e) { m._fw_stage_free(id); return { err: e }; }
  if (!fh && !screen) { const ch = m._fw_content_h(id); if (ch > H) { H = ch; m._fw_stage_resize(id, W, H); } }
  tick(2);
  const anims = m._fw_anims();
  // the component's own box (its roots' union): ink shares are measured inside it, not the stage
  const roots = JSON.parse(m.UTF8ToString(m._fw_audit(id))).filter((o) => o.p < 0 && o.w > 0 && o.h > 0);
  const box = roots.length ? { x: Math.max(0, Math.min(...roots.map((o) => o.x))), y: Math.max(0, Math.min(...roots.map((o) => o.y))) } : { x: 0, y: 0 };
  if (roots.length) { box.w = Math.min(W, Math.max(...roots.map((o) => o.x + o.w))) - box.x; box.h = Math.min(H, Math.max(...roots.map((o) => o.y + o.h))) - box.y; } else { box.w = W; box.h = H; }
  const grab = () => {
    m._fw_refresh(id);
    const p = m._fw_frame(id), b = m.HEAPU8.slice(p, p + W * H * 4);
    for (let i = 3; i < b.length; i += 4) b[i] = 255;  // the stage is RGBA in memory (fw_frame); opaque
    return b;
  };
  const frames = [grab()];
  for (let n = 0; n < ms / TICK / every; n++) { tick(every); frames.push(grab()); }
  m._fw_stage_free(id);
  return { W, H, frames, anims, box };
}
const hash = (b) => createHash('sha1').update(b).digest('hex').slice(0, 12);

// ---- pixel measures
function ink(b, W, H, bg0 = null) {
  const bg = bg0 ? [bg0[0], bg0[1], bg0[2]] : [b[0], b[1], b[2]];  // the stage background (its corner) unless given
  let n = 0, x0 = W, y0 = H, x1 = -1, y1 = -1;
  const cols = new Map();
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
    const i = (y * W + x) * 4;
    if (Math.abs(b[i] - bg[0]) + Math.abs(b[i + 1] - bg[1]) + Math.abs(b[i + 2] - bg[2]) < 24) continue;
    n++; x0 = Math.min(x0, x); y0 = Math.min(y0, y); x1 = Math.max(x1, x); y1 = Math.max(y1, y);
    const k = ((b[i] >> 4) << 8) | ((b[i + 1] >> 4) << 4) | (b[i + 2] >> 4);
    cols.set(k, (cols.get(k) || 0) + 1);
  }
  const top = Math.max(0, ...cols.values());
  return { n, share: n / (W * H), box: x1 < 0 ? 0 : ((x1 - x0 + 1) * (y1 - y0 + 1)) / (W * H), one: n ? top / n : 0, bg };
}
// luminance with the background (corner pixel) taken away, so a different canvas colour (the docs card surface vs the
// stage bg) does not count as a difference: only the drawn shapes do
function lum(b, W, H) { const L = new Float32Array(W * H), bg = 0.299 * b[0] + 0.587 * b[1] + 0.114 * b[2]; for (let i = 0; i < W * H; i++) L[i] = Math.abs(0.299 * b[i * 4] + 0.587 * b[i * 4 + 1] + 0.114 * b[i * 4 + 2] - bg); return L; }
function ssim(a, b, W, H) {
  const C1 = (0.01 * 255) ** 2, C2 = (0.03 * 255) ** 2, w = 8;
  let s = 0, n = 0;
  for (let y = 0; y + w <= H; y += 4) for (let x = 0; x + w <= W; x += 4) {
    let ma = 0, mb = 0;
    for (let j = 0; j < w; j++) for (let i = 0; i < w; i++) { const k = (y + j) * W + x + i; ma += a[k]; mb += b[k]; }
    ma /= w * w; mb /= w * w;
    let va = 0, vb = 0, cv = 0;
    for (let j = 0; j < w; j++) for (let i = 0; i < w; i++) { const k = (y + j) * W + x + i; va += (a[k] - ma) ** 2; vb += (b[k] - mb) ** 2; cv += (a[k] - ma) * (b[k] - mb); }
    va /= w * w - 1; vb /= w * w - 1; cv /= w * w - 1;
    s += ((2 * ma * mb + C1) * (2 * cv + C2)) / ((ma * ma + mb * mb + C1) * (va + vb + C2)); n++;
  }
  return n ? s / n : 1;
}
// ink overlap with a 2 px tolerance (each image's ink grown by 2 px before the overlap is taken): thin strokes a pixel
// apart still count as the same mark
function iou(a, b, W, H) {
  const mask = (p) => { const bg = [p[0], p[1], p[2]], m = new Uint8Array(W * H); for (let i = 0; i < W * H; i++) m[i] = Math.abs(p[i * 4] - bg[0]) + Math.abs(p[i * 4 + 1] - bg[1]) + Math.abs(p[i * 4 + 2] - bg[2]) >= 24 ? 1 : 0; return m; };
  const grow = (m, r = 2) => { const o = new Uint8Array(W * H); for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) { if (!m[y * W + x]) continue; for (let j = Math.max(0, y - r); j <= Math.min(H - 1, y + r); j++) for (let i = Math.max(0, x - r); i <= Math.min(W - 1, x + r); i++) o[j * W + i] = 1; } return o; };
  const A = mask(a), B = mask(b), GA = grow(A), GB = grow(B);
  let hitA = 0, nA = 0, hitB = 0, nB = 0;
  for (let i = 0; i < W * H; i++) { if (A[i]) { nA++; if (GB[i]) hitA++; } if (B[i]) { nB++; if (GA[i]) hitB++; } }
  return nA + nB ? (hitA + hitB) / (nA + nB) : 1;  // share of each image's ink that lies within 2 px of the other's
}
const png = async (b, W, H) => encodePng(b, W, H);
function inkBox(b, W, H) {
  const k = ink(b, W, H); if (!k.n) return null;
  let x0 = W, y0 = H, x1 = -1, y1 = -1;
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) { const i = (y * W + x) * 4; if (Math.abs(b[i] - k.bg[0]) + Math.abs(b[i + 1] - k.bg[1]) + Math.abs(b[i + 2] - k.bg[2]) < 24) continue; x0 = Math.min(x0, x); y0 = Math.min(y0, y); x1 = Math.max(x1, x); y1 = Math.max(y1, y); }
  return { x: x0, y: y0, w: x1 - x0 + 1, h: y1 - y0 + 1 };
}
function crop(b, W, r) { const o = new Uint8Array(r.w * r.h * 4); for (let y = 0; y < r.h; y++) o.set(b.subarray(((r.y + y) * W + r.x) * 4, ((r.y + y) * W + r.x + r.w) * 4), y * r.w * 4); return o; }

// ---- references (tests/references.json): [{comp, variant, astro: {item, index, theme?}, note?}]
const refsFile = join(HERE, 'tests/references.json');
const REFS = existsSync(refsFile) ? JSON.parse(readFileSync(refsFile, 'utf8')).refs : [];
const refDir = join(HERE, 'tests/ref');  // the Astro originals, kept in the repo
async function capture() {
  const { chromium } = await import('/home/claude/claude-assistant/apps/hub/node_modules/playwright/index.mjs');
  const DOCS = process.env.FW_DOCS_URL || 'http://127.0.0.1:4399/facet';
  mkdirSync(refDir, { recursive: true });
  const b = await chromium.launch({ executablePath: '/usr/bin/chromium' });
  const p = await b.newPage({ viewport: { width: 1600, height: 1200 } });
  const items = [...new Set(REFS.filter((r) => r.astro.item).map((r) => r.astro.item))];
  for (const item of items) {
    await p.goto(`${DOCS}/components/#${item}`, { waitUntil: 'load' });
    await p.waitForTimeout(2500);
    for (const th of ['dark', 'light']) {
      await p.evaluate((th) => document.documentElement.setAttribute('data-facet-theme', th), th);
      await p.waitForTimeout(600);
      for (const r of REFS.filter((x) => x.astro.item === item)) {
        const els = await p.$$(r.astro.selector || '.explorer__preview .ex-prev');
        if (r.astro.selector) await p.waitForTimeout(400);
        const el = els[r.astro.index ?? 0];
        if (el) await el.screenshot({ path: join(refDir, `${r.comp}--${r.variant}--${th}.png`), animations: 'disabled' });
        else console.log(`capture: ${item} has no element ${r.astro.index ?? 0} for ${r.comp} ${r.variant}`);
      }
    }
  }
  await b.close();
}

// ---------------------------------------------------------------- run
if (flag('--capture')) await capture();
const m = await load(den);
const meta = JSON.parse(m.UTF8ToString(m._fw_meta()));
const report = [], cards = [];
let errors = 0;
const rep = join(OUT, 'visual', den);  // one report per density
mkdirSync(rep, { recursive: true });
for (const c of meta) {
  if (!['components', 'screens'].includes(c.section) || c.id === 'device') continue;
  if (only && !only.includes(c.id)) continue;
  const defaults = Object.fromEntries((c.meta.props || []).map((p) => [p.name, p.default]));
  const cases = [{ label: 'default', props: {} }, ...(c.meta.variants || [])];
  for (const v of cases) for (const light of [false, true]) {
    const th = light ? 'light' : 'dark', props = { ...defaults, ...v.props };
    const motion = c.meta.motion || (v.props && v.props.live === false ? null : null);
    const r = shoot(m, den, c, props, light, { ms: 3000, every: 6 });
    if (r.err) { report.push({ c: c.id, v: v.label, th, kind: 'error', msg: `render: ${r.err}` }); errors++; continue; }
    // measured on the settled frame (after 3 s: an opening growth / draw-in has finished; the first frame of a bar chart
    // is its bars at 0, which read as blank)
    const f = r.frames[r.frames.length - 1], bx = r.box, k = ink(crop(f, r.W, bx), bx.w, bx.h, f.slice(0, 3));
    if (arg('--dump')) { mkdirSync(arg('--dump'), { recursive: true }); writeFileSync(join(arg('--dump'), `${c.id}--${v.label.replace(/[^a-z0-9]+/gi, '-')}--${th}--${den}.png`), encodePng(f, r.W, r.H)); }
    const add = (kind, msg, img = f, extra = null) => { report.push({ c: c.id, v: v.label, th, kind, msg }); cards.push({ c: c.id, v: v.label, th, kind, msg, img, W: r.W, H: r.H, extra }); if (kind === 'error') errors++; };
    if (k.n < 16 || k.share < 0.0005) add('error', `blank: ${(k.share * 100).toFixed(2)} % ink in its ${bx.w}x${bx.h} box`);
    else if (k.share < 0.02 || k.box < 0.1) add('review', `little ink: ${(k.share * 100).toFixed(1)} % of its ${bx.w}x${bx.h} box, the ink's own box ${(k.box * 100).toFixed(0)} % of it`);
    if (k.n && k.one > 0.97 && k.n > 64) add('review', `one colour: ${(k.one * 100).toFixed(0)} % of the ink is one colour`);
    const distinct = new Set(r.frames.map(hash)).size;
    // a declared motion can be switched off per variant by its meta "motion_off" prop (a pinned time: demo false)
    const declared = c.meta.motion && !(c.meta.motion_off && props[c.meta.motion_off] === false);
    if (declared && distinct < 3) add('error', `animates: meta motion "${c.meta.motion}" but ${distinct} distinct frame${distinct > 1 ? 's' : ''} in 3 s`);
    else if (!declared && r.anims > 0 && distinct < 2) {  // slow loops (a mascot blinks every ~4.5 s): look for 10 s
      const r2 = shoot(m, den, c, props, light, { ms: 10000, every: 6 });
      if (!r2.err && new Set(r2.frames.map(hash)).size < 2) add('review', `animates: ${r.anims} animation(s) running at the build, but no pixel changes in 10 s`);
    }
    // reference
    const ref = REFS.find((x) => x.comp === c.id && x.variant === v.label);
    if (ref && den === '1.5x') {
      const file = join(refDir, `${c.id}--${v.label}--${th}.png`);
      if (!existsSync(file)) { add('review', `reference: no capture (run with --capture)`); continue; }
      let A = fit(decodePng(readFileSync(file)), r.W, r.H), B = f, CW = r.W, CH = r.H;
      if (ref.crop) {  // both cropped to their ink box, the docs one scaled to the WASM one
        const dec = decodePng(readFileSync(file)), ka = inkBox(dec.px, dec.W, dec.H), kb = inkBox(f, r.W, r.H);
        if (ka && kb) { CW = kb.w; CH = kb.h; B = crop(f, r.W, kb); A = fit({ W: ka.w, H: ka.h, px: crop(dec.px, dec.W, ka) }, CW, CH); }
      }
      const s = ssim(lum(A, CW, CH), lum(B, CW, CH), CW, CH), o = iou(A, B, CW, CH);
      const ok = s >= (ref.ssim ?? 0.8) && o >= (ref.iou ?? 0.6);
      report.push({ c: c.id, v: v.label, th, kind: ok ? 'ref-ok' : 'review', msg: `reference: SSIM ${s.toFixed(2)}, ink overlap ${o.toFixed(2)}` });
      cards.push({ c: c.id, v: v.label, th, kind: ok ? 'ref-ok' : 'review', msg: `reference: SSIM ${s.toFixed(2)}, ink overlap ${o.toFixed(2)}${ref.note ? ` (${ref.note})` : ''}`, img: B, W: CW, H: CH, extra: A });
    }
  }
}
// ---- report page: every flagged case (and every reference pair) with its pictures
const esc = (s) => String(s).replace(/[&<>"]/g, (x) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[x]);
let html = `<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>Visual checks</title><style>
:root{--bg:#0e1013;--fg:#e8e6e1;--dim:#8a919b;--line:#2a2e35;--bad:#e8542a;--rev:#e8b33a;--ok:#5fb37a}
@media (prefers-color-scheme:light){:root{--bg:#f6f4f0;--fg:#16181c;--dim:#5c636d;--line:#dcd8d0;--bad:#b63f1a;--rev:#926600;--ok:#2f7a45}}
body{background:var(--bg);color:var(--fg);font:14px/1.4 system-ui;margin:0;padding:16px}h1{font-size:20px}
.c{display:inline-block;vertical-align:top;margin:0 12px 16px 0;padding:8px;border:1px solid var(--line);border-radius:8px;max-width:100%}
.c img{display:block;max-width:100%;image-rendering:pixelated}.k{font-weight:600}.error .k{color:var(--bad)}.review .k{color:var(--rev)}.ref-ok .k{color:var(--ok)}
.p{display:flex;gap:8px;flex-wrap:wrap}.p span{font-size:12px;color:var(--dim)}</style>
<h1>Visual checks · ${den} · ${new Date().toISOString().slice(0, 16).replace('T', ' ')}</h1><p>${errors} error(s), ${report.filter((r) => r.kind === 'review').length} for review, ${report.filter((r) => r.kind === 'ref-ok').length} reference(s) within the thresholds. Reference pairs: Astro original left, WASM right.</p>`;
let n = 0;
for (const k of cards) {
  const a = `v${n++}.png`;
  writeFileSync(join(rep, a), await png(k.img, k.W, k.H));
  let ref = '';
  if (k.extra) { const b = `v${n++}.png`; writeFileSync(join(rep, b), await png(k.extra, k.W, k.H)); ref = `<div><span>Astro</span><img src="${b}" width="${k.W}"></div>`; }
  html += `<div class="c ${k.kind}"><div><span class="k">${esc(k.kind)}</span> ${esc(k.c)} · ${esc(k.v)} · ${k.th}</div><div>${esc(k.msg)}</div><div class="p">${ref}<div><span>WASM</span><img src="${a}" width="${k.W}"></div></div></div>`;
}
writeFileSync(join(rep, 'index.html'), html);
for (const r of report.filter((x) => x.kind !== 'ref-ok')) console.log(`${r.kind === 'error' ? '✗' : '?'} ${r.c} · ${r.v} · ${r.th}: ${r.msg}`);
for (const r of report.filter((x) => x.kind === 'ref-ok')) console.log(`= ${r.c} · ${r.v} · ${r.th}: ${r.msg}`);
console.log(`visual [${den}]: ${errors} error(s), ${report.filter((r) => r.kind === 'review').length} for review; report ${join(rep, 'index.html')}`);
if (flag('--json')) writeFileSync(arg('--json'), JSON.stringify(report, null, 1));
process.exit(errors ? 1 : 0);
