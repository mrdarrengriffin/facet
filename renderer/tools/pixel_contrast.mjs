#!/usr/bin/env node
// Pixel contrast proof (renderer/CONTRACT.md C15-C17, P4; Facet release gate section 4). tools/contrast.mjs checks the
// DECLARED colour pairs; this one checks what is really on the pixels. For every contract component (meta "contract":
// 2, or --ids any) x every meta variant x density, in four looks: built dark, switched LIVE to light, built light,
// switched LIVE to dark (a component that holds a colour from build time fails the live ones). In each look every shown
// text and icon layer (fw_test "inks") is veiled (drawn transparent, nothing moves) and the frame compared: the pixels
// that change are its ink, the veiled frame is what lies behind it. Its contrast = its own drawn colour (text colour /
// icon recolour, RGB565 as the panel shows it) against the commonest pixel really behind its ink, or the best ink pixel
// vs the pixel behind it if higher (a picture with its own colours: that alone); relative luminance as WCAG 2.x. Minimum: text 4.5, large text 3, icons 3, disabled (fw::st::OFF) 3.
// Disabled graphics (inactive components: WCAG 1.4.11 exempts them) 1.5:1: still visible, not 3:1.
// Graphical objects (WCAG 1.4.11): the toggle knob, the chosen segment and the Pen's drawn knob / fill, 3:1 against
// what is behind them, in every variant, look and theme (fw_test gfx + veil_part): its face, or a colour drawing a whole
// side of it (its outline: Win98's sunken chosen segment, black / white edges on a lighter grey face); a knob also against everything
// round it (track, fill AND page: see the knob block below); a Win98-style checkbox's tick = the drawn part "switch".
// No exemption list: a failing graphic fails the build (renderer/CONTRACT.md C17).
// A text that changes no pixel when veiled is invisible: ratio 1, a failure. Exit 1 on any failure. A failing item
// that a LEGACY component built (one not on the contract, inside a contract one: the Segmented in a Header) is listed
// as LEGACY and counted, not failed: its migration piece fixes it.
// --theme <id,id|all>: the same in each theme (theme engine T2: themes/<id>.json made live before every build, then the
// live mode switch as above); all = facet + every built-in theme. Default facet.
//   node tools/pixel_contrast.mjs [--den 1x|1.5x] [--ids a,b] [--theme all] [--build <FW_OUT>] [-v]
import { readFileSync, existsSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { instance } from './lib/fwbench.mjs';

const HERE = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const env = (() => { try { return Object.fromEntries(readFileSync(join(HERE, '..', 'local.env'), 'utf8').split('\n').map((l) => l.match(/^([A-Z_]+)=(.*)$/)).filter(Boolean).map((m) => [m[1], m[2]])); } catch { return {}; } })();
const arg = (n) => { const i = process.argv.indexOf(n); return i > 0 ? process.argv[i + 1] : null; };
const BUILD = arg('--build') || process.env.FW_OUT || env.FW_OUT || join(HERE, 'out');
const DENS = arg('--den') ? [arg('--den')] : ['1x', '1.5x'].filter((d) => existsSync(join(BUILD, d, 'fw.mjs')));
const IDS = arg('--ids') ? arg('--ids').split(',') : null;
const THEME_ARG = arg('--theme') || 'facet';
const V = process.argv.includes('-v');
const TICK = 16;
// how far inside a knob's footprint its own colour may start: its halo (size stroke:knob, the canvas colour round
// it: the theme's, else Facet's) + the theme's knob edges (a bevel, in pixel cells on a snapped theme), at a density
const HALO = (theme, den) => {
  const d = den === '1x' ? 1 : 0, f = JSON.stringify(JSON.parse(readFileSync(join(HERE, 'tokens/space.json'), 'utf8'))).match(/"knob":\{"v":(\d+),"1x":(\d+)/);
  let v = f ? [+f[1], +f[2]] : [4, 2], edge = 0;
  try {
    const t = JSON.parse(readFileSync(join(HERE, `themes/${theme}.json`), 'utf8'));
    if (t.size?.['stroke:knob']) v = t.size['stroke:knob'];
    const cell = t.shape?.snap ? (t.size?.['stroke:pixel'] || [1, 1])[d] : 1;
    for (const e of t.parts?.knob?.edges || []) edge = Math.max(edge, ((e.inset || 0) + (e.w || 1)) * cell);
  } catch {}
  return v[d] + edge;
};

const lin = (c) => { c /= 255; return c <= 0.04045 ? c / 12.92 : ((c + 0.055) / 1.055) ** 2.4; };
const LUM = new Float64Array(65536);
for (let w = 0; w < 65536; w++) LUM[w] = 0.2126 * lin((w >> 11) << 3) + 0.7152 * lin(((w >> 5) & 63) << 2) + 0.0722 * lin((w & 31) << 3);
const ratio = (a, b) => { const x = LUM[a], y = LUM[b]; return (Math.max(x, y) + 0.05) / (Math.min(x, y) + 0.05); };

let checks = 0, fails = 0, legacy = 0;
const out = [];
for (const den of DENS) {
  const D = den === '1x' ? 1 : 1.5;
  let m = await instance(den, BUILD);
  const metas = JSON.parse(m.UTF8ToString(m._fw_meta())).map((c) => ({ ...c, meta: typeof c.meta === 'string' ? JSON.parse(c.meta) : c.meta }));
  const comps = metas.filter((c) => c.section !== 'screens' && (IDS ? IDS.includes(c.id) : c.meta && c.meta.contract === 2));
  const THEMES = THEME_ARG === 'all' ? ['facet', ...JSON.parse(m.UTF8ToString(m._fw_lab(-1, m.stringToNewUTF8('theme_builtins'), m.stringToNewUTF8('')))).map((x) => x.id).filter((x) => x !== 'facet')] : THEME_ARG.split(',');
  for (const theme of THEMES) for (const c of comps) {
    m = await instance(den, BUILD);  // a fresh module per component (one per render runs the heap out)
    if (theme !== 'facet') { const r = JSON.parse(m.UTF8ToString(m._fw_lab(-1, m.stringToNewUTF8('theme_use'), m.stringToNewUTF8(theme)))); if (!r.ok) { fails++; out.push(`FAIL theme ${theme}: refused ${JSON.stringify(r.errors).slice(0, 200)}`); continue; } }
    const SS = den === '1x' ? 480 : 720, [fw, fh] = c.meta.frame || [SS / D, SS / D];  // as tools/visual.mjs
    const W = Math.round(fw * D), H = fh ? Math.round(fh * D) : SS;
    for (const v of c.meta.variants?.length ? c.meta.variants : [{ label: 'default', props: {} }]) {
      for (const [first, then] of [[0, 1], [1, 0]]) {
        let t = m.__t || 0;
        const S = (s, f) => { const p = m.stringToNewUTF8(s); try { return f(p); } finally { m._free(p); } };
        const test = (cmd, a = '') => S(cmd, (pc) => S(a, (pa) => m.UTF8ToString(m._fw_test(pc, pa))));
        const tick = (n = 1) => { for (let i = 0; i < n; i++) { t += TICK; m._fw_tick(t); } m._fw_events(); };
        const settle = () => { for (let i = 0, calm = 0; i < 120 && calm < 3; i++) { tick(); calm = m._fw_anims() ? 0 : calm + 1; } };
        const st = m._fw_stage(W, H);
        tick(2);
        if (process.env.PC_TRACE) console.error(`render ${den} ${c.id} "${v.label}" ${first}`);
        const e = S(c.id, (pc) => S(JSON.stringify(v.props || {}), (pp) => m.UTF8ToString(m._fw_render(st, pc, pp, first, 0))));
        if (e) { m._fw_stage_free(st); out.push(`skip ${den} ${c.id} "${v.label}": ${e}`); continue; }
        settle();
        const frame = () => {
          m._fw_refresh(st);
          const p = m._fw_frame(st), h = m.HEAPU8, w = new Uint16Array(W * H);
          for (let i = 0; i < W * H; i++) { const q = p + i * 4; w[i] = ((h[q] >> 3) << 11) | ((h[q + 1] >> 2) << 5) | (h[q + 2] >> 3); }
          return w;
        };
        const dis = v.props?.state === 'disabled' || v.props?.disabled === true;
        const measure = (look) => {
          const items = JSON.parse(test('inks') || '[]'), base = frame();
          for (const it of items) {
            if (it.k === 'text' && !String(it.s).trim()) continue;
            test('veil', `${it.i},1`);
            const only = frame();  // its own pixels: what changes when it alone goes
            // what is behind it: for an icon layer, the whole icon veiled (its other parts are the icon, not its background)
            const grp = it.k === 'ui' ? items.filter((o) => o.grp === it.grp && o.i !== it.i) : [];
            for (const o of grp) test('veil', `${o.i},1`);
            const bg = grp.length ? frame() : only;
            for (const o of grp) test('veil', `${o.i},0`);
            test('veil', `${it.i},0`);
            // the pixels it changes = its ink on screen; the veiled frame there = what is really behind it
            let best = 0, n = 0;
            const behind = new Map();
            for (let y = Math.max(0, it.y); y < Math.min(H, it.y + it.h); y++)
              for (let x = Math.max(0, it.x); x < Math.min(W, it.x + it.w); x++) {
                const k = y * W + x;
                if (base[k] !== only[k]) { n++; const r = ratio(base[k], bg[k]); if (r > best) best = r; behind.set(bg[k], (behind.get(bg[k]) || 0) + 1); }
              }
            // its colour (as the panel shows it) against the commonest pixel behind its ink: a thin anti-aliased stroke
            // never covers a whole pixel, so its pixels alone under-read a correct colour; a stale colour reads stale
            if (n && it.ink >= 0) { const b0 = [...behind].sort((a, b) => b[1] - a[1])[0][0]; best = Math.max(best, ratio(it.ink, b0)); }
            if (!n && it.k === 'ui') continue;  // an icon layer with nothing of its own on screen (covered, or empty here)
            if (!n) best = 1;  // a text that changes no pixel: invisible
            checks++;
            const what = it.k === 'text' ? `"${String(it.s).slice(0, 28)}"` : `icon layer ${it.w}x${it.h}@${it.x},${it.y}`;
            const line = `${den}${theme === 'facet' ? '' : ' ' + theme} ${c.id} "${v.label}" ${look}: ${what} ${best.toFixed(2)} / ${it.min}`;
            if (best + 1e-9 < it.min && it.legacy && c.meta.contract === 2) {  // a legacy component under test (--ids) is never excused
 legacy++; out.push(`LEGACY ${line} (inside ${it.c}, not on the contract yet: reported, not failed)`); }
            else if (best + 1e-9 < it.min) { fails++; out.push(`FAIL ${line}`); } else if (V) out.push(`ok   ${line}`);
          }
          // graphical objects (WCAG 1.4.11, 3:1): what shows the state (toggle knob on its track, the chosen segment,
          // a drawn knob / fill on its track). Veil it: the pixels that change are it, the veiled frame is behind it;
          // its contrast = the best of its own colours (any covering >= 3% of it: a face, or a ring round it) against the
          // commonest pixel behind. Objects: fw_test "gfx"; drawn parts: fw_test "veil_part" (the Pen skips that part)
          const gfx = (name, veil, unveil, min, shown) => {
            if (dis) min = Math.min(min, 1.5);
            // both frames full redraws (veil_part repaints the whole screen): a partial repaint after the text veils above
            // can differ from a full one over a theme's pattern face (Blockcraft: a hardening item), which is not this object
            test('veil_part', ''); const before = frame(); veil(); const after = frame(); unveil();
            const own = new Map(), behind = new Map(); let n = 0;
            for (let k = 0; k < W * H; k++) if (before[k] !== after[k]) { n++; own.set(before[k], (own.get(before[k]) || 0) + 1); behind.set(after[k], (behind.get(after[k]) || 0) + 1); }
            if (!n && !shown) return;  // a drawn part not drawn in this variant
            let best = 1;  // a shown object that changes no pixel: invisible (the same colour as what is behind it)
            if (n) { const b0 = [...behind].sort((a, b) => b[1] - a[1])[0][0]; for (const [col, cnt] of own) if (cnt >= n * 0.03) best = Math.max(best, ratio(col, b0));
              // its outline counts too (WCAG 1.4.11: a boundary is a state indicator): a colour that draws a whole side
              // of the object (>= 90% of one of its two outermost lines, a box's edge: Win98's sunken chosen segment)
              // is one of its own colours however few pixels that is; a round object (a knob) has no straight side
              if (!/knob/.test(name)) {
                let x0 = W, y0 = H, x1 = -1, y1 = -1;
                for (let k = 0; k < W * H; k++) if (before[k] !== after[k]) { const x = k % W, y = (k - x) / W; x0 = Math.min(x0, x); x1 = Math.max(x1, x); y0 = Math.min(y0, y); y1 = Math.max(y1, y); }
                const side = (pts) => { const c = new Map(); for (const k of pts) c.set(before[k], (c.get(before[k]) || 0) + 1); for (const [col, cnt] of c) if (cnt >= pts.length * 0.9) best = Math.max(best, ratio(col, b0)); };
                for (let d = 0; d < 2 && x1 - x0 > 2 * d + 4 && y1 - y0 > 2 * d + 4; d++) {
                  const T = [], B = [], L = [], R2 = [];
                  for (let x = x0 + d; x <= x1 - d; x++) { T.push((y0 + d) * W + x); B.push((y1 - d) * W + x); }
                  for (let y = y0 + d; y <= y1 - d; y++) { L.push(y * W + x0 + d); R2.push(y * W + x1 - d); }
                  for (const pts of [T, B, L, R2]) side(pts);
                }
              }
            }
            // a knob (the Pen's, the Toggle's): 3:1 against EVERYTHING round it, not just the commonest pixel behind:
            // the track, the fill AND the page or card round it (a knob the colour of the page, or of its own fill,
            // fails). Its footprint = the rows of pixels it changes, filled across (a part of it the colour of what is
            // under it is still it); every pixel just outside must see a footprint pixel within R px (its halo + bevel + 2) at 3:1 (its face, or
            // a halo / ring round it); grouped by what that pixel is (page, track, fill), each group (5 px or 2% of it at least: a corner's few pixels are anti-aliasing) its 10th percentile
            // counts (anti-aliasing). Disabled (inactive, WCAG 1.4.11 exempts it): 1.5:1, still visible (C17)
            if (n && /knob/.test(name)) {
              const F = new Uint8Array(W * H);
              for (let y = 0; y < H; y++) {
                let a = -1, b = -1;
                for (let x = 0; x < W; x++) if (before[y * W + x] !== after[y * W + x]) { if (a < 0) a = x; b = x; }
                for (let x = a; a >= 0 && x <= b; x++) F[y * W + x] = 1;
              }
              const R = HALO(theme, den) + 2;  // its halo + bevel + 2
              const seen = new Uint8Array(W * H), groups = new Map();
              for (let k = 0; k < W * H; k++) if (F[k]) {
                const x = k % W, y = (k - x) / W;
                for (let dy = -1; dy <= 1; dy++) for (let dx = -1; dx <= 1; dx++) {
                  const X = x + dx, Y = y + dy, q = Y * W + X;
                  if (X < 0 || Y < 0 || X >= W || Y >= H || seen[q] || F[q]) continue;
                  seen[q] = 1;
                  let b = 1;
                  for (let ey = -R; ey <= R; ey++) for (let ex = -R; ex <= R; ex++) {
                    const XX = X + ex, YY = Y + ey, r = YY * W + XX;
                    if (XX >= 0 && YY >= 0 && XX < W && YY < H && F[r]) b = Math.max(b, ratio(before[r], after[q]));
                  }
                  if (!groups.has(after[q])) groups.set(after[q], []);
                  groups.get(after[q]).push(b);
                }
              }
              const all = [...groups.values()].reduce((t, g) => t + g.length, 0);
              for (const g of groups.values()) if (g.length >= Math.max(5, all * 0.02)) { g.sort((a, b) => a - b); best = Math.min(best, g[Math.floor(g.length * 0.1)]); }
            }
            checks++;
            const line = `${den}${theme === 'facet' ? '' : ' ' + theme} ${c.id} "${v.label}" ${look}: graphic ${name} ${best.toFixed(2)} / ${min}`;
            if (best + 1e-9 < min) { fails++; out.push(`FAIL ${line}`); } else if (V) out.push(`ok   ${line}`);
          };
          for (const g of JSON.parse(test('gfx') || '[]')) {
            if (g.s === 'seg-part' && !g.on) continue;  // an unchosen segment shows no state of its own
            gfx(g.s, () => test('veil', `${g.i},1`), () => test('veil', `${g.i},0`), g.min, g.w > 0 && g.h > 0);
          }
          for (const part of ['knob', 'track-fill', 'arc-fill', 'switch']) gfx(part, () => test('veil_part', part), () => test('veil_part', ''), 3);
        };
        measure(first ? 'built light' : 'built dark');
        test('look', `${then},-1`); settle();
        measure(then ? 'dark -> light live' : 'light -> dark live');
        m._fw_stage_free(st);
        m.__t = t;
      }
    }
  }
}
for (const l of out) console.log(l);
console.log(`pixel contrast: ${checks} checks, ${fails} failed, ${legacy} in legacy parts (${DENS.join(' + ')}, themes ${THEME_ARG}${IDS ? `, ${IDS.join(',')}` : ', every contract component'})`);
process.exit(fails ? 1 : 0);
