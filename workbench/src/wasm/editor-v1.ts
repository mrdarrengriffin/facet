// Facet editor v1 (src/pages/wasm/editor-v1.astro; scope workbench/EDITOR-V1.md), part 1: Screens. The layout: the
// live panel in the middle, a sidebar (a bottom sheet on a phone); the top bar holds the screens as tabs in swipe
// order, + Screen, Undo, Settings and Save · N.
//   Screens   your own screens (a Home page: a grid you fill with cards, part 2) and built-in screens (Clock, Weather…,
//             each its own swipe page, its settings from its spec params); add, name, order (drag a tab, or the
//             arrows), remove, the start page. A fresh panel holds one empty Home.
//   Settings  the schema's settings in their groups (part 3 makes it the panel's own Settings list)
//   Save      Save · N opens the review: the changes in words, the panel's own deck check (fw_spec "check" in the
//             WASM engine) with every error said against its screen; Save keeps it as what "the panel" runs (this
//             page's memory); Undo goes back to what ran before the last save; Back to factory = one empty Home.
// The previews are the real renderer (engine.ts) with Home Assistant stubbed (no states: sample data). Nothing leaves
// the page.
import { Engine, Stage, DEVICES, type DeviceId, type HaBridge } from './engine';

type Param = { about?: string; type: string; options?: string[]; label?: string; default?: string; domain?: string };
type Screen = { name: string; params: Record<string, Param> };
type Opt = { id: string; label: string; hint?: string };
type Setting = { id: string; group: string; label: string; help?: string; kind: string; control?: string; default: unknown;
  options?: Opt[]; min?: number; max?: number; step?: number; unit?: string; presets?: number[]; zero?: string };
type Header = { button?: string; status?: string; buttons?: unknown[] };
/** a deck page: kind "home" = your own screen (cards), else a built-in screen (its id = its kind, once per deck) */
type Page = { id: string; kind: string; name?: string; source?: string; params?: Record<string, string>; cards?: unknown[]; header_actions?: Header };
type Boot = {
  screens: Record<string, Screen>; settings: { groups: { id: string; label: string }[]; settings: Setting[] };
  deck: { start: string; pages: Page[] };
  options: Record<string, { id: string; name: string }[]>;
  accents: { default: string; sets: { id: string; label: string; dark: { accent: string } }[] };
  scope: string; hash: string; own_max: number;
};
/** what the editor holds (and what "the panel" runs, the same shape): the deck's pages in swipe order + the panel's settings */
type Draft = { pages: Page[]; start: string; settings: Record<string, unknown>; ha: { url: string; token: string } };

const boot: Boot = JSON.parse(document.getElementById('v1-boot')!.textContent!);
const $ = (id: string) => document.getElementById(id)!;
function h<K extends keyof HTMLElementTagNameMap>(tag: K, cls = '', text = ''): HTMLElementTagNameMap[K] {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text) e.textContent = text;
  return e;
}
const clone = <T>(x: T): T => JSON.parse(JSON.stringify(x));
const own = (p: Page) => p.kind === 'home';
const pageOf = (d: Draft, id: string) => d.pages.find((p) => p.id === id);
const pname = (p: Page | undefined) => (!p ? '?' : p.name || (own(p) ? 'Home' : boot.screens[p.kind]?.name ?? p.kind));
const name = (id: string, d: Draft = draft) => pname(pageOf(d, id));
const noHa: HaBridge = { states: async () => null, action: async () => undefined, ask: async () => null };
const defaults = () => Object.fromEntries(boot.settings.settings.map((s) => [s.id, s.default]));

// ---- the factory, the panel, the draft
/** a fresh panel: one empty Home of your own */
function factory(): Draft {
  return { pages: [{ id: 'home', kind: 'home', name: 'Home', cards: [] }], start: 'home', settings: defaults(), ha: { url: '', token: '' } };
}
/** what panels run today (specs/deck.json), so the mock starts from a real deck */
function today(): Draft {
  const pages = boot.deck.pages.filter((p) => own(p) || boot.screens[p.kind]).map((p) => clone(p));
  return { pages, start: pages.some((p) => p.id === boot.deck.start) ? boot.deck.start : pages[0].id, settings: defaults(), ha: { url: '', token: '' } };
}
const panel = { rev: 3, runs: today(), before: [] as { rev: number; runs: Draft }[] };
let draft = clone(panel.runs);

function paramVal(p: Page, k: string) { return p.params?.[k] ?? boot.screens[p.kind].params[k].default ?? ''; }
function toDeck(d: Draft, rev: number) {
  const pages = d.pages.map((p) => {
    const o: Page = { ...clone(p) };
    if (!own(p) && o.params) {
      const set = Object.entries(o.params).filter(([k, v]) => v !== (boot.screens[p.kind].params[k]?.default ?? ''));
      if (set.length) o.params = Object.fromEntries(set); else delete o.params;
    }
    return o;
  });
  return { v: 2, rev, start: d.start, pages };
}
/** a page id from a name: a-z 0-9 _, at most 12, unique in the draft */
function newId(n: string) {
  const base = n.toLowerCase().replace(/[^a-z0-9]+/g, '_').replace(/^_+|_+$/g, '').slice(0, 10) || 'screen';
  const taken = new Set(draft.pages.map((p) => p.id));
  if (!taken.has(base) && !boot.screens[base]) return base;
  for (let i = 2; ; i++) if (!taken.has(`${base}_${i}`)) return `${base}_${i}`;
}

// ---- what changed, in words
type Change = { where: string; what: string; was: string; now: string };
function changes(a: Draft, b: Draft): Change[] {
  const out: Change[] = [];
  const ia = new Set(a.pages.map((p) => p.id)), ib = new Set(b.pages.map((p) => p.id));
  for (const p of b.pages) if (!ia.has(p.id)) out.push({ where: pname(p), what: own(p) ? 'Added your screen' : 'Added the built-in screen', was: '', now: '' });
  for (const p of a.pages) if (!ib.has(p.id)) out.push({ where: pname(p), what: 'Removed', was: '', now: '' });
  const keptA = a.pages.filter((p) => ib.has(p.id)).map((p) => p.id), keptB = b.pages.filter((p) => ia.has(p.id)).map((p) => p.id);
  if (keptA.join() !== keptB.join()) out.push({ where: 'Screens', what: 'Swipe order', was: keptA.map((id) => name(id, a)).join(', '), now: keptB.map((id) => name(id, b)).join(', ') });
  if (a.start !== b.start) out.push({ where: 'Screens', what: 'Start page', was: name(a.start, a), now: name(b.start, b) });
  for (const pb of b.pages) {
    const pa = pageOf(a, pb.id);
    if (!pa) continue;
    if (pname(pa) !== pname(pb)) out.push({ where: pname(pb), what: 'Name', was: pname(pa), now: pname(pb) });
    if (own(pb)) {
      const ha = pa.header_actions ?? {}, hb = pb.header_actions ?? {};
      if ((ha.button ?? 'settings') !== (hb.button ?? 'settings')) out.push({ where: pname(pb), what: 'Header button', was: hbName(ha.button, a), now: hbName(hb.button, b) });
      if ((ha.status ?? 'none') !== (hb.status ?? 'none')) out.push({ where: pname(pb), what: 'Header status', was: ha.status === 'timer' ? 'Timer' : 'Nothing', now: hb.status === 'timer' ? 'Timer' : 'Nothing' });
      continue;
    }
    for (const [k, p] of Object.entries(boot.screens[pb.kind].params)) {
      if (p.type === 'hidden') continue;
      const va = paramVal(pa, k), vb = paramVal(pb, k);
      if (va !== vb) out.push({ where: pname(pb), what: p.label ?? k, was: va || 'none', now: vb || 'none' });
    }
  }
  if (a.ha.url !== b.ha.url) out.push({ where: 'Home Assistant', what: 'Address', was: a.ha.url || 'not set', now: b.ha.url || 'not set' });
  if (a.ha.token !== b.ha.token) out.push({ where: 'Home Assistant', what: 'Token', was: hint(a.ha.token), now: hint(b.ha.token) });
  for (const s of boot.settings.settings) if (a.settings[s.id] !== b.settings[s.id]) out.push({ where: 'Settings', what: s.label, was: say(s, a.settings[s.id]), now: say(s, b.settings[s.id]) });
  return out;
}
const hint = (t: string) => (t ? `…${t.slice(-4)}` : 'not set');  // a token is never shown back whole
const hbName = (b: string | undefined, d: Draft) => (!b || b === 'settings' ? 'Settings' : name(b, d));
function say(s: Setting, v: unknown) {
  if (s.options) return s.options.find((o) => o.id === v)?.label ?? String(v);
  if (s.kind === 'switch') return v ? 'on' : 'off';
  if (s.zero && v === 0) return s.zero;
  return `${v}${s.unit ? ` ${s.unit}` : ''}`;
}

// ---- the panel's own check, its errors in words
type Err = { path: string; code: string; msg: string };
function check(): { ok: boolean; errors: { where: string; what: string; msg: string }[] } {
  if (!engine) return { ok: false, errors: [{ where: 'Editor', what: '', msg: 'the renderer is still loading' }] };
  const d = toDeck(draft, panel.rev + 1), e = engine;
  const r = JSON.parse(e.withStr('check', (pc) => e.withStr(JSON.stringify(d), (pj) => e.str(e.m._fw_spec(pc, pj))))) as { ok: boolean; errors: Err[] };
  return { ok: r.ok, errors: (r.errors ?? []).map((x) => {
    const m = x.path.match(/^\.pages\[(\d+)\](?:\.(params)\.(\w+)|\.(header_actions)(?:\.(\w+))?|\.(cards)\[(\d+)\]|\.(\w+))?/);
    const pg = m ? d.pages[+m[1]] : null;
    const where = pg ? pname(pg) : x.path.startsWith('.start') ? 'Start page' : 'Screens';
    const what = !pg || !m ? '' : m[3] ? boot.screens[pg.kind]?.params[m[3]]?.label ?? m[3] : m[4] ? 'Header' : m[6] ? `Card ${+m[7] + 1}` : '';
    return { where, what, msg: x.msg };
  }) };
}

// ---- engine + previews
let device: DeviceId = new URLSearchParams(location.search).get('device') === 'guition' ? 'guition' : 'waveshare';
let engine: Engine | null = null;
let sel = draft.start;                                     // the screen on the live panel
let side: 'screen' | 'add' | 'settings' | 'review' | 'saved' = 'screen';  // what the sidebar holds (a bottom sheet on a phone)
let savedAt = '';
const stages: Stage[] = [];
const S = () => DEVICES[device].size;
/** the panel's Mode; Auto follows this computer's light / dark (the mock has no sun). The page wears the same mode. */
const light = () => draft.settings.theme === 'light' || (draft.settings.theme === 'auto' && matchMedia('(prefers-color-scheme: light)').matches);
const accent = () => boot.accents.sets.findIndex((a) => a.id === draft.settings.accent);
function applyDeck() {
  const e = engine;
  if (!e) return;
  e.withStr('deck', (pc) => e.withStr(JSON.stringify(toDeck(draft, panel.rev)), (pj) => e.str(e.m._fw_spec(pc, pj))));
}
function preview(p: Page, cssW: number, label: string) {
  const s = engine!.stage(S(), S());
  s.label = label;
  stages.push(s);
  s.canvas.style.width = `${cssW}px`;
  s.canvas.style.height = `${cssW}px`;
  s.canvas.className = 'v1-canvas';
  if (own(p)) s.render('home', { page: p.id }, light(), accent());
  else s.render('spec', { screen: p.kind, ...Object.fromEntries(Object.keys(boot.screens[p.kind].params).map((k) => [k, paramVal(p, k)])) }, light(), accent());
  return s;
}
function freeStages() { for (const s of stages.splice(0)) s.free(); }

// ---- the page
function edit(f: () => void) { f(); if (!pageOf(draft, sel)) sel = draft.start; applyDeck(); top(); render(); }
function go(id: string) { sel = id; side = 'screen'; top(); render(); }
function top() {
  const dev = $('v1-device');
  dev.replaceChildren(...(['waveshare', 'guition'] as DeviceId[]).map((d) => {
    const b = h('button', '', DEVICES[d].name);
    b.type = 'button';
    b.setAttribute('aria-pressed', String(d === device));
    b.onclick = () => { if (d !== device) { device = d; void load(); } };
    return b;
  }));
  const tabs = $('v1-tabs');
  const strip = h('div', 'v1-strip');  // the tabs scroll; + Screen stays in view
  strip.append(...draft.pages.map((p) => tab(p)));
  tabs.replaceChildren(strip);
  const add = h('button', 'v1-add', '+ Screen');
  add.type = 'button';
  add.setAttribute('aria-current', String(side === 'add'));
  add.onclick = () => { side = 'add'; top(); render(); };
  tabs.append(add);
  const n = changes(panel.runs, draft).length;
  const save = $('v1-save') as HTMLButtonElement;
  save.textContent = n ? `Save · ${n}` : 'Saved';
  save.disabled = !engine || !n;
  const undo = $('v1-undo') as HTMLButtonElement;
  undo.disabled = !panel.before.length;
  undo.title = panel.before.length ? 'Go back to what the panel ran before your last save' : 'Nothing saved yet';
  $('v1-settings').setAttribute('aria-pressed', String(side === 'settings'));
  document.documentElement.dataset.theme = light() ? 'light' : 'dark';
  $('v1-where').textContent = `${DEVICES[device].name} · ${S()} × ${S()}`;
  $('v1-title').textContent = pname(pageOf(draft, sel));
}
let dragging = '';
function tab(p: Page) {
  const b = h('button', 'v1-tab', pname(p));
  b.type = 'button';
  b.setAttribute('aria-current', String(p.id === sel && side !== 'add'));
  if (p.id === draft.start) { const st = h('span', 'v1-star', '★'); st.title = 'Start page'; b.prepend(st); }
  if (!own(p)) b.classList.add('builtin');
  b.onclick = () => go(p.id);
  b.draggable = true;
  b.ondragstart = (e) => { dragging = p.id; b.classList.add('lift'); e.dataTransfer?.setData('text/plain', p.id); };
  b.ondragend = () => { dragging = ''; b.classList.remove('lift'); };
  b.ondragover = (e) => { if (dragging && dragging !== p.id) { e.preventDefault(); b.classList.add('over'); } };
  b.ondragleave = () => b.classList.remove('over');
  b.ondrop = (e) => {
    e.preventDefault();
    const from = dragging;
    if (!from || from === p.id) return;
    edit(() => {
      const ids = draft.pages.map((x) => x.id);
      const after = ids.indexOf(from) < ids.indexOf(p.id);
      const moved = draft.pages.find((x) => x.id === from)!;
      const rest = draft.pages.filter((x) => x.id !== from);
      rest.splice(rest.findIndex((x) => x.id === p.id) + (after ? 1 : 0), 0, moved);
      draft.pages = rest;
    });
  };
  return b;
}

function render() {
  if (!engine) return;
  freeStages();
  const main = $('v1-main');
  const p = pageOf(draft, sel)!;
  const stage = h('div', 'v1-stage');
  const big = Math.min(S(), 600);
  const frame = h('div', 'v1-frame');
  frame.append(preview(p, big, 'Live panel').canvas);
  stage.append(frame, h('p', 'v1-meta', `Live panel · ${pname(p)} · ${S()} × ${S()}${big < S() ? ` at ${Math.round((big / S()) * 100)}%` : ''}`));
  const sb = h('aside', 'v1-side');
  sb.append(h('i', 'v1-grab'));
  const body = side === 'add' ? addSide() : side === 'settings' ? settingsSide() : side === 'review' ? reviewSide() : side === 'saved' ? savedSide() : screenSide(p);
  sb.setAttribute('aria-label', body[0].querySelector('b')?.textContent ?? '');
  sb.append(...body);
  main.replaceChildren(stage, sb);
}

/** a sidebar's head: title, a quiet line under it, and ✕ back to the screen when it is a passing view */
function sideHead(t: string, sub: string, close = true) {
  const hd = h('div', 'v1-sidehead');
  const tx = h('div', 'v1-sidetitle');
  tx.append(h('b', '', t), h('span', 'v1-meta', sub));
  hd.append(tx);
  if (close) { const x = btn('✕', 'Close', false, () => { side = 'screen'; top(); render(); }, 'icon'); hd.append(x); }
  return hd;
}

// 1. Screens: the selected screen
function screenSide(p: Page): HTMLElement[] {
  const out: HTMLElement[] = [];
  out.push(sideHead(pname(p), `${own(p) ? 'Your screen' : 'Built-in screen'} · ${draft.pages.indexOf(p) + 1} of ${draft.pages.length} in the swipe`, false));
  const nm = h('input', 'v1-input');
  nm.value = pname(p);
  nm.maxLength = 32;
  nm.setAttribute('aria-label', 'Name');
  nm.onchange = () => {
    const v = nm.value.trim();
    edit(() => { if (own(p)) p.name = v || 'Home'; else if (v && v !== boot.screens[p.kind].name) p.name = v; else delete p.name; });
  };
  out.push(field('Name', own(p) ? 'Its tab here and the title on the panel.' : 'Leave it to use the screen\'s own name.', nm));
  const i = draft.pages.indexOf(p), n = draft.pages.length;
  const order = h('div', 'v1-row');
  order.append(
    btn('← Earlier', `Move ${pname(p)} one place earlier`, i === 0, () => edit(() => { draft.pages.splice(i, 1); draft.pages.splice(i - 1, 0, p); })),
    btn('Later →', `Move ${pname(p)} one place later`, i === n - 1, () => edit(() => { draft.pages.splice(i, 1); draft.pages.splice(i + 1, 0, p); })),
  );
  out.push(field('Swipe order', 'Drag its tab, or move it here.', order));
  const start = btn(p.id === draft.start ? '★ The panel opens on this' : 'Open the panel on this', 'Start page', p.id === draft.start, () => edit(() => { draft.start = p.id; }));
  out.push(field('Start page', undefined, start));
  if (own(p)) {
    const cards = p.cards?.length ?? 0;
    out.push(h('p', 'v1-note', cards ? `${cards} card${cards === 1 ? '' : 's'} on this screen. Adding and arranging cards comes in part 2.` : 'An empty grid. Adding cards comes in part 2.'));
    const hdr = p.header_actions ?? {};
    const setH = (k: 'button' | 'status', v: string, dflt: string) => edit(() => {
      const x = { ...(p.header_actions ?? {}) };
      if (v === dflt) delete x[k]; else x[k] = v;
      if (Object.keys(x).length) p.header_actions = x; else delete p.header_actions;
    });
    out.push(h('h2', 'cap', 'Header'));
    out.push(field('Button', 'Top right: Settings, or a screen it opens.', list([['settings', 'Settings'], ...draft.pages.filter((x) => x.id !== p.id).map((x) => [x.id, pname(x)] as [string, string])], hdr.button ?? 'settings', (v) => setH('button', v, 'settings'))));
    out.push(field('Status', 'Next to the time: nothing, or a running timer.', seg([['none', 'Nothing'], ['timer', 'Timer']], hdr.status ?? 'none', (v) => setH('status', v, 'none'))));
  } else {
    const ps = Object.entries(boot.screens[p.kind].params).filter(([, x]) => x.type !== 'hidden');
    if (ps.length) {
      out.push(h('h2', 'cap', 'Its settings'));
      for (const [k, x] of ps) out.push(field(x.label ?? k, x.about, paramControl(p, k, x)));
      out.push(btn('Back to defaults', `${pname(p)} back to defaults`, !p.params || !Object.keys(p.params).length, () => edit(() => { delete p.params; }), 'ghost'));
    } else out.push(h('p', 'dim', 'This screen has nothing to set.'));
  }
  const rm = btn('Remove this screen', `Remove ${pname(p)}`, n === 1, () => edit(() => {
    draft.pages = draft.pages.filter((x) => x !== p);
    if (draft.start === p.id) draft.start = draft.pages[0].id;
    sel = draft.pages[Math.max(0, i - 1)].id;
  }), 'ghost danger');
  out.push(h('div', 'v1-gap'), rm);
  if (n === 1) out.push(h('p', 'dim', 'The panel needs at least one screen.'));
  return out;
}

// 1. Screens: add one
function addSide(): HTMLElement[] {
  const out: HTMLElement[] = [];
  out.push(sideHead('New screen', 'It goes at the end of the swipe'));
  const owns = draft.pages.filter(own).length, full = owns >= boot.own_max;
  out.push(h('h2', 'cap', 'Your own screen'));
  const nm = h('input', 'v1-input');
  nm.placeholder = 'Kitchen, Gaming, TV…';
  nm.maxLength = 32;
  nm.setAttribute('aria-label', 'Name of the new screen');
  const add = btn('Add', 'Add your own screen', full, () => {
    const v = nm.value.trim() || `Screen ${owns + 1}`;
    const id = newId(v);
    edit(() => { draft.pages.push({ id, kind: 'home', name: v, cards: [] }); sel = id; side = 'screen'; });
  }, 'primary');
  nm.onkeydown = (e) => { if (e.key === 'Enter') add.click(); };
  const r = h('div', 'v1-row');
  r.append(nm, add);
  out.push(field('Name', full ? `A panel holds ${boot.own_max} of your own screens.` : `A grid you fill with cards. ${owns} of ${boot.own_max} used.`, r));
  out.push(h('h2', 'cap', 'Built-in screens'));
  const have = new Set(draft.pages.map((p) => p.kind));
  const free = Object.entries(boot.screens).filter(([k]) => !have.has(k));
  if (!free.length) out.push(h('p', 'dim', 'Every built-in screen is on the panel.'));
  const grid = h('div', 'v1-builtins');
  for (const [k, s] of free) {
    const b = h('button', 'v1-builtin');
    b.type = 'button';
    b.setAttribute('aria-label', `Add ${s.name}`);
    b.append(preview({ id: k, kind: k }, 96, `add:${k}`).canvas, h('span', '', s.name));
    b.onclick = () => edit(() => { draft.pages.push({ id: k, kind: k }); sel = k; side = 'screen'; });
    grid.append(b);
  }
  out.push(grid);
  out.push(h('p', 'dim', 'A built-in screen is never rebuilt from cards: it is its own swipe page, once per panel.'));
  return out;
}

/** a setting as the panel's Settings draws it: the label (and a quiet hint) on the left, a small control or the
 *  current value on the right; a wide control (a field, a slider, many choices) goes under the label */
const opened = new Set<string>();  // pickers left open across re-renders
function field(label: string, about: string | undefined, control: HTMLElement) {
  const narrow = control.matches('.v1-switch, .v1-pick, .btn') || (control.matches('.seg') && control.children.length <= 3);
  const f = h('div', `v1-field${narrow ? ' inline' : ''}`);
  const txt = h('div', 'v1-ftxt');
  txt.append(h('label', '', label));
  if (about && !narrow) txt.append(h('p', '', about));  // a row on the panel carries no hint: a short row keeps it as a tooltip
  else if (about) f.title = about;
  f.append(txt, control);
  const opts = (control as HTMLElement & { opts?: HTMLElement }).opts;
  if (opts) {
    const key = label;
    const show = (on: boolean) => { f.classList.toggle('open', on); control.setAttribute('aria-expanded', String(on)); on ? opened.add(key) : opened.delete(key); };
    control.onclick = () => show(!f.classList.contains('open'));
    f.append(opts);
    show(opened.has(key));
  }
  return f;
}
function btn(t: string, label: string, disabled: boolean, f: () => void, cls = '') {
  const b = h('button', `btn ${cls}`.trim(), t);
  b.type = 'button';
  b.disabled = disabled;
  b.title = label;
  b.onclick = f;
  return b;
}
function setParam(p: Page, k: string, v: string) { edit(() => { (p.params ??= {})[k] = v; }); }
function paramControl(pg: Page, k: string, p: Param): HTMLElement {
  const v = paramVal(pg, k);
  if (p.type === 'select' && p.options) return p.options.length <= 3 ? seg(p.options.map((o) => [o, o === 'ha' ? 'Home Assistant' : o === 'sample' ? 'Sample' : o]), v, (x) => setParam(pg, k, x)) : list(p.options.map((o) => [o, o]), v, (x) => setParam(pg, k, x));
  if (p.type === 'entity') return entityPick(p.domain ?? '', v, (x) => setParam(pg, k, x));
  if (p.type === 'entities') {
    const box = h('div', 'v1-multi');
    const have = new Set(v.split(',').filter(Boolean));
    const opts = boot.options[p.domain ?? ''] ?? [];
    if (!opts.length) box.append(h('span', 'dim', `No ${p.domain} entities in this house`));
    for (const o of opts) {
      const l = h('label', 'v1-check');
      const c = h('input');
      c.type = 'checkbox';
      c.checked = have.has(o.id);
      c.onchange = () => { c.checked ? have.add(o.id) : have.delete(o.id); setParam(pg, k, [...have].join(',')); };
      l.append(c, document.createTextNode(` ${o.name}`), h('span', 'dim mono', ` ${o.id}`));
      box.append(l);
    }
    return box;
  }
  const t = h('input', 'v1-input');
  t.value = v;
  t.onchange = () => setParam(pg, k, t.value);
  return t;
}
function entityPick(domain: string, v: string, set: (x: string) => void) {
  const opts = boot.options[domain] ?? [];
  const items: [string, string, string?][] = opts.map((o) => [o.id, o.name, o.id]);
  if (v && !opts.some((o) => o.id === v)) items.unshift([v, `${v} (not found)`]);
  return list(items, v, set, `Pick a ${domain.replace('_', ' ')}`);
}
function seg(items: [string, string][], cur: string, pick: (v: string) => void) {
  const g = h('div', 'seg');
  g.setAttribute('role', 'group');
  for (const [v, l] of items) {
    const b = h('button', '', l);
    b.type = 'button';
    b.setAttribute('aria-pressed', String(v === cur));
    b.onclick = () => pick(v);
    g.append(b);
  }
  return g;
}
/** a picker row: the current value + › on the right; tapped, its choices open under it as radio rows */
function list(items: [string, string, string?][], cur: string, pick: (v: string) => void, empty = 'Pick one') {
  const b = h('button', 'v1-pick') as HTMLButtonElement & { opts?: HTMLElement };
  b.type = 'button';
  b.append(h('span', '', items.find(([v]) => v === cur)?.[1] ?? (cur || empty)), h('i', 'chev', '›'));
  const ul = h('div', 'v1-opts');
  ul.setAttribute('role', 'radiogroup');
  for (const [v, l, sub] of items) {
    const o = h('button', 'v1-opt');
    o.type = 'button';
    o.setAttribute('role', 'radio');
    o.setAttribute('aria-checked', String(v === cur));
    const t = h('span', 'v1-ftxt');
    t.append(h('span', '', l));
    if (sub) t.append(h('p', '', sub));
    o.append(h('i', 'radio'), t);
    o.onclick = () => { opened.clear(); pick(v); };
    ul.append(o);
  }
  if (!items.length) ul.append(h('p', 'v1-meta', 'Nothing to pick here'));
  b.opts = ul;
  return b;
}
function toggle(on: boolean, label: string, disabled: boolean, f: () => void) {
  const b = h('button', 'v1-switch');
  b.type = 'button';
  b.setAttribute('role', 'switch');
  b.setAttribute('aria-checked', String(on));
  b.setAttribute('aria-label', label);
  b.title = label;
  b.disabled = disabled;
  b.onclick = (e) => { e.stopPropagation(); f(); };
  return b;
}

// 3. Settings (part 3 makes this the panel's own Settings list)
function settingsSide(): HTMLElement[] {
  const out: HTMLElement[] = [];
  out.push(sideHead('Settings', `${DEVICES[device].name} · every screen`));
  out.push(h('h2', 'cap', 'Home Assistant'));
  const url = h('input', 'v1-input');
  url.placeholder = 'http://homeassistant.local:8123';
  url.value = draft.ha.url;
  url.onchange = () => edit(() => { draft.ha.url = url.value.trim(); });
  out.push(field('Address', "Home Assistant's address, as you open it in a browser at home (http or https).", url));
  const tok = h('input', 'v1-input');
  tok.type = 'password';
  tok.autocomplete = 'off';
  tok.placeholder = draft.ha.token ? `set (${hint(draft.ha.token)})` : 'paste a long-lived access token';
  tok.onchange = () => { if (tok.value.trim()) edit(() => { draft.ha.token = tok.value.trim(); }); };
  out.push(field('Token', 'In Home Assistant: your profile > Security > Long-lived access tokens > Create token. It stays on the panel and is never shown again.', tok));
  for (const g of boot.settings.groups) {
    out.push(h('h2', 'cap', g.label));
    for (const s of boot.settings.settings.filter((x) => x.group === g.id)) out.push(field(s.label, s.help, settingControl(s)));
  }
  out.push(h('h2', 'cap', 'Start again'), factoryBtn());
  return out;
}
function setSetting(id: string, v: unknown) { edit(() => { draft.settings[id] = v; }); }
function settingControl(s: Setting): HTMLElement {
  const v = draft.settings[s.id];
  if (s.control === 'swatches' && s.options) {
    const g = h('div', 'v1-swatches');
    for (const o of s.options) {
      const b = h('button', '');
      b.type = 'button';
      b.title = o.label;
      b.setAttribute('aria-label', o.label);
      b.setAttribute('aria-pressed', String(o.id === v));
      b.style.background = boot.accents.sets.find((a) => a.id === o.id)?.dark.accent ?? 'var(--f-accent)';
      b.onclick = () => setSetting(s.id, o.id);
      g.append(b);
    }
    return g;
  }
  if (s.options) return s.control === 'segmented' ? seg(s.options.map((o) => [o.id, o.label]), String(v), (x) => setSetting(s.id, x)) : list(s.options.map((o) => [o.id, o.label, o.hint]), String(v), (x) => setSetting(s.id, x));
  if (s.kind === 'switch') return toggle(!!v, s.label, false, () => setSetting(s.id, !v));
  if (s.control === 'presets' && s.presets) return seg(s.presets.map((p) => [String(p), p === 0 && s.zero ? s.zero : p >= 60 && s.unit === 's' ? `${p / 60} min` : `${p}${s.unit ?? ''}`]), String(v), (x) => setSetting(s.id, Number(x)));
  const r = h('div', 'v1-range');
  const i = h('input');
  i.type = 'range';
  i.min = String(s.min ?? 0);
  i.max = String(s.max ?? 100);
  i.step = String(s.step ?? 1);
  i.value = String(v);
  const out = h('output', '', say(s, v));
  i.oninput = () => { out.textContent = say(s, Number(i.value)); };
  i.onchange = () => setSetting(s.id, Number(i.value));
  r.append(i, out);
  return r;
}

// 4. Save: Save · N opens the review in the sidebar: the changes in words, the panel's check, Save; Undo; Back to factory
function reviewSide(): HTMLElement[] {
  const out: HTMLElement[] = [];
  const ch = changes(panel.runs, draft);
  out.push(sideHead(ch.length ? `${ch.length} change${ch.length === 1 ? '' : 's'}` : 'Nothing to save', savedAt ? `since you saved at ${savedAt}` : 'since the panel last saved'));
  const ul = h('ul', 'v1-changes');
  for (const c of ch) {
    const li = h('li');
    li.append(h('b', '', c.what ? `${c.where} · ${c.what}` : c.where));
    if (c.was || c.now) { const w = h('span', 'v1-meta'); w.append(h('s', '', c.was), document.createTextNode(' → '), h('span', 'now', c.now)); li.append(w); }
    ul.append(li);
  }
  out.push(ul);
  const r = check();
  out.push(h('h2', 'cap', r.ok ? 'The panel checked them' : `The panel can't take this yet · ${r.errors.length}`));
  const ck = h('ul', 'v1-checks');
  if (r.ok) { const li = h('li', 'ok'); li.append(h('i', 'dot'), document.createTextNode('All good: every screen and card is valid.')); ck.append(li); }
  for (const e of r.errors) { const li = h('li', 'bad'); li.append(h('i', 'dot'), h('b', '', e.what ? `${e.where} · ${e.what}` : e.where), document.createTextNode(` ${e.msg}`)); ck.append(li); }
  out.push(ck);
  if (!r.ok) out.push(h('p', 'v1-meta', 'Fix these, then Save. Your changes are kept.'));
  const acts = h('div', 'v1-acts');
  acts.append(btn('Not yet', 'Keep editing', false, () => { side = 'screen'; top(); render(); }, 'ghost'), btn('Save', 'Save to the panel', !r.ok || !ch.length, () => {
    panel.before.push({ rev: panel.rev, runs: panel.runs });
    panel.rev++;
    panel.runs = clone(draft);
    savedAt = new Date().toTimeString().slice(0, 5);
    side = 'saved';
    top(); render();
  }, 'primary'));
  out.push(acts);
  return out;
}
function savedSide(): HTMLElement[] {
  const n = panel.before.length ? changes(panel.before.at(-1)!.runs, panel.runs).length : 0;
  const note = h('div', 'v1-note ok');
  note.append(h('i', 'dot'), document.createTextNode(`The panel shows ${n === 1 ? 'it' : 'them'} now. Undo, top right, goes back to what it ran before.`));
  const acts = h('div', 'v1-acts');
  acts.append(btn('Done', 'Back to the screen', false, () => { side = 'screen'; top(); render(); }, 'primary'));
  return [sideHead('Saved', `${n} change${n === 1 ? '' : 's'} at ${savedAt}`), note, acts];
}
function undo() {
  const b = panel.before.pop();
  if (!b) return;
  panel.rev++;
  panel.runs = clone(b.runs);
  draft = clone(b.runs);
  if (side === 'saved' || side === 'review') side = 'screen';
  edit(() => {});
}
function factoryBtn() {
  return btn('Back to factory', 'One empty Home, default settings', false, () => {
    if (!confirm('Put the panel back to one empty Home with default settings? What you saved is replaced (Undo brings it back).')) return;
    panel.before.push({ rev: panel.rev, runs: panel.runs });
    panel.rev++;
    panel.runs = factory();
    draft = factory();
    sel = draft.start;
    side = 'screen';
    edit(() => {});
  }, 'danger');
}

// the scope, from workbench/EDITOR-V1.md (headings, lists, bold, code)
function scope() {
  const d = $('v1-dialog') as HTMLDialogElement;
  const box = h('div', 'dlg v1-doc');
  const inline = (s: string) => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/\*\*(.+?)\*\*/g, '<b>$1</b>').replace(/`(.+?)`/g, '<code>$1</code>');
  let ul: HTMLElement | null = null;
  for (const raw of boot.scope.split(/\n(?=\S)/)) {
    const l = raw.replace(/\n\s+/g, ' ').trim();
    if (!l) continue;
    const li = l.match(/^(?:-|\d+\.) (.*)$/);
    if (li) { if (!ul) { ul = h(l.startsWith('-') ? 'ul' : 'ol'); box.append(ul); } const e = h('li'); e.innerHTML = inline(li[1]); ul.append(e); continue; }
    ul = null;
    const hd = l.match(/^(#+) (.*)$/);
    const e = hd ? h(hd[1].length === 1 ? 'h2' : 'h3') : h('p');
    e.innerHTML = inline(hd ? hd[2] : l);
    box.append(e);
  }
  const acts = h('div', 'acts');
  acts.append(btn('Close', 'Close', false, () => d.close(), 'primary'));
  box.append(acts);
  d.replaceChildren(box);
  d.showModal();
}

async function load() {
  freeStages();
  engine?.dispose();
  engine = null;
  top();
  $('v1-main').replaceChildren(h('p', 'dim', 'Loading the renderer'));
  engine = await Engine.load(DEVICES[device].den, `/facet-wasm/${DEVICES[device].den}/`, noHa);
  applyDeck();
  top();
  render();
}

export function start() {
  $('v1-save').onclick = () => { side = 'review'; top(); render(); };
  $('v1-undo').onclick = undo;
  $('v1-settings').onclick = () => { side = side === 'settings' ? 'screen' : 'settings'; top(); render(); };
  $('v1-scope').onclick = scope;
  (window as any).__v1 = { draft: () => draft, load: (d: Draft) => edit(() => { draft = d; sel = d.start; }), panel: () => panel, deck: () => toDeck(draft, panel.rev), check, changes: () => changes(panel.runs, draft) };  // tests
  void load();
}
