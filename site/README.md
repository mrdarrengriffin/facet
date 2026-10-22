# Facet docs site (WASM)

Astro site generated from the renderer: `npm run build` = `scripts/extract.mjs` (runs the WASM build in node: registry,
tokens CSS, icons, home previews, roadmap snapshot) + `astro build`. Every example is the live WASM renderer.

**Look (decided 2026-10-22):** the site keeps the original Facet site's own sizes (type, spacing, radii, from main
`site/src/styles`); colours, fonts and motion come from the WASM tokens (`src/data/tokens.css`, generated).

- Updates (timeline): one Markdown file per entry in `src/content/updates/` (format: README there), images in
  `public/updates/`.
- Roadmap: `src/roadmap.json`, a public snapshot refreshed by `scripts/extract.mjs` on the build box.
