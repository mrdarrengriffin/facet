#!/usr/bin/env node
// Facet WASM workbench watcher: on a save under components/ core/ foundations/ screens/ tokens/ tools/ icons/ specs/
// sources/ themes/ it rebuilds the 720 density (1.5x) only; the workbench page polls the build manifest and re-renders
// by itself. The 480 density (1x) builds once the tree has been quiet for QUIET_MS, or at once on SIGUSR1
// (`systemctl kill --kill-whom=main -s USR1 claude-wasm-watch`). The docs site (../site, when present) is rebuilt when a file under
// site/src or specs/ changed, or on SIGUSR2; never after every build. Run: `node watch.mjs`. Ctrl-C to stop.
import { spawn } from 'node:child_process';
import { existsSync, renameSync, rmSync, watch } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
const QUIET_MS = 60_000;

// one build at a time; a save during a build queues one more of the same density, 1.5x always first
const want = new Set();
let running = false, timer = null, quiet = null, docsWanted = false;
function queue(den, why) {
  want.add(den);
  if (!running) next(why);
}
function next(why) {
  const den = want.has('1.5x') ? '1.5x' : want.has('1x') ? '1x' : null;
  if (!den) { if (docsWanted) { docsWanted = false; docs(); } return; }
  want.delete(den);
  running = true;
  const t0 = Date.now();
  console.log(`\n${new Date().toLocaleTimeString()} build ${den} (${why})`);
  const p = spawn('/bin/sh', ['-c', `LINKOPT=-O0 '${join(HERE, 'build.sh')}' ${den}`], { stdio: ['ignore', 'pipe', 'pipe'] });
  let err = '';
  p.stdout.on('data', (d) => process.stdout.write(d));
  p.stderr.on('data', (d) => { err += d; });
  p.on('close', (code) => {
    running = false;
    if (code === 0) console.log(`ok ${den} in ${((Date.now() - t0) / 1000).toFixed(1)} s${den === '1.5x' ? ': the page reloads by itself' : ''}`);
    else console.log(`FAILED ${den} (${code}):\n${err.split('\n').filter((l) => /error|undefined/i.test(l)).slice(0, 20).join('\n') || err.slice(-2000)}`);
    next(want.size ? 'changed during build' : why);
  });
}
// the 480 build waits for a quiet tree (each save restarts the wait)
function quietLater() {
  clearTimeout(quiet);
  quiet = setTimeout(() => queue('1x', 'quiet'), QUIET_MS);
}

// docs site: extract + astro build into dist.next, swapped in as dist; never while a WASM build runs
const SITE = join(HERE, '..', 'site');
let docsRunning = false;
const step = (args, env) => new Promise((res) => {
  const p = spawn(process.execPath, args, { cwd: SITE, env: { ...process.env, ...env }, stdio: ['ignore', 'pipe', 'pipe'] });
  let out = '';
  p.stdout.on('data', (d) => { out += d; });
  p.stderr.on('data', (d) => { out += d; });
  p.on('error', (e) => res({ code: -1, out: String(e) }));
  p.on('close', (code) => res({ code, out }));
});
async function docs() {
  if (!existsSync(join(SITE, 'node_modules', 'astro'))) return;
  if (running || docsRunning) { docsWanted = true; return; }
  docsRunning = true;
  const t0 = Date.now();
  try {
    const nx = join(SITE, 'dist.next'), dist = join(SITE, 'dist'), old = join(SITE, 'dist.old');
    let r = await step(['scripts/extract.mjs']);
    if (r.code === 0) r = await step(['node_modules/astro/bin/astro.mjs', 'build'], { FACET_DOCS_OUT: nx });
    if (r.code === 0) {
      rmSync(old, { recursive: true, force: true });
      if (existsSync(dist)) renameSync(dist, old);
      renameSync(nx, dist);
      rmSync(old, { recursive: true, force: true });
      console.log(`docs site rebuilt in ${((Date.now() - t0) / 1000).toFixed(1)} s`);
    } else console.log(`docs site build FAILED (${r.code}), WASM watch carries on:\n${r.out.split('\n').slice(-15).join('\n')}`);
  } catch (e) {
    console.log(`docs site build FAILED: ${e?.message ?? e}`);
  }
  docsRunning = false;
  if (docsWanted && !running) { docsWanted = false; docs(); }
}

const src = /\.(cpp|h|json|mjs|py|inc|svg)$/;
for (const d of ['components', 'foundations', 'screens', 'core', 'tokens', 'tools', 'icons', 'specs', 'sources', 'themes'])
  watch(join(HERE, d), { recursive: true }, (_e, f) => {
    if (!f || f.includes('node_modules') || !src.test(f)) return;
    if (d === 'specs') docsWanted = true;
    clearTimeout(timer);
    timer = setTimeout(() => { queue('1.5x', `${d}/${f}`); quietLater(); }, 150);
  });
if (existsSync(join(SITE, 'src')))  // not roadmap.json or data/: extract.mjs writes those
  watch(join(SITE, 'src'), { recursive: true }, (_e, f) => { if (f && f !== 'roadmap.json' && !f.startsWith('data')) { docsWanted = true; if (!running) next('site'); } });
process.on('SIGUSR1', () => { clearTimeout(quiet); queue('1x', 'on demand'); });
process.on('SIGUSR2', () => { docsWanted = true; if (!running) next('docs on demand'); });
console.log(`watching ${HERE}/{components,foundations,screens,core,tokens,tools,icons,specs,sources,themes} + site/src`);
queue('1.5x', 'start');
quietLater();
