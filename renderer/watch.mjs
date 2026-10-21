#!/usr/bin/env node
// Facet WASM workbench watcher: rebuilds (build.sh, both densities, incremental) when a .cpp/.h in components/ or core/
// changes, or tokens/*.json (colour tokens -> generated header); the workbench page polls the build manifest and re-renders by itself. Run: `node watch.mjs`. Ctrl-C to stop.
import { spawn } from 'node:child_process';
import { watch } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
let running = false, again = false, timer = null;
function build(why) {
  if (running) { again = true; return; }
  running = true;
  const t0 = Date.now();
  console.log(`\n${new Date().toLocaleTimeString()} build (${why})`);
  const p = spawn(join(HERE, 'build.sh'), { stdio: ['ignore', 'pipe', 'pipe'] });
  let err = '';
  p.stdout.on('data', (d) => process.stdout.write(d));
  p.stderr.on('data', (d) => { err += d; });
  p.on('close', (code) => {
    running = false;
    if (code === 0) console.log(`ok in ${((Date.now() - t0) / 1000).toFixed(1)} s: the page reloads by itself`);
    else console.log(`FAILED (${code}):\n${err.split('\n').filter((l) => /error|undefined/i.test(l)).slice(0, 20).join('\n') || err.slice(-2000)}`);
    if (again) { again = false; build('changed during build'); }
  });
}
for (const d of ['components', 'foundations', 'core', 'tokens'])
  watch(join(HERE, d), { recursive: true }, (_e, f) => {
    if (!f || !/\.(cpp|h|json)$/.test(f)) return;
    clearTimeout(timer);
    timer = setTimeout(() => build(`${d}/${f}`), 150);
  });
console.log(`watching ${HERE}/{components,foundations,core,tokens}`);
build('start');
