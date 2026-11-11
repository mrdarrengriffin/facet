#!/usr/bin/env node
// A release's file changes, in one step (RELEASING.md): the version in every esphome/facet-<board>.yaml
// (facet_version; dashboard_import reads @v<version>), the release's Updates entry out of draft with today's date
// (site/src/content/updates/*.md whose title is "Facet <version>", a trailing ".0" dropped: 0.6.0 = "Facet 0.6",
// 0.6.1 = "Facet 0.6.1"), and the Updates page on (site/src/lib/nav.ts). A beta (0.6.0-beta.1) changes nothing: its
// YAMLs are stamped at build and it gets no Updates entry (its GitHub pre-release notes are enough).
//   node firmware/release-prep.mjs <version>        then review `git diff` and commit "Release: <version>"
// Prints what it changed; fails when a file is not as expected (nothing half done: every change is checked first).
import { readFileSync, readdirSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = dirname(dirname(fileURLToPath(import.meta.url)));
const V = process.argv[2];
if (!V || !/^\d+\.\d+\.\d+(-beta\.\d+)?$/.test(V)) { console.error('usage: node firmware/release-prep.mjs <version> (0.6.0, 0.6.1, 0.6.0-beta.1)'); process.exit(2); }
if (V.includes('-')) { console.log(`beta ${V}: nothing to change (its YAMLs are stamped at build; no Updates entry)`); process.exit(0); }
const edits = [];
const edit = (rel, f) => { const p = join(ROOT, rel), s = readFileSync(p, 'utf8'), t = f(s); if (t === null) { console.error(`${rel}: not as expected`); process.exit(1); } if (t !== s) edits.push([p, t, rel]); };

for (const y of readdirSync(join(ROOT, 'esphome')).filter((f) => /^facet-.*\.yaml$/.test(f)))
  edit(`esphome/${y}`, (s) => (/^( *facet_version: *)\S+/m.test(s) ? s.replace(/^( *facet_version: *)\S+/m, (_, a) => a + V) : null));

const mm = V.replace(/\.0$/, '');
const UP = 'site/src/content/updates';
const entry = readdirSync(join(ROOT, UP)).find((f) => f.endsWith('.md') && new RegExp(`^title: "Facet ${mm.replaceAll('.', '\\.')}"`, 'm').test(readFileSync(join(ROOT, UP, f), 'utf8')));
if (!entry) { console.error(`${UP}: no entry titled "Facet ${mm}": draft it first (${UP}/README.md)`); process.exit(1); }
const today = new Date().toISOString().slice(0, 10);
edit(`${UP}/${entry}`, (s) => (/^draft: (true|false)$/m.test(s) && /^date: "[\d-]+"$/m.test(s) ? s.replace(/^draft: true$/m, 'draft: false').replace(/^date: "[\d-]+"$/m, `date: "${today}"`) : null));

edit('site/src/lib/nav.ts', (s) => (/export const UPDATES_PAGE = (true|false);/.test(s) ? s.replace('export const UPDATES_PAGE = false;', 'export const UPDATES_PAGE = true;') : null));

for (const [p, t, rel] of edits) { writeFileSync(p, t); console.log(`changed ${rel}`); }
console.log(edits.length ? `release ${V}: ${edits.length} file(s) changed` : `release ${V}: nothing to change`);
