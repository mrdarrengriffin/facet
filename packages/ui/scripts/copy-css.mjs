// Copies the component styles into dist/ next to the compiled JavaScript.
import { copyFile, mkdir } from 'node:fs/promises';

await mkdir(new URL('../dist/', import.meta.url), { recursive: true });
await copyFile(
  new URL('../src/styles.css', import.meta.url),
  new URL('../dist/styles.css', import.meta.url),
);
console.log('Copied dist/styles.css');
