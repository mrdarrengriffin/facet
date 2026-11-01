// Build-time theme check (build.sh, 1.5x; renderer/CONTRACT.md C44, C47): built-in themes are validated when the
// build runs, so the panel never meets a bad built-in at run time (it never aborts on data).
//   1. themes/facet.json is what the tokens say (tools/gen_theme.mjs)
//   2. every built-in theme (themes/*.json, embedded in this build) loads: structure, ranges, guards
//   3. facet.json compiles to exactly Facet's defaults (fx::theme_facet: the same digest)
//   4. a compiled theme fits the memory budget: <= 24 KB (theme engine S6)
// Moved from tools/theme_check.mjs: a startup check of the runner (tools/lib/rules.mjs), once at 1.5x.
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { facetTheme, THEMES } from '../../tools/gen_theme.mjs';

export default {
  id: 'theme-check', tiers: ['fast', 'gate'], dens: ['1.5x'],
  about: 'Built-in themes: facet.json up to date, every built-in loads, facet.json = Facet defaults, skin within 24 KB',
  once({ m }, rep) {
    const fails = [];
    if (readFileSync(join(THEMES, 'facet.json'), 'utf8') !== facetTheme()) fails.push('themes/facet.json is out of date (node tools/gen_theme.mjs)');
    const S = (p, f) => { const q = m.stringToNewUTF8(p); try { return f(q); } finally { m._free(q); } };
    const lab = (c, a = '') => JSON.parse(S(c, (pc) => S(a, (pa) => m.UTF8ToString(m._fw_lab(-1, pc, pa)))));
    const built = lab('theme_builtins');
    for (const b of built) if (!b.ok) fails.push(`built-in theme ${b.id} is refused: ${b.errors.slice(0, 3).map((e) => `${e.path}: ${e.msg}`).join(' | ')}`);
    const facet = built.find((b) => b.id === 'facet'), def = lab('theme_facet');
    if (!facet || facet.digest !== def.digest) fails.push(`themes/facet.json compiles to ${facet?.digest} but Facet's defaults are ${def.digest}`);
    const info = lab('theme_info');
    if (info.skin_bytes > 24 * 1024) fails.push(`a compiled theme is ${info.skin_bytes} B (budget 24 KB)`);
    for (const f of fails) rep.check(false, f);
    if (!fails.length) rep.check(true, 'every built-in theme');
    rep.note(`theme-check: ${built.length} built-in themes (${built.map((b) => `${b.id} ${b.ok ? `${b.checks} checks` : 'REFUSED'}`).join(', ')}), skin ${info.skin_bytes} B`);
  },
};
