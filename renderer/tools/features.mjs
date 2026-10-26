#!/usr/bin/env node
// Feature flags (README "Feature flags"): reads renderer/features.json, the one manifest of what ships where.
// Build-time only (no flags on the device): the flavour of a build is the env FW_FEATURES = docs | firmware | panel
// (unset = everything, the WASM workbench). panel = the workbench panel firmware (`firmware/bundle.mjs --all`): every
// feature except one marked "panel": false (Darren 2026-10-26: Lightning is on NO firmware, not even his own panels). Used by build.sh, tools/gen_specs.mjs, tools/gen_icons.mjs, firmware/bundle.mjs and
// site/scripts/extract.mjs.
//   node tools/features.mjs check            fail (exit 1) when a source belongs to no feature (or two), a listed id
//                                            does not exist (docs_pages: a screen of that feature), or a feature shown in
//                                            docs / firmware needs a hidden one
//   node tools/features.mjs exclude <flag>   the source files (relative to renderer/) a docs | firmware build leaves out
//   node tools/features.mjs table            one line per feature: docs, status, firmware
import { existsSync, readdirSync, readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

export const R = join(dirname(fileURLToPath(import.meta.url)), '..');
export const FLAGS = ['docs', 'firmware', 'panel'];
const KINDS = ['screens', 'components', 'foundations', 'bench'];
// spec screens have their own id space (a spec and a component may share a name: weather_days): key spec:<id>
const screenKey = (id) => (existsSync(join(R, 'specs', `${id}.json`)) ? `spec:${id}` : id);
const SRC_DIRS = { foundations: 'foundations', components: 'components', screens: 'screens', bench: 'bench' };

/** every source: { file (relative to renderer/), ids, kind, deps (ids / core files it needs) } */
function inventory() {
  const out = [];
  const core = new Set(readdirSync(join(R, 'core')));
  // a line marked `features: optional` asks whether a part is in the build (and does without it): not a dependency
  const depsOf = (src0) => {
    const src = src0.split('\n').filter((l) => !l.includes('features: optional')).join('\n');
    const d = new Set([...src.matchAll(/\bfind\("([a-z0-9_]+)"/g)].map((m) => m[1]));
    for (const m of src.matchAll(/#include "([a-z0-9_]+\.h)"/g)) if (core.has(m[1])) d.add(`core/${m[1]}`);
    return d;
  };
  for (const dir of Object.values(SRC_DIRS)) {
    if (!existsSync(join(R, dir))) continue;
    for (const f of readdirSync(join(R, dir)).filter((x) => x.endsWith('.cpp')).sort()) {
      const src = readFileSync(join(R, dir, f), 'utf8');
      const ids = [...src.matchAll(/FW_COMPONENT\(\s*([A-Za-z0-9_]+)|fw::add\(\{\s*"([a-z0-9_]+)"/g)].map((m) => m[1] ?? m[2]);
      out.push({ file: `${dir}/${f}`, kind: dir, ids, deps: depsOf(src) });
    }
  }
  for (const f of readdirSync(join(R, 'specs')).filter((x) => x.endsWith('.json') && x !== 'deck.json').sort()) {
    const spec = JSON.parse(readFileSync(join(R, 'specs', f), 'utf8'));
    const deps = new Set();
    const walk = (v) => {
      if (Array.isArray(v)) return v.forEach(walk);
      if (!v || typeof v !== 'object') return;
      if (typeof v.c === 'string') deps.add(v.c);
      if (v.do === 'nav.open' && typeof v.screen === 'string') deps.add(screenKey(v.screen));
      Object.values(v).forEach(walk);
    };
    walk(spec.nodes);
    if (spec.header) deps.add('header');
    out.push({ file: `specs/${f}`, kind: 'specs', ids: [`spec:${f.replace(/\.json$/, '')}`], deps });
  }
  for (const f of [...core].filter((x) => /\.(h|cpp)$/.test(x)).sort()) {
    const src = readFileSync(join(R, 'core', f), 'utf8');
    out.push({ file: `core/${f}`, kind: 'core', ids: [], deps: depsOf(src) });
  }
  return out;
}

/** the manifest resolved: features, owner of every id / core file, sources with their feature */
export function load() {
  const man = JSON.parse(readFileSync(join(R, 'features.json'), 'utf8'));
  const owner = {};      // id or core/<file> -> feature
  const errors = [];
  // components / foundations listed nowhere = the library (shipped); screens, specs and bench items must be listed
  const lib = man.library;
  const features = [lib, ...man.features];
  for (const ft of features) {
    ft.panel ??= true;  // the workbench panel firmware: everything unless a feature opts out
    if (ft.firmware && !ft.panel) errors.push(`${ft.id}: firmware true needs panel true (the release is a subset of the workbench firmware)`);
    // wip must not leak into the release firmware (facet-184): a feature ships on firmware only when it is done
    if (ft.firmware && ft.status === 'wip') errors.push(`${ft.id}: status wip with firmware true: wip features stay off the firmware`);
    for (const flag of FLAGS) if (typeof ft[flag] !== 'boolean') errors.push(`${ft.id}: ${flag} is true or false`);
    if (!man.status[ft.status]) errors.push(`${ft.id}: status ${ft.status} is one of ${Object.keys(man.status).join(', ')}`);
    for (const k of KINDS) for (const id of (ft[k] ?? []).map((x) => (k === 'screens' ? screenKey(x) : x))) {
      if (owner[id]) errors.push(`${id} is in two features: ${owner[id].id} and ${ft.id}`);
      owner[id] = ft;
    }
    for (const f of ft.core ?? []) owner[`core/${f}`] = ft;
  }
  const sources = inventory();
  for (const s of sources) {
    if (s.kind === 'core') { s.feature = owner[s.file] ?? null; continue; }  // null = the engine (always in)
    const fts = [...new Set(s.ids.map((id) => owner[id] ?? (['components', 'foundations'].includes(s.kind) ? lib : undefined)))];
    s.feature = fts.length === 1 && fts[0] ? fts[0] : null;
    if (!s.ids.length) errors.push(`${s.file}: registers nothing`);
    else if (fts.includes(undefined)) errors.push(`${s.file}: ${s.ids.filter((id) => !owner[id]).join(', ')} in no feature (renderer/features.json)`);
    else if (fts.length > 1) errors.push(`${s.file}: its ids are in ${fts.map((f) => f.id).join(' and ')}`);
  }
  const known = new Set(sources.flatMap((s) => s.ids));
  for (const ft of man.features) {
    for (const k of KINDS) for (const id of ft[k] ?? []) if (!known.has(k === 'screens' ? screenKey(id) : id)) errors.push(`${ft.id}: ${k} ${id} does not exist`);
    for (const f of ft.core ?? []) if (!existsSync(join(R, 'core', f))) errors.push(`${ft.id}: core/${f} does not exist`);
    // docs_pages: real screens of this feature (a typo would silently drop a docs page)
    for (const id of ft.docs_pages ?? []) {
      if (!known.has(screenKey(id))) errors.push(`${ft.id}: docs_pages ${id} is not a screen`);
      else if (!(ft.screens ?? []).includes(id)) errors.push(`${ft.id}: docs_pages ${id} is not one of its screens`);
    }
  }
  for (const s of sources) if (s.feature === lib) for (const id of s.ids) owner[id] = lib;
  return { man, features, owner, sources, errors };
}

const on = (ft, flag) => !flag || !ft || ft[flag];
/** source files (relative to renderer/) a build of this flavour leaves out (none when flag is unset) */
export function excluded(flag, L = load()) {
  if (!flag) return [];
  // the docs previews of spec screens are drawn through the Spec screen, so a docs build keeps it (it stays off the nav)
  const keep = flag === 'docs' ? new Set(['screens/spec.cpp']) : new Set();
  return L.sources.filter((s) => s.feature && !s.feature[flag] && !keep.has(s.file)).map((s) => s.file);
}
/** ids (screens, components, foundations, bench) hidden in this flavour */
export function hiddenIds(flag, L = load()) {
  const out = new Set();
  if (flag) for (const ft of L.features) if (!ft[flag]) for (const k of KINDS) for (const id of ft[k] ?? []) out.add(id);
  return out;
}
/** Home card types hidden in this flavour (their detail screen is) */
export function hiddenCards(flag, L = load()) {
  return new Set(flag ? L.features.filter((ft) => !ft[flag]).flatMap((ft) => ft.cards ?? []) : []);
}
/** colour page sections (groups, contrast pair sections) of hidden features: their ids, feature ids, "colour" aliases */
export function hiddenColour(flag, L = load(), aliasesOnly = false) {
  const out = aliasesOnly ? new Set() : hiddenIds(flag, L);
  if (flag) for (const ft of L.features.filter((f) => !f[flag])) { if (!aliasesOnly) out.add(ft.id); for (const c of ft.colour ?? []) out.add(c); }
  return out;
}
/** roadmap rows to leave out of the docs: every feature naming them is hidden */
export function hiddenRoadmap(L = load()) {
  const shown = new Set(L.features.filter((f) => f.docs).flatMap((f) => f.roadmap ?? []));
  return new Set(L.features.filter((f) => !f.docs).flatMap((f) => f.roadmap ?? []).filter((r) => !shown.has(r)));
}
/** id -> {feature, label, docs, status, firmware} for every registered id and spec screen (spec ids without spec:) */
export function byId(L = load()) {
  const out = {};
  for (const s of L.sources) if (s.feature) for (const id of s.ids) {
    const ft = s.feature;
    out[id.replace(/^spec:/, '')] = { feature: ft.id, label: ft.label, docs: ft.docs, status: ft.status, firmware: ft.firmware };
  }
  return out;
}
/** the flavour this process builds (env FW_FEATURES), checked */
export function flavour() {
  const f = process.env.FW_FEATURES || '';
  if (f && !FLAGS.includes(f)) { console.error(`FW_FEATURES=${f}: docs | firmware | unset`); process.exit(2); }
  return f;
}

/** errors: manifest problems + every shown feature (per flag) that needs a hidden one */
export function check(L = load()) {
  const errors = [...L.errors];
  for (const flag of FLAGS) {
    for (const s of L.sources) {
      if (!on(s.feature, flag)) continue;  // hidden itself: may need anything
      for (const d of s.deps) {
        if (s.ids.includes(d) || s.ids.includes(d.replace(/^spec:/, ''))) continue;
        const need = L.owner[d];  // unknown ids (dynamic finds) are not checked
        if (need && !need[flag]) errors.push(`${flag}: ${s.file} (${s.feature ? s.feature.id : 'engine'}) needs ${d} of ${need.id}, which is off`);
      }
    }
  }
  return errors;
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  const [cmd, arg] = process.argv.slice(2);
  const L = load();
  if (cmd === 'check') {
    const e = check(L);
    for (const x of e) console.error(`features: ${x}`);
    const n = (flag) => L.features.filter((f) => f[flag]).length;
    console.log(e.length ? `features: ${e.length} error(s)` : `features: ok (${L.features.length} features, docs ${n('docs')}, firmware ${n('firmware')}, ${L.sources.filter((s) => s.kind !== 'core').length} sources)`);
    process.exit(e.length ? 1 : 0);
  } else if (cmd === 'exclude') {
    if (!FLAGS.includes(arg)) { console.error('exclude docs|firmware'); process.exit(2); }
    process.stdout.write(excluded(arg, L).join('\n') + '\n');
  } else if (cmd === 'table') {
    for (const f of L.features) console.log(`${f.id.padEnd(12)} docs ${f.docs ? 'on ' : 'off'}  ${f.status.padEnd(7)} firmware ${f.firmware ? 'on ' : 'off'}  ${f.label}`);
  } else {
    console.error('usage: features.mjs check | exclude docs|firmware | table');
    process.exit(2);
  }
}
