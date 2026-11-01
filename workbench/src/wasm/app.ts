// Facet WASM workbench page (/wasm): left nav (Foundations / Components / Screens) + the component page template
// (big preview, variants grid, playground). Everything comes from the WASM build's registry (fw_meta); a new build
// (watcher) is picked up within a second and the page re-renders in place. renderer/README.md (branch
// `wasm` of the Facet repo). Status per item (WIP / Done) = on the contract or not (renderer/catalogue/contract.json), via
// /api/wasm/status (src/lib/wasm-approve.mjs).
import { hasLab, layoutLab } from './lablayout';
import { onSoundVariant, pickVariant, playUi, setSoundVariant, soundDoc, soundPage, soundVariant } from './sound';
import { legend as olLegend, outlineView } from './outline';
import { entityPicker } from './entity-picker';
import { DEVICES, Engine, fit, manifest, zoomSize, type Zoom, type Comp, type DeviceId, type Prop, type Stage } from './engine';
import { pixelView } from './pixgrid';
import { tapView } from './taps';
import { themeEditor } from './theme-editor';

const SECTIONS = [
  { id: 'foundations', name: 'Foundations' },
  { id: 'components', name: 'Components' },
  { id: 'screens', name: 'Screens' },
];
// HTML pages beside the WASM registry (not drawn in WASM): Foundations > Sound (src/wasm/sound.ts, tokens/sound.json)
// ?build=<dir>: load a review build from /<dir>/<density>/ instead of /facet-wasm/ (a worktree's out, copied to
// public/<dir>; the live workbench build is never touched). Letters, digits and - only.
const BUILD = (() => { const b = new URLSearchParams(location.search).get('build') ?? ''; return /^facet-wasm-[a-z0-9-]+$/.test(b) ? b : 'facet-wasm'; })();
const base = (den: string) => `/${BUILD}/${den}/`;
// release flags per id (renderer/features.json, written by build.sh next to the build as features.json): nav items of
// a feature hidden from the docs get an "off" tag (the workbench keeps them, it is where they are built); /wasm/release
let flags: Record<string, { feature: string; docs: boolean; firmware: boolean; status: string }> = {};
const HTML_PAGES = [{ section: 'foundations', id: 'sound', name: 'Sound' }];
const q = new URLSearchParams(location.search);
// Send-to-device targets: the panels the server knows (FACET_PANELS, lib/panels.ts, handed to the page as
// window.__FACET_PANELS) + all of them. TARGET_NAMES = the picker.
type Target = string;
const PANELS: Record<string, { name: string; board: string; density: string }> = (window as unknown as { __FACET_PANELS?: Record<string, { name: string; board: string; density: string }> }).__FACET_PANELS ?? {};
const TARGETS: string[] = [...Object.keys(PANELS), ...(Object.keys(PANELS).length > 1 ? ['all'] : [])];
const TARGET_NAMES: Record<string, string> = { ...Object.fromEntries(Object.entries(PANELS).map(([k, v]) => [k, v.name])), all: 'All panels' };
const TARGET_SHORT: Record<string, string> = { waveshare: 'Waveshare', guition: 'Guition', shelly: 'X1i' };
// Size picker (preview resolution): the renderer's square sizes; engine DEVICES ids carry the density (shelly = 720)
const SIZES: [DeviceId, string][] = [['guition', '480 \u00d7 480'], ['waveshare', '720 \u00d7 720']];
// remembered target (per browser): the last one picked; ?target= wins
const savedTarget = (() => { try { return localStorage.getItem('wb.target') ?? ''; } catch { return ''; } })();
const st = {
  device: (q.get('device') === 'guition' ? 'guition' : 'waveshare') as DeviceId,
  // theme: 'auto' (default) follows the workbench theme toggle, else the device (prefers-color-scheme); ?theme=dark|light pins it
  theme: (['dark', 'light'].includes(q.get('theme') ?? '') ? q.get('theme') : 'auto') as 'auto' | 'dark' | 'light',
  get light(): boolean {
    if (this.theme !== 'auto') return this.theme === 'light';
    const t = document.documentElement.dataset.theme;
    return t ? t === 'light' : matchMedia('(prefers-color-scheme: light)').matches;
  },
  zoom: (['1', 'fit', '2'].includes(q.get('zoom') ?? '') ? q.get('zoom') : '1') as Zoom,
  // main preview: the component on an empty screen of the device's real size (default) or the component alone
  screen: q.get('view') !== 'component',
  // accent set id (tokens/colour.json accent.sets), '' = the default set; applies to every render
  accent: q.get('accent') ?? '',
  // theme file (renderer themes/*.json, a contract build's fw_lab theme_use), '' = Facet; live for every stage of the engine
  look: q.get('look') ?? '',
  props: {} as Record<string, unknown>,
  // "Show on device" target (Facet workbench firmware on a real panel; /api/wasm/show)
  target: (TARGETS.includes(q.get('target') ?? '') ? q.get('target') : TARGETS.includes(q.get('device') ?? '') ? q.get('device') : TARGETS.includes(savedTarget) ? savedTarget : TARGETS[0] ?? '') as Target,
};
// a ?target= link also becomes the remembered target, so nav clicks (no ?target=) keep it
try { localStorage.setItem('wb.target', st.target); } catch { /* private mode */ }
type DevStatus = { online: boolean; browser?: boolean; error?: string; stale?: Record<string, string>; info?: { built: string; core: string; board?: string; clients?: number }; all?: boolean; devices?: Record<string, DevStatus> };
// Pixel grid debug view (src/wasm/pixgrid.ts): off / grid / grid + object outlines, per browser (localStorage wb.pixgrid);
// comps = the Components view instead (src/wasm/outline.ts: component outlines over the preview + Screens nav badges)
type PixMode = 'off' | 'grid' | 'boxes' | 'comps' | 'taps';
let pixMode: PixMode = (() => { try { const v = localStorage.getItem('wb.pixgrid'); return v === 'grid' || v === 'boxes' || v === 'comps' || v === 'taps' ? v : 'off'; } catch { return 'off'; } })();
let dev: DevStatus | null = null, devKey = '';  // devKey = the last status the Show-on-device button was drawn for
// Spec screens ("can't see calendar screen"): each factory spec (renderer/specs/*.json) gets its own
// Screens entry /wasm/screens/<spec id>, rendered by the Spec screen component with that screen; names from the workbench's
// spec API (factory: {id: {name}}). A screens component with the same id wins.
let specNames: Record<string, string> = {};
void fetch('/api/wasm/spec', { cache: 'no-store' }).then((r) => r.json()).then((d) => {
  specNames = Object.fromEntries(Object.entries(d?.factory ?? {}).map(([id, s]) => [id, String((s as { name?: string })?.name ?? id)]));
  specFactory = d?.factory ?? {};
  if (engine) { nav(); if (specIds().includes(route().id)) page(); }
}).catch(() => {});
const specComp = () => engine?.comps.find((x) => x.id === 'spec' && x.section === 'screens');
/** the factory spec ids this build has (the Spec screen's `screen` options), minus ids a screens component already has */
function specIds(): string[] {
  const opts = (specComp()?.meta.props ?? []).find((p) => p.name === 'screen')?.options ?? [];
  return opts.filter((id) => !engine!.comps.some((x) => x.section === 'screens' && x.id === id));
}
const specName = (id: string) => specNames[id] ?? id.replace(/_/g, ' ').replace(/^./, (s) => s.toUpperCase());
/** the screen sources found in Home Assistant (lib/facet-sources.ts, GET /api/wasm/sources):
 *  the house deck (factory deck + each screen's source and params: Media's real speakers + Auto, the calendars, ...)
 *  goes into every engine (fw_spec deck), so spec screens, Home and Settings > Screens show this house; `options` =
 *  the entities per domain for the playground's pickers */
type Sources = { deck: unknown; specs?: Record<string, unknown>; options: Record<string, { id: string; name: string }[]>; screens: Record<string, { params: Record<string, string> }> };
let sources: Sources | null = null;
let specFactory: Record<string, { params?: Record<string, unknown> }> = {};
function applyDeck(e: Engine) {
  if (!sources?.deck || !e.m._fw_spec) return;
  for (const s of Object.values(sources.specs ?? {})) e.withStr('screen_dev', (pc) => e.withStr(JSON.stringify(s), (pj) => e.m._fw_spec(pc, pj)));  // the house's own screens (real entities)
  e.withStr('deck', (pc) => e.withStr(JSON.stringify(sources!.deck), (pj) => e.m._fw_spec(pc, pj)));
}
void fetch('/api/wasm/sources', { cache: 'no-store' }).then((r) => r.json()).then((d) => {
  if (!d?.deck) return;
  sources = d as Sources;
  if (engine) { applyDeck(engine); if (specIds().includes(route().id)) { st.props = {}; page(); } }
}).catch(() => {});
/** a spec's typed params (spec "params") -> playground controls: select = its options (the spec default), toggle = a
 *  checkbox, entity = the shared searchable picker over this house's entities of its domain (all of them), entities / text = text; '' = the deck's value
 *  (else the spec's default), so Auto and the discovered lists apply unless a control picks something */
function paramProps(id: string): Prop[] {
  const out: Prop[] = [];
  for (const [k, v] of Object.entries(specFactory[id]?.params ?? {})) {
    if (typeof v !== 'object' || !v) { out.push({ name: k, type: 'text', default: '' }); continue; }
    const t = v as { type?: string; options?: string[]; default?: string; domain?: string; about?: string; label?: string };
    if (t.type === 'hidden') continue;
    const about = t.about ?? t.label;
    if (t.type === 'select') out.push({ name: k, type: 'select', about, options: t.options ?? [], default: t.default ?? t.options?.[0] ?? '' });
    else if (t.type === 'toggle') out.push({ name: k, type: 'bool', about, default: t.default === 'true' });
    else if (t.type === 'entity') out.push({ name: k, type: 'entity', about: about ?? `A ${t.domain ?? ''} entity from this house. Empty uses the deck's own.`, domains: (t.domain ?? '').split(',').filter(Boolean), default: '' });
    else out.push({ name: k, type: 'text', about: about ?? (t.type === 'entities' ? 'A comma list of entity ids. Empty uses the deck\'s own.' : undefined), default: '' });
  }
  return out;
}
/** the Spec screen component set up as one screen: `screen` = id, then a control per typed param of that spec, no
 *  variants grid */
function specPage(id: string): Comp | null {
  const s = specComp();
  if (!s) return null;
  const screen = (s.meta.props ?? []).find((p) => p.name === 'screen');
  return { ...s, name: specName(id), meta: { ...s.meta, props: [...(screen ? [{ ...screen, default: id }] : []), ...paramProps(id)], variants: [] } };
}
let engine: Engine | null = null;
let built = '';
type Status = 'new' | 'done';
type StatusRow = { file: string; status: Status; name?: string; contract?: boolean; last: { sha: string; subject: string; at: string } | null };
let status: Record<string, StatusRow> = {};  // keyed "<section>/<id>"
let statusKey = '';
const STATUS: Record<Status, { label: string; hint: string }> = {
  new: { label: 'WIP', hint: 'not on the contract yet' },
  done: { label: 'Done', hint: 'on the contract' },
};
async function loadStatus() {
  try {
    const s = await (await fetch('/api/wasm/status', { cache: 'no-store' })).json();
    const k = JSON.stringify(s);
    if (k === statusKey) return false;
    status = s; statusKey = k; return true;
  } catch { return false; }
}
const badge = (key: string) => { const s = status[key]?.status; const b = h('span', `wb-st ${s ?? 'none'}`); b.title = s ? `${STATUS[s].label}: ${STATUS[s].hint} (${status[key].file})` : 'no status'; return b; };
const routeKey = () => { const r = route(); return `${r.section}/${r.id}`; };
function refreshStatusUi() {
  const t = document.getElementById('wb-title-st'); if (t) t.replaceWith(titleStatus());
  const w = document.getElementById('wb-new'); if (w) w.replaceWith(whatsNew());
}
const $ = <T extends HTMLElement>(id: string) => document.getElementById(id) as T;
const h = (tag: string, cls = '', text = '') => { const e = document.createElement(tag); if (cls) e.className = cls; if (text) e.textContent = text; return e; };
const route = () => { const p = location.pathname.replace(/^\/wasm\/?/, '').split('/').filter(Boolean); return { section: p[0] ?? '', id: p[1] ?? '' }; };
const qs = () => { const u = new URLSearchParams(); if (BUILD !== 'facet-wasm') u.set('build', BUILD); if (st.device !== 'waveshare') u.set('device', st.device); if (st.theme !== 'auto') u.set('theme', st.theme); if (st.zoom !== '1') u.set('zoom', st.zoom); if (!st.screen) u.set('view', 'component'); if (st.accent) u.set('accent', st.accent); if (st.look) u.set('look', st.look); if (soundVariant()) u.set('sound', soundVariant()); const s = u.toString(); return s ? `?${s}` : ''; };
// theme files: only a build with the theme engine (fw_lab theme_*) lists them; the live one is the module's (all stages)
const labCall = (cmd: string, arg = ''): string => engine!.withStr(cmd, (pc) => engine!.withStr(arg, (pa) => engine!.str(engine!.m._fw_lab(-1, pc, pa))));
const looks = (): string[] => { if (!engine || !hasLab(engine)) return []; try { return (JSON.parse(labCall('theme_builtins')) as { id: string; ok: boolean }[]).filter((x) => x.ok).map((x) => x.id); } catch { return []; } };
// look 'custom' = the theme editor's file (theme-editor.ts), kept per browser (localStorage wb.theme.custom); a refused
// draft never goes live (the module keeps what it had: the panel's rule)
const draft = { json: (() => { try { return localStorage.getItem('wb.theme.custom') ?? ''; } catch { return ''; } })() };
const applyLook = () => {
  if (!engine || !hasLab(engine) || !looks().length) return;
  if (st.look === 'custom') { if (draft.json) labCall('theme_apply', draft.json); } else labCall('theme_use', st.look);
};
const lookName = (id: string) => (id === 'win98' ? 'Win98' : id === 'custom' ? 'Edited' : id[0].toUpperCase() + id.slice(1));
const editTheme = () => themeEditor({
  call: labCall,
  builtins: looks,
  text: () => {
    if (st.look === 'custom' && draft.json) return draft.json;
    try { return (JSON.parse(labCall('theme_slot_builtin', st.look || 'facet')) as { json?: string }).json ?? ''; } catch { return ''; }
  },
  applied: (json) => { draft.json = json; try { localStorage.setItem('wb.theme.custom', json); } catch { /* private mode */ } st.look = 'custom'; go(); nav(); page(); },
});
const accentIdx = () => engine!.tokens.accent.sets.findIndex((a) => a.id === st.accent);
const defaults = (c: Comp) => Object.fromEntries((c.meta.props ?? []).map((p) => [p.name, p.default ?? (p.type === 'bool' ? false : p.type === 'range' ? p.min ?? 0 : '')]));

export async function start() {
  // narrow windows: the sidebar (nav + settings) collapses behind the Menu button (wasm.css @media 900px)
  const menu = $('wb-menu');
  menu.onclick = () => { const open = $('wb').classList.toggle('open'); menu.setAttribute('aria-expanded', String(open)); };
  await loadStatus();
  await boot();
  // status comes from git (commits don't rebuild): re-check every 3 s, redraw nav + heading badge on change
  setInterval(async () => { if (engine && (await loadStatus())) { nav(); refreshStatusUi(); } }, 3000);
  // fast loop: poll the build manifest; a new build -> reload the module and re-render in place
  // workbench firmware on the target panel: online? which components are stale? (button state)
  const devPoll = async () => {
    if (!st.target) return;  // no panels configured (FACET_PANELS)
    try {
      const t = st.target, d = await (await fetch(`/api/wasm/show?device=${t}`, { cache: 'no-store' })).json();
      if (t !== st.target) return; // the target changed while this (slow, offline panel) reply was on its way
      const k = JSON.stringify([st.target, d.online, d.stale, d.info?.built, d.info?.clients, d.devices]);
      dev = d;
      if (k !== devKey) { devKey = k; refreshDevBtn(); }
    } catch { /* workbench restarting */ }
  };
  void devPoll();
  setInterval(devPoll, 5000);
  (window as unknown as { __wbDevPoll: () => void }).__wbDevPoll = devPoll;
  setInterval(async () => {
    try {
      const man = await manifest(DEVICES[st.device].den, base(DEVICES[st.device].den));
      if (man.built !== built) { await boot(); flash(`rebuilt in ${man.build_s} s`); }
    } catch { /* mid-build */ }
  }, 1000);
}

async function boot() {
  const den = DEVICES[st.device].den;
  let e: Engine;
  try { e = await Engine.load(den, base(den)); } catch (err) {
    $('wb-main').replaceChildren(callout('bad', `No ${den} build yet: run renderer/build.sh ${den} (${String(err).slice(0, 120)})`));
    return;
  }
  holdScroll();
  engine?.dispose();
  engine = e;
  built = e.man.built;
  if (st.look) applyLook();  // a new module starts in Facet: the chosen theme file again
  try { flags = (await (await fetch(`${base(den)}features.json`, { cache: 'no-store' })).json()).ids ?? {}; } catch { flags = {}; }
  applyDeck(e);
  // a screen's navigate (settings button, Home cards) on a single-screen page: open that page (a screen, or a workbench
  // page from the placeholder Home); only from a Screens page, never for a page the build doesn't have or the one
  // already shown. The Device page (screens/device) navigates inside WASM (core/device.h): no redirect there
  e.onNavigate = (id) => {
    const at = id.indexOf('?'), args = at < 0 ? '' : id.slice(at + 1);  // "screen_set?{"id":"calendar"}": its props
    id = at < 0 ? id : id.slice(0, at);
    const r = route(), spec = specIds().includes(id), c = e.comps.find((x) => x.id === id);
    if (r.section !== 'screens' || r.id === 'device' || r.id === id || (!c && !spec)) return;
    let pq = '';
    try { if (args && JSON.parse(args) && typeof JSON.parse(args) === 'object') pq = `${qs() ? '&' : '?'}p=${encodeURIComponent(args)}`; } catch { /* not JSON: the page's defaults */ }
    setTimeout(() => { location.href = `/wasm/${spec ? 'screens' : c!.section}/${id}${qs()}${pq}`; }, 150);  // let the open sound start
  };
  nav();
  page();
}

/** a nav section's entries: registry components + HTML pages; Screens also every factory spec as its own entry;
 *  every group in name order */
function navItems(section: string) {
  const items = [...engine!.comps.filter((c) => c.section === section), ...HTML_PAGES.filter((p) => p.section === section).map((p) => ({ ...p, meta: {} as Comp['meta'] }))];
  if (section === 'screens') items.push(...specIds().map((id) => ({ ...specComp()!, id, name: specName(id) })));
  return items.sort((a, b) => a.name.localeCompare(b.name));
}
// Contract chips: Components + Screens say Contract (rebuilt on renderer/CONTRACT.md, listed in
// renderer/catalogue/contract.json via /api/wasm/status .contract) or Legacy; a summary + All / Contract / Legacy filter
// above them, per browser (localStorage wb.contractFilter)
const CONTRACT_SECS = ['components', 'screens'];
let contractFilter = (() => { try { const v = localStorage.getItem('wb.contractFilter'); return v === 'contract' || v === 'legacy' ? v : 'all'; } catch { return 'all'; } })();
function contractBar() {
  const keys = CONTRACT_SECS.flatMap((sec) => navItems(sec).map((c) => `${sec}/${c.id}`));
  const n = keys.filter((k) => status[k]?.contract).length;
  const bar = h('div', 'wb-contract');
  bar.append(h('div', 'wb-contract-sum', `Contract ${n} of ${keys.length}`),
    seg([['all', 'All', contractFilter === 'all'], ['contract', 'Contract', contractFilter === 'contract'], ['legacy', 'Legacy', contractFilter === 'legacy']], (v) => { contractFilter = v; try { localStorage.setItem('wb.contractFilter', v); } catch { /* private mode */ } nav(); }));
  return bar;
}
function nav() {
  const r = route(), el = $('wb-links');
  el.replaceChildren();
  const home = h('a', 'wb-brand', 'Facet WASM') as HTMLAnchorElement;
  home.href = `/wasm${qs()}`;
  el.append(home);
  for (const s of SECTIONS) {
    const onC = CONTRACT_SECS.includes(s.id);
    if (s.id === CONTRACT_SECS[0]) el.append(contractBar());
    el.append(h('div', 'wb-sec', s.name));
    const items = navItems(s.id);
    if (!items.length) el.append(h('div', 'wb-none', 'nothing yet'));
    for (const c of items) {
      const isC = !!status[`${s.id}/${c.id}`]?.contract;
      if (onC && contractFilter !== 'all' && (contractFilter === 'contract') !== isC) continue;
      const a = h('a', 'wb-item' + (r.id === c.id ? ' on' : ''), c.name) as HTMLAnchorElement;
      a.href = `/wasm/${s.id}/${c.id}${qs()}`;
      a.prepend(badge(`${s.id}/${c.id}`));
      // built on the contract in code (meta "contract") but not reviewed in contract.json: Contract, hollow dot, still counts as Legacy
      const code = !isC && !!(c.meta as { contract?: number }).contract;
      if (onC) { const t = h('span', `wb-tag wb-con ${isC ? 'on' : code ? 'code' : 'off'}`, isC || code ? 'Contract' : 'Legacy'); t.prepend(h('i')); t.title = isC ? 'rebuilt on the contract and reviewed (renderer/CONTRACT.md; catalogue/contract.json)' : code ? 'built on the contract in code, not reviewed yet (no functional tests or goldens): counts as Legacy' : 'not rebuilt on the contract yet'; a.append(t); }
      if (c.meta.demo) a.append(h('span', 'wb-tag', 'demo'));
      const fl = flags[c.id];
      if (fl && !fl.docs) { const t = h('span', 'wb-tag', 'off'); t.title = `${fl.feature}: hidden from the docs${fl.firmware ? '' : ', not in the firmware'} (Tools > Release)`; a.append(t); }
      // Components view: spec screens say whether their body is composed or a legacy hand-built *_view
      if (pixMode === 'comps' && s.id === 'screens' && specFactory[c.id]) {
        const hb = JSON.stringify((specFactory[c.id] as { nodes?: unknown }).nodes ?? []).match(/"c":"\w+_view"/);
        a.append(h('span', `wb-tag ${hb ? 'wb-ol-hb' : ''}`, hb ? 'hand-built' : 'components'));
      }
      el.append(a);
    }
  }
  // pages about the whole build (not a component)
  el.append(h('div', 'wb-sec', 'Tools'));
  const bench = h('a', 'wb-item', 'Bench') as HTMLAnchorElement;
  bench.href = '/wasm/bench';
  bench.title = 'Frame cost, redrawn area and memory of every component and screen, against each panel’s budget';
  el.append(bench);
  const corpus = h('a', 'wb-item', 'Spec corpus') as HTMLAnchorElement;  // corpus test, seen (src/pages/wasm/corpus.astro)
  corpus.href = '/wasm/corpus';
  corpus.title = 'Every spec corpus render with what each check found, before vs after the working tree';
  el.append(corpus);
  const tests = h('a', 'wb-item', 'Functional tests') as HTMLAnchorElement;  // functional tests, seen (src/pages/wasm/tests.astro)
  tests.href = '/wasm/tests';
  tests.title = 'Each component\'s functional tests: pass / fail per size, log and screenshot';
  el.append(tests);
  const gd = h('a', 'wb-item', 'Golden review') as HTMLAnchorElement;  // golden screenshot candidates (src/pages/wasm/goldens.astro)
  gd.href = '/wasm/goldens';
  gd.title = 'Accept or reject the screenshots the tests wrote';
  el.append(gd);
  const ct = h('a', 'wb-item', 'Contract') as HTMLAnchorElement;  // renderer/CONTRACT.md, readable (src/pages/wasm/contract.astro)
  ct.href = '/wasm/contract';
  ct.title = 'The rules every component, screen and theme follows (renderer/CONTRACT.md)';
  el.append(ct);
  const rel = h('a', 'wb-item', 'Release') as HTMLAnchorElement;  // release flags per feature (src/pages/wasm/release.astro)
  rel.href = '/wasm/release';
  rel.title = 'Which features ship: docs site, status and panel firmware (renderer/features.json)';
  el.append(rel);
  const specs = h('a', 'wb-item', 'Send specs') as HTMLAnchorElement;  // screens + deck to a panel (src/pages/wasm/specs.astro)
  specs.href = '/wasm/specs';
  specs.title = 'Send a screen spec or the deck to a panel, or revert it to factory';
  el.append(specs);
  const b = h('div', 'wb-build');
  b.id = 'wb-build';
  b.textContent = `${engine!.man.density} build ${engine!.man.built.slice(11, 19)} · ${engine!.man.build_s} s`;
  const key = h('div', 'wb-key');
  for (const s of ['new', 'done'] as Status[]) { const row = h('div', 'wb-key-row'); const d = h('span', `wb-st ${s}`); row.append(d, h('span', '', STATUS[s].label), h('span', 'wb-key-hint', STATUS[s].hint)); key.append(row); }
  el.append(key, b);
}

// Events (fw::emit / fw::sound from the component, engine.onEvent): per component page, newest first, with the
// stage it came from (Preview or a variant). Sound requests play in the browser in the selected variant when the
// sidebar's Sound group is On (soundPick).
const evLog = { comp: '', taps: 0, rows: [] as string[] };
let soundOn = (() => { try { return localStorage.getItem('wb.sound') !== '0'; } catch { return true; } })();
let evRender: (() => void) | null = null;
function eventsPanel(compId: string) {
  if (evLog.comp !== compId) Object.assign(evLog, { comp: compId, taps: 0, rows: [] });
  const box = h('div', 'wb-events'), bar = h('div', 'wb-ev-bar'), count = h('strong', 'wb-ev-count'), list = h('ol', 'wb-ev-list');
  const clear = h('button', 'wb-btn wb-ev-clear', 'Clear') as HTMLButtonElement;
  clear.type = 'button';
  clear.onclick = () => { evLog.taps = 0; evLog.rows = []; evRender?.(); };
  bar.append(count, clear);
  box.append(bar, list);
  evRender = () => {
    count.textContent = `${evLog.taps} tap${evLog.taps === 1 ? '' : 's'}`;
    list.replaceChildren(...(evLog.rows.length ? evLog.rows.map((r) => h('li', '', r)) : [h('li', 'wb-none', 'Tap the preview or a variant.')]));
  };
  evRender();
  engine!.onEvent = (ev, stage) => {
    if (!demoEvent(stage)) return;
    if (ev.kind === 'sound') { if (soundOn) void playUi(ev.name, ev.detail); if (evLog.rows.length) evLog.rows[0] += ` · sound ${ev.name}`; evRender?.(); return; }
    if (['tap', 'toggle', 'back'].includes(ev.name)) evLog.taps++;  // real taps (states.json: switches emit toggle, Back back; blocked is not a tap)
    const t = new Date().toTimeString().slice(0, 8);
    evLog.rows.unshift(`${t} · ${stage?.label || 'stage'} · ${ev.name}${ev.detail ? ` "${ev.detail}"` : ''}`);
    evLog.rows.length = Math.min(evLog.rows.length, 30);
    evRender?.();
  };
  return box;
}

// Demo reset: variant and doc-section samples go back to their initial state motion.json
// "demo-reset" ms after the last interaction (any event from the stage, or a release on it); the timer restarts on every
// event and never runs while a finger is down; the revert takes the update path first (the component animates back to
// its starting props, silent), else a silent rebuild (its events / sounds are dropped). The main
// playground preview is the free sandbox: never reset. The docs site (facet-wasm site/src/scripts/live.ts) does the same.
let demoMs = 1800;
void fetch('/api/wasm/motion', { cache: 'no-store' }).then((r) => r.json()).then((m) => { const v = m?.durations?.['demo-reset']?.ms; if (typeof v === 'number') demoMs = v; }).catch(() => {});
const demos = new Map<Stage, { rebuild: () => void; held: boolean; timer?: number; muteUntil: number }>();
function demoArm(s: Stage) {
  const d = demos.get(s);
  if (!d) return;
  clearTimeout(d.timer);
  if (d.held) return;
  // a demo at its preset (its replay ended where it started, fw::demo_state 0): no reset step
  d.timer = window.setTimeout(() => { if (d.held || !engine?.views.has(s) || s.demoState() === 0) return; d.muteUntil = performance.now() + 250; d.rebuild(); }, demoMs);
}
/** false = drop this event (a silent demo revert); else (re)arms the stage's reset */
function demoEvent(s: Stage | undefined) {
  const d = s && demos.get(s);
  if (!d) return true;
  if (performance.now() < d.muteUntil) return false;
  demoArm(s!);
  return true;
}
function demoSample(s: Stage, rebuild: () => void) {
  demos.set(s, { rebuild, held: false, muteUntil: 0 });
  s.canvas.addEventListener('pointerdown', () => { const d = demos.get(s); if (d) { d.held = true; clearTimeout(d.timer); } });
  const up = () => { const d = demos.get(s); if (d?.held) { d.held = false; demoArm(s); } };
  s.canvas.addEventListener('pointerup', up);
  s.canvas.addEventListener('pointercancel', up);
}

let ro: ResizeObserver | null = null;
// re-rendering the same page (a setting, a new build) keeps the window scroll and the sidebar's: the old height is
// held while the page rebuilds (so the document never shrinks under the scroll position), then the position is put
// back; a different page (nav) starts at the top. Doc stages are auto height (no LVGL scroll inside to keep).
let lastPage = '';
let held: { y: number; sy: number } | null = null;
// hold the page height + remember the scroll (boot calls it before disposing the old engine, which removes canvases)
function holdScroll() {
  if (held || location.pathname !== lastPage) return;
  const main = $('wb-main');
  held = { y: window.scrollY, sy: $('wb-nav').scrollTop };
  main.style.minHeight = `${main.offsetHeight}px`;
}
function page() {
  const main = $('wb-main'), side = $('wb-nav');
  holdScroll();
  const keep = held;
  held = null;
  lastPage = location.pathname;
  try { pageBuild(); } finally {
    if (keep) {
      window.scrollTo(0, keep.y); side.scrollTop = keep.sy;
      // after the ResizeObservers have sized the canvases: release the hold, then put the scroll back once more
      requestAnimationFrame(() => requestAnimationFrame(() => { main.style.minHeight = ''; window.scrollTo(0, keep.y); side.scrollTop = keep.sy; }));
    }
  }
}
function pageBuild() {
  const r = route(), main = $('wb-main');
  for (const s of [...engine!.views]) s.free();
  for (const d of demos.values()) clearTimeout(d.timer);
  demos.clear();
  ro?.disconnect();
  engine!.onEvent = null;
  const hp = HTML_PAGES.find((x) => x.section === r.section && x.id === r.id);
  if (hp) {
    settings(null);
    const head = h('div', 'wb-head'), t = h('h1', '', hp.name);
    t.append(titleStatus());
    head.append(t);
    // live components on the Sound page (pitch range sample): real Stages at 1:1, their sounds always play (the page
    // is for listening; the sidebar Sound On/Off is for the other pages)
    engine!.onEvent = (ev) => { if (ev.kind === 'sound') void playUi(ev.name, ev.detail); };
    const mount = (id: string, props: Record<string, unknown>) => {
      const c = engine!.comps.find((x) => x.id === id);
      if (!c) return null;
      const D = DEVICES[st.device].d, [fw, fh] = c.meta.frame, s = engine!.stage(Math.round(fw * D), fh ? Math.round(fh * D) : 64);
      if (s.render(id, { ...defaults(c), ...props }, st.light, accentIdx(), !fh)) { s.free(); return null; }
      const z = zoomSize(s.w, s.h, 1);
      Object.assign(s.canvas.style, { width: `${z.width}px`, height: `${z.height}px` });
      return s.canvas;
    };
    return void soundPage(main, head, mount);
  }
  const asSpec = r.section === 'screens' && specIds().includes(r.id);
  const c = asSpec ? specPage(r.id) : engine!.comps.find((x) => x.id === r.id && x.section === r.section) ?? engine!.comps.find((x) => x.id === r.id);
  if (c?.meta.layout === 'doc') return docPage(c);
  if (!c) {
    settings(null);
    main.replaceChildren(h('h1', '', 'Facet WASM workbench'),
      h('p', 'wb-lead', 'Facet components written in C++/LVGL, compiled to WebAssembly and drawn here pixel for pixel. Pick one on the left.'),
      whatsNew());
    return;
  }
  const pageKey = asSpec ? `spec:${r.id}` : c.id;  // a spec screen page keeps its own playground
  // ?p={json}: the props a navigate carried (Settings > Screens > a screen = screen_set {"id": ...}), on first open only
  const carried = (() => { try { const v = JSON.parse(q.get('p') ?? ''); return v && typeof v === 'object' && !Array.isArray(v) ? v : {}; } catch { return {}; } })();
  const keep = st.props.__id === pageKey ? st.props : { __id: pageKey, ...defaults(c), ...carried };
  st.props = keep;
  const head = h('div', 'wb-head');
  const title = h('h1', '', c.name);
  title.append(titleStatus());
  if (c.meta.demo) title.append(h('span', 'wb-tag', 'demo'));
  head.append(title);
  settings({ view: true, comp: c, what: [['Playground', () => props()], ...(c.meta.variants ?? []).map((v) => [`Variant: ${v.label}`, () => ({ ...defaults(c), ...v.props })] as [string, () => Record<string, unknown>])] });
  const preview = h('section', 'wb-preview'), err = h('div');
  const variants = h('section', 'wb-variants'), play = h('section', 'wb-play');
  // preview left, playground right (sticky); stacks under ~1100 px (wasm.css .wb-top)
  const top = h('div', 'wb-top'), side = h('aside', 'wb-side');
  side.append(h('h2', '', 'Playground'), play, ...slotsPanel(c), h('h2', '', 'Events'), eventsPanel(c.id));
  if (c.meta.opening) {
    const bar = h('div', 'wb-open'), rp = h('button', 'wb-btn', 'Replay opening') as HTMLButtonElement;
    rp.type = 'button';
    rp.onclick = () => rerender(true);
    bar.append(h('span', '', c.meta.opening), rp);
    play.before(bar);
  }
  top.append(preview, side);
  // a picture's URL (Image: props.src): what happened to it, in plain words (engine imgLoop, /api/wasm/img)
  const imgNote = h('div', 'wb-imgnote');
  const showImg = () => {
    const src = typeof st.props.src === 'string' ? st.props.src : '', s = src ? engine!.imgStatus.get(src) : undefined;
    imgNote.replaceChildren(...(!s || s.state === 'loading' ? [] : s.state === 'failed'
      ? [callout('bad', `Picture didn't load: ${s.why || s.code}. The panel keeps the picture it had, or shows the fallback.`)]
      : s.via === 'browser' ? [callout('info', `Loaded by your browser only (${s.why}). The panel downloads pictures itself and can't get this one: it would show the fallback.`)] : []));
  };
  engine!.onImage = (src) => { if (src === st.props.src) showImg(); };
  main.replaceChildren(head, err, imgNote, top, h('h2', '', 'Variants'), variants);
  // Pixel grid view of the preview + meta "compare" size studies (this density only), each off and on
  const pixSec = h('section', 'wb-pixsec');
  if (pixMode === 'grid' || pixMode === 'boxes') main.insertBefore(pixSec, variants.previousSibling);

  const D = DEVICES[st.device].d, [fw, fh] = c.meta.frame;
  // frame h 0 = auto height (the stage follows the content)
  const auto = !fh, W = Math.round(fw * D), H = auto ? 64 : Math.round(fh * D);
  const props = () => { const { __id, ...p } = st.props; void __id; return p; };
  // big preview: 1:1 = one panel pixel per physical screen pixel (default), Fit = whole-number fit, 2x = two
  // Screen view: one WASM display of the device's real size (the same screen object the component builds on, Facet
  // bg token, component centred at true size); zoom scales the whole screen so proportions stay true
  const S = DEVICES[st.device].size, PW = st.screen ? S : W;
  let PH = st.screen ? S : H;
  const big = engine!.stage(PW, PH);
  big.label = 'Preview';
  if (st.screen) { const bez = h('div', 'wb-screen'); bez.append(big.canvas); preview.append(bez); } else preview.append(big.canvas);
  preview.dataset.zoom = st.zoom;
  // every build is an opening (the component's build-time motion, e.g. the Dial's sweep up from 0); a control change is
  // not: it takes the update path (new props in place, the component's own animation, silent; facet-wasm README
  // "Update path") and only re-builds, with props key "opening": false, when the component can't ("Replay opening" =
  // a fresh build)
  const rerender = (open = false) => {
    const e = open ? big.render(c.id, props(), st.light, accentIdx(), auto && !st.screen) : big.apply(c.id, { ...props(), opening: false }, st.light, accentIdx(), auto && !st.screen);
    PH = big.h; err.replaceChildren(...(e ? [callout('bad', e)] : [])); showImg(); size();
  };
  const size = () => { const pad = st.screen ? 32 + 24 : 32; const f = st.zoom === 'fit' ? fit(PW, PH, preview.clientWidth - pad, Math.max(240, window.innerHeight * (st.screen ? 0.75 : 0.5) - pad)) : zoomSize(PW, PH, Number(st.zoom)); Object.assign(big.canvas.style, { width: `${f.width}px`, height: `${f.height}px` }); };
  (ro = new ResizeObserver(size)).observe(preview);
  rerender(true);
  if (hasLab(engine!)) top.after(layoutLab(engine!, c.id, props, st.light, accentIdx(), S));  // contract build: the Layout lab (lablayout.ts)
  preview.dataset.light = String(st.light);
  if (pixMode === 'comps') {
    // Components view: outlines over the preview (any zoom, follows updates) + legend and the page's summary line
    const line = h('div', 'wb-ol-sum'), bar = h('div', 'wb-ol-bar');
    outlineView(big, (id) => engine!.comps.find((x) => x.id === id)?.section, (t) => { line.textContent = t; });
    bar.append(line, olLegend());
    top.before(bar);
  }
  if (pixMode === 'taps') {  // Tap areas view (taps.ts): every hit area over the preview, red under touch:min
    const line = h('div', 'wb-ol-sum'), bar = h('div', 'wb-ol-bar');
    tapView(big, (t) => { line.textContent = t; });
    bar.append(line);
    top.before(bar);
  }
  if (pixMode === 'grid' || pixMode === 'boxes') {
    pixSec.append(h('h2', '', 'Pixel grid'), h('p', 'wb-lead', 'The preview magnified: 1 px grid, 4 px grid stronger (stage coordinates), hover for x / y and the colour. Live while it animates.'), pixelView(big, { boxes: pixMode === 'boxes' }));
    const cmp = (c.meta.compare ?? []).filter((x) => !x.density || x.density === DEVICES[st.device].den);
    if (cmp.length) pixSec.append(h('h2', '', `Size study (${DEVICES[st.device].size} \u00d7 ${DEVICES[st.device].size})`));
    for (const x of cmp) {
      const row = h('div', 'wb-pix-cmp'), pair = h('div', 'wb-pix-pair');
      for (const [lab, pp] of [['off', x.props], ['on', x.on]] as [string, Record<string, unknown> | undefined][]) {
        if (!pp) continue;
        const s: Stage = engine!.stage(W, H);
        s.label = `${x.label} · ${lab}`;
        const e = s.render(c.id, { ...defaults(c), ...pp }, st.light, accentIdx(), auto);
        const fig = h('figure', 'wb-var');
        fig.append(e ? callout('bad', e) : pixelView(s, { boxes: pixMode === 'boxes', maxW: 560 }), h('figcaption', '', lab));
        pair.append(fig);
      }
      row.append(h('h3', '', x.label), ...(x.about ? [h('p', 'wb-lead', x.about)] : []), pair);
      pixSec.append(row);
    }
  }

  // variants: defaults + each variant's props
  for (const v of c.meta.variants ?? [{ label: 'default', props: {} }]) {
    const cell = h('figure', 'wb-var'), s: Stage = engine!.stage(W, H);
    s.label = v.label;
    const e = s.render(c.id, { ...defaults(c), ...v.props }, st.light, accentIdx(), auto);
    const f = zoomSize(W, s.h, 1); // variants always true scale
    Object.assign(s.canvas.style, { width: `${f.width}px`, height: `${f.height}px` });
    demoSample(s, () => { if (s.update(c.id, { ...defaults(c), ...v.props, opening: false }, st.light, accentIdx()) === '') return; if (s.w !== W || s.h !== H) s.resize(W, H); s.render(c.id, { ...defaults(c), ...v.props, opening: false }, st.light, accentIdx(), auto); const g = zoomSize(W, s.h, 1); Object.assign(s.canvas.style, { width: `${g.width}px`, height: `${g.height}px` }); });
    cell.append(s.canvas, h('figcaption', '', e ? `${v.label}: ${e}` : v.label));
    variants.append(cell);
  }

  // playground: controls -> props -> live re-render
  for (const p of c.meta.props ?? []) play.append(control(p, rerender));
  if (!(c.meta.props ?? []).length) play.append(h('div', 'wb-none', 'no props'));
  const json = h('pre', 'wb-json');
  const showJson = () => (json.textContent = JSON.stringify(props()));
  play.addEventListener('input', showJson);
  showJson();
  play.append(json);
}

// "doc" layout (foundations, meta.layout = 'doc'): one full-width, auto-height WASM stage per variant (= section), with
// its heading and about line in HTML; zoom 1:1 / 2x as components, Fit = fit the column width
function docPage(c: Comp) {
  const main = $('wb-main');
  const head = h('div', 'wb-head');
  const title = h('h1', '', c.name);
  title.append(titleStatus());
  head.append(title);
  // meta "events": true = a doc page with live components (States): the Events panel under the title
  if (c.meta.events) head.append(eventsPanel(c.id));
  // every doc page with live components plays their sounds (Layout's Chime toggle etc.), Events panel or not
  else engine!.onEvent = (ev, stage) => { if (demoEvent(stage) && ev.kind === 'sound' && soundOn) void playUi(ev.name, ev.detail); };
  settings({ view: false, comp: c, what: (c.meta.variants ?? []).map((v) => [v.label, () => ({ ...defaults(c), ...v.props })] as [string, () => Record<string, unknown>]) });
  // each section canvas is at least the device's screen: width = screen, height = max(screen, content)
  const S = DEVICES[st.device].size, list = h('div', 'wb-doc');
  main.replaceChildren(head, list);
  const stages: Stage[] = [];
  for (const v of c.meta.variants ?? []) {
    const sec = h('section', 'wb-doc-sec'), box = h('div', 'wb-doc-box');
    sec.append(h('h2', '', v.label));
    if (v.about) sec.append(h('p', 'wb-lead', v.about));
    const s = engine!.stage(S, S);
    s.label = v.label;
    const e = s.render(c.id, { ...defaults(c), ...v.props }, st.light, accentIdx(), true, S);
    // the revert repeats the first render exactly: from the same S x S stage (autoH measures from the current size)
    demoSample(s, () => { if (s.update(c.id, { ...defaults(c), ...v.props, opening: false }, st.light, accentIdx()) === '') return; if (s.w !== S || s.h !== S) s.resize(S, S); s.render(c.id, { ...defaults(c), ...v.props, opening: false }, st.light, accentIdx(), true, S); size(); });
    box.append(s.canvas);
    sec.append(box);
    if (e) sec.append(callout('bad', e));
    list.append(sec);
    stages.push(s);
  }
  const size = () => {
    for (const s of stages) {
      const f = st.zoom === 'fit' ? fit(s.w, s.h, list.clientWidth - 2, 1e6) : zoomSize(s.w, s.h, Number(st.zoom));
      Object.assign(s.canvas.style, { width: `${f.width}px`, height: `${f.height}px` });
    }
  };
  (ro = new ResizeObserver(size)).observe(list);
  size();
}

function control(p: Prop, rerender: () => void) {
  if (p.type === 'entity') {
    const row = h('div', 'wb-ctl wb-ctl-ent');
    row.append(h('span', '', p.name), entityPicker({ doms: p.domains ?? [], cur: String(st.props[p.name] ?? ''), options: sources?.options ?? {}, none: "Deck's own", pick: (id) => { st.props[p.name] = id; rerender(); } }));
    const help = propHelp(p);
    if (!help) return row;
    const wrap = h('div', 'wb-ctl-wrap');
    wrap.append(row, h('p', 'wb-ctl-help', help));
    return wrap;
  }
  const row = h('label', 'wb-ctl');
  row.append(h('span', '', p.name));
  let input: HTMLInputElement | HTMLSelectElement;
  const val = st.props[p.name];
  if (p.type === 'select') {
    const s = document.createElement('select');
    for (const o of p.options ?? []) { const op = document.createElement('option'); op.value = o; op.textContent = o || '(none)'; s.append(op); }
    s.value = String(val ?? '');
    input = s;
  } else {
    const i = document.createElement('input');
    i.type = p.type === 'bool' ? 'checkbox' : p.type === 'range' ? 'range' : 'text';
    if (p.type === 'range') { i.min = String(p.min ?? 0); i.max = String(p.max ?? 100); i.step = String(p.step ?? 1); }
    if (p.type === 'bool') i.checked = !!val; else i.value = String(val ?? '');
    input = i;
  }
  const out = h('output', '', p.type === 'range' ? String(val) : '');
  input.addEventListener('input', () => {
    const i = input as HTMLInputElement;
    st.props[p.name] = p.type === 'bool' ? i.checked : p.type === 'range' ? Number(i.value) : i.value;
    if (p.type === 'range') out.textContent = i.value;
    rerender();
  });
  row.append(input, out);
  const help = propHelp(p);
  if (!help) return row;
  const wrap = h('div', 'wb-ctl-wrap');
  wrap.append(row, h('p', 'wb-ctl-help', help));
  return wrap;
}

/** the line under a playground control: the prop's about (catalogue meta), then what it accepts and its default */
type Slot = { name: string; about?: string; accepts?: string[]; max?: number; count?: number; template?: boolean };
/** meta "slots" (CONTRACT C8a): where this component takes other components, what each slot accepts and how many */
function slotsPanel(c: Comp): HTMLElement[] {
  const raw = (c.meta as { slots?: unknown }).slots;  // a list (C8a), or an older layout's plain count (takes anything)
  const slots: Slot[] = Array.isArray(raw) ? raw.filter((x): x is Slot => !!x && typeof x === 'object' && !!(x as Slot).name)
    : typeof raw === 'number' && raw > 0 ? [{ name: 'children', count: raw }] : [];
  if (!slots.length) return [];
  const box = h('div', 'wb-slots');
  for (const x of slots) {
    const row = h('div', 'wb-slot'), top = h('div', 'wb-slot-head');
    const n = x.template ? 'one per item' : x.max != null ? `up to ${x.max}` : x.count != null ? `${x.count} cells` : 'any number';
    top.append(h('strong', '', x.name), h('span', 'wb-slot-n', n));
    const acc = h('div', 'wb-slot-acc');
    for (const a of x.accepts ?? []) {
      const name = engine?.comps.find((k) => k.id === a)?.name ?? a;
      acc.append(h('span', 'wb-slot-chip', name));
    }
    if (!(x.accepts ?? []).length) acc.append(h('span', 'wb-slot-chip any', 'any component'));
    row.append(top, ...(x.about ? [h('p', 'wb-ctl-help', x.about)] : []), acc);
    box.append(row);
  }
  return [h('h2', '', 'Slots'), box];
}

function propHelp(p: Prop): string {
  const accepts =
    p.type === 'select' ? (p.options?.length && p.options.length <= 12 ? `One of: ${p.options.map((o) => o || '(none)').join(', ')}.` : '')
    : p.type === 'range' ? `${p.min ?? 0} to ${p.max ?? 100}${(p.step ?? 1) !== 1 ? `, in steps of ${p.step}` : ''}.`
    : p.type === 'bool' ? 'On or off.'
    : '';
  const d = p.default;
  const def = d === undefined || d === '' ? '' : p.type === 'bool' ? `Default ${d ? 'on' : 'off'}.` : `Default ${String(d)}.`;
  return [p.about?.trim(), accepts, def].filter(Boolean).join(' ');
}

// Sidebar Settings (global, every page): small labelled segmented groups that wrap to the sidebar width, then the
// Send-to-device block for the page's item. URL params + localStorage exactly as before (qs(), wb.sound, wb.target,
// wb.soundVariant). View only on component pages (doc pages have no single preview); Send only where there is an item.
type SendSpec = { view: boolean; comp: Comp; what: [string, () => Record<string, unknown>][] } | null;
const seg = (items: [string, string, boolean][], on: (v: string) => void) => {
  const g = h('div', 'seg');
  for (const [v, label, pressed] of items) {
    const b = h('button', '', label) as HTMLButtonElement;
    b.type = 'button';
    b.dataset.v = v;
    b.setAttribute('aria-pressed', String(pressed));
    b.onclick = () => on(v);
    g.append(b);
  }
  balance(g);
  return g;
};
// balanced wrap: when a segmented group needs more than one line, spread the items evenly (5 = 3 + 2, not 4 + 1)
const balanceRO = new ResizeObserver((es) => { for (const e of es) balanceNow(e.target as HTMLElement); });
const balanceW = new WeakMap<HTMLElement, number>();
function balance(g: HTMLElement) { balanceRO.observe(g); }
function balanceNow(g: HTMLElement) {
  if (balanceW.get(g) === g.clientWidth) return;
  balanceW.set(g, g.clientWidth);
  g.style.display = ''; g.style.gridTemplateColumns = '';
  const n = g.children.length, rows = new Set([...g.children].map((b) => (b as HTMLElement).offsetTop)).size;
  if (rows < 2) return;
  g.style.display = 'grid';
  g.style.gridTemplateColumns = `repeat(${Math.ceil(n / rows)}, minmax(0, 1fr))`;
}
const group = (label: string, ...body: HTMLElement[]) => { const g = h('div', 'wb-grp'), l = h('div', 'wb-grp-label', label); g.dataset.group = label.toLowerCase(); g.append(l, ...body); return g; };
const go = () => history.replaceState(null, '', location.pathname + qs());
// Auto theme: redraw when the workbench theme toggle or the device's colour scheme changes
const themeFollow = () => { if (st.theme === 'auto') { nav(); page(); } };
new MutationObserver(themeFollow).observe(document.documentElement, { attributes: true, attributeFilter: ['data-theme'] });
matchMedia('(prefers-color-scheme: light)').addEventListener('change', themeFollow);
function settings(send: SendSpec) {
  const el = $('wb-settings');
  el.replaceChildren(h('div', 'wb-sec', 'Settings'),
    group('Mode', seg([['auto', 'Auto', st.theme === 'auto'], ['dark', 'Dark', st.theme === 'dark'], ['light', 'Light', st.theme === 'light']], (v) => { st.theme = v as typeof st.theme; go(); nav(); page(); })),
    group('Accent', seg(engine!.tokens.accent.sets.map((a) => [a.id, a.label, (st.accent || engine!.tokens.accent.default) === a.id]), (v) => { st.accent = v === engine!.tokens.accent.default ? '' : v; go(); nav(); page(); })),
    ...(looks().length > 1 ? [themeGroup()] : []),
    soundPick(),
    group('Size', seg(SIZES.map(([d, l]) => [d, l, st.device === d]), async (v) => {
      const den0 = DEVICES[st.device].den;
      st.device = v as DeviceId;
      go();
      if (DEVICES[st.device].den !== den0) await boot(); else { nav(); page(); }
    })),
    group('Pixel grid', seg([['off', 'Off', pixMode === 'off'], ['grid', 'Grid', pixMode === 'grid'], ['boxes', 'Grid + boxes', pixMode === 'boxes'], ['comps', 'Components', pixMode === 'comps'], ['taps', 'Tap areas', pixMode === 'taps']], (v) => { pixMode = v as PixMode; try { localStorage.setItem('wb.pixgrid', v); } catch { /* private mode */ } nav(); page(); })),
    group('Zoom', seg([['1', '1:1', st.zoom === '1'], ['fit', 'Fit', st.zoom === 'fit'], ['2', '2x', st.zoom === '2']], (v) => { st.zoom = v as Zoom; go(); nav(); page(); })),
    ...(send?.view ? [group('View', seg([['screen', 'Screen', st.screen], ['component', 'Component only', !st.screen]], (v) => { st.screen = v === 'screen'; go(); nav(); page(); }))] : []),
    send ? deviceBar(send.comp, send.what) : group('Send to device', h('div', 'wb-none', 'Open a component or foundation to send it.')),
  );
}

// Theme group (sidebar Settings; the panel's Theme = setting `look`): the built-in theme files, Edited (the editor's
// file) once there is one, and Edit = the theme editor (theme-editor.ts: live preview + the guard panel)
function themeGroup() {
  const ids = [...looks(), ...(draft.json ? ['custom'] : [])];
  const s = seg(ids.map((id) => [id, lookName(id), (st.look || 'facet') === id]), (v) => { st.look = v === 'facet' ? '' : v; applyLook(); go(); nav(); page(); });
  const ed = h('button', 'wb-edit-theme', 'Edit') as HTMLButtonElement;
  ed.title = 'Edit a theme file: live preview + the guards a panel runs';
  ed.addEventListener('click', editTheme);
  return group('Theme', s, ed);
}

// Sound group (sidebar Settings): On/Off (component sounds in the browser, per browser: localStorage wb.sound) and the
// variant (pack) = the one shared setting (sound.ts soundVariant: ?sound= + localStorage), used by component taps, the
// Sound page selection and Show on device.
const SPK = '<svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 10v4h4l5 4V6L8 10z"/><path d="M16.5 9.5a4 4 0 0 1 0 5M19 7a7.5 7.5 0 0 1 0 10"/></svg>';
function soundPick() {
  const vars = h('div', 'seg wb-seg-sound');
  const onoff = seg([['1', 'On', soundOn], ['0', 'Off', !soundOn]], (v) => { soundOn = v === '1'; try { localStorage.setItem('wb.sound', soundOn ? '1' : '0'); } catch { /* private mode */ } sync(); });
  onoff.title = 'Component sounds in the browser (taps in the preview and variants)';
  const g = group('Sound', onoff, vars);
  (g.firstChild as HTMLElement).insertAdjacentHTML('afterbegin', SPK);
  const sync = () => { for (const b of onoff.querySelectorAll('button')) b.setAttribute('aria-pressed', String((b.dataset.v === '1') === soundOn)); vars.classList.toggle('off', !soundOn); };
  sync();
  void soundDoc().then((doc) => {
    const btns = doc.variants.map((v) => {
      const b = h('button', '', v.name) as HTMLButtonElement;
      b.type = 'button';
      b.dataset.sound = v.id;
      b.title = `${v.name} sound variant${v.id === doc.default ? ' (default)' : ''}: ${v.about}`;
      b.onclick = () => { setSoundVariant(v.id === doc.default ? '' : v.id); nav(); if (soundOn) void playUi('tap'); };
      return b;
    });
    const mark = () => { const id = pickVariant(doc).id; for (const b of btns) b.setAttribute('aria-pressed', String(b.dataset.sound === id)); };
    mark();
    const off = onSoundVariant(() => { if (!g.isConnected) return void off(); mark(); });
    vars.append(...btns);
  });
  return g;
}

// "Show on device": target picker + what to send (playground / a variant / a doc section) + the button. The button
// says "Needs firmware build" (disabled) when the panel's workbench firmware is older than this component's code.
let devBtn: { btn: HTMLButtonElement; chips: HTMLElement; id: string } | null = null;
function deviceBar(c: Comp, what: [string, () => Record<string, unknown>][]) {
  const wrap = h('div', 'wb-device');
  const tgt = document.createElement('select');
  tgt.setAttribute('aria-label', 'Send target');
  for (const v of TARGETS) { const o = document.createElement('option'); o.value = v; o.textContent = TARGET_NAMES[v]; o.selected = st.target === v; tgt.append(o); }
  tgt.title = 'Waveshare + Guition run the Facet workbench firmware; the Shelly X1i shows it in its kiosk browser (/wasm/device?target=shelly)';
  tgt.onchange = () => { st.target = tgt.value as typeof st.target; try { localStorage.setItem('wb.target', st.target); } catch { /* private mode */ } dev = null; devKey = ''; refreshDevBtn(); (window as unknown as { __wbDevPoll?: () => void }).__wbDevPoll?.(); };
  const sel = document.createElement('select');
  what.forEach(([l], i) => { const o = document.createElement('option'); o.value = String(i); o.textContent = l; sel.append(o); });
  sel.title = 'what to show';
  sel.setAttribute('aria-label', 'What to send');
  const btn = h('button', 'wb-device-btn', 'Send') as HTMLButtonElement;
  btn.type = 'button';
  btn.onclick = async () => {
    const props = what[Number(sel.value) || 0]?.[1]() ?? {};
    btn.disabled = true;
    try {
      const r = await (await fetch(`/api/wasm/show?device=${st.target}`, { method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify({ id: c.id, props, theme: st.light ? 'light' : 'dark', accent: st.accent || engine!.tokens.accent.default, look: st.look === 'custom' ? '' : st.look, sound: pickVariant(await soundDoc()).id }) })).json();
      if (r.results) {
        // All devices: one flash with each target's outcome (the chips show who could take it)
        const res = Object.entries(r.results as Record<string, { ok?: boolean; ms?: number; error?: string; skipped?: string }>);
        flash(r.ok ? `shown on ${res.filter(([, x]) => x.ok).map(([d, x]) => `${TARGET_SHORT[d] ?? d} (${x.ms} ms)`).join(', ')}` + (res.some(([, x]) => !x.ok && !x.skipped) ? `; failed: ${res.filter(([, x]) => !x.ok && !x.skipped).map(([d, x]) => `${TARGET_SHORT[d] ?? d} (${x.error ?? '?'})`).join(', ')}` : '') : 'shown on no device');
        refreshDevBtn();
        return;
      }
      flash(r.ok ? `shown on ${TARGET_NAMES[st.target] ?? st.target} in ${r.ms} ms` : `${TARGET_NAMES[st.target] ?? st.target}: ${r.error}`);
    } catch (e) { flash(`${st.target}: ${String(e)}`); }
    refreshDevBtn();
  };
  // one chip per targeted device: tick = can show this item now, cross = why not (hover; short reason if it fits)
  const chips = h('div', 'wb-device-chips');
  if (what.length > 1) wrap.append(tgt, sel, btn, chips); else wrap.append(tgt, btn, chips);
  devBtn = { btn, chips, id: c.id };
  refreshDevBtn();
  return group('Send to device', wrap);
}
function devReason(n: string, d: DevStatus | undefined, id: string): { ok: boolean; short: string; long: string } {
  const fw = `rebuild and flash the workbench firmware (esphome/build.sh ${PANELS[n]?.board ?? '<board>'}, then flash it)`;
  if (!d) return { ok: false, short: 'checking', long: 'checking the device' };
  if (!d.online) return { ok: false, short: 'offline', long: `offline: ${d.error ?? 'no answer'}` };
  if (d.stale?.[id]) return { ok: false, short: 'needs build', long: `needs firmware build: ${d.stale[id]}; ${fw}` };
  if (d.browser && !d.info?.clients) return { ok: false, short: 'no page open', long: `no device page open: load /wasm/device?target=${n} in the kiosk browser` };
  return { ok: true, short: '', long: d.browser ? `kiosk browser, latest WASM build (${d.info?.clients} page(s) open)` : `firmware built ${d.info?.built ?? '?'}; props, theme and accent need no rebuild` };
}
function refreshDevBtn() {
  if (!devBtn || !devBtn.btn.isConnected) return;
  const { btn, chips, id } = devBtn;
  const names = st.target === 'all' ? TARGETS.filter((t) => t !== 'all') : [st.target];
  const rows = names.map((n) => [n, devReason(n, st.target === 'all' ? dev?.devices?.[n] : dev && !dev.all ? dev : undefined, id)] as const);
  btn.textContent = 'Send';
  btn.disabled = !rows.some(([, r]) => r.ok);
  btn.title = rows.map(([n, r]) => `${TARGET_NAMES[n] ?? n}: ${r.ok ? 'ready' : r.long}`).join('\n');
  (btn.parentElement as HTMLElement).title = btn.title;
  chips.replaceChildren(...rows.map(([n, r]) => {
    const c = h('span', `wb-chip ${r.ok ? 'ok' : 'no'}`);
    c.append(h('span', 'mark', r.ok ? '\u2713' : '\u2717'), h('span', 'name', TARGET_SHORT[n] ?? n));
    if (!r.ok) c.append(h('span', 'why', r.short));
    c.title = `${TARGET_NAMES[n] ?? n}: ${r.long}`;
    return c;
  }));
}

function titleStatus() {
  const key = routeKey(), row = status[key], s = row?.status, wrap = h('span', 'wb-title-wrap'), t = h('span', `wb-title-st ${s ?? 'none'}`);
  wrap.id = 'wb-title-st';
  wrap.append(t);
  if (!s) return wrap;
  t.append(h('span', `wb-st ${s}`), h('span', '', STATUS[s].label)); t.title = `${STATUS[s].hint} (${row.file})`;
  return wrap;
}

const ago = (iso: string) => {
  const m = Math.round((Date.now() - Date.parse(iso)) / 60000);
  return m < 1 ? 'just now' : m < 60 ? `${m} min ago` : m < 1440 ? `${Math.round(m / 60)} h ago` : `${Math.round(m / 1440)} d ago`;
};
/** home: everything not on the contract yet, newest change first (name, section, dot, latest commit subject + when, link) */
function whatsNew() {
  const box = h('section', 'wb-new');
  box.id = 'wb-new';
  const rows = SECTIONS.flatMap((s) => navItems(s.id).map((c) => ({ key: `${s.id}/${c.id}`, sec: s.name, href: `/wasm/${s.id}/${c.id}${qs()}`, name: c.name })))
    .map((x) => ({ ...x, row: status[x.key] })).filter((x) => x.row && x.row.status !== 'done')
    .sort((a, b) => (b.row.last?.at ?? '').localeCompare(a.row.last?.at ?? ''));
  const head = h('div', 'wb-new-head');
  head.append(h('h2', '', 'What\u2019s new'), h('span', 'wb-new-count', rows.length ? `${rows.length} not on the contract yet` : 'all on the contract'));
  box.append(head);
  const list = h('ul', 'wb-new-list');
  for (const x of rows) {
    const li = h('li'), a = h('a', 'wb-new-item') as HTMLAnchorElement;
    a.href = x.href;
    a.title = STATUS[x.row.status].hint;
    const name = h('span', 'wb-new-name', x.name);
    name.prepend(badge(x.key));
    a.append(name, h('span', 'wb-new-sec', x.sec), h('span', 'wb-new-sub', x.row.last?.subject ?? ''), h('span', 'wb-new-when', x.row.last ? ago(x.row.last.at) : ''));
    li.append(a);
    list.append(li);
  }
  box.append(list);
  return box;
}

function callout(kind: 'info' | 'bad', text: string) {
  const c = h('div', `wb-callout ${kind}`);
  c.append(h('span', 'dot'), h('span', '', text));
  return c;
}

function flash(text: string) {
  const b = document.getElementById('wb-build');
  if (!b) return;
  b.textContent = `${engine!.man.density} ${text} · ${engine!.man.built.slice(11, 19)}`;
  b.classList.add('fresh');
  setTimeout(() => b.classList.remove('fresh'), 1500);
}
