// Agent-readable docs (npm run build runs it after astro build, before scripts/check-dist.mjs), from the built pages:
//   <page>.md       every page as plain Markdown at its own path with .md (scripts/md.mjs: its own HTML, no navigation)
//   llms.txt        what Facet is, where to start (the spec format, the schema, the components), then every page with
//                   its one-line description, in the sidebar's order (read from a built docs page), the rest after
//   llms-full.txt   every page's Markdown in that same order, each headed by its address
//   agents.md       the agent guide a panel serves (renderer/tools/gen_agents.mjs, the docs build's features): the whole
//                   spec format, every component's props and the actions each of its events takes
// The schemas are the site's /schema.json and /spec.schema.json (src/pages/*.json.ts). Usage: node scripts/llms.mjs [dist]
import { execFileSync } from 'node:child_process';
import { readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { docPages, mdPath, pageInfo, toMarkdown } from './md.mjs';

const DIST = process.argv[2] ?? process.env.FACET_DOCS_OUT ?? './dist';
const BASE = process.env.FACET_DOCS_BASE ?? '/facet-docs/';
const at = (p) => BASE.slice(0, -1) + p;
const file = (p) => join(DIST, p === '/' ? 'index.html' : `${p}index.html`);
const pages = docPages(DIST);
const info = Object.fromEntries(pages.map((p) => [p, pageInfo(readFileSync(file(p), 'utf8'))]));

// the order: the docs sidebar's groups as a built page shows them, then the pages it does not list, by section
const side = readFileSync(file('/screens/'), 'utf8').match(/<nav aria-label="Docs">([\s\S]*?)<\/nav>/)?.[1] ?? '';
const groups = [];
const named = {};  // a page's name in the sidebar (two pages may share a title: Components, Component library)
for (const g of side.split('<section').slice(1)) {
  const label = g.match(/<h2[^>]*>([^<]*)<\/h2>/)?.[1]?.trim();
  const links = [...g.matchAll(/<a href="([^"]+)"[^>]*>([\s\S]*?)<\/a>/g)].map((m) => [('/' + m[1].slice(BASE.length)), m[2].replace(/<[^>]+>/g, '').trim()]).filter(([p]) => info[p])
    .map(([p, name]) => { if (name !== 'Overview') named[p] ??= name; return p; });
  if (label && links.length) groups.push({ label, pages: links });
}
const listed = new Set(groups.flatMap((g) => g.pages));
const rest = (label, test) => { const ps = pages.filter((p) => !listed.has(p) && test(p)); ps.forEach((p) => listed.add(p)); if (ps.length) groups.push({ label, pages: ps }); };
rest('Components', (p) => p.startsWith('/components/'));
rest('Foundations', (p) => p.startsWith('/foundations/'));
rest('Screens made with AI', (p) => p.startsWith('/ai/'));
groups.unshift({ label: 'Facet', pages: ['/'].filter((p) => info[p]) });
listed.add('/');
rest('More pages', () => true);
const order = groups.flatMap((g) => g.pages);

// the agent guide, as the docs build's firmware would serve it
const gen = fileURLToPath(new URL('../../renderer/tools/gen_agents.mjs', import.meta.url));
writeFileSync(join(DIST, 'agents.md'), execFileSync('node', [gen, '--flavour', 'docs', '--print', 'agents.md'], { encoding: 'utf8' }));

// the Markdown twins
const md = {};
for (const p of order) {
  md[p] = toMarkdown(readFileSync(file(p), 'utf8'), BASE, pages);
  writeFileSync(join(DIST, mdPath(p)), md[p]);
}

const line = (p) => `- [${named[p] ?? info[p].title}](${at(mdPath(p))})${info[p].description ? `: ${info[p].description}` : ''}`;
const llms = `# Facet

> Facet is open-source firmware for 4 inch ESP32 wall panels that shows and controls your home through Home Assistant.
> Its screens are JSON specs built from a fixed set of components; one C++/LVGL renderer draws them on the panel and on this site.

Every page below is plain Markdown (the page's address with .md); [llms-full.txt](${at('/llms-full.txt')}) is all of them in one file.

## Write a screen

- [Agent guide](${at('/agents.md')}): the whole spec format in one file, as a panel serves it: bindings, actions, limits, every component with its props and the actions its events take. Start here.
- [Spec format](${at(mdPath('/inside/spec/'))}): how a screen spec is written: its fields, layouts, bindings, actions and limits.
- [Schema](${at('/schema.json')}): the machine-readable schema (JSON): every component and its props, the screens, tokens, settings and the panel's HTTP routes.
- [Spec schema](${at('/spec.schema.json')}): the JSON Schema a screen spec must pass.
- [Component library](${at(mdPath('/components/'))}): every component a spec can use, each with its props.
- [Make screens with AI](${at(mdPath('/ai/'))}): how to send a spec to a panel, with worked examples.

${groups.map((g) => `## ${g.label}\n\n${g.pages.map(line).join('\n')}`).join('\n\n')}
`;
writeFileSync(join(DIST, 'llms.txt'), llms);
writeFileSync(join(DIST, 'llms-full.txt'), `# Facet: every page\n\n${order.map((p) => `<!-- page: ${at(p)} (${at(mdPath(p))}) -->\n\n${md[p]}`).join('\n---\n\n')}`);
console.log(`llms: ${order.length} pages as Markdown, llms.txt, llms-full.txt (${(Buffer.byteLength(Object.values(md).join('')) / 1024).toFixed(0)} KB)`);
