// Facet WASM engine (copy of the hub workbench's src/wasm/engine.ts for docs site v0; the site passes its own build
// folder): loads a build (renderer/build.sh, copied to <base>wasm/<density>/ by scripts/extract.mjs) and draws
// component "stages" (one LVGL display each) into canvases. Mouse/touch -> fw_touch in device pixels.
export type Prop = { name: string; type: 'text' | 'select' | 'range' | 'bool'; default?: unknown; options?: string[]; min?: number; max?: number; step?: number };
export type Meta = { demo?: boolean; layout?: 'doc'; events?: boolean; frame: [number, number]; props?: Prop[]; variants?: { label: string; about?: string; props: Record<string, unknown> }[]; compare?: Compare[] };
/** meta "compare": debug size studies shown in the Pixel grid view (off = props, on = the on/second state), per density */
export type Compare = { density?: '1x' | '1.5x'; label: string; about?: string; props: Record<string, unknown>; on?: Record<string, unknown> };
/** fw_rects: every LVGL object under a stage's screen, device px as drawn (d = depth, 1 = the screen's children) */
export type Rect = { d: number; x: number; y: number; w: number; h: number };
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

export async function manifest(den: string, root = '/facet-wasm/'): Promise<Manifest> {
  return (await fetch(`${root}${den}/manifest.json`, { cache: 'no-cache' })).json();
}

export class Engine {
  m!: Mod;
  man!: Manifest;
  comps: Comp[] = [];
  tokens!: Tokens;
  views = new Set<Stage>();
  /** every fw::emit / fw::sound, with the stage it came from (drained once per frame) */
  onEvent: ((ev: FwEvent, stage: Stage | undefined) => void) | null = null;
  /** every animation frame: when it started, how long the WASM work took (LVGL tick + render + copy to the canvas),
   *  whether a stage repainted and how many LVGL animations run (browser targets' frame stats, src/wasm/device.ts) */
  onFrame: ((f: { t: number; work: number; painted: boolean; anims: number }) => void) | null = null;
  private alive = true;
  /** root = the folder holding <density>/fw.mjs (hub: /facet-wasm/, docs site: <base>wasm/) */
  static async load(den: string, root = '/facet-wasm/'): Promise<Engine> {
    const e = new Engine();
    e.man = await manifest(den, root);
    const v = encodeURIComponent(e.man.built), base = `${root}${den}/`;
    const { default: create } = await import(/* @vite-ignore */ `${base}fw.mjs?v=${v}`);
    e.m = await create({ locateFile: (f: string) => `${base}${f}?v=${v}` });
    e.m._fw_init();
    e.comps = JSON.parse(e.str(e.m._fw_meta()));
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
      if (evs && e.onEvent) for (const ev of JSON.parse(evs) as FwEvent[]) e.onEvent(ev, [...e.views].find((s) => s.id === ev.stage));
    };
    requestAnimationFrame(frame);
    return e;
  }
  str(p: number) { return this.m.UTF8ToString(p) as string; }
  withStr<T>(s: string, f: (p: number) => T): T { const p = this.m.stringToNewUTF8(s); try { return f(p); } finally { this.m._free(p); } }
  /** a stage of w x h device pixels drawing into its own canvas */
  stage(w: number, h: number) { const s = new Stage(this, w, h); this.views.add(s); return s; }
  dispose() { this.alive = false; for (const s of [...this.views]) s.free(); }
}

export class Stage {
  canvas = document.createElement('canvas');
  id: number;
  /** where this stage sits on the page (event log source), e.g. 'Preview' or a variant label */
  label = '';
  private g: CanvasRenderingContext2D;
  private img: ImageData;
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
  resize(w: number, h: number) {
    this.e.m._fw_stage_resize(this.id, w, h);
    this.w = w; this.h = h;
    this.canvas.width = w; this.canvas.height = h;
    this.img = this.g.createImageData(w, h);
  }
  /** copy the stage to its canvas if LVGL redrew it; true = it did */
  paint(): boolean {
    if (!this.e.m._fw_dirty(this.id)) return false;
    const p = this.e.m._fw_frame(this.id);
    this.img.data.set(this.e.m.HEAPU8.subarray(p, p + this.w * this.h * 4));
    this.g.putImageData(this.img, 0, 0);
    return true;
  }
  free() { this.e.m._fw_stage_free(this.id); this.e.views.delete(this); this.canvas.remove(); }
  /** the LVGL objects of the last build (Pixel grid view outlines); [] on an older build without fw_rects */
  rects(): Rect[] { return this.e.m._fw_rects ? JSON.parse(this.e.str(this.e.m._fw_rects(this.id))) : []; }
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
