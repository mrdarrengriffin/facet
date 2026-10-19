// Checks the schema and writes dist/settings.json + dist/esphome/settings.yaml. Runs after `tsc` in `pnpm build`.
import { copyFile, mkdir, readFile, writeFile } from 'node:fs/promises';
import { check, toEsphome } from '../dist/index.js';

const packs = JSON.parse(
  await readFile(new URL('../../sounds/src/packs.json', import.meta.url), 'utf8'),
).packs.map((p) => p.id);
const errors = check(packs);
if (errors.length) {
  console.error(`settings.json has ${errors.length} problem(s):\n  ${errors.join('\n  ')}`);
  process.exit(1);
}
const dist = new URL('../dist/', import.meta.url);
await mkdir(new URL('esphome/', dist), { recursive: true });
await copyFile(new URL('../src/settings.json', import.meta.url), new URL('settings.json', dist));
await writeFile(new URL('esphome/settings.yaml', dist), toEsphome());
console.log('Checked settings.json; wrote dist/settings.json and dist/esphome/settings.yaml');
