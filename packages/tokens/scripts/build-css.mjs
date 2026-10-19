// Writes dist/tokens.css and dist/esphome/tokens.yaml from the compiled tokens. Runs after `tsc` in `pnpm build`.
import { mkdir, writeFile } from 'node:fs/promises';
import { toCss, toEsphome } from '../dist/index.js';

const dist = new URL('../dist/', import.meta.url);
await mkdir(new URL('esphome/', dist), { recursive: true });
await writeFile(new URL('tokens.css', dist), toCss());
await writeFile(new URL('esphome/tokens.yaml', dist), toEsphome());
console.log('Wrote dist/tokens.css and dist/esphome/tokens.yaml');
