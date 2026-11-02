// Theme fit (renderer/CONTRACT.md C40, C45, C47; P2 + P6 per theme): every contract component, each built-in theme
// (the runner makes the theme live before it mounts the case), its default and variants, at each density:
//   P2  no object outside its parent that Facet does not already have (Facet's own bounds are the audit's: here only
//       what a THEME adds; Facet runs first, so its bounds are known when the other themes run)
//   P6  no text cut except by its rule: a "dot" or "fit" text ends in "..." by design; a "none" text (fx::text) is
//       never cut, in any theme, Facet included
// Folded here from the theme_apply functional test (nightly) so it runs in the gate tier, per component.
const FACET = new Map();  // den|id|vi -> Facet's bounds (why), from this run
// a theme's type pushes a fixed box past its parent: a layout answer per component is still open (board facet-226).
// Listed on every run (LEGACY), never hidden; a case that fits again must be removed here.
const KNOWN = new Set(['moon_icon-11', 'progress_ring-4', 'toggle-5'].flatMap((k) => [`1.5x blockcraft ${k}`, `1x blockcraft ${k}`]).concat(['1.5x win98 hero_block-8']));
// the Title's long name is cut with no "..." (it is declared none): its dot fix waits for Darren's look (parked)
for (const d of ['1.5x', '1x']) for (const t of ['facet', 'blockcraft', 'phosphor', 'win98']) KNOWN.add(`${d} ${t} header-6`);

export default {
  id: 'theme-fit', tiers: ['gate'], order: 20, looks: [0],
  about: 'Per theme: no object outside its parent that Facet does not have, and no "none" text cut (C40, C45, P2 + P6)',
  wants: (c) => c.section !== 'screens' && c.meta?.contract === 2,
  themes: () => 'all',
  run(x, rep) {
    const { m, den, theme, c, vi } = x;
    const S = (p, f) => { const q = m.stringToNewUTF8(p); try { return f(q); } finally { m._free(q); } };
    const proof = JSON.parse(S('proof', (pc) => S('', (pa) => m.UTF8ToString(m._fw_lab(x.st, pc, pa)))));
    const key = `${den}|${c.id}|${vi}`, where = `${den} ${theme} ${c.id}-${vi}`;
    if (theme === 'facet') FACET.set(key, new Set(proof.bounds.map((b) => b.why)));
    const facet = FACET.get(key) || new Set();
    const added = theme === 'facet' ? [] : proof.bounds.filter((b) => !facet.has(b.why));
    const known = KNOWN.has(where);
    for (const b of added.slice(0, 2)) (known ? rep.legacy : (l) => rep.check(false, l))(`${where}: ${b.why} (P2: only in this theme)`);
    const cut = proof.trunc.filter((t) => t.why.includes(' (none) '));
    for (const t of cut.slice(0, 2)) (known ? rep.legacy : (l) => rep.check(false, l))(`${where}: ${t.why} (C40: a "none" text is never cut; declare dot or fit, or give it room)`);
    if (known && !added.length && !cut.length) rep.check(false, `${where}: fits now: remove it from KNOWN in tests/rules/theme_fit.mjs`);
    if (!added.length && !cut.length && !known) rep.check(true, `${where}: fits`);
  },
};
