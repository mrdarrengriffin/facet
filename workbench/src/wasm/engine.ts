// Facet WASM workbench engine: loads a build from /facet-wasm/<density>/ (renderer/build.sh) and draws
// component "stages" (one LVGL display each) into canvases. Mouse/touch -> fw_touch in device pixels.
import { setSoundVariant } from './sound';

export type Prop = { name: string; type: 'text' | 'select' | 'range' | 'bool' | 'entity'; about?: string; default?: unknown; options?: string[]; domains?: string[]; min?: number; max?: number; step?: number };
export type Meta = { demo?: boolean; opening?: string; layout?: 'doc'; events?: boolean; frame: [number, number]; props?: Prop[]; variants?: { label: string; about?: string; props: Record<string, unknown> }[]; compare?: Compare[] };
/** meta "compare": debug size studies shown in the Pixel grid view (off = props, on = the on/second state), per density */
export type Compare = { density?: '1x' | '1.5x'; label: string; about?: string; props: Record<string, unknown>; on?: Record<string, unknown> };
/** fw_rects: every LVGL object under a stage's screen, device px as drawn (d = depth, 1 = the screen's children) */
export type Rect = { d: number; x: number; y: number; w: number; h: number };
/** fw_comps: every component instance under a stage's screen (d = nesting, 1 = outermost; ids outer..inner), device px */
export type TapBox = { x: number; y: number; w: number; h: number; bx: number; by: number; bw: number; bh: number; min: number };
export type CompBox = { d: number; ids: string[]; x: number; y: number; w: number; h: number };
export type SlotBox = { n: string; x: number; y: number; w: number; h: number; empty?: boolean };
/** tokens/colour.json as the build saw it (fw_tokens) */
export type Tokens = { accent: { default: string; sets: { id: string; label: string }[] } };
/** fw::emit / fw::sound from a component (fw_events): kind event = the page's event log, sound = a UI sound id */
export type FwEvent = { stage: number; kind: 'event' | 'sound'; name: string; detail: string };
export type Comp = { id: string; name: string; section: string; meta: Meta };
export type Manifest = { built: string; density: string; build_s: number; wasm_bytes: number };
export type Zoom = '1' | 'fit' | '2';
export type DeviceId = 'waveshare' | 'guition' | 'shelly';
export const DEVICES: Record<DeviceId, { name: string; den: '1.5x' | '1x'; d: number; size: number }> = {
  waveshare: { name: 'Waveshare 720', den: '1.5x', d: 1.5, size: 720 },
  guition: { name: 'Guition 480', den: '1x', d: 1, size: 480 },
  shelly: { name: 'Shelly 720', den: '1.5x', d: 1.5, size: 720 },
};
// eslint-disable-next-line @typescript-eslint/no-explicit-any
type Mod = any;
/** one entity as the HA bridge returns it (the workbench's /api/panel/entities shape) */
export type HaState = { id: string; name: string; state: string; attrs?: Record<string, unknown> };
/** where bound components get Home Assistant data and send actions (core/ha.h). The workbench = hubHa; a host without HA
 *  (the editor served by a panel, for now) answers null = the components keep their sample data */
export type HaBridge = {
  states(ids: string[], attrs: string[]): Promise<HaState[] | null>;
  /** poll period in ms (default 3000); a change in the wanted entities fetches at once and restarts the period
   *  (the panel-served editor uses a slower one) */
  pollMs?: number;
  action(value: { service: string; entity: string; data?: Record<string, unknown> }): Promise<void>;
  /** an HA action whose reply a feed adapter reads (core/ha.h ask: calendar.get_events); null = no HA here */
  ask?(service: string, data: Record<string, unknown>): Promise<{ ok: boolean; response?: unknown } | null>;
};
export const hubHa: HaBridge = {
  async states(ids, attrs) {
    const r = await fetch(`/api/panel/entities?ids=${ids.join(',')}&attrs=${attrs.join(',')}`, { cache: 'no-store' });
    return r.ok ? r.json() : null;
  },
  async action(value) {
    await fetch('/api/panel/action', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ action: 'spec_call', value }) }).catch(() => undefined);
  },
  async ask(service, data) {
    const r = await fetch('/api/panel/ask', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ service, data }) }).catch(() => null);
    return r ? r.json().catch(() => null) : null;
  },
};

export async function manifest(den: string, base = `/facet-wasm/${den}/`): Promise<Manifest> {
  return (await fetch(`${base}manifest.json`, { cache: 'no-store' })).json();
}

export class Engine {
  m!: Mod;
  man!: Manifest;
  comps: Comp[] = [];
  tokens!: Tokens;
  views = new Set<Stage>();
  /** every fw::emit / fw::sound, with the stage it came from (drained once per frame) */
  onEvent: ((ev: FwEvent, stage: Stage | undefined) => void) | null = null;
  /** fw::emit(obj, "navigate", "<screen id>") (the screen chrome's settings button, Home's cards): open that screen */
  onNavigate: ((screen: string) => void) | null = null;
  /** every animation frame: when it started, how long the WASM work took (LVGL tick + render + copy to the canvas),
   *  whether a stage repainted and how many LVGL animations run (browser targets' frame stats, src/wasm/device.ts) */
  onFrame: ((f: { t: number; work: number; painted: boolean; anims: number }) => void) | null = null;
  private alive = true;
  ha: HaBridge = hubHa;
  /** base = where manifest.json / fw.mjs / fw.wasm live (the workbench's /facet-wasm/<den>/ unless a host says otherwise);
   *  ha = where HA states / actions go (the workbench unless a host says otherwise) */
  static async load(den: string, base = `/facet-wasm/${den}/`, ha: HaBridge = hubHa): Promise<Engine> {
    const e = new Engine();
    e.ha = ha;
    e.man = await manifest(den, base);
    const v = encodeURIComponent(e.man.built);
    const { default: create } = await import(/* @vite-ignore */ `${base}fw.mjs?v=${v}`);
    e.m = await create({ locateFile: (f: string) => `${base}${f}?v=${v}` });
    e.m._fw_init();
    // the panel's own HA link starts linked to the workbench's proxy (each /wasm page is a fresh module, so the pickers have
    // a list on every page); ?ha=off leaves it unset (the pickers' "not connected" state)
    if (e.m._fw_spec && new URLSearchParams(location.search).get('ha') !== 'off')
      e.withStr('act', (pa) => e.withStr(JSON.stringify({ do: 'ha.connect', url: location.origin }), (pj) => e.m._fw_spec(pa, pj)));
    e.comps =JSON.parse(e.str(e.m._fw_meta()));
    e.tokens = JSON.parse(e.str(e.m._fw_tokens()));
    const t0 = performance.now();
    const frame = () => {
      if (!e.alive) return;
      requestAnimationFrame(frame);
      const ft = performance.now();
      const anims = e.m._fw_anims ? (e.m._fw_anims() as number) : 0;
      e.m._fw_tick(Math.round(ft - t0));
      let painted = false;
      for (const s of e.views) painted = s.paint() || painted;
      e.onFrame?.({ t: ft, work: performance.now() - ft, painted, anims });
      const evs = e.m._fw_events ? e.str(e.m._fw_events()) : '';
      if (evs) for (const ev of JSON.parse(evs) as FwEvent[]) {
        e.onEvent?.(ev, [...e.views].find((s) => s.id === ev.stage));
        if (ev.kind === 'event' && ev.name === 'navigate' && ev.detail) e.onNavigate?.(ev.detail);
        // the device's Sound pack setting (core/device.h settings: "setting" soundPack=<id>) = the workbench pack
        if (ev.kind === 'event' && ev.name === 'setting' && ev.detail.startsWith('soundPack=')) setSoundVariant(ev.detail.slice(10));
        if (ev.kind === 'event' && ev.name === 'ha') void haAction(e.ha, ev.detail);
      }
      // feed adapters' asks (core/ha.h ask, CONTRACT C51): HA actions whose reply goes back with fw_ha_answer
      const asks = e.m._fw_ha_asks ? e.str(e.m._fw_ha_asks()) : '[]';
      if (asks !== '[]') for (const q of JSON.parse(asks) as { id: number; service: string; data: Record<string, unknown> }[]) void e.answer(q);
      // the panel's own HA link (core/device.h ha_link): its GET /api/states, here through the workbench with the workbench's token
      const gets = e.m._fw_ha_gets ? e.str(e.m._fw_ha_gets()) : '[]';
      // ... and its WebSocket reads (ha_link::ws: {id, ws: the command}), through /api/panel/ha-ws, reads only
      // ... and its REST reads (ha_link::get: {id, get: the path}), through /api/panel/ha-get, reads only
      if (gets !== '[]') for (const g of JSON.parse(gets) as (number | { id: number; ws?: unknown; get?: string })[]) void (typeof g === 'number' ? e.haStates(g) : g.get !== undefined ? e.haGet(g.id, g.get) : e.haWs(g.id, g.ws));
    };
    requestAnimationFrame(frame);
    if (e.m._fw_ha_wants) e.haLoop();
    if (e.m._fw_img_wants) e.imgLoop();
    return e;
  }
  /** Home Assistant data for bound components (core/ha.h): the wanted (entity, attribute) pairs' values from the HA
   *  bridge (the workbench: /api/panel/entities), put into the store every pollMs (3 s) and at once when the wants change (a screen opened, an entity picked), which restarts the period */
  private async haLoop() {
    let last = '', at = 0;
    this.haNow = () => { at = 0; };
    const period = this.ha.pollMs ?? 3000;
    while (this.alive) {
      const w = this.str(this.m._fw_ha_wants());
      if (w !== '[]' && (w !== last || Date.now() - at > period)) {
        last = w; at = Date.now();
        const pairs = JSON.parse(w) as [string, string][];
        const ids = [...new Set(pairs.map((p) => p[0]))], attrs = [...new Set(pairs.map((p) => p[1]).filter((a) => a && a !== 'friendly_name'))];
        try {
          const got = await this.ha.states(ids, attrs);
          if (got && this.alive) for (const s of got) {
            this.haPut(s.id, '', s.state);
            this.haPut(s.id, 'friendly_name', s.name);
            for (const a of attrs) this.haPut(s.id, a, s.attrs?.[a] == null ? 'None' : String(s.attrs[a]));
          }
        } catch { /* workbench or HA away: keep the last values */ }
      }
      await new Promise((res) => setTimeout(res, 500));
    }
  }
  /** Images (renderer core/images.h): the URLs the built images want, fetched through the workbench (/api/wasm/img: CORS,
   *  redirects, a plain-words reason when it fails), decoded by the browser and put as RGBA pixels (fw_img_put), or
   *  failed (fw_img_fail) with the HTTP status / "timeout" / "decode". The panel fetches like the workbench does (a plain HTTP
   *  client), so the proxy's answer is the panel's answer. Workbench nicety only: when the proxy fails, the browser
   *  tries the URL itself (crossOrigin, works when the host sends CORS headers and lets browser tabs through); a picture
   *  that only loads that way is marked via 'browser' (the panel would show the fallback). Every outcome goes to
   *  imgStatus + onImage (the playground's status line).  */
  imgStatus = new Map<string, ImgStatus>();
  onImage?: (src: string, s: ImgStatus) => void;
  private async imgLoop() {
    const decode = async (blob: Blob) => { try { return await createImageBitmap(blob); } catch { throw Object.assign(new Error('decode'), { why: "the file isn't a picture the browser can read" }); } };
    const direct = (src: string) => new Promise<ImageBitmap>((res, rej) => {
      const im = new Image();
      im.crossOrigin = 'anonymous';
      im.onload = () => createImageBitmap(im).then(res, rej);  // a host without CORS taints the canvas: rejected here
      im.onerror = () => rej(new Error('direct'));
      im.src = src;
    });
    const one = async (src: string) => {
      const status = (s: ImgStatus) => { this.imgStatus.set(src, s); this.onImage?.(src, s); };
      status({ state: 'loading' });
      let bm: ImageBitmap, via: ImgStatus['via'] = 'proxy';
      try {
        const r = await fetch(`/api/wasm/img?src=${encodeURIComponent(src)}`);
        if (!r.ok) {
          const why = r.headers.get('x-fw-reason') || (await r.text().catch(() => '')).slice(0, 160) || `HTTP ${r.status}`;
          throw Object.assign(new Error(r.status === 504 ? 'timeout' : String(r.status)), { why });
        }
        bm = await decode(await r.blob());
      } catch (e) {
        const code = (e as Error).message || 'failed', why = (e as { why?: string }).why || code;
        try { bm = await direct(src); via = 'browser'; } catch {
          if (this.alive) {
            this.withStr(src, (ps) => this.withStr(code, (pw) => this.m._fw_img_fail(ps, pw)));
            status({ state: 'failed', code, why });
          }
          return;
        }
        status({ state: 'loading', why });  // the browser got it (below): keep the proxy's reason for the note
      }
      const c = new OffscreenCanvas(bm.width, bm.height), g = c.getContext('2d')!;
      g.drawImage(bm, 0, 0);
      const px = g.getImageData(0, 0, bm.width, bm.height).data;
      if (!this.alive) return;
      const p = this.m._malloc(px.length);
      this.m.HEAPU8.set(px, p);
      this.withStr(src, (ps) => this.m._fw_img_put(ps, bm.width, bm.height, p));
      this.m._free(p);
      status({ state: 'ready', via, w: bm.width, h: bm.height, why: via === 'browser' ? this.imgStatus.get(src)?.why : undefined });
    };
    while (this.alive) {
      const w = JSON.parse(this.str(this.m._fw_img_wants()) || '[]') as string[];
      for (const src of w) void one(src);
      await new Promise((res) => setTimeout(res, 250));
    }
  }
  /** fetch the HA values now (the editor's Refresh button); the poll period restarts from it */
  haNow = () => {};
  private async answer(q: { id: number; service: string; data: Record<string, unknown> }) {
    const r = this.ha.ask ? await this.ha.ask(q.service, q.data).catch(() => null) : null;
    if (!this.alive) return;
    this.withStr(r?.ok ? JSON.stringify(r.response ?? {}) : '', (pj) => this.m._fw_ha_answer(q.id, r?.ok ? 1 : 0, pj));
  }
  /** stream HA's /api/states into the module piece by piece (fw_ha_chunk), as the panel's HTTP task does */
  private haStates(id: number) { return this.haStream(id, fetch('/api/panel/ha-states', { cache: 'no-store' })); }
  /** one HA WebSocket read (the workbench connects with its own token), its result message streamed in the same way */
  private haWs(id: number, cmd: unknown) {
    return this.haStream(id, fetch('/api/panel/ha-ws', { method: 'POST', cache: 'no-store', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(cmd) }));
  }
  /** one HA REST read (GET /api/<path>: an entity's state, the calendars, a calendar's events), streamed the same way */
  private haGet(id: number, path: string) { return this.haStream(id, fetch(`/api/panel/ha-get?path=${encodeURIComponent(path)}`, { cache: 'no-store' })); }
  private async haStream(id: number, req: Promise<Response>) {
    let ok = 0, why = '';
    try {
      const r = await req;
      if (!r.ok || !r.body) why = (await r.text().catch(() => '')).slice(0, 120) || `Home Assistant answered ${r.status}`;
      else {
        const rd = r.body.getReader();
        for (;;) {
          const { done, value } = await rd.read();
          if (done || !this.alive) break;
          for (let i = 0; i < value.length; i += 4096) {  // the panel's read size
            const part = value.subarray(i, i + 4096), p = this.m._malloc(part.length);
            this.m.HEAPU8.set(part, p);
            try { this.m._fw_ha_chunk(id, p, part.length); } finally { this.m._free(p); }
          }
        }
        ok = 1;
      }
    } catch (err) { why = String((err as Error)?.message || err); }
    if (this.alive) this.withStr(why, (pw) => this.m._fw_ha_got(id, ok, pw));
  }
  private haPut(e: string, a: string, v: string) { this.withStr(e, (pe) => this.withStr(a, (pa) => this.withStr(v, (pv) => this.m._fw_ha_put(pe, pa, pv)))); }
  str(p: number) { return this.m.UTF8ToString(p) as string; }
  withStr<T>(s: string, f: (p: number) => T): T { const p = this.m.stringToNewUTF8(s); try { return f(p); } finally { this.m._free(p); } }
  /** a stage of w x h device pixels drawing into its own canvas */
  stage(w: number, h: number) { const s = new Stage(this, w, h); this.views.add(s); return s; }
  dispose() { this.alive = false; for (const s of [...this.views]) s.free(); }
}

/** one image URL's outcome on the workbench (Engine.imgStatus): via 'browser' = only the browser tab could load it */
export type ImgStatus = { state: 'loading' | 'ready' | 'failed'; via?: 'proxy' | 'browser'; code?: string; why?: string; w?: number; h?: number };

export class Stage {
  canvas = document.createElement('canvas');
  id: number;
  /** where this stage sits on the page (event log source), e.g. 'Preview' or a variant label */
  label = '';
  private g: CanvasRenderingContext2D;
  private img: ImageData;
  private full = true;  // next paint copies the whole stage (new or resized)
  constructor(private e: Engine, public w: number, public h: number) {
    this.id = e.m._fw_stage(w, h);
    const c = this.canvas;
    c.width = w; c.height = h; c.className = 'wb-canvas';
    c.style.touchAction = 'none';
    this.g = c.getContext('2d')!;
    this.img = this.g.createImageData(w, h);
    const xy = (ev: PointerEvent) => { const r = c.getBoundingClientRect(); return [Math.round(((ev.clientX - r.left) * this.w) / r.width), Math.round(((ev.clientY - r.top) * this.h) / r.height)]; };
    // LVGL reads the pointer on its indev timer (~33 ms): hold a release until the press is >= 70 ms old
    let downAt = 0;
    const touch = (x: number, y: number, d: number) => e.m._fw_touch(this.id, x, y, d);
    c.addEventListener('pointerdown', (ev) => { c.setPointerCapture(ev.pointerId); downAt = performance.now(); const [x, y] = xy(ev); touch(x, y, 1); });
    c.addEventListener('pointermove', (ev) => { if (ev.buttons) { const [x, y] = xy(ev); touch(x, y, 1); } });
    const up = (ev: PointerEvent) => { const [x, y] = xy(ev); const wait = 70 - (performance.now() - downAt); if (wait > 0) setTimeout(() => touch(x, y, 0), wait); else touch(x, y, 0); };
    c.addEventListener('pointerup', up);
    c.addEventListener('pointercancel', up);
  }
  /** build component `comp` with props; accent = accent set index (-1 = default); "" = ok, else the error.
   *  autoH: the stage height follows the content (meta frame h = 0): build, measure, resize, build again;
   *  minH: autoH never goes below this (a device screen preview is at least the screen's height) */
  render(comp: string, props: Record<string, unknown>, light: boolean, accent = -1, autoH = false, minH = 1): string {
    const e = this.e;
    const go = () => e.withStr(comp, (c) => e.withStr(JSON.stringify(props), (p) => e.str(e.m._fw_render(this.id, c, p, light ? 1 : 0, accent))));
    let err = go();
    if (!err && autoH) {
      const h = Math.max(minH, 1, e.m._fw_content_h(this.id));
      if (h !== this.h) { this.resize(this.w, h); err = go(); }
    }
    return err;
  }
  /** the update path (facet-wasm README "Update path"): new props for what this stage shows, in place, with the
   *  component's own animation, silent. "" = applied, "rebuild" = call render() (another component / theme / accent,
   *  no update path yet, a structural prop; also an older build without fw_update), else an error */
  update(comp: string, props: Record<string, unknown>, light: boolean, accent = -1): string {
    const e = this.e;
    if (!e.m._fw_update) return 'rebuild';
    return e.withStr(comp, (c) => e.withStr(JSON.stringify(props), (p) => e.str(e.m._fw_update(this.id, c, p, light ? 1 : 0, accent))));
  }
  /** demos on this stage (fw::demo_state): -1 none reported, 0 every demo at its preset (a replay that ended where it
   *  started: skip the demo reset), 1 one moved away; -1 on an older build */
  demoState(): number { return this.e.m._fw_demo_state ? (this.e.m._fw_demo_state(this.id) as number) : -1; }
  /** update in place when the component can, else render(); autoH: a new content height = render() (same args as render) */
  apply(comp: string, props: Record<string, unknown>, light: boolean, accent = -1, autoH = false, minH = 1): string {
    const u = this.update(comp, props, light, accent);
    if (u === '' && (!autoH || Math.max(minH, 1, this.e.m._fw_content_h(this.id)) === this.h)) return '';
    if (u !== '' && u !== 'rebuild') return u;
    return this.render(comp, props, light, accent, autoH, minH);
  }
  resize(w: number, h: number) {
    this.e.m._fw_stage_resize(this.id, w, h);
    this.w = w; this.h = h;
    this.canvas.width = w; this.canvas.height = h;
    this.img = this.g.createImageData(w, h);
    this.full = true;
  }
  /** copy the stage to its canvas if LVGL redrew it; true = it did */
  paint(): boolean {
    if (!this.e.m._fw_dirty(this.id)) return false;
    const p = this.e.m._fw_frame(this.id), H: Uint8Array = this.e.m.HEAPU8, W = this.w;
    // only the areas LVGL flushed (fw_areas), not the whole stage every frame (a page of animated icons copied
    // ~3 MB per frame at 720 x 1079); the whole stage after a resize, on an older build or when most of it changed
    const ap = this.e.m._fw_areas ? (this.e.m._fw_areas(this.id) as number) : 0;
    const A = ap ? new Int32Array(H.buffer, ap, 1 + 4 * new Int32Array(H.buffer, ap, 1)[0]) : null;
    let px = 0;
    if (A) for (let i = 0; i < A[0]; i++) px += (A[3 + 4 * i] - A[1 + 4 * i] + 1) * (A[4 + 4 * i] - A[2 + 4 * i] + 1);
    if (!A || this.full || px > (W * this.h) / 2) {
      this.full = false;
      this.img.data.set(H.subarray(p, p + W * this.h * 4));
      this.g.putImageData(this.img, 0, 0);
      return true;
    }
    for (let i = 0; i < A[0]; i++) {
      const x1 = Math.max(0, A[1 + 4 * i]), y1 = Math.max(0, A[2 + 4 * i]), x2 = Math.min(W - 1, A[3 + 4 * i]), y2 = Math.min(this.h - 1, A[4 + 4 * i]);
      if (x2 < x1 || y2 < y1) continue;
      for (let y = y1; y <= y2; y++) this.img.data.set(H.subarray(p + (y * W + x1) * 4, p + (y * W + x2 + 1) * 4), (y * W + x1) * 4);
      this.g.putImageData(this.img, 0, 0, x1, y1, x2 - x1 + 1, y2 - y1 + 1);
    }
    return true;
  }
  free() { this.e.m._fw_stage_free(this.id); this.e.views.delete(this); this.canvas.remove(); }
  /** the LVGL objects of the last build (Pixel grid view outlines); [] on an older build without fw_rects */
  /** every tap target (core fw_taps): hit area x/y/w/h (box + ext click area), the drawn box b*, touch:min now; [] on older builds */
  taps(): TapBox[] { return this.e.m._fw_taps ? JSON.parse(this.e.str(this.e.m._fw_taps(this.id))) : []; }
  rects(): Rect[] { return this.e.m._fw_rects ? JSON.parse(this.e.str(this.e.m._fw_rects(this.id))) : []; }
  /** the component instances of the current screen (Components view outlines); [] on a build without fw_comps */
  comps(): CompBox[] { return this.e.m._fw_comps ? JSON.parse(this.e.str(this.e.m._fw_comps(this.id))) : []; }
  /** meta slots (CONTRACT C8a) shown on this stage: where other components may go (fx::slot; older builds: none) */
  slots(): SlotBox[] { return this.e.m._fw_slots ? JSON.parse(this.e.str(this.e.m._fw_slots(this.id))) : []; }
}

/** CSS size for a w x h canvas in a box: whole-number upscale in SCREEN pixels when it fits (crisp), else shrink */
export function fit(w: number, h: number, bw: number, bh: number) {
  const dpr = window.devicePixelRatio || 1;
  let s = Math.min((bw * dpr) / w, (bh * dpr) / h);
  if (s >= 1) s = Math.floor(s);
  return { width: (w * s) / dpr, height: (h * s) / dpr, scale: s };
}

/** CSS size at a fixed zoom: each panel pixel = `z` physical screen pixels (CSS px = panel px * z / dpr); z=1 = true scale */
export function zoomSize(w: number, h: number, z: number) {
  const dpr = window.devicePixelRatio || 1;
  return { width: (w * z) / dpr, height: (h * z) / dpr, scale: z };
}

/** a component's HA action (core/ha.h act: light.turn_on [+ brightness_pct], light.turn_off, light.toggle) through the
 *  HA bridge (the workbench: its whitelisted spec_call), as the panel would call it; the next state poll shows the result */
async function haAction(ha: HaBridge, detail: string) {
  let a: { service: string; entity: string; data?: Record<string, unknown> | null };
  try { a = JSON.parse(detail); } catch { return; }
  const value = a.data && Object.keys(a.data).length ? { service: a.service, entity: a.entity, data: a.data }
    : { service: a.service.replace(/^light\./, 'homeassistant.'), entity: a.entity };
  await ha.action(value).catch(() => undefined);
}
