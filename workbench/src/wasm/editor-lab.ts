// Facet editor lab (/wasm/editor-lab; page src/pages/wasm/editor-lab.astro): prototype 1, SLOT-FIRST
// (after Stream Deck, Watch and iOS). The live WASM panel IS the editor: every slot is
// outlined on the glass (empty = faint "+"), tap one and the right panel shows only the components that fit it, each
// a live mini render; tap a component on the glass to change its settings. "Spec" (top right) shows the JSON + the
// renderer's errors; users never see it. Nothing is saved in the page; "Send to panel" sends your screens +
// the Home deck to a real panel (POST /api/wasm/spec, the agents' path: your screens have their own ids), Revert = factory.
import { DEVICES, Engine, fit, type Comp, type Stage } from './engine';
import { drawTaps, tapSummary } from './taps';
import { entityPicker as sharedPicker } from './entity-picker';

type Node = { c: string; p?: Record<string, unknown>; in?: number | 'header'; slot?: number | string; on?: Record<string, Record<string, unknown>> };
type SlotHit = { n: string; in?: number | 'header'; x: number; y: number; w: number; h: number; empty?: boolean; i?: number };
type Hit = { n: number; c: string; x: number; y: number; w: number; h: number };
type Sel = { kind: 'slot'; slot: SlotHit } | { kind: 'node'; n: number } | null;
// the Header's title is a real part (components/title.cpp): the editor writes it as an explicit
// node in the title slot and drops header.label (no label + no Title node = no title), so it removes and adds like any part

const D = DEVICES.waveshare;
// chrome / layout parts never offered as "fits here" (they are the frame, not the content)
const NOT_CONTENT = new Set(['header', 'back_button', 'bottom_sheet', 'toast', 'toast_layer', 'card', 'empty_state']);
let engine: Engine, stage: Stage, scale = 1, sel: Sel = null, err = '', showSpec = false;
// the screens you can edit: the lab's own starter, or any factory screen (renderer/specs/*.json, GET /api/wasm/spec);
// each comes with its own settings = its spec's typed params (Bins = Source + the
// bin calendar, Media = the visual), set in the right panel when nothing on the glass is picked; the feeds (C51) follow
type Param = { type?: string; label?: string; about?: string; default?: string; options?: string[]; domain?: string };
type Spec = { id: string; name?: string; params?: Record<string, Param | string>; nodes: Node[]; [k: string]: unknown };
let factory: Record<string, Spec> = {};
let options: Record<string, { id: string; name: string }[]> = {};  // this house's entities per domain (GET /api/wasm/sources)
const vals: Record<string, Record<string, string>> = {};  // per screen: the settings picked here (nothing saved yet)
// what is open: Home (the deck's Home page: its cards, to try each screen's Home card, C52), a
// CUSTOM screen (yours: every slot open, customise everything) or a BUILT-IN screen (made as it is: only its settings,
// no slots, no parts; a built-in that wants an open slot will say so in its spec, later)
type Cur = { k: 'home' } | { k: 'custom'; i: number } | { k: 'built'; id: string };
let cur: Cur = { k: 'home' };
type Card = { t: string; page?: string; e?: string; size: 's' | 'm' | 'l'; label?: string; tap?: string };
let cards: Card[] = [{ t: 'page', page: 'bins', size: 'm' }, { t: 'page', page: 'energy', size: 'm' }];
const customs: Spec[] = [];
const editable = () => cur.k !== 'home' && !spec.locked;  // a locked screen (spec "locked", C53: every factory one) = settings only
const starter = () => ({
  v: 1, id: 'lab', name: 'Living room', rev: 1, role: 'page', header: { back: true } as { back: boolean; label?: string }, layout: 'list',
  nodes: [
    { c: 'column', p: { outline: false } },  // the root: every custom screen is one Column, so its top + foot always take parts
    { c: 'hero_block', p: {}, in: 0, slot: 0 },
    { c: 'cells', p: { cells: 'c,c,c,c,c,c', sample: false }, in: 0, slot: 0 },
    { c: 'entity_tile', p: {}, in: 2, slot: 0 },
    { c: 'stat', p: {}, in: 2, slot: 1 },
    { c: 'title', p: { text: 'Living room' }, in: 'header', slot: 'title' },
  ] as Node[],
}) as Spec;
let spec: Spec = starter();
// a custom screen's root = a Column (node 0; never removed here): one without it gets its top-level parts wrapped into
// its top slot, so a screen can never end up with nowhere to add a part
const isRoot = (n: number) => n === 0 && spec.nodes[0]?.c === 'column' && spec.nodes[0].in === undefined;
function rooted(sp: Spec): Spec {
  if (sp.nodes[0]?.c === 'column' && sp.nodes[0].in === undefined) return sp;
  sp.nodes = [{ c: 'column', p: { outline: false } }, ...sp.nodes.map((n) => (typeof n.in === 'number' ? { ...n, in: n.in + 1 } : n.in === 'header' ? n : { ...n, in: 0, slot: 0 }))];
  return sp;
}
const light = () => matchMedia('(prefers-color-scheme: light)').matches;
const $ = (id: string) => document.getElementById(id)!;
function h<K extends keyof HTMLElementTagNameMap>(tag: K, cls = '', text = ''): HTMLElementTagNameMap[K] {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text) e.textContent = text;
  return e;
}
const comp = (id: string) => engine.comps.find((c) => c.id === id);
const nameOf = (id: string) => comp(id)?.name ?? id;
const defaults = (c?: Comp) => Object.fromEntries((c?.meta.props ?? []).map((p) => [p.name, p.default]));
// a new part starts as its first example (variants[0]), not bare defaults: a Hero block with words, a Stat with a value
const sample = (c?: Comp) => ({ ...defaults(c), ...(c?.meta.variants?.[0]?.props ?? {}) });
const migrated = (c: Comp) => (c.meta as { contract?: number }).contract === 2;

// ---- the screen: install the spec (the panel's own path, fw_spec "screen"), render, read slots + hits ----
function draw() {
  // only your own screens are installed (sent); a built-in renders as the panel has it (it refuses a sent copy)
  const js = JSON.stringify(cur.k === 'custom' ? spec : customs[0] ?? starter());
  const res = engine.withStr('screen', (pc) => engine.withStr(js, (pj) => engine.str(engine.m._fw_spec(pc, pj)))) || '';
  try { const r = JSON.parse(res); err = r.ok === false || r.error || r.errors?.length ? JSON.stringify(r.errors ?? r.error ?? r) : ''; } catch { err = res; }
  for (const c of customs) if (c !== spec) engine.withStr('screen', (pc) => engine.withStr(JSON.stringify(c), (pj) => engine.m._fw_spec(pc, pj)));
  engine.withStr('deck', (pc) => engine.withStr(JSON.stringify(deck()), (pj) => engine.m._fw_spec(pc, pj)));
  const e = cur.k === 'home' ? stage.render('home', { page: 'home', opening: false }, light(), 0)
    : stage.render('spec', { screen: spec.id, opening: false, ...(vals[spec.id] ?? {}) }, light(), 0);
  if (e) err = err ? `${err}\n${e}` : e;
  overlay();
  requestAnimationFrame(() => overlay());  // again once LVGL has laid the page out (a new tile's box is the row's until then)
  side();
  if (showSpec) specPane();
}
// the slots on the glass, each with its index in its owner (reading order) and whether a node fills it
function slots(): SlotHit[] {
  const all = stage.slots() as SlotHit[], out: SlotHit[] = [];
  const groups = new Map<string, SlotHit[]>();
  for (const s of all) { const k = `${s.in}|${s.n === 'half' ? 'cell' : s.n}`; groups.set(k, [...(groups.get(k) ?? []), s]); }
  for (const g of groups.values()) {
    g.sort((a, b) => a.y - b.y || a.x - b.x);
    g.forEach((s, i) => {
      const named = s.in === 'header', col = typeof s.in === 'number' && spec.nodes[s.in]?.c === 'column';
      const k = col ? (s.n === 'foot' ? 1 : 0) : i;  // a Column's slots: top 0, foot 1
      const filled = spec.nodes.some((n) => n.in === s.in && (named ? (n.slot ?? 'end') === s.n : (n.slot ?? 0) === k));
      out.push({ ...s, i: k, empty: s.empty || !filled });
    });
  }
  return out;
}
// each spec node's box on the glass: fw_comps lists every component instance in tree order (d = depth); the k-th node
// of a component = its k-th instance outside the Header (a prototype mapping: good while a screen has no repeats via "each")
function hits(): Hit[] {
  const boxes = stage.comps();
  const head = boxes.find((b) => b.ids[0] === 'header');
  const inHead = (b: { x: number; y: number; w: number; h: number }) => {  // by its centre: a touch area may outgrow the header
    const cx = b.x + b.w / 2, cy = b.y + b.h / 2;
    return !!head && cx >= head.x && cx <= head.x + head.w && cy >= head.y && cy <= head.y + head.h;
  };
  // header nodes count among the header's parts, the rest among the body's (the Header's own back / time are no nodes)
  const seen = new Map<string, number>(), out: Hit[] = [];
  const byComp = new Map<string, typeof boxes>();
  for (const b of boxes) if (b.d >= 2 && b.ids[0] !== 'header') { const key = `${inHead(b) ? 'h' : 'b'}|${b.ids[0]}`; byComp.set(key, [...(byComp.get(key) ?? []), b]); }
  spec.nodes.forEach((n, i) => {
    const key = `${n.in === 'header' ? 'h' : 'b'}|${n.c}`;
    const k = seen.get(key) ?? 0;
    seen.set(key, k + 1);
    const b = byComp.get(key)?.[k];
    if (b) out.push({ n: i, c: n.c, x: b.x, y: b.y, w: b.w, h: b.h });
  });
  return out;
}
function size() {
  const box = document.querySelector('.stagewrap') as HTMLElement;
  const narrow = matchMedia('(max-width: 900px)').matches;  // one column: the glass fits the width (its height is the content's)
  const f = narrow ? fit(stage.w, stage.h, Math.min(box.clientWidth - 32, 720), 10000) : fit(stage.w, stage.h, Math.min(box.clientWidth - 48, 720), Math.min(box.clientHeight - 48, 720));
  Object.assign(stage.canvas.style, { width: `${f.width}px`, height: `${f.height}px` });
  scale = f.width / stage.w;
  const ov = $('ov');
  Object.assign(ov.style, { width: `${f.width}px`, height: `${f.height}px` });
  overlay();
}
// the edit layer over the glass: one box per slot and per component (one per node: its biggest hit)
function overlay() {
  const ov = $('ov');
  const out: HTMLElement[] = [];
  const at = (b: HTMLElement, r: { x: number; y: number; w: number; h: number }) =>
    Object.assign(b.style, { left: `${r.x * scale}px`, top: `${r.y * scale}px`, width: `${r.w * scale}px`, height: `${r.h * scale}px` });
  if (cur.k === 'home') {  // Home: a box per card; click = the card's dialog (edit in place, the old editor's card modal)
    const hb = stage.comps().find((c) => c.ids[0] === 'header');
    if (hb) {  // the header: its end buttons (header actions per Home)
      const on = !!draft?.hd, el = h('button', 'ov-node ov-head' + (on ? ' on' : ''));
      el.title = 'Edit the header buttons';
      el.append(h('span', 'tag', 'Header' + (on && dirty() ? ' · unsaved' : '')));
      at(el, hb);
      el.onclick = () => selectHead();
      out.push(el);
    }
    for (const b of homeBoxes(stage)) {
      const on = !draft?.hd && draft?.i === b.card;
      const el = h('button', 'ov-node ov-card' + (on ? ' on' : ''));
      el.dataset.card = String(b.card);
      el.title = `Edit ${cardName(view()[b.card])}`;
      el.append(h('span', 'tag', cardName(view()[b.card]) + (on && dirty() ? ' · unsaved' : '')));
      at(el, b);
      el.onclick = () => selectCard(b.card);
      out.push(el);
    }
    ov.replaceChildren(...out);
    return;
  }
  if (!editable()) { ov.replaceChildren(); return; }  // built-in screens: tap through to the panel itself
  const best = new Map<number, Hit>();
  for (const x of hits()) { const b = best.get(x.n); if (!b || x.w * x.h > b.w * b.h) best.set(x.n, x); }
  for (const x of [...best.values()].sort((a, b) => b.w * b.h - a.w * a.h)) {  // big first: the inner parts on top
    const b = h('button', 'ov-node' + (sel?.kind === 'node' && sel.n === x.n ? ' on' : ''));
    b.title = nameOf(spec.nodes[x.n]?.c ?? x.c);
    b.append(h('span', 'tag', b.title));
    at(b, x);
    b.onclick = () => { sel = { kind: 'node', n: x.n }; overlay(); side(); };
    out.push(b);
  }
  // slots never touch: each one inset by GAP; a collapsed slot (w 0, the Header's end) gets a square just before where
  // it opens, and a slot of the same owner that would run under it ends a gap earlier
  const GAP = 6 / scale;
  // an empty layout slot that takes no height (a Column's top / foot) gets a row's worth: down from the top, up from a foot
  const ROW = 64;
  const sl = slots().map((s) => (!s.w ? { ...s, x: s.x - s.h, w: s.h } : s.h >= ROW / 2 ? { ...s } : { ...s, h: ROW, y: s.n === 'foot' ? s.y + s.h - ROW : s.y }));
  for (const a of sl) for (const b of sl) if (a !== b && a.in === b.in && a.x < b.x && a.x + a.w > b.x - GAP) a.w = Math.max(0, b.x - GAP - a.x);
  for (const s of sl) {
    if (!s.empty) continue;
    const r = { x: s.x + GAP / 2, y: s.y + GAP / 2, w: s.w - GAP, h: s.h - GAP };
    const b = h('button', 'ov-slot' + (sel?.kind === 'slot' && same(sel.slot, s) ? ' on' : ''));  // the plus is drawn (CSS), a text + sits off centre
    b.title = `Add to ${s.n}`;
    at(b, r);
    b.onclick = () => { sel = { kind: 'slot', slot: s }; overlay(); side(); };
    out.push(b);
  }
  ov.replaceChildren(...out);
  ov.classList.toggle('editing', true);
}
const same = (a: SlotHit, b: SlotHit) => a.n === b.n && a.in === b.in && a.x === b.x && a.y === b.y;

// ---- what fits a slot: the owner's slot declaration (CONTRACT C8a) or, for "any", every contract content part ----
function fits(s: SlotHit): Comp[] {
  const owner = typeof s.in === 'number' ? spec.nodes[s.in]?.c : s.in === 'header' ? 'header' : null;
  const decls = owner ? (comp(owner)?.meta as { slots?: unknown }).slots : null;
  const decl = Array.isArray(decls) ? (decls as { name: string; accepts?: Accept[] | 'any' }[]).find((d) => d.name === s.n.replace(/\s*\d+$/, '')) : null;
  // C8a entries: an id, or {c, p: pinned props, editor: false = specs only, never offered here}
  const acc = decl && Array.isArray(decl.accepts) ? new Set(decl.accepts.filter((a) => typeof a === 'string' || a.editor !== false).map(accId)) : null;
  const list = engine.comps.filter((c) => c.section === 'components' && migrated(c) && !NOT_CONTENT.has(c.id) && (acc ? acc.has(c.id) : true));
  return list;
}
type Accept = string | { c: string; p?: Record<string, string[]>; editor?: boolean };
const accId = (a: Accept) => (typeof a === 'string' ? a : a.c);
/** the props a slot pins for a component (C8a {c, p}): prop -> its allowed values; owner = the slot's component */
function pinsFor(owner: string | null, slot: string, id: string): Record<string, string[]> {
  const decls = owner ? (comp(owner)?.meta as { slots?: unknown }).slots : null;
  if (!Array.isArray(decls)) return {};
  const d = (decls as { name: string; count?: number; accepts?: Accept[] }[]).find((x) => x.name === slot.replace(/\s*\d+$/, '')) ?? (decls as { count?: number; accepts?: Accept[] }[]).find((x) => x.count);
  const a = Array.isArray(d?.accepts) ? d!.accepts!.find((x) => typeof x !== 'string' && x.c === id) : undefined;
  return (typeof a === 'object' && a.p) || {};
}
const nodePins = (node: Node) => pinsFor(node.in === 'header' ? 'header' : typeof node.in === 'number' ? spec.nodes[node.in]?.c ?? null : null, node.in === 'header' ? String(node.slot ?? 'end') : String(node.slot ?? ''), node.c);
function insert(s: SlotHit, id: string) {
  const pins = pinsFor(typeof s.in === 'number' ? spec.nodes[s.in]?.c : s.in === 'header' ? 'header' : null, s.n, id);
  const base = id === 'title' ? { text: spec.name } : sample(comp(id));
  for (const [k, vs] of Object.entries(pins)) if (!vs.includes(String(base[k] ?? ''))) base[k] = vs[0];  // a pinned variant (the Header's end: square)
  spec.nodes.push({ c: id, p: base, ...(s.in !== undefined ? { in: s.in, slot: s.in === 'header' ? s.n : s.i ?? 0 } : {}) });
  sel = { kind: 'node', n: spec.nodes.length - 1 };
  draw();
}
function remove(n: number) {
  if (isRoot(n)) return;  // the root Column stays
  const drop = new Set([n]);
  for (let i = n + 1; i < spec.nodes.length; i++) if (typeof spec.nodes[i].in === 'number' && drop.has(spec.nodes[i].in as number)) drop.add(i);
  const map = new Map<number, number>();
  spec.nodes = spec.nodes.filter((_, i) => !drop.has(i) && (map.set(i, map.size), true));
  for (const x of spec.nodes) if (typeof x.in === 'number') x.in = map.get(x.in);
  sel = null;
  draw();
}

// ---- the right panel ----
const minis = new Map<string, Stage>();
function mini(c: Comp): HTMLElement {
  const card = h('button', 'mini');
  let s = minis.get(c.id);
  if (!s) {
    const [fw, fh] = c.meta.frame ?? [320, 0];
    s = engine.stage(fw || 320, fh || 160);
    s.render(c.id, { ...sample(c), opening: false }, light(), 0, !fh);
    minis.set(c.id, s);
  }
  const k = Math.min(1, 132 / s.w, 96 / s.h);
  Object.assign(s.canvas.style, { width: `${s.w * k}px`, height: `${s.h * k}px` });
  const pic = h('div', 'pic');
  pic.append(s.canvas);
  card.append(pic, h('span', 'nm', c.name));
  card.title = (c.meta as { about?: string }).about ?? c.name;
  return card;
}
// ---- the right panel: "Fits here" (AA01 HA card picker: search, Suggested first, then groups of live previews) or the
// selected part's settings; Done (top) closes either and leaves the panel as it is (nothing to save: every change is live)
const GROUPS: [string, string[]][] = [
  ['Controls', ['button', 'icon_button', 'toggle', 'checkbox', 'radio', 'segmented', 'slider', 'dial']],
  ['Read-outs', ['entity_tile', 'stat', 'readout', 'hero_block', 'status_chip', 'count_badge', 'time']],
  ['Charts', ['bar_chart', 'sparkline']],
  ['Lists', ['list_row', 'more_row', 'picker_row', 'section_header']],
  ['Layouts', ['cells', 'grid', 'split', 'column', 'group']],
];
// what a slot most likely wants: a Header end = small controls; a cell = tiles and read-outs; the page = blocks
function suggested(s: SlotHit): string[] {
  if (s.in === 'header') return s.n === 'end' ? ['icon_button', 'status_chip', 'segmented'] : ['title', 'segmented'];
  if (s.n === 'cell' || s.n === 'half') return ['entity_tile', 'stat', 'toggle', 'button', 'dial'];
  if (s.n === 'foot') return ['button', 'group', 'icon_button'];  // a Column's foot: the action buttons
  return ['hero_block', 'cells', 'list_row', 'section_header'];
}
// a setting's words (props are named for code): known names first, else the name in sentence case
const LABELS: Record<string, string> = { style: 'Style', icon: 'Icon', state: 'State', kind: 'Kind', label: 'Label', width: 'Width', size: 'Size',
  min_w: 'Narrowest cell', 'min-w': 'Narrowest cell', 'min-h': 'Shortest cell', hold_label: 'Words while held', hold_time: 'Hold time (ms)', value: 'Sends',
  act: 'When tapped', press: 'Pressed look', n: 'Number', line: 'Detail line', label_type: 'Label style', meta: 'Text at the end',
  action: 'Link words', min: 'Lowest', max: 'Highest', cols: 'Columns', caption: 'Caption', dimmable: 'Can be dimmed', pulse: 'Pulsing dot',
  divider: 'Line underneath', over: 'Line above', sub: 'Line below', trailing: 'At the end', badge: 'Count badge', unit: 'Unit', count: 'Count up' };
const nice = (n: string) => LABELS[n] ?? (n.replace(/[_-]+/g, ' ').replace(/^./, (c) => c.toUpperCase()));
const optWord = (o: string) => o.replace(/[_-]+/g, ' ').replace(/^./, (c) => c.toUpperCase());

function top(title: string, sub?: string): HTMLElement {
  const bar = h('div', 'sidetop');
  const t = h('div', 'ttl');
  t.append(h('h2', '', title));
  if (sub) t.append(h('p', 'muted', sub));
  const done = h('button', 'done', 'Done');
  done.onclick = () => { sel = null; overlay(); side(); };
  bar.append(t, done);
  return bar;
}
function side() {
  const r = $('side');
  document.body.classList.toggle('card-sel', cur.k === 'home' && !!draft);
  if (cur.k === 'home') {
    if (!draft) { r.replaceChildren(...homeSide()); return; }
    if (draft.hd) { r.replaceChildren(...headSide()); if (epOpen) (r.querySelector('.ep input') as HTMLInputElement | null)?.focus(); return; }
    const a = document.activeElement;  // typing the label: keep the field (and focus), refresh only the status
    if (r.contains(a) && a instanceof HTMLInputElement && a.type !== 'search' && r.querySelector('.cs-state')) { r.querySelector('.cs-state')!.replaceWith(cardState()); r.querySelector('.cs-save')!.toggleAttribute('disabled', !dirty()); return; }
    r.replaceChildren(...cardSide());
    if (epOpen) (r.querySelector('.ep input') as HTMLInputElement | null)?.focus();
    return;
  }
  if (!sel) { r.replaceChildren(...screenSettings()); return; }
  if (sel.kind === 'slot') {
    const s = sel.slot, list = fits(s), ids = new Set(list.map((c) => c.id));
    const q = h('input', 'search');
    q.placeholder = `Search ${list.length} parts`;
    const body = h('div', 'groups');
    const card = (c: Comp) => { const m = mini(c); m.onclick = () => insert(s, c.id); return m; };
    const fill = () => {
      const t = q.value.trim().toLowerCase();
      const out: HTMLElement[] = [];
      const section = (name: string, cs: Comp[]) => {
        if (!cs.length) return;
        const g = h('section', 'group');
        const hd = h('h3', '', name);
        hd.append(h('span', 'n', String(cs.length)));
        const grid = h('div', 'minis');
        grid.append(...cs.map(card));
        g.append(hd, grid);
        out.push(g);
      };
      if (t) section('Results', list.filter((c) => c.name.toLowerCase().includes(t) || ((c.meta as { about?: string }).about ?? '').toLowerCase().includes(t)));
      else {
        section('Suggested', suggested(s).filter((id) => ids.has(id)).map((id) => comp(id)!));
        const seen = new Set<string>();
        for (const [name, gids] of GROUPS) { section(name, gids.filter((id) => ids.has(id)).map((id) => comp(id)!)); gids.forEach((id) => seen.add(id)); }
        section('Other', list.filter((c) => !seen.has(c.id)));
      }
      body.replaceChildren(...(out.length ? out : [h('p', 'muted', 'Nothing matches.')]));
    };
    q.oninput = fill;
    fill();
    const where = typeof s.in === 'number' ? `${optWord(s.n)} in ${nameOf(spec.nodes[s.in].c)}` : s.in === 'header' ? `Header ${s.n}` : optWord(s.n);
    r.replaceChildren(top('Add a part', where), q, body);
    q.focus();
    return;
  }
  const n = sel.n, node = spec.nodes[n], c = comp(node.c);
  const out: HTMLElement[] = [top(nameOf(node.c), (c?.meta as { about?: string })?.about)];
  const pins = nodePins(node);
  out.push(...tapSettings(node));  // what a tap does (a button you place says what it does)
  for (const p0 of c?.meta.props ?? []) {
    // debug props (meta "debug": a look state such as pressed / unavailable) and previews-only ones are the workbench's
    if (p0.name === 'opening' || p0.name === 'sample' || p0.name === 'outline' || (p0 as { debug?: boolean }).debug || /^for previews only/i.test(p0.about ?? '')) continue;
    const p = pins[p0.name] ? { ...p0, options: (p0.options ?? []).filter((o) => pins[p0.name].includes(o)) } : p0;  // a pinned variant: only its values
    const row = h('div', 'prop');
    row.append(h('span', 'pl', nice(p.name)));
    const v = node.p?.[p.name] ?? p.default;
    const set = (x: unknown) => { node.p = { ...(node.p ?? {}), [p.name]: x }; draw(); };
    if (p.type === 'select' && (p.options?.length ?? 0) <= 4) {  // a few choices: chips, all visible (like the Segmented)
      const chips = h('div', 'chips');
      for (const o of p.options ?? []) {
        const b = h('button', 'chip' + (String(v) === o ? ' on' : ''), optWord(o));
        b.onclick = () => set(o);
        chips.append(b);
      }
      row.append(chips);
    } else if (p.type === 'select') {
      const inp = h('select');
      for (const o of p.options ?? []) { const op = h('option', '', optWord(o)); op.value = o; inp.append(op); }
      inp.value = String(v ?? '');
      inp.onchange = () => set(inp.value);
      row.append(inp);
    } else if (p.type === 'bool') {
      const b = h('button', 'switch' + (v ? ' on' : ''), v ? 'On' : 'Off');
      b.onclick = () => set(!v);
      row.append(b);
    } else {
      const inp = h('input');
      if (p.type === 'range') { inp.type = 'number'; inp.min = String(p.min ?? ''); inp.max = String(p.max ?? ''); inp.step = String(p.step ?? 1); }
      inp.value = typeof v === 'object' ? JSON.stringify(v) : String(v ?? '');
      inp.onchange = () => set(p.type === 'range' ? Number(inp.value) : inp.value);
      row.append(inp);
    }
    if (p.about) row.append(h('p', 'help', p.about));
    out.push(row);
  }
  if (!isRoot(n)) {  // the screen's root Column stays (its slots are where parts go)
    const rm = h('button', 'btn danger', 'Remove');
    rm.onclick = () => remove(n);
    out.push(rm);
  }
  r.replaceChildren(...out);
}

// ---- the screen's own settings: a control per typed param (hidden ones never; "For previews only" ones only while
// the Source is sample, or when the screen has no Source) ----
const typed = (p: Param | string): Param => (typeof p === 'string' ? { type: 'text', default: p } : p);
function screenSettings(): HTMLElement[] {
  const out: HTMLElement[] = [h('h2', '', spec.name ?? spec.id)];
  const v = (vals[spec.id] ??= {});
  const ps = Object.entries(spec.params ?? {}).map(([k, p]) => [k, typed(p)] as const);
  const val = (k: string, p: Param) => v[k] ?? p.default ?? '';
  const src = ps.find(([k]) => k === 'source');
  const show = ps.filter(([, p]) => p.type !== 'hidden' && !(src && val('source', src[1]) !== 'sample' && /^for previews only/i.test(p.about ?? '')));
  if (!show.length) out.push(h('p', 'muted', 'This screen has no settings.'));
  for (const [k, p] of show) {
    const row = h('div', 'prop');
    row.append(h('span', 'pl', p.label ?? nice(k)));
    const cur = val(k, p);
    const set = (x: string) => { v[k] = x; draw(); };
    const opts = options[p.domain ?? ''] ?? [];
    if (p.type === 'select' && (p.options?.length ?? 0) <= 4) {
      const chips = h('div', 'chips');
      for (const o of p.options ?? []) { const b = h('button', 'chip' + (cur === o ? ' on' : ''), optWord(o)); b.onclick = () => set(o); chips.append(b); }
      row.append(chips);
    } else if (p.type === 'select' || (p.type === 'entity' && opts.length <= 80)) {
      const inp = h('select');
      const list: [string, string][] = p.type === 'select' ? (p.options ?? []).map((o) => [o, optWord(o)]) : [['', 'None'], ...opts.map((o) => [o.id, o.name] as [string, string])];
      if (cur && !list.some(([x]) => x === cur)) list.push([cur, cur]);
      for (const [x, t] of list) { const op = h('option', '', t); op.value = x; inp.append(op); }
      inp.value = cur;
      inp.onchange = () => set(inp.value);
      row.append(inp);
    } else if (p.type === 'toggle') {
      const b = h('button', 'switch' + (cur === 'true' ? ' on' : ''), cur === 'true' ? 'On' : 'Off');
      b.onclick = () => set(cur === 'true' ? 'false' : 'true');
      row.append(b);
    } else if (p.type === 'entities' && opts.length && opts.length <= 80) {
      const on = new Set(cur.split(',').filter(Boolean));
      const chips = h('div', 'chips');
      for (const o of opts) {
        const b = h('button', 'chip' + (on.has(o.id) ? ' on' : ''), o.name);
        b.onclick = () => { if (on.has(o.id)) on.delete(o.id); else on.add(o.id); set(opts.map((x) => x.id).filter((x) => on.has(x)).join(',')); };
        chips.append(b);
      }
      row.append(chips);
    } else {  // text, or a big domain (sensors): type to search
      const inp = h('input');
      inp.value = cur;
      if (p.type === 'entity' || p.type === 'entities') {
        const dl = h('datalist');
        dl.id = `dl-${k}`;
        for (const o of opts) { const x = h('option'); x.value = o.id; x.label = o.name; dl.append(x); }
        inp.setAttribute('list', dl.id);
        row.append(dl);
      }
      inp.onchange = () => set(inp.value.trim());
      row.append(inp);
    }
    if (p.about) row.append(h('p', 'help', p.about));
    out.push(row);
  }
  if (editable()) {
    const nm = h('div', 'prop');
    nm.append(h('span', 'pl', 'Name'));
    const inp = h('input');
    inp.value = spec.name ?? '';
    inp.onchange = () => { spec.name = inp.value.trim() || 'My screen'; picker(); draw(); };
    nm.append(inp);
    out.splice(1, 0, nm);
    out.push(h('p', 'muted', 'Tap a + on the panel to add something there, or tap a part of the screen to change it.'));
  } else out.push(h('p', 'muted', 'A built-in screen: made as it is, so only its settings change here. Make your own screen to lay one out yourself.'));
  return out;
}
// ---- Home: the deck the panel would get (a Home page with your cards + a page per screen they open, each with the
// settings picked here, so a screen's card shows what the screen shows) ----
const pageIds = () => [...new Set([...view().filter((c) => c.t === 'page' && c.page).map((c) => c.page!), ...headView().filter((b) => b.t === 'page' && b.page).map((b) => b.page!)])];
function deck() {
  const hb = headView();
  const pages = [{ id: 'home', kind: 'home', name: 'Home', cards: layOrder ?? view(), ...(hb.length ? { header_actions: { buttons: hb } } : {}) }];  // the preview shows your unsaved card edits (a drag: its would-be order)
  for (const id of pageIds()) pages.push({ id, kind: id, ...(Object.keys(vals[id] ?? {}).length ? { params: vals[id] } : {}) } as never);
  if (cur.k === 'built' && !pageIds().includes(cur.id)) pages.push({ id: cur.id, kind: cur.id, ...(Object.keys(vals[cur.id] ?? {}).length ? { params: vals[cur.id] } : {}) } as never);
  return { v: 2, rev: 1, start: 'home', pages: pages.slice(0, 16) };
}
const screenName = (id: string) => customs.find((c) => c.id === id)?.name ?? factory[id]?.name ?? id;
function homeSide(): HTMLElement[] {
  const out: HTMLElement[] = [h('h2', '', 'Home'), h('p', 'muted', 'Your cards. Click a card on the panel (or its name here) to change it.')];
  if (Date.now() - savedAt < 5000) { const n = h('p', 'note ok', 'Saved. Send to panel puts your Home on a panel.'); n.dataset.testid = 'saved'; out.push(n); }
  cards.forEach((c, i) => {
    const row = h('div', 'prop');
    const hd = h('div', 'cardrow');
    const nm = h('button', 'pl linkish', cardName(c));
    nm.title = 'Edit this card';
    nm.onclick = () => selectCard(i);
    hd.append(nm);
    const chips = h('div', 'chips');
    for (const z of ['s', 'm', 'l'] as const) { const b = h('button', 'chip' + (c.size === z ? ' on' : ''), { s: 'Tile', m: 'Card', l: 'Wide' }[z]); b.onclick = () => { c.size = z; draw(); }; chips.append(b); }
    const up = h('button', 'chip', '↑'); up.title = 'Move up'; up.disabled = !i;
    up.onclick = () => { [cards[i - 1], cards[i]] = [cards[i], cards[i - 1]]; draw(); };
    const x = h('button', 'chip', '✕'); x.title = 'Remove';
    x.onclick = () => { cards.splice(i, 1); draw(); };
    chips.append(up, x);
    row.append(hd, chips);
    out.push(row);
  });
  // add a card: a screen's own Home card (C52) first, then any screen as a plain page card
  const add = h('select');
  const own = Object.values(factory).filter((x) => x.card), rest = [...customs, ...Object.values(factory).filter((x) => !x.card && x.role !== 'deck' && Array.isArray(x.nodes))];
  const grp = (label: string, xs: Spec[]) => { const g = h('optgroup'); g.label = label; for (const x of xs) { const o = h('option', '', x.name ?? x.id); o.value = x.id; g.append(o); } return g; };
  const first = h('option', '', 'Add a card…'); first.value = '';
  add.append(first, grp('With their own card', own), grp('Other screens', rest.sort((a, b) => (a.name ?? a.id).localeCompare(b.name ?? b.id))));
  // a card for a thing (the old editor's Add card by type): the type's first entity, size and tap, then its dialog
  const dev = h('optgroup'); dev.label = 'A device or sensor';
  for (const t of Object.keys(types).filter((t) => t !== 'page')) { const o = h('option', '', TYPE_NAME[t] ?? t); o.value = `type:${t}`; dev.append(o); }
  if (dev.children.length) add.append(dev);
  add.onchange = () => {
    if (!add.value) return;
    if (add.value.startsWith('type:')) {
      const t = add.value.slice(5), ty = types[t], doms = ty.domains === '*' ? ['light', ...Object.keys(options)] : ty.domains.split(',');
      const e = doms.flatMap((d) => options[d] ?? [])[0]?.id ?? `${doms[0]}.none`;
      const c: Card = { t, e, size: ty.sizes.split(',')[0] as Card['size'] };
      const ts = tapsOf(c);
      if (t === 'entity' || ts.length > 1) c.tap = ts[0];
      addCard(c);
      return;
    }
    addCard({ t: 'page', page: add.value, size: 'm' });  // staged like any edit: Save keeps it, Cancel drops it
  };
  const ar = h('div', 'prop'); ar.append(add); out.push(ar);
  return out;
}
// the screen picker (left): Home, your own screens (+ New screen), then the built-in screens by name
function picker() {
  const s = $('screen') as HTMLSelectElement;
  const opt = (v: string, t: string) => { const o = h('option', '', t); o.value = v; return o; };
  const grp = (label: string, xs: HTMLOptionElement[]) => { const g = h('optgroup'); g.label = label; g.append(...xs); return g; };
  const built = Object.values(factory).filter((x) => x.role !== 'deck' && Array.isArray(x.nodes)).sort((a, b) => (a.name ?? a.id).localeCompare(b.name ?? b.id));
  s.replaceChildren(opt('home', 'Home'),
    grp('Your screens', [...customs.map((c, i) => opt(`custom:${i}`, c.name ?? c.id)), opt('new', '+ New screen')]),
    grp('Built-in screens', built.map((x) => opt(`built:${x.id}`, x.name ?? x.id))));
  s.value = cur.k === 'home' ? 'home' : cur.k === 'custom' ? `custom:${cur.i}` : `built:${cur.id}`;
  s.onchange = () => open(s.value);
}
function open(v: string) {
  if (dirty() && !confirm('Discard your unsaved card changes?')) { picker(); return; }
  draft = null;
  if (v === 'new') {
    const n = customs.length + 1, sp = starter();
    sp.id = `my${n}`; sp.name = `My screen ${n}`;
    for (const x of sp.nodes) { if (!Object.keys(x.p ?? {}).length) x.p = sample(comp(x.c)); if (x.c === 'title') x.p = { text: sp.name }; }
    customs.push(sp);
    v = `custom:${customs.length - 1}`;
  }
  if (v === 'home') cur = { k: 'home' };
  else if (v.startsWith('custom:')) { cur = { k: 'custom', i: Number(v.slice(7)) }; spec = customs[cur.i] = rooted(customs[cur.i]); }
  else { const id = v.slice(6); cur = { k: 'built', id }; spec = structuredClone(factory[id]); }
  sel = null;
  picker();
  draw();
}

// ---- Send to panel: your screens first (a deck page may only open a screen the panel has), then the deck (Home + a
// page per screen its cards open, with the settings picked here). The panel validates + builds each before it swaps.
async function post(body: Record<string, unknown>) {
  const r = await fetch('/api/wasm/spec', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
  return r.json().catch(() => ({ error: `HTTP ${r.status}` }));
}
const why = (j: any) => j?.error === 'needs_firmware' ? `needs firmware: ${j.missing.join(', ')}`
  : j?.error ?? (j?.dry?.errors ?? j?.panel?.errors ?? []).map((e: { code: string; msg?: string; path?: string }) => `${e.code}${e.path ? ` ${e.path}` : ''}: ${e.msg ?? ''}`).join('; ') ?? 'refused';
function say(t: string, bad = false) { const m = $('sendmsg'); m.textContent = t; m.title = t; m.classList.toggle('bad', bad); }
async function send() {
  const target = ($('target') as HTMLSelectElement).value, btn = $('sendbtn') as HTMLButtonElement;
  btn.disabled = true;
  try {
    const used = new Set(pageIds());
    for (const c of customs.filter((x) => used.has(x.id))) {  // only the screens Home opens (the rest stay here)
      say(`Sending ${c.name}…`);
      const j = await post({ target, kind: 'screen', spec: c });
      if (j?.stage !== 'sent' || !j.panel?.ok) { say(`${c.name}: ${why(j)}`, true); return; }
    }
    say('Sending Home…');
    const d = deck();
    d.pages = d.pages.filter((p: { id: string }) => p.id === 'home' || used.has(p.id));
    const j = await post({ target, kind: 'deck', spec: d });
    if (j?.stage !== 'sent' || !j.panel?.ok) { say(`Home: ${why(j)}`, true); return; }
    say(`Sent: ${j.panel.status === 'unchanged' ? 'the panel already had it' : 'on the panel now'}${j.panel.clients === 0 ? ` (open /wasm/device?target=${target} to see it)` : ''}`);
  } catch (e) { say(String(e), true); } finally { btn.disabled = false; }
}
async function revert() {
  if (!confirm('Put the panel back to its factory deck and screens?')) return;
  const j = await post({ target: ($('target') as HTMLSelectElement).value, kind: 'revert', id: 'all' }).catch((e) => ({ error: String(e) }));
  say(j?.panel?.ok === false || j?.error ? `Revert: ${why(j)}` : 'Panel back to factory', !!(j?.error || j?.panel?.ok === false));
}

// ---- Spec: the JSON, editable, and what the renderer said ----
function specPane() {
  const ta = $('spec') as HTMLTextAreaElement;
  if (document.activeElement !== ta) ta.value = JSON.stringify(spec, null, 1);
  $('err').textContent = err || 'No errors.';
  $('err').classList.toggle('bad', !!err);
}

// Tap areas (header toggle): every hit area over the glass, red when under touch:min (taps.ts), redrawn each frame
let showTaps = false;
function tapsLoop() {
  const cv = $('taps') as HTMLCanvasElement;
  cv.style.display = showTaps ? 'block' : 'none';
  if (!showTaps) return;
  const r = stage.canvas.getBoundingClientRect(), dpr = window.devicePixelRatio || 1;
  const W = Math.max(1, Math.round(r.width * dpr)), H = Math.max(1, Math.round(r.height * dpr));
  if (cv.width !== W || cv.height !== H) { cv.width = W; cv.height = H; Object.assign(cv.style, { width: `${r.width}px`, height: `${r.height}px` }); }
  const g = cv.getContext('2d')!;
  g.clearRect(0, 0, W, H);
  const list = stage.taps();
  drawTaps(g, list, W / stage.w, dpr);
  $('tapsum').textContent = list.length ? tapSummary(list) : '';
  requestAnimationFrame(tapsLoop);
}
export async function start() {
  engine = await Engine.load(D.den);
  stage = engine.stage(D.size, D.size);
  const wrap = $('dev');
  wrap.replaceChildren(stage.canvas, Object.assign(h('canvas'), { id: 'taps' }), Object.assign(h('div'), { id: 'ov' }));
  $('tapsbtn').onclick = () => { showTaps = !showTaps; $('tapsbtn').setAttribute('aria-pressed', String(showTaps)); $('tapsum').textContent = ''; tapsLoop(); };
  $('sendbtn').onclick = () => void send();
  $('revertbtn').onclick = () => void revert();
  $('specbtn').onclick = () => {
    showSpec = !showSpec;
    document.body.classList.toggle('spec', showSpec);
    $('specbtn').setAttribute('aria-pressed', String(showSpec));
    specPane();
  };
  ($('spec') as HTMLTextAreaElement).oninput = (ev) => {
    try { spec = JSON.parse((ev.target as HTMLTextAreaElement).value); sel = null; draw(); }
    catch (e) { $('err').textContent = `JSON: ${(e as Error).message}`; $('err').classList.add('bad'); }
  };
  addEventListener('keydown', (ev) => {
    if (ev.key !== 'Escape') return;
    if (draft) { if (!dirty() || confirm('Discard your unsaved card changes?')) cancelDraft(); return; }  // a Home card: Esc = Cancel
    sel = null; overlay(); side();
  });
  const [f, so] = await Promise.all([
    fetch('/api/wasm/spec', { cache: 'no-store' }).then((r) => r.json()).catch(() => ({})),
    fetch('/api/wasm/sources', { cache: 'no-store' }).then((r) => r.json()).catch(() => ({})),
  ]);
  factory = f?.factory ?? {};
  options = so?.options ?? {};
  // a tap on Home (or a screen's link) opens that screen here: a built-in or one of yours
  engine.onNavigate = (to) => {
    const id = to.split('?')[0], i = customs.findIndex((c) => c.id === id);
    if (i >= 0) open(`custom:${i}`); else if (factory[id]) open(`built:${id}`); else if (id === 'home') open('home');
  };
  picker();
  try { cat = JSON.parse(engine.str(engine.m._fw_catalogue())); types = (cat as { cards?: Record<string, CardType> }).cards ?? {}; } catch { types = {}; }
  (window as unknown as { lab: unknown }).lab = {  // console / headless checks (tests/editor/lab-card.test.mjs)
    engine, stage, spec: () => spec, cards: () => structuredClone(cards), boxes: () => homeBoxes(stage), selected: () => draft?.i ?? -1,
    draft: () => (draft ? structuredClone(draft.cards) : null), dirty: () => dirty(), reorder,
    head: () => structuredClone(heads[HOME]), headDraft: () => (draft ? structuredClone(draft.head) : null), headRule: () => headRule(), deck: () => deck(), slots: () => slots(), err: () => err,
    texts: () => { const s = stage; try { return (JSON.parse(engine.str(engine.m._fw_audit(s.id))) as { text?: string }[]).map((o) => o.text).filter(Boolean); } catch { return null; } },
  };
  draw();
  size();
  addEventListener('resize', size);
  cardDrag(wrap);
  headDrag();
}

// ---- pick up a Home card and move it (ported from the old editor's pickup, src/wasm/editor.ts
// press and drag (mouse / pen: after 6 px; touch: a still 300 ms hold, a swipe still scrolls) = the card
// lifts and follows the pointer, the other cards glide out of its way (a scratch render of the would-be order, the
// panel isn't told); drop = the new order STAGED in the draft like Move earlier / later (Save / Cancel, "Unsaved"),
// Esc = back where it was. A tap without a drag still opens the card's settings.
const reduced = () => matchMedia('(prefers-reduced-motion: reduce)').matches;
const moMs = () => (reduced() ? 0 : 220);
let layOrder: Card[] | null = null;  // deck(): the Home's cards for a scratch layout
let lifting: (() => void) | null = null;  // Esc = cancel the pick-up
let noClickUntil = 0;  // the click after a drop isn't a click
/** move item `from` of an order to position `to` (the others shift up / down one) */
export function reorder<T>(order: T[], from: number, to: number): T[] {
  const out = order.slice(), [x] = out.splice(from, 1);
  out.splice(to, 0, x);
  return out;
}
type Lift = { down(ev: PointerEvent): boolean; lift(ev: PointerEvent): void; move(ev: PointerEvent): void; drop(ev: PointerEvent): void; cancel(): void };
function pickup(el: HTMLElement, l: Lift) {
  let p: { id: number; x: number; y: number; on: boolean; t: number } | null = null;
  const end = () => { if (!p) return; clearTimeout(p.t); try { el.releasePointerCapture(p.id); } catch { /* gone */ } p = null; lifting = null; };
  const go = (ev: PointerEvent) => {
    if (!p) return;
    p.on = true;
    try { el.setPointerCapture(p.id); } catch { /* gone */ }  // only now: a capture from the press would retarget a plain click
    lifting = () => { end(); noClickUntil = Infinity; l.cancel(); };
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
/** the lifted card: a crop of the glass, raised, following the pointer */
function floatCopy(cv: HTMLCanvasElement, r: DOMRect, ev: PointerEvent, crop: [number, number, number, number]) {
  const f = h('div', 'ed-lift'), c = h('canvas');
  c.width = Math.max(1, Math.round(crop[2])); c.height = Math.max(1, Math.round(crop[3]));
  c.getContext('2d')!.drawImage(cv, crop[0], crop[1], crop[2], crop[3], 0, 0, c.width, c.height);
  f.append(c);
  Object.assign(f.style, { width: `${r.width}px`, height: `${r.height}px` });
  const ox = ev.clientX - r.left, oy = ev.clientY - r.top;
  const at = (x: number, y: number) => { f.style.transform = `translate(${x}px, ${y}px)`; };
  at(r.left, r.top);
  document.body.append(f);
  requestAnimationFrame(() => f.classList.add('up'));
  return {
    move: (e: PointerEvent) => at(e.clientX - ox, e.clientY - oy),
    land: (to: { left: number; top: number }, then?: () => void) => {
      f.classList.add('landing'); f.classList.remove('up'); at(to.left, to.top);
      window.setTimeout(() => { f.remove(); then?.(); }, moMs());
    },
  };
}
function cardDrag(wrap: HTMLElement) {
  document.addEventListener('click', (ev) => { if (performance.now() < noClickUntil) { noClickUntil = 0; ev.stopPropagation(); ev.preventDefault(); } }, true);
  document.addEventListener('pointerdown', () => { noClickUntil = 0; }, true);
  addEventListener('keydown', (ev) => { if (lifting && ev.key === 'Escape') { ev.preventDefault(); ev.stopImmediatePropagation(); lifting(); } }, true);
  let lift: null | { from: number; order: number[]; pos: number; boxes: HBox[]; first: HBox[]; layer: HTMLElement; ghosts: HTMLElement[]; lay: Stage; fl: ReturnType<typeof floatCopy> } = null;
  let downAt = -1;
  const ov = () => $('ov');
  const pct = (el: HTMLElement, b: HBox) => Object.assign(el.style, { left: `${(b.x / stage.w) * 100}%`, top: `${(b.y / stage.h) * 100}%`, width: `${(b.w / stage.w) * 100}%`, height: `${(b.h / stage.h) * 100}%` });
  const client = (b: HBox) => { const r = stage.canvas.getBoundingClientRect(), k = r.width / stage.w; return { left: r.left + b.x * k, top: r.top + b.y * k }; };
  const dev = (ev: PointerEvent): [number, number] => { const r = stage.canvas.getBoundingClientRect(); return [((ev.clientX - r.left) * stage.w) / r.width, ((ev.clientY - r.top) * stage.h) / r.height]; };
  const install = () => engine.withStr('deck', (pc) => engine.withStr(JSON.stringify(deck()), (pj) => engine.m._fw_spec(pc, pj)));
  /** the card boxes for a would-be order, from a scratch render */
  const layout = (order: number[]) => {
    const list = view();
    layOrder = order.map((i) => list[i]);
    install();
    lift!.lay.render('home', { page: 'home', opening: false }, light(), 0);
    layOrder = null;
    install();
    return homeBoxes(lift!.lay);
  };
  const reflow = () => { const l = lift!; l.order.forEach((c, j) => { if (l.boxes[j]) pct(l.ghosts[c], l.boxes[j]); }); };
  const finish = (to: HBox | undefined, after: () => void) => {
    const l = lift!;
    lift = null;
    wrap.classList.remove('dragging');
    l.fl.land(to ? client(to) : client(l.first[l.from]), () => { l.layer.remove(); l.lay.free(); });
    after();
  };
  pickup(wrap, {
    down: (ev) => {
      const b = cur.k === 'home' ? (ev.target as HTMLElement).closest?.('.ov-card') as HTMLElement | null : null;
      downAt = b ? Number(b.dataset.card) : -1;
      return downAt >= 0 && view().length > 1;
    },
    lift: (ev) => {
      const cv = stage.canvas, k = cv.width / stage.w, first = homeBoxes(stage), from = downAt, b0 = first[from];
      if (!b0) return;
      // the still layer: the glass with every card cut out (filled with the gap colour), each other card a copy that glides
      const layer = h('div', 'ed-lay'), bg = h('canvas');
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
        const c = h('canvas', 'g');
        c.width = Math.max(1, Math.round(b.w * k)); c.height = Math.max(1, Math.round(b.h * k));
        c.getContext('2d')!.drawImage(cv, b.x * k, b.y * k, b.w * k, b.h * k, 0, 0, c.width, c.height);
        layer.append(c);
        return c as HTMLElement;
      });
      Object.assign(layer.style, { width: cv.style.width, height: cv.style.height });
      wrap.insertBefore(layer, ov());
      const r = cv.getBoundingClientRect(), z = r.width / stage.w;
      const fl = floatCopy(cv, new DOMRect(r.left + b0.x * z, r.top + b0.y * z, b0.w * z, b0.h * z), ev, [b0.x * k, b0.y * k, b0.w * k, b0.h * k]);
      const lay = engine.stage(stage.w, stage.h);
      lay.label = 'Drag layout';
      lift = { from, order: first.map((_, i) => i), pos: from, boxes: first, first, layer, ghosts, lay, fl };
      reflow();
      wrap.classList.add('dragging');
    },
    move: (ev) => {
      if (!lift) return;
      lift.fl.move(ev);
      const [x, y] = dev(ev), j = lift.boxes.findIndex((b) => x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h);
      if (j < 0 || j === lift.pos) return;
      lift.order = reorder(lift.order, lift.pos, j);
      lift.pos = j;
      lift.boxes = layout(lift.order);
      reflow();
    },
    drop: () => {
      if (!lift) return;
      const order = lift.order, pos = lift.pos, moved = order.some((c, j) => c !== j);
      finish(lift.boxes[pos], () => {
        if (!moved) return;
        const list = view();
        if (!draft) draft = mkDraft(-1);
        draft.hd = false;
        draft.cards = order.map((i) => structuredClone(list[i]));
        draft.i = pos;  // the moved card stays picked: its settings show Save / Cancel
        draw();
      });
    },
    cancel: () => {
      if (!lift) return;
      lift.order = lift.first.map((_, i) => i);
      lift.boxes = lift.first;
      reflow();
      finish(undefined, () => {});
    },
  });
}

// ---- a Home card's settings (steps 1-2 of the new editor; ported from the old editor's card modal,
// src/wasm/editor.ts cardFields). Click a card on the glass = it is selected (outlined) and its
// settings open in the side panel (a bottom sheet on a phone); the whole Home stays visible and live. Every change
// (entity, tap, label, size, move, duplicate, remove, add) is STAGED in a draft of the card list, drawn live, and only
// kept on Save; Cancel (or Esc) drops it. Nothing here reaches Home Assistant (the glass's taps go to the selection).
type CardType = { domains: string; sizes: string; taps: string; detail: string; by_domain?: Record<string, { taps: string; detail?: string }> };
const domOf = (e?: string) => (e ?? '').split('.')[0];
/** a card's tap choices: its type's, or for the Entity tile its entity's domain's (core/spec.h ENTITY_TAPS; a detail
 *  only where this build has the domain's screen) */
function tapsOf(c: Card): string[] {
  const ty = types[c.t];
  if (c.t !== 'entity') return (ty?.taps ?? '').split(',').filter(Boolean);
  const bd = ty?.by_domain?.[domOf(c.e)] ?? ty?.by_domain?.['*'] ?? { taps: 'none' };
  return bd.taps.split(',').filter((x) => x && (x !== 'detail' || !!bd.detail));
}
const RUN_WORD: Record<string, string> = { script: 'Run it', scene: 'Activate it', automation: 'Trigger it now', button: 'Press it' };
function tapName(c: Card, x: string) {
  const d = domOf(c.e), det = types.entity?.by_domain?.[d]?.detail;
  if (c.t === 'entity' && x === 'run') return RUN_WORD[d] ?? 'Run it';
  if (c.t === 'entity' && x === 'toggle' && d === 'automation') return 'Turn it on / off';
  if (c.t === 'entity' && x === 'detail' && det) return `Open its screen (${screenName(det)})`;
  return TAP_NAME[x] ?? x;
}
let types: Record<string, CardType> = {};
const TYPE_NAME: Record<string, string> = { light: 'Light or switch', cover: 'Blind or curtain', climate: 'Climate', media: 'Media player', sensor: 'Sensor (read only)', page: 'Screen', entity: 'Any entity (a tile, like HA’s tile card)' };
const SIZE_NAME: Record<string, string> = { s: 'Tile', m: 'Card', l: 'Wide' };
const sizeName = (t: string, x: string) => (t === 'sensor' && x === 'm' ? 'Readout (half a card)' : SIZE_NAME[x] ?? x);
const TAP_NAME: Record<string, string> = { toggle: 'Toggle', detail: 'Open its screen', page: 'Go to the screen', none: 'Nothing (reports only)', run: 'Run it' };
const entName = (id?: string) => Object.values(options).flat().find((o) => o.id === id)?.name ?? '';
function cardName(c?: Card) {
  if (!c) return '';
  if (c.label) return c.label;
  if (c.t === 'page') return screenName(c.page ?? '');
  return entName(c.e) || c.e || TYPE_NAME[c.t] || c.t;
}
type HBox = { card: number; x: number; y: number; w: number; h: number };
/** the Home's card boxes on a stage, in card order (fw_comps: every entity_tile / card / readout at the top level) */
function homeBoxes(s: Stage): HBox[] {
  const out: HBox[] = [];
  let i = 0;
  for (const c of s.comps()) if (['entity_tile', 'card', 'readout'].includes(c.ids[0])) out.push({ card: i++, x: c.x, y: c.y, w: c.w, h: c.h });
  return out;
}
// the draft: a copy of the card list being edited + the selected card (-1 = none, e.g. after Remove)
// + the Home's header buttons (head) and whether the header is the selection (hd; hi = the button open in the list)
let draft: { cards: Card[]; i: number; head: Btn[]; hd: boolean; hi: number } | null = null;
const mkDraft = (i: number, hd = false) => ({ cards: structuredClone(cards), i, head: structuredClone(heads[HOME]), hd, hi: -1 });
let epOpen = false, savedAt = 0;
const view = () => draft?.cards ?? cards;
const dirty = () => !!draft && (JSON.stringify(draft.cards) !== JSON.stringify(cards) || JSON.stringify(draft.head) !== JSON.stringify(heads[HOME]));
function selectCard(i: number) {
  if (draft) { draft.i = i; draft.hd = false; } else draft = mkDraft(i);
  epOpen = false;
  draw();
  if (matchMedia('(max-width: 900px)').matches) document.querySelector('.stagewrap')?.scrollIntoView({ block: 'start' });  // phone: the glass above the sheet
}
function addCard(c: Card) {
  if (!draft) draft = mkDraft(-1);
  draft.hd = false;
  draft.cards.push(c);
  draft.i = draft.cards.length - 1;
  epOpen = false;
  draw();
  if (matchMedia('(max-width: 900px)').matches) document.querySelector('.stagewrap')?.scrollIntoView({ block: 'start' });
}
function editCard(f: (c: Card) => void) { if (!draft || !draft.cards[draft.i]) return; f(draft.cards[draft.i]); draw(); }
function saveDraft() { if (!draft) return; cards = draft.cards; heads[HOME] = draft.head; draft = null; savedAt = Date.now(); draw(); }
function cancelDraft() { draft = null; draw(); }
function chipsOf(list: [string, string][], v: string, on: (x: string) => void) {
  const chips = h('div', 'chips');
  for (const [k, t] of list) { const b = h('button', 'chip' + (v === k ? ' on' : ''), t); b.type = 'button'; b.dataset.v = k; b.onclick = () => on(k); chips.append(b); }
  return chips;
}
const prop = (label: string, ...kids: HTMLElement[]) => { const r = h('div', 'prop'); r.append(h('span', 'pl', label), ...kids); return r; };
/** the shared searchable entity picker (entity-picker.ts); its open state lives in `epOpen` so a side redraw keeps it */
const entityPicker = (doms: string[], cur: string, pick: (id: string) => void): HTMLElement =>
  sharedPicker({ doms, cur, options, pick, open: epOpen, setOpen: (v, redraw) => { epOpen = v; if (redraw) side(); } });
function cardState(): HTMLElement {
  const d = dirty(), p = h('p', 'cs-state note ' + (d ? 'warn' : 'quiet'), d ? 'Unsaved changes: Save keeps them, Cancel drops them.' : 'No changes yet.');
  p.dataset.testid = 'card-state';
  return p;
}
function cardSide(): HTMLElement[] {
  const d = draft!, i = d.i, list = d.cards, c = list[i], out: HTMLElement[] = [];
  const bar = h('div', 'sidetop');
  const t = h('div', 'ttl');
  t.append(h('h2', '', c ? cardName(c) : 'Home'), h('p', 'muted', c ? `${TYPE_NAME[c.t] ?? c.t} · card ${i + 1} of ${list.length}` : 'Click a card on the panel to change it.'));
  bar.append(t);
  out.push(bar, cardState());
  if (c) {
    const ty = types[c.t];
    if (c.t === 'page') {
      const screens = [...customs.map((x) => [x.id, x.name ?? x.id] as [string, string]),
        ...Object.values(factory).filter((x) => x.role !== 'deck' && x.role !== 'detail' && Array.isArray(x.nodes)).map((x) => [x.id, x.name ?? x.id] as [string, string])]
        .sort((a, b) => a[1].localeCompare(b[1]));
      const sel = h('select');
      sel.setAttribute('aria-label', 'Screen');
      for (const [id, nm] of screens) { const o = h('option', '', nm); o.value = id; sel.append(o); }
      if (c.page && !screens.some(([id]) => id === c.page)) { const o = h('option', '', c.page); o.value = c.page; sel.append(o); }
      sel.value = c.page ?? '';
      sel.onchange = () => editCard((x) => { x.page = sel.value; });
      out.push(prop('Screen', sel));
    } else if (ty) out.push(prop('Entity', entityPicker(ty.domains === '*' ? Object.keys(options).sort() : ty.domains.split(','), c.e ?? '', (id) => editCard((x) => {
      x.e = id;
      if (x.t === 'entity') x.tap = tapsOf(x)[0];  // a new entity: its domain's default tap (like HA's)
    }))));
    const taps = tapsOf(c);
    if (taps.length > 1) {
      const r = prop('Tap', chipsOf(taps.map((x) => [x, tapName(c, x)]), c.tap ?? taps[0], (v) => editCard((x) => { x.tap = v; })));
      r.dataset.testid = 'tap';
      out.push(r);
    } else if (taps.length === 1) { const r = prop('Tap', h('p', 'help', tapName(c, taps[0]))); r.dataset.testid = 'tap'; out.push(r); }
    const sizes = (ty?.sizes ?? '').split(',').filter(Boolean);
    if (sizes.length > 1) {
      const r = prop('Size', chipsOf(sizes.map((x) => [x, sizeName(c.t, x)]), c.size, (v) => editCard((x) => { x.size = v as Card['size']; })));
      r.dataset.testid = 'size';
      out.push(r);
    }
    const lb = h('input');
    lb.value = c.label ?? '';
    lb.placeholder = cardName({ ...c, label: undefined });
    lb.setAttribute('aria-label', 'Label');
    lb.oninput = () => editCard((x) => { if (lb.value) x.label = lb.value; else delete x.label; });
    out.push(prop('Label (empty = its own name)', lb));
    const acts = h('div', 'cm-acts');
    const btn = (label: string, on: () => void, dis = false, cls = 'btn') => { const b = h('button', cls, label); b.type = 'button'; b.disabled = dis; b.onclick = on; acts.append(b); };
    btn('Move earlier', () => { [list[i - 1], list[i]] = [list[i], list[i - 1]]; d.i = i - 1; draw(); }, i === 0);
    btn('Move later', () => { [list[i + 1], list[i]] = [list[i], list[i + 1]]; d.i = i + 1; draw(); }, i === list.length - 1);
    btn('Duplicate', () => { list.splice(i + 1, 0, structuredClone(c)); d.i = i + 1; draw(); });
    if (c.t === 'page' && c.page) btn('Open the screen', () => { const id = c.page!, k = customs.findIndex((x) => x.id === id); open(k >= 0 ? `custom:${k}` : `built:${id}`); }, dirty());
    btn('Remove', () => { list.splice(i, 1); d.i = -1; draw(); }, false, 'btn danger');
    out.push(acts);
  }
  out.push(footer());
  return out;
}
/** Save / Cancel under a Home card's or the header's settings (the staged draft) */
function footer(): HTMLElement {
  const foot = h('div', 'cs-foot');
  const cancel = h('button', 'btn cs-cancel', dirty() ? 'Cancel' : 'Close');
  cancel.type = 'button';
  cancel.title = 'Drop the unsaved changes (Esc)';
  cancel.onclick = cancelDraft;
  const save = h('button', 'done cs-save', 'Save');
  save.type = 'button';
  save.disabled = !dirty();
  save.onclick = saveDraft;
  foot.append(cancel, save);
  return foot;
}

// ---- a placed component's tap (step 4): the actions its "tap" event allows (the catalogue's
// events, core/spec.h RULES) that the editor can set up: nothing, open a screen, go back, toggle a light, or the
// Action tile's four (run a script, activate a scene, trigger an automation, press a button)
let cat: { components?: Record<string, { events?: Record<string, string[]> }> } = {};
const TAP_ACTS: [string, string, string[]][] = [  // action, words, entity domains ([] = none)
  ['nav.open', 'Open a screen', []], ['nav.back', 'Go back', []], ['light.toggle', 'Toggle a light', ['light']],
  ['script.run', 'Run a script', ['script']], ['scene.activate', 'Activate a scene', ['scene']],
  ['automation.trigger', 'Trigger an automation', ['automation']], ['button.press', 'Press a button', ['button']]];
function tapSettings(node: Node): HTMLElement[] {
  const allowed = cat.components?.[node.c]?.events?.tap ?? [];
  const acts = TAP_ACTS.filter(([d]) => allowed.includes(d));
  if (!acts.length) return [];
  const a = node.on?.tap as { do?: string; e?: string; screen?: string } | undefined;
  const cur = a?.do && acts.some(([d]) => d === a.do) ? a.do : a?.do ? '' : 'none';
  const set = (x: Record<string, unknown> | null) => { if (x) node.on = { ...(node.on ?? {}), tap: x }; else if (node.on) { delete node.on.tap; if (!Object.keys(node.on).length) delete node.on; } draw(); };
  const out: HTMLElement[] = [];
  const pick = h('select');
  pick.setAttribute('aria-label', 'On tap');
  const opt = (v: string, t: string) => { const o = h('option', '', t); o.value = v; pick.append(o); };
  opt('none', 'Nothing');
  for (const [d, t] of acts) opt(d, t);
  if (!cur) opt('', `Set in the spec (${a?.do})`);
  pick.value = cur;
  pick.onchange = () => {
    const d = pick.value, doms = TAP_ACTS.find(([x]) => x === d)?.[2] ?? [];
    epOpen = false;
    if (d === 'none') return set(null);
    if (d === 'nav.open') return set({ do: d, screen: Object.values(factory).find((x) => x.role !== 'deck' && x.role !== 'detail' && Array.isArray(x.nodes))?.id ?? 'clock' });
    if (!doms.length) return set({ do: d });
    set({ do: d, e: doms.flatMap((x) => options[x] ?? [])[0]?.id ?? `${doms[0]}.none` });
  };
  const r = prop('On tap', pick);
  r.dataset.testid = 'on-tap';
  out.push(r);
  if (a?.do === 'nav.open') {
    const sel = h('select');
    sel.setAttribute('aria-label', 'Screen');
    for (const x of Object.values(factory).filter((x) => x.role !== 'deck' && x.role !== 'detail' && Array.isArray(x.nodes)).sort((p, q) => (p.name ?? p.id).localeCompare(q.name ?? q.id))) { const o = h('option', '', x.name ?? x.id); o.value = x.id; sel.append(o); }
    sel.value = a.screen ?? '';
    sel.onchange = () => set({ ...a, screen: sel.value });
    out.push(prop('Screen', sel));
  }
  const doms = TAP_ACTS.find(([x]) => x === a?.do)?.[2] ?? [];
  if (a && doms.length) out.push(prop('Entity', entityPicker(doms, a.e ?? '', (id) => set({ ...a, e: id }))));
  return out;
}

// ---- the Home's header buttons: tap the header on the glass = it is selected and the side panel
// lists this Home's header-end buttons; each opens a screen and wears that screen's icon (no
// entity taps, no icon or style picker here; Facet home.cpp button_node picks the icon); add / reorder (drag a row, or Move up / down) / remove,
// staged in the draft with the cards (Save / Cancel, "Unsaved"). How many: the Header's end slot "max" (its meta
// slots, CONTRACT C8a; core/spec.h refuses more) less the header's own items (the Home always has Settings).
const HOME = 'home';
type Btn = { t: 'page'; page: string };
const heads: Record<string, Btn[]> = { [HOME]: [] };  // each Home its own list (the lab has one Home)
const headView = () => draft?.head ?? heads[HOME];
function headRule() {
  const decls = (comp('header')?.meta as { slots?: unknown } | undefined)?.slots;
  const d = Array.isArray(decls) ? (decls as { name: string; max?: number }[]).find((x) => x.name === 'end') : undefined;
  const max = d?.max ?? 0, own = 1;  // own: the Home's Settings button
  return { max, own, room: max ? Math.max(0, max - own) : 99 };
}
const btnName = (b: Btn) => `Opens ${screenName(b.page)}`;
const screenList = () => [...customs.map((x) => [x.id, x.name ?? x.id] as [string, string]),
  ...Object.values(factory).filter((x) => x.role !== 'deck' && x.role !== 'detail' && Array.isArray(x.nodes)).map((x) => [x.id, x.name ?? x.id] as [string, string])]
  .sort((a, b) => a[1].localeCompare(b[1]));
function selectHead() {
  if (draft) { draft.hd = true; draft.i = -1; } else draft = mkDraft(-1, true);
  epOpen = false;
  draw();
  if (matchMedia('(max-width: 900px)').matches) document.querySelector('.stagewrap')?.scrollIntoView({ block: 'start' });
}
function editHead(k: number, f: (b: Btn) => void) { if (!draft?.head[k]) return; f(draft.head[k]); draw(); }
function newBtn(): Btn {
  const used = new Set(headView().map((x) => x.page)), l = screenList();
  return { t: 'page', page: (l.find(([id]) => !used.has(id)) ?? l[0])?.[0] ?? 'clock' };
}
function btnFields(b: Btn, k: number): HTMLElement[] {
  const d = draft!, out: HTMLElement[] = [];
  const sel = h('select');
  sel.setAttribute('aria-label', 'Screen');
  for (const [id, nm] of screenList()) { const o = h('option', '', nm); o.value = id; sel.append(o); }
  sel.value = b.page;
  sel.onchange = () => editHead(k, (x) => { x.page = sel.value; });
  out.push(prop('Opens', sel), h('p', 'help', "The button shows that screen's icon."));
  const acts = h('div', 'cm-acts');
  const btn = (label: string, on: () => void, dis = false, cls = 'btn') => { const x = h('button', cls, label); x.type = 'button'; x.disabled = dis; x.onclick = on; acts.append(x); };
  btn('Move up', () => { d.head = reorder(d.head, k, k - 1); d.hi = k - 1; draw(); }, k === 0);
  btn('Move down', () => { d.head = reorder(d.head, k, k + 1); d.hi = k + 1; draw(); }, k === d.head.length - 1);
  btn('Remove', () => { d.head.splice(k, 1); d.hi = -1; epOpen = false; draw(); }, false, 'btn danger');
  out.push(acts);
  return out;
}
function headSide(): HTMLElement[] {
  const d = draft!, list = d.head, r = headRule(), out: HTMLElement[] = [];
  const bar = h('div', 'sidetop'), t = h('div', 'ttl');
  t.append(h('h2', '', 'Header'), h('p', 'muted', `Home · buttons at the end of the header: ${list.length} of ${r.room}`));
  bar.append(t);
  out.push(bar, cardState());
  const ul = h('div', 'hb-list');
  ul.dataset.testid = 'head-buttons';
  list.forEach((b, k) => {
    const row = h('div', 'hb-row' + (d.hi === k ? ' open' : ''));
    row.dataset.b = String(k);
    const hd = h('button', 'hb-head');
    hd.type = 'button';
    hd.title = list.length > 1 ? 'Click to change it; drag to reorder' : 'Click to change it';
    hd.append(h('span', 'hb-grip', '⋮⋮'), h('b', '', btnName(b)));
    hd.onclick = () => { d.hi = d.hi === k ? -1 : k; epOpen = false; side(); };
    row.append(hd);
    if (d.hi === k) row.append(...btnFields(b, k));
    ul.append(row);
  });
  if (!list.length) ul.append(h('p', 'muted', 'No buttons yet.'));
  out.push(ul);
  const add = h('button', 'btn', 'Add a button');
  add.type = 'button';
  add.dataset.testid = 'head-add';
  add.disabled = list.length >= r.room;
  add.onclick = () => { list.push(newBtn()); d.hi = list.length - 1; epOpen = false; draw(); };
  out.push(add);
  if (r.max) out.push(h('p', 'help', `The header's end holds ${r.max} items and the Settings button is one, so ${r.room} buttons fit.`));
  out.push(footer());
  return out;
}
/** drag a button row in the list to reorder (the cards' pick-up, src above): the order follows the pointer live */
function headDrag() {
  const el = $('side');
  let from = -1, pos = -1;
  const mark = () => el.querySelector(`.hb-row[data-b="${pos}"]`)?.classList.add('lifted');
  pickup(el, {
    down: (ev) => {
      const r = cur.k === 'home' && draft?.hd ? ((ev.target as HTMLElement).closest?.('.hb-head')?.parentElement as HTMLElement | null) : null;
      from = pos = r ? Number(r.dataset.b) : -1;
      return from >= 0 && draft!.head.length > 1;
    },
    lift: () => mark(),
    move: (ev) => {
      if (!draft) return;
      const rows = [...el.querySelectorAll('.hb-row')] as HTMLElement[];
      const j = rows.findIndex((x) => { const b = x.getBoundingClientRect(); return ev.clientY >= b.top && ev.clientY < b.bottom; });
      if (j < 0 || j === pos) return;
      draft.head = reorder(draft.head, pos, j);
      if (draft.hi === pos) draft.hi = j;
      pos = j;
      draw();
      mark();
    },
    drop: () => side(),
    cancel: () => { if (draft && pos !== from) { draft.head = reorder(draft.head, pos, from); if (draft.hi === pos) draft.hi = from; } draw(); },
  });
}
