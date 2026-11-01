// The Facet editor's page data, shared by the workbench page (src/pages/wasm/editor.astro) and the device bundle
// (scripts/editor-device.mjs): Facet's own tokens as CSS variables + the boot JSON (accents, source catalogue, the
// specs' typed params), all read from a renderer/ tree (the shared facet-wasm, or the worktree a firmware is built from).
import { readFileSync, readdirSync } from 'node:fs';
import { join } from 'node:path';

export function editorPage(R) {
  const rj = (p) => JSON.parse(readFileSync(join(R, p), 'utf8'));
  const colour = rj('tokens/colour.json'), space = rj('tokens/space.json'), type = rj('tokens/type.json');
  const css = [];
  const vars = (mode) => Object.entries(colour.colours).map(([k, v]) => `--f-${k}:${v[mode]};`).join('');
  const sp = Object.entries(space.tokens.space).map(([k, v]) => `--sp-${k}:${v.v}px;`).join('');
  const rad = Object.entries(space.tokens.radius).map(([k, v]) => `--rad-${k}:${v.v}px;`).join('');
  const ty = ['c-title', 'c-subtitle', 'c-body', 'c-label-sm', 'c-meta', 'c-meta-sm', 'c-caps-sm', 'title', 'label'].map((k) => {
    const s = type.styles[k];
    return `--t-${k}:${s.weight} ${s.size}px/${s.lineHeight} var(--font);--ls-${k}:${s.letterSpacing}px;`;
  }).join('');
  const motion = rj('tokens/motion.json');  // the ui durations (--mo-<name>) + easings (--ease-out, --ease-in-out, --ease-linear)
  const mo = Object.entries(motion.durations).filter(([, v]) => v.group === 'ui').map(([k, v]) => `--mo-${k}:${v.ms}ms;`).join('')
    + Object.entries(motion.easings).map(([k, v]) => `--ease-${k.replace(/^ease-/, '')}:${v.css};`).join('');
  const touch = Object.entries(space.tokens.touch).filter(([, v]) => typeof v?.v === 'number').map(([k, v]) => `--touch-${k}:${v.v}px;`).join('');
  css.push(`:root{${vars('dark')}${sp}${rad}${ty}${touch}${mo}--font:'${type.families.sans.family}',system-ui,sans-serif;--mono:'${type.families.mono.family}',ui-monospace,monospace;color-scheme:dark}`);
  css.push(`@media (prefers-color-scheme: light){:root:not([data-theme="dark"]){${vars('light')}color-scheme:light}}`);
  css.push(`:root[data-theme="light"]{${vars('light')}color-scheme:light}`);
  const accents = { default: colour.accent.default, sets: colour.accent.sets };
  const catalogue = rj('sources/sources.json');
  const specs = {};
  for (const f of readdirSync(join(R, 'specs'))) {
    if (!f.endsWith('.json') || f === 'deck.json') continue;
    try { const s = rj(`specs/${f}`); specs[s.id ?? f.slice(0, -5)] = { name: s.name ?? f.slice(0, -5), role: s.role ?? 'page', params: s.params ?? {} }; } catch { /* skip */ }
  }
  return { css: css.join('\n'), boot: JSON.stringify({ accents, catalogue, specs }).replace(/</g, '\\u003c') };
}
