// @ts-check
import { defineConfig } from 'astro/config';
import { unified } from '@astrojs/markdown-remark';
import { rehypeBaseLinks } from './src/plugins/rehype-base-links.mjs';

// Served from GitHub Pages under /facet/. If the site moves to its own domain, change `site`
// and set `base` to '/'.
const base = '/facet';

export default defineConfig({
  site: 'https://mrdarrengriffin.github.io',
  base,
  trailingSlash: 'always',
  markdown: {
    shikiConfig: {
      themes: { light: 'github-light', dark: 'github-dark' },
      defaultColor: false,
    },
    processor: unified({ rehypePlugins: [[rehypeBaseLinks, { base }]] }),
  },
});
