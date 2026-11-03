// The functional tests' fake Home Assistant and test context (tools/functional.mjs runs the tests with them; the docs
// site's build step, site/scripts/shots.mjs, drives the same fake into each screen state for its pictures). Headless on
// the WASM build (tools/lib/fwbench.mjs): FakeHA = the states, replies, unavailable / disconnect / reconnect; Ctx = `t`
// in a test's run (mount, the device's navigation, touch, read back, soft assertions).
import { readFileSync } from 'node:fs';
import { demoValue } from '../demo_ha.mjs';
import { demoReply, demoWs } from '../demo_ha_replies.mjs';

const B = JSON.parse(readFileSync(new URL('../../tests/behaviour/_budgets.json', import.meta.url), 'utf8'));

// ---------------------------------------------------------------- fake Home Assistant
// A store of entity states the device reads through fw_ha_put. Values the store lacks fall back to tools/demo_ha.mjs.
// auto: service calls the device sends ("ha" events) change the store the way HA would, and the new state arrives on
// the next frame (lag: ms before it arrives, to test optimistic UI). Disconnect = every bound entity reads
// "unavailable" (what the panel sees when HA drops); reconnect = the stored states arrive again.
const DOMAIN_ON = { light: 1, switch: 1, fan: 1, input_boolean: 1 };
export class FakeHA {
  // absent: entities Home Assistant does not have; it never answers them (no state at all, as HA does for a deleted one)
  constructor(t) { this.t = t; this.s = {}; this.calls = []; this.auto = true; this.lag = 0; this.down = false; this.pending = []; this.absent = new Set(); }
  get(e, a = '') { const st = this.s[e]; if (st && a in st) return st[a] ?? ''; return demoValue(e, a); }  // null = HA dropped it -> ''
  wants() { return JSON.parse(this.t.b.m.UTF8ToString(this.t.b.m._fw_ha_wants()) || '[]'); }
  push(only) {  // send the bound pairs (all, or one entity's) to the device
    for (const [e, a] of this.wants()) {
      if ((only && e !== only) || this.absent.has(e)) continue;
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
  // Replies (opt in: answers = true; a test that checks an ask stays open leaves it off): the HA actions an adapter
  // wants the reply of (fw_ha_asks: weather.get_forecasts, calendar.get_events) and the panel's own link's reads
  // (fw_ha_gets: Energy's three WebSocket reads, the link check; a REST read = not found). Served on every tick (Ctx). Failing while
  // down (the network pulled) or fail (Home Assistant answering but its data unavailable); missing = an empty
  // Energy dashboard and no entity found. Never a real Home Assistant.
  serve() {
    if (!this.answers) return;
    const m = this.t.b.m;
    if (!m?._fw_ha_asks) return;
    const bad = this.down || this.fail;
    const str = (x, f) => { const q = m.stringToNewUTF8(x); try { return f(q); } finally { m._free(q); } };
    const asks = JSON.parse(m.UTF8ToString(m._fw_ha_asks()) || '[]');
    for (const a of asks) {
      const reply = bad ? null : this.reply(a.service, a.data || {});
      str(reply ? JSON.stringify(reply) : '', (q) => m._fw_ha_answer(a.id, reply ? 1 : 0, q));
    }
    if (!m._fw_ha_gets) return;
    for (const g of JSON.parse(m.UTF8ToString(m._fw_ha_gets()) || '[]')) {
      const id = typeof g === 'number' ? g : g.id;
      if (g.get === '') {  // the link check (GET <url>/api/), in the panel's words: refused = a wrong token (refuse)
        const why = this.down ? 'cannot reach Home Assistant at that address' : this.refuse ? 'Home Assistant refused the token' : '';
        const b = '{"message":"API running."}';
        if (!why) str(b, (q) => m._fw_ha_chunk(id, q, b.length));
        str(why, (q) => m._fw_ha_got(id, why ? 0 : 1, q));
        continue;
      }
      if (typeof g === 'number' && !bad) {  // GET <url>/api/states (the link's entity list): the store's entities
        const b = JSON.stringify(Object.entries(this.s).filter(([, v]) => v && v[''] !== undefined && v[''] !== '')
          .map(([e, v]) => ({ entity_id: e, state: v[''], attributes: { friendly_name: v.friendly_name ?? e } })));
        str(b, (q) => m._fw_ha_chunk(id, q, Buffer.byteLength(b, 'utf8')));
        str('', (q) => m._fw_ha_got(id, 1, q));
        continue;
      }
      if (g.get === 'config' && !bad) {  // GET <url>/api/config: HA's own settings (the panel reads its country)
        const b = JSON.stringify(this.country ? { country: this.country, unit_system: { length: 'km' } } : { unit_system: { length: 'km' } });
        str(b, (q) => m._fw_ha_chunk(id, q, b.length));
        str('', (q) => m._fw_ha_got(id, 1, q));
        continue;
      }
      const body = bad || !g.ws ? null : this.ws(typeof g.ws === 'string' ? JSON.parse(g.ws) : g.ws);
      if (body) { const b = JSON.stringify(body); str(b, (q) => m._fw_ha_chunk(id, q, Buffer.byteLength(b, 'utf8'))); }
      str(body ? '' : (this.down ? 'Home Assistant not reached' : 'not found'), (q) => m._fw_ha_got(id, body ? 1 : 0, q));
    }
  }
  known(e) { return !this.missing && !this.absent.has(e) && this.s[e]?.[''] !== '' && (this.s[e] !== undefined || demoValue(e, '') !== undefined); }  // '' / absent: no such entity
  reply(service, data) {
    const e = [].concat(data.entity_id || [])[0];
    return e && this.known(e) ? demoReply(service, data) : null;
  }
  ws(cmd) { return demoWs(cmd, { empty: this.missing, prefix: 'test_' }); }  // HA's result for the link's reads (Energy)
  // the device is told (fw_ha_connected) first, as a panel's API link is: Offline while down, not Unavailable
  disconnect() { this.down = true; this.t.log.push('ha: disconnected'); this.t.b.m._fw_ha_connected?.(0); this.push(); this.t.b.settle(1500); }
  reconnect() { this.down = false; this.t.log.push('ha: reconnected'); this.t.b.m._fw_ha_connected?.(1); this.push(); this.t.b.settle(1500); }
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

// ---------------------------------------------------------------- the test context (`t` in a test's run)
export class Ctx {
  constructor(b) {
    this.b = b; this.log = []; this.fails = []; this.ha = new FakeHA(this); this.comp = null;
    const tick = b.tick.bind(b);
    b.tick = (n = 1) => { for (let i = 0; i < n; i++) { tick(1); this.ha.serve(); } };  // the fake HA's replies (FakeHA.serve, opt in)
  }
  // -- mount
  async mount(comp, props = {}) {
    this.comp = comp; this.props = props;
    const err = await this.b.render(comp, props); this.built = this.b.t; this.b.settle(1500); this.b.events();
    if (err) throw new Error(`render ${comp}: ${err}`);
    this.ha.push(); this.b.settle(1500); this.b.events();
    this.log.push(`mount ${comp} ${JSON.stringify(props)}`);
    return this;
  }
  async device(deck) {  // the device app (Home first); deck: a deck spec, 'house' = tests/fixtures/deck-house.json (else the factory deck)
    if (deck === 'house') deck = JSON.parse(readFileSync(new URL('../../tests/fixtures/deck-house.json', import.meta.url), 'utf8'));
    await this.mount('device', {});
    if (deck) {
      const r = JSON.parse(this.b.spec('deck', deck) || '{}');
      if (r.errors?.length) throw new Error(`deck: ${JSON.stringify(r.errors).slice(0, 200)}`);
      this.b.rebuild('device', {}); this.b.settle(1500); this.ha.push(); this.b.settle(1500); this.b.events();
    }
    return this;
  }
  // -- the device's navigation: send a spec (stored, never shown), show a screen (= POST /fw/show), the stack
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
    const r = this.b.update(this.comp, this.props); this.b.settle(1500);
    this.log.push(`update ${JSON.stringify(props)}${r ? ' -> ' + r : ''}`);
    return this.b.events();
  }
  // Rotate screen. The panel turns the picture in ESPHome's LVGL flush (software rotation, clockwise) and
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
  wait(ms) { const end = this.b.t + ms; while (this.b.t < end) { this.b.tick(); this.ha.flush(); } return this.b.events(); }
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
