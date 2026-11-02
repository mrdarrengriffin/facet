// Facet editor v1, round 1 mock (src/pages/wasm/editor-v1.astro; scope workbench/EDITOR-V1.md). Four parts:
//   Screens         the deck: Home first, then every page screen on / off and in order, the start page
//   screen settings a screen's params as a form made from the schema (its spec params), its preview large and live
//   Panel settings  the schema's settings in their groups (a control per setting's own `control`) + Home's header
//                   buttons (the deck's home header_actions), Home previewed with the look applied
//   send / check / revert  Review lists the changes in words; the check is the panel's own deck check (fw_spec
//                   "check" in the WASM engine) with every error said against its screen and setting; Send keeps it
//                   as what "the panel" runs (this page's memory); Undo last send and Back to factory
// The previews are the real renderer (engine.ts) with Home Assistant stubbed (no states: sample data). Nothing leaves
// the page.
import { Engine, Stage, DEVICES, type DeviceId, type HaBridge } from './engine';

type Param = { about?: string; type: string; options?: string[]; label?: string; default?: string; domain?: string };
type Screen = { name: string; params: Record<string, Param> };
type Opt = { id: string; label: string; hint?: string };
type Setting = { id: string; group: string; label: string; help?: string; kind: string; control?: string; default: unknown;
  options?: Opt[]; min?: number; max?: number; step?: number; unit?: string; presets?: number[]; zero?: string };
type Boot = {
  screens: Record<string, Screen>; settings: { groups: { id: string; label: string }[]; settings: Setting[] };
  deck: { start: string; pages: { id: string; params?: Record<string, string> }[] };
  options: Record<string, { id: string; name: string }[]>;
  accents: { default: string; sets: { id: string; label: string; dark: { accent: string } }[] };
  scope: string; hash: string;
};
/** what the editor holds (and what "the panel" runs, the same shape): the deck as rows + the panel's settings */
type Draft = { order: string[]; on: Record<string, boolean>; start: string; params: Record<string, Record<string, string>>;
  header: { button: string; status: string }; settings: Record<string, unknown> };
type Page = { id: string; kind: string; name?: string; params?: Record<string, string>; cards?: unknown[]; header_actions?: unknown };

const boot: Boot = JSON.parse(document.getElementById('v1-boot')!.textContent!);
const $ = (id: string) => document.getElementById(id)!;
function h<K extends keyof HTMLElementTagNameMap>(tag: K, cls = '', text = ''): HTMLElementTagNameMap[K] {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text) e.textContent = text;
  return e;
}
const clone = <T>(x: T): T => JSON.parse(JSON.stringify(x));
const name = (id: string) => (id === 'home' ? 'Home' : boot.screens[id]?.name ?? id);
const noHa: HaBridge = { states: async () => null, action: async () => undefined, ask: async () => null };

// ---- the factory, the panel, the draft
function factory(): Draft {
  const inDeck = boot.deck.pages.map((p) => p.id).filter((id) => id !== 'home' && boot.screens[id]);
  const order = [...inDeck, ...Object.keys(boot.screens).filter((id) => !inDeck.includes(id))];
  const settings = Object.fromEntries(boot.settings.settings.map((s) => [s.id, s.default]));
  return { order, on: Object.fromEntries(order.map((id) => [id, inDeck.includes(id)])), start: boot.deck.start,
    params: Object.fromEntries(order.map((id) => [id, { ...(boot.deck.pages.find((p) => p.id === id)?.params ?? {}) }])),
    header: { button: 'settings', status: 'none' }, settings };
}
const panel = { rev: 3, runs: factory(), before: [] as { rev: number; runs: Draft }[] };
let draft = clone(panel.runs);

function paramVal(id: string, k: string) { return draft.params[id]?.[k] ?? boot.screens[id].params[k].default ?? ''; }
function toDeck(d: Draft, rev: number) {
  const on = d.order.filter((id) => d.on[id]);
  const home: Page = { id: 'home', kind: 'home', name: 'Home', cards: on.map((page) => ({ t: 'page', page, size: 'm' })) };
  if (d.header.button !== 'settings' || d.header.status !== 'none') home.header_actions = { button: d.header.button, status: d.header.status };
  const pages: Page[] = [home, ...on.map((id) => {
    const p: Page = { id, kind: id };
    const set = Object.entries(d.params[id] ?? {}).filter(([k, v]) => v !== (boot.screens[id].params[k]?.default ?? ''));
    if (set.length) p.params = Object.fromEntries(set);
    return p;
  })];
  return { v: 2, rev, start: d.start, pages };
}

// ---- what changed, in words
type Change = { where: string; what: string; was: string; now: string; key: string };
function changes(a: Draft, b: Draft): Change[] {
  const out: Change[] = [];
  const onA = a.order.filter((id) => a.on[id]), onB = b.order.filter((id) => b.on[id]);
  for (const id of b.order) if (!!a.on[id] !== !!b.on[id]) out.push({ where: name(id), what: 'On Home and in the swipe', was: a.on[id] ? 'on' : 'off', now: b.on[id] ? 'on' : 'off', key: `on.${id}` });
  const keptA = onA.filter((id) => onB.includes(id)), keptB = onB.filter((id) => onA.includes(id));
  if (keptA.join() !== keptB.join()) out.push({ where: 'Screens', what: 'Order', was: keptA.map(name).join(', '), now: keptB.map(name).join(', '), key: 'order' });
  if (a.start !== b.start) out.push({ where: 'Screens', what: 'Start page', was: name(a.start), now: name(b.start), key: 'start' });
  for (const id of b.order) for (const [k, p] of Object.entries(boot.screens[id].params)) {
    if (p.type === 'hidden') continue;
    const va = a.params[id]?.[k] ?? p.default ?? '', vb = b.params[id]?.[k] ?? p.default ?? '';
    if (va !== vb) out.push({ where: name(id), what: p.label ?? k, was: va || 'none', now: vb || 'none', key: `param.${id}.${k}` });
  }
  if (a.header.button !== b.header.button) out.push({ where: 'Home header', what: 'Button', was: hbName(a.header.button), now: hbName(b.header.button), key: 'header.button' });
  if (a.header.status !== b.header.status) out.push({ where: 'Home header', what: 'Status', was: a.header.status, now: b.header.status, key: 'header.status' });
  for (const s of boot.settings.settings) if (a.settings[s.id] !== b.settings[s.id]) out.push({ where: 'Panel', what: s.label, was: say(s, a.settings[s.id]), now: say(s, b.settings[s.id]), key: `set.${s.id}` });
  return out;
}
const hbName = (b: string) => (b === 'settings' ? 'Settings' : name(b));
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
    const m = x.path.match(/^\.pages\[(\d+)\](?:\.(params)\.(\w+)|\.(header_actions)(?:\.(\w+))?|\.(\w+))?/);
    const pg = m ? d.pages[+m[1]] : null;
    const where = pg ? (m![4] ? 'Home header' : name(pg.kind)) : x.path.startsWith('.start') ? 'Start page' : 'Screens';
    const what = pg && m![3] ? boot.screens[pg.kind]?.params[m![3]]?.label ?? m![3] : m?.[5] ?? '';
    return { where, what, msg: x.msg };
  }) };
}

// ---- engine + previews
let device: DeviceId = new URLSearchParams(location.search).get('device') === 'guition' ? 'guition' : 'waveshare';
let engine: Engine | null = null;
let view: { kind: 'screens' } | { kind: 'screen'; id: string } | { kind: 'panel' } = { kind: 'screens' };
const stages: Stage[] = [];
const S = () => DEVICES[device].size;
const light = () => draft.settings.theme === 'light';  // Auto previews dark (the mock has no sun)
const accent = () => boot.accents.sets.findIndex((a) => a.id === draft.settings.accent);
function applyDeck() {
  const e = engine;
  if (!e) return;
  e.withStr('deck', (pc) => e.withStr(JSON.stringify(toDeck(draft, panel.rev)), (pj) => e.str(e.m._fw_spec(pc, pj))));
}
function preview(id: string, cssW: number, label: string) {
  const s = engine!.stage(S(), S());
  s.label = label;
  stages.push(s);
  s.canvas.style.width = `${cssW}px`;
  s.canvas.style.height = `${cssW}px`;
  s.canvas.className = 'v1-canvas';
  draw(s, id);
  return s;
}
function draw(s: Stage, id: string) {
  if (id === 'home') s.render('home', {}, light(), accent());
  else s.render('spec', { screen: id, ...Object.fromEntries(Object.keys(boot.screens[id].params).map((k) => [k, paramVal(id, k)])) }, light(), accent());
}
function freeStages() { for (const s of stages.splice(0)) s.free(); }

// ---- the page
function edit(f: () => void) { f(); applyDeck(); top(); render(); }
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
  tabs.replaceChildren(...([['screens', 'Screens'], ['panel', 'Panel settings']] as const).map(([k, l]) => {
    const b = h('button', '', l);
    b.type = 'button';
    b.setAttribute('aria-current', String(view.kind === k || (k === 'screens' && view.kind === 'screen')));
    b.onclick = () => { view = { kind: k }; render(); top(); };
    return b;
  }));
  const n = changes(panel.runs, draft).length;
  $('v1-count').textContent = n ? `${n} change${n === 1 ? '' : 's'} not sent` : `The panel runs rev ${panel.rev}`;
  ($('v1-review') as HTMLButtonElement).disabled = !engine;
}

function render() {
  if (!engine) return;
  freeStages();
  const main = $('v1-main');
  main.dataset.view = view.kind;
  if (view.kind === 'screens') main.replaceChildren(screensView());
  else if (view.kind === 'screen') main.replaceChildren(screenView(view.id));
  else main.replaceChildren(panelView());
}

// 1. Screens
function screensView() {
  const wrap = h('section', 'v1-screens');
  const head = h('div', 'v1-head');
  head.append(h('h1', '', 'Screens'), h('p', 'dim', 'Home first, then the screens you swipe to, in this order. Their cards on Home follow the same order. Tap a screen to set it up.'));
  wrap.append(head);
  const list = h('ol', 'v1-list');
  list.append(row('home'));
  const on = draft.order.filter((id) => draft.on[id]), off = draft.order.filter((id) => !draft.on[id]);
  for (const id of on) list.append(row(id));
  wrap.append(list);
  if (off.length) {
    wrap.append(h('h2', 'cap', 'Not on this panel'));
    const l2 = h('ol', 'v1-list off');
    for (const id of off) l2.append(row(id));
    wrap.append(l2);
  }
  return wrap;
}
function row(id: string) {
  const li = h('li', 'v1-row');
  li.dataset.id = id;
  const on = id === 'home' || draft.on[id];
  const grip = h('span', 'v1-grip', id === 'home' || !on ? '' : '⋮⋮');
  grip.setAttribute('aria-hidden', 'true');
  const thumb = h('button', 'v1-thumb');
  thumb.type = 'button';
  thumb.setAttribute('aria-label', `Set up ${name(id)}`);
  thumb.append(preview(id, 88, `thumb:${id}`).canvas);
  const txt = h('button', 'v1-txt');
  txt.type = 'button';
  txt.append(h('b', '', name(id)), h('span', 'dim', id === 'home' ? `${draft.order.filter((x) => draft.on[x]).length} cards, always first` : summary(id)));
  if (id === draft.start) txt.append(h('span', 'v1-badge', 'Start page'));
  const open = () => { if (id === 'home') { view = { kind: 'panel' }; } else { view = { kind: 'screen', id }; } top(); render(); };
  thumb.onclick = open;
  txt.onclick = open;
  const end = h('div', 'v1-end');
  if (id !== 'home' && on) {
    const i = draft.order.filter((x) => draft.on[x]).indexOf(id), n = draft.order.filter((x) => draft.on[x]).length;
    end.append(iconBtn('↑', `Move ${name(id)} up`, i === 0, () => move(id, -1)), iconBtn('↓', `Move ${name(id)} down`, i === n - 1, () => move(id, 1)));
  }
  if (id !== 'home') {
    const locked = id === draft.start;
    end.append(toggle(on, locked ? `${name(id)} is the start page` : `${name(id)} on this panel`, locked, () => edit(() => { draft.on[id] = !on; })));
  }
  li.append(grip, thumb, txt, end);
  if (id !== 'home' && on) dragRow(li, id);
  return li;
}
function summary(id: string) {
  const p = boot.screens[id].params;
  const src = p.source ? paramVal(id, 'source') : '';
  const ent = Object.entries(p).find(([, x]) => x.type === 'entity');
  if (src === 'sample') return 'Sample data (previews)';
  if (ent) return paramVal(id, ent[0]) || `No ${ent[1].label?.toLowerCase() ?? 'entity'} picked`;
  return src ? `From ${src === 'ha' ? 'Home Assistant' : src}` : 'Built in';
}
function iconBtn(t: string, label: string, disabled: boolean, f: () => void) {
  const b = h('button', 'v1-ibtn', t);
  b.type = 'button';
  b.disabled = disabled;
  b.setAttribute('aria-label', label);
  b.onclick = (e) => { e.stopPropagation(); f(); };
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
function move(id: string, by: number) {
  edit(() => {
    const on = draft.order.filter((x) => draft.on[x]);
    const i = on.indexOf(id), j = i + by;
    if (j < 0 || j >= on.length) return;
    [on[i], on[j]] = [on[j], on[i]];
    draft.order = [...on, ...draft.order.filter((x) => !draft.on[x])];
  });
}
let dragging = '';
function dragRow(li: HTMLElement, id: string) {
  li.draggable = true;
  li.ondragstart = (e) => { dragging = id; li.classList.add('lift'); e.dataTransfer?.setData('text/plain', id); };
  li.ondragend = () => { dragging = ''; li.classList.remove('lift'); };
  li.ondragover = (e) => { if (dragging && dragging !== id) { e.preventDefault(); li.classList.add('over'); } };
  li.ondragleave = () => li.classList.remove('over');
  li.ondrop = (e) => {
    e.preventDefault();
    const from = dragging;
    if (!from || from === id) return;
    edit(() => {
      const on = draft.order.filter((x) => draft.on[x] && x !== from);
      on.splice(on.indexOf(id) + (draft.order.indexOf(from) < draft.order.indexOf(id) ? 1 : 0), 0, from);
      draft.order = [...on, ...draft.order.filter((x) => !draft.on[x])];
    });
  };
}

// 2. A screen's settings
function screenView(id: string) {
  const wrap = h('section', 'v1-split');
  const left = h('div', 'v1-stage');
  const back = h('button', 'link', '← Screens');
  back.type = 'button';
  back.onclick = () => { view = { kind: 'screens' }; top(); render(); };
  const big = Math.min(S(), 480);
  const s = preview(id, big, 'Preview');
  left.append(back, s.canvas, h('p', 'dim', `${DEVICES[device].name}, ${S()} × ${S()}${big < S() ? `, shown at ${Math.round((big / S()) * 100)}%` : ', 1:1'}`));
  const form = h('div', 'v1-form');
  form.append(h('h1', '', name(id)));
  const on = draft.on[id];
  const st = h('div', 'v1-field inline');
  st.append(h('label', '', 'On this panel'), toggle(on, `${name(id)} on this panel`, id === draft.start, () => edit(() => { draft.on[id] = !on; })));
  form.append(st);
  const st2 = h('div', 'v1-field inline');
  const startBtn = h('button', 'btn ghost', id === draft.start ? 'This is the start page' : 'Make it the start page');
  startBtn.type = 'button';
  startBtn.disabled = id === draft.start || !on;
  startBtn.onclick = () => edit(() => { draft.start = id; });
  st2.append(h('label', '', 'Start page'), startBtn);
  form.append(st2);
  for (const [k, p] of Object.entries(boot.screens[id].params)) {
    if (p.type === 'hidden') continue;
    form.append(field(p.label ?? k, p.about, paramControl(id, k, p)));
  }
  const reset = h('button', 'btn ghost', 'Back to defaults');
  reset.type = 'button';
  reset.onclick = () => edit(() => { draft.params[id] = {}; });
  form.append(reset);
  if (Object.values(boot.screens[id].params).some((p) => p.type === 'select' && p.options?.includes('ha'))) form.append(h('p', 'dim', 'Home Assistant is a stub in this mock: set to Home Assistant, the preview shows Loading; Sample shows the screen filled.'));
  if (!Object.keys(boot.screens[id].params).length) form.append(h('p', 'dim', 'This screen has nothing to set.'));
  wrap.append(left, form);
  return wrap;
}
function field(label: string, about: string | undefined, control: HTMLElement) {
  const f = h('div', 'v1-field');
  f.append(h('label', '', label), control);
  if (about) f.append(h('p', 'dim', about));
  return f;
}
function setParam(id: string, k: string, v: string) { edit(() => { (draft.params[id] ??= {})[k] = v; }); }
function paramControl(id: string, k: string, p: Param): HTMLElement {
  const v = paramVal(id, k);
  if (p.type === 'select' && p.options) return p.options.length <= 3 ? seg(p.options.map((o) => [o, o === 'ha' ? 'Home Assistant' : o === 'sample' ? 'Sample' : o]), v, (x) => setParam(id, k, x)) : list(p.options.map((o) => [o, o]), v, (x) => setParam(id, k, x));
  if (p.type === 'entity') return entityPick(p.domain ?? '', v, (x) => setParam(id, k, x));
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
      c.onchange = () => { c.checked ? have.add(o.id) : have.delete(o.id); setParam(id, k, [...have].join(',')); };
      l.append(c, document.createTextNode(` ${o.name}`), h('span', 'dim mono', ` ${o.id}`));
      box.append(l);
    }
    return box;
  }
  const t = h('input', 'v1-input');
  t.value = v;
  t.onchange = () => setParam(id, k, t.value);
  return t;
}
function entityPick(domain: string, v: string, set: (x: string) => void) {
  const opts = boot.options[domain] ?? [];
  const s = h('select', 'v1-input');
  const known = opts.some((o) => o.id === v);
  s.append(Object.assign(h('option', '', v ? (known ? '' : `${v} (not in this house)`) : `Pick a ${domain.replace('_', ' ')}`), { value: v, hidden: known }));
  for (const o of opts) s.append(Object.assign(h('option', '', `${o.name}  ·  ${o.id}`), { value: o.id }));
  s.value = v;
  s.onchange = () => set(s.value);
  return s;
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
function list(items: [string, string][], cur: string, pick: (v: string) => void) {
  const s = h('select', 'v1-input');
  for (const [v, l] of items) s.append(Object.assign(h('option', '', l), { value: v }));
  s.value = cur;
  s.onchange = () => pick(s.value);
  return s;
}

// 3. Panel settings
function panelView() {
  const wrap = h('section', 'v1-split');
  const left = h('div', 'v1-stage');
  const big = Math.min(S(), 480);
  left.append(preview('home', big, 'Preview').canvas, h('p', 'dim', 'Home with the look applied. Auto shows dark here; text size, rotation, sounds and idle show on the panel.'));
  const form = h('div', 'v1-form');
  form.append(h('h1', '', 'Panel settings'));
  form.append(h('h2', 'cap', 'Home header'));
  const pages = draft.order.filter((x) => draft.on[x]);
  form.append(field('Button', 'The button at the top right of Home: Settings, or a screen it opens.', list([['settings', 'Settings'], ...pages.map((p) => [p, name(p)] as [string, string])], draft.header.button, (v) => edit(() => { draft.header.button = v; }))));
  form.append(h('p', 'dim', 'To see the panel refuse a send: pick a screen here, then turn that screen off in Screens and review.'));
  form.append(field('Status', 'What Home shows next to the time: nothing, or a running timer.', seg([['none', 'Nothing'], ['timer', 'Timer']], draft.header.status, (v) => edit(() => { draft.header.status = v; }))));
  for (const g of boot.settings.groups) {
    form.append(h('h2', 'cap', g.label));
    for (const s of boot.settings.settings.filter((x) => x.group === g.id)) form.append(field(s.label, s.help, settingControl(s)));
  }
  wrap.append(left, form);
  return wrap;
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
  if (s.options) return s.control === 'segmented' ? seg(s.options.map((o) => [o.id, o.label]), String(v), (x) => setSetting(s.id, x)) : list(s.options.map((o) => [o.id, o.hint ? `${o.label} (${o.hint})` : o.label]), String(v), (x) => setSetting(s.id, x));
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

// 4. Review, check, send, revert
function review() {
  const d = $('v1-dialog') as HTMLDialogElement;
  const box = h('div', 'dlg');
  const ch = changes(panel.runs, draft);
  box.append(h('span', 'cap', `Send to ${DEVICES[device].name}`), h('h2', '', ch.length ? `${ch.length} change${ch.length === 1 ? '' : 's'}` : 'Nothing changed'));
  if (ch.length) {
    const ul = h('ul', 'v1-changes');
    for (const c of ch) {
      const li = h('li');
      li.append(h('b', '', c.what ? `${c.where} · ${c.what}` : c.where), h('span', 'was', c.was), document.createTextNode('  →  '), h('span', '', c.now));
      ul.append(li);
    }
    box.append(ul);
  }
  const r = check();
  const verdict = h('div', `v1-verdict ${r.ok ? 'ok' : 'bad'}`);
  verdict.append(h('span', 'dot'), h('b', '', r.ok ? 'The panel checked it: ready to send.' : `The panel would refuse this (${r.errors.length}):`));
  box.append(verdict);
  if (!r.ok) {
    const ul = h('ul', 'v1-errors');
    for (const e of r.errors) { const li = h('li'); li.append(h('b', '', e.what ? `${e.where} · ${e.what}` : e.where), document.createTextNode(`: ${e.msg}`)); ul.append(li); }
    box.append(ul, h('p', 'dim', 'Fix these and review again. Nothing was sent; your changes are kept.'));
  }
  const acts = h('div', 'acts');
  const keep = h('button', 'btn ghost', 'Keep editing');
  keep.type = 'button';
  keep.onclick = () => d.close();
  const send = h('button', 'btn primary', 'Send');
  send.type = 'button';
  send.disabled = !r.ok || !ch.length;
  send.onclick = () => {
    panel.before.push({ rev: panel.rev, runs: panel.runs });
    panel.rev++;
    panel.runs = clone(draft);
    top();
    sent(d, ch.length);
  };
  acts.append(keep, send);
  box.append(acts, revertBar(d));
  d.replaceChildren(box);
  if (!d.open) d.showModal();
}
function sent(d: HTMLDialogElement, n: number) {
  const box = h('div', 'dlg');
  box.append(h('span', 'cap', DEVICES[device].name), h('h2', '', `Sent: the panel runs rev ${panel.rev}`));
  box.append(h('p', 'dim', `${n} change${n === 1 ? '' : 's'}: the deck (POST /fw/spec, kind deck) and the settings that changed (POST /fw/settings). On a real panel the screen changes now.`));
  const acts = h('div', 'acts');
  const ok = h('button', 'btn primary', 'Done');
  ok.type = 'button';
  ok.onclick = () => d.close();
  acts.append(ok);
  box.append(acts, revertBar(d));
  d.replaceChildren(box);
}
function revertBar(d: HTMLDialogElement) {
  const bar = h('div', 'v1-revert');
  bar.append(h('span', 'cap', 'Go back'));
  const undo = h('button', 'btn ghost', panel.before.length ? `Undo last send (back to rev ${panel.before.at(-1)!.rev})` : 'Undo last send');
  undo.type = 'button';
  undo.disabled = !panel.before.length;
  undo.onclick = () => {
    const b = panel.before.pop()!;
    panel.rev++;
    panel.runs = clone(b.runs);
    draft = clone(b.runs);
    applyDeck(); top(); render(); review();
  };
  const fac = h('button', 'btn ghost', 'Back to factory');
  fac.type = 'button';
  fac.onclick = () => {
    if (!confirm('Put the factory screens and settings back on the panel? What you sent is replaced.')) return;
    panel.before.push({ rev: panel.rev, runs: panel.runs });
    panel.rev++;
    panel.runs = factory();
    draft = factory();
    applyDeck(); top(); render(); review();
  };
  bar.append(undo, fac);
  return bar;
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
  const ok = h('button', 'btn primary', 'Close');
  ok.type = 'button';
  ok.onclick = () => d.close();
  acts.append(ok);
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
  $('v1-review').onclick = review;
  $('v1-scope').onclick = scope;
  (window as any).__v1 = { draft: () => draft, panel: () => panel, deck: () => toDeck(draft, panel.rev), check, changes: () => changes(panel.runs, draft) };  // tests (round 2)
  void load();
}
