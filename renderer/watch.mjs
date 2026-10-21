#!/usr/bin/env node
// Facet WASM workbench watcher: rebuilds (build.sh, both densities, incremental) when a .cpp/.h in components/ or core/
// changes, or tokens/*.json (colour tokens -> generated header); the workbench page polls the build manifest and re-renders by itself. Run: `node watch.mjs`. Ctrl-C to stop.
// After every green build the docs site (../site, when present) is rebuilt too: extract + astro build into dist.next,
// then swapped in as dist (~5 s). Debounced, at most one queued; it never blocks or breaks the WASM watch (failures are logged).
import { spawn } from 'node:child_process';
import { existsSync, renameSync, rmSync, watch } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
let running = false, again = false, timer = null;

// docs site: one run at a time, at most one queued
const SITE = join(HERE, '..', 'site');
let docsRunning = false, docsAgain = false, docsTimer = null;
const step = (args, env) => new Promise((res) => {
  const p = spawn(process.execPath, args, { cwd: SITE, env: { ...process.env, ...env }, stdio: ['ignore', 'pipe', 'pipe'] });
  let out = '';
  p.stdout.on('data', (d) => { out += d; });
  p.stderr.on('data', (d) => { out += d; });
  p.on('error', (e) => res({ code: -1, out: String(e) }));
  p.on('close', (code) => res({ code, out }));
});
async function docs() {
  if (docsRunning) { docsAgain = true; return; }
  docsRunning = true;
  const t0 = Date.now();
  try {
    const next = join(SITE, 'dist.next'), dist = join(SITE, 'dist'), old = join(SITE, 'dist.old');
    let r = await step(['scripts/extract.mjs']);
    if (r.code === 0) r = await step(['node_modules/astro/bin/astro.mjs', 'build'], { FACET_DOCS_OUT: next });
    if (r.code === 0) {
      rmSync(old, { recursive: true, force: true });
      if (existsSync(dist)) renameSync(dist, old);
      renameSync(next, dist);
      rmSync(old, { recursive: true, force: true });
      console.log(`docs site rebuilt in ${((Date.now() - t0) / 1000).toFixed(1)} s`);
    } else console.log(`docs site build FAILED (${r.code}), WASM watch carries on:\n${r.out.split('\n').slice(-15).join('\n')}`);
  } catch (e) {
    console.log(`docs site build FAILED: ${e?.message ?? e}`);
  }
  docsRunning = false;
  if (docsAgain) { docsAgain = false; docs(); }
}
function docsSoon() {
  if (!existsSync(join(SITE, 'node_modules', 'astro'))) return;
  clearTimeout(docsTimer);
  docsTimer = setTimeout(docs, 1000);
}
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
    if (code === 0) { console.log(`ok in ${((Date.now() - t0) / 1000).toFixed(1)} s: the page reloads by itself`); docsSoon(); }
    else console.log(`FAILED (${code}):\n${err.split('\n').filter((l) => /error|undefined/i.test(l)).slice(0, 20).join('\n') || err.slice(-2000)}`);
    if (again) { again = false; build('changed during build'); }
  });
}
for (const d of ['components', 'foundations', 'core', 'tokens', 'tools', 'icons'])
  watch(join(HERE, d), { recursive: true }, (_e, f) => {
    if (!f || f.includes('node_modules') || !/\.(cpp|h|json|mjs|py|inc|svg)$/.test(f)) return;
    clearTimeout(timer);
    timer = setTimeout(() => build(`${d}/${f}`), 150);
  });
console.log(`watching ${HERE}/{components,foundations,core,tokens,tools,icons}`);
build('start');
