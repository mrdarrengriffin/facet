// Live previews: every <figure data-live> on a page is a stage of ONE shared WASM engine (the same build the panels
// run), created when it scrolls near the viewport. The page-level switches (theme, device 720 / 480, accent set) live
// in the header (Base.astro) and re-render every stage. The engine module (~1 MB) loads only when the first preview
// comes into view.
import { Engine, type Stage } from '../lib/engine';
import { feedSampleHa } from '../lib/sample-ha';
import { getPrefs, onPrefs, type Prefs } from './prefs';
import { playUi } from './sound';
import site from '../data/site.json';

const ROOT = `${import.meta.env.BASE_URL.replace(/\/?$/, '/')}wasm/`;
const DEN = { 720: { den: '1.5x', d: 1.5, size: 720 }, 480: { den: '1x', d: 1, size: 480 } } as const;

type Item = { el: HTMLElement; comp: string; props: Record<string, unknown>; label: string; frame: [number, number]; doc: boolean; stage?: Stage; err?: HTMLElement;
  /** the playground stage (props from the controls, never reset) / a picture only (home strip, components overview) */
  sandbox?: boolean; still?: boolean;
  /** a screen spec's JSON to install before the first draw (Live spec) */
  spec?: string;
  /** a finger is down on the canvas */
  held?: boolean };

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

async function ensure(p: Prefs): Promise<Engine> {
  if (engine && loadedFor === p.device) return engine;
  if (loading && loadedFor === p.device) return loading;
  loadedFor = p.device;
  document.documentElement.dataset.live = 'loading';
  loading = Engine.load(DEN[p.device].den, ROOT).then((e) => {
    if (loadedFor !== p.device) { e.dispose(); return loading!; }
    engine?.dispose();
    for (const it of items) { it.stage = undefined; clearTimeout(it.timer); it.held = false; }
    engine = e;
    feedSampleHa(e);
    accentSets = e.tokens.accent.sets;
    e.onEvent = (ev, s) => {
      const it = s && items.find((i) => i.stage === s);
      if (ev.kind === 'sound') void playUi(ev.name, ev.detail);
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
  const p = getPrefs();
  const e = await ensure(p);
  draw(it, e, p);
}

async function redrawAll(p: Prefs) {
  if (!shown.size || (engine && loadedFor === p.device && lastLook === look(p))) return;
  lastLook = look(p);
  const e = await ensure(p);
  for (const it of shown) draw(it, e, p);
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
  (window as unknown as { __facetDocs: unknown }).__facetDocs = { engine: () => engine, items };
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
  onPrefs((p) => void redrawAll(p));
  // preview theme switches (Site / Dark / Light) of each box
  for (const sw of document.querySelectorAll<HTMLButtonElement>('[data-theme-set]')) {
    sw.addEventListener('click', () => {
      const scope = sw.closest<HTMLElement>('[data-preview-scope]');
      if (!scope) return;
      const v = sw.dataset.themeSet!;
      if (v === 'site') delete scope.dataset.previewTheme; else scope.dataset.previewTheme = v;
      for (const b of scope.querySelectorAll<HTMLButtonElement>('[data-theme-set]')) b.setAttribute('aria-pressed', String(b.dataset.themeSet === v));
      void redrawAll(getPrefs());
    });
  }
}

/** the playground: new props for a live example (redrawn now, from w x 1 again when its height is automatic) */
export function setLiveProps(el: HTMLElement, props: Record<string, unknown>) {
  const it = items.find((i) => i.el === el);
  if (!it) return;
  it.props = props;
  if (engine && it.stage) draw(it, engine, getPrefs(), true);
}
