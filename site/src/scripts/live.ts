// Live previews: every <figure data-live> on a page is a stage of ONE shared WASM engine (the same build the panels
// run), created when it scrolls near the viewport. The page-level switches (theme, device 720 / 480, accent set) live
// in the header (Base.astro) and re-render every stage. The engine module (~1 MB) loads only when the first preview
// comes into view.
import { Engine, type Stage } from '../lib/engine';
import { fakeHaEvent, fakeHaState, feedSampleHa } from '../lib/sample-ha';
import { getPrefs, livePrefs, onPrefs, setPrefs, type Prefs } from './prefs';
import { playUi, setVolume } from './sound';
import site from '../data/site.json';

const ROOT = `${import.meta.env.BASE_URL.replace(/\/?$/, '/')}wasm/`;
const DEN = { 720: { den: '1.5x', d: 1.5, size: 720 }, 480: { den: '1x', d: 1, size: 480 } } as const;

type Item = { el: HTMLElement; comp: string; props: Record<string, unknown>; label: string; frame: [number, number]; doc: boolean; stage?: Stage; err?: HTMLElement;
  /** the playground stage (props from the controls, never reset) / a picture only (home strip, components overview) */
  sandbox?: boolean; still?: boolean;
  /** a screen spec's JSON to install before the first draw (Live spec) */
  spec?: string;
  /** a finger is down on the canvas */
  held?: boolean;
  /** the screen the stage shows now (its "screen" events; a fake HA reaction is per screen) */
  screen?: string };

// No demo reset (Darren 2026-10-25): a demo keeps its state until the page is left (it was rebuilt with its original
// props a moment after the last interaction).
function watchGestures(it: Item, c: HTMLCanvasElement) {
  c.addEventListener('pointerdown', () => { it.held = true; });
  const up = () => { it.held = false; };
  c.addEventListener('pointerup', up);
  c.addEventListener('pointercancel', up);
}

let engine: Engine | null = null;
let loading: Promise<Engine> | null = null;
let loadedFor = 0;
const look = (p: Prefs) => `${p.theme}/${p.device}/${p.accent}/${[...document.querySelectorAll<HTMLElement>('[data-preview-scope]')].map((x) => x.dataset.previewTheme ?? '').join()}`;
/** the theme an example is drawn in: its box's preview theme switch (Site / Dark / Light), else the site's */
function themeOf(it: Item, p: Prefs): 'dark' | 'light' {
  const t = it.el.closest<HTMLElement>('[data-preview-scope]')?.dataset.previewTheme;
  return t === 'dark' || t === 'light' ? t : p.theme;
}
let lastLook = '';
let accentSets: { id: string }[] = [];
const items: Item[] = [];
// specs installed per engine (a new engine = a new panel: install again)
const installed = new WeakMap<Engine, Set<string>>();
/** a spec item's screen spec, sent to the engine as a panel takes one (POST /fw/spec kind screen); '' or the error */
function install(e: Engine, it: Item): string {
  if (!it.spec) return '';
  let done = installed.get(e);
  if (!done) installed.set(e, (done = new Set()));
  const id = String(it.props.screen ?? '');
  if (done.has(id)) return '';
  const m = e.m as unknown as { _fw_spec: (a: number, b: number) => number };
  const r = JSON.parse(e.withStr('screen', (pa) => e.withStr(it.spec!, (pb) => e.str(m._fw_spec(pa, pb))))) as { ok: boolean; status?: string; errors?: { path: string; code: string }[] };
  if (!r.ok) return `spec ${r.errors?.map((x) => `${x.path} ${x.code}`).join(', ') ?? r.status}`;
  done.add(id);
  return '';
}
const shown = new Set<Item>();

function accentIdx(p: Prefs) { return accentSets.findIndex((a) => a.id === p.accent); }

// ---- the live demo's Settings <-> the site prefs (Darren 2026-10-25: demo settings did not stick). The panel's
// settings store (core/device.h, one per engine) is the source while you use the demo: a change there ("setting"
// id=value) updates the site prefs (theme, accent, sounds, sound pack) or, for the rest, this browser's demo-only
// values (kept and set again on the next engine / page); a site pref change is set on the panel (fw_setting, HOST).
// Device stages are never rendered again for a pref change: the device restyles itself, so its screen stays put.
const SHARED: Record<string, keyof Prefs> = { theme: 'theme', accent: 'accent', sounds: 'sound', soundPack: 'pack' };
const OWN_KEY = 'facet-docs-panel';
let pushed: Record<string, string> = {};
function panelOwn(): Record<string, string> {
  try { return JSON.parse(localStorage.getItem(OWN_KEY) ?? '{}') as Record<string, string>; } catch { return {}; }
}
function setting(e: Engine, id: string, value: string | null): string {
  const m = e.m as unknown as { _fw_setting?: (a: number, b: number) => number };
  if (!m._fw_setting) return 'old build';
  if (value === null) return e.withStr(id, (pa) => e.str(m._fw_setting!(pa, 0)));
  if (id === 'volume') setVolume(Number(value));
  return e.withStr(id, (pa) => e.withStr(value, (pb) => e.str(m._fw_setting!(pa, pb))));
}
/** a stage that is a whole panel (the Device component, a live spec stack): its look follows the panel settings */
const isDevice = (i: Item) => i.comp === 'device' || i.props.live === true;
/** the Theme the panel is set to: the panels' common theme, or auto when they differ (an example page's dark + light
 *  pair: each then shows its own stage theme, so one shared settings store cannot force them the same) */
function panelTheme(p: Prefs): string {
  const ts = new Set(items.filter(isDevice).map((i) => themeOf(i, p)));
  return ts.size === 1 ? [...ts][0] : ts.size ? 'auto' : p.theme;
}
/** the site prefs onto the panel: only what changed since the last push (a demo Theme Auto stays Auto) */
function pushPrefs(e: Engine, p: Prefs) {
  const want: Record<string, string> = { theme: panelTheme(p), accent: p.accent, sounds: String(p.sound), soundPack: p.pack };
  for (const [k, v] of Object.entries(want)) {
    if (!v || pushed[k] === v) continue;
    pushed[k] = v;
    setting(e, k, v);
  }
}
function fromPanel(detail: string) {
  const eq = detail.indexOf('=');
  if (eq < 0) return;
  const id = detail.slice(0, eq), v = detail.slice(eq + 1);
  const pref = SHARED[id];
  if (!pref) {
    if (id === 'volume') setVolume(Number(v));
    const own = panelOwn();
    own[id] = v;
    try { localStorage.setItem(OWN_KEY, JSON.stringify(own)); } catch { /* private mode: this page only */ }
    return;
  }
  pushed[id] = v;
  const cur = getPrefs();
  if (pref === 'theme') { if ((v === 'dark' || v === 'light') && v !== cur.theme && panelTheme(cur) !== 'auto') setPrefs({ theme: v }); }
  else if (pref === 'accent') { if (v !== cur.accent && accentSets.some((a) => a.id === v)) setPrefs({ accent: v }); }
  else if (pref === 'sound') { if ((v === 'true') !== cur.sound) setPrefs({ sound: v === 'true' }); }
  else if (v !== cur.pack) setPrefs({ pack: v });
}

async function ensure(p: Prefs): Promise<Engine> {
  if (engine && loadedFor === p.device) return engine;
  if (loading && loadedFor === p.device) return loading;
  loadedFor = p.device;
  document.documentElement.dataset.live = 'loading';
  loading = Engine.load(DEN[p.device].den, ROOT).then((e) => {
    if (loadedFor !== p.device) { e.dispose(); return loading!; }
    engine?.dispose();
    for (const it of items) { it.stage = undefined; clearTimeout(it.timer); it.held = false; it.screen = undefined; }
    engine = e;
    feedSampleHa(e);
    accentSets = e.tokens.accent.sets;
    pushed = {};
    for (const [k, v] of Object.entries(panelOwn())) setting(e, k, v);  // this browser's demo-only settings
    pushPrefs(e, p);
    e.onEvent = (ev, s) => {
      if (ev.kind === 'sound') void playUi(ev.name, ev.detail);
      if (ev.kind === 'event' && ev.name === 'setting') fromPanel(ev.detail);
      if (ev.kind === 'event') {  // the fake HA (lib/sample-ha.ts): an action or a screen's reaction changes the demo entities
        const it = items.find((i) => i.stage === s);
        if (it && ev.name === 'screen') it.screen = ev.detail;
        else if (it) fakeHaEvent(it.screen ?? String(it.props.screen ?? it.props.start ?? 'home'), ev.name, ev.detail);
      }
      log(ev.kind, ev.name, ev.detail, s?.label ?? '');
    };
    document.documentElement.dataset.live = 'ready';
    return e;
  });
  return loading;
}

/** reset = a demo revert / new playground props: the update path first (in place, animated, silent); else repeat the
 *  first render exactly (an auto-height stage measures from its current size, so it starts again from w x 1 like a
 *  fresh stage) */
function draw(it: Item, e: Engine, p: Prefs, reset = false) {
  const dd = DEN[p.device];
  const w = it.doc ? dd.size : Math.round(it.frame[0] * dd.d);
  const auto = it.frame[1] === 0;
  const h = auto ? 1 : Math.round(it.frame[1] * dd.d);
  if (reset && it.stage && it.stage.w === w && it.stage.update(it.comp, it.props, themeOf(it, p) === 'light', accentIdx(p)) === ''
    && (!auto || it.stage.contentH() === it.stage.h)) return;
  if (!it.stage) {
    it.stage = e.stage(w, h);
    it.stage.label = it.label;
    it.stage.canvas.className = 'live__canvas';
    watchGestures(it, it.stage.canvas);
    it.el.querySelector('.live__stage')!.replaceChildren(it.stage.canvas);
  } else if (it.stage.w !== w || it.stage.h !== h && (!auto || reset)) it.stage.resize(w, h);
  const err = install(e, it) || it.stage.render(it.comp, it.props, themeOf(it, p) === 'light', accentIdx(p), auto, 1);
  it.stage.canvas.style.width = `${it.stage.w}px`;
  it.el.style.setProperty('--w', `${it.stage.w}px`);
  it.el.classList.toggle('live--err', !!err);
  const cap = it.el.querySelector('figcaption');
  if (err && cap) cap.textContent = `${it.label}: ${err}`;
}

async function show(it: Item) {
  shown.add(it);
  const p = livePrefs();
  const e = await ensure(p);
  draw(it, e, p);
}

async function redrawAll(p: Prefs) {
  if (!shown.size || (engine && loadedFor === p.device && lastLook === look(p))) return;
  lastLook = look(p);
  const e = await ensure(p);
  const own = panelTheme(p) !== 'auto';
  for (const it of shown) if (!(own && isDevice(it) && it.stage)) draw(it, e, p);  // a panel restyles itself (pushPrefs)
}

// event log: the page's [data-events] box (components and the States foundation)
const logBox = () => document.querySelector<HTMLElement>('[data-events] ol');
function log(kind: string, name: string, detail: string, from: string) {
  const ol = logBox();
  if (!ol) return;
  const li = document.createElement('li');
  const k = document.createElement('span');
  k.className = `ev ev--${kind}`;
  k.textContent = kind === 'sound' ? 'sound' : 'event';
  const t = document.createElement('code');
  t.textContent = detail ? `${name} (${detail})` : name;
  const f = document.createElement('span');
  f.className = 'ev__from';
  f.textContent = from;
  li.append(k, t, f);
  ol.prepend(li);
  while (ol.children.length > 8) ol.lastElementChild!.remove();
  ol.closest('[data-events]')!.classList.add('has-events');
}

export function initLive() {
  // debugging / tests: the page's engine and previews
  (window as unknown as { __facetDocs: unknown }).__facetDocs = { engine: () => engine, items, ha: fakeHaState, setting: (id: string, v: string | null = null) => (engine ? setting(engine, id, v) : '') };
  const io = new IntersectionObserver((xs) => {
    for (const x of xs) if (x.isIntersecting) { io.unobserve(x.target); const it = items.find((i) => i.el === x.target); if (it) void show(it); }
  }, { rootMargin: '400px 0px' });
  for (const el of document.querySelectorAll<HTMLElement>('figure[data-live]')) {
    const it: Item = {
      el,
      comp: el.dataset.comp!,
      props: JSON.parse(el.dataset.props ?? '{}'),
      label: el.dataset.label ?? '',
      frame: JSON.parse(el.dataset.frame ?? '[240,0]'),
      doc: el.dataset.doc === '1',
      sandbox: el.dataset.sandbox === '1',
      still: el.dataset.still === '1',
      spec: el.dataset.spec,
    };
    items.push(it);
    io.observe(el);
  }
  onPrefs(() => { const p = livePrefs(); if (engine && loadedFor === p.device) pushPrefs(engine, p); void redrawAll(p); });
  // preview theme switches (Site / Dark / Light) of each box
  for (const sw of document.querySelectorAll<HTMLButtonElement>('[data-theme-set]')) {
    sw.addEventListener('click', () => {
      const scope = sw.closest<HTMLElement>('[data-preview-scope]');
      if (!scope) return;
      const v = sw.dataset.themeSet!;
      if (v === 'site') delete scope.dataset.previewTheme; else scope.dataset.previewTheme = v;
      for (const b of scope.querySelectorAll<HTMLButtonElement>('[data-theme-set]')) b.setAttribute('aria-pressed', String(b.dataset.themeSet === v));
      const p = livePrefs();
      if (engine && loadedFor === p.device) pushPrefs(engine, p);
      void redrawAll(p);
    });
  }
}

/** the playground: new props for a live example (redrawn now, from w x 1 again when its height is automatic) */
export function setLiveProps(el: HTMLElement, props: Record<string, unknown>) {
  const it = items.find((i) => i.el === el);
  if (!it) return;
  it.props = props;
  if (engine && it.stage) draw(it, engine, livePrefs(), true);
}
