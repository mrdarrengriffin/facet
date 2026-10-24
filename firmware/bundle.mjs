#!/usr/bin/env node
// Device bundle (README "Device firmware"): turns renderer/ into an ESPHome external component `facet_wb` so a
// panel firmware runs the SAME core + foundations + components as the WASM workbench.
//   node firmware/bundle.mjs --density 1x|1.5x --out <dir>   write <dir>/facet_wb/ (flat: ESPHome compiles only the
//                                                          component's top-level files)
//   node firmware/bundle.mjs --density 1x|1.5x --hashes      print the source hashes as JSON (no files written)
// Hashes (sha1, 12 hex) let the workbench tell whether a device runs the current code: `core` covers everything every
// item's build depends on (core/*.h, fw_core.cpp, fonts.cpp + the density's font header, tokens/*.json, the colour
// generator), `files` one hash per foundations/components source (an item = its file + core), `glue` the device host
// code (informational only). The firmware embeds the hashes it
// was built from (fw_bundle_gen.h, GET /fw/info); same function here and in the hub (it runs --hashes).
// Fonts: fonts.cpp includes this density's generated font header (tools/gen_fonts.py from tokens/type.json; needs
// FW_FONT_PY + FW_TTF, see local.env); the bundle inlines it into fonts.cpp, because ESPHome #includes every header of a
// component into main.cpp.
// Static registration: ESP-IDF links the app as an archive, so an object nobody references is dropped (and its
// FW_COMPONENT registrations with it): each copied source gets an `fwlink_<n>()` and fw_bundle_gen.h calls them all.
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, readdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { basename, dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

// ROOT = repo root (local.env, firmware/ glue); HERE = renderer/ (sources; hash keys are relative to it: core/fw.h, ...)
const ROOT = dirname(dirname(fileURLToPath(import.meta.url)));
const HERE = join(ROOT, 'renderer');
const arg = (k) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : undefined; };
const density = arg('--density') ?? '1.5x';
if (!['1x', '1.5x'].includes(density)) { console.error('--density 1x|1.5x'); process.exit(2); }
const env = {};
if (existsSync(join(ROOT, 'local.env')))
  for (const l of readFileSync(join(ROOT, 'local.env'), 'utf8').split('\n')) { const m = l.match(/^([A-Z_]+)=(.*)$/); if (m) env[m[1]] = m[2]; }

const sha = (...bufs) => { const h = createHash('sha1'); for (const b of bufs) h.update(b); return h.digest('hex').slice(0, 12); };
const ls = (d, ext) => (existsSync(join(HERE, d)) ? readdirSync(join(HERE, d)).filter((f) => f.endsWith(ext)).sort().map((f) => `${d}/${f}`) : []);
// core = what every item's pixels depend on (compiled into each item's build); glue = the device-side host code
// (HTTP, screen setup), reported separately: a glue change does not make an item show old code
const lsr = (d) => (existsSync(join(HERE, d)) ? readdirSync(join(HERE, d), { recursive: true }).filter((f) => f.endsWith('.svg')).sort().map((f) => `${d}/${f}`) : []);
// core .cpp files the list below doesn't name (sport.cpp, ...): bundled as they are (not main.cpp / bench.cpp: WASM host only)
const coreExtra = ls('core', '.cpp').filter((f) => !['core/fw_core.cpp', 'core/motion.cpp', 'core/atoms.cpp', 'core/states.cpp', 'core/mascot.cpp', 'core/device.cpp', 'core/fonts.cpp', 'core/icons.cpp', 'core/main.cpp', 'core/bench.cpp'].includes(f));
const coreFiles = [...ls('core', '.h'), ...coreExtra, 'core/fw_core.cpp', 'core/motion.cpp', 'core/atoms.cpp', 'core/states.cpp', 'core/mascot.cpp', 'core/device.cpp', 'core/fonts.cpp', 'core/icons.cpp', ...ls('tokens', '.json'), 'tools/gen_colour.mjs', 'tools/gen_type.mjs', 'tools/gen_space.mjs', 'tools/gen_layout.mjs', 'tools/gen_motion.mjs', 'tools/gen_states.mjs', 'tools/gen_mascot.mjs', 'tools/gen_chart.mjs', 'tools/gen_surfaces.mjs', 'tools/gen_floorplan.mjs', 'tools/gen_settings.mjs', 'tools/gen_specs.mjs', ...ls('specs', '.json'), 'sources/sources.json', 'tools/gen_fonts.py', 'tools/facet_font.inc', 'tools/gen_icons.mjs', ...lsr('icons')];
const glueFiles = ['firmware/fw_device.h', 'firmware/fw_bench.h', 'firmware/esphome_init.py', 'firmware/bundle.mjs'];  // relative to ROOT
const srcFiles = [...ls('foundations', '.cpp'), ...ls('components', '.cpp'), ...ls('screens', '.cpp'), ...ls('bench', '.cpp')];  // bench = the stress item (facet-153)
const read = (rel) => readFileSync(join(HERE, rel));
const core = sha(...coreFiles.flatMap((f) => [f, '\0', read(f), '\0']));
// per-file hashes of the core too, so a host can say WHICH file changed
const corefiles = Object.fromEntries(coreFiles.map((f) => [f, sha(read(f))]));
const glue = sha(...glueFiles.flatMap((f) => [f, '\0', readFileSync(join(ROOT, f)), '\0']));
const files = Object.fromEntries(srcFiles.map((f) => [f, sha(read(f))]));
const ids = {};
for (const f of srcFiles) for (const m of read(f).toString().matchAll(/FW_COMPONENT\(\s*([A-Za-z0-9_]+)/g)) ids[m[1]] = f;
const hashes = { v: 1, density, core, corefiles, glue, files, ids };

if (process.argv.includes('--hashes')) { process.stdout.write(JSON.stringify(hashes)); process.exit(0); }
const out = arg('--out');
if (!out) { console.error('--out <dir> or --hashes'); process.exit(2); }
const C = join(out, 'facet_wb');
mkdirSync(C, { recursive: true });
// write only on change (an unchanged file keeps its mtime = no recompile); files no longer produced are removed
const wrote = new Set();
const put = (name, text) => {
  wrote.add(name);
  const p = join(C, name);
  if (!existsSync(p) || readFileSync(p, 'utf8') !== String(text)) writeFileSync(p, text);
};
put('__init__.py', readFileSync(join(ROOT, 'firmware/esphome_init.py')));
for (const f of ls('core', '.h')) put(basename(f), read(f));
put('fw_device.h', readFileSync(join(ROOT, 'firmware/fw_device.h')));
put('fw_bench.h', readFileSync(join(ROOT, 'firmware/fw_bench.h')));
execFileSync('node', [join(HERE, 'tools/gen_colour.mjs'), join(C, 'colour_gen.h')]);  // rewrites only on change
wrote.add('colour_gen.h');
execFileSync('node', [join(HERE, 'tools/gen_type.mjs'), join(C, 'type_gen.h')]);
wrote.add('type_gen.h');
execFileSync('node', [join(HERE, 'tools/gen_space.mjs'), join(C, 'space_gen.h')]);
wrote.add('space_gen.h');
execFileSync('node', [join(HERE, 'tools/gen_layout.mjs'), join(C, 'layout_gen.h')]);
wrote.add('layout_gen.h');
execFileSync('node', [join(HERE, 'tools/gen_motion.mjs'), join(C, 'motion_gen.h')]);
wrote.add('motion_gen.h');
execFileSync('node', [join(HERE, 'tools/gen_states.mjs'), join(C, 'states_gen.h')]);
wrote.add('states_gen.h');
execFileSync('node', [join(HERE, 'tools/gen_mascot.mjs'), join(C, 'mascot_gen.h')]);
wrote.add('mascot_gen.h');
execFileSync('node', [join(HERE, 'tools/gen_chart.mjs'), join(C, 'chart_gen.h')]);
wrote.add('chart_gen.h');
execFileSync('node', [join(HERE, 'tools/gen_surfaces.mjs'), join(C, 'surfaces_gen.h')]);
wrote.add('surfaces_gen.h');
execFileSync('node', [join(HERE, 'tools/gen_floorplan.mjs'), join(C, 'floorplan_gen.h')]);
wrote.add('floorplan_gen.h');
execFileSync('node', [join(HERE, 'tools/gen_settings.mjs'), join(C, 'settings_gen.h')]);
wrote.add('settings_gen.h');
execFileSync('node', [join(HERE, 'tools/gen_specs.mjs'), join(C, 'specs_gen.h')]);  // factory screen specs (core/spec.h)
wrote.add('specs_gen.h');
execFileSync('node', [join(HERE, 'tools/gen_icons.mjs'), '--meta', join(C, 'icons_gen.h')]);
wrote.add('icons_gen.h');
// this density's fonts (cached next to the WASM build's; the generator skips work when type.json is unchanged)
const FONT_PY = process.env.FW_FONT_PY ?? env.FW_FONT_PY, FW_TTF = process.env.FW_TTF ?? env.FW_TTF;
if (!FONT_PY || !FW_TTF) { console.error('set FW_FONT_PY and FW_TTF (env or local.env)'); process.exit(2); }
const fontDir = join(process.env.FW_CACHE ?? env.FW_CACHE ?? join(HERE, '.cache'), 'gen');
mkdirSync(fontDir, { recursive: true });
const fontH = join(fontDir, density === '1x' ? 'facet_fonts_1x.h' : 'facet_fonts.h');
execFileSync(FONT_PY, [join(HERE, 'tools/gen_fonts.py'), density, fontH], { env: { ...process.env, FW_TTF }, stdio: ['ignore', 'ignore', 'inherit'] });
put('fw_core.cpp', read('core/fw_core.cpp'));
put('motion.cpp', read('core/motion.cpp'));
put('atoms.cpp', read('core/atoms.cpp'));
put('states.cpp', read('core/states.cpp'));
put('mascot.cpp', read('core/mascot.cpp'));
put('device.cpp', read('core/device.cpp'));  // the device runtime (screen stack, navigation)
for (const f of coreExtra) put(basename(f), read(f));
// fonts.cpp with this density's font header inlined (and the #ifdef picking it)
const fonts = read('core/fonts.cpp').toString().replace(/#ifdef FW_1X\n#include "facet_fonts_1x.h"\n#else\n#include "facet_fonts.h"\n#endif\n/,
  `// ---- inlined ${basename(fontH)} (firmware/bundle.mjs)\n${readFileSync(fontH, 'utf8').replace('#pragma once', '')}\n// ---- end of ${basename(fontH)}\n`);
if (fonts.includes('#include "facet_fonts')) { console.error('fonts.cpp: font include not found where expected'); process.exit(1); }
const links = [];
const link = (name, text) => { links.push(name); return `${text}\n// firmware/bundle.mjs: keeps this object (and its registrations) in the link\nextern "C" void ${name}() {}\n`; };
put('fonts.cpp', link('fwlink_fonts', fonts));
// icons.cpp with this density's A8 masks inlined (tools/gen_icons.mjs; FW_ICONS=used = only what components declare)
const iconsH = join(fontDir, density === '1x' ? 'facet_icons_1x.h' : 'facet_icons.h');
execFileSync('node', [join(HERE, 'tools/gen_icons.mjs'), '--data', density, iconsH], { env: { ...process.env, FW_ICONS: process.env.FW_ICONS ?? env.FW_ICONS ?? 'all' }, stdio: ['ignore', 'ignore', 'inherit'] });
const icons = read('core/icons.cpp').toString().replace(/#ifdef FW_1X\n#include "facet_icons_1x.h"\n#else\n#include "facet_icons.h"\n#endif\n/,
  `// ---- inlined ${basename(iconsH)} (firmware/bundle.mjs)\n${readFileSync(iconsH, 'utf8').replace('#pragma once', '')}\n// ---- end of ${basename(iconsH)}\n`);
if (icons.includes('#include "facet_icons')) { console.error('icons.cpp: icon include not found where expected'); process.exit(1); }
put('icons.cpp', link('fwlink_icons', icons));
for (const f of srcFiles) {
  const n = `${f.startsWith('foundations') ? 'f' : f.startsWith('screens') ? 's' : 'c'}_${basename(f, '.cpp')}`;
  put(`${n}.cpp`, link(`fwlink_${n}`, read(f).toString()));
}
// `built` = when these hashes were first bundled (kept while the sources are unchanged, so main.cpp, which includes
// this header, only recompiles when something changed)
let built = new Date().toISOString();
try {
  const prev = JSON.parse(readFileSync(join(C, 'fw_bundle_gen.h'), 'utf8').match(/R"__fwb__\((.*)\)__fwb__"/)[1]);
  if (prev.core === core && prev.glue === glue && JSON.stringify(prev.files) === JSON.stringify(files) && JSON.stringify(prev.ids) === JSON.stringify(ids)) built = prev.built;
} catch { /* first bundle */ }
const json = JSON.stringify({ ...hashes, built });
put('fw_bundle_gen.h', `// GENERATED by firmware/bundle.mjs: what this firmware was built from. Do not edit.
#pragma once
extern "C" {
${links.map((l) => `void ${l}();`).join('\n')}
}
namespace fw::bundle {
inline void link_all() {
${links.map((l) => `  ${l}();`).join('\n')}
}
constexpr const char *INFO = R"__fwb__(${json})__fwb__";
}  // namespace fw::bundle
`);
for (const f of readdirSync(C)) if (!wrote.has(f) && !f.startsWith('__pycache__')) rmSync(join(C, f), { recursive: true, force: true });
console.log(`bundle ${density}: ${srcFiles.length} sources, core ${core} -> ${C}`);
