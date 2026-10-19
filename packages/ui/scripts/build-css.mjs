// Joins the component styles in src/styles/ (in order) into dist/styles.css.
import { mkdir, readFile, readdir, writeFile } from 'node:fs/promises';

const dir = new URL('../src/styles/', import.meta.url);
const files = (await readdir(dir)).filter((f) => f.endsWith('.css')).sort();
const parts = await Promise.all(files.map((f) => readFile(new URL(f, dir), 'utf8')));
await mkdir(new URL('../dist/', import.meta.url), { recursive: true });
await writeFile(new URL('../dist/styles.css', import.meta.url), parts.join('\n'));
console.log(`Wrote dist/styles.css (${files.join(', ')})`);
await writeFile(
  new URL('../dist/checkbox.css', import.meta.url),
  await readFile(new URL('15-checkbox.css', dir), 'utf8'),
);
