// Joins the component styles in src/styles/ (in order) into dist/styles.css, then adds the settings rules generated
// from @facet/settings: one accent set per `data-facet-accent` (light-dark(), so it follows the theme in use) and the
// Large text sizes under `data-facet-text`.
import { mkdir, readFile, readdir, writeFile } from 'node:fs/promises';
import { setting } from '@facet/settings';

const dir = new URL('../src/styles/', import.meta.url);
const files = (await readdir(dir)).filter((f) => f.endsWith('.css')).sort();
const parts = await Promise.all(files.map((f) => readFile(new URL(f, dir), 'utf8')));
parts.push(settingsCss());
await mkdir(new URL('../dist/', import.meta.url), { recursive: true });
await writeFile(new URL('../dist/styles.css', import.meta.url), parts.join('\n'));
console.log(`Wrote dist/styles.css (${files.join(', ')})`);
await writeFile(
  new URL('../dist/checkbox.css', import.meta.url),
  await readFile(new URL('15-checkbox.css', dir), 'utf8'),
);

function settingsCss() {
  const out = ['/* Settings (generated from @facet/settings by scripts/build-css.mjs) */'];
  for (const o of setting('accent').options) {
    const { dark, light } = o.colors;
    const vars = Object.keys(dark).map(
      (k) => `  --facet-color-${k}: light-dark(${light[k]}, ${dark[k]});`,
    );
    // doubled attribute: outranks the theme blocks in tokens.css when both sit on the same element
    out.push(`[data-facet-accent='${o.id}'][data-facet-accent] {\n${vars.join('\n')}\n}`);
  }
  // same line boxes, only the font size moves; attribute + class outranks the plain .fst-* rule
  for (const o of setting('textSize').options)
    for (const [role, px] of Object.entries(o.type ?? {}))
      out.push(`[data-facet-text='${o.id}'] .fst-${role} {\n  font-size: ${px}px;\n}`);
  return out.join('\n') + '\n';
}
