#!/usr/bin/env node
// Functional tests (tests/functional/README.md): end-to-end checks that each component and screen DOES what it should,
// read back from what it renders. Behaviour tests (tools/behaviour.mjs) sweep every id for generic rules (one sound
// per tap, disabled fires nothing, scroll limits, lag); these are written by hand per component: a slider clamps at
// max, a light tile's state line follows the live HA state after a toggle, a sensor readout comes back after a
// reconnect. Headless on the WASM build (no browser): mount a component or the device, script a fake Home Assistant
// (states, unavailable, disconnect / reconnect, auto-reacting to service calls), touch it (tap, hold, drag), read the
// texts, boxes, events and sounds back.
// Usage: node renderer/tools/functional.mjs [--den 1.5x|1x|all] [--ids a,b] [--grep text] [--png fail|all|none]
//        [--out dir] [--json file] [--soft] [-v] [--probe comp '{props}']   exit 1 = a test failed (unless --soft)
// Results: tests/functional/out/results.json + per-test PNGs (default: failures only) in tests/functional/out/png/.
import { readFileSync, writeFileSync, existsSync, readdirSync, mkdirSync, rmSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { performance } from 'node:perf_hooks';
import { Bench, setBuild, shot, T } from './lib/fwbench.mjs';
import { demoValue } from './demo_ha.mjs';

const HERE = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const argv = process.argv;
const arg = (k, d) => { const i = argv.indexOf(k); return i > 0 ? argv[i + 1] : d; };
const DIR = join(HERE, 'tests/functional'), OUT = resolve(arg('--out', join(DIR, 'out')));
const env = (() => { try { return Object.fromEntries(readFileSync(join(HERE, '..', 'local.env'), 'utf8').split('\n').map((l) => l.match(/^([A-Z_]+)=(.*)$/)).filter(Boolean).map((m) => [m[1], m[2]])); } catch { return {}; } })();
const BUILD = process.env.FW_OUT || env.FW_OUT || join(HERE, 'out');
setBuild(BUILD);
const B = JSON.parse(readFileSync(join(HERE, 'tests/behaviour/_budgets.json'), 'utf8'));
const DENS = (arg('--den', 'all') === 'all' ? ['1.5x', '1x'] : [arg('--den')]).filter((d) => existsSync(join(BUILD, d, 'fw.wasm')));
const ONLY = arg('--ids') ? arg('--ids').split(',') : null, GREP = arg('--grep'), PNG = arg('--png', 'fail');
const VERBOSE = argv.includes('-v');

// ---------------------------------------------------------------- fake Home Assistant
// A store of entity states the device reads through fw_ha_put. Values the store lacks fall back to tools/demo_ha.mjs.
// auto: service calls the device sends ("ha" events) change the store the way HA would, and the new state arrives on
// the next frame (lag: ms before it arrives, to test optimistic UI). Disconnect = every bound entity reads
// "unavailable" (what the panel sees when HA drops); reconnect = the stored states arrive again.
const DOMAIN_ON = { light: 1, switch: 1, fan: 1, input_boolean: 1 };
class FakeHA {
  constructor(t) { this.t = t; this.s = {}; this.calls = []; this.auto = true; this.lag = 0; this.down = false; this.pending = []; }
  get(e, a = '') { const st = this.s[e]; if (st && a in st) return st[a] ?? ''; return demoValue(e, a); }  // null = HA dropped it -> ''
  wants() { return JSON.parse(this.t.b.m.UTF8ToString(this.t.b.m._fw_ha_wants()) || '[]'); }
  push(only) {  // send the bound pairs (all, or one entity's) to the device
    for (const [e, a] of this.wants()) {
      if (only && e !== only) continue;
      const v = this.down ? (a ? undefined : 'unavailable') : this.get(e, a);
      if (v !== undefined) this.t.b.haPut(e, a, String(v));
    }
  }
  set(e, state, attrs = {}) {  // HA reports a new state (and attributes) for e
    this.s[e] = { ...(this.s[e] || {}), ...(state === undefined ? {} : { '': String(state) }), ...Object.fromEntries(Object.entries(attrs).map(([k, v]) => [k, v === null ? null : String(v)])) };
    this.t.log.push(`ha: ${e} = ${state ?? ''}${Object.keys(attrs).length ? ' ' + JSON.stringify(attrs) : ''}`);
    this.push(e); this.t.b.settle(1500);
  }
  unavailable(e) { this.set(e, 'unavailable'); }
  disconnect() { this.down = true; this.t.log.push('ha: disconnected'); this.push(); this.t.b.settle(1500); }
  reconnect() { this.down = false; this.t.log.push('ha: reconnected'); this.push(); this.t.b.settle(1500); }
  react(evs) {  // the HA side of service calls in a gesture's events
    for (const ev of evs.filter((x) => x.kind === 'event' && x.name === 'ha')) {
      let d; try { d = JSON.parse(ev.detail); } catch { continue; }
      this.calls.push(d);
      this.t.log.push(`ha call: ${d.service} ${d.entity || ''}${d.data ? ' ' + JSON.stringify(d.data) : ''}`);
      if (!this.auto || !d.entity) continue;
      const [dom, svc] = d.service.split('.'), ed = d.entity.split('.')[0];
      const was = this.get(d.entity), data = d.data || {};
      let next = null, attrs = {};
      if (DOMAIN_ON[ed] && /^(toggle|turn_on|turn_off)$/.test(svc) && (dom === ed || dom === 'homeassistant')) {
        next = svc === 'toggle' ? (was === 'on' ? 'off' : 'on') : svc === 'turn_on' ? 'on' : 'off';
        if (ed === 'light') {
          if (next === 'off') attrs.brightness = null;  // HA drops brightness when a light is off
          else if (data.brightness_pct !== undefined) attrs.brightness = Math.round(data.brightness_pct * 2.55);
          else if (data.brightness !== undefined) attrs.brightness = data.brightness;
          else attrs.brightness = this.s[d.entity]?.last_brightness ?? demoValue(d.entity, 'brightness') ?? 255;
          if (next === 'off' && this.get(d.entity, 'brightness')) attrs.last_brightness = this.get(d.entity, 'brightness');
        }
      } else if (ed === 'cover' && dom === 'cover') {
        if (svc === 'open_cover') { next = 'open'; attrs.current_position = 100; }
        else if (svc === 'close_cover') { next = 'closed'; attrs.current_position = 0; }
        else if (svc === 'set_cover_position') { attrs.current_position = data.position; next = data.position > 0 ? 'open' : 'closed'; }
        else if (svc === 'set_cover_tilt_position') attrs.current_tilt_position = data.tilt_position;
      } else if (ed === 'vacuum' && dom === 'vacuum') {  // start / a room clean -> cleaning, pause, dock, suction
        next = { start: 'cleaning', send_command: 'cleaning', pause: 'paused', return_to_base: 'returning' }[svc] ?? null;
        if (svc === 'set_fan_speed') attrs.fan_speed = data.fan_speed;
      } else if (ed === 'select' && dom === 'select' && svc === 'select_option') next = data.option;
      if (next === null && !Object.keys(attrs).length) continue;
      const apply = () => this.set(d.entity, next ?? undefined, attrs);
      if (this.lag) this.pending.push({ at: this.t.b.t + this.lag, apply }); else apply();
    }
  }
  flush() { const now = this.t.b.t; const due = this.pending.filter((p) => p.at <= now); this.pending = this.pending.filter((p) => p.at > now); due.forEach((p) => p.apply()); }
}

// ---------------------------------------------------------------- fake image host
// What the WASM host does for the Image component (core/images.h): the device lists the URLs it wants, the host answers
// with RGBA pixels (fw_img_put) or a failure (fw_img_fail). serve(url, w, h, fill) = an image (fill: [r,g,b,a] or
// (x, y) => [r,g,b,a]); status(url, 404) = an HTTP error; hang(url) = never answers (the device times out); unknown
// URLs = 404. asked = every URL the device fetched, in order.
class FakeImg {
  constructor(t) { this.t = t; this.map = {}; this.asked = []; }
  serve(url, w, h, fill = [128, 128, 128, 255]) { this.map[url] = { w, h, fill }; return this; }
  status(url, code) { this.map[url] = { code: String(code) }; return this; }
  hang(url) { this.map[url] = { hang: true }; return this; }
  flush() {
    const m = this.t.b.m;
    if (!m._fw_img_wants) return;
    for (const url of JSON.parse(m.UTF8ToString(m._fw_img_wants()) || '[]')) {
      this.asked.push(url);
      const s = this.map[url] || { code: '404' };
      if (s.hang) { this.t.log.push(`img: ${url} (no answer)`); continue; }
      if (s.code) { this.t.log.push(`img: ${url} -> ${s.code}`); this.t.b.str(url, (pu) => this.t.b.str(s.code, (pw) => m._fw_img_fail(pu, pw))); continue; }
      const px = new Uint8Array(s.w * s.h * 4);
      for (let y = 0; y < s.h; y++) for (let x = 0; x < s.w; x++) px.set(typeof s.fill === 'function' ? s.fill(x, y) : s.fill, (y * s.w + x) * 4);
      const p = m._malloc(px.length); m.HEAPU8.set(px, p);
      this.t.b.str(url, (pu) => m._fw_img_put(pu, s.w, s.h, p)); m._free(p);
      this.t.log.push(`img: ${url} -> ${s.w}x${s.h}`);
    }
  }
  stats() { const m = this.t.b.m; return m._fw_img_stats ? JSON.parse(m.UTF8ToString(m._fw_img_stats())) : {}; }
  /** leave the screen: build something with no image on the same stage (every image view goes) */
  leave() { this.t.b.rebuild('icon', { name: 'bulb' }); this.t.b.settle(500); this.t.log.push('img: left the screen'); return this.stats(); }
}

// ---------------------------------------------------------------- the test context (`t` in a test's run)
class Ctx {
  constructor(b) { this.b = b; this.log = []; this.fails = []; this.ha = new FakeHA(this); this.img = new FakeImg(this); this.comp = null; }
  // -- mount
  async mount(comp, props = {}) {
    this.comp = comp; this.props = props;
    const err = await this.b.render(comp, props); this.built = this.b.t; this.b.settle(1500); this.b.events();
    if (err) throw new Error(`render ${comp}: ${err}`);
    this.ha.push(); this.b.settle(1500); this.img.flush(); this.b.settle(1500); this.b.events();
    this.log.push(`mount ${comp} ${JSON.stringify(props)}`);
    return this;
  }
  async device(deck) {  // the device app (Home first); deck: a deck spec (else the built-in demo deck)
    await this.mount('device', {});
    if (deck) {
      const r = JSON.parse(this.b.spec('deck', deck) || '{}');
      if (r.errors?.length) throw new Error(`deck: ${JSON.stringify(r.errors).slice(0, 200)}`);
      this.b.rebuild('device', {}); this.b.settle(1500); this.ha.push(); this.b.settle(1500); this.b.events();
    }
    return this;
  }
  // -- the device's navigation (facet-188): send a spec (stored, never shown), show a screen (= POST /fw/show), the stack
  send(spec) {
    const r = JSON.parse(this.b.spec('screen_dev', spec) || '{}'); this.b.settle(1500); this.b.events();
    this.log.push(`send ${spec.id}: ${r.status}${r.errors ? ' ' + JSON.stringify(r.errors).slice(0, 160) : ''}`);
    return r;
  }
  unsend(id) { this.b.spec('revert', id); this.b.settle(1500); this.b.events(); }
  show(id) {
    const e = this.b.str(id, (p) => this.b.m.UTF8ToString(this.b.m._fw_bench_nav(p, id === 'home' ? 2 : 0)));
    this.b.settle(1500); this.ha.push(); this.b.settle(1500); this.log.push(`show ${id}${e ? ': ' + e : ''}`);
    return this.b.events();
  }
  stack() { return (JSON.parse(this.b.str('stack', (c) => this.b.str('', (a) => this.b.m.UTF8ToString(this.b.m._fw_test(c, a))))) || [])[0]?.stack || []; }
  where() { return this.stack().at(-1); }
  update(props) {  // the component's update path (fw_update), as a host pushing new props
    this.props = { ...this.props, ...props };
    const r = this.b.update(this.comp, this.props); this.b.settle(1500); this.img.flush(); this.b.settle(1500);
    this.log.push(`update ${JSON.stringify(props)}${r ? ' -> ' + r : ''}`);
    return this.b.events();
  }
  // Rotate screen (facet-191). The panel turns the picture in ESPHome's LVGL flush (software rotation, clockwise) and
  // maps every touch back through LvglComponent::rotate_coordinates (ESPHome 2026.9 lvgl_esphome.cpp); LVGL itself
  // stays upright, which is what this WASM stage is. turn(deg) = what the firmware's rotate_hook does; glass([x, y]) =
  // where a point of the UI shows on the glass, tapGlass(px, py) = a finger there (the touch goes through ESPHome's map).
  turn(deg) { this.rot = deg; this.log.push(`turned ${deg}°`); }
  glass([x, y]) { const W = this.b.S - 1, r = this.rot || 0; return r === 90 ? [W - y, x] : r === 180 ? [W - x, W - y] : r === 270 ? [y, W - x] : [x, y]; }
  untouch([px, py]) { const W = this.b.S - 1, r = this.rot || 0; return r === 90 ? [py, W - px] : r === 180 ? [W - px, W - py] : r === 270 ? [W - py, px] : [px, py]; }
  tapGlass(px, py) { const [x, y] = this.untouch([px, py]); return this.gesture(`tap glass ${Math.round(px)},${Math.round(py)} (${this.rot || 0}°)`, this.b.tap(x, y)); }
  setting(id) { return this.b.str(id, (p) => this.b.m.UTF8ToString(this.b.m._fw_setting(p, 0))); }
  toast(o) { this.b.haPut('facet.toast', '', JSON.stringify({ n: (this.nToast = (this.nToast || 0) + 1), ...o })); this.log.push(`ha: toast ${JSON.stringify(o)}`); this.b.tick(2); const ev = this.b.events(); this.b.settle(1500); return ev.concat(this.b.events()); }
  since() { return this.b.t - (this.built ?? 0); }  // ms since the mount was built (mounts settle a few seconds)
  until(ms) { return this.wait(Math.max(0, ms - this.since())); }  // wait until ms after the build
  wait(ms) { const end = this.b.t + ms; while (this.b.t < end) { this.b.tick(); this.ha.flush(); this.img.flush(); } return this.b.events(); }
  // -- read back
  objs() { return this.b.audit(); }
  visible(o) { return o.w > 0 && o.h > 0 && o.opa !== 0 && o.x + o.w > 0 && o.y + o.h > 0 && o.x < this.b.S && o.y < this.b.S; }
  texts(box) { return this.objs().filter((o) => o.text && this.visible(o) && (!box || inside(o, box))).map((o) => String(o.text)); }
  comps() { return JSON.parse(this.b.m.UTF8ToString(this.b.m._fw_comps(this.b.stage)) || '[]'); }
  /** the box of component `id` (innermost) that holds the text `label` (or the first one) */
  part(id, label) {
    const os = this.objs(), lab = label && os.find((o) => o.text && match(o.text, label) && this.visible(o));
    if (label && !lab) return null;
    const cs = this.comps().filter((c) => c.ids.includes(id) && (!lab || inside(lab, c)));
    const c = cs.sort((a, z) => a.w * a.h - z.w * z.h)[0];
    return c ? { x: c.x, y: c.y, w: c.w, h: c.h, texts: this.texts(c) } : null;
  }
  find(sel) {  // an object: "text" | /re/ | {text} | {clk: i} | {type} | {at: [x, y]} | a box
    if (sel && sel.w !== undefined) return sel;
    const os = this.objs().filter((o) => this.visible(o));
    let o;
    if (typeof sel === 'string' || sel instanceof RegExp) sel = { text: sel };
    if (sel.text !== undefined) o = os.find((k) => k.text && match(k.text, sel.text));
    else if (sel.clk !== undefined) o = os.filter((k) => k.clk)[sel.clk];
    else if (sel.type) o = os.find((k) => k.t === sel.type);
    else if (sel.at) o = { x: sel.at[0], y: sel.at[1], w: 1, h: 1 };
    if (!o) throw new Error(`not found: ${JSON.stringify(sel, (k, v) => (v instanceof RegExp ? String(v) : v))}`);
    return o;
  }
  // -- touch (each returns the events + sounds it caused; HA service calls are answered by the fake HA)
  gesture(what, evs) { this.ha.react(evs); this.b.settle(1500); this.ha.flush(); evs = evs.concat(this.b.events()); this.log.push(`${what}: ${evs.map(fmt).join(' ') || '-'}`); return evs; }
  tap(sel) { const o = this.find(sel), [x, y] = ctr(o); return this.gesture(`tap ${desc(sel)}`, this.b.tap(x, y)); }
  hold(sel, ms = B.long_press_ms + 100) { const o = this.find(sel), [x, y] = ctr(o); return this.gesture(`hold ${desc(sel)} ${ms} ms`, this.b.tap(x, y, ms)); }
  /** drag from sel's centre (or {from: [x, y]}) by [dx, dy] px, or to x = toX (px) */
  drag(sel, { by, toX, from } = {}) {
    const o = sel ? this.find(sel) : null, [x, y] = from || ctr(o);
    const [dx, dy] = by || [toX - x, 0];
    return this.gesture(`drag ${desc(sel)} by ${Math.round(dx)},${Math.round(dy)}`, this.b.drag(x, y, dx, dy));
  }
  // -- assertions (soft: every failure is collected, the test goes on)
  ok(c, msg) { if (!c) this.fails.push(msg); return !!c; }
  eq(a, e, msg) { return this.ok(JSON.stringify(a) === JSON.stringify(e), `${msg}: got ${JSON.stringify(a)}, want ${JSON.stringify(e)}`); }
  has(list, want, msg) { return this.ok(list.some((s) => match(s, want)), `${msg}: ${want} not in ${JSON.stringify(list)}`); }
  hasNot(list, bad, msg) { return this.ok(!list.some((s) => match(s, bad)), `${msg}: ${bad} found in ${JSON.stringify(list)}`); }
  emitted(evs, name, detail, msg) { return this.ok(evs.some((e) => e.kind === 'event' && e.name === name && (detail === undefined || match(String(e.detail ?? ''), detail))), `${msg || 'event'}: no ${name}${detail !== undefined ? `(${detail})` : ''} in ${evs.map(fmt).join(' ') || 'nothing'}`); }
  notEmitted(evs, name, msg) { return this.ok(!evs.some((e) => e.kind === 'event' && e.name === name), `${msg || 'event'}: ${name} fired: ${evs.map(fmt).join(' ')}`); }
  sounds(evs, want, msg) { const s = evs.filter((e) => e.kind === 'sound' && !/tick|landmark/.test(e.name)).map((e) => e.name); return this.eq(s, want, msg || 'sounds'); }
  silent(evs, msg) { return this.ok(!evs.some((e) => e.kind === 'sound'), `${msg || 'silent'}: played ${evs.filter((e) => e.kind === 'sound').map(fmt).join(' ')}`); }
  call(want, msg) { return this.ok(this.ha.calls.some((c) => c.service === want.service && (!want.entity || c.entity === want.entity)), `${msg || 'HA call'}: no ${want.service} ${want.entity || ''} in ${JSON.stringify(this.ha.calls)}`); }
}
const match = (s, w) => (w instanceof RegExp ? w.test(String(s)) : String(s) === String(w));
const inside = (o, b) => o.x >= b.x && o.y >= b.y && o.x + o.w <= b.x + b.w && o.y + o.h <= b.y + b.h;
const ctr = (o) => [Math.round(o.x + o.w / 2), Math.round(o.y + o.h / 2)];
const fmt = (e) => `${e.kind === 'sound' ? '♪' : ''}${e.name}${e.detail ? `(${String(e.detail).slice(0, 60)})` : ''}`;
const desc = (s) => (s === null || s === undefined ? '' : s instanceof RegExp ? String(s) : typeof s === 'string' ? `"${s}"` : JSON.stringify(s).slice(0, 40));
const slug = (s) => s.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '').slice(0, 60);

// ---------------------------------------------------------------- probe: print what a mount shows (writing tests)
if (argv.includes('--probe')) {
  const i = argv.indexOf('--probe'), comp = argv[i + 1], props = JSON.parse(argv[i + 2] || '{}');
  const b = new Bench(null, DENS[0], { build: BUILD, long_ms: B.long_press_ms, drag_px: B.drag_px_per_tick }), t = new Ctx(b);
  await (comp === 'device' ? t.device() : t.mount(comp, props));
  for (const o of t.objs().filter((o) => t.visible(o) && (o.text || o.clk))) console.log(`${o.clk ? 'CLK ' : '    '}${o.t} ${o.w}x${o.h}@${o.x},${o.y}${o.text ? ' "' + o.text + '"' : ''}`);
  console.log('comps:', t.comps().filter((c) => c.d <= 3).map((c) => `${c.ids.at(-1)} ${c.w}x${c.h}@${c.x},${c.y}`).join(' | '));
  process.exit(0);
}

// ---------------------------------------------------------------- run
const files = readdirSync(DIR).filter((f) => f.endsWith('.test.mjs')).sort();
const suites = [];
for (const f of files) { const s = (await import(pathToFileURL(join(DIR, f)).href)).default; if (!ONLY || ONLY.includes(s.id)) suites.push({ ...s, file: f }); }
if (PNG !== 'none') { rmSync(join(OUT, 'png'), { recursive: true, force: true }); mkdirSync(join(OUT, 'png'), { recursive: true }); }
const t0 = performance.now(), results = [];
for (const s of suites) for (const test of s.tests) {
  if (GREP && !test.name.includes(GREP)) continue;
  for (const den of DENS.filter((d) => !test.den || test.den === d)) {
    const b = new Bench(null, den, { build: BUILD, long_ms: B.long_press_ms, drag_px: B.drag_px_per_tick }), t = new Ctx(b);
    const a = performance.now();
    try {
      if (test.mount) await t.mount(test.mount[0] || s.id, test.mount[1] || {});
      await test.run(t, { den, S: b.S });
    } catch (e) { t.fails.push(`error: ${e.message}`); if (VERBOSE) console.error(e.stack); }
    if (t.fails.length && b.stage >= 0) try { t.log.push(`screen texts: ${JSON.stringify(t.texts())}`); } catch {}
    const ok = !t.fails.length, xfail = test.xfail || null;
    const r = { id: s.id, family: s.family || '', test: test.name, den, ok, xfail, fails: t.fails, log: t.log, ms: Math.round(performance.now() - a) };
    if (b.stage >= 0 && (PNG === 'all' || (PNG === 'fail' && !ok))) {
      const p = `png/${s.id}/${den}-${slug(test.name)}.png`;
      mkdirSync(join(OUT, 'png', s.id), { recursive: true }); writeFileSync(join(OUT, p), shot(b)); r.png = p;
    }
    results.push(r);
    const tag = ok ? (xfail ? 'XPASS' : 'PASS') : xfail ? 'XFAIL' : 'FAIL';
    if (VERBOSE || !ok) console.log(`${tag} ${s.id} [${den}] ${test.name}${ok ? '' : ': ' + t.fails.slice(0, 3).join(' | ')}${xfail && !ok ? ` (${xfail})` : ''}`);
    if (VERBOSE && !ok) for (const l of t.log) console.log(`    ${l}`);
  }
}
const fails = results.filter((r) => !r.ok && !r.xfail), xf = results.filter((r) => !r.ok && r.xfail), xp = results.filter((r) => r.ok && r.xfail);
const summary = { at: new Date().toISOString(), build: BUILD, dens: DENS, total: results.length, pass: results.length - fails.length - xf.length, fail: fails.length, xfail: xf.length, xpass: xp.length, sec: +((performance.now() - t0) / 1000).toFixed(1) };
const doc = { ...summary, suites: suites.map((s) => ({ id: s.id, family: s.family || '', about: s.about || '', tests: s.tests.map((x) => x.name) })), results };
mkdirSync(OUT, { recursive: true });
writeFileSync(join(OUT, 'results.json'), JSON.stringify(doc, null, 1));
if (arg('--json')) writeFileSync(arg('--json'), JSON.stringify(doc, null, 1));
for (const r of xp) console.log(`XPASS ${r.id} [${r.den}] ${r.test}: passes now, remove its xfail`);
console.log(`functional: ${summary.total} tests, ${summary.pass} pass, ${summary.fail} FAIL, ${summary.xfail} expected-fail, ${summary.xpass} xpass (${summary.sec} s)`);
process.exit(fails.length && !argv.includes('--soft') ? 1 : 0);
