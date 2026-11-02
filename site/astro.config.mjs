// @ts-check
// Facet docs site (WASM), v0: static, generated from the renderer (scripts/extract.mjs runs first, see package.json).
// Where it is served is not decided yet: FACET_DOCS_BASE sets the path (default /facet-docs/ = the local preview
// preview), FACET_DOCS_OUT the output folder (default ./dist).
import { defineConfig } from 'astro/config';
import { facetCodeTheme } from './src/lib/code.ts';

export default defineConfig({
  site: 'https://mrdarrengriffin.github.io',
  base: process.env.FACET_DOCS_BASE ?? '/facet-docs/',
  outDir: process.env.FACET_DOCS_OUT ?? './dist',
  trailingSlash: 'always',
  // whitespace as written: the compressor has dropped the line break between text and an inline tag on the CI build
  // ("named<code>Facet-</code>" on the live Install page); scripts/check-dist.mjs fails the build on any such join
  compressHTML: false,
  build: { format: 'directory' },
  // markdown fences use the same token theme as src/components/CodeBlock.astro
  markdown: { shikiConfig: { theme: facetCodeTheme } },
});
