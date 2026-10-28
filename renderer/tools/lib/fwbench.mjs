// Shared WASM bench (tools/behaviour.mjs, tools/functional.mjs): one fw.mjs/fw.wasm module per (build, density),
// a fresh instance per render (no state leaks), LVGL time in fixed 16 ms ticks, touch gestures, events, HA puts,
// the audit tree (fw_audit: every object's box, text, clickable, parent) and the frame (fw_frame) for PNGs.
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';
import { performance } from 'node:perf_hooks';
import { deflateSync } from 'node:zlib';

export const T = 16;  // ms per tick = LV_DEF_REFR_PERIOD
let BUILD_DIR = '';
export const setBuild = (d) => { BUILD_DIR = d; };

const mods = {};
export async function instance(den, build = BUILD_DIR) {
  if (!mods[build + den]) {
    const dir = join(build, den);
    mods[build + den] = { F: (await import(pathToFileURL(join(dir, 'fw.mjs')).href)).default, wasm: await WebAssembly.compile(readFileSync(join(dir, 'fw.wasm'))) };
  }
  const M = mods[build + den];
  const m = await M.F({ instantiateWasm: (imports, ok) => { WebAssembly.instantiate(M.wasm, imports).then((i) => ok(i, M.wasm)); return {}; } });
  m._fw_init();
  return m;
}

export class Bench {  // one module instance, one stage at a time
  constructor(m, den, { build = BUILD_DIR, long_ms = 500, drag_px = 24 } = {}) {
    this.build = build; this.long_ms = long_ms; this.drag_px = drag_px;
    this.m = m; this.den = den; this.S = den === '1x' ? 480 : 720; this.t = 0; this.stage = -1; this.rec = null;
    this.hasTest = !!m?._fw_test; this.hasBench = !!m?._fw_bench_take;
    if (this.hasBench) m._fw_bench_attach();
  }
  str(s, f) { const p = this.m.stringToNewUTF8(s); try { return f(p); } finally { this.m._free(p); } }
  tick(n = 1) {
    for (let i = 0; i < n; i++) {
      this.t += T;
      const w0 = performance.now();
      this.m._fw_tick(this.t);
      if (this.hasBench) {
        const s = new Float64Array(this.m.HEAPU8.buffer, this.m._fw_bench_take(), 9);
        if (this.rec && (s[0] || s[8])) this.rec.push({ at: this.t, renders: s[0], ms: Math.max(0, s[1] - s[3]), px: s[4], areas: s[5], full: s[7], over: s[8], anims: this.m._fw_anims(), work: performance.now() - w0 });
      }
    }
  }
  settle(max = 1500) {  // until no animation runs (or max ms: live things never settle)
    const end = this.t + max;
    let calm = 0;
    while (this.t < end && calm < 3) { this.tick(); calm = this.m._fw_anims() ? 0 : calm + 1; }
  }
  // every build gets a fresh module (2.7 ms): no state leaks between gestures (deck, settings, HA values, timers)
  async render(comp, props = {}, light = 0) {
    this.m = await instance(this.den, this.build);
    this.hasTest = !!this.m._fw_test; this.hasBench = !!this.m._fw_bench_take;
    this.t = 0;
    this.stage = this.frame ? this.m._fw_stage(this.frame[0], this.frame[1]) : this.m._fw_stage(this.S, this.S);  // frame: behaviour spec "frame"
    if (this.hasBench) this.m._fw_bench_attach();  // hooks the new display (idempotent), zeroes the counters
    const rec = this.rec; this.rec = null; this.tick(2); this.rec = rec;
    this.events();
    const e = this.str(comp, (c) => this.str(JSON.stringify(props), (p) => this.m.UTF8ToString(this.m._fw_render(this.stage, c, p, light, 0))));
    return e;
  }
  // build it again in this module (state kept: sent specs, the deck), as the device does after a deck change
  rebuild(comp, props = {}, light = 0) { const e = this.str(comp, (c) => this.str(JSON.stringify(props), (p) => this.m.UTF8ToString(this.m._fw_render(this.stage, c, p, light, 0)))); this.tick(2); this.events(); return e; }
  spec(cmd, a) { return this.str(cmd, (pc) => this.str(typeof a === 'string' ? a : JSON.stringify(a), (pa) => this.m.UTF8ToString(this.m._fw_spec(pc, pa)))); }
  update(comp, props, light = 0) { return this.str(comp, (c) => this.str(JSON.stringify(props), (p) => this.m.UTF8ToString(this.m._fw_update(this.stage, c, p, light, 0)))); }
  events() { const s = this.m.UTF8ToString(this.m._fw_events()); return s ? JSON.parse(s) : []; }
  audit() { return JSON.parse(this.m.UTF8ToString(this.m._fw_audit(this.stage))); }
  hits() { return JSON.parse(this.m.UTF8ToString(this.m._fw_hits(this.stage)) || '[]'); }
  test(cmd, a = '') { if (!this.hasTest) return null; const r = this.str(cmd, (c) => this.str(a, (p) => this.m.UTF8ToString(this.m._fw_test(c, p)))); return cmd === 'setting' ? r : JSON.parse(r || 'null'); }
  scrolls() { return this.test('scrolls') || this.audit().filter((o) => o.sc).map((o) => ({ ...o, sy: 0, sx: 0, t: 0, b: 1, l: 0, r: 0 })); }
  stack() { const s = this.test('stack'); return s && s[0] ? s[0].stack : null; }
  haPut(e, a, v) { this.str(e, (pe) => this.str(a, (pa) => this.str(v, (pv) => this.m._fw_ha_put(pe, pa, pv)))); }
  touch(x, y, down) { this.m._fw_touch(this.stage, Math.round(x), Math.round(y), down ? 1 : 0); }
  // gestures; each returns the events (incl. sounds) it caused, after the UI settles
  tap(x, y, hold = 64) { this.touch(x, y, 1); this.tick(Math.ceil(hold / T)); this.touch(x, y, 0); this.tick(2); this.settle(); return this.events(); }
  long(x, y) { return this.tap(x, y, this.long_ms); }
  drag(x, y, dx, dy, { steps = 0, hold = 48, after = true, onStep } = {}) {
    const n = steps || Math.max(2, Math.ceil(Math.hypot(dx, dy) / this.drag_px));
    this.touch(x, y, 1); this.tick(Math.ceil(hold / T));
    for (let i = 1; i <= n; i++) { this.touch(x + (dx * i) / n, y + (dy * i) / n, 1); this.tick(); onStep && onStep(i); }
    this.touch(x + dx, y + dy, 0); this.tick(2);
    if (after) this.settle(2000);
    return this.events();
  }
}

// RGBA frame of a stage -> PNG buffer (no dependency; same encoder as tools/corpus.mjs)
const crc = (() => { const t = new Int32Array(256); for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; t[n] = c; } return (b) => { let c = -1; for (const x of b) c = t[(c ^ x) & 255] ^ (c >>> 8); return (c ^ -1) >>> 0; }; })();
export function png(px, W, H) {
  const raw = Buffer.alloc((W * 4 + 1) * H);
  for (let y = 0; y < H; y++) { raw[y * (W * 4 + 1)] = 0; Buffer.from(px.buffer, px.byteOffset + y * W * 4, W * 4).copy(raw, y * (W * 4 + 1) + 1); }
  const chunk = (type, data) => { const l = Buffer.alloc(4); l.writeUInt32BE(data.length); const td = Buffer.concat([Buffer.from(type), data]); const c = Buffer.alloc(4); c.writeUInt32BE(crc(td)); return Buffer.concat([l, td, c]); };
  const ih = Buffer.alloc(13); ih.writeUInt32BE(W, 0); ih.writeUInt32BE(H, 4); ih[8] = 8; ih[9] = 6;
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ih), chunk('IDAT', deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
}
export function shot(b) {  // the stage as a PNG
  b.m._fw_refresh(b.stage);
  const p = b.m._fw_frame(b.stage), px = b.m.HEAPU8.slice(p, p + b.S * b.S * 4);
  for (let i = 3; i < px.length; i += 4) px[i] = 255;
  return png(px, b.S, b.S);
}
