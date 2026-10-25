#!/usr/bin/env node
// Spec corpus test (2026-10-25): renders many screen specs on the WASM build and checks each one in code, no image
// review. Corpus = the hand-written specs in tests/spec-corpus/specs/*.json + schema-driven specs (tests/spec-corpus/
// gen.mjs: every catalogue component / prop / option at least once) + any --extra dir of real specs (decks there are
// validated only). Per spec x density x theme (dark, light), after 1 s of ticks:
//   valid     the panel's validation (fw_spec check / screen) or a build error
//   budget    bytes / nodes / binds over the catalogue budget
//   outside   something drawn off the screen (audit_screens escape, clipped by its ancestors)
//   overlap   two regions, or two sibling component boxes, intersect (audit_screens overlap + fw_comps)
//   text      a label cut at the bottom, wider than its box (clipped or ellipsised) or holding an ellipsis
//   spacing   list layout: a non-row node (hero_block, gauge, ...) followed by the next node closer than the group gap
//   grid      a drawn box off the 4 px grid in its parent (the component audit's rule)
//   contrast  a label's ink against its background in the rendered pixels (565, as drawn): < 4.5 (< 3 large text)
//   leak      text on the screen that is not in the spec (a demo / sample default showing through)
//   blank     a generated spec that draws nothing in the content box
// tests/spec-corpus/allow.json: UI vocabulary a component draws itself, components showing their own feed / sample
// data (no leak check inside), generated specs allowed to be blank.
// Skipped: docs-site previews (an Astro build, not the WASM; "blank" covers a component that draws nothing).
// Usage: node tools/corpus.mjs [--den 1x|1.5x|all] [--gen-den 1x|1.5x|all] [--no-gen] [--only text] [--extra dir]
//        [--json out.json] [--out dir] [--all-png] [-v]   -> table on stdout, PNGs of failures only in tests/spec-corpus/out/
//        (--out: another PNG dir; --all-png: every render, both themes), exit 1 on fail. JSON: per render {file, comp, tag,
//        den, fails: [{kind, msg, theme, box: [{x, y, w, h}]}], png: {dark, light}} (box = the regions the check found)
import { readFileSync, writeFileSync, readdirSync, mkdirSync, rmSync, existsSync } from 'node:fs';
import { dirname, join, resolve, basename } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { deflateSync } from 'node:zlib';
import { checkScreen } from './audit_screens.mjs';
import { generate, optionCount } from '../tests/spec-corpus/gen.mjs';

const R = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const CORPUS = join(R, 'tests', 'spec-corpus');
const argv = process.argv;
const arg = (k, d) => { const i = argv.indexOf(k); return i > 0 ? argv[i + 1] : d; };
const OUT = resolve(arg('--out', join(CORPUS, 'out'))), ALLPNG = argv.includes('--all-png');
const args = (k) => argv.flatMap((a, i) => (a === k ? [argv[i + 1]] : []));
const env = (() => { try { return Object.fromEntries(readFileSync(join(R, '..', 'local.env'), 'utf8').split('\n').map((l) => l.match(/^([A-Z_]+)=(.*)$/)).filter(Boolean).map((m) => [m[1], m[2]])); } catch { return {}; } })();
const BUILD = process.env.FW_OUT || env.FW_OUT || join(R, 'out');
const dens = (v) => (v === 'all' ? ['1x', '1.5x'] : [v]);
const DEN = dens(arg('--den', 'all')), GDEN = dens(arg('--gen-den', '1x')), VERB = argv.includes('-v'), ONLY = arg('--only');
const cat = JSON.parse(readFileSync(join(R, 'catalogue', 'catalogue.json'), 'utf8'));
const layoutTok = JSON.parse(readFileSync(join(R, 'tokens', 'layout.json'), 'utf8'));
const findTok = (o, k) => { if (!o || typeof o !== 'object') return null; if (o[k] && o[k]['1x'] !== undefined) return o[k]; for (const v of Object.values(o)) { const r = findTok(v, k); if (r) return r; } return null; };
const GROUP = findTok(layoutTok, 'group'), TOP = findTok(layoutTok, 'top');
const ALLOW = JSON.parse(readFileSync(join(CORPUS, 'allow.json'), 'utf8'));
const allowed = (who, t) => { const l = t.toLowerCase().trim(); return [...(ALLOW.exact['*'] || []), ...(ALLOW.exact[who] || [])].includes(l) || (ALLOW.pattern[who] && new RegExp(ALLOW.pattern[who]).test(t.trim())); };
const tok = (t, den) => (den === '1x' ? t['1x'] : t.v);

// ---- corpus
const corpus = [];
const addDir = (dir, tag) => {
  for (const f of readdirSync(dir).filter((f) => f.endsWith('.json')).sort()) {
    const text = readFileSync(join(dir, f), 'utf8');
    let spec; try { spec = JSON.parse(text); } catch { spec = null; }
    corpus.push({ file: `${tag}/${f}`, spec, text, deck: !!(spec && (spec.pages || spec.screens)) });
  }
};
addDir(join(CORPUS, 'specs'), 'hand');
for (const d of args('--extra')) if (existsSync(d)) addDir(d, basename(d));
if (!argv.includes('--no-gen')) corpus.push(...generate(cat));
const todo = corpus.filter((c) => !ONLY || c.file.includes(ONLY));

// ---- helpers
const lin = (v) => { const s = v / 255; return s <= 0.03928 ? s / 12.92 : ((s + 0.055) / 1.055) ** 2.4; };
const lum = (r, g, b) => 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b);
const crc = (() => { const t = new Int32Array(256); for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; t[n] = c; } return (b) => { let c = -1; for (const x of b) c = t[(c ^ x) & 255] ^ (c >>> 8); return (c ^ -1) >>> 0; }; })();
function png(px, W, H) {
  const raw = Buffer.alloc((W * 4 + 1) * H);
  for (let y = 0; y < H; y++) { raw[y * (W * 4 + 1)] = 0; Buffer.from(px.buffer, px.byteOffset + y * W * 4, W * 4).copy(raw, y * (W * 4 + 1) + 1); }
  const chunk = (type, data) => { const l = Buffer.alloc(4); l.writeUInt32BE(data.length); const td = Buffer.concat([Buffer.from(type), data]); const c = Buffer.alloc(4); c.writeUInt32BE(crc(td)); return Buffer.concat([l, td, c]); };
  const ih = Buffer.alloc(13); ih.writeUInt32BE(W, 0); ih.writeUInt32BE(H, 4); ih[8] = 8; ih[9] = 6;
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ih), chunk('IDAT', deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
}
const strings = (v, acc = []) => { if (typeof v === 'string' || typeof v === 'number') acc.push(String(v)); else if (v && typeof v === 'object') for (const x of Object.values(v)) strings(x, acc); return acc; };
const ROW = (c) => /_row$/.test(c) || c === 'section_header';
const CHROME = new Set(['header', 'screen_chrome', 'back_button', 'toast_layer', 'bottom_sheet']);
const inside = (o, b) => o.x >= b.x && o.y >= b.y && o.x + o.w <= b.x + b.w && o.y + o.h <= b.y + b.h;
const meets = (a, b) => Math.min(a.x + a.w, b.x + b.w) > Math.max(a.x, b.x) && Math.min(a.y + a.h, b.y + b.h) > Math.max(a.y, b.y);

function gridErrs(os) {  // tools/audit.mjs "grid", same allowances
  const errs = [];
  for (const o of os) {
    if (o.t === 'label' || o.t === 'image' || o.t === 'line' || o.free || o.w <= 0 || o.h <= 0) continue;
    if (!o.box && o.t !== 'arc' && o.p >= 0) continue;
    const P = o.p >= 0 ? os[o.p] : null;
    if (!P) continue;
    const axis = (pos, size, ppos, psize, pb) => {
      if (size <= 2) return [];
      const d = pos - ppos, bad = [];
      const centred = Math.abs(d - (psize - d - size)) <= 1, inset = pb > 0 && d === pb && size === psize - 2 * pb;
      const bleedOwn = o.bw > 0 && o.bw <= 2 && d === -o.bw && size === psize + 2 * o.bw;
      if (d % 4 && !centred && !inset && !bleedOwn) bad.push(d);
      if (size % 4 && !inset && !bleedOwn) bad.push(`size ${size}`);
      return bad;
    };
    const bx = axis(o.x, o.w, P.x, P.w, P.bw), by = axis(o.y, o.h, P.y, P.h, P.bw);
    if (bx.length || by.length) errs.push({ b: o, m: `${o.t} ${o.w}x${o.h} at ${o.x - P.x},${o.y - P.y} off the 4 px grid (${[...bx.map((v) => `x ${v}`), ...by.map((v) => `y ${v}`)].join(', ')})` });
  }
  return errs;
}

// ---- WASM
const mods = {};
async function mod(den) {
  if (mods[den]) return mods[den];
  const dir = join(BUILD, den);
  const F = (await import(pathToFileURL(join(dir, 'fw.mjs')).href)).default;
  const wasm = await WebAssembly.compile(readFileSync(join(dir, 'fw.wasm')));
  const m = await F({ instantiateWasm: (imports, ok) => { WebAssembly.instantiate(wasm, imports).then((i) => ok(i, wasm)); return {}; } });
  m._fw_init();
  m.t = 0;
  // each component's prop defaults (the registered meta: catalogue.json leaves defaults out)
  m.meta = Object.fromEntries(JSON.parse(m.UTF8ToString(m._fw_meta())).map((c) => [c.id, typeof c.meta === 'string' ? JSON.parse(c.meta) : c.meta]));
  return (mods[den] = m);
}
const str = (m, s, f) => { const p = m.stringToNewUTF8(s); try { return f(p); } finally { m._free(p); } };
const call = (m, fn, a, b) => str(m, a, (pa) => str(m, b, (pb) => m.UTF8ToString(m[fn](pa, pb))));
const tick = (m, ms) => { const end = m.t + ms; while (m.t < end) { m.t += 16; m._fw_tick(m.t); } m._fw_events(); };

async function run(item, den) {
  const m = await mod(den), W = den === '1x' ? 480 : 720, D = den === '1x' ? 1 : 1.5;
  const bx = (o) => ({ x: o.x, y: o.y, w: o.w, h: o.h });
  const fails = [], add = (kind, msg, theme, box) => { if (!fails.some((f) => f.kind === kind && f.msg === msg)) fails.push({ kind, msg, theme, box: (box || []).map(bx) }); };
  if (!item.spec) { add('valid', 'not JSON'); return { fails }; }
  const text = item.text || JSON.stringify(item.spec), spec = item.spec;
  const chk = JSON.parse(call(m, '_fw_spec', 'check', text));
  for (const e of chk.errors || []) add(/budget|too_(many|big|long)|over/.test(e.code) ? 'budget' : 'valid', `${e.path || '/'} ${e.code}: ${e.msg || ''}`.trim());
  const B = cat.budget, bytes = Buffer.byteLength(text);
  if (bytes > (item.deck ? B.deck_bytes : B.bytes)) add('budget', `${bytes} bytes > ${item.deck ? B.deck_bytes : B.bytes}`);
  if (!item.deck) { if (chk.nodes > B.nodes) add('budget', `${chk.nodes} nodes > ${B.nodes}`); if (chk.binds > B.binds) add('budget', `${chk.binds} binds > ${B.binds}`); }
  if (!chk.ok || item.deck) return { fails };
  const ap = JSON.parse(call(m, '_fw_spec', 'screen', text));
  if (!ap.ok) { add('valid', `apply: ${JSON.stringify(ap.errors || ap.status)}`); return { fails }; }
  const nodeComps = new Set((spec.nodes || []).map((n) => n.c));
  const numDefaults = [...nodeComps].flatMap((c) => (m.meta[c]?.props || []).filter((p) => p.type !== 'text' && p.default !== undefined).map((p) => p.default));
  const blob = strings({ spec, numDefaults }).join('\n').toLowerCase();
  const frames = {};
  const disabled = (spec.nodes || []).some((n) => /disabled|unavailable/.test(String(n.p?.state ?? '')));
  for (const light of [0, 1]) {
    const theme = light ? 'light' : 'dark';
    const st = m._fw_stage(W, W);
    tick(m, 32);
    const e = str(m, 'spec', (pc) => str(m, JSON.stringify({ screen: spec.id }), (pp) => m.UTF8ToString(m._fw_render(st, pc, pp, light, -1))));
    if (e) { add('valid', `build: ${e}`); m._fw_stage_free(st); break; }
    tick(m, 1000);
    m._fw_refresh(st);
    const p = m._fw_frame(st), px = m.HEAPU8.slice(p, p + W * W * 4);
    for (let i = 3; i < px.length; i += 4) px[i] = 255;
    frames[theme] = px;
    const os = JSON.parse(m.UTF8ToString(m._fw_audit(st)));
    const comps = JSON.parse(m.UTF8ToString(m._fw_comps(st))).map((c) => ({ ...c, c: c.ids[c.ids.length - 1], top: c.ids.find((i) => nodeComps.has(i)) }));
    m._fw_stage_free(st);
    const at = (o) => { const cx = o.x + o.w / 2, cy = o.y + o.h / 2; const h = comps.filter((c) => cx >= c.x && cx < c.x + c.w && cy >= c.y && cy < c.y + c.h); return (h.filter((c) => c.top).sort((a, b) => a.d - b.d)[0] || h.sort((a, b) => b.d - a.d)[0] || {}).top || (h[0] || {}).c || '?'; };
    // a label that can show: on screen, not transparent, not inside a clipping ancestor that cuts it away
    const shows = (o) => { if (!o.w || !o.h || o.opa === 0 || o.x < 0 || o.y < 0 || o.x + o.w > W || o.y + o.h > W) return false; for (let k = o.p; k >= 0; k = os[k].p) { const a = os[k]; if ((!a.ov || a.sc) && !meets(o, a)) return false; } return true; };
    for (const err of checkScreen(os, W, W)) add(/leaves the screen/.test(err) ? 'outside' : 'overlap', err, theme, [...err.matchAll(/(\d+)x(\d+) at (-?\d+),(-?\d+)/g)].map((q) => ({ w: +q[1], h: +q[2], x: +q[3], y: +q[4] })));
    if (!light) {
      // sibling component boxes (the spec's nodes, side by side in one column or slot)
      const tops = comps.filter((c) => c.top === c.c && !CHROME.has(c.c));
      for (let a = 0; a < tops.length; a++) for (let b = a + 1; b < tops.length; b++) {
        const A = tops[a], Bx = tops[b];
        if (A.d === Bx.d && meets(A, Bx) && !inside(A, Bx) && !inside(Bx, A)) add('overlap', `${A.c} ${A.w}x${A.h} at ${A.x},${A.y} and ${Bx.c} ${Bx.w}x${Bx.h} at ${Bx.x},${Bx.y}`, undefined, [A, Bx]);
      }
      // spacing: list layout, the group gap after a non-row node
      if ((spec.layout || 'list') === 'list') {
        const minD = Math.min(...tops.map((c) => c.d));
        const col = tops.filter((c) => c.d === minD && c.y >= tok(TOP, den) - 1).sort((a, b) => a.y - b.y);
        for (let i = 0; i + 1 < col.length; i++) {
          const a = col[i], b = col[i + 1], gap = b.y - (a.y + a.h);
          if (!ROW(a.c) && Math.abs(a.x - b.x) < 2 && gap < tok(GROUP, den)) add('spacing', `${a.c} -> ${b.c}: gap ${gap} px < group ${tok(GROUP, den)}`, undefined, [a, b]);
        }
      }
      for (const g of gridErrs(os)) add('grid', g.m, undefined, [g.b]);
    }
    for (const o of os) {
      if (o.t !== 'label' || !o.text || !String(o.text).trim() || !shows(o)) continue;
      const t = String(o.text), who = at(o);
      if (!light) {
        if (o.cut) add('text', `${who} "${t}" cut ${o.cut} px at the bottom`, undefined, [o]);
        if (o.cutr) add('text', `${who} "${t}" ${o.cutr} px wider than its box (clipped/ellipsised)`, undefined, [o]);
        if (/…|\.\.\.$/.test(t)) add('text', `${who} "${t}" ellipsised`, undefined, [o]);
        // leak: every word / number of the label must come from the spec
        const low = t.toLowerCase().trim();
        if (!blob.includes(low) && !allowed(who, t) && !ALLOW.own_data.list.includes(who)) {
          const words = low.match(/[a-zÀ-ɏ]+|\d+/g) || [];
          const miss = words.filter((w) => !blob.includes(w));
          if (miss.length) add('leak', `${who} "${t.slice(0, 40)}"`, undefined, [o]);
        }
      }
      // contrast in the drawn pixels: background = the commonest colour in the box, ink = the farthest from it
      if (disabled) continue;
      const hist = new Map();
      for (let y = o.y; y < o.y + o.h; y++) for (let x = o.x; x < o.x + o.w; x++) { const i = (y * W + x) * 4, k = (px[i] << 16) | (px[i + 1] << 8) | px[i + 2]; hist.set(k, (hist.get(k) || 0) + 1); }
      let bg = 0, bn = -1; for (const [k, n] of hist) if (n > bn) { bn = n; bg = k; }
      const lb = lum(bg >> 16, (bg >> 8) & 255, bg & 255);
      let best = 1; for (const k of hist.keys()) { const l = lum(k >> 16, (k >> 8) & 255, k & 255); best = Math.max(best, (Math.max(l, lb) + 0.05) / (Math.min(l, lb) + 0.05)); }
      const need = o.h >= 32 * D ? 3 : 4.5;
      if (best > 1.2 && best < need) add('contrast', `${who} "${t.slice(0, 30)}" ${best.toFixed(2)}:1 < ${need} (${theme})`, theme, [o]);
    }
    // blank: a generated spec drawing nothing under the header
    if (item.gen && !light && !ALLOW.blank_ok.list.some((b) => b === item.comp || b === `${item.comp}/${item.tag}`)) {
      const y0 = tok(TOP, den), b0 = [px[(y0 * W + 2) * 4], px[(y0 * W + 2) * 4 + 1], px[(y0 * W + 2) * 4 + 2]];
      let ink = 0; for (let y = y0; y < W; y++) for (let x = 0; x < W; x++) { const i = (y * W + x) * 4; if (px[i] !== b0[0] || px[i + 1] !== b0[1] || px[i + 2] !== b0[2]) ink++; }
      if (ink < 16) add('blank', `draws ${ink} px under the header`, undefined, [{ x: 0, y: y0, w: W, h: W - y0 }]);
    }
  }
  call(m, '_fw_spec', 'revert', spec.id);
  return { fails, frames, W };
}

// ---- run
rmSync(OUT, { recursive: true, force: true });
const results = [];
const kinds = {};
for (const item of todo) {
  for (const den of item.gen ? GDEN : DEN) {
    const r = await run(item, den);
    const res = { file: item.file, comp: item.comp, tag: item.tag, den, fails: r.fails, png: {} };
    results.push(res);
    for (const k of new Set(r.fails.map((f) => f.kind))) kinds[k] = (kinds[k] || 0) + 1;
    if ((r.fails.length || ALLPNG) && r.frames) {
      mkdirSync(OUT, { recursive: true });
      const themes = ALLPNG ? ['dark', 'light'] : [r.fails.every((f) => f.theme === 'light') ? 'light' : 'dark'];
      for (const theme of themes) if (r.frames[theme]) {
        res.png[theme] = `${item.file.replace(/[^a-z0-9=._-]+/gi, '_')}--${den}--${theme}.png`;
        writeFileSync(join(OUT, res.png[theme]), png(r.frames[theme], r.W, r.W));
      }
    }
  }
}
// table: hand / extra specs one line each; generated ones grouped by component
const sum = (fs) => Object.entries(fs.reduce((a, f) => ((a[f.kind] = (a[f.kind] || 0) + 1), a), {})).map(([k, n]) => `${k} ${n}`).join(', ');
const lines = [];
for (const r of results.filter((r) => !r.comp)) lines.push(`${r.fails.length ? 'FAIL' : 'ok  '} ${r.file} [${r.den}]${r.fails.length ? `  ${sum(r.fails)}` : ''}`);
const byComp = {};
for (const r of results.filter((r) => r.comp)) (byComp[r.comp] ||= []).push(r);
for (const [c, rs] of Object.entries(byComp)) {
  const bad = rs.filter((r) => r.fails.length);
  lines.push(`${bad.length ? 'FAIL' : 'ok  '} gen/${c} ${rs.length - bad.length}/${rs.length}${bad.length ? `  ${sum(bad.flatMap((r) => r.fails))}` : ''}`);
}
for (const l of lines) if (VERB || l.startsWith('FAIL')) console.log(l);
if (VERB) for (const r of results) for (const f of r.fails) console.log(`  ${r.file} [${r.den}] ${f.kind}: ${f.msg}`);
const failed = results.filter((r) => r.fails.length).length;
const gen = results.filter((r) => r.comp);
console.log(`corpus: ${results.length} renders (${results.length - gen.length} hand/extra, ${gen.length} generated over ${Object.keys(byComp).length}/${Object.keys(cat.components).length} components, ${optionCount(cat)} props+options), ${results.length - failed} pass, ${failed} fail`);
console.log(`by kind (renders failing): ${Object.entries(kinds).sort((a, b) => b[1] - a[1]).map(([k, n]) => `${k} ${n}`).join(', ') || 'none'}`);
if (existsSync(OUT)) console.log(`${ALLPNG ? 'render' : 'failure'} PNGs: ${OUT}`);
if (arg('--json')) writeFileSync(arg('--json'), JSON.stringify(results, null, 1));
process.exit(failed ? 1 : 0);
