#!/usr/bin/env node
// Build-time theme check (build.sh, 1.5x; renderer/CONTRACT.md C44, C47): built-in themes are validated when the
// build runs, so the panel never meets a bad built-in at run time (it never aborts on data).
//   1. themes/facet.json is what the tokens say (tools/gen_theme.mjs)
//   2. every built-in theme (themes/*.json, embedded in this build) loads: structure, ranges, guards
//   3. facet.json compiles to exactly Facet's defaults (fx::theme_facet: the same digest)
//   4. a compiled theme fits the memory budget: <= 24 KB (theme engine S6)
// node tools/theme_check.mjs [--build <FW_OUT>]   exit 1 on any failure
import { readFileSync, existsSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { facetTheme, THEMES } from './gen_theme.mjs';

const HERE = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const env = {};
if (existsSync(join(HERE, '..', 'local.env'))) for (const l of readFileSync(join(HERE, '..', 'local.env'), 'utf8').split('\n')) { const m = l.match(/^(\w+)=(.*)$/); if (m) env[m[1]] = m[2]; }
const i = process.argv.indexOf('--build');
const BUILD = i > 0 ? process.argv[i + 1] : process.env.FW_OUT || env.FW_OUT || join(HERE, 'out');
const den = process.argv.includes('--den') ? process.argv[process.argv.indexOf('--den') + 1] : '1.5x';
const fails = [];
if (readFileSync(join(THEMES, 'facet.json'), 'utf8') !== facetTheme()) fails.push('themes/facet.json is out of date (node tools/gen_theme.mjs)');
const m = await (await import(pathToFileURL(join(BUILD, den, 'fw.mjs')).href)).default();
m._fw_init();
const S = (p, f) => { const q = m.stringToNewUTF8(p); try { return f(q); } finally { m._free(q); } };
const lab = (c, a = '') => JSON.parse(S(c, (pc) => S(a, (pa) => m.UTF8ToString(m._fw_lab(-1, pc, pa)))));
const built = lab('theme_builtins');
for (const b of built) if (!b.ok) fails.push(`built-in theme ${b.id} is refused: ${b.errors.slice(0, 3).map((e) => `${e.path}: ${e.msg}`).join(' | ')}`);
const facet = built.find((b) => b.id === 'facet'), def = lab('theme_facet');
if (!facet || facet.digest !== def.digest) fails.push(`themes/facet.json compiles to ${facet?.digest} but Facet's defaults are ${def.digest}`);
const info = lab('theme_info');
if (info.skin_bytes > 24 * 1024) fails.push(`a compiled theme is ${info.skin_bytes} B (budget 24 KB)`);
for (const f of fails) console.log(`theme_check: FAIL ${f}`);
console.log(`theme_check: ${built.length} built-in themes (${built.map((b) => `${b.id} ${b.ok ? `${b.checks} checks` : 'REFUSED'}`).join(', ')}), skin ${info.skin_bytes} B, ${fails.length} fail`);
process.exit(fails.length ? 1 : 0);
