#!/usr/bin/env node
// Feature flags (README "Feature flags"): reads renderer/features.json, the one manifest of what ships where.
// Build-time only (no flags on the device): the flavour of a build is the env FW_FEATURES = docs | firmware | panel
// (unset = everything, the WASM workbench), contract = a panel build of the contract features only (load()). panel = the workbench panel firmware (`firmware/bundle.mjs --all`): every
// feature except one marked "panel": false (Lightning is on NO firmware, not even his own panels). Used by build.sh, tools/gen_specs.mjs, tools/gen_icons.mjs, firmware/bundle.mjs and
// site/scripts/extract.mjs.
//   node tools/features.mjs check            fail (exit 1) when a source belongs to no feature (or two), a listed id
//                                            does not exist (docs_pages: a screen of that feature), or a feature shown in
//                                            docs / firmware needs a hidden one
//   node tools/features.mjs exclude <flag>   the source files (relative to renderer/) a docs | firmware build leaves out
//   node tools/features.mjs table            one line per feature: docs, status, firmware
//   node tools/features.mjs channels         one line per screen: its channel (stable | beta | soon) and since
//   node tools/features.mjs since [--write V]  what the release tags say each screen's since is; --write V (before
//                                            tagging V) turns every since "next" into V, and a stable V into release.stable
import { execFileSync } from 'node:child_process';
import { existsSync, readdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

export const R = join(dirname(fileURLToPath(import.meta.url)), '..');
export const FLAGS = ['docs', 'firmware', 'panel'];
export const FLAVOURS = [...FLAGS, 'contract'];  // contract: derived in load() (a flavour, not a features.json key)
const KINDS = ['screens', 'components', 'foundations', 'bench'];
// spec screens have their own id space (a spec and a component may share a name): key spec:<id>
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
  // contract (derived, not in features.json): a panel build of the
  // features whose every component is on the contract ("contract": 2) and whose spec screens use only such components;
  // the library (unused legacy parts are linked but unreachable) and system (the panel's own screens) always
  const v2 = (id) => { const f = join(R, 'components', `${id}.cpp`); return existsSync(f) && /"contract":\s*2\b/.test(readFileSync(f, 'utf8')); };
  const specOk = (id) => { const f = join(R, 'specs', `${id}.json`); if (!existsSync(f)) return false; return [...readFileSync(f, 'utf8').matchAll(/"c":\s*"([a-z0-9_]+)"/g)].every((m) => v2(m[1])); };
  for (const ft of features) {
    ft.panel ??= true;  // the workbench panel firmware: everything unless a feature opts out
    ft.contract = ft.panel && (ft === lib || ft.id === 'system' || ((ft.components ?? []).length + (ft.screens ?? []).length > 0 && (ft.components ?? []).every(v2) && (ft.screens ?? []).every(specOk)));
    if (ft.firmware && !ft.panel) errors.push(`${ft.id}: firmware true needs panel true (the release is a subset of the workbench firmware)`);
    // wip must not leak into the release firmware: a feature ships on firmware only when it is done
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
  // features.json "later": the roadmap rows of features held on the wip branch until they are finished
  const later = L.man.later?.roadmap ?? [];
  return new Set([...L.features.filter((f) => !f.docs).flatMap((f) => f.roadmap ?? []), ...later].filter((r) => !shown.has(r)));
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
// Screen channels (RELEASING.md "Screen channels"): features.json release.stable + each feature's since /
// screen_since. stable = in the latest stable release; beta = in a released beta, not yet in a stable one; soon (Coming
// soon) = in no release yet (since "next", or a feature that is not on the firmware).
export const CHANNELS = ['stable', 'beta', 'soon'];
const VER = /^(\d+)\.(\d+)\.(\d+)(?:-([a-z]+)\.(\d+))?$/;
/** semver order for X.Y.Z and X.Y.Z-pre.N (a pre-release sorts before its release) */
export function cmpVer(a, b) {
  const x = a.match(VER), y = b.match(VER);
  for (let i = 1; i <= 3; i++) if (+x[i] !== +y[i]) return +x[i] - +y[i];
  if (!x[4] !== !y[4]) return x[4] ? -1 : 1;
  if (!x[4]) return 0;
  return x[4] !== y[4] ? (x[4] < y[4] ? -1 : 1) : +x[5] - +y[5];
}
/** a screen's channel from its since, the latest stable version and whether its feature is on the firmware */
export function channelOf(since, stable, firmware = true) {
  if (!firmware || !since || since === 'next') return 'soon';
  return cmpVer(since, stable) <= 0 ? 'stable' : 'beta';
}
/** problems with the channel fields of a manifest (no git): a released or documented feature without since, bad
 *  versions, screen_since naming a screen the feature does not have */
export function channelErrors(man) {
  const errors = [];
  const stable = man.release?.stable;
  if (!stable || !VER.test(stable) || stable.includes('-')) errors.push(`release.stable: the latest stable version (X.Y.Z), not ${JSON.stringify(stable)}`);
  const ok = (v) => v === 'next' || (typeof v === 'string' && VER.test(v));
  for (const ft of man.features) {
    const shown = ft.firmware || ft.docs;
    if (shown && ft.firmware && !ft.since) errors.push(`${ft.id}: no since (the release its screens first shipped in, or next): every screen needs a channel`);
    if (ft.since !== undefined && !ok(ft.since)) errors.push(`${ft.id}: since ${JSON.stringify(ft.since)} is a version (X.Y.Z or X.Y.Z-beta.N) or next`);
    if (ft.since !== undefined && !ft.firmware) errors.push(`${ft.id}: since with firmware false: a feature off the firmware is Coming soon, it has no since`);
    for (const [id, v] of Object.entries(ft.screen_since ?? {})) {
      if (!(ft.screens ?? []).includes(id)) errors.push(`${ft.id}: screen_since ${id} is not one of its screens`);
      if (!ok(v)) errors.push(`${ft.id}: screen_since ${id} ${JSON.stringify(v)} is a version or next`);
    }
  }
  // panel features: one of the panel's own screens, or a setting with its own since
  const screensOf = new Set(man.features.flatMap((f) => f.screens ?? []));
  const settingIds = settingsIds(readFileSync(join(R, 'tokens', 'settings.json'), 'utf8'));
  const seen = new Set();
  for (const p of man.panel_features?.list ?? []) {
    const at = `panel_features ${p.id ?? '?'}`;
    if (!p.id || !p.label || !p.about) errors.push(`${at}: id, label and about`);
    if (seen.has(p.id)) errors.push(`${at}: listed twice`);
    seen.add(p.id);
    if (!p.screen === !p.setting) errors.push(`${at}: one of screen or setting`);
    for (const id of [p.screen, ...(p.also ?? [])].filter(Boolean)) if (!screensOf.has(id)) errors.push(`${at}: ${id} is not a screen of any feature`);
    if (p.setting) {
      if (!settingIds.has(p.setting)) errors.push(`${at}: setting ${p.setting} is not in tokens/settings.json`);
      if (!ok(p.since)) errors.push(`${at}: a setting says since (a version or next), not ${JSON.stringify(p.since)}`);
    } else if (p.since !== undefined) errors.push(`${at}: since comes from its screen (features[].since / screen_since)`);
  }
  return errors;
}
const settingsIds = (src) => new Set([...src.matchAll(/"id":\s*"([A-Za-z0-9_]+)"/g)].map((m) => m[1]));
/** the screens that are panel features (kind panel: Set up, Settings, the idle clock ...), not screens people add */
export const panelScreens = (man) => new Set((man.panel_features?.list ?? []).flatMap((p) => [p.screen, ...(p.also ?? [])].filter(Boolean)));
/** panel features with their channel: [{ id, label, about, screen, setting, channel, since }] */
export function panelFeatures(man = load().man) {
  const ch = channels(man);
  return (man.panel_features?.list ?? []).map((p) => {
    const c = p.screen ? ch[p.screen] : { channel: channelOf(p.since, man.release.stable), since: p.since };
    return { ...p, channel: c?.channel ?? 'soon', since: c?.since ?? null };
  });
}
/** screen id -> { channel, since, feature, kind (screen | panel) } for every screen of a feature shown in the docs or on the firmware */
export function channels(man = load().man) {
  const out = {}, panel = panelScreens(man);
  for (const ft of man.features) {
    if (!ft.docs && !ft.firmware) continue;
    for (const id of ft.screens ?? []) {
      const since = ft.firmware ? (ft.screen_since?.[id] ?? ft.since ?? null) : null;
      out[id] = { channel: channelOf(since, man.release.stable, ft.firmware), since, feature: ft.id, kind: panel.has(id) ? 'panel' : 'screen' };
    }
  }
  return out;
}
const git = (...a) => execFileSync('git', a, { cwd: R, encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] });
/** the releases as the tags say: [{ version, ref, screens (on that firmware) }] oldest first, or null without tags (a
 *  shallow checkout). The latest stable is its tag vX.Y.Z, or the stable channel's tag (firmware) before stable tags. */
export function tagReleases(man = load().man) {
  let tags;
  try { tags = git('tag', '--list', 'v*', 'firmware').split('\n').filter(Boolean); } catch { return null; }
  const vs = tags.filter((t) => VER.test(t.slice(1))).map((t) => ({ version: t.slice(1), ref: t }));
  const stable = man.release.stable;
  if (!vs.some((v) => v.version === stable)) { if (!tags.includes('firmware')) return null; vs.push({ version: stable, ref: 'firmware' }); }
  vs.sort((a, b) => cmpVer(a.version, b.version));
  for (const v of vs) {
    const m = JSON.parse(git('show', `${v.ref}:renderer/features.json`));
    v.screens = new Set(m.features.filter((f) => f.firmware).flatMap((f) => f.screens ?? []));
    try { v.settings = settingsIds(git('show', `${v.ref}:renderer/tokens/settings.json`)); } catch { v.settings = new Set(); }
  }
  return vs;
}
/** the channel fields against the tags: a stable or beta screen is in the release its since names (and not before it,
 *  for a beta), a next screen is in no release, release.stable is not behind the newest stable tag */
export function tagErrors(man = load().man, rel = tagReleases(man)) {
  if (!rel) return null;
  const errors = [];
  const stable = man.release.stable;
  const newest = rel.filter((v) => !v.version.includes('-')).at(-1);
  if (newest && cmpVer(newest.version, stable) > 0) errors.push(`release.stable ${stable} but v${newest.version} is tagged: write ${newest.version}`);
  for (const [id, c] of Object.entries(channels(man))) {
    if (!c.since) continue;
    const first = rel.find((v) => v.screens.has(id));
    if (c.since === 'next') { if (first) errors.push(`${id}: since next, but it is on the ${first.ref} firmware: write ${first.version}`); continue; }
    const at = rel.find((v) => v.version === c.since);
    if (c.channel === 'stable') {
      const st = rel.find((v) => v.version === stable);
      if (st && !st.screens.has(id)) errors.push(`${id}: stable (since ${c.since}) but not on the ${st.ref} firmware`);
    } else if (!at) errors.push(`${id}: since ${c.since} is not a tagged release`);
    else if (first?.version !== c.since) errors.push(`${id}: since ${c.since}, the tags say ${first ? first.version : 'next'}`);
  }
  for (const p of (man.panel_features?.list ?? []).filter((x) => x.setting)) {
    const first = rel.find((v) => v.settings.has(p.setting));
    const want = !first ? 'next' : cmpVer(first.version, stable) <= 0 ? null : first.version;  // null: stable, any since <= stable
    if (want === null ? p.since === 'next' || cmpVer(p.since, stable) > 0 : p.since !== want) errors.push(`panel_features ${p.id}: since ${p.since}, the tags say ${first ? first.version : 'next'}`);
  }
  return errors;
}

/** the flavour this process builds (env FW_FEATURES), checked */
export function flavour() {
  const f = process.env.FW_FEATURES || '';
  if (f && !FLAVOURS.includes(f)) { console.error(`FW_FEATURES=${f}: ${FLAVOURS.join(' | ')} | unset`); process.exit(2); }
  return f;
}

/** errors: manifest problems + every shown feature (per flag) that needs a hidden one */
export function check(L = load()) {
  const errors = [...L.errors, ...channelErrors(L.man)];
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
    // the channels against the release tags, when the checkout has them (a shallow clone does not: CI fetches them)
    const te = e.length ? [] : tagErrors(L.man);
    if (te) e.push(...te.map((x) => `channels: ${x}`));
    for (const x of e) console.error(`features: ${x}`);
    const n = (flag) => L.features.filter((f) => f[flag]).length;
    const ch = Object.values(channels(L.man)).reduce((a, c) => ((a[c.channel] = (a[c.channel] ?? 0) + 1), a), {});
    console.log(e.length ? `features: ${e.length} error(s)` : `features: ok (${L.features.length} features, docs ${n('docs')}, firmware ${n('firmware')}, ${L.sources.filter((s) => s.kind !== 'core').length} sources; screens ${CHANNELS.map((c) => `${c} ${ch[c] ?? 0}`).join(', ')}; tags ${te ? 'checked' : 'not here, not checked'})`);
    process.exit(e.length ? 1 : 0);
  } else if (cmd === 'exclude') {
    if (!FLAGS.includes(arg)) { console.error('exclude docs|firmware'); process.exit(2); }
    process.stdout.write(excluded(arg, L).join('\n') + '\n');
  } else if (cmd === 'table') {
    for (const f of L.features) console.log(`${f.id.padEnd(12)} docs ${f.docs ? 'on ' : 'off'}  ${f.status.padEnd(7)} firmware ${f.firmware ? 'on ' : 'off'}  ${f.label}`);
  } else if (cmd === 'channels') {
    for (const [id, c] of Object.entries(channels(L.man))) console.log(`${id.padEnd(14)} ${c.kind.padEnd(6)} ${c.channel.padEnd(6)} ${c.since ?? '-'}`);
    for (const p of panelFeatures(L.man).filter((x) => x.setting)) console.log(`${p.id.padEnd(14)} panel  ${p.channel.padEnd(6)} ${p.since ?? '-'} (setting ${p.setting})`);
  } else if (cmd === 'since') {
    const w = process.argv.indexOf('--write');
    if (w > 0) {
      const V = process.argv[w + 1];
      if (!V || !VER.test(V)) { console.error('since --write X.Y.Z[-beta.N]'); process.exit(2); }
      const p = join(R, 'features.json');
      let src = readFileSync(p, 'utf8').replace(/"next"/g, `"${V}"`);  // since / screen_since values only say next
      if (!V.includes('-')) src = src.replace(/("stable":\s*)"[^"]*"/, `$1"${V}"`);
      writeFileSync(p, src);
      console.log(`features.json: next -> ${V}${V.includes('-') ? '' : `, release.stable ${V}`}`);
    } else {
      const rel = tagReleases(L.man);
      if (!rel) { console.error('since: no release tags here (git fetch --tags)'); process.exit(1); }
      for (const id of Object.keys(channels(L.man))) console.log(`${id.padEnd(12)} ${rel.find((v) => v.screens.has(id))?.version ?? 'next'}`);
    }
  } else {
    console.error('usage: features.mjs check | exclude docs|firmware | table | channels | since [--write V]');
    process.exit(2);
  }
}
