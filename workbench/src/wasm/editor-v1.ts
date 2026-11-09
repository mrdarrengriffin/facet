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
type Card = { t: string; e?: string; size: string; tap?: string; page?: string; label?: string };
type Page = { id: string; kind: string; name?: string; source?: string; params?: Record<string, string>; cards?: Card[]; header_actions?: Header };
type CardDef = { domains: string; taps: string; detail: string; by_domain?: Record<string, { taps: string; detail?: string }> };
type Boot = {
  screens: Record<string, Screen>; settings: { groups: { id: string; label: string }[]; settings: Setting[] };
  deck: { start: string; pages: Page[] };
  options: Record<string, { id: string; name: string }[]>;
  accents: { default: string; sets: { id: string; label: string; dark: { accent: string } }[] };
  scope: string; hash: string; own_max: number;
  cards: Record<string, CardDef>; grid_sizes: string; card_max: number;
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

// ---- 2. the grid: Home's Grid placed as the panel places it (core/fx_core.cpp grid_lay, aspect half: rows half a
// square; an even-height card starts on an even row; a one-row card after a one-row card goes under it), so the
// editor knows where every card sits and which squares are empty
const COLS = 3;  // the square panels' homeCols (tokens/layout.json); both panels are square
type Spot = { x: number; y: number; w: number; h: number };
const span = (size: string): [number, number] => {
  const s = size === 's' ? '1x1' : size === 'm' ? '2x1' : size === 'l' ? '2x2' : size;
  return [s[0] === '2' ? 2 : 1, s === 'half' ? 1 : s === '1x2' || s === '2x2' ? 4 : 2];
};
const gsize = (c: Card) => (c.size === 's' ? '1x1' : c.size === 'm' ? (c.t === 'sensor' ? 'half' : '2x1') : c.size === 'l' ? '2x2' : c.size);
/** where each card goes (null = left out: a page card whose page is not in the deck), and the taken half rows */
function place(cards: Card[], pages: Page[]): { at: (Spot | null)[]; occ: boolean[][] } {
  const occ: boolean[][] = [], at: (Spot | null)[] = [];
  const grow = (n: number) => { while (occ.length < n) occ.push(Array(COLS).fill(false)); };
  let px = -1, py = -1;
  for (const c of cards) {
    const pg = c.t === 'page' || c.tap === 'page' ? c.page ?? '' : null;
    if (pg !== null && !pages.some((p) => p.id === pg)) { at.push(null); continue; }
    const [sw, sh] = span(gsize(c)), w = Math.min(sw, COLS), hh = sh;
    let done: Spot | null = null;
    for (let y = 0; !done; y++) {
      grow(y + hh);
      if (hh === 1 && px >= 0 && y === 0) {
        grow(py + 2);
        if (py % 2 === 0 && !occ[py + 1][px]) { occ[py + 1][px] = true; done = { x: px, y: py + 1, w, h: 1 }; px = -1; break; }
      }
      if (hh % 2 === 0 && y % 2) continue;
      for (let x = 0; x + w <= COLS && !done; x++) {
        let free = true;
        for (let yy = y; yy < y + hh && free; yy++) for (let xx = x; xx < x + w && free; xx++) free = !occ[yy][xx];
        if (!free) continue;
        for (let yy = y; yy < y + hh; yy++) for (let xx = x; xx < x + w; xx++) occ[yy][xx] = true;
        done = { x, y, w, h: hh };
        px = hh === 1 ? x : -1; py = y;
      }
    }
    at.push(done);
  }
  return { at, occ };
}
/** a new card at a square (or under a half): the place in the card order that puts it there and moves the fewest
 *  other cards (none, nearly always); -1 = nowhere */
function insertAt(p: Page, card: Card, x: number, y: number): number {
  const cards = p.cards ?? [], was = place(cards, draft.pages).at;
  let best = -1, moved = Infinity;
  for (let i = 0; i <= cards.length; i++) {
    const next = [...cards.slice(0, i), card, ...cards.slice(i)];
    const at = place(next, draft.pages).at;
    if (at[i]?.x !== x || at[i]?.y !== y) continue;
    const m = was.filter((s, j) => { const t = at[j < i ? j : j + 1]; return !!s !== !!t || (s && t && (s.x !== t.x || s.y !== t.y)); }).length;
    if (m < moved) { best = i; moved = m; }
    if (!m) break;
  }
  return best;
}
const TYPE_NAME: Record<string, string> = { light: 'Light', media: 'Media', sensor: 'Sensor', page: 'Screen', entity: 'Entity' };
const SIZE_NAME: Record<string, string> = { '1x1': '1 × 1', '2x1': '2 × 1', '2x2': '2 × 2', '1x2': '1 × 2', half: 'Half' };
const TAP_NAME: Record<string, string> = { toggle: 'Toggle', detail: 'Its screen', page: 'A screen', run: 'Run', none: 'Nothing' };
function entName(id: string | undefined) {
  if (!id) return '';
  for (const l of Object.values(boot.options)) { const o = l.find((x) => x.id === id); if (o) return o.name; }
  return id;
}
const cardName = (c: Card, d: Draft = draft) => c.label || (c.t === 'page' ? name(c.page ?? '', d) : entName(c.e)) || TYPE_NAME[c.t] || c.t;
/** the taps a card can have: the type's, or for an Entity card its domain's (the default first) */
function tapsOf(c: Card) {
  const def = boot.cards[c.t];
  if (!def) return ['none'];
  if (c.t === 'entity') { const dom = (c.e ?? '').split('.')[0]; return (def.by_domain?.[dom] ?? def.by_domain?.['*'] ?? { taps: 'none' }).taps.split(','); }
  return def.taps.split(',');
}

// ---- what changed, in words
type Change = { where: string; what: string; was: string; now: string };
function changes(a: Draft, b: Draft): Change[] {
  const out: Change[] = [];
  const ia = new Set(a.pages.map((p) => p.id)), ib = new Set(b.pages.map((p) => p.id));
  for (const p of b.pages) if (!ia.has(p.id)) out.push({ where: pname(p), what: own(p) ? 'Added your screen' : 'Added the built-in screen', was: '', now: '' });
  for (const p of b.pages) if (!ia.has(p.id) && own(p)) for (const c of p.cards ?? []) out.push({ where: pname(p), what: `Added a ${TYPE_NAME[c.t] ?? c.t} card`, was: '', now: `${cardName(c, b)}, ${SIZE_NAME[gsize(c)]}` });
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
      out.push(...cardChanges(pa, pb, a, b));
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
/** a Home's cards, before and after: added, removed, changed (size, label, tap), the order */
function cardChanges(pa: Page, pb: Page, a: Draft, b: Draft): Change[] {
  const out: Change[] = [], key = (c: Card) => `${c.t}|${c.e ?? ''}|${c.page ?? ''}`;
  const ca = [...(pa.cards ?? [])], cb = pb.cards ?? [], kept: [Card, Card][] = [];
  for (const c of cb) {
    const i = ca.findIndex((x) => key(x) === key(c));
    if (i < 0) { out.push({ where: pname(pb), what: `Added a ${TYPE_NAME[c.t] ?? c.t} card`, was: '', now: `${cardName(c, b)}, ${SIZE_NAME[gsize(c)]}` }); continue; }
    kept.push([ca[i], c]);
    ca.splice(i, 1);
  }
  for (const c of ca) out.push({ where: pname(pb), what: 'Removed a card', was: cardName(c, a), now: '' });
  for (const [x, y] of kept) {
    const w = `${pname(pb)} · ${cardName(y, b)}`;
    if (gsize(x) !== gsize(y)) out.push({ where: w, what: 'Size', was: SIZE_NAME[gsize(x)], now: SIZE_NAME[gsize(y)] });
    if ((x.label ?? '') !== (y.label ?? '')) out.push({ where: w, what: 'Name', was: x.label || 'its own', now: y.label || 'its own' });
    const tx = x.tap || tapsOf(x)[0], ty = y.tap || tapsOf(y)[0];
    if (tx !== ty || (ty === 'page' && x.page !== y.page)) out.push({ where: w, what: 'Tap', was: ty === 'page' && tx === 'page' ? name(x.page ?? '', a) : TAP_NAME[tx] ?? tx, now: ty === 'page' && tx === 'page' ? name(y.page ?? '', b) : TAP_NAME[ty] ?? ty });
  }
  const oa = (pa.cards ?? []).filter((c) => kept.some(([x]) => x === c)).map(key), ob = kept.map(([, y]) => key(y));
  if (oa.join() !== ob.join()) out.push({ where: pname(pb), what: 'Card order', was: '', now: '' });
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
    const what = !pg || !m ? '' : m[3] ? boot.screens[pg.kind]?.params[m[3]]?.label ?? m[3] : m[4] ? 'Header' : m[6] ? `${cardName((pg.cards ?? [])[+m[7]] ?? { t: '', size: '' })} card` : '';
    return { where, what, msg: x.msg };
  }) };
}

// ---- engine + previews
/** this page's own light / dark: the browser's by default, or the top bar's toggle (remembered on this browser) */
let pageLook: 'auto' | 'light' | 'dark' = (() => { try { const v = localStorage.getItem('facet-v1-look'); return v === 'light' || v === 'dark' ? v : 'auto'; } catch { return 'auto'; } })();
function look() {
  if (pageLook === 'auto') delete document.documentElement.dataset.theme; else document.documentElement.dataset.theme = pageLook;
  const b = $('v1-look');
  b.textContent = pageLook === 'auto' ? '◐' : pageLook === 'light' ? '☼' : '☾';
  b.setAttribute('aria-label', `Page look: ${pageLook}`);
  b.title = `This page: ${pageLook === 'auto' ? 'follows the browser' : pageLook} (the panel's own Mode is in Settings)`;
}
let device: DeviceId = new URLSearchParams(location.search).get('device') === 'guition' ? 'guition' : 'waveshare';
let engine: Engine | null = null;
let sel = draft.start;                                     // the screen on the live panel
let side: 'screen' | 'add' | 'settings' | 'review' | 'saved' = 'screen';  // what the sidebar holds (a bottom sheet on a phone)
/** on your own screen: the card picked on the grid (its index), or an empty place picked to add a card to */
let cardSel = -1;
let target: { x: number; y: number; size: string } | null = null;
/** the Grid on the live panel, in device px (measured once per panel from the renderer: engine.comps) */
let geo = { x: 0, y: 0, cw: 1, gap: 0, inset: 0, top: 0 };
const rowY = (y: number) => geo.y + Math.floor(y / 2) * (geo.cw + geo.gap) + (y % 2) * (((geo.cw - geo.gap) / 2) + geo.gap);
const spotBox = (s: Spot) => ({ left: geo.x + s.x * (geo.cw + geo.gap), top: rowY(s.y), width: s.w * geo.cw + (s.w - 1) * geo.gap, height: s.h === 1 ? (geo.cw - geo.gap) / 2 : (s.h / 2) * geo.cw + (s.h / 2 - 1) * geo.gap });
let savedAt = '';
const stages: Stage[] = [];
const S = () => DEVICES[device].size;
/** the panel's Mode; Auto follows this computer's light / dark (the mock has no sun) */
const light = () => draft.settings.theme === 'light' || (draft.settings.theme === 'auto' && matchMedia('(prefers-color-scheme: light)').matches);
/** the live panel's light / dark: the page's toggle when it is set, else the panel's Mode (a Mode change in
 *  Settings puts the toggle back to Auto, so you see the Mode you are about to save) */
const plight = () => (pageLook === 'auto' ? light() : pageLook === 'light');
const accent = () => boot.accents.sets.findIndex((a) => a.id === draft.settings.accent);
function applyDeck() {
  const e = engine;
  if (!e) return;
  applySettings();
  e.withStr('deck', (pc) => e.withStr(JSON.stringify(toDeck(draft, panel.rev)), (pj) => e.str(e.m._fw_spec(pc, pj))));
}
function preview(p: Page, cssW: number, label: string) {
  const s = engine!.stage(S(), S());
  s.label = label;
  stages.push(s);
  s.canvas.style.width = `${cssW}px`;
  s.canvas.style.height = `${cssW}px`;
  s.canvas.className = 'v1-canvas';
  if (own(p)) s.render('home', { page: p.id }, plight(), accent());
  else s.render('spec', { screen: p.kind, ...Object.fromEntries(Object.keys(boot.screens[p.kind].params).map((k) => [k, paramVal(p, k)])) }, plight(), accent());
  return s;
}
function freeStages() { for (const s of stages.splice(0)) s.free(); }

// ---- the page
function edit(f: () => void) { f(); if (!pageOf(draft, sel)) sel = draft.start; const p = pageOf(draft, sel)!; if (cardSel >= (p.cards?.length ?? 0) || !own(p)) cardSel = -1; if (!own(p)) target = null; applyDeck(); top(); render(); }
function go(id: string) { sel = id; side = 'screen'; cardSel = -1; target = null; top(); render(); }
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
  look();
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

/** the live panel's size on this page: at most 720 CSS px, in whole screen pixels per panel pixel when that fits (a
 *  480 panel on a 2x screen: 3 screen pixels each, 720 px wide, drawn nearest-neighbour: sharp), else shrunk smooth */
let crisp = false, zoom = 1;
function panelSize() {
  const main = $('v1-main'), phone = matchMedia('(max-width: 760px)').matches;
  const sideW = phone ? 0 : (main.querySelector('.v1-side') as HTMLElement | null)?.offsetWidth ?? 420;
  const pad = phone ? 32 : 64;
  const w = Math.min(720, main.clientWidth - sideW - pad), hh = phone ? w : Math.min(720, main.clientHeight - pad - 96);
  const dpr = window.devicePixelRatio || 1, room = Math.max(120, Math.min(w, hh));
  const best = (room * dpr) / S(), whole = Math.floor(best);
  // whole screen pixels when that keeps at least 85% of the room; else fill the room, smooth
  crisp = whole >= 1 && whole / best >= 0.85;
  zoom = crisp ? whole : best;
  return Math.floor((S() * zoom) / dpr);
}
let resizeT = 0;
addEventListener('resize', () => { clearTimeout(resizeT); resizeT = window.setTimeout(() => render(), 150); });
function render() {
  if (!engine) return;
  freeStages();
  const main = $('v1-main');
  const p = pageOf(draft, sel)!;
  const stage = h('div', 'v1-stage');
  const big = panelSize();
  const frame = h('div', 'v1-frame');
  if (own(p)) frame.append(gridPanel(p, big));
  else frame.append(preview(p, big, 'Live panel').canvas);
  if (own(p)) {
    const end = btn('+ Add a card at the end', 'Add a 1 × 1 card after the last one', (p.cards?.length ?? 0) >= budgetCards, () => {
      const sp = place([...(p.cards ?? []), { t: 'page', page: p.id, size: '1x1' }], draft.pages).at.at(-1)!;
      cardSel = -1; target = { x: sp.x, y: sp.y, size: '1x1' }; side = 'screen'; top(); render();
    }, 'ghost');
    stage.append(end);
  }
  frame.classList.toggle('crisp', crisp);
  stage.prepend(frame, h('p', 'v1-meta', `Live panel · ${pname(p)} · ${S()} × ${S()}${!crisp ? ' · scaled to fit' : zoom !== 1 ? ` · ${zoom}× sharp` : ''}${own(p) ? ' · tap a card to change it, an empty square to add one' : ''}`));
  const sb = h('aside', 'v1-side');
  sb.append(h('i', 'v1-grab'));
  const body = side === 'screen' && own(p) && target ? addCardSide(p) : side === 'screen' && own(p) && cardSel >= 0 ? cardSide(p, cardSel) : side === 'add' ? addSide() : side === 'settings' ? settingsSide() : side === 'review' ? reviewSide() : side === 'saved' ? savedSide() : screenSide(p);
  sb.setAttribute('aria-label', body[0].querySelector('b')?.textContent ?? '');
  sb.append(...body);
  main.replaceChildren(stage, sb);
}

/** your own screen on the live panel: the real screen (it scrolls as the panel does: drag or the wheel), with the
 *  editing layer over its grid, kept in step with the scroll: every card a box to pick, every empty square a + (a
 *  half with room under it: a half +). The layer stops at the header, as the cards do. */
function gridPanel(p: Page, cssW: number) {
  const cards = p.cards ?? [], { at, occ } = place(cards, draft.pages);
  const fit = Math.floor((S() - geo.inset - geo.y + geo.gap) / (geo.cw + geo.gap)) * 2;  // the squares one screen shows
  const rows = Math.max(occ.length + (occ.length % 2) + 2, fit);
  const s = preview(p, cssW, 'Live panel');
  const k = cssW / S();
  const wrap = h('div', 'v1-grid');
  const clip = h('div', 'v1-clip');  // over the whole panel: a tap on the header doesn't leave the screen being edited
  const win = h('div', 'v1-win');    // the cards' window: under the header
  win.style.top = `${geo.top * k}px`;
  const layer = h('div', 'v1-layer');
  layer.style.top = `${-geo.top * k}px`;
  const put = (e: HTMLElement, sp: Spot) => { const b = spotBox(sp); e.style.left = `${b.left * k}px`; e.style.top = `${b.top * k}px`; e.style.width = `${b.width * k}px`; e.style.height = `${b.height * k}px`; return e; };
  at.forEach((sp, i) => {
    if (!sp) return;
    const b = put(h('button', 'v1-cell card'), sp);
    b.type = 'button';
    b.title = `${cardName(cards[i])} · ${SIZE_NAME[gsize(cards[i])]}`;
    b.setAttribute('aria-label', `Card: ${b.title}`);
    b.setAttribute('aria-pressed', String(i === cardSel && !target));
    b.onclick = () => { if (dragged) return; target = null; cardSel = cardSel === i ? -1 : i; side = 'screen'; top(); render(); };
    layer.append(b);
  });
  const free = (y: number, x: number) => !(occ[y]?.[x]);
  for (let y = 0; y < rows; y += 2)
    for (let x = 0; x < COLS; x++) {
      const up = free(y, x), down = free(y + 1, x);
      if (up && down) layer.append(addCell(put, x, y, '1x1'));
      else if (!up && down && at.some((sp) => sp && sp.x === x && sp.y === y && sp.h === 1)) layer.append(addCell(put, x, y + 1, 'half'));
    }
  win.append(layer);
  clip.append(win);
  wrap.append(s.canvas, clip);
  // the scroll: the layer follows the grid's place on the panel (engine comps); a drag or the wheel over the layer
  // goes to the panel as a touch drag, so it scrolls with the panel's own feel
  let gy = geo.y;
  const follow = () => {
    if (!stages.includes(s)) return;
    const g = s.comps().find((c) => c.ids.includes('grid'));
    if (g && g.y !== gy) { gy = g.y; layer.style.transform = `translateY(${(gy - geo.y) * k}px)`; }
    requestAnimationFrame(follow);
  };
  requestAnimationFrame(follow);
  const xy = (ev: PointerEvent | WheelEvent) => { const r = s.canvas.getBoundingClientRect(); return [Math.round((ev.clientX - r.left) / k), Math.round((ev.clientY - r.top) / k)]; };
  const touch = (x: number, y: number, d: number) => engine!.m._fw_touch(s.id, x, y, d);
  let down: number[] | null = null;
  clip.onpointerdown = (ev) => { down = xy(ev); dragged = false; };
  clip.onpointermove = (ev) => {
    if (!down || !ev.buttons) return;
    const [x, y] = xy(ev);
    if (!dragged && Math.abs(y - down[1]) > 8) { dragged = true; clip.setPointerCapture(ev.pointerId); touch(down[0], down[1], 1); }
    if (dragged) touch(x, y, 1);
  };
  clip.onpointerup = clip.onpointercancel = (ev) => { if (dragged) { const [x, y] = xy(ev); setTimeout(() => touch(x, y, 0), 40); setTimeout(() => { dragged = false; }, 0); } down = null; };
  let wheel = 0, wy = 0;
  clip.onwheel = (ev) => {
    ev.preventDefault();
    const [x] = xy(ev);
    if (!wheel) { wy = S() / 2; touch(x, wy, 1); }
    wy = Math.max(geo.top, Math.min(S() - 1, wy - ev.deltaY / k / 2));
    touch(x, wy, 1);
    clearTimeout(wheel);
    wheel = window.setTimeout(() => { touch(x, wy, 0); wheel = 0; }, 90);
  };
  return wrap;
}
let dragged = false;
function addCell(put: (e: HTMLElement, sp: Spot) => HTMLElement, x: number, y: number, size: string) {
  const b = put(h('button', `v1-cell add${size === 'half' ? ' half' : ''}`, '+'), { x, y, w: 1, h: size === 'half' ? 1 : 2 });
  b.type = 'button';
  b.title = size === 'half' ? 'Add another half underneath' : 'Add a card here';
  b.setAttribute('aria-label', b.title);
  b.setAttribute('aria-pressed', String(!!target && target.x === x && target.y === y));
  b.onclick = () => { cardSel = -1; target = target && target.x === x && target.y === y ? null : { x, y, size }; side = 'screen'; top(); render(); };
  return b;
}

// 2. a card: its size, what it shows, what a tap does, its place, remove
function cardSide(p: Page, i: number): HTMLElement[] {
  const cards = p.cards!, c = cards[i], out: HTMLElement[] = [];
  const close = () => { cardSel = -1; top(); render(); };
  const hd = sideHead(cardName(c), `${TYPE_NAME[c.t] ?? c.t} card · ${i + 1} of ${cards.length} on ${pname(p)}`, false);
  hd.append(btn('✕', 'Close', false, close, 'icon'));
  out.push(hd);
  const set = (f: (c: Card) => void) => edit(() => f(c));
  out.push(field('Size', 'The card reflows the grid: the cards after it move up or down to fit.', seg(boot.grid_sizes.split(',').map((z) => [z, SIZE_NAME[z] ?? z]), gsize(c), (z) => set((c) => { c.size = z; }))));
  const { at, occ } = place(cards, draft.pages), sp = at[i];
  if (sp && sp.h === 1 && sp.y % 2 === 0 && !occ[sp.y + 1]?.[sp.x]) {
    out.push(field('Under it', 'Two halves share one square. Optional: the space can stay empty.', btn('+ Add another half', 'Add another half underneath', false, () => { cardSel = -1; target = { x: sp.x, y: sp.y + 1, size: 'half' }; top(); render(); })));
  }
  out.push(h('h2', 'cap', 'What it shows'));
  if (c.t === 'page') out.push(field('Screen', 'The screen it opens.', list(draft.pages.filter((x) => x.id !== p.id).map((x) => [x.id, pname(x)] as [string, string]), c.page ?? '', (v) => set((c) => { c.page = v; }))));
  else {
    const doms = c.t === 'entity' ? Object.keys(boot.options) : boot.cards[c.t].domains.split(',');
    const items: [string, string, string?][] = doms.flatMap((d) => (boot.options[d] ?? []).map((o) => [o.id, o.name, o.id] as [string, string, string]));
    if (c.e && !items.some(([v]) => v === c.e)) items.unshift([c.e, `${c.e} (not found)`]);
    out.push(field('Entity', undefined, list(items, c.e ?? '', (v) => set((c) => {
      c.e = v;
      if (c.tap && !tapsOf(c).includes(c.tap)) delete c.tap;
    }), 'Pick one')));
  }
  const nm = h('input', 'v1-input');
  nm.value = c.label ?? '';
  nm.placeholder = c.t === 'page' ? name(c.page ?? '') : entName(c.e) || 'Its own name';
  nm.maxLength = 32;
  nm.onchange = () => set((c) => { const v = nm.value.trim(); if (v) c.label = v; else delete c.label; });
  out.push(field('Name', 'Leave it empty to use its own name.', nm));
  const taps = tapsOf(c);
  if (taps.length > 1) {
    const cur = c.tap || taps[0];
    const pick = (v: string) => set((c) => {
      if (v === taps[0]) delete c.tap; else c.tap = v;
      if (v === 'page' && c.t !== 'page' && !c.page) c.page = draft.pages.find((x) => x.id !== p.id)?.id;
      if (v !== 'page' && c.t !== 'page') delete c.page;
    });
    const items = taps.map((t) => [t, TAP_NAME[t] ?? t] as [string, string]);
    out.push(field('Tap', 'What a tap on the card does.', items.length <= 3 ? seg(items, cur, pick) : list(items, cur, pick)));
    if (cur === 'page' && c.t !== 'page') out.push(field('Opens', undefined, list(draft.pages.filter((x) => x.id !== p.id).map((x) => [x.id, pname(x)] as [string, string]), c.page ?? '', (v) => set((c) => { c.page = v; }))));
  }
  out.push(h('h2', 'cap', 'Place'));
  const order = h('div', 'v1-row');
  order.append(
    btn('← Earlier', 'One place earlier in the grid', i === 0, () => { cards.splice(i, 1); cards.splice(i - 1, 0, c); cardSel = i - 1; edit(() => {}); }),
    btn('Later →', 'One place later in the grid', i === cards.length - 1, () => { cards.splice(i, 1); cards.splice(i + 1, 0, c); cardSel = i + 1; edit(() => {}); }),
  );
  out.push(field('Order', 'Cards fill the grid in reading order.', order));
  out.push(h('div', 'v1-gap'), btn('Remove this card', `Remove ${cardName(c)}`, false, () => { cards.splice(i, 1); cardSel = -1; edit(() => {}); }, 'ghost danger'));
  return out;
}

// 2. an empty place picked: everything that fits there (every card type takes every size), as one list to search
function addCardSide(p: Page): HTMLElement[] {
  const t = target!, out: HTMLElement[] = [];
  const hd = sideHead('Add a card', `${SIZE_NAME[t.size]} · row ${Math.floor(t.y / 2) + 1}, column ${t.x + 1}${t.size === 'half' ? ', under the half' : ''}`, false);
  hd.append(btn('✕', 'Close', false, () => { target = null; top(); render(); }, 'icon'));
  out.push(hd);
  const full = (p.cards?.length ?? 0) >= budgetCards;
  if (full) { out.push(h('p', 'v1-note', `A screen holds ${budgetCards} cards. Remove one to add another.`)); return out; }
  const q = h('input', 'v1-input');
  q.placeholder = 'Search';
  q.setAttribute('aria-label', 'Search the cards');
  const box = h('div', 'v1-cardlist');
  const groups: { label: string; items: Card[] }[] = [
    { label: 'Screens', items: draft.pages.filter((x) => x.id !== p.id).map((x) => ({ t: 'page', page: x.id, size: t.size })) },
  ];
  const typed = new Set<string>();
  for (const [ty, def] of Object.entries(boot.cards)) {
    if (ty === 'page' || ty === 'entity') continue;
    const doms = def.domains.split(',');
    doms.forEach((d) => typed.add(d));
    groups.push({ label: `${TYPE_NAME[ty] ?? ty}`, items: doms.flatMap((d) => (boot.options[d] ?? []).map((o) => ({ t: ty, e: o.id, size: t.size }))) });
  }
  groups.push({ label: 'Other entities', items: Object.keys(boot.options).filter((d) => !typed.has(d)).flatMap((d) => boot.options[d].map((o) => ({ t: 'entity', e: o.id, size: t.size }))) });
  const fill = () => {
    const f = q.value.trim().toLowerCase();
    box.replaceChildren();
    for (const g of groups) {
      const its = g.items.filter((c) => !f || `${cardName(c)} ${c.e ?? ''} ${g.label}`.toLowerCase().includes(f));
      if (!its.length) continue;
      box.append(h('h2', 'cap', g.label));
      for (const c of its) {
        const b = h('button', 'v1-opt');
        b.type = 'button';
        const tx = h('span', 'v1-ftxt');
        tx.append(h('span', '', cardName(c)));
        if (c.e) tx.append(h('p', 'mono', c.e));
        b.append(tx);
        b.onclick = () => {
          const i = insertAt(p, c, t.x, t.y);
          const cards = (p.cards ??= []);
          const at = i < 0 ? cards.length : i;
          cards.splice(at, 0, c);
          target = null;
          cardSel = at;
          edit(() => {});
        };
        box.append(b);
      }
    }
    if (!box.children.length) box.append(h('p', 'dim', 'Nothing matches.'));
  };
  q.oninput = fill;
  fill();
  out.push(q, box);
  requestAnimationFrame(() => q.focus());
  return out;
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
    out.push(h('p', 'v1-note', `${cards ? `${cards} card${cards === 1 ? '' : 's'} on this screen. ` : 'An empty grid. '}Tap an empty square on the panel to add a card, a card to change it.`));
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

// 3. Settings: one grouped list, as the panel's own Settings screen: Panel (Screens, Home Assistant), then the
// schema's groups, then Start again. Screens and Home Assistant open their own page in the sidebar (‹ back), as on the
// panel; every change shows on the live panel as it is made
let setView: 'main' | 'screens' | 'ha' = 'main';
const openScreens = new Set<string>();
function settingsSide(): HTMLElement[] {
  if (setView === 'screens') return screensView();
  if (setView === 'ha') return haView();
  const out: HTMLElement[] = [];
  out.push(sideHead('Settings', `${DEVICES[device].name} · the whole panel`));
  out.push(h('h2', 'cap', 'Panel'));
  const kinds = Object.keys(boot.screens), on = kinds.filter((k) => draft.pages.some((p) => p.kind === k)).length;
  out.push(navRow('Screens', `${on} of ${kinds.length} on`, () => { setView = 'screens'; render(); }));
  out.push(navRow('Home Assistant', haStatus(), () => { setView = 'ha'; render(); }));
  for (const g of boot.settings.groups) {
    out.push(h('h2', 'cap', g.label));
    for (const s of boot.settings.settings.filter((x) => x.group === g.id)) out.push(field(s.label, s.help, settingControl(s)));
  }
  out.push(h('h2', 'cap', 'Start again'), factoryBtn());
  return out;
}
const haStatus = () => (!draft.ha.url || !draft.ha.token ? 'Not set up' : `Set, ${draft.ha.url.replace(/^https?:\/\//, '').replace(/[:/].*$/, '')}`);
/** a row that opens a page: the label left, its value + › right (the panel's List row, value-chevron) */
function navRow(label: string, value: string, f: () => void) {
  const b = h('button', 'v1-field inline v1-nav');
  b.type = 'button';
  const t = h('div', 'v1-ftxt');
  t.append(h('span', '', label));
  const v = h('span', 'v1-pick');
  v.append(h('span', '', value), h('i', 'chev', '›'));
  b.append(t, v);
  b.onclick = f;
  return b;
}
/** a page inside Settings: ‹ back to the list, its title */
function subHead(t: string, sub: string) {
  const hd = sideHead(t, sub, false);
  const back = btn('‹', 'Back to Settings', false, () => { setView = 'main'; render(); }, 'icon');
  hd.prepend(back);
  return hd;
}
// Settings > Screens: every built-in screen in this firmware, on (in the swipe) or off, its options under it
function screensView(): HTMLElement[] {
  const out: HTMLElement[] = [subHead('Screens', 'Built-in screens: on adds one to the end of the swipe')];
  for (const [k, sc] of Object.entries(boot.screens)) {
    const pg = draft.pages.find((p) => p.kind === k);
    const ps = Object.entries(sc.params).filter(([, x]) => x.type !== 'hidden');
    const row = h('div', 'v1-field inline');
    const t = h('div', 'v1-ftxt');
    const nm = h('span', '', pg ? pname(pg) : sc.name);
    t.append(nm);
    if (pg && ps.length) {
      const more = h('button', 'link', openScreens.has(k) ? 'Hide its settings' : `Its settings · ${ps.length}`);
      more.type = 'button';
      more.onclick = () => { openScreens.has(k) ? openScreens.delete(k) : openScreens.add(k); render(); };
      t.append(more);
    } else if (!pg) t.append(h('p', '', 'Off'));
    const last = !!pg && draft.pages.length === 1;
    row.append(t, toggle(!!pg, `${sc.name} on the panel`, last, () => edit(() => {
      if (pg) { draft.pages = draft.pages.filter((x) => x !== pg); if (draft.start === pg.id) draft.start = draft.pages[0].id; openScreens.delete(k); }
      else draft.pages.push({ id: k, kind: k });
    })));
    out.push(row);
    if (pg && openScreens.has(k)) {
      const box = h('div', 'v1-sub');
      for (const [pk, x] of ps) box.append(field(x.label ?? pk, x.about, paramControl(pg, pk, x)));
      box.append(btn('Back to defaults', `${sc.name} back to defaults`, !pg.params || !Object.keys(pg.params).length, () => edit(() => { delete pg.params; }), 'ghost'));
      out.push(box);
    }
  }
  return out;
}
// Settings > Home Assistant: the panel's own link (ha.connect): the address, a long-lived token sent once, its last
// four characters shown back, never the token
function haView(): HTMLElement[] {
  const out: HTMLElement[] = [subHead('Home Assistant', haStatus())];
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
  out.push(h('p', 'v1-note', 'The editor reads your entities through this link: the pickers list what this Home Assistant has.'));
  return out;
}
function setSetting(id: string, v: unknown) {
  if (id === 'theme' && pageLook !== 'auto') { pageLook = 'auto'; try { localStorage.removeItem('facet-v1-look'); } catch { /* private window */ } }
  edit(() => { draft.settings[id] = v; });
}
/** the settings the live panel shows as you change them (Mode and Accent go with each render; sound, idle, updates
 *  and API keys have nothing to see; Rotate screen is left upright here, so the grid's editing layer stays on it) */
const LIVE = ['look', 'textSize', 'distanceUnit', 'speedUnit', 'transition', 'toastPosition'];
function applySettings() {
  const e = engine;
  if (!e) return;
  for (const id of LIVE) if (id in draft.settings) e.withStr(id, (pi) => e.withStr(String(draft.settings[id]), (pv) => e.m._fw_setting(pi, pv)));
}
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

/** the Grid's place and cell size on this panel: a Home with two 1 x 1 cards, drawn once, its boxes read back */
const budgetCards = boot.card_max;
function measure() {
  const e = engine!;
  const d = { v: 2, rev: 0, start: 'm', pages: [{ id: 'm', kind: 'home', name: 'M', cards: [{ t: 'page', page: 'm', size: '1x1' }, { t: 'page', page: 'm', size: '1x1' }] }] };
  e.withStr('deck', (pc) => e.withStr(JSON.stringify(d), (pj) => e.str(e.m._fw_spec(pc, pj))));
  const s = e.stage(S(), S());
  s.render('home', { page: 'm' }, false);
  const bx = s.comps(), g = bx.find((b) => b.ids.includes('grid'));
  const cs = bx.filter((b) => g && b.d === g.d + 1 && b.y >= g.y).sort((a, b) => a.x - b.x);
  const hd = bx.find((b) => b.ids.includes('header'));
  if (g && cs.length >= 2) geo = { x: cs[0].x, y: g.y, cw: cs[0].w, gap: cs[1].x - cs[0].x - cs[0].w, inset: S() - g.x - g.w, top: hd ? hd.y + hd.h : g.y };
  s.free();
}
async function load() {
  freeStages();
  engine?.dispose();
  engine = null;
  top();
  $('v1-main').replaceChildren(h('p', 'dim', 'Loading the renderer'));
  engine = await Engine.load(DEVICES[device].den, `/facet-wasm/${DEVICES[device].den}/`, noHa);
  measure();
  applyDeck();
  top();
  render();
}

export function start() {
  $('v1-save').onclick = () => { side = 'review'; top(); render(); };
  $('v1-undo').onclick = undo;
  $('v1-look').onclick = () => {
    // two states: the browser's look, or the other one
    pageLook = pageLook !== 'auto' ? 'auto' : matchMedia('(prefers-color-scheme: light)').matches ? 'dark' : 'light';
    try { if (pageLook === 'auto') localStorage.removeItem('facet-v1-look'); else localStorage.setItem('facet-v1-look', pageLook); } catch { /* private window */ }
    look();
    render();
  };
  look();
  $('v1-settings').onclick = () => { side = side === 'settings' ? 'screen' : 'settings'; setView = 'main'; top(); render(); };
  $('v1-scope').onclick = scope;
  (window as any).__v1 = { draft: () => draft, load: (d: Draft) => edit(() => { draft = d; sel = d.start; }), put: (e: string, a: string, v: string) => engine?.haPut(e, a, v), engine: () => engine, panel: () => panel, deck: () => toDeck(draft, panel.rev), check, changes: () => changes(panel.runs, draft) };  // tests
  void load();
}
