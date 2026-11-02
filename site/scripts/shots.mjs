// Screen pictures for the site (npm run extract runs it after scripts/extract.mjs): every screen picture on the site is a
// PNG made here by the renderer itself, so a page loads no WASM until someone presses Try it (src/scripts/live.ts).
//   main     each screen with a page, as its page's demo starts (src/pages/screens/[id].astro): the docs build's module
//            in node, fed by the site's own sample Home Assistant (src/lib/sample-ha.ts sampleFeeder: the same values,
//            replies and site deck as the live demo), at 720 (1.5x) and 480 (1x), dark and light; full size for the
//            screen page and the homepage panel, half size (-half) for the grids (homepage, Screens overview)
//   states   each shipped screen in the states the release test checks (renderer/tests/functional/release_states.test.mjs
//            STATES: with data, loading, not found, unavailable, offline, on the functional tests' fake Home Assistant,
//            tools/lib/fakeha.mjs), half size; a state is left out when the screen looks the same as with data in it
//            (its texts, digits masked: Moon and Timer need no Home Assistant)
// Output: public/shots/*.png (palette PNG, sharp) + src/data/shots.json (the paths, sizes and state labels the pages use;
// a copy stays in public/shots/ with the pictures, so a cached folder alone, as CI restores it, is enough).
// Cache: public/shots/key.txt = a hash of the two docs builds' fw.wasm and every input here (this script, the sample HA,
// the test's states, the screens and the site deck); the same key = nothing rendered. FACET_SHOTS=fresh renders anyway.
import { createHash } from 'node:crypto';
import { copyFileSync, existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { performance } from 'node:perf_hooks';
import sharp from 'sharp';

const SITE = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const ROOT = resolve(SITE, '..');
const R = join(ROOT, 'renderer');
const OUT = join(SITE, 'public/shots');
const DATA = join(SITE, 'src/data/shots.json');
const DENS = [['1.5x', 720], ['1x', 480]];
const THEMES = ['dark', 'light'];
const t0 = performance.now();

// the docs build, as scripts/extract.mjs finds it: FW_OUT (env > local.env > renderer/out) + /docs
let fwOut = process.env.FW_OUT;
if (!fwOut && existsSync(join(ROOT, 'local.env'))) fwOut = readFileSync(join(ROOT, 'local.env'), 'utf8').match(/^FW_OUT=(.*)$/m)?.[1]?.trim();
const BUILD = join(resolve(ROOT, fwOut || 'renderer/out'), 'docs');

const rel = (p) => join(ROOT, p);
const INPUTS = [
  ...DENS.map(([d]) => join(BUILD, d, 'fw.wasm')), fileURLToPath(import.meta.url),
  rel('site/src/lib/sample-ha.ts'), rel('site/src/data/site-deck.json'),
  rel('renderer/tools/demo_ha.mjs'), rel('renderer/tools/demo_ha_examples.mjs'), rel('renderer/tools/demo_ha_replies.mjs'),
  rel('renderer/tools/lib/fwbench.mjs'), rel('renderer/tools/lib/fakeha.mjs'), rel('renderer/tests/functional/release_states.test.mjs'),
];
// the screens with a page, and only what a picture depends on (screens.json also carries git's status)
const screens = JSON.parse(readFileSync(join(SITE, 'src/data/screens.json'), 'utf8')).filter((s) => s.page);
const h = createHash('sha256');
for (const f of INPUTS) h.update(readFileSync(f));
h.update(JSON.stringify(screens.map((s) => [s.id, s.comp, s.sampleProps, s.frame])));
const KEY = h.digest('hex').slice(0, 16);
const KEYFILE = join(OUT, 'key.txt'), KEPT = join(OUT, 'shots.json');
if (process.env.FACET_SHOTS !== 'fresh' && existsSync(KEPT) && existsSync(KEYFILE) && readFileSync(KEYFILE, 'utf8').trim() === KEY) {
  mkdirSync(dirname(DATA), { recursive: true });
  copyFileSync(KEPT, DATA);
  console.log(`shots: cached (${KEY}), nothing rendered`);
  process.exit(0);
}

const { Bench, instance } = await import(pathToFileURL(join(R, 'tools/lib/fwbench.mjs')).href);
const { Ctx } = await import(pathToFileURL(join(R, 'tools/lib/fakeha.mjs')).href);
const RS = await import(pathToFileURL(join(R, 'tests/functional/release_states.test.mjs')).href);
const { sampleFeeder } = await import(pathToFileURL(join(SITE, 'src/lib/sample-ha.ts')).href);

rmSync(OUT, { recursive: true, force: true });
mkdirSync(OUT, { recursive: true });
let files = 0, bytes = 0;
/** an S x S RGBA frame -> public/shots/<name>.png at width w (palette PNG: the panel's flat colours, small) */
async function save(name, rgba, S, w) {
  let img = sharp(Buffer.from(rgba), { raw: { width: S, height: S, channels: 4 } });
  if (w !== S) img = img.resize(w, w, { kernel: 'lanczos3' });
  const buf = await img.png({ palette: true, quality: 95, effort: 10, compressionLevel: 9 }).toBuffer();
  writeFileSync(join(OUT, `${name}.png`), buf);
  files++, bytes += buf.length;
  return `shots/${name}.png`;
}
const frame = (m, st, S) => { m._fw_refresh(st); const p = m._fw_frame(st), px = m.HEAPU8.slice(p, p + S * S * 4); for (let i = 3; i < px.length; i += 4) px[i] = 255; return px; };

// ---- main: the screen page's demo (the same comp / props as [id].astro), on a fresh module per picture
const demoOf = (s) => (s.comp === 'spec' ? ['spec', { ...s.sampleProps, live: true }]
  : s.comp === 'home' || s.comp === 'settings' ? ['device', { start: s.comp }] : [s.comp, s.sampleProps]);
async function mainShot(s, den, S, theme) {
  const m = await instance(den, BUILD);
  const host = { m, str: (p) => m.UTF8ToString(p), withStr: (x, f) => { const p = m.stringToNewUTF8(x); try { return f(p); } finally { m._free(p); } } };
  const feed = sampleFeeder(host);
  const [comp, props] = demoOf(s);
  // a whole panel (the Device, a live spec stack) takes its look from the panel's Theme setting, as live.ts pushes it
  if (m._fw_setting) host.withStr('theme', (a) => host.withStr(theme, (b) => m._fw_setting(a, b)));
  const st = m._fw_stage(S, S);
  let t = 0;
  const tick = (n) => { for (let i = 0; i < n; i++) { t += 16; m._fw_tick(t); if (i % 16 === 0) feed?.tick(); m._fw_events(); } };
  tick(2);
  const err = host.withStr(comp, (c) => host.withStr(JSON.stringify(props), (p) => m.UTF8ToString(m._fw_render(st, c, p, theme === 'light' ? 1 : 0, -1))));
  if (err) throw new Error(`shots: ${s.id} ${den} ${theme}: ${err}`);
  tick(190);  // ~3 s: the sample values and replies in, entrance motion settled
  for (let k = 0; k < 120 && m._fw_anims(); k++) tick(4);
  const px = frame(m, st, S);
  feed?.stop();
  return px;
}

// ---- states: the release test's states on its fake HA (a fresh test context per state and theme)
const shape = (xs) => JSON.stringify([...new Set(xs.map((x) => x.replace(/\d+/g, '#')))].sort());
async function stateShot(id, st, den, light) {
  const b = new Bench(null, den, { build: BUILD }), t = new Ctx(b);
  const render = b.render.bind(b);
  b.render = (c, p) => render(c, p, light ? 1 : 0);
  try { await st.to(t, id); } finally { RS.realClock(); }
  return { px: frame(b.m, b.stage, b.S), texts: shape(t.texts()) };
}

const out = {};
for (const s of screens) {
  const rec = (out[s.id] = { main: {}, half: {}, states: [] });
  for (const [den, S] of DENS) {
    rec.main[S] = {}, rec.half[S] = {};
    for (const theme of THEMES) {
      const px = await mainShot(s, den, S, theme);
      rec.main[S][theme] = await save(`${s.id}-${S}-${theme}`, px, S, S);
      rec.half[S][theme] = await save(`${s.id}-${S}-${theme}-half`, px, S, S / 2);
    }
  }
  if (s.comp !== 'spec' || !RS.SHIPPED.includes(s.id)) continue;
  // which states it has: the ones that look different from its data (1.5x texts decide for both sizes)
  const data = (await stateShot(s.id, RS.STATES[0], '1.5x', false)).texts;
  for (const st of RS.STATES) {
    if (st.id !== 'data' && (await stateShot(s.id, st, '1.5x', false)).texts === data) continue;
    const srcs = {};
    for (const [den, S] of DENS) {
      srcs[S] = {};
      for (const theme of THEMES) srcs[S][theme] = await save(`${s.id}-state-${st.id}-${S}-${theme}`, (await stateShot(s.id, st, den, theme === 'light')).px, S, S / 2);
    }
    rec.states.push({ id: st.id, label: st.label, src: srcs });
  }
}
writeFileSync(DATA, JSON.stringify(out, null, 1) + '\n');
copyFileSync(DATA, KEPT);
writeFileSync(KEYFILE, KEY + '\n');
const sec = ((performance.now() - t0) / 1000).toFixed(1);
console.log(`shots: ${files} pictures, ${(bytes / 1024).toFixed(0)} KB, ${sec} s (${KEY})`);
