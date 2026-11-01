// visual (rule, gate tier, 1.5x, Facet theme): the pixels, per component x variant x look (moved from tools/visual.mjs
// onto the runner, plan Phase 2.3). The audit rule proves geometry; this one looks at what is drawn (after
// A batch E review: "some of [the card previews] are broken", "does the lightning radar actually work?"):
//   blank      degenerate render, inside the component's own box (its roots): (almost) no ink (pixels that differ from
//              the stage background) or ink in a sliver (error: under 16 px or 0.05 %: nothing there; review: under 2 %, or
//              the ink's own box under a tenth of it)
//   one-colour review: 97 %+ of the ink is one colour (a picture that lost its parts / colours)
//   animates   meta "motion": "<what moves>" (or a running LVGL animation at the build): the frames over 3 s must change
//              (3+ distinct frames); a declared motion that never shows is an error; meta "motion_off": "<bool prop>" =
//              variants with that prop false are still (a pinned demo)
//   reference  ported components (tests/references.json): the Astro original (tests/ref/, captured by
//              tools/ref_capture.mjs) against the render at 720 (1.5x), same box: structural similarity (SSIM on
//              luminance minus the background, 8 px windows; 0.8) and ink overlap (share of ink within 2 px of the
//              other's; 0.6). Under the thresholds = flagged for review, never a failure (the Facet port differs on
//              purpose: the grid, tokens), with both images side by side
// Errors fail; review flags and references within the thresholds are notes. finish() writes the report:
// $FW_OUT/visual/<den>/index.html (the workbench serves it), every flagged case + picture.
// FW_VISUAL_DUMP=<dir>: every render as a PNG there too.
import { readFileSync, writeFileSync, existsSync, mkdirSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
import { inflateSync } from 'node:zlib';
import { png as encodePng } from '../../tools/lib/fwbench.mjs';

const HERE = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
// PNG in without dependencies (8-bit RGBA / RGB, non-interlaced: what Chromium writes)
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
  // dirty-area overflow (more than LVGL's 32 areas in one refresh = a full-screen redraw), counted by the bench probes
  if (m._fw_bench_attach) { m._fw_bench_attach(); m._fw_bench_take(); }
  let overflows = 0;
  const frames = [grab()];
  for (let n = 0; n < ms / TICK / every; n++) {
    tick(every);
    if (m._fw_bench_take) overflows += new Float64Array(m.HEAPU8.buffer, m._fw_bench_take(), 9)[8] || 0;
    frames.push(grab());
  }
  m._fw_stage_free(id);
  return { W, H, frames, anims, box, overflows };
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

const cards = {}, counts = {};  // per density: the report's cases; errors / reviews / references within the thresholds
export default {
  id: 'visual', tiers: ['gate'], dens: ['1.5x'], ownStage: true,
  about: 'Every component and screen draws something, in more than one colour, moves when it says it does, and looks like its Astro original',
  wants: (c) => ['components', 'screens'].includes(c.section) && c.id !== 'device',
  run({ m, den, c, v, light }, rep) {
    const defaults = Object.fromEntries((c.meta.props || []).map((p) => [p.name, p.default]));
    const th = light ? 'light' : 'dark', props = { ...defaults, ...v.props };
    const list = (cards[den] ??= []), n = (counts[den] ??= { error: 0, review: 0, ok: 0 });
    const r = shoot(m, den, c, props, light, { ms: 3000, every: 6 });
    if (r.err) { n.error++; rep.check(false, `✗ ${c.id} · ${v.label} · ${th}: render: ${r.err}`); return; }
    // measured on the settled frame (after 3 s: an opening growth / draw-in has finished; the first frame of a bar chart
    // is its bars at 0, which read as blank)
    const f = r.frames[r.frames.length - 1], bx = r.box, k = ink(crop(f, r.W, bx), bx.w, bx.h, f.slice(0, 3));
    const dump = process.env.FW_VISUAL_DUMP;
    if (dump) { mkdirSync(dump, { recursive: true }); writeFileSync(join(dump, `${c.id}--${v.label.replace(/[^a-z0-9]+/gi, '-')}--${th}--${den}.png`), encodePng(f, r.W, r.H)); }
    let bad = false;
    const add = (kind, msg, img = f, extra = null) => {
      list.push({ c: c.id, v: v.label, th, kind, msg, img, W: r.W, H: r.H, extra });
      if (kind === 'error') { n.error++; bad = true; rep.check(false, `✗ ${c.id} · ${v.label} · ${th}: ${msg}`); }
      else { n.review++; rep.note(`? ${c.id} · ${v.label} · ${th}: ${msg}`); }
    };
    if (k.n < 16 || k.share < 0.0005) add('error', `blank: ${(k.share * 100).toFixed(2)} % ink in its ${bx.w}x${bx.h} box`);
    else if (k.share < 0.02 || k.box < 0.1) add('review', `little ink: ${(k.share * 100).toFixed(1)} % of its ${bx.w}x${bx.h} box, the ink's own box ${(k.box * 100).toFixed(0)} % of it`);
    if (k.n && k.one > 0.97 && k.n > 64) add('review', `one colour: ${(k.one * 100).toFixed(0)} % of the ink is one colour`);
    if (r.overflows > 0) add('review', `dirty areas: ${r.overflows} refresh${r.overflows > 1 ? 'es' : ''} over LVGL's 32 areas in 3 s (each one a full-screen redraw on the panel)`);
    const distinct = new Set(r.frames.map(hash)).size;
    // a declared motion can be switched off per variant by its meta "motion_off" prop (a pinned time: demo false)
    const declared = c.meta.motion && !(c.meta.motion_off && props[c.meta.motion_off] === false);
    if (declared && distinct < 3) add('error', `animates: meta motion "${c.meta.motion}" but ${distinct} distinct frame${distinct > 1 ? 's' : ''} in 3 s`);
    else if (!declared && r.anims > 0 && distinct < 2) {  // slow loops (a mascot blinks every ~4.5 s): look for 10 s
      const r2 = shoot(m, den, c, props, light, { ms: 10000, every: 6 });
      if (!r2.err && new Set(r2.frames.map(hash)).size < 2) add('review', `animates: ${r.anims} animation(s) running at the build, but no pixel changes in 10 s`);
    }
    if (!bad) rep.check(true, `${c.id} · ${v.label} · ${th}`);
    // reference
    const ref = REFS.find((x) => x.comp === c.id && x.variant === v.label);
    if (ref && den === '1.5x') {
      const file = join(refDir, `${c.id}--${v.label}--${th}.png`);
      if (!existsSync(file)) { add('review', `reference: no capture (run tools/ref_capture.mjs)`); return; }
      let A = fit(decodePng(readFileSync(file)), r.W, r.H), B = f, CW = r.W, CH = r.H;
      if (ref.crop) {  // both cropped to their ink box, the docs one scaled to the WASM one
        const dec = decodePng(readFileSync(file)), ka = inkBox(dec.px, dec.W, dec.H), kb = inkBox(f, r.W, r.H);
        if (ka && kb) { CW = kb.w; CH = kb.h; B = crop(f, r.W, kb); A = fit({ W: ka.w, H: ka.h, px: crop(dec.px, dec.W, ka) }, CW, CH); }
      }
      const s = ssim(lum(A, CW, CH), lum(B, CW, CH), CW, CH), o = iou(A, B, CW, CH);
      const ok = s >= (ref.ssim ?? 0.8) && o >= (ref.iou ?? 0.6), msg = `reference: SSIM ${s.toFixed(2)}, ink overlap ${o.toFixed(2)}`;
      if (ok) { n.ok++; rep.note(`= ${c.id} · ${v.label} · ${th}: ${msg}`); } else { n.review++; rep.note(`? ${c.id} · ${v.label} · ${th}: ${msg}`); }
      list.push({ c: c.id, v: v.label, th, kind: ok ? 'ref-ok' : 'review', msg: `${msg}${ref.note ? ` (${ref.note})` : ''}`, img: B, W: CW, H: CH, extra: A });
    }
  },
  // the report page: every flagged case (and every reference pair) with its pictures
  finish({ build }) {
    const esc = (s) => String(s).replace(/[&<>"]/g, (x) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[x]);
    for (const den of Object.keys(cards)) {
      const dir = join(build, 'visual', den), n = counts[den];
      mkdirSync(dir, { recursive: true });
      let html = `<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>Visual checks</title><style>
:root{--bg:#0e1013;--fg:#e8e6e1;--dim:#8a919b;--line:#2a2e35;--bad:#e8542a;--rev:#e8b33a;--ok:#5fb37a}
@media (prefers-color-scheme:light){:root{--bg:#f6f4f0;--fg:#16181c;--dim:#5c636d;--line:#dcd8d0;--bad:#b63f1a;--rev:#926600;--ok:#2f7a45}}
body{background:var(--bg);color:var(--fg);font:14px/1.4 system-ui;margin:0;padding:16px}h1{font-size:20px}
.c{display:inline-block;vertical-align:top;margin:0 12px 16px 0;padding:8px;border:1px solid var(--line);border-radius:8px;max-width:100%}
.c img{display:block;max-width:100%;image-rendering:pixelated}.k{font-weight:600}.error .k{color:var(--bad)}.review .k{color:var(--rev)}.ref-ok .k{color:var(--ok)}
.p{display:flex;gap:8px;flex-wrap:wrap}.p span{font-size:12px;color:var(--dim)}</style>
<h1>Visual checks · ${den} · ${new Date().toISOString().slice(0, 16).replace('T', ' ')}</h1><p>${n.error} error(s), ${n.review} for review, ${n.ok} reference(s) within the thresholds. Reference pairs: Astro original left, WASM right.</p>`;
      let i = 0;
      for (const k of cards[den]) {
        const a = `v${i++}.png`;
        writeFileSync(join(dir, a), encodePng(k.img, k.W, k.H));
        let ref = '';
        if (k.extra) { const b = `v${i++}.png`; writeFileSync(join(dir, b), encodePng(k.extra, k.W, k.H)); ref = `<div><span>Astro</span><img src="${b}" width="${k.W}"></div>`; }
        html += `<div class="c ${k.kind}"><div><span class="k">${esc(k.kind)}</span> ${esc(k.c)} · ${esc(k.v)} · ${k.th}</div><div>${esc(k.msg)}</div><div class="p">${ref}<div><span>WASM</span><img src="${a}" width="${k.W}"></div></div></div>`;
      }
      writeFileSync(join(dir, 'index.html'), html);
      console.log(`visual [${den}]: ${n.error} error(s), ${n.review} for review; report ${join(dir, 'index.html')}`);
    }
  },
};
