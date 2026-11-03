// Facet editor concept (/wasm/editor, src/pages/wasm/editor.astro): edit the panel's deck (its screens: on / order /
// source / typed params) with the real WASM renderer as the preview. Every edit = a new deck applied in the engine
// (fw_spec deck, the same path a panel takes) and the preview built again; Save only shows what would be sent.
// Two layouts on one deck model: the workbench page (/wasm/editor, has #ed-flow) = the click-to-edit flow (
// Panel overview of live thumbnails -> a Screen large, hover outlines the regions the renderer anchors (core/spec.h
// anchors, WASM fw_hits / fw_hit) -> click = that region's params in a drawer; Screen settings = everything); the page
// a panel serves (scripts/editor-device.mjs, no #ed-flow) keeps the three columns for now.
// Three columns (screens | preview | settings). ?device=guition|waveshare,
// ?theme=dark|light. Picking a source refills the params that source fills (one Undo step). Everything host-specific
// (sources, panel status, the WASM files, Save) goes through ./editor-host, so the panel can serve this editor later.
import { Engine, Stage, DEVICES, type DeviceId, type Comp } from './engine';
import { host, type Deck, type DeckPage, type DeckEntry as Entry, type Discovery as Sources } from './editor-host';
/** the editor's view of a deck v2: the Homes kept as they are (their cards: a later round), the other pages as rows
 *  with on / off (off = left out of the deck sent) */
type View = { rev: number; start: string; homes: DeckPage[]; screens: Entry[]; specs?: Record<string, string> };  // specs: screens edited here (kind -> spec JSON), installed as sent screens
function fromDeck(d: any): View {
  if (d?.v === 2 && Array.isArray(d.pages)) {
    const pages = d.pages as DeckPage[];
    return { rev: d.rev ?? 1, start: d.start ?? '', homes: pages.filter((p) => p.kind === 'home'), screens: pages.filter((p) => p.kind !== 'home').map((p) => ({ ...p, on: true })) };
  }
  // a v1 deck (an old panel): its screens as pages, a factory Home as the start
  const sc = (Array.isArray(d?.screens) ? d.screens : []).filter((x: any) => x.id !== 'settings' && x.id !== 'home' && boot.specs[x.id] && boot.specs[x.id].role !== 'detail');
  return { rev: d?.rev ?? 1, start: 'home', homes: [{ id: 'home', kind: 'home', name: 'Home', cards: [] }], screens: sc.map((x: any) => ({ id: x.id, kind: x.id, source: x.source, params: x.params, on: x.on !== false })) };
}
function toDeck(v: View): Deck {
  const pages: DeckPage[] = [...v.homes, ...v.screens.filter((e) => e.on !== false).map(({ on: _on, ...p }) => ({ ...p, kind: p.kind ?? p.id }))];
  return { v: 2, rev: v.rev, start: v.start, pages };
}

type Param = string | { type?: string; label?: string; default?: string; options?: string[]; domain?: string };
type Src = { id: string; kind: string; name: string; setup?: string[]; fills?: Record<string, unknown> };
type AccentSet = { id: string; label: string; dark: Record<string, string>; light: Record<string, string> };
type Boot = { accents: { default: string; sets: AccentSet[] }; catalogue: { screens: Record<string, { needs: string | null; sources: Src[] }> }; specs: Record<string, { name: string; role?: string; params: Record<string, Param> }> };

const boot: Boot = JSON.parse(document.getElementById('ed-boot')!.textContent!);
const q = new URLSearchParams(location.search);
const $ = (id: string) => document.getElementById(id)!;
function h<K extends keyof HTMLElementTagNameMap>(tag: K, cls = '', text = ''): HTMLElementTagNameMap[K] {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text) e.textContent = text;
  return e;
}
const ls = { get: (k: string) => { try { return localStorage.getItem(k); } catch { return null; } }, set: (k: string, v: string) => { try { localStorage.setItem(k, v); } catch { /* private */ } } };

const st = {
  device: (host.devices.includes(q.get('device') as DeviceId) ? q.get('device') : host.devices[0]) as DeviceId,
  theme: (['dark', 'light'].includes(q.get('theme') ?? ls.get('fe.theme') ?? '') ? (q.get('theme') ?? ls.get('fe.theme')) : 'auto') as 'auto' | 'dark' | 'light',
  accent: ls.get('fe.accent') ?? boot.accents.default,
  section: 'screens' as 'screens' | 'home' | 'panel',
  view: 'screen' as 'screen' | 'home',
  sel: '',
};
let src: Sources | null = null;
let deck: View = { rev: 1, start: '', homes: [], screens: [] };
let saved = '';
const undo: string[] = [];
const redo: string[] = [];
const $o = (id: string) => document.getElementById(id);
const FLOW = !!$o('ed-flow');
/** an anchored region of the screen (core/spec.h hits): node index, component, the part's prop, its params, device px */
type Hit = { n: number; c: string; prop?: string; params: string[]; x: number; y: number; w: number; h: number };
const flow = { step: 'panel' as 'panel' | 'screen' | 'home', drawer: null as null | 'screen' | 'item' | 'home' | 'slot', item: null as Hit | null, card: -1 as number, slot: null as SlotPick | null };
/** a slot on the screen (WASM fw_slots, CONTRACT C8a): its name, whose it is (spec node index or the header), device px */
type SlotHit = { n: string; in?: number | 'header'; x: number; y: number; w: number; h: number; empty?: boolean };
type SlotPick = { n: string; in: number | 'header' };
const thumbs = new Map<string, Stage>();
let engine: Engine | null = null;
let main: Stage | null = null;
let deckErr = '';

const light = () => (st.theme === 'auto' ? matchMedia('(prefers-color-scheme: light)').matches : st.theme === 'light');
const accentIdx = () => boot.accents.sets.findIndex((a) => a.id === st.accent);
const size = () => DEVICES[st.device].size;
const entry = (id: string) => deck.screens.find((e) => e.id === id);
const savedDeck = (): View => JSON.parse(saved);
const isOn = (e?: Entry) => !!e && e.on !== false;
const specOf = (id: string) => boot.specs[id];
const nameOf = (id: string) => specOf(id)?.name ?? src?.sent?.find((x) => x.id === id)?.name ?? id.replace(/_/g, ' ').replace(/^./, (s) => s.toUpperCase());
const typed = (p: Param) => (typeof p === 'string' ? { type: 'text', default: p } : p);
const optName = (domain: string, id: string) => src?.options[domain]?.find((o) => o.id === id)?.name ?? id;
function srcName(id: string, s?: string) {
  if (!s) return '';
  if (s === 'panel') return 'On the panel';
  return boot.catalogue.screens[id]?.sources.find((x) => x.id === s)?.name ?? s;
}

// ---- deck edits, undo, the unsaved state ----
/** change the deck: snapshot for Undo (hist), apply it in the engine, redraw what shows it */
function edit(f: () => void, hist = true) {
  const before = JSON.stringify(deck);
  f();
  if (JSON.stringify(deck) === before) return;
  if (hist) { undo.push(before); if (undo.length > 100) undo.shift(); redo.length = 0; }
  changed();
}
function changed() {
  if (FLOW) { applyDeck(); flowRender(); if (cm) cardModalRender(); top(); return; }
  applyDeck();
  renderMain();
  lists();
  if (st.sel) settings();
  top();
}
function doUndo() {
  const s = undo.pop();
  if (!s) return;
  redo.push(JSON.stringify(deck));
  deck = JSON.parse(s);
  changed();
}
function doRedo() {
  const s = redo.pop();
  if (!s) return;
  undo.push(JSON.stringify(deck));
  deck = JSON.parse(s);
  changed();
}
const houseSpecs = new WeakSet<object>();
const installedSpecs = new Set<string>();  // screen kinds this editor installed edited specs for (revert when dropped)
/** push the deck into the engine; tell = also hand it to the host (the panel). A drag's would-be orders don't. */
function applyDeck(tell = true) {
  if (!engine?.m._fw_spec) return;
  const e = engine;
  if (src && !houseSpecs.has(e)) {  // the discovered screen specs (lib/facet-sources.ts), once per engine
    houseSpecs.add(e);
    for (const s of Object.values((src as any)?.specs ?? {})) e.withStr('screen_dev', (pc) => e.withStr(JSON.stringify(s), (pj) => e.m._fw_spec(pc, pj)));
  }
  const want = deck.specs ?? {};
  for (const k of [...installedSpecs]) if (!(k in want)) { e.withStr('revert', (pc) => e.withStr(k, (pj) => e.m._fw_spec(pc, pj))); installedSpecs.delete(k); }
  for (const [k, js] of Object.entries(want)) { e.withStr('screen_dev', (pc) => e.withStr(js, (pj) => e.m._fw_spec(pc, pj))); installedSpecs.add(k); }
  const d = toDeck(deck);
  if (tell) host.deckChanged?.(d);
  if (cm) d.pages.push({ id: CM_PAGE, kind: 'home', name: 'Card', cards: [cards()[cm.i]].filter(Boolean) as DeckPage['cards'] });  // the card modal's scratch Home (never sent)
  const r = e.withStr('deck', (pc) => e.withStr(JSON.stringify(d), (pj) => e.str(e.m._fw_spec(pc, pj))));
  try { const o = JSON.parse(r); deckErr = o.ok === false ? (o.errors?.[0]?.msg ?? o.errors?.[0]?.message ?? o.status ?? 'refused') : ''; } catch { deckErr = ''; }
}
type Change = { what: string; was?: string; now: string };
function diff(a: View, b: View): Change[] {
  const out: Change[] = [];
  const ao = a.screens.map((e) => e.id), bo = b.screens.map((e) => e.id);
  if (ao.join() !== bo.join()) out.push({ what: 'Order', was: ao.map(nameOf).join(', '), now: bo.map(nameOf).join(', ') });
  for (const e of b.screens) {
    const o = a.screens.find((x) => x.id === e.id);
    if (!o) { out.push({ what: nameOf(e.id), now: 'added' }); continue; }
    if (isOn(o) !== isOn(e)) out.push({ what: nameOf(e.id), was: isOn(o) ? 'on' : 'off', now: isOn(e) ? 'on' : 'off' });
    if ((o.source ?? '') !== (e.source ?? '')) out.push({ what: `${nameOf(e.id)} source`, was: srcName(e.id, o.source), now: srcName(e.id, e.source) });
    const keys = new Set([...Object.keys(o.params ?? {}), ...Object.keys(e.params ?? {})]);
    for (const k of keys) {
      const was = o.params?.[k] ?? '', now = e.params?.[k] ?? '';
      if (was === now) continue;
      const p = typed(specOf(e.id)?.params[k] ?? '');
      const label = `${nameOf(e.id)} · ${p.label ?? k}`;
      if (p.type === 'entities') {
        const A = new Set(was.split(',').filter(Boolean)), B = new Set(now.split(',').filter(Boolean));
        const add = [...B].filter((x) => !A.has(x)), rem = [...A].filter((x) => !B.has(x));
        out.push({ what: label, now: [add.length ? `+ ${add.map((x) => optName(p.domain ?? '', x)).join(', ')}` : '', rem.length ? `− ${rem.map((x) => optName(p.domain ?? '', x)).join(', ')}` : ''].filter(Boolean).join('  ') });
      } else out.push({ what: label, was: p.type === 'entity' ? optName(p.domain ?? '', was) : was, now: p.type === 'entity' ? optName(p.domain ?? '', now) : now });
    }
  }
  // Homes: their cards and the rest of the page (header actions etc.) count too, or Save stays off after a card edit
  for (const hb of b.homes) {
    const ha = a.homes.find((x) => x.id === hb.id), name = hb.name ?? 'Home';
    if (!ha) { out.push({ what: name, now: 'added' }); continue; }
    const ca = ha.cards ?? [], cb = hb.cards ?? [];
    if (JSON.stringify(ca) !== JSON.stringify(cb)) {
      // only the run that differs (same start, same end trimmed): removing card 3 of 10 = one change, not eight
      const eq = (x?: HCard, y?: HCard) => JSON.stringify(x) === JSON.stringify(y);
      let p0 = 0, e0 = 0;
      while (p0 < ca.length && p0 < cb.length && eq(ca[p0], cb[p0])) p0++;
      while (e0 < ca.length - p0 && e0 < cb.length - p0 && eq(ca[ca.length - 1 - e0], cb[cb.length - 1 - e0])) e0++;
      const n = Math.max(ca.length, cb.length) - p0 - e0;
      for (let i = p0; i < p0 + n; i++) {
        const x = i < ca.length - e0 ? ca[i] : undefined, y = i < cb.length - e0 ? cb[i] : undefined;
        out.push({ what: `${name} · card ${i + 1}`, was: x ? cardName(x) : '', now: y ? (x ? cardName(y) : `${cardName(y)} added`) : 'removed' });
      }
    }
    const { cards: _a, ...ra } = ha, { cards: _b, ...rb } = hb;
    if (JSON.stringify(ra) !== JSON.stringify(rb)) out.push({ what: `${name} · name or header`, now: 'changed' });
  }
  for (const ha of a.homes) if (!b.homes.some((x) => x.id === ha.id)) out.push({ what: ha.name ?? 'Home', now: 'removed' });
  // the Homes' order + the start page are in the deck too (a drag in the overview / Make start page = something to save)
  const ah = a.homes.map((x) => x.id).filter((id) => b.homes.some((x) => x.id === id)), bh = b.homes.map((x) => x.id).filter((id) => a.homes.some((x) => x.id === id));
  if (ah.join() !== bh.join()) out.push({ what: 'Homes order', was: ah.map(pageName).join(', '), now: bh.map(pageName).join(', ') });
  if (a.start !== b.start) out.push({ what: 'Start page', was: pageName(a.start), now: pageName(b.start) });
  return out;
}
const isDirty = (id: string) => {
  if (!saved) return false;
  const a = savedDeck().screens.find((e) => e.id === id), b = entry(id);
  return JSON.stringify({ ...a, on: isOn(a) }) !== JSON.stringify({ ...b, on: isOn(b) });
};

// ---- the preview ----
function compFor(): { comp: string; props: Record<string, unknown> } {
  const view = st.section === 'home' || (st.section === 'screens' && st.view === 'home') ? 'home' : st.section === 'panel' ? 'settings' : st.sel;
  return target(view);
}
function target(id: string): { comp: string; props: Record<string, unknown> } {
  if (specOf(id)) return { comp: 'spec', props: { screen: id } };
  return { comp: id, props: {} };
}
function defaults(c?: Comp) { return Object.fromEntries((c?.meta.props ?? []).map((p) => [p.name, p.default])); }
function build(s: Stage, id: string | { comp: string; props: Record<string, unknown> }) {
  if (!engine) return 'loading';
  const t = typeof id === 'string' ? target(id) : id;
  const c = engine.comps.find((x) => x.id === t.comp && x.section === 'screens') ?? engine.comps.find((x) => x.id === t.comp);
  if (!c) return `no ${t.comp} in this build`;
  return s.render(c.id, { ...defaults(c), ...t.props }, light(), accentIdx());
}
function renderMain() {
  if (!engine || !main) return;
  const err = build(main, compFor());
  const d = DEVICES[st.device];
  $('ed-meta').textContent = err ? `Preview: ${err}` : deckErr ? `Deck refused: ${deckErr}` : `${d.name} · ${d.size} × ${d.size} · 1:1 · live deck`;
}
async function load() {
  engine?.dispose();
  engine = null; main = null;
  thumbs.clear();
  const holder = $o('ed-canvas') ?? $('ed-flow');
  holder.dataset.view = '';
  holder.replaceChildren(h('p', 'ed-meta', 'Loading the renderer'));
  const e = await Engine.load(DEVICES[st.device].den, host.wasmBase(DEVICES[st.device].den), host.ha);
  engine = e;
  e.onNavigate = (id) => { if (entry(id)) { st.section = 'screens'; st.view = 'screen'; select(id); } };
  applyDeck();
  if (FLOW) { flowRender(); return; }
  main = e.stage(size(), size());
  main.label = 'Preview';
  Object.assign(main.canvas.style, { width: `${size()}px`, height: `${size()}px` });
  $('ed-canvas').replaceChildren(main.canvas);
  renderMain();
  lists();
}

// ---- the page ----
function seg(el: HTMLElement, items: [string, string][], cur: string, pick: (v: string) => void) {
  el.replaceChildren(...items.map(([v, label]) => {
    const b = h('button', '', label);
    b.type = 'button';
    b.setAttribute('aria-pressed', String(v === cur));
    b.dataset.v = v;
    b.onclick = () => pick(v);
    return b;
  }));
}
function sw(on: boolean, label: string, flip: () => void) {
  const b = h('button', 'sw');
  b.type = 'button';
  b.setAttribute('role', 'switch');
  b.setAttribute('aria-checked', String(on));
  b.setAttribute('aria-label', label);
  b.onclick = (ev) => { ev.stopPropagation(); flip(); };
  return b;
}
function setAccentVars() {
  const set = boot.accents.sets.find((a) => a.id === st.accent) ?? boot.accents.sets[0];
  const vals = set[light() ? 'light' : 'dark'];
  for (const [k, v] of Object.entries(vals)) document.documentElement.style.setProperty(`--f-${k}`, v);
}
function top() {
  seg($('ed-device'), host.devices.map((d) => [d, DEVICES[d].name] as [string, string]), st.device, (v) => { st.device = v as DeviceId; url(); top(); void status(); void load(); });
  seg($('ed-theme'), [['dark', 'Dark'], ['light', 'Light'], ['auto', 'Auto']], st.theme, (v) => {
    st.theme = v as typeof st.theme;
    if (v === 'auto') delete document.documentElement.dataset.theme; else document.documentElement.dataset.theme = v;
    ls.set('fe.theme', v);
    restyle();
  });
  $('ed-accents').replaceChildren(...boot.accents.sets.map((a) => {
    const b = h('button');
    b.type = 'button';
    b.title = a.label;
    b.setAttribute('aria-label', `Accent ${a.label}`);
    b.setAttribute('aria-pressed', String(a.id === st.accent));
    b.style.setProperty('--sw', a[light() ? 'light' : 'dark'].accent);
    b.onclick = () => { st.accent = a.id; ls.set('fe.accent', a.id); restyle(); };
    return b;
  }));
  const n = saved ? diff(savedDeck(), deck).length : 0;
  const dirty = $('ed-dirty');
  dirty.hidden = !n;
  dirty.textContent = FLOW ? `${n} unsaved change${n === 1 ? '' : 's'}` : `${n} unsaved`;
  ($('ed-undo') as HTMLButtonElement).disabled = !undo.length;
  const rd = $o('ed-redo') as HTMLButtonElement | null;
  if (rd) rd.disabled = !redo.length;
  ($('ed-save') as HTMLButtonElement).disabled = !n;
  if (FLOW) return;
  const sections: [typeof st.section, string][] = [['screens', 'Screens'], ['home', 'Home cards'], ['panel', 'Panel settings']];
  $('ed-sections').replaceChildren(...sections.map(([v, label]) => {
    const b = h('button', '', label);
    b.type = 'button';
    b.setAttribute('aria-current', String(v === st.section));
    b.onclick = () => { st.section = v; renderMain(); lists(); settings(); top(); };
    return b;
  }));
  seg($('ed-view'), [['screen', 'Screen'], ['home', 'Home']], st.view, (v) => { st.view = v as typeof st.view; top(); renderMain(); });
  $('ed-view').style.visibility = st.section === 'screens' ? 'visible' : 'hidden';
}
function restyle() {
  setAccentVars();
  top();
  if (FLOW) flowRender(); else renderMain();
}
function url() {
  const u = new URLSearchParams(location.search);
  if (st.device === host.devices[0]) u.delete('device'); else u.set('device', st.device);
  if (FLOW) {
    if (flow.step === 'screen' && st.sel) u.set('screen', st.sel); else u.delete('screen');
    if (flow.step === 'home' && st.sel) u.set('home', st.sel); else u.delete('home');
  }
  history.replaceState(null, '', `?${u}`);
}
async function status() {
  const el = $('ed-status');
  el.className = 'ed-status';
  el.querySelector('.txt')!.textContent = 'checking';
  const r = await host.status(st.device);
  el.className = `ed-status ${r.online ? 'on' : 'off'}`;
  el.querySelector('.txt')!.textContent = r.online ? `online · ${r.ip}` : `offline${r.ip ? ` · ${r.ip}` : ''}`;
}
function select(id: string) {
  st.sel = id;
  if (st.section === 'screens') st.view = 'screen';
  renderMain();
  lists();
  settings();
  top();
}

// drag to reorder: drop above / below the hovered row
let dragId = '';
function draggable(el: HTMLElement, id: string, across = false) {
  el.draggable = true;
  el.addEventListener('dragstart', (ev) => { dragId = id; el.classList.add('dragging'); ev.dataTransfer?.setData('text/plain', id); });
  el.addEventListener('dragend', () => { dragId = ''; el.classList.remove('dragging'); });
  const side = (ev: DragEvent) => { const r = el.getBoundingClientRect(); return across ? ev.clientX > r.left + r.width / 2 : ev.clientY > r.top + r.height / 2; };
  const clear = () => el.classList.remove('drop-before', 'drop-after');
  el.addEventListener('dragover', (ev) => {
    if (!dragId || dragId === id) return;
    ev.preventDefault();
    const after = side(ev);
    el.classList.toggle('drop-after', after);
    el.classList.toggle('drop-before', !after);
  });
  el.addEventListener('dragleave', clear);
  el.addEventListener('drop', (ev) => {
    ev.preventDefault();
    clear();
    if (!dragId || dragId === id) return;
    const r = el.getBoundingClientRect();
    const after = across ? ev.clientX > r.left + r.width / 2 : ev.clientY > r.top + r.height / 2;
    const from = dragId;
    edit(() => {
      const moved = deck.screens.splice(deck.screens.findIndex((e) => e.id === from), 1)[0];
      deck.screens.splice(deck.screens.findIndex((e) => e.id === id) + (after ? 1 : 0), 0, moved);
    });
  });
}
function move(id: string, by: number) {
  const i = deck.screens.findIndex((e) => e.id === id), j = i + by;
  if (i < 0 || j < 0 || j >= deck.screens.length) return;
  edit(() => { const [m] = deck.screens.splice(i, 1); deck.screens.splice(j, 0, m); });
}

// ---- pick up and move: the item lifts and follows the pointer, the others make room live, drop = one Undo
// step, Esc = back where it was. Pointer events: mouse / pen lift after 6 px, touch after a still 300 ms hold (a swipe
// still scrolls). Motion = Facet's motion tokens (--mo-zoom, --ease-out); reduced motion = everything jumps.
const reduced = () => matchMedia('(prefers-reduced-motion: reduce)').matches;
const moMs = () => (reduced() ? 0 : parseFloat(getComputedStyle(document.documentElement).getPropertyValue('--mo-zoom')) || 220);
let lifting: (() => void) | null = null;  // Esc = cancel the pick-up
let noClickUntil = 0;  // the click after a drop isn't a click
document.addEventListener('click', (ev) => { if (performance.now() < noClickUntil) { noClickUntil = 0; ev.stopPropagation(); ev.preventDefault(); } }, true);
document.addEventListener('pointerdown', () => { noClickUntil = 0; }, true);
window.addEventListener('keydown', (ev) => { if (lifting && ev.key === 'Escape') { ev.preventDefault(); ev.stopImmediatePropagation(); lifting(); } }, true);
type Lift = { down(ev: PointerEvent): boolean; lift(ev: PointerEvent): void; move(ev: PointerEvent): void; drop(ev: PointerEvent): void; cancel(): void };
function pickup(el: HTMLElement, l: Lift) {
  let p: { id: number; x: number; y: number; on: boolean; t: number } | null = null;
  const end = () => { if (!p) return; clearTimeout(p.t); try { el.releasePointerCapture(p.id); } catch { /* gone */ } p = null; lifting = null; };
  const go = (ev: PointerEvent) => {
    if (!p) return;
    p.on = true;
    try { el.setPointerCapture(p.id); } catch { /* gone */ }  // only now: a capture from the press would retarget a plain click
    lifting = () => { end(); noClickUntil = Infinity; l.cancel(); };  // Esc: the release that follows isn't a click either
    l.lift(ev);
  };
  el.addEventListener('pointerdown', (ev) => {
    if (p || ev.button !== 0 || !l.down(ev)) return;
    p = { id: ev.pointerId, x: ev.clientX, y: ev.clientY, on: false, t: 0 };
    if (ev.pointerType === 'touch') p.t = window.setTimeout(() => { if (p && !p.on) go(ev); }, 300);
  });
  el.addEventListener('pointermove', (ev) => {
    if (!p || ev.pointerId !== p.id) return;
    if (!p.on) {
      if (Math.hypot(ev.clientX - p.x, ev.clientY - p.y) <= 6) return;
      if (ev.pointerType === 'touch') { end(); return; }  // moved before the hold = a scroll
      go(ev);
    }
    l.move(ev);
  });
  el.addEventListener('pointerup', (ev) => { if (!p || ev.pointerId !== p.id) return; const on = p.on; end(); if (on) { noClickUntil = performance.now() + 300; l.drop(ev); } });
  el.addEventListener('pointercancel', (ev) => { if (!p || ev.pointerId !== p.id) return; const on = p.on; end(); if (on) l.cancel(); });
  el.addEventListener('touchmove', (ev) => { if (p?.on) ev.preventDefault(); }, { passive: false });
  el.addEventListener('contextmenu', (ev) => { if (p) ev.preventDefault(); });
}
/** the lifted copy: a tile's clone (its canvases copied) or a crop of a canvas, raised, following the pointer */
function floatCopy(from: HTMLElement, r: DOMRect, ev: PointerEvent, crop?: [number, number, number, number]) {
  const f = h('div', 'ed-lift');
  let node: HTMLElement;
  if (crop) {
    const c = document.createElement('canvas');
    c.width = crop[2]; c.height = crop[3];
    c.getContext('2d')!.drawImage(from as HTMLCanvasElement, crop[0], crop[1], crop[2], crop[3], 0, 0, crop[2], crop[3]);
    node = c;
  } else {
    node = from.cloneNode(true) as HTMLElement;
    const a = from.querySelectorAll('canvas'), b = node.querySelectorAll('canvas');
    a.forEach((c, i) => { b[i].width = c.width; b[i].height = c.height; b[i].getContext('2d')?.drawImage(c, 0, 0); });
    node.classList.remove('lifted');
  }
  f.append(node);
  Object.assign(f.style, { width: `${r.width}px`, height: `${r.height}px` });
  const ox = ev.clientX - r.left, oy = ev.clientY - r.top;
  const at = (x: number, y: number) => { f.style.transform = `translate(${x}px, ${y}px)`; };
  at(r.left, r.top);
  document.body.append(f);
  requestAnimationFrame(() => f.classList.add('up'));
  return {
    move: (e: PointerEvent) => at(e.clientX - ox, e.clientY - oy),
    /** settle into r (its slot), then go */
    land: (to: { left: number; top: number }, then?: () => void) => {
      f.classList.add('landing'); f.classList.remove('up'); at(to.left, to.top);
      window.setTimeout(() => { f.remove(); then?.(); }, moMs());
    },
  };
}
/** move the DOM, the moved elements glide from where they were (FLIP) */
function flip(els: HTMLElement[], change: () => void) {
  const before = new Map(els.map((e) => [e, e.getBoundingClientRect()]));
  change();
  if (reduced()) return;
  for (const e of els) {
    const a = before.get(e)!, b = e.getBoundingClientRect(), dx = a.left - b.left, dy = a.top - b.top;
    if (!dx && !dy) continue;
    e.style.transition = 'none';
    e.style.transform = `translate(${dx}px, ${dy}px)`;
    void e.offsetWidth;
    e.style.transition = 'transform var(--mo-zoom) var(--ease-out)';
    e.style.transform = '';
  }
}
/** where it sits in the layout (a FLIP glide still under way doesn't count, so the order can't flicker back) */
const restRect = (e: HTMLElement) => { const r = e.getBoundingClientRect(), m = new DOMMatrixReadOnly(getComputedStyle(e).transform); return new DOMRect(r.left - m.m41, r.top - m.m42, r.width, r.height); };
const inside = (r: DOMRect, ev: PointerEvent) => ev.clientX >= r.left && ev.clientX < r.right && ev.clientY >= r.top && ev.clientY < r.bottom;
/** overview grid: pick a tile up, the others make room live, drop = commit(ids in the new order) */
function sortTiles(grid: HTMLElement, commit: (ids: string[]) => void) {
  let cur: HTMLElement | null = null, fl: ReturnType<typeof floatCopy> | null = null, first: string[] = [];
  const tiles = () => [...grid.querySelectorAll<HTMLElement>('.tile[data-id]')];
  const slotRect = () => { cur!.style.transition = 'none'; cur!.style.transform = ''; return cur!.getBoundingClientRect(); };
  pickup(grid, {
    down: (ev) => { const t = ev.target as HTMLElement; cur = t.closest<HTMLElement>('.tile[data-id]'); return !!cur && !t.closest('button'); },
    lift: (ev) => { first = tiles().map((t) => t.dataset.id!); fl = floatCopy(cur!, cur!.getBoundingClientRect(), ev); cur!.classList.add('lifted'); },
    move: (ev) => {
      fl!.move(ev);
      const ts = tiles(), over = ts.find((t) => t !== cur && inside(restRect(t), ev));
      if (over) flip(ts, () => grid.insertBefore(cur!, ts.indexOf(over) > ts.indexOf(cur!) ? over.nextSibling : over));
    },
    drop: () => {
      const ids = tiles().map((t) => t.dataset.id!), c = cur!;
      fl!.land(slotRect(), () => c.classList.remove('lifted'));
      if (ids.join() !== first.join()) commit(ids);
    },
    cancel: () => {
      const ts = tiles(), tail = grid.querySelector('.tile.add'), c = cur!;
      flip(ts, () => { for (const id of first) grid.insertBefore(ts.find((t) => t.dataset.id === id)!, tail); });
      fl!.land(slotRect(), () => c.classList.remove('lifted'));
    },
  });
}
const byIds = <T extends { id: string }>(xs: T[], ids: string[]) => xs.sort((a, b) => ids.indexOf(a.id) - ids.indexOf(b.id));
const toggle = (id: string) => edit(() => { const e = entry(id)!; e.on = !isOn(e); });
function caption(e: Entry) {
  const parts = [srcName(e.id, e.source)];
  if (!isOn(e)) parts.unshift('Off');
  return parts.filter(Boolean).join(' · ') || (specOf(e.id) ? 'Screen' : 'Built in');
}
function lists() {
  const box = $('ed-screens');
  if (st.section !== 'screens') { box.replaceChildren(placeholder()); return; }
  const head = h('div', 'list-head');
  head.append(h('span', 'cap', 'Screens'), h('span', 'ed-meta', `${deck.screens.filter(isOn).length} of ${deck.screens.length} on`));
  box.replaceChildren(head, ...deck.screens.map((e) => {
    const r = h('div', `row${isOn(e) ? '' : ' off'}`);
    r.tabIndex = 0;
    r.setAttribute('role', 'option');
    r.setAttribute('aria-selected', String(e.id === st.sel));
    const txt = h('div');
    const nm = h('div', 'nm', nameOf(e.id));
    if (isDirty(e.id)) nm.append(h('span', 'changed'));
    txt.append(nm, h('div', 'sub', caption(e)));
    r.append(h('span', 'grip', '⋮⋮'), txt, sw(isOn(e), `Show ${nameOf(e.id)}`, () => toggle(e.id)));
    r.onclick = () => select(e.id);
    r.onkeydown = (ev) => {
      if (ev.altKey && (ev.key === 'ArrowUp' || ev.key === 'ArrowDown')) { ev.preventDefault(); move(e.id, ev.key === 'ArrowUp' ? -1 : 1); (box.querySelector('[aria-selected="true"]') as HTMLElement | null)?.focus(); }
      else if (ev.key === 'Enter' || ev.key === ' ') { ev.preventDefault(); select(e.id); }
    };
    draggable(r, e.id);
    return r;
  }), ...sentRows());
}
/** the Sent list: screens sent to this panel that aren't in the deck, each with Add to deck + Remove */
function sentRows(): HTMLElement[] {
  const sent = (src?.sent ?? []).filter((x) => x.role !== 'detail' && !entry(x.id));
  if (!sent.length) return [];
  const head = h('div', 'list-head');
  head.append(h('span', 'cap', 'Sent'), h('span', 'ed-meta', `${sent.length}`));
  return [head, ...sent.map((x) => {
    const r = h('div', 'row sent');
    const txt = h('div');
    txt.append(h('div', 'nm', x.name), h('div', 'sub', 'Sent to this panel'));
    const acts = h('div', 'acts');
    const add = h('button', 'btn', 'Add to deck');
    add.type = 'button';
    add.onclick = () => edit(() => { deck.screens.push({ id: x.id, kind: x.id, on: true } as Entry); });
    const rm = h('button', 'btn ghost', 'Remove');
    rm.type = 'button';
    rm.disabled = !host.removeSent;
    rm.onclick = async () => {
      if (!confirm(`Remove ${x.name} from this panel? It goes back to how it was before it was sent.`)) return;
      if (await host.removeSent!(st.device, x.id)) { src!.sent = src!.sent!.filter((y) => y.id !== x.id); lists(); }
    };
    acts.append(add, rm);
    r.append(h('span', 'grip'), txt, acts);
    return r;
  })];
}
function placeholder() {
  const p = h('div', 'placeholder');
  if (st.section === 'home') p.append(h('h2', '', 'Home cards'), h('p', '', 'Which cards Home shows, their order and size. The preview shows Home from the deck as it is now; this section comes in a later round.'));
  else p.append(h('h2', '', 'Panel settings'), h('p', '', 'Name, brightness, sleep, sounds, theme and accent for this panel. The preview shows the panel\'s own Settings screen; this section comes in a later round.'));
  return p;
}

// ---- the selected screen's settings: source catalogue + typed params ----
function settings(box = $('ed-settings'), only?: string[]) {
  if (st.section !== 'screens') { box.replaceChildren(placeholder()); return; }
  const e = entry(st.sel);
  if (!e) { box.replaceChildren(h('p', 'empty', 'Pick a screen to change its settings.')); return; }
  const out: HTMLElement[] = [];
  const head = h('div', 'set-head');
  const t = h('div');
  t.append(h('span', 'cap', only ? nameOf(e.id) : 'Screen'), h('h2', '', only ? itemName(flow.item) : nameOf(e.id)));
  head.append(t);
  if (FLOW) {
    const x = h('button', 'btn ghost', 'Close');
    x.type = 'button';
    x.title = 'Close (Esc)';
    x.onclick = closeDrawer;
    head.append(x);
  }
  out.push(head);
  if (only) {
    const sec = h('section', 'set-sec');
    const was = saved ? savedDeck().screens.find((x) => x.id === e.id)?.params ?? {} : {};
    for (const k of only) {
      const p = typed(specOf(e.id)?.params[k] ?? '');
      if (p.type === 'hidden') continue;
      const val = e.params?.[k] ?? p.default ?? '';
      sec.append(control(k, p, val, (v, hist = true) => edit(() => { e.params = { ...e.params, [k]: v }; }, hist), (was[k] ?? p.default ?? '') !== val, 'auto' in (specOf(e.id)?.params ?? {})));
    }
    const more = h('button', 'btn ghost', 'All screen settings');
    more.type = 'button';
    more.onclick = () => openDrawer('screen');
    sec.append(more);
    out.push(sec);
    box.replaceChildren(...out);
    return;
  }
  const on = h('div', 'field');
  const onr = h('div', 'field-row');
  onr.append(h('span', 'lbl', 'Show on Home'), sw(isOn(e), 'Show on Home', () => toggle(e.id)));
  on.append(onr);
  out.push(on);
  // source: the catalogue's sources for this screen, what discovery found
  const cat = boot.catalogue.screens[e.id];
  if (cat) {
    const sec = h('section', 'set-sec');
    sec.append(h('span', 'cap', 'Source'));
    const list = h('div', 'srcs');
    for (const s of cat.sources) {
      const f = src?.screens[e.id]?.found[s.id];
      const b = h('button', 'src');
      b.type = 'button';
      b.setAttribute('role', 'radio');
      b.setAttribute('aria-checked', String(e.source === s.id));
      b.disabled = f ? !f.found : false;
      const nm = h('span');
      nm.append(h('div', 'nm', s.name), h('div', 'ed-meta', s.kind === 'ha' ? 'Home Assistant' : s.kind === 'direct' ? 'Direct' : 'Built in'));
      b.append(h('span', 'radio'), nm, h('span', `ev${f?.found ? ' ok' : ''}`, f?.evidence ?? ''));
      b.onclick = () => pickSource(e, s);
      list.append(b);
    }
    sec.append(list);
    if (cat.needs) sec.append(h('p', 'ed-meta', `Needs ${cat.needs}.`));
    out.push(sec);
  }
  // the spec's typed params
  const params = Object.entries(specOf(e.id)?.params ?? {}).map(([k, p]) => [k, typed(p)] as const).filter(([, p]) => p.type !== 'hidden');
  const sec = h('section', 'set-sec');
  sec.append(h('span', 'cap', 'Settings'));
  if (!params.length) sec.append(h('p', 'empty', specOf(e.id) ? 'This screen has no settings yet.' : 'A built-in screen: nothing to set here.'));
  const was = saved ? savedDeck().screens.find((x) => x.id === e.id)?.params ?? {} : {};
  for (const [k, p] of params) {
    const val = e.params?.[k] ?? p.default ?? '';
    const set = (v: string, hist = true) => edit(() => { e.params = { ...e.params, [k]: v }; }, hist);
    sec.append(control(k, p, val, set, (was[k] ?? p.default ?? '') !== val, 'auto' in (specOf(e.id)?.params ?? {})));
  }
  out.push(sec);
  const reset = h('button', 'btn', 'Reset this screen');
  reset.type = 'button';
  reset.disabled = !isDirty(e.id);
  reset.onclick = () => edit(() => { const s = savedDeck().screens.find((x) => x.id === e.id); if (s) Object.assign(e, { on: undefined, source: undefined, params: undefined }, s); });
  const foot = h('div', 'set-sec');
  foot.append(reset);
  out.push(foot);
  box.replaceChildren(...out);
}
/** pick a source: the params it fills take what it found in this house (an entity list re-picked, one entity kept
 *  when it is Auto or still in the new lists, else the source's pick); params it does not fill stay. One Undo step. */
function pickSource(e: Entry, s: Src) {
  const got = src?.screens[e.id]?.fills?.[s.id] ?? {};
  edit(() => {
    e.source = s.id;
    const p = { ...e.params };
    for (const k of Object.keys(s.fills ?? {})) {
      const v = got[k];
      if (v === undefined) continue;
      const cur = p[k] ?? '';
      const keep = typed(specOf(e.id)?.params[k] ?? '').type !== 'entities' && !!cur
        && (cur === 'auto' || Object.values(got).some((x) => x.split(',').includes(cur)));
      if (!keep) p[k] = v;
    }
    e.params = p;
  });
}
function control(k: string, p: ReturnType<typeof typed>, val: string, set: (v: string, hist?: boolean) => void, changedFlag: boolean, hasAuto: boolean) {
  const f = h('div', `field${changedFlag ? ' changed' : ''}`);
  const id = `p-${k}`;
  const lbl = h('label', 'lbl', p.label ?? k);
  lbl.htmlFor = id;
  const opts = src?.options[p.domain ?? ''] ?? [];
  if (p.type === 'toggle') {
    const r = h('div', 'field-row');
    r.append(lbl, sw(val === 'true', p.label ?? k, () => set(val === 'true' ? 'false' : 'true')));
    f.append(r);
    return f;
  }
  f.append(lbl);
  if (p.type === 'select' && (p.options?.length ?? 0) <= 5) {
    const s = h('div', 'seg');
    seg(s, (p.options ?? []).map((o) => [o, o.charAt(0).toUpperCase() + o.slice(1)]), val, (v) => set(v));
    f.append(s);
  } else if (p.type === 'select' || (p.type === 'entity' && opts.length <= 60)) {
    const s = h('select', 'inp');
    s.id = id;
    const list: [string, string][] = p.type === 'select' ? (p.options ?? []).map((o) => [o, o]) : [['', 'None'], ...(hasAuto && k === 'entity' ? [['auto', 'Auto (follows the house target)'] as [string, string]] : []), ...opts.map((o) => [o.id, o.name] as [string, string])];
    if (val && !list.some(([v]) => v === val)) list.push([val, val]);
    for (const [v, t] of list) { const o = h('option', '', t); o.value = v; o.selected = v === val; s.append(o); }
    s.onchange = () => set(s.value);
    f.append(s);
  } else if (p.type === 'entities') {
    const cur = new Set(val.split(',').filter(Boolean));
    const box = h('div', 'checks');
    for (const o of opts) {
      const l = h('label');
      const c = h('input');
      c.type = 'checkbox';
      c.checked = cur.has(o.id);
      c.onchange = () => { if (c.checked) cur.add(o.id); else cur.delete(o.id); set(opts.map((x) => x.id).filter((x) => cur.has(x)).join(',')); };
      l.append(c, document.createTextNode(o.name));
      box.append(l);
    }
    f.append(box, h('span', 'hint', `${cur.size} of ${opts.length} picked`));
  } else {
    // text, or an entity of a big domain (sensors): type to search
    const i = h('input', 'inp');
    i.id = id;
    i.value = val;
    if (p.type === 'entity') {
      const dl = h('datalist');
      dl.id = `${id}-list`;
      for (const o of opts) { const x = h('option'); x.value = o.id; x.label = o.name; dl.append(x); }
      i.setAttribute('list', dl.id);
      f.append(dl);
    }
    let first = true;
    i.oninput = () => { set(i.value, first); first = false; };  // live preview; one Undo step per edit
    i.onchange = () => { first = true; };
    f.append(i);
    if (p.type === 'entity' && val) f.append(h('span', 'hint', optName(p.domain ?? '', val)));
  }
  return f;
}

// ---- Save: what would be sent ----
function save() {
  const d = $('ed-dialog') as HTMLDialogElement;
  const ch = diff(savedDeck(), deck);
  const dev = DEVICES[st.device];
  const box = h('div', 'dlg');
  box.append(h('span', 'cap', 'Save to panel'), h('h2', '', `${ch.length} change${ch.length === 1 ? '' : 's'} for ${dev.name}`));
  const ul = h('ul');
  for (const c of ch) {
    const li = h('li');
    const now = h('span');
    if (c.was !== undefined) { now.append(h('span', 'was', c.was || 'none'), document.createTextNode('  →  ')); }
    now.append(document.createTextNode(c.now || 'none'));
    li.append(h('b', '', c.what), now);
    ul.append(li);
  }
  box.append(ul);
  const next = { ...deck, rev: savedDeck().rev + 1 };
  const out = toDeck(next);
  const det = h('details');
  det.append(h('summary', 'ed-meta', `${host.live ? 'Sends' : 'Would send'}: POST /deck (rev ${next.rev}, ${JSON.stringify(out).length} bytes)`), Object.assign(h('pre'), { textContent: JSON.stringify(out, null, 1) }));
  box.append(det, h('div', 'note', host.saveNote));
  const acts = h('div', 'acts');
  const cancel = h('button', 'btn ghost', 'Keep editing');
  cancel.type = 'button';
  cancel.onclick = () => d.close();
  const ok = h('button', 'btn primary', host.saveLabel);
  ok.type = 'button';
  const res = h('div', 'note');
  res.hidden = true;
  ok.onclick = async () => {
    ok.disabled = true;
    const r = await host.save(st.device, out);
    if (host.live && !r.sent) { res.hidden = false; res.textContent = r.note; ok.disabled = false; return; }  // keep the edits unsaved
    deck = next; saved = JSON.stringify(deck); undo.length = 0; d.close(); changed();
  };
  box.append(res);
  acts.append(cancel, ok);
  box.append(acts);
  d.replaceChildren(box);
  d.showModal();
}

// ---- API keys (the panel's own editor only: host.agents): the switch Allow API access, the keys with Revoke,
// Create key = the same request an agent makes (the panel asks "Allow <name>?" on its glass; the key shows here once)
async function agentsDialog() {
  const A = host.agents;
  if (!A) return;
  const d = $('ed-dialog') as HTMLDialogElement;
  const box = h('div', 'dlg agents');
  d.replaceChildren(box);
  if (!d.open) d.showModal();
  const draw = async (made?: HTMLElement) => {
    const a = await A.list();
    box.replaceChildren(h('span', 'cap', 'API'), h('h2', '', 'API keys'));
    if (!a) { box.append(h('div', 'note', 'The panel did not answer.')); return; }
    const row = h('div', 'ag-row');
    const lab = h('div', 'txt');
    lab.append(h('div', 'nm', 'Allow API access'), h('div', 'sub', 'Lets scripts, automations and agents on your network change this panel, each with its own key.'));
    const err = h('div', 'note');
    err.hidden = true;
    row.append(lab, sw(a.on, 'Allow API access', async () => {
      const e = await A.setOn(!a.on);
      if (e) { err.hidden = false; err.textContent = e; return; }
      void draw();
    }));
    box.append(row, err);
    const ul = h('ul', 'ag-keys');
    if (!a.keys.length) ul.append(h('li', 'empty', 'No keys yet.'));
    for (const k of a.keys) {
      const li = h('li');
      const t = h('div', 'txt');
      t.append(h('div', 'nm', k.name), h('div', 'sub', `${k.used_words} · ${k.ends_words}`));
      const rv = h('button', 'btn ghost', 'Revoke');
      rv.type = 'button';
      rv.onclick = async () => { rv.disabled = true; await A.revoke(k.id); void draw(); };
      li.append(t, rv);
      ul.append(li);
    }
    box.append(h('h3', '', 'Keys'), ul);
    // Create key: a name + how long, then the panel asks on its glass
    const form = h('div', 'ag-new');
    const name = Object.assign(h('input'), { type: 'text', placeholder: 'Script on my laptop', ariaLabel: 'Name for the key', maxLength: 60 }) as HTMLInputElement;
    const life = h('select');
    life.setAttribute('aria-label', 'How long the key lasts');
    for (const [v, l] of [['30d', 'Ends after 30 days unused'], ['1d', 'Ends after 1 day'], ['never', 'Until revoked']]) life.append(Object.assign(h('option', '', l), { value: v }));
    const go = h('button', 'btn primary', 'Create key');
    go.type = 'button';
    go.disabled = !a.on;
    const say = h('div', 'note');
    say.hidden = a.on;
    if (!a.on) say.textContent = 'Turn on Allow API access first.';
    go.onclick = async () => {
      const n = name.value.trim();
      if (!n) { name.focus(); return; }
      go.disabled = true;
      say.hidden = false;
      const r = await A.request(n, life.value as '1d' | '30d' | 'never');
      if (!r.id) { say.textContent = r.error ?? 'The panel refused.'; go.disabled = false; return; }
      say.textContent = 'Tap Allow on the panel. It waits two minutes.';
      for (;;) {
        await new Promise((ok) => setTimeout(ok, 2000));
        if (!d.open) return;
        const p = await A.poll(r.id);
        if (p.state === 'waiting') continue;
        if (p.state === 'allowed' && p.key) {
          const k = h('div', 'ag-key');
          k.append(h('div', 'sub', 'The key, shown this once: copy it now.'), Object.assign(h('pre'), { textContent: p.key }));
          void draw(k);
        } else { say.textContent = p.error ?? 'Not allowed.'; go.disabled = false; }
        return;
      }
    };
    form.append(name, life, go);
    box.append(h('h3', '', 'Create key'), form, say);
    if (made) box.append(made);
    box.append(h('p', 'sub', 'A key can do what this editor does. Plain HTTP on your own Wi-Fi: the editor is open on the LAN, as panels at this tier are.'));
    const acts = h('div', 'acts');
    const close = h('button', 'btn ghost', 'Close');
    close.type = 'button';
    close.onclick = () => d.close();
    acts.append(close);
    box.append(acts);
  };
  await draw();
}

// test bridge (editor tests): what the editor holds + what each preview stage really drew (fw_audit labels,
// fw_comps boxes), so the end-to-end tests assert the rendered outcome, not the DOM around it. Read-only.
const stageBy = (label: string) => (engine ? [...engine.views].find((s) => s.label === label) ?? null : null);
(window as any).__fe = {
  ready: () => !!engine && (FLOW ? !!$o('ed-flow')?.dataset.view : !!main),
  deck: () => toDeck(deck),
  saved: () => toDeck(savedDeck()),
  undo: () => undo.length,
  flow: () => ({ step: flow.step, drawer: flow.drawer, card: flow.card, sel: st.sel, modal: cm ? cm.i : -1, err: deckErr }),
  stages: () => (engine ? [...engine.views].map((s) => s.label) : []),
  texts: (label: string) => { const s = stageBy(label); if (!s || !engine) return null; try { return (JSON.parse(engine.str(engine.m._fw_audit(s.id))) as { text?: string; y: number; x: number }[]).filter((o) => o.text).map((o) => ({ t: o.text!, x: o.x, y: o.y })); } catch { return null; } },
  comps: (label: string) => { const s = stageBy(label); return s ? s.comps() : null; },
  boxes: (label = 'Preview') => homeBoxes(stageBy(label)),
};

export async function start() {
  setAccentVars();
  top();
  void status();
  $('ed-undo').onclick = doUndo;
  const rd = $o('ed-redo');
  if (rd) rd.onclick = doRedo;
  $('ed-save').onclick = save;
  const ag = $o('ed-agents');  // the panel's own editor only (host.agents)
  if (ag) { ag.hidden = !host.agents; ag.onclick = () => void agentsDialog(); }
  const rf = $o('ed-refresh');
  if (rf) rf.onclick = () => engine?.haNow();
  addEventListener('keydown', (ev) => {
    const typing = ev.target instanceof HTMLInputElement || ev.target instanceof HTMLSelectElement;
    if ((ev.ctrlKey || ev.metaKey) && ev.key.toLowerCase() === 'z' && !typing) { ev.preventDefault(); if (ev.shiftKey) doRedo(); else doUndo(); }
    else if ((ev.ctrlKey || ev.metaKey) && ev.key === 'y' && !typing) { ev.preventDefault(); doRedo(); }
    else if (cm && ev.altKey && ['ArrowLeft', 'ArrowUp', 'ArrowRight', 'ArrowDown'].includes(ev.key)) { ev.preventDefault(); moveCard(ev.key === 'ArrowLeft' || ev.key === 'ArrowUp' ? -1 : 1); }
    else if (FLOW && ev.key === 'Escape' && !($('ed-dialog') as HTMLDialogElement).open) { ev.preventDefault(); if (flow.drawer) { if (flow.step === 'home') closeHomeDrawer(); else closeDrawer(); } else if (flow.step !== 'panel') openPanel(); }
  });
  matchMedia('(prefers-color-scheme: light)').addEventListener('change', () => { if (st.theme === 'auto') restyle(); });
  const dlg = $('ed-dialog') as HTMLDialogElement;  // a click on the backdrop (outside the box) closes any dialog
  dlg.addEventListener('click', (ev) => { if (ev.target === dlg) dlg.close(); });
  src = await host.discover();
  if (src?.deck) deck = fromDeck(src.deck);  // else the factory deck in the engine
  saved = JSON.stringify(deck);
  st.sel = q.get('screen') ?? deck.screens.find((e) => e.id === 'media')?.id ?? deck.screens[0]?.id ?? '';
  if (FLOW) {
    flow.step = q.get('screen') && entry(q.get('screen')!) ? 'screen' : 'panel';
    if (q.get('home') && deck.homes.some((x) => x.id === q.get('home'))) { flow.step = 'home'; st.sel = q.get('home')!; }
    addEventListener('resize', () => { if (flow.step === 'screen') markSel(); });
    top();
    await load();
    return;
  }
  lists();
  settings();
  top();
  await load();
}

// ---- the click-to-edit flow (workbench page): Panel overview -> Screen -> Item ----
const pretty = (id: string) => id.replace(/_/g, ' ').replace(/^./, (c) => c.toUpperCase());
function itemName(it: Hit | null) {
  if (!it) return '';
  const p = it.prop ? specOf(st.sel)?.params[it.params[0]] : null;
  return it.prop ? (p && typeof p !== 'string' && p.label) || pretty(it.prop) : pretty(it.c);
}
function hits(): Hit[] {
  if (!engine?.m._fw_hits || !main) return [];
  try { return JSON.parse(engine.str(engine.m._fw_hits(main.id))); } catch { return []; }
}
function hitAt(x: number, y: number): Hit | null {
  if (!engine?.m._fw_hit || !main) return null;
  try { return JSON.parse(engine.str(engine.m._fw_hit(main.id, x, y))); } catch { return null; }
}
function crumbs() {
  const nav = $('ed-crumbs');
  const out: HTMLElement[] = [];
  const link = (label: string, go: (() => void) | null) => {
    const b = h(go ? 'button' : 'span', go ? 'crumb' : 'crumb cur', label);
    if (go) { (b as HTMLButtonElement).type = 'button'; b.onclick = go; } else b.setAttribute('aria-current', 'page');
    out.push(b);
  };
  const sep = () => out.push(h('span', 'crumb-sep', '›'));
  if (flow.step === 'home') {
    const hm = home();
    link('Panel', openPanel); sep();
    link(hm?.name ?? 'Home', flow.drawer ? closeHomeDrawer : null);
    if (flow.drawer) { sep(); link(flow.card >= 0 ? cardName(hm?.cards?.[flow.card]) : flow.drawer === 'home' ? 'Header actions' : '', null); }
    nav.replaceChildren(...out);
    return;
  }
  const scr = flow.step === 'screen';
  link('Panel', scr ? openPanel : null);
  if (scr) { sep(); link(nameOf(st.sel), flow.drawer ? closeDrawer : null); }
  if (scr && flow.drawer) { sep(); link(flow.drawer === 'item' ? itemName(flow.item) : 'Screen settings', null); }
  nav.replaceChildren(...out);
}
function flowRender() {
  if (!engine) return;
  crumbs();
  if (flow.step === 'panel') overview(); else if (flow.step === 'home') homeView(); else screenView();
}
function openPanel() {
  flow.step = 'panel'; flow.drawer = null; flow.item = null;
  url(); flowRender();
}
function openScreen(id: string) {
  st.sel = id; flow.step = 'screen'; flow.drawer = null; flow.item = null;
  url(); flowRender();
}
function openDrawer(kind: 'screen' | 'item' | 'slot', it: Hit | null = null, sl: SlotPick | null = null) {
  flow.drawer = kind; flow.item = it; flow.slot = sl;
  crumbs(); drawer(); markSel();
}
function closeDrawer() {
  flow.drawer = null; flow.item = null; flow.slot = null;
  crumbs(); drawer(); markSel();
}

// Panel overview: every deck screen as a live thumbnail, deck order, drag to reorder, on / off, + Add screen
function freeThumbs() { for (const s of thumbs.values()) s.free(); thumbs.clear(); }
function thumb(id: string, pool: Map<string, Stage>) {
  let s = pool.get(id);
  if (!s) { s = engine!.stage(size(), size()); s.label = `Thumb ${id}`; pool.set(id, s); }
  build(s, id);
  return s.canvas;
}
function overview() {
  if (main) { main.free(); main = null; }
  for (const [id, s] of thumbs) if (!entry(id) && !(id.startsWith('home:') && deck.homes.some((x) => `home:${x.id}` === id))) { s.free(); thumbs.delete(id); }
  const box = $('ed-flow');
  box.dataset.view = 'panel';
  const head = h('div', 'ov-head');
  const t = h('div');
  t.append(h('span', 'cap', DEVICES[st.device].name), h('h1', '', 'Screens'));
  head.append(t, h('span', 'ed-meta', `${deck.screens.filter(isOn).length} of ${deck.screens.length} on Home · drag (or Alt + arrows) to reorder`));
  // the Homes first (each a room's page: click = its cards and header actions)
  const homesHead = h('div', 'ov-head');
  const ht = h('div');
  ht.append(h('span', 'cap', 'Pages'), h('h1', '', 'Homes'));
  homesHead.append(ht, h('span', 'ed-meta', 'one per room · click to edit its cards, drag to reorder'));
  const hgrid = h('div', 'ov-grid');
  for (const hm of deck.homes) {
    const tile = h('div', 'tile');
    tile.tabIndex = 0;
    tile.setAttribute('role', 'button');
    tile.setAttribute('aria-label', `Edit ${hm.name ?? 'Home'}`);
    const shot = h('div', 'shot');
    let s = thumbs.get(`home:${hm.id}`);
    if (!s) { s = engine!.stage(size(), size()); s.label = `Thumb home ${hm.id}`; thumbs.set(`home:${hm.id}`, s); }
    build(s, { comp: 'home', props: { page: hm.id } });
    shot.append(s.canvas);
    const foot = h('div', 'tile-foot');
    const txt = h('div');
    txt.append(h('div', 'nm', hm.name ?? 'Home'), h('div', 'sub', `${hm.cards?.length ?? 0} cards${hm.id === deck.start ? ' · start page' : ''}`));
    foot.append(txt);
    tile.append(shot, foot);
    tile.dataset.id = hm.id;
    tile.dataset.testid = 'home-tile';
    tile.onclick = () => openHome(hm.id);
    tile.onkeydown = (ev) => { if (ev.key === 'Enter' || ev.key === ' ') { ev.preventDefault(); openHome(hm.id); } };
    hgrid.append(tile);
  }
  // + Add Home (an empty room page; its cards next, in its view)
  const addH = h('button', 'tile add');
  addH.type = 'button';
  addH.dataset.testid = 'add-home';
  addH.append(h('span', 'plus', '+'), h('span', 'nm', 'Add Home'));
  addH.onclick = () => {
    const used = new Set([...deck.homes, ...deck.screens].map((x) => x.id));
    let n = deck.homes.length + 1;
    while (used.has(`home${n}`)) n++;
    edit(() => { deck.homes.push({ id: `home${n}`, kind: 'home', name: `Home ${n}`, cards: [] }); });
    openHome(`home${n}`);
  };
  hgrid.append(addH);
  const grid = h('div', 'ov-grid');
  for (const e of deck.screens) {
    const tile = h('div', `tile${isOn(e) ? '' : ' off'}`);
    tile.tabIndex = 0;
    tile.setAttribute('role', 'button');
    tile.setAttribute('aria-label', `Edit ${nameOf(e.id)}`);
    const shot = h('div', 'shot');
    shot.append(thumb(e.id, thumbs));
    const foot = h('div', 'tile-foot');
    const txt = h('div');
    const nm = h('div', 'nm', nameOf(e.id));
    if (isDirty(e.id)) nm.append(h('span', 'changed'));
    txt.append(nm, h('div', 'sub', caption(e)));
    foot.append(txt, sw(isOn(e), `Show ${nameOf(e.id)} on Home`, () => toggle(e.id)));
    tile.append(shot, foot);
    tile.onclick = () => openScreen(e.id);
    tile.onkeydown = (ev) => {
      if (ev.target !== tile) return;
      if (ev.altKey && ['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown'].includes(ev.key)) {
        ev.preventDefault();
        move(e.id, ev.key === 'ArrowLeft' || ev.key === 'ArrowUp' ? -1 : 1);
        (box.querySelector(`[aria-label="Edit ${nameOf(e.id)}"]`) as HTMLElement | null)?.focus();
      } else if (ev.key === 'Enter' || ev.key === ' ') { ev.preventDefault(); openScreen(e.id); }
    };
    tile.dataset.id = e.id;
    tile.dataset.testid = 'screen-tile';
    grid.append(tile);
  }
  const add = h('button', 'tile add');
  add.type = 'button';
  add.append(h('span', 'plus', '+'), h('span', 'nm', 'Add screen'));
  add.disabled = !addable().length;
  add.dataset.testid = 'add-screen';
  add.onclick = gallery;
  grid.append(add);
  sortTiles(hgrid, (ids) => edit(() => byIds(deck.homes, ids)));
  sortTiles(grid, (ids) => edit(() => byIds(deck.screens, ids)));
  box.replaceChildren(homesHead, hgrid, head, grid);
}
/** spec screens not in the deck yet (sub-views a screen opens itself are left out: no catalogue entry) */
const addable = () => Object.keys(boot.specs).filter((id) => !entry(id) && boot.specs[id].role !== 'detail');  // page kinds (spec role page / both)
function gallery() {
  const d = $('ed-dialog') as HTMLDialogElement;
  const pool = new Map<string, Stage>();
  const box = h('div', 'dlg');
  box.append(h('span', 'cap', 'Add screen'), h('h2', '', 'Pick a screen for this panel'));
  const grid = h('div', 'ov-grid gal');
  for (const id of addable()) {
    const b = h('button', 'tile');
    b.type = 'button';
    const shot = h('div', 'shot');
    shot.append(thumb(id, pool));
    const foot = h('div', 'tile-foot');
    const txt = h('div');
    txt.append(h('div', 'nm', nameOf(id)), h('div', 'sub', boot.catalogue.screens[id]?.needs ? `Needs ${boot.catalogue.screens[id].needs}` : 'Every home'));
    foot.append(txt);
    b.append(shot, foot);
    b.onclick = () => { d.close(); edit(() => { deck.screens.push({ id, kind: id, on: true }); }); };
    grid.append(b);
  }
  const acts = h('div', 'acts');
  const cancel = h('button', 'btn ghost', 'Cancel');
  cancel.type = 'button';
  cancel.onclick = () => d.close();
  acts.append(cancel);
  box.append(grid, acts);
  d.replaceChildren(box);
  d.classList.add('wide');
  d.addEventListener('close', () => { for (const s of pool.values()) s.free(); d.classList.remove('wide'); }, { once: true });
  d.showModal();
}

// Screen: the real render large; hover outlines the anchored regions, click = that region's params (the drawer)
let hov: HTMLElement | null = null, sel: HTMLElement | null = null, selTimer = 0;
function screenView() {
  freeThumbs();
  const e = entry(st.sel);
  if (!e) { openPanel(); return; }
  if (!main) { main = engine!.stage(size(), size()); main.label = 'Preview'; }
  const err = build(main, st.sel);
  const box = $('ed-flow');
  const key = `screen:${st.sel}:${st.device}`;
  clearTimeout(selTimer);
  selTimer = window.setTimeout(markSel, 450);  // after the screen's rise
  if (box.dataset.view === key) {  // an edit: same DOM (keeps focus in the drawer), new bar + drawer
    box.querySelector('.sv-bar')?.replaceWith(svBar(e, err));
    drawer();
    placeSlots();
    return;
  }
  box.dataset.view = key;
  const sv = h('div', `sv${flow.drawer ? ' open' : ''}`);
  const col = h('div', 'sv-main');
  const wrap = h('div', 'sv-shot');
  wrap.style.setProperty('--sz', `${size()}px`);
  const ov = h('div', 'sv-hits');
  hov = h('div', 'hit hov');
  sel = h('div', 'hit sel');
  hov.hidden = sel.hidden = true;
  slotLayer = h('div', 'sv-slots');
  ov.append(hov, sel, slotLayer);
  ov.dataset.testid = 'preview-hits'; ov.dataset.size = String(size());
  const dev = (ev: MouseEvent) => { const r = ov.getBoundingClientRect(); return [Math.round(((ev.clientX - r.left) * size()) / r.width), Math.round(((ev.clientY - r.top) * size()) / r.height)]; };
  ov.onpointermove = (ev) => { const it = hitAt(...(dev(ev) as [number, number])); place(hov!, it); ov.style.cursor = it ? 'pointer' : 'default'; if (it) hov!.dataset.label = itemName(it); };
  ov.onpointerleave = () => { hov!.hidden = true; };
  ov.onclick = (ev) => {  // the picked region again or empty space = deselect
    const it = hitAt(...(dev(ev) as [number, number]));
    const same = it && flow.drawer === 'item' && flow.item && flow.item.n === it.n && flow.item.prop === it.prop;
    if (it && !same) openDrawer('item', it); else closeDrawer();
  };
  col.onclick = (ev) => { if (ev.target === col && flow.drawer) closeDrawer(); };
  wrap.append(main.canvas, ov);
  col.append(svBar(e, err), wrap);
  if (!hits().length && engine?.m._fw_hits) col.append(h('p', 'ed-meta', 'Nothing on this screen is tied to a setting yet: use Screen settings.'));
  const dr = h('aside', 'sv-drawer');
  dr.id = 'ed-drawer';
  dr.setAttribute('aria-label', 'Settings');
  sv.append(col, dr);
  box.replaceChildren(sv);
  drawer();
  placeSlots();
  window.setTimeout(placeSlots, 450);  // after the screen's rise
}
function svBar(e: Entry, err: string) {
  const bar = h('div', 'sv-bar');
  const t = h('div', 'sv-title');
  t.append(h('h1', '', nameOf(e.id)), h('span', 'ed-meta', err ? `Preview: ${err}` : deckErr ? `Deck refused: ${deckErr}` : caption(e)));
  const onl = h('div', 'sv-on');
  onl.append(h('span', 'ed-meta', 'On Home'), sw(isOn(e), `Show ${nameOf(e.id)} on Home`, () => toggle(e.id)));
  const set = h('button', 'btn', 'Screen settings');
  set.type = 'button';
  set.setAttribute('aria-pressed', String(flow.drawer === 'screen'));
  set.onclick = () => (flow.drawer === 'screen' ? closeDrawer() : openDrawer('screen'));
  bar.append(t, onl, set);
  return bar;
}
function place(el: HTMLElement, it: Hit | null) {
  if (!it) { el.hidden = true; return; }
  const S = size(), x = Math.max(0, it.x), y = Math.max(0, it.y);
  el.hidden = false;
  Object.assign(el.style, { left: `${(x / S) * 100}%`, top: `${(y / S) * 100}%`, width: `${((Math.min(S, it.x + it.w) - x) / S) * 100}%`, height: `${((Math.min(S, it.y + it.h) - y) / S) * 100}%` });
}
/** the picked region's outline, from a fresh hit list (the render moved it) */
function markSel() {
  if (!sel) return;
  const it = flow.drawer === 'item' && flow.item ? hits().find((x) => x.n === flow.item!.n && x.prop === flow.item!.prop) ?? null : null;
  place(sel, it);
  $o('ed-drawer')?.parentElement?.classList.toggle('open', !!flow.drawer);
}
// ---- slots (CONTRACT C8a): where a screen takes components. Pink outlines on the screen; click = the slot's drawer
// (its items in order, add from what it accepts up to its max, remove, move, each item's settings). Edits change the
// screen's spec (deck.specs[kind]), installed in the engine as a sent screen, the same path a panel takes.
let slotLayer: HTMLElement | null = null;
function slotHits(): SlotHit[] {
  if (!main || !host.slots) return [];
  try { return (main as Stage & { slots(): SlotHit[] }).slots().filter((x) => x.in !== undefined); } catch { return []; }
}
function placeSlots() {
  if (!slotLayer) return;
  const S = size();
  slotLayer.replaceChildren(...slotHits().map((b) => {
    const el = h('button', `slot${b.empty || b.w < 2 ? ' empty' : ''}${flow.slot && flow.slot.n === b.n && flow.slot.in === b.in ? ' on' : ''}`);
    el.type = 'button';
    el.dataset.label = `${b.n}${b.empty ? ' (empty)' : ''}`;
    el.title = `Slot: ${b.n}`;
    const w = b.empty || b.w < 2 ? 0 : b.w;
    Object.assign(el.style, { left: `${(b.x / S) * 100}%`, top: `${(b.y / S) * 100}%`, width: w ? `${(w / S) * 100}%` : '', height: `${(b.h / S) * 100}%` });
    el.onclick = (ev) => { ev.stopPropagation(); openDrawer('slot', null, { n: b.n, in: b.in! }); placeSlots(); };
    return el;
  }));
}
const specCache = new Map<string, string>();
function specText(kind: string): string | null {
  if (deck.specs?.[kind]) return deck.specs[kind];
  if (!specCache.has(kind) && engine?.m._fw_spec) {
    try {
      const all = JSON.parse(engine.withStr('list', (pc) => engine!.withStr('', (pj) => engine!.str(engine!.m._fw_spec(pc, pj)))));
      for (const [k, v] of Object.entries(all.screens ?? {})) specCache.set(k, typeof v === 'string' ? v : JSON.stringify(v));
    } catch { /* an old build */ }
  }
  return specCache.get(kind) ?? null;
}
type SpecNode = { c: string; p?: Record<string, unknown>; in?: number | string; slot?: number | string; [k: string]: unknown };
/** a slot's accepts entry (core/spec.h): a component id, or {c, p?: {prop: allowed values}, editor?: false = not offered here} */
type Accept = string | { c: string; p?: Record<string, unknown[]>; editor?: boolean };
function slotDecl(owner: string, name: string) {
  const m = engine?.comps.find((x) => x.id === owner)?.meta as { slots?: unknown } | undefined;
  return Array.isArray(m?.slots) ? (m!.slots as { name: string; about?: string; accepts?: Accept[]; max?: number }[]).find((d) => d.name === name) : undefined;
}
function slotDrawer(dr: HTMLElement) {
  const e = entry(st.sel), pick = flow.slot;
  const kind = e?.kind ?? st.sel, txt = pick ? specText(kind) : null;
  if (!pick || !txt) { dr.replaceChildren(h('p', 'ed-meta', 'This screen can’t be edited here.')); return; }
  const spec = JSON.parse(txt) as { nodes: SpecNode[] };
  const owner = pick.in === 'header' ? 'header' : spec.nodes[pick.in]?.c ?? '';
  const decl = slotDecl(owner, pick.n);
  const mine = (x: SpecNode) => (pick.in === 'header' ? x.in === 'header' && (x.slot ?? 'end') === pick.n : x.in === pick.in);
  const items = spec.nodes.map((x, i) => [x, i] as [SpecNode, number]).filter(([x]) => mine(x));
  const commit = (f: (nodes: SpecNode[]) => void) => edit(() => {
    const sp = JSON.parse(specText(kind)!);
    f(sp.nodes);
    deck.specs = { ...(deck.specs ?? {}), [kind]: JSON.stringify(sp) };
  });
  const compName = (id: string) => engine?.comps.find((x) => x.id === id)?.name ?? id;
  const head = h('div', 'slot-head');
  head.append(h('h2', '', `Slot: ${pick.n}`), h('span', 'ed-meta', `${compName(owner)} · ${items.length}${decl?.max != null ? ` of ${decl.max}` : ''}`));
  const out: HTMLElement[] = [head];
  if (decl?.about) out.push(h('p', 'ed-meta', decl.about));
  items.forEach(([x, i], k) => {
    const row = h('div', 'slot-item'), top = h('div', 'slot-item-head');
    const btn = (t: string, label: string, on: boolean, f: () => void) => { const b = h('button', 'btn small', t); b.type = 'button'; b.title = label; b.setAttribute('aria-label', label); b.disabled = !on; b.onclick = f; return b; };
    const swap = (j: number) => commit((ns) => { const a = items[k][1], b2 = items[j][1]; [ns[a], ns[b2]] = [ns[b2], ns[a]]; });
    top.append(h('strong', '', compName(x.c)),
      btn('↑', 'Move earlier', k > 0, () => swap(k - 1)), btn('↓', 'Move later', k < items.length - 1, () => swap(k + 1)),
      btn('✕', 'Remove', true, () => commit((ns) => { ns.splice(i, 1); fixIn(ns, i, -1); })));
    row.append(top);
    for (const p of engine?.comps.find((c) => c.id === x.c)?.meta.props ?? []) {
      const lab = h('label', 'slot-prop'), cur = x.p?.[p.name] ?? p.default;
      lab.append(h('span', '', p.name));
      let inp: HTMLInputElement | HTMLSelectElement;
      if (p.type === 'select') { inp = h('select'); for (const o of p.options ?? []) { const op = h('option', '', o); op.value = o; inp.append(op); } inp.value = String(cur ?? ''); }
      else { inp = h('input'); inp.type = p.type === 'bool' ? 'checkbox' : p.type === 'range' ? 'number' : 'text'; if (p.type === 'bool') (inp as HTMLInputElement).checked = !!cur; else inp.value = String(cur ?? ''); }
      inp.onchange = () => commit((ns) => {
        const v = p.type === 'bool' ? (inp as HTMLInputElement).checked : p.type === 'range' ? Number(inp.value) : inp.value;
        ns[i].p = { ...(ns[i].p ?? {}), [p.name]: v };
      });
      lab.append(inp);
      row.append(lab);
    }
    out.push(row);
  });
  const full = decl?.max != null && items.length >= decl.max;
  const add = h('div', 'slot-add'), sel = h('select');
  const takes = (decl?.accepts ?? []).map((a) => (typeof a === 'string' ? { c: a } : a)).filter((a) => a.editor !== false && engine?.comps.some((c) => c.id === a.c));
  for (const a of takes) { const o = h('option', '', compName(a.c)); o.value = a.c; sel.append(o); }
  const ab = h('button', 'btn', full ? 'Slot is full' : 'Add');
  ab.type = 'button';
  ab.disabled = full || !sel.options.length;
  ab.onclick = () => commit((ns) => {
    const c = engine!.comps.find((x) => x.id === sel.value);
    const must = Object.fromEntries(Object.entries(takes.find((a) => a.c === sel.value)?.p ?? {}).map(([k, v]) => [k, v[0]]));  // what the slot requires (the first allowed value)
    const node: SpecNode = { c: sel.value, p: { ...defaults(c), ...must }, in: pick.in, slot: pick.in === 'header' ? pick.n : items.length };
    const at = items.length ? items[items.length - 1][1] + 1 : ns.length;
    fixIn(ns, at, 1);  // before the insert: refs at or past `at` move up one
    ns.splice(at, 0, node);
  });
  add.append(sel, ab);
  out.push(add);
  if (deck.specs?.[kind]) {
    const rs = h('button', 'btn ghost', 'Reset this screen');
    rs.type = 'button';
    rs.onclick = () => edit(() => { const { [kind]: _drop, ...rest } = deck.specs!; deck.specs = rest; });
    out.push(rs);
  }
  dr.replaceChildren(...out);
}
/** numeric "in" refs follow a node removed at i (by -1: refs past i move down; call after the splice) or one about
 *  to be inserted at i (by 1: refs at or past i move up; call before the splice) */
function fixIn(ns: SpecNode[], i: number, by: 1 | -1) {
  for (const x of ns) if (typeof x.in === 'number' && (by > 0 ? x.in >= i : x.in > i)) x.in += by;
}
function drawer() {
  const dr = $o('ed-drawer');
  if (!dr) return;
  dr.parentElement?.classList.toggle('open', !!flow.drawer);
  if (!flow.drawer) { dr.replaceChildren(); return; }
  if (dr.contains(document.activeElement) && document.activeElement instanceof HTMLInputElement && document.activeElement.type !== 'checkbox') return;  // typing: keep focus
  if (flow.drawer === 'slot') { slotDrawer(dr); return; }
  settings(dr, flow.drawer === 'item' ? flow.item?.params ?? [] : undefined);
  const t = $o('ed-flow')?.querySelector('.sv-bar .btn');
  t?.setAttribute('aria-pressed', String(flow.drawer === 'screen'));
}


// ---- a Home (screen model piece 6): the real Home large; click a card = its drawer (entity, tap, size, label, move,
// remove), click the header's right side = the header actions (the settings button or a page's shortcut, a status);
// nothing picked = the list of every card (the ones below the fold too) + Add card
type HCard = { t: string; e?: string; size: string; tap?: string; page?: string; label?: string };
type CardType = { domains: string; sizes: string; taps: string; detail: string };
type HBox = { card: number; x: number; y: number; w: number; h: number };  // card -1 = the header actions
let cardTypes: Record<string, CardType> | null = null;
function types(): Record<string, CardType> {
  if (!cardTypes && engine?.m._fw_catalogue) { try { cardTypes = JSON.parse(engine.str(engine.m._fw_catalogue())).cards ?? {}; } catch { cardTypes = {}; } }
  return cardTypes ?? {};
}
const home = () => deck.homes.find((x) => x.id === st.sel);
const cards = (): HCard[] => ((home() as any)?.cards ?? []) as HCard[];
const TYPE_NAME: Record<string, string> = { entity: 'Any entity', light: 'Light or switch', cover: 'Blind or curtain', climate: 'Climate', media: 'Media player', sensor: 'Sensor (read only)', page: 'Page' };
const SIZE_NAME: Record<string, string> = { s: 'Tile', m: 'Card', l: 'Wide' };
// a sensor's m is the State readout, two stacked in one card slot (sensors in both sizes)
const sizeName = (t: string, x: string) => (t === 'sensor' && x === 'm' ? 'Readout (half a card)' : t === 'sensor' && x === 's' ? 'Tile' : SIZE_NAME[x] ?? x);
const TAP_NAME: Record<string, string> = { toggle: 'Toggle', detail: 'Open its screen', page: 'Go to a page', none: 'Nothing (reports only)' };
function pageName(id?: string) { const p = [...deck.homes, ...deck.screens].find((x) => x.id === id) as any; return p?.name ?? (p ? nameOf(p.kind ?? p.id) : id ?? ''); }
function cardName(c?: HCard) {
  if (!c) return '';
  if (c.label) return c.label;
  if (c.t === 'page') return pageName(c.page);
  const dom = (c.e ?? '').split('.')[0];
  return src?.options[dom]?.find((o) => o.id === c.e)?.name ?? c.e ?? c.t;
}
function homeBoxes(s: Stage | null = main): HBox[] {
  if (!engine?.m._fw_comps || !s) return [];
  let comps: { ids: string[]; x: number; y: number; w: number; h: number }[] = [];
  try { comps = JSON.parse(engine.str(engine.m._fw_comps(s.id))); } catch { return []; }
  const out: HBox[] = [];
  let i = 0;
  for (const c of comps) {
    const id = c.ids[0];
    if (id === 'header' && !out.some((b) => b.card === -1)) out.push({ card: -1, x: c.x, y: c.y, w: c.w, h: c.h });  // Home's Header (its header actions)
    else if (id === 'entity_tile' || id === 'card' || id === 'readout') out.push({ card: i++, x: c.x, y: c.y, w: c.w, h: c.h });
  }
  return out;
}
function boxAt(x: number, y: number) {
  const top = DEVICES[st.device].size;
  return homeBoxes().find((b) => x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h && y < top) ?? null;
}
function openHome(id: string) {
  st.sel = id; flow.step = 'home'; flow.drawer = null; flow.card = -1;
  url(); flowRender();
}
function openHomeDrawer(card: number) {
  if (card >= 0) { cardModal(card); return; }
  if (flow.drawer === 'home') { closeHomeDrawer(); return; }  // the header again = deselect
  flow.drawer = 'home'; flow.card = -1; crumbs(); homeDrawer(); markHome();
}
function closeHomeDrawer() { flow.drawer = null; flow.card = -1; crumbs(); homeDrawer(); markHome(); }
let hsel: HTMLElement | null = null;
function placeBox(el: HTMLElement, b: HBox | null) {
  place(el, b ? { n: 0, c: '', params: [], ...b } : null);
}
function markHome() {
  if (!hsel) return;
  placeBox(hsel, flow.drawer ? homeBoxes().find((b) => (flow.drawer === 'home' ? b.card === -1 : b.card === flow.card)) ?? null : null);
  $o('ed-drawer')?.parentElement?.classList.toggle('open', true);
}
function homeView() {
  freeThumbs();
  const hm = home();
  if (!hm) { openPanel(); return; }
  if (!main) { main = engine!.stage(size(), size()); main.label = 'Preview'; }
  const err = build(main, { comp: 'home', props: { page: hm.id } });
  const box = $('ed-flow');
  const key = `home:${hm.id}:${st.device}`;
  window.setTimeout(() => { markHome(); homeDrawer(); }, 450);  // after the rise (the below-the-fold list needs the layout)
  if (box.dataset.view === key) {
    box.querySelector('.sv-bar')?.replaceWith(homeBar(err));
    homeDrawer();
    return;
  }
  box.dataset.view = key;
  const sv = h('div', 'sv open');
  const col = h('div', 'sv-main');
  const wrap = h('div', 'sv-shot');
  wrap.style.setProperty('--sz', `${size()}px`);
  const ov = h('div', 'sv-hits');
  const hv = h('div', 'hit hov');
  hsel = h('div', 'hit sel');
  hv.hidden = hsel.hidden = true;
  ov.append(hv, hsel);
  ov.dataset.testid = 'preview-hits'; ov.dataset.size = String(size());
  const dev = (ev: MouseEvent): [number, number] => { const r = ov.getBoundingClientRect(); return [Math.round(((ev.clientX - r.left) * size()) / r.width), Math.round(((ev.clientY - r.top) * size()) / r.height)]; };
  // hover = the card's edit outline, click = its modal; pick a card up (drag; touch: hold, then drag) = it follows the
  // pointer and the other cards make room live; drop = one Undo step, Esc = back. Keyboard: Alt + arrows in the modal.
  let lift: null | { from: number; order: number[]; pos: number; boxes: HBox[]; first: HBox[]; layer: HTMLElement; ghosts: HTMLElement[]; lay: Stage; fl: ReturnType<typeof floatCopy> } = null;
  let downAt: HBox | null = null;
  const S = size();
  const at = (el: HTMLElement, b?: HBox) => { if (!b) { el.hidden = true; return; } el.hidden = false; Object.assign(el.style, { left: `${(b.x / S) * 100}%`, top: `${(b.y / S) * 100}%`, width: `${(b.w / S) * 100}%`, height: `${(b.h / S) * 100}%` }); };
  const client = (b: HBox) => { const r = ov.getBoundingClientRect(), k = r.width / S; return { left: r.left + b.x * k, top: r.top + b.y * k }; };
  /** the Home's card boxes for a would-be order, from a scratch render (the panel isn't told) */
  const layout = (order: number[]) => {
    const hm = home() as any, real = hm.cards;
    hm.cards = order.map((i) => real[i]);
    applyDeck(false);
    build(lift!.lay, { comp: 'home', props: { page: hm.id } });
    hm.cards = real;
    applyDeck(false);
    return homeBoxes(lift!.lay).filter((b) => b.card >= 0);
  };
  const reflow = () => { const l = lift!; l.order.forEach((c, j) => at(l.ghosts[c], l.boxes[j])); };
  const finish = (to: HBox | undefined, after: () => void) => {
    const l = lift!;
    lift = null;
    ov.classList.remove('dragging');
    l.fl.land(to ? client(to) : client(l.first[l.from]), () => { l.layer.remove(); l.lay.free(); });
    after();
  };
  pickup(ov, {
    down: (ev) => { downAt = boxAt(...dev(ev)); return !!downAt && downAt.card >= 0 && cards().length > 1; },
    lift: (ev) => {
      const cv = main!.canvas, k = cv.width / S, first = homeBoxes().filter((b) => b.card >= 0), from = downAt!.card, b0 = first[from];
      if (!b0) return;
      // the still layer: the render with every card cut out (filled with the gap colour), each card a copy that glides
      const layer = h('div', 'ed-lay'), bg = document.createElement('canvas');
      bg.width = cv.width; bg.height = cv.height;
      const g = bg.getContext('2d')!;
      g.drawImage(cv, 0, 0);
      const right = first.find((b) => b !== b0 && b.x >= b0.x + b0.w && b.y < b0.y + b0.h && b.y + b.h > b0.y);
      const sx = right ? (b0.x + b0.w + right.x) / 2 : Math.max(1, b0.x - 2), sy = b0.y + b0.h / 2;
      const px = g.getImageData(Math.round(sx * k), Math.round(sy * k), 1, 1).data;
      g.fillStyle = `rgb(${px[0]} ${px[1]} ${px[2]})`;
      for (const b of first) g.fillRect(b.x * k, b.y * k, b.w * k, b.h * k);
      bg.className = 'bg';
      layer.append(bg);
      const ghosts = first.map((b) => {
        if (b.card === from) { const s = h('div', 'g slot'); layer.append(s); return s; }
        const c = document.createElement('canvas');
        c.className = 'g';
        c.width = Math.round(b.w * k); c.height = Math.round(b.h * k);
        c.getContext('2d')!.drawImage(cv, b.x * k, b.y * k, b.w * k, b.h * k, 0, 0, c.width, c.height);
        layer.append(c);
        return c as HTMLElement;
      });
      wrap.insertBefore(layer, ov);
      const r = ov.getBoundingClientRect(), z = r.width / S;
      const fl = floatCopy(cv, new DOMRect(r.left + b0.x * z, r.top + b0.y * z, b0.w * z, b0.h * z), ev, [b0.x * k, b0.y * k, b0.w * k, b0.h * k]);
      const lay = engine!.stage(S, S);
      lay.label = 'Drag layout';
      lift = { from, order: first.map((_, i) => i), pos: from, boxes: first, first, layer, ghosts, lay, fl };
      reflow();
      hv.hidden = hsel!.hidden = true;
      ov.classList.add('dragging');
    },
    move: (ev) => {
      if (!lift) return;
      lift.fl.move(ev);
      const [x, y] = dev(ev), j = lift.boxes.findIndex((b) => x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h);
      if (j < 0 || j === lift.pos) return;
      lift.order.splice(lift.pos, 1);
      lift.order.splice(j, 0, lift.from);
      lift.pos = j;
      lift.boxes = layout(lift.order);
      reflow();
    },
    drop: () => {
      if (!lift) return;
      const order = lift.order, moved = order.some((c, j) => c !== j);
      finish(lift.boxes[lift.pos], () => {
        if (moved) editCards((cs) => { const next = order.map((i) => cs[i]); cs.splice(0, cs.length, ...next); });
        markHome();
      });
    },
    cancel: () => {
      if (!lift) return;
      lift.order = lift.first.map((_, i) => i);
      lift.boxes = lift.first;
      reflow();
      finish(undefined, markHome);
    },
  });
  ov.onpointermove = (ev) => {
    if (lift) return;
    const b = boxAt(...dev(ev));
    placeBox(hv, b);
    hv.classList.toggle('edit', !!b && b.card >= 0);
    ov.style.cursor = b ? (b.card >= 0 ? 'grab' : 'pointer') : 'default';
    if (b) hv.dataset.label = b.card >= 0 ? `Edit ${cardName(cards()[b.card])}` : 'Header actions';
  };
  ov.onpointerleave = () => { if (!lift) hv.hidden = true; };
  ov.onclick = (ev) => {
    const b = boxAt(...dev(ev));
    if (b) openHomeDrawer(b.card); else closeHomeDrawer();
  };
  col.onclick = (ev) => { if (ev.target === col && flow.drawer === 'home') closeHomeDrawer(); };
  wrap.append(main.canvas, ov);
  col.append(homeBar(err), wrap, h('p', 'ed-meta', 'Click a card to edit it, drag it to move it (touch: hold, then drag). The top right corner = the header actions.'));
  const dr = h('aside', 'sv-drawer');
  dr.id = 'ed-drawer';
  dr.setAttribute('aria-label', 'Home settings');
  sv.append(col, dr);
  box.replaceChildren(sv);
  homeDrawer();
}
function homeBar(err: string) {
  const bar = h('div', 'sv-bar');
  const t = h('div', 'sv-title');
  const hm = home()!;
  t.append(h('h1', '', hm.name ?? 'Home'), h('span', 'ed-meta', err ? `Preview: ${err}` : deckErr ? `Deck refused: ${deckErr}` : `${cards().length} cards${hm.id === deck.start ? ' · start page' : ''}`));
  bar.append(t);
  return bar;
}
function editCards(f: (cs: HCard[]) => void) { edit(() => { const hm = home() as any; hm.cards = hm.cards ?? []; f(hm.cards); }); }
function homeDrawer() {
  const dr = $o('ed-drawer');
  if (!dr) return;
  dr.parentElement?.classList.toggle('open', true);
  if (dr.contains(document.activeElement) && document.activeElement instanceof HTMLInputElement) return;  // typing: keep focus
  const hm = home() as any;
  if (!hm) return;
  const out: HTMLElement[] = [];
  const head = h('div', 'set-head');
  const t = h('div');
  const close = () => { const x = h('button', 'btn ghost', 'Close'); x.type = 'button'; x.onclick = closeHomeDrawer; return x; };
  // the header actions + Add card (a card itself = in place: its modal) + the cards below the fold (no box to click)
  t.append(h('span', 'cap', 'Home'), h('h2', '', hm.name ?? 'Home'));
  head.append(t);
  if (flow.drawer) head.append(close());
  out.push(head);
  // this Home: its name, start page, remove (the cards that open it + a shortcut to it go with it; one Undo step)
  const me = h('section', 'set-sec');
  me.dataset.testid = 'home-settings';
  me.append(h('h3', '', 'This Home'));
  me.append(control('home-name', { type: 'text', label: 'Name' }, hm.name ?? '', (v, hist = true) => edit(() => { const x = home() as any; if (v) x.name = v; else delete x.name; }, hist), false, false));
  const ma = h('div', 'acts');
  const mb = (label: string, tid: string, on: () => void, dis: boolean) => { const b = h('button', 'btn', label); b.type = 'button'; b.dataset.testid = tid; b.disabled = dis; b.onclick = on; ma.append(b); };
  mb(hm.id === deck.start ? 'Start page' : 'Make start page', 'home-start', () => edit(() => { deck.start = hm.id; }), hm.id === deck.start);
  mb('Remove Home', 'home-remove', () => {
    const id = hm.id;
    edit(() => {
      deck.homes = deck.homes.filter((x) => x.id !== id);
      for (const x of deck.homes as any[]) {
        x.cards = (x.cards ?? []).filter((c: HCard) => !(c.t === 'page' && c.page === id));
        if (x.header_actions?.button === id) { delete x.header_actions.button; if (!Object.keys(x.header_actions).length) delete x.header_actions; }
      }
      if (deck.start === id) deck.start = deck.homes[0].id;
    });
    openPanel();
  }, deck.homes.length < 2);
  me.append(ma);
  out.push(me);
  const ha = h('section', 'set-sec');
  ha.append(h('h3', '', 'Header actions'));
  const cur = hm.header_actions ?? {};
  const setHa = (k: 'button' | 'status', v: string) => edit(() => {
    const x = (home() as any);
    const n = { ...(x.header_actions ?? {}), [k]: v };
    if ((n.button ?? 'settings') === 'settings') delete n.button;
    if ((n.status ?? 'none') === 'none') delete n.status;
    if (Object.keys(n).length) x.header_actions = n; else delete x.header_actions;
  });
  ha.append(control('ha-button', { type: 'select', label: 'Shortcut (before Settings)', options: ['settings', ...deck.screens.map((p) => p.id)] }, cur.button ?? 'settings', (v) => setHa('button', v), false, false));
  if ((cur.button ?? 'settings') !== 'settings') ha.append(h('p', 'hint', `A ${pageName(cur.button)} button before the Settings button (a long press on the time opens Settings too).`));
  ha.append(control('ha-status', { type: 'select', label: 'Beside the time', options: ['none', 'timer'] }, cur.status ?? 'none', (v) => setHa('status', v), false, false));
  out.push(ha);
  const list = h('section', 'set-sec');
  const S = size(), bx = homeBoxes(), low = cards().map((_, i) => i).filter((i) => !bx.some((b) => b.card === i && b.y < S));
  if (low.length) list.append(h('h3', '', 'Below the fold'));
  low.forEach((i) => {
    const c = cards()[i];
    const b = h('button', 'btn ghost', `${i + 1}. ${cardName(c)} · ${sizeName(c.t, c.size)}`);
    b.type = 'button';
    b.onclick = () => openHomeDrawer(i);
    list.append(b);
  });
  // Add card: a type, then it lands at the end with the first entity / page that fits (pick the entity in its drawer)
  const add = h('div', 'field');
  add.append(h('span', 'lbl', 'Add a card'));
  const sg = h('div', 'seg');
  sg.dataset.testid = 'add-card';
  seg(sg, Object.keys(types()).map((k) => [k, TYPE_NAME[k] ?? k]), '', (k) => {
    const ty = types()[k];
    const size = ty.sizes.split(',').includes('m') ? 'm' : ty.sizes.split(',')[0];
    const c: HCard = { t: k, size };
    if (k === 'page') c.page = deck.screens[0]?.id ?? deck.homes.find((x) => x.id !== hm.id)?.id;
    else {  // "*" = any domain (the Entity tile): the house's first light, else the first entity it has
      const ds = ty.domains === '*' ? ['light', ...Object.keys(src?.options ?? {})] : ty.domains.split(',');
      c.e = ds.flatMap((d) => src?.options[d] ?? [])[0]?.id ?? `${ds[0]}.none`;
    }
    editCards((cs) => { cs.push(c); });
    cardModal(cards().length - 1);
  });
  add.append(sg);
  list.append(add);
  out.push(list);
  dr.replaceChildren(...out);
}

// ---- a card's modal (edit in place): the card alone, like the workbench playground (a scratch Home
// of just this card in the engine's deck, never sent, the stage cropped to it) + its settings. Edits go through
// editCards -> edit -> changed() like everywhere else (one Undo step each); Esc / the backdrop / Close = deselect.
// a page id the engine accepts (core/spec.h: a-z 0-9 _, at most 12 characters). 'ed_card_preview' (15) got the whole
// deck refused, so the modal showed the start Home and the preview behind it the deck before the edit.
const CM_PAGE = '_ed_card';
let cm: { i: number; stage: Stage; crop: HTMLElement; head: HTMLElement; set: HTMLElement } | null = null;
let cmTimer = 0;
function moveCard(by: number) {
  if (!cm) return;
  const i = cm.i, j = i + by;
  if (j < 0 || j >= cards().length) return;
  cm.i = j; flow.card = j;
  editCards((cs) => { [cs[i], cs[j]] = [cs[j], cs[i]]; });
}
function cardFields(i: number): HTMLElement {
  const hm = home() as any, c = cards()[i], ty = types()[c.t];
  const sec = h('section', 'set-sec');
  const set = (f: (c: HCard) => void) => editCards((cs) => f(cs[i]));
  if (c.t === 'page') {
    sec.append(control('page', { type: 'select', label: 'Page', options: [...deck.homes, ...deck.screens].filter((p) => p.id !== hm.id).map((p) => p.id) }, c.page ?? '', (v) => set((x) => { x.page = v; }), false, false));
  } else if (ty) {
    const doms = ty.domains.split(',');
    const opts = doms.flatMap((d) => src?.options[d] ?? []);
    const f = h('div', 'field');
    const lbl = h('label', 'lbl', 'Entity');
    const inp = h('input', 'inp');
    inp.id = 'hc-entity';
    lbl.htmlFor = inp.id;
    inp.value = c.e ?? '';
    const dl = h('datalist');
    dl.id = 'hc-entity-list';
    for (const o of opts) { const x = h('option'); x.value = o.id; x.label = o.name; dl.append(x); }
    inp.setAttribute('list', dl.id);
    inp.onchange = () => { if (doms.includes(inp.value.split('.')[0])) set((x) => { x.e = inp.value; }); };
    f.append(lbl, dl, inp, h('span', 'hint', `${doms.join(' or ')} · ${opts.find((o) => o.id === c.e)?.name ?? ''}`));
    sec.append(f);
  }
  const taps = (ty?.taps ?? '').split(',').filter(Boolean);
  if (taps.length > 1) {
    const f = h('div', 'field');
    f.append(h('span', 'lbl', 'Tap'));
    const sg = h('div', 'seg');
    seg(sg, taps.map((x) => [x, TAP_NAME[x] ?? x]), c.tap ?? taps[0], (v) => set((x) => { x.tap = v; }));
    f.append(sg);
    if ((c.tap ?? taps[0]) === 'detail' && ty?.detail) f.append(h('span', 'hint', `Opens ${nameOf(ty.detail)} for ${cardName(c)}`));
    sec.append(f);
  } else if (taps.length === 1) sec.append(h('p', 'hint', `Tap: ${TAP_NAME[taps[0]] ?? taps[0]}${taps[0] === 'page' ? ` (${pageName(c.page)})` : ''}`));
  const sizes = (ty?.sizes ?? '').split(',').filter(Boolean);
  if (sizes.length > 1) {
    const f = h('div', 'field');
    f.append(h('span', 'lbl', 'Size'));
    const sg = h('div', 'seg');
    seg(sg, sizes.map((x) => [x, sizeName(c.t, x)]), c.size, (v) => set((x) => { x.size = v; }));
    f.append(sg);
    sec.append(f);
  }
  sec.append(control('label', { type: 'text', label: 'Label (empty = its own name)' }, c.label ?? '', (v, hist = true) => edit(() => { const x = (home() as any).cards[i]; if (v) x.label = v; else delete x.label; }, hist), false, false));
  const acts = h('div', 'acts');
  const btn = (label: string, on: () => void, dis = false) => { const b = h('button', 'btn', label); b.type = 'button'; b.disabled = dis; b.onclick = on; acts.append(b); };
  btn('Move earlier', () => moveCard(-1), i === 0);
  btn('Move later', () => moveCard(1), i === cards().length - 1);
  btn('Duplicate', () => { editCards((cs) => { cs.splice(i + 1, 0, { ...cs[i] }); }); });
  btn('Remove', () => { const dl = $('ed-dialog') as HTMLDialogElement; if (dl.open) dl.close(); editCards((cs) => { cs.splice(i, 1); }); });
  sec.append(acts);
  return sec;
}
function cardModal(i: number) {
  if (!engine || !cards()[i]) return;
  const d = $('ed-dialog') as HTMLDialogElement;
  if (d.open) d.close();
  flow.drawer = 'item'; flow.card = i;
  const box = h('div', 'dlg cm');
  box.dataset.testid = 'card-modal';
  const head = h('div', 'set-head cm-head');
  const pv = h('div', 'cm-prev');
  const crop = h('div', 'cm-crop');
  const stage = engine.stage(size(), size());
  stage.label = 'Card preview';
  crop.append(stage.canvas);
  pv.append(crop);
  const set = h('div', 'cm-set');
  box.append(head, pv, set);
  d.replaceChildren(box);
  d.classList.add('card');
  cm = { i, stage, crop, head, set };
  d.addEventListener('close', () => {
    stage.free();
    cm = null;
    d.classList.remove('card');
    applyDeck();
    if (flow.step === 'home' && flow.drawer === 'item') closeHomeDrawer();
  }, { once: true });
  applyDeck();
  crumbs(); markHome();
  d.showModal();
  cardModalRender();
}
function cardModalRender() {
  if (!cm || !engine) return;
  const c = cards()[cm.i];
  if (!c) { ($('ed-dialog') as HTMLDialogElement).close(); return; }
  const err = build(cm.stage, { comp: 'home', props: { page: CM_PAGE } });
  const t = h('div');
  t.append(h('span', 'cap', `${TYPE_NAME[c.t] ?? c.t} · card ${cm.i + 1} of ${cards().length}`), h('h2', '', cardName(c)));
  const x = h('button', 'btn ghost', 'Close');
  x.type = 'button';
  x.title = 'Close (Esc)';
  x.onclick = () => ($('ed-dialog') as HTMLDialogElement).close();
  cm.head.replaceChildren(t, x);
  if (err || deckErr) cm.head.append(h('span', 'ed-meta', err ? `Preview: ${err}` : `Deck refused: ${deckErr}`));
  clearTimeout(cmTimer);
  cmTimer = window.setTimeout(cropCard, 450);  // after the rise
  const a = document.activeElement;
  if (cm.set.contains(a) && a instanceof HTMLInputElement) return;  // typing: keep focus
  const fields = cardFields(cm.i);
  fields.append(h('p', 'hint', 'Alt + arrow keys move it too. Every change is one Undo step.'));
  cm.set.replaceChildren(fields);
}
/** crop the scratch Home's stage to its one card (device px, shown 1:1 up to the modal's width) */
function cropCard() {
  if (!cm) return;
  const b = homeBoxes(cm.stage).find((x) => x.card === 0), cv = cm.stage.canvas, S = size();
  if (!b) { cm.crop.classList.add('full'); Object.assign(cv.style, { width: '', left: '', top: '' }); cm.crop.style.width = ''; cm.crop.style.aspectRatio = ''; return; }
  cm.crop.classList.remove('full');
  Object.assign(cm.crop.style, { width: `min(${b.w}px, 100%)`, aspectRatio: `${b.w} / ${b.h}` });
  Object.assign(cv.style, { width: `${(S / b.w) * 100}%`, left: `${(-b.x / b.w) * 100}%`, top: `${(-b.y / b.h) * 100}%` });
  cm.crop.classList.add('ready');
}
