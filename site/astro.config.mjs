// @ts-check
// Facet docs site (WASM), v0: static, generated from the renderer (scripts/extract.mjs runs first, see package.json).
// Where it is served is not decided yet: FACET_DOCS_BASE sets the path (default /facet-docs/ = the local preview
// through the hub), FACET_DOCS_OUT the output folder (default ./dist).
import { defineConfig } from 'astro/config';

export default defineConfig({
  site: 'https://mrdarrengriffin.github.io',
  base: process.env.FACET_DOCS_BASE ?? '/facet-docs/',
  outDir: process.env.FACET_DOCS_OUT ?? './dist',
  trailingSlash: 'always',
  build: { format: 'directory' },
});
