#!/usr/bin/env node
// The board options of the GitHub issue templates (.github/ISSUE_TEMPLATE/*.yml, between "# boards:" and "# end boards")
// from boards/*.json: one "<name> (<w> x <h>)" per board with status target, largest screen first.
//   node boards/templates.mjs           write them
//   node boards/templates.mjs --check   exit 1 when a template is out of date (CI)
import { readdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url)), T = join(HERE, '..', '.github', 'ISSUE_TEMPLATE');
const boards = readdirSync(HERE).filter((f) => f.endsWith('.json')).map((f) => JSON.parse(readFileSync(join(HERE, f), 'utf8')))
  .filter((b) => b.status === 'target').sort((a, b) => b.resolution.w - a.resolution.w || a.id.localeCompare(b.id));
const lines = boards.map((b) => `        - ${b.name} (${b.resolution.w} x ${b.resolution.h})`).join('\n');
let stale = 0;
for (const f of readdirSync(T).filter((x) => x.endsWith('.yml'))) {
  const p = join(T, f), s = readFileSync(p, 'utf8');
  const n = s.replace(/(\n *# boards:[^\n]*\n)[\s\S]*?(\n *# end boards)/, `$1${lines}$2`);
  if (n === s) continue;
  stale++;
  if (process.argv.includes('--check')) console.error(`${f}: board options out of date (node boards/templates.mjs)`);
  else writeFileSync(p, n);
}
process.exit(process.argv.includes('--check') && stale ? 1 : 0);
