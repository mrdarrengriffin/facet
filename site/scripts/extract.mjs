// Facet docs site: metadata extraction (run before every site build: `npm run extract`).
//
// Everything the site shows comes from what the renderer already declares; nothing is hand-written per page:
//   - the WASM build itself (fw.mjs runs in node: build.sh links with -sENVIRONMENT=web,node): fw_meta() = every
//     registered FW_COMPONENT (id, name, section, meta JSON: frame, props, variants), fw_tokens() = the colour tokens
//   - the .cpp sources: the header comment (notes), fw::emit / fw::sound calls (events + sounds; the fw::st:: names
//     resolved through tokens/states.json, the step ticks of core fw::step_ticks), FW_COMPONENT ids -> file
//   - README.md: the "Components" table row per id (description)
//   - tokens/*.json: colour (site theme + colour table), sound (sound names), each foundation's own token file
//   - git: WIP / Done / Done, edited per source file (same rule as the workbench's /api/wasm/status)
// Output: src/data/{site,components,foundations,tokens}.json + public/wasm/<density>/ (a copy of the build, so the
// site is self-contained wherever it is deployed).
//
// Build location: $FW_OUT (env, else local.env at the repo root, else renderer/out), densities 1.5x and 1x.
import { execFileSync } from 'node:child_process';
import { copyFileSync, existsSync, mkdirSync, readdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const SITE = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const ROOT = resolve(SITE, '..');
const R = join(ROOT, 'renderer');
const REPO = 'https://github.com/mrdarrengriffin/facet/blob/wasm';
const DENS = ['1.5x', '1x'];
// meta "about" sentences are drafts (shown with a "draft" tag) until Darren's writing pass; set false then
const ABOUT_DRAFT = true;

// FW_OUT: env > local.env > renderer/out
let fwOut = process.env.FW_OUT;
if (!fwOut && existsSync(join(ROOT, 'local.env'))) fwOut = readFileSync(join(ROOT, 'local.env'), 'utf8').match(/^FW_OUT=(.*)$/m)?.[1]?.trim();
fwOut = resolve(ROOT, fwOut || 'renderer/out');
for (const d of DENS) if (!existsSync(join(fwOut, d, 'fw.mjs'))) { console.error(`extract: no build in ${fwOut}/${d} (run renderer/build.sh)`); process.exit(1); }

// 1. the WASM registry, read by running the build in node
const base = join(fwOut, '1.5x') + '/';
const { default: create } = await import(pathToFileURL(base + 'fw.mjs').href);
const m = await create({ locateFile: (f) => base + f, print: () => {}, printErr: () => {} });
m._fw_init();
const reg = JSON.parse(m.UTF8ToString(m._fw_meta()));
const fwTokens = JSON.parse(m.UTF8ToString(m._fw_tokens()));

// 2. sources: id -> file, header comment, events, sounds
const json = (p) => JSON.parse(readFileSync(join(R, p), 'utf8'));
const states = json('tokens/states.json').states;
const ST = {
  TAP_EVENT: states.rest.event, TAP_SOUND: states.rest.sound,
  BLOCKED_EVENT: states.disabled.event, BLOCKED_SOUND: states.disabled.sound,
  SWITCH_EVENT: states.on.event, ON_SOUND: states.on.sound, OFF_SOUND: states.on.off_sound,
};
const soundTok = json('tokens/sound.json');
const soundName = Object.fromEntries((soundTok.types ?? []).map((t) => [t.id, { name: t.name, for: t.for }]));

const git = (...a) => { try { return execFileSync('git', ['-C', ROOT, ...a], { encoding: 'utf8' }); } catch { return ''; } };
const tracked = new Set(git('ls-files', '--', 'renderer').split('\n').filter(Boolean));
const changed = new Set(git('status', '--porcelain', '--untracked-files=all', '--', 'renderer').split('\n').filter(Boolean).map((l) => l.slice(3)));
const one = (rel) => (!tracked.has(rel) ? 'wip' : changed.has(rel) ? 'edited' : 'done');
const combine = (xs) => (xs.includes('wip') ? 'wip' : xs.includes('edited') ? 'edited' : 'done');

/** the leading // comment block of a source file, as paragraphs */
function headerComment(src) {
  const lines = [];
  for (const l of src.split('\n')) { if (!l.startsWith('//')) break; lines.push(l.replace(/^\/\/ ?/, '')); }
  return lines.join('\n').trim();
}
/** string-literal or fw::st:: names used as the 2nd argument of fn( */
function calls(src, fn) {
  const out = new Set();
  const re = new RegExp(`${fn}\\(\\s*[^,]+,\\s*([^,)]+(?:\\?[^:]+:[^,)]+)?)`, 'g');
  for (const mm of src.matchAll(re)) {
    const arg = mm[1];
    for (const s of arg.matchAll(/"([^"]+)"/g)) out.add(s[1]);
    for (const s of arg.matchAll(/fw::st::([A-Z_]+)|\bst::([A-Z_]+)/g)) { const v = ST[s[1] ?? s[2]]; if (v) out.add(v); }
  }
  return out;
}
const files = {};
for (const dir of ['components', 'foundations']) {
  for (const f of readdirSync(join(R, dir)).filter((f) => f.endsWith('.cpp'))) {
    const rel = `renderer/${dir}/${f}`, src = readFileSync(join(ROOT, rel), 'utf8');
    const rels = [rel];
    const tok = `renderer/tokens/${f.replace(/\.cpp$/, '.json')}`;
    if (dir === 'foundations' && existsSync(join(ROOT, tok))) rels.push(tok);
    const events = calls(src, 'fw::emit'), sounds = calls(src, 'fw::sound');
    if (/st::blocked\(/.test(src)) { events.add(ST.BLOCKED_EVENT); sounds.add(ST.BLOCKED_SOUND); }
    if (/fw::step_ticks\(/.test(src)) { sounds.add('slider-tick'); sounds.add('slider-landmark'); }
    const uses = [...new Set([...src.matchAll(/fw::find\("([a-z0-9_]+)"\)/g)].map((x) => x[1]))];
    const info = { uses, files: rels, status: combine(rels.map(one)), notes: headerComment(src), events: [...events], sounds: [...sounds] };
    for (const mm of src.matchAll(/FW_COMPONENT\(\s*([A-Za-z0-9_]+)/g)) files[mm[1]] = info;
  }
}

// 3. README "Components" rows: | `id` | description |
const readme = readFileSync(join(ROOT, 'README.md'), 'utf8');
const rows = {};
for (const mm of readme.matchAll(/^\| `([a-z0-9_]+)` \| (.+) \|$/gm)) rows[mm[1]] = mm[2];

// 4. one record per registered entry
/** drop "(docs ...)" / "(see ...)" references, parentheses balanced */
function stripRefs(s) {
  let out = '', i = 0;
  while (i < s.length) {
    const m = /\s*\((?:docs|see)\b/.exec(s.slice(i));
    if (!m) { out += s.slice(i); break; }
    out += s.slice(i, i + m.index);
    let j = i + m.index + m[0].length, depth = 1;
    while (j < s.length && depth) { if (s[j] === '(') depth++; else if (s[j] === ')') depth--; j++; }
    i = j;
  }
  return out;
}
// first clause of the README row / header comment: "Facet text button"
const lede = (s) => {
  const t = stripRefs((s ?? '').replace(/\s*\n\s*/g, ' ')).split(/[:;.]\s/)[0].replace(/,\s*WIP$/, '').replace(/`/g, '').trim();
  if (!t) return t;
  const u = t[0].toUpperCase() + t.slice(1);
  return u.length > 160 ? u.slice(0, 157) + '...' : u;
};
const entries = reg.map((c) => {
  const f = files[c.id] ?? { uses: [], files: [], status: 'wip', notes: '', events: [], sounds: [] };
  const desc = rows[c.id] ?? '';
  return {
    id: c.id, name: c.name, section: c.section, meta: c.meta,
    // public description: meta "about" (drafts until Darren's writing pass: ABOUT_DRAFT), else the first clause of the README row / header comment
    lede: c.meta.about || lede(desc || f.notes.replace(/^[^:]+:\s*/, '')),
    ledeKind: c.meta.about ? (ABOUT_DRAFT ? 'draft' : 'about') : 'fallback',
    description: desc,
    notes: f.notes,
    events: f.events,
    uses: f.uses.filter((u) => u !== c.id),
    sounds: f.sounds.map((id) => ({ id, ...(soundName[id] ?? { name: id, for: '' }) })),
    status: f.status,
    source: f.files.map((p) => ({ path: p, url: `${REPO}/${p}` })),
  };
});
const comps = entries.filter((e) => e.section === 'components');
const founds = entries.filter((e) => e.section === 'foundations');
const others = entries.filter((e) => e.section !== 'components' && e.section !== 'foundations');
if (others.length) console.warn(`extract: sections not on the site yet: ${[...new Set(others.map((e) => e.section))].join(', ')}`);

// 5. tokens for the site itself (theme CSS + colour table)
const colour = json('tokens/colour.json');
const tokens = { colour: { groups: colour.groups, colours: colour.colours, accent: colour.accent }, sets: fwTokens.accent?.sets ?? [] };

const manifests = Object.fromEntries(DENS.map((d) => [d, JSON.parse(readFileSync(join(fwOut, d, 'manifest.json'), 'utf8'))]));
const motion = json('tokens/motion.json');
const site = {
  // a demo sample returns to its initial state this long after the last interaction (motion.json durations.demo-reset)
  demoResetMs: motion.durations?.['demo-reset']?.ms ?? 1800,
  built: new Date().toISOString(),
  commit: git('rev-parse', '--short', 'HEAD').trim(),
  repo: REPO,
  wasm: manifests,
};

const out = join(SITE, 'src/data');
mkdirSync(out, { recursive: true });
const w = (n, v) => writeFileSync(join(out, n), JSON.stringify(v, null, 1) + '\n');
w('site.json', site);
w('components.json', comps);
w('foundations.json', founds);
w('tokens.json', tokens);
// sound: the packs for the header picker; the whole sound.json is copied to public/ (fetched on the first sound)
w('sound-packs.json', { default: soundTok.default, variants: soundTok.variants.map((v) => ({ id: v.id, name: v.name, about: v.about })) });
mkdirSync(join(SITE, 'public'), { recursive: true });
copyFileSync(join(R, 'tokens/sound.json'), join(SITE, 'public/sound.json'));

// 6. copy the build into public/wasm/<density>/
for (const d of DENS) {
  const to = join(SITE, 'public/wasm', d);
  mkdirSync(to, { recursive: true });
  for (const f of ['fw.mjs', 'fw.wasm', 'manifest.json']) copyFileSync(join(fwOut, d, f), join(to, f));
}
console.log(`extract: ${comps.length} components, ${founds.length} foundations, build ${manifests['1.5x'].built} (${fwOut})`);
