// A golden as a PNG, rendered on demand from its .565.gz (goldens are committed as 565 only).
//   node tools/golden_png.mjs <suite>/<den>-<name> [out.png]   out omitted or '-' = stdout
//   node tools/golden_png.mjs --all <dir>                      every golden into <dir>/<suite>/<den>-<name>.png
import { mkdirSync, readdirSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { GOLDEN, goldenPng } from '../tests/functional/_release.mjs';

const [a, b] = process.argv.slice(2);
if (!a) { console.error('usage: golden_png.mjs <suite>/<den>-<name> [out.png] | --all <dir>'); process.exit(2); }
if (a === '--all') {
  const keys = readdirSync(GOLDEN, { withFileTypes: true }).filter((d) => d.isDirectory()).flatMap((d) =>
    readdirSync(join(GOLDEN, d.name)).filter((f) => f.endsWith('.565.gz') && !f.includes('.new.')).map((f) => `${d.name}/${f.slice(0, -7)}`));
  for (const k of keys) { const p = join(b, `${k}.png`); mkdirSync(dirname(p), { recursive: true }); writeFileSync(p, goldenPng(k)); }
  console.log(`${keys.length} goldens -> ${b}`);
} else if (!b || b === '-') process.stdout.write(goldenPng(a));
else writeFileSync(b, goldenPng(a));
