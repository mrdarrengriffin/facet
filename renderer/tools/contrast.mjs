#!/usr/bin/env node
// Contrast test: every declared foreground/background pair (tokens/colour.json "contrast.pairs") x theme (dark, light)
// x accent set, checked against WCAG 2.x AA. Pairs are declared where they are used (foundations, then one block
// per component), a pair's "bg" may list several backgrounds (a transparent control sits on the canvas or a card).
// Each combination is checked twice, on the exact token values and on the RGB565 values the panel shows; the worse
// ratio decides.
//   node tools/contrast.mjs            report the failures + a summary, exit 1 if any fail
//   node tools/contrast.mjs --all      every combination
//   node tools/contrast.mjs --json     the expanded matrix as JSON
//   node tools/contrast.mjs --summary  one line (build.sh)
// Also imported by gen_colour.mjs (expand(), the C++ table for the Colour page).
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
export const SRC = join(HERE, '..', 'tokens', 'colour.json');

const lin = (v) => { const s = v / 255; return s <= 0.03928 ? s / 12.92 : ((s + 0.055) / 1.055) ** 2.4; };
const rgb = (hex) => [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16));
const lum = ([r, g, b]) => 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b);
const ratio = (a, b) => { const x = lum(a), y = lum(b); return (Math.max(x, y) + 0.05) / (Math.min(x, y) + 0.05); };
// RGB565 as LVGL converts (truncate to 5/6/5 bits) and the panel expands (bit replication)
const to565 = ([r, g, b]) => { const R = r >> 3, G = g >> 2, B = b >> 3; return [(R << 3) | (R >> 2), (G << 2) | (G >> 4), (B << 3) | (B >> 2)]; };

// token value in a theme with an accent set (accent roles come from the set)
export function valueOf(t, name, theme, set) {
  if (t.accent.roles.includes(name)) return set[theme][name];
  const c = t.colours[name];
  if (!c) throw new Error(`contrast: unknown token ${name}`);
  return c[theme];
}

// the declared pairs, one entry per (pair, background)
export function pairs(t) {
  const out = [];
  const kinds = t.contrast.kinds;
  for (const p of t.contrast.pairs) {
    if (!kinds[p.kind]) throw new Error(`contrast: ${p.use}: unknown kind ${p.kind}`);
    for (const k of [p.fg, ...[p.bg].flat()]) if (!t.colours[k]) throw new Error(`contrast: ${p.use}: unknown token ${k}`);
    for (const bg of [p.bg].flat()) out.push({ section: p.section, fg: p.fg, bg, kind: p.kind, min: kinds[p.kind].min, use: p.use });
  }
  return out;
}

// every combination: pair x theme x accent set
export function expand(t) {
  const rows = [];
  for (const set of t.accent.sets)
    for (const theme of ['dark', 'light'])
      for (const p of pairs(t)) {
        const fg = valueOf(t, p.fg, theme, set), bg = valueOf(t, p.bg, theme, set);
        const exact = ratio(rgb(fg), rgb(bg)), panel = ratio(to565(rgb(fg)), to565(rgb(bg)));
        const r = Math.min(exact, panel);
        rows.push({ ...p, accent: set.id, theme, fgHex: fg, bgHex: bg, exact, panel, ratio: r, pass: r >= p.min });
      }
  return rows;
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const t = JSON.parse(readFileSync(SRC, 'utf8'));
  const rows = expand(t), fails = rows.filter((r) => !r.pass);
  const arg = process.argv[2] ?? '';
  const line = `contrast: ${rows.length} combinations (${pairs(t).length} pairs x 2 themes x ${t.accent.sets.length} accents), ${fails.length} fail`;
  if (arg === '--json') { console.log(JSON.stringify(rows, null, 1)); process.exit(0); }
  if (arg === '--summary') { console.log(line); process.exit(0); }
  const show = arg === '--all' ? rows : fails;
  const f2 = (x) => x.toFixed(2).padStart(5);
  for (const r of show)
    console.log(`${r.pass ? 'ok  ' : 'FAIL'} ${r.accent.padEnd(6)} ${r.theme.padEnd(5)} ${f2(r.ratio)} / ${r.min}  ${r.kind.padEnd(8)} ${r.fg} ${r.fgHex} on ${r.bg} ${r.bgHex}  (${r.use})`);
  console.log(line);
  process.exit(fails.length ? 1 : 0);
}
