// @ts-check
// The Facet workbench (README.md). `./run.sh` or `npm start`: astro dev on 0.0.0.0:4322. Server routes (/api/*) read
// the repo this folder sits in; the renderer build comes from $FW_OUT (lib/repo.ts) and is served at /facet-wasm/.
import { defineConfig } from 'astro/config';
import node from '@astrojs/node';
import { fileURLToPath } from 'node:url';

export default defineConfig({
  output: 'server',
  adapter: node({ mode: 'standalone' }),
  devToolbar: { enabled: false },
  server: { host: '0.0.0.0', port: Number(process.env.PORT ?? 4322), allowedHosts: true },
  vite: {
    // the repo (the renderer's tools and the docs site's synth are imported from outside this folder)
    server: { fs: { allow: [fileURLToPath(new URL('..', import.meta.url))] }, watch: { ignored: ['**/renderer/.cache/**', '**/renderer/out/**'] } },
  },
});
