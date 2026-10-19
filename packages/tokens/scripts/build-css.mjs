// Writes dist/tokens.css from the compiled tokens. Runs after `tsc` in `pnpm build`.
import { mkdir, writeFile } from 'node:fs/promises';
import { toCss } from '../dist/index.js';

const out = new URL('../dist/tokens.css', import.meta.url);
await mkdir(new URL('.', out), { recursive: true });
await writeFile(out, toCss());
console.log('Wrote dist/tokens.css');
