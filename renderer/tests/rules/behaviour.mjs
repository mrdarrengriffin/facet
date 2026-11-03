// behaviour (rule, fast + gate tiers, Facet theme, built dark): the audits check how things LOOK; this checks that they
// WORK (README "Behaviour tests"; moved from tools/behaviour.mjs onto the runner). Every component and
// screen is driven through LVGL input (press, release, tap, long-press, drag) with LVGL time advanced in fixed 16 ms
// ticks (deterministic), then asserted on what it does: events to the host (fw_events: actions, navigation, HA service
// calls "ha", settings), sounds (kind "sound"), scroll position + limits (fw_test "scrolls"), the device screen stack
// (fw_test "stack"), the update path (fw_update) and HA state (fw_ha_put). Kinds (every id gets the automatic tests
// below; tests/behaviour/<id>.json adds its own + expected fails):
//   interaction  taps fire one action; disabled / unavailable fire nothing but the error; a press dragged off and
//                released fires nothing; spec screens: a handled event makes an action with the screen's entity,
//                unavailable entities fire no HA call; device: navigate + Back return to the right screen
//   scroll       content that overflows scrolls without jumps, stops at both ends, keeps its place on an update,
//                nested scrollers never move together; content that fits never moves under a drag
//   sound        each gesture plays at most one sound (step ticks aside) from the id's sound usage (tools/gen_sound_usage.mjs);
//                updates (fw_update, HA values) play nothing; Sounds off plays nothing (device)
//   lag          not here: tools/bench.mjs --lag (nightly) runs these same cases for it, a PROXY for the Guition (1x,
//                weakest panel): refreshes past LVGL's 32 dirty areas (= full redraws), runs of near-full-screen frames,
//                estimated Guition frame ms (a + b x WASM ms, bench/budgets.json fit), redraws while idle or after an
//                update that changed nothing. Real fps needs the device (tools/bench.mjs --panel)
// Cases: the default and up to _budgets.json max_variants variants (the blocked ones kept), each on fresh modules (a
// tap may navigate or open a sheet); the id's own tests after its last case. An expected fail (xfail) passes; one that
// passes now is a note (XPASS: remove it from the id's xfail). FW_BEHAVIOUR_JSON=<file>: every result as JSON there.
import { readFileSync, writeFileSync, existsSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { Bench as Bench0 } from '../../tools/lib/fwbench.mjs';
import { usage as USAGE } from '../../tools/gen_sound_usage.mjs';

const HERE = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const TESTS = join(HERE, 'tests/behaviour');
export const B = JSON.parse(readFileSync(join(TESTS, '_budgets.json'), 'utf8'));
const FIT = JSON.parse(readFileSync(join(HERE, 'bench/budgets.json'), 'utf8')).devices['guition-4848s040'].measured;
const T = 16;  // ms per tick = LV_DEF_REFR_PERIOD
class Bench extends Bench0 { constructor(m, den) { super(m, den, { long_ms: B.long_press_ms, drag_px: B.drag_px_per_tick }); } }
const specsCache = {};  // the screen specs the module lists (fw_spec list), for the Spec screen's checks
function specsOf(m) {
  if (!specsCache.done) {
    const S = (s, f) => { const p = m.stringToNewUTF8(s); try { return f(p); } finally { m._free(p); } };
    const l = JSON.parse(S('list', (c) => S('', (a) => m.UTF8ToString(m._fw_spec(c, a)))));
    for (const [k, v] of Object.entries(l.screens || {})) specsCache[k] = typeof v === 'string' ? JSON.parse(v) : v;
    Object.defineProperty(specsCache, 'done', { value: true });
  }
  return specsCache;
}

// ---------------------------------------------------------------- helpers
const isTick = (s) => /tick|landmark/.test(s.name);
const NOT_ACTIONS = new Set(['screen', 'restyle', 'blocked', 'delay', 'toast']);  // reports, not actions a tap makes
const DRAGGY = new Set(['change', 'input', 'dismiss', 'value', 'drag', 'scrub']);  // a drag is the gesture itself
const BLOCKED_VALUES = new Set(['disabled', 'unavailable']);
const blockedProps = (p) => Object.entries(p || {}).some(([k, v]) => BLOCKED_VALUES.has(String(v)) || ((k === 'disabled' || k === 'unavailable') && v === true) || (k === 'available' && v === false));
const centre = (o) => [Math.round(o.x + o.w / 2), Math.round(o.y + o.h / 2)];
const inBox = (b, x, y) => x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h;
const fmtEv = (e) => `${e.kind === 'sound' ? '♪' : ''}${e.name}${e.detail ? `(${String(e.detail).slice(0, 60)})` : ''}`;
const glob = (pat, s) => new RegExp(`^${pat.replace(/[.+?^${}()|[\]\\]/g, '\\$&').replace(/\*/g, '.*')}$`).test(s);

// allowed sounds per id: its own fw::sound calls + every component it builds (sound usage `via`), transitively
const usageById = Object.fromEntries((USAGE.components || []).concat(USAGE.screens || [], USAGE.foundations || []).map((c) => [c.id, c]));
function allowedSounds(id, seen = new Set()) {
  const u = usageById[id];
  if (!u || seen.has(id)) return null;
  seen.add(id);
  const out = new Set(u.sounds || []);
  for (const v of u.via || []) for (const s of allowedSounds(v, seen) || []) out.add(s);
  return out;
}

// per-id test files: tests/behaviour/<id>.json {id, tests: [...], xfail: {"kind:test:variant": "BF-012 ..."}, live?, budgets?}
function specOf(id) {
  const f = join(TESTS, `${id}.json`);
  return existsSync(f) ? JSON.parse(readFileSync(f, 'utf8')) : { id, tests: [], xfail: {} };
}


// clickable targets of what the stage shows: visible, on the stage, one per distinct box, at most `cap` spread evenly
function targets(objs, S, cap) {
  const seen = new Set(), out = [];
  for (const o of objs) {
    if (!o.clk || o.w <= 0 || o.h <= 0 || o.opa === 0) continue;
    const [x, y] = centre(o);
    if (x < 0 || y < 0 || x >= S || y >= S) continue;
    const k = `${o.x},${o.y},${o.w},${o.h}`;
    if (seen.has(k)) continue;
    seen.add(k); out.push(o);
  }
  if (out.length <= cap) return out;
  return Array.from({ length: cap }, (_, i) => out[Math.floor((i * out.length) / cap)]);
}
const label = (objs, o) => {  // a short name for a target: its first label text, else its box
  const i = objs.indexOf(o);
  const txt = objs.find((k, j) => j > i && k.text && inBox(o, k.x + 1, k.y + 1))?.text;
  return txt ? `"${String(txt).slice(0, 18)}"` : `${o.w}x${o.h}@${o.x},${o.y}`;
};

// ---------------------------------------------------------------- results
// what the tests found since the last take(): the rule reports them per case, bench --lag per case; only the kinds
// asked for (setKinds) are kept
let results = [], KINDS = ['interaction', 'scroll', 'sound', 'lag'], VERBOSE = false;
export const setKinds = (k, verbose = false) => { KINDS = k; VERBOSE = verbose; };
export const take = () => { const r = results; results = []; return r; };
function record(r) { if (KINDS.includes(r.kind)) results.push(r); }
function judge(id, den, kind, test, variant, msgs, extra = {}) {
  const key = `${kind}:${test}:${variant}`;
  // "warn-..." = reported, never fails (timing near the budget: host noise must not flip the gate)
  const warns = [...new Set(msgs.filter((m) => m.startsWith('warn-')))];
  const fails = [...new Set(msgs.filter((m) => !m.startsWith('warn-')))];
  record({ id, den, kind, test, variant, key, ok: !fails.length, msgs: fails, ...(warns.length ? { warns } : {}), ...extra });
}

// gesture checks shared by taps / long presses (kinds interaction + sound)
function checkGesture(evs, { blocked, allowed, what, gesture }) {
  const inter = [], snd = [];
  const acts = evs.filter((e) => e.kind === 'event' && !NOT_ACTIONS.has(e.name));
  const sounds = evs.filter((e) => e.kind === 'sound' && !isTick(e));
  const wasBlocked = blocked || evs.some((e) => e.kind === 'event' && e.name === 'blocked');
  if (wasBlocked) {
    if (acts.length) inter.push(`disabled-fires: ${gesture} on ${what} (disabled/unavailable) fired ${acts.map(fmtEv).join(', ')}`);
    const bad = sounds.filter((s) => s.name !== 'error');
    if (bad.length) snd.push(`disabled-sound: ${gesture} on ${what} (disabled/unavailable) played ${bad.map(fmtEv).join(', ')} (only error)`);
  }
  if (sounds.length > 1) snd.push(`double-sound: ${gesture} on ${what} played ${sounds.length} sounds: ${sounds.map(fmtEv).join(', ')}`);
  if (allowed) for (const s of sounds) if (!allowed.has(s.name)) snd.push(`unknown-sound: ${gesture} on ${what} played ${s.name}, not in its sound usage (${[...allowed].join(', ') || 'none'})`);
  if (acts.some((e) => !DRAGGY.has(e.name)) && !sounds.length && !evs.some((e) => e.kind === 'sound')) snd.push(`silent-action: ${gesture} on ${what} fired ${acts.map(fmtEv).join(', ')} with no sound`);
  const names = acts.map((e) => `${e.name}|${e.detail}`);
  const dup = names.find((n, i) => names.indexOf(n) !== i && !DRAGGY.has(n.split('|')[0]));
  if (dup) inter.push(`double-action: ${gesture} on ${what} fired ${dup.split('|')[0]} twice`);
  return { inter, snd, acts, sounds };
}

// lag judgement of one phase's frames
// a live id's idle and same-update redraws are allowed only up to livePx dirty px per frame (behaviour JSON "live_px";
// none = unbounded): a full redraw still fails
const liveOver = (frames, livePx) => (livePx ? frames.filter((f) => f.renders && f.px > livePx) : []);
export function judgeLag(frames, S, { live = false, idle = false, budgets = {}, stats = null, livePx = 0 } = {}) {
  const b = { ...B.lag, ...budgets }, msgs = [];
  const area = S * S;
  const over = frames.reduce((n, f) => n + f.over, 0);
  if (over > b.overflow_max) msgs.push(`lag-overflow: ${over} refresh${over > 1 ? 'es' : ''} past 32 dirty areas (LVGL redraws the whole screen)`);
  let run = 0, worst = 0;
  for (const f of frames) { run = f.px >= b.heavy_frac * area ? run + 1 : 0; worst = Math.max(worst, run); }
  if (worst > b.heavy_run_max) msgs.push(`lag-heavy: ${worst} frames in a row redraw >= ${Math.round(b.heavy_frac * 100)}% of the screen (Guition ~12-19 fps territory)`);
  const moving = frames.filter((f) => f.renders);
  if (moving.length >= b.est_min_frames) {
    const ms = moving.map((f) => f.ms).sort((a, c) => a - c);
    const med = ms[Math.floor(ms.length / 4)];  // the lower quartile: host noise only ever adds time
    const est = FIT.a + FIT.b * med;
    if (est > b.est_ms_max) msgs.push(`${est > b.est_ms_fail ? '' : 'warn-'}lag-est: typical frame est ${est.toFixed(0)} ms on the Guition (> ${b.est_ms_max} ms, ${moving.length} frames, WASM ${med.toFixed(3)} ms)`);
  }
  const wide = idle && live ? liveOver(frames, livePx) : [];
  if (wide.length) msgs.push(`idle-redraw-area: ${wide.length} idle frames redrew more than ${livePx} px (largest ${Math.max(...wide.map((f) => f.px))} px)`);
  if (idle && !live && moving.length) msgs.push(`idle-redraw: ${moving.length} redraws (${Math.round(moving.reduce((n, f) => n + f.px, 0) / 1000)}k px) in ${b.idle_ms / 1000} s with nothing changing`);
  if (stats) {
    const ms = moving.map((f) => f.ms).sort((a, c) => a - c);
    Object.assign(stats, { frames: moving.length, over, heavy_run: worst, px_max: Math.round(Math.max(0, ...moving.map((f) => f.px)) / area * 100) / 100, areas_max: Math.max(0, ...moving.map((f) => f.areas)), est_ms: ms.length ? Math.round(FIT.a + FIT.b * ms[Math.floor(ms.length / 4)]) : 0 });
  }
  return msgs;
}

// ---------------------------------------------------------------- automatic tests for one id
function pickVariants(c) {
  const vs = [{ label: 'default', props: {} }, ...(c.meta.variants || [])];
  if (vs.length <= B.max_variants) return vs;
  const blocked = vs.filter((v) => blockedProps(v.props));
  const rest = vs.filter((v) => !blockedProps(v.props));
  return [...rest.slice(0, Math.max(2, B.max_variants - blocked.length)), ...blocked.slice(0, B.max_variants)].slice(0, B.max_variants + 2);
}

// the cases of an id behaviour runs, as runner variant indexes (0 = default): the Spec screen only its variants
export function picks(c) {
  const vs = c.meta.variants || [];
  return c.id === 'spec' ? vs.map((_, i) => i + 1) : pickVariants(c).map((v) => vs.indexOf(v) + 1);  // the default: -1 + 1
}
export function open(c, den, m) {  // one id at one density: what every case of it shares
  const spec = specOf(c.id);
  const id = c.id, isScreen = c.section === 'screens';
  const allowed = allowedSounds(id);
  const defaults = Object.fromEntries((c.meta.props || []).map((p) => [p.name, p.default]));
  const b = new Bench(m, den);
  const S = b.S;
  // spec "frame": true = mounted at its meta frame (the size it has on a screen), not filling the whole stage: a tile
  // fills its parent (C5), so on a bare screen-sized stage its tap fade covered the screen (the Entity
  // tile's lag-heavy, a tile is a Home grid cell on a panel). The lag budget is still a share of the SCREEN (S x S).
  if (spec.frame && c.meta.frame) { const D = den === '1x' ? 1 : 1.5; b.frame = c.meta.frame.map((v) => Math.round(v * D)); }
  const liveId = !!spec.live || !!c.meta.motion;  // live data or meta "motion" (moves by design): may redraw while idle
  const lagOn = KINDS.includes('lag') && den === '1x';
  const cap = isScreen ? B.targets_screen : B.targets;
  return { c, den, id, isScreen, allowed, defaults, b, S, liveId, lagOn, cap, spec, specs: specsOf(m) };
}

export async function variant(s, v) {  // the automatic tests of one case
  const { c, den, id, isScreen, allowed, defaults, b, S, liveId, lagOn, cap, spec, specs } = s;
  const props = { ...defaults, ...v.props };
  const vl = v.label;
  const live = liveId || (spec.live_screens || []).includes(props.screen);  // spec: per screen (clock, lightning, ...)
  const vspec = { ...spec, live };
  const blocked = blockedProps(v.props);
  // ---- build + lag of the build (appear motion)
  b.rec = [];
  const err = await b.render(id, props);
  if (err) { judge(id, den, 'interaction', 'render', vl, [`render-error: ${err}`]); return; }
  b.settle(1500);
  const buildFrames = b.rec.slice(b.rec.findIndex((f) => f.renders) + 1);  // the first refresh = the build itself (a full cut)
  b.events();
  if (lagOn) { const st = {}; judge(id, den, 'lag', 'build', vl, judgeLag(buildFrames, S, { live, budgets: spec.budgets, stats: st }), { stats: st }); }
  // ---- idle + same-props update (redraws when nothing changed: lag, 1x; the update is silent: every density)
  if (vl === 'default') {
    b.rec = []; b.tick(Math.ceil(B.lag.idle_ms / T));
    judge(id, den, 'lag', 'idle', vl, judgeLag(b.rec, S, { live, idle: true, budgets: spec.budgets, livePx: spec.live_px }));
    b.rec = [];
    const u = b.update(id, props);
    b.tick(Math.ceil(B.lag.idle_ms / T));
    const evs = b.events();
    const msgs = [];
    const big = live ? liveOver(b.rec, spec.live_px) : [];
    if (u === '' && big.length) msgs.push(`same-update-redraw-area: ${big.length} frames after an update with the same props redrew more than ${spec.live_px} px (largest ${Math.max(...big.map((f) => f.px))} px)`);
    if (u === '' && !live && b.rec.some((f) => f.renders)) msgs.push(`same-update-redraw: an update with the same props redrew ${b.rec.filter((f) => f.renders).length} frames (${Math.round(b.rec.reduce((n, f) => n + f.px, 0) / 1000)}k px)`);
    judge(id, den, 'lag', 'same-update', vl, msgs);
    if (KINDS.includes('sound')) judge(id, den, 'sound', 'quiet-same-update', vl, evs.filter((e) => e.name !== 'screen').length ? [`update-noise: an update with the same props sent ${evs.map(fmtEv).join(', ')}`] : []);
  }
  b.rec = null;
  // ---- taps on every target (fresh build each: a tap may navigate or open a sheet)
  const objs = b.audit();
  const tg = targets(objs, S, vl === 'default' ? cap : Math.min(cap, B.targets_variant));
  if (KINDS.includes('interaction') || KINDS.includes('sound') || lagOn) {
    const inter = [], snd = [], lagMsgs = [], specMsgs = [], lagStats = [];
    for (const o of tg) {
      await b.render(id, props); b.settle(1500); b.events();
      const what = label(objs, o);
      const [x, y] = centre(o);
      if (lagOn && vl === 'default') b.rec = [];
      const evs = b.tap(x, y);
      if (b.rec) { const st = {}; lagMsgs.push(...judgeLag(b.rec, S, { live, budgets: spec.budgets, stats: st }).map((s) => `${s} (tap ${what})`)); lagStats.push({ what, ...st }); b.rec = null; }
      const r = checkGesture(evs, { blocked, allowed, what, gesture: 'tap' });
      inter.push(...r.inter); snd.push(...r.snd);
      if (VERBOSE) console.log(`    ${id}[${vl}] tap ${what}: ${evs.map(fmtEv).join(' ') || '-'}`);
      if (id === 'spec') specMsgs.push(...checkSpecTap(b, props, specs, o, evs, what));
      if (id === 'device' && evs.some((e) => e.name === 'navigate')) inter.push(...await checkNav(b, evs, what));
    }
    if (KINDS.includes('interaction')) judge(id, den, 'interaction', 'tap', vl, [...inter, ...specMsgs]);
    if (KINDS.includes('sound')) judge(id, den, 'sound', 'tap', vl, snd);
    if (lagOn && vl === 'default') judge(id, den, 'lag', 'tap', vl, lagMsgs, { stats: lagStats });
  }
  // ---- long press + press dragged off (default and blocked variants)
  if (KINDS.includes('interaction') && (vl === 'default' || blocked || id === 'spec')) {
    const lm = [], cm = [], sm = [];
    for (const o of tg.slice(0, B.targets_variant)) {
      const what = label(objs, o);
      const [x, y] = centre(o);
      await b.render(id, props); b.settle(1500); b.events();
      const r = checkGesture(b.long(x, y), { blocked, allowed, what, gesture: 'long press' });
      lm.push(...r.inter); sm.push(...r.snd.filter((s) => !s.startsWith('silent-action')));
    }
    // press, slide well off the target (away from the stage centre), release: nothing may fire. Only targets a finger
    // can reach: the topmost clickable at the centre is the target or inside it (not one under an open sheet's scrim)
    const reach = (o) => {
      const [x, y] = centre(o), i = objs.indexOf(o);
      const top = objs.filter((k) => k.clk && k.w > 0 && k.h > 0 && k.opa !== 0 && inBox(k, x, y)).pop();
      return top === o || (!!top && objs.indexOf(top) > i && inBox(o, top.x, top.y) && inBox(o, top.x + top.w - 1, top.y + top.h - 1));
    };
    for (const o of targets(objs, S, 1e9).filter(reach).slice(0, B.targets_variant)) {
      const what = label(objs, o);
      const [x, y] = centre(o);
      await b.render(id, props); b.settle(1500); b.events();
      const away = Math.max(o.w, o.h) + B.cancel_px;
      const dx = x < S / 2 ? Math.min(away, S - 1 - x) : -Math.min(away, x);
      const dy = 0;
      const evs = b.drag(x, y, dx, dy, { hold: 96 });
      const acts = evs.filter((e) => e.kind === 'event' && !NOT_ACTIONS.has(e.name) && !DRAGGY.has(e.name));
      const end = x + dx;
      if (acts.length && !(end >= o.x && end < o.x + o.w)) cm.push(`cancel-fires: press on ${what}, slid ${Math.abs(dx)} px off and released, fired ${acts.map(fmtEv).join(', ')}`);
    }
    judge(id, den, 'interaction', 'long-press', vl, lm);
    judge(id, den, 'interaction', 'press-cancel', vl, cm);
    if (KINDS.includes('sound')) judge(id, den, 'sound', 'long-press', vl, sm);
  }
  // ---- update path to this variant from the default is silent
  if (KINDS.includes('sound') && vl !== 'default' && id !== 'spec') {
    await b.render(id, defaults); b.settle(1500); b.events();
    const u = b.update(id, props);
    b.settle(1500);
    const evs = b.events().filter((e) => e.name !== 'screen');
    judge(id, den, 'sound', 'quiet-update', vl, u !== 'rebuild' && evs.length ? [`update-noise: update default -> ${vl} sent ${evs.map(fmtEv).join(', ')}`] : []);
  }
  // ---- spec screens: HA values arriving are silent; unavailable entities fire no HA call
  if (id === 'spec') await specChecks(b, props, specs, vl, den);
  // ---- scroll
  if ((KINDS.includes('scroll') || lagOn) && (vl === 'default' || isScreen) && !Object.values(v.props || {}).includes('loop')) await scrollTests(b, c, props, vl, den, lagOn, vspec);  // bench loops never rest
}
export async function close(s) {  // after the id's last case: Sounds off (device), the id's own tests
  if (s.id === 'device' && KINDS.includes('sound')) await muteTest(s.b, s.den);
  await customTests(s.c, s.den, s.spec, s.b.m);
}


// a tap on a spec screen: an event the tapped node handles (spec "on") must make an action; HA calls name the
// entity the screen is set to (params) and a service of its domain
function checkSpecTap(b, props, specs, o, evs, what) {
  const sp = specs[props.screen];
  if (!sp) return [];
  const msgs = [];
  const hits = b.hits();  // after the tap: the screen may have moved on, use the boxes of the build (approx)
  const [x, y] = centre(o);
  const node = hits.filter((h) => inBox(h, x, y)).sort((a, c) => a.w * a.h - c.w * c.h)[0];
  const n = node ? sp.nodes?.[node.n] : null;
  const params = Object.fromEntries(Object.entries(sp.params || {}).map(([k, p]) => [k, props[k] || p.default]));
  for (const e of evs.filter((e) => e.name === 'ha')) {
    let d; try { d = JSON.parse(e.detail); } catch { msgs.push(`spec-bad-call: ${what} sent ha "${e.detail}" (not JSON)`); continue; }
    const ents = Object.values(params).flatMap((v) => String(v || '').split(','));
    if (!d.entity) msgs.push(`spec-wrong-entity: ${what} called ${d.service} with no entity`);
    else if (d.service.split('.')[0] !== d.entity.split('.')[0] && !(d.service.startsWith('homeassistant.'))) msgs.push(`spec-wrong-entity: ${what} called ${d.service} on ${d.entity} (another domain)`);
    else if (ents.length && !ents.includes(d.entity) && !ents.some((v) => !v.includes('.'))) msgs.push(`spec-wrong-entity: ${what} called ${d.service} on ${d.entity}, not the screen's entity (${ents.join(', ')})`);
  }
  if (n && n.on) {
    const handled = evs.filter((e) => e.kind === 'event' && n.on[e.name]);
    const actions = evs.filter((e) => ['ha', 'navigate', 'back', 'param', 'setting'].includes(e.name) || e.name.startsWith('screen.'));
    if (handled.length && !actions.length && !evs.some((e) => e.name === 'blocked')) msgs.push(`spec-no-action: ${what} (${n.c}) emitted ${handled.map(fmtEv).join(', ')}, handled by the spec (${handled.map((h) => n.on[h.name].do).join(', ')}), but no action reached the host`);
  }
  return msgs;
}

async function specChecks(b, props, specs, vl, den) {
  const sp = specs[props.screen];
  if (!sp) return;
  // HA values arriving: every pair the screen binds gets a new value; nothing may sound or act
  if (KINDS.includes('sound')) {
    await b.render('spec', props); b.settle(1500); b.events();
    const wants = JSON.parse(b.m.UTF8ToString(b.m._fw_ha_wants()) || '[]');
    for (const [e, a] of wants) b.haPut(e, a, a ? '42' : (e.startsWith('sensor.') ? '42' : 'on'));
    b.settle(1500);
    for (const [e, a] of wants) b.haPut(e, a, a ? '7' : (e.startsWith('sensor.') ? '7' : 'off'));
    b.settle(1500);
    const evs = b.events().filter((e) => !['screen'].includes(e.name));
    judge('spec', den, 'sound', 'quiet-ha', vl, evs.length ? [`update-noise: HA values for ${wants.length} bound pairs sent ${evs.slice(0, 6).map(fmtEv).join(', ')}`] : []);
  }
  // unavailable: every entity the screen binds reports unavailable; no tap may call HA
  if (KINDS.includes('interaction')) {
    const ents = [...new Set(Object.entries(sp.params || {}).filter(([, p]) => p.type === 'entity').map(([k, p]) => props[k] || p.default).filter(Boolean))];
    if (!ents.length) return;
    await b.render('spec', props); b.settle(500); b.events();
    for (const e of ents) b.haPut(e, '', 'unavailable');
    b.settle(1500); b.events();
    const objs = b.audit();
    const msgs = [];
    for (const o of targets(objs, b.S, B.targets_screen)) {
      await b.render('spec', props); b.settle(300);
      for (const e of ents) b.haPut(e, '', 'unavailable');
      b.settle(1500); b.events();
      const evs = b.tap(...centre(o));
      const calls = evs.filter((e) => e.name === 'ha');
      if (calls.length) msgs.push(`unavailable-fires: tap ${label(objs, o)} with ${ents.join(', ')} unavailable called ${calls.map((c) => JSON.parse(c.detail).service).join(', ')}`);
    }
    judge('spec', den, 'interaction', 'unavailable', vl, msgs);
  }
}

// device: a tap that navigates must put that screen on top of the stack; its Back button returns to the screen before
async function checkNav(b, evs, what) {
  const msgs = [];
  const nav = evs.find((e) => e.name === 'navigate');
  const target = String(nav.detail).split('?')[0];
  const st = b.stack();
  if (!st) return msgs;
  if (st[st.length - 1] !== target) msgs.push(`nav-wrong: tap ${what} asked for ${target}, the stack is [${st.join(', ')}]`);
  if (!evs.some((e) => e.name === 'screen' && e.detail === target)) msgs.push(`nav-no-screen: tap ${what} navigated to ${target} without a "screen" report`);
  // Back: the clickable nearest the top-left corner in the header row
  const objs = b.audit();
  const back = objs.filter((o) => o.clk && o.y < b.S * 0.15 && o.x < b.S * 0.3 && o.opa !== 0).sort((a, c) => a.x + a.y - (c.x + c.y))[0];
  if (!back) { msgs.push(`nav-no-back: ${target} (from ${what}) has no Back button in the header`); return msgs; }
  const before = st.slice(0, -1);
  const bev = b.tap(...centre(back));
  const st2 = b.stack();
  if (!bev.some((e) => e.name === 'back')) msgs.push(`nav-back-silent: Back on ${target} sent no "back" event`);
  if (JSON.stringify(st2) !== JSON.stringify(before)) msgs.push(`nav-back-wrong: Back on ${target} left [${st2.join(', ')}], expected [${before.join(', ')}]`);
  return msgs;
}

// device: Settings > Sounds off: no sound requests reach the host
async function muteTest(b, den) {
  if (!b.hasTest) return;
  await b.render('device', { start: 'home' }); b.settle(1500); b.events();
  const r = b.test('setting', 'sounds=false');
  b.settle(); b.events();
  const objs = b.audit();
  const msgs = [];
  for (const o of targets(objs, b.S, 4)) {
    await b.render('device', { start: 'home' }); b.settle(1500); b.events();
    b.test('setting', 'sounds=false'); b.settle(); b.events();  // every render is a fresh instance (settings at defaults)
    const snd = b.tap(...centre(o)).filter((e) => e.kind === 'sound');
    if (snd.length) msgs.push(`muted-sound: with Sounds off (setting ${r || 'applied'}), tap ${label(objs, o)} still requested ${snd.map(fmtEv).join(', ')}`);
  }
  b.test('setting', 'sounds=true');
  judge('device', den, 'sound', 'muted', 'default', msgs);
}

// scroll tests for every scroller the build shows (+ the fits check when none)
async function scrollTests(b, c, props, vl, den, lagOn, spec) {
  const id = c.id;
  await b.render(id, props); b.settle(1500); b.events();
  // scrollers: room to scroll in a direction they allow; labels (text taller than its box) and parts under an overlay
  // that covers them (a sheet over the list) are not scroll lists
  const objs0 = b.audit();
  const same = (o, s) => o.x === s.x && o.y === s.y && o.w === s.w && o.h === s.h;
  const under = (k, i) => { for (let j = k; j >= 0; j = objs0[j].p) if (j === i) return true; return false; };
  const overlays = objs0.filter((o, i) => typeof o.free === 'string' && /^overlay/i.test(o.free) && o.w > 0 && o.opa !== 0 && objs0.some((k, j) => j !== i && k.box && k.opa !== 0 && k.w * k.h > 0.2 * b.S * b.S && under(j, i)));
  const sc = b.scrolls().filter((s) => ((s.t + s.b > 0 && s.dir & 12) || (s.l + s.r > 0 && s.dir & 3)) && !objs0.some((o) => o.t === 'label' && same(o, s)));
  if (!sc.length) {
    if (spec.live || c.meta.motion) { judge(id, den, 'scroll', 'fits', vl, []); return; }  // live: things move anyway  // fits: a drag across the middle moves nothing (objects that move by themselves aside: live)
    const n = Math.ceil((b.S * 0.3) / B.drag_px_per_tick) + 3;
    const pos = () => b.audit().map((o) => `${o.x},${o.y}`);
    const p0 = pos(); b.tick(n); b.settle(2000); const p1 = pos();
    const self = new Set(p0.map((v, i) => (v !== p1[i] ? i : -1)));
    const q0 = pos();
    const evs = b.drag(b.S / 2, b.S * 0.65, 0, -b.S * 0.3);
    const q1 = pos();
    const moved = q0.length === q1.length && q0.some((v, i) => v !== q1[i] && !self.has(i));
    const draggy = evs.some((e) => e.kind === 'event' && (DRAGGY.has(e.name) || !NOT_ACTIONS.has(e.name)));
    judge(id, den, 'scroll', 'fits', vl, moved && !draggy ? [`scroll-when-fits: nothing overflows, but a vertical drag moved objects`] : []);
    return;
  }
  for (const [k, s0] of sc.entries()) {
    const vert = s0.t + s0.b > 0 && !!(s0.dir & 12);
    const si = objs0.findIndex((o) => same(o, s0));
    const inOverlay = (x, y) => overlays.some((o) => inBox(o, x, y) && !(si >= 0 && under(si, objs0.indexOf(o))));
    const name = `${vert ? 'v' : 'h'}${k}`;
    const total = vert ? s0.t + s0.b : s0.l + s0.r;
    const pos = (s) => (vert ? s.sy : s.sx);
    const find = () => b.scrolls().find((s) => s.x === s0.x && s.w === s0.w && s.h === s0.h && Math.abs(s.y - s0.y) < 2) || null;
    const msgs = [];
    // start point: inside the scroller, avoiding drag controls (try a few points)
    const pts = [[0.5, 0.7], [0.15, 0.7], [0.85, 0.7], [0.5, 0.4]].map(([fx, fy]) => [s0.x + s0.w * fx, Math.min(b.S - 8, s0.y + Math.min(s0.h, b.S - s0.y) * fy)]);
    let moved = false, used = null;
    if (pts.every(([x, y]) => inOverlay(x, y))) { judge(id, den, 'scroll', `moves-${name}`, vl, []); continue; }  // covered by a sheet: must NOT scroll
    for (const [x, y] of pts.filter(([x, y]) => !inOverlay(x, y))) {
      await b.render(id, props); b.settle(1500); b.events();
      const p0 = pos(find() || s0);
      const trace = [];
      if (lagOn && k === 0) b.rec = [];
      const dist = Math.min((vert ? s0.h : s0.w) * 0.5, total + 40);
      b.drag(x, y, vert ? 0 : -dist, vert ? -dist : 0, { after: false, onStep: () => { const s = find(); trace.push(s ? pos(s) : NaN); } });
      b.settle(2000);
      const frames = b.rec; b.rec = null;
      const s1 = find();
      if (!s1 || pos(s1) === p0) continue;
      moved = true; used = [x, y];
      // no jump: per step the content follows the finger (<= finger step + slack), never backwards
      const step = B.drag_px_per_tick;
      for (let i = 1; i < trace.length; i++) {
        const d = trace[i] - trace[i - 1];
        if (d < -B.scroll.slack) { msgs.push(`scroll-jump: ${name} went back ${-d} px in one step while the finger moved on`); break; }
        if (d > step + B.scroll.jump_slack) { msgs.push(`scroll-jump: ${name} moved ${d} px in one step (finger ${step} px)`); break; }
      }
      // a scrolling list redraws its whole box every frame (it all moves): only a redraw past the list counts as heavy
      if (frames && lagOn) judge(id, den, 'lag', `scroll-${name}`, vl, judgeLag(frames, b.S, { live: !!spec.live, budgets: { heavy_frac: B.lag.scroll_heavy_frac, ...(spec.budgets || {}) } }));
      break;
    }
    if (!moved) { judge(id, den, 'scroll', `moves-${name}`, vl, [`scroll-stuck: ${name} overflows by ${total} px but a drag did not move it`]); continue; }
    judge(id, den, 'scroll', `moves-${name}`, vl, msgs);
    // ends: a long fast drag past the end stops exactly at the end and stays; back past the start stops at 0
    const end = [];
    const [x, y] = used;
    await b.render(id, props); b.settle(1500); b.tick(80); b.events();  // tick: rows made after the first frame (a long items grid, facet-244) are all in before the end is measured
    const s00 = find() || s0;
    const max = pos(s00) + (vert ? s00.b : s00.r);
    for (let i = 0; i < Math.ceil((total + 200) / (b.S * 0.4)) + 1; i++) b.drag(x, y, vert ? 0 : -b.S * 0.4, vert ? -b.S * 0.4 : 0, { steps: 6, hold: 16 });
    b.settle(2500);
    let s = find();
    if (s && pos(s) !== max) end.push(`scroll-end: ${name} stopped at ${pos(s)} after flinging past the end (end = ${max})`);
    const hist = []; for (let i = 0; i < 20; i++) { b.tick(); const q = find(); hist.push(q ? pos(q) : NaN); }
    if (new Set(hist).size > 1) end.push(`scroll-settle: ${name} still moving after it reached the end (${[...new Set(hist)].slice(0, 5).join(', ')})`);
    const top = [vert ? x : s0.x + 8, vert ? Math.max(s0.y, 0) + 8 : y];
    const reach = vert ? Math.min(b.S * 0.4, b.S - 8 - top[1]) : Math.min(b.S * 0.4, b.S - 8 - top[0]);
    for (let i = 0; i < Math.ceil((total + 200) / reach) + 1; i++) b.drag(top[0], top[1], vert ? 0 : reach, vert ? reach : 0, { steps: 6, hold: 16 });
    b.settle(2500);
    s = find();
    if (s && pos(s) !== 0) end.push(`scroll-start: ${name} stopped at ${pos(s)} after flinging past the start (start = 0)`);
    judge(id, den, 'scroll', `ends-${name}`, vl, end);
    // keeps its place on an update (same props, and HA values for spec screens)
    const keep = [];
    await b.render(id, props); b.settle(1500); b.events();
    b.drag(x, y, vert ? 0 : -Math.min(total / 2, b.S * 0.3), vert ? -Math.min(total / 2, b.S * 0.3) : 0, { steps: 8 });
    b.settle(2500);
    const mid = find();
    if (mid) {
      const u = b.update(id, props);
      if (id === 'spec') { const wants = JSON.parse(b.m.UTF8ToString(b.m._fw_ha_wants()) || '[]'); for (const [e, a] of wants) b.haPut(e, a, a ? '3' : 'on'); }
      b.settle(1500);
      const after = find();
      if (!after || pos(after) !== pos(mid)) keep.push(`scroll-lost-on-update: ${name} was at ${pos(mid)}, after an update (${u || 'in place'}) it is at ${after ? pos(after) : 'gone'}`);
    }
    judge(id, den, 'scroll', `keeps-${name}`, vl, keep);
    // nested: another scroller of the same direction around this one never moves in the same drag frames
    const outer = sc.find((o, j) => j !== k && (vert ? o.t + o.b > 0 : o.l + o.r > 0) && o.x <= s0.x && o.y <= s0.y && o.x + o.w >= s0.x + s0.w && o.y + o.h >= s0.y + s0.h);
    if (outer) {
      await b.render(id, props); b.settle(1500); b.events();
      const fo = () => b.scrolls().find((q) => q.x === outer.x && q.w === outer.w && q.h === outer.h);
      let both = 0, pi = pos(find() || s0), po = pos(fo() || outer);
      b.drag(x, y, vert ? 0 : -b.S * 0.5, vert ? -b.S * 0.5 : 0, { onStep: () => { const a = find(), o = fo(); if (a && o && pos(a) !== pi && pos(o) !== po) both++; if (a) pi = pos(a); if (o) po = pos(o); } });
      judge(id, den, 'scroll', `nested-${name}`, vl, both ? [`scroll-fight: ${name} and the scroller around it both moved in ${both} drag steps`] : []);
    }
  }
}

// ---------------------------------------------------------------- custom tests (tests/behaviour/<id>.json "tests")
// {name, kind, den?, props?, steps: [{tap|long|press|release|drag: sel, by?: [dx,dy]} | {wait: ms} | {update: props}
//  | {ha: [e, a, v]} | {setting: "id=value"}], expect: {events?: [[name, detailRegex?]...] (in order, others may
//  come between), no_events?: [name...], sounds?: [name...] (exact, ticks aside), ticks_min?: n, silent?: true,
//  stack?: [ids], exists?: sel, absent?: sel, scroll?: {sel, at: "start"|"end"|n}}}
// sel: {text: "Start"|"/re/"} (nearest clickable around that label, else the label) | {type: "arc", i?} |
//      {clk: i} (i-th clickable) | {hit: "dial", i?} (fw_hits component of a spec screen) | {comp: "toggle", i?} (the i-th
//      component of that id, its box) | {at: [fx, fy]} (stage fractions)
function resolveSel(b, sel) {
  if (!sel) return null;
  const S = b.S;
  if (sel.at) return { x: sel.at[0] * S, y: sel.at[1] * S, w: 1, h: 1 };
  const objs = b.audit();
  const vis = (o) => o.w > 0 && o.h > 0 && o.opa !== 0;
  if (sel.text !== undefined) {
    const re = String(sel.text).startsWith('/') ? new RegExp(String(sel.text).slice(1, -1)) : null;
    const ls = objs.map((o, i) => [o, i]).filter(([o]) => vis(o) && o.text !== undefined && (re ? re.test(o.text) : o.text === sel.text));
    const pick = ls[sel.i || 0];
    if (!pick) return null;
    for (let k = pick[0].p; k >= 0; k = objs[k].p) if (objs[k].clk) return objs[k];
    return pick[0];
  }
  if (sel.type) return objs.filter((o) => vis(o) && o.t === sel.type)[sel.i || 0] || null;
  if (sel.clk !== undefined) return objs.filter((o) => vis(o) && o.clk)[sel.clk] || null;
  if (sel.hit) return b.hits().filter((h) => h.c === sel.hit)[sel.i || 0] || null;
  if (sel.comp) return JSON.parse(b.m.UTF8ToString(b.m._fw_comps(b.stage)) || '[]').filter((c) => c.ids.at(-1) === sel.comp)[sel.i || 0] || null;
  return null;
}
function matchEvents(evs, want) {  // ordered subsequence
  let i = 0;
  for (const e of evs) {
    if (i >= want.length) break;
    const [n, d] = want[i];
    if (e.kind === 'event' && e.name === n && (d === undefined || new RegExp(d).test(String(e.detail)))) i++;
  }
  return i >= want.length ? null : want[i];
}
async function customTests(c, den, spec, m) {
  const tests = (spec.tests || []).filter((t) => (!t.den || t.den === den) && KINDS.includes(t.kind));
  if (!tests.length) return;
  const defaults = Object.fromEntries((c.meta.props || []).map((p) => [p.name, p.default]));
  const b = new Bench(m, den);
  for (const t of tests) {
    const msgs = [];
    const props = { ...defaults, ...(t.props || {}) };
    const err = await b.render(c.id, props);
    for (const h of t.before_ha || []) b.haPut(...h);
    if (t.before_spec && !err) { for (const x of t.before_spec) b.spec(...x); b.rebuild(c.id, props); }  // specs sent first (a fresh module each test): built with them
    if (err) { judge(c.id, den, t.kind, t.name, 'custom', [`render-error: ${err}`]); continue; }
    b.settle(1500); b.events();
    const evs = [];
    let down = null;
    b.rec = (t.expect || {}).redraw_max !== undefined ? [] : null;  // expect.redraw_max: the biggest share of the screen one frame may redraw during the steps
    for (const st of t.steps || []) {
      const op = ['tap', 'long', 'press', 'release', 'drag'].find((k) => st[k] !== undefined);
      if (op) {
        const o = op === 'release' ? null : resolveSel(b, st[op]);
        if (op !== 'release' && !o) { msgs.push(`no-target: ${JSON.stringify(st[op])} not found`); break; }
        const [x, y] = o ? centre(o) : down;
        if (o && (x < 0 || y < 0 || x >= b.S || y >= b.S)) { msgs.push(`no-target: ${JSON.stringify(st[op])} is off the screen (${x}, ${y})`); break; }
        if (op === 'tap') evs.push(...b.tap(x, y));
        else if (op === 'long') evs.push(...b.long(x, y));
        else if (op === 'press') { b.touch(x, y, 1); b.tick(4); down = [x, y]; evs.push(...b.events()); }
        else if (op === 'release') { b.touch(down[0], down[1], 0); b.tick(2); b.settle(); evs.push(...b.events()); }
        else { const [dx, dy] = (st.by || [0, 0]).map((v) => (Math.abs(v) <= 1 ? v * b.S : v)); evs.push(...b.drag(x, y, dx, dy)); }
      } else if (st.wait) { b.tick(Math.ceil(st.wait / T)); evs.push(...b.events()); }
      else if (st.update) { const u = b.update(c.id, { ...props, ...st.update }); if (u === 'rebuild') await b.render(c.id, { ...props, ...st.update }); b.settle(1500); evs.push(...b.events()); }
      else if (st.ha) { b.haPut(...st.ha); b.settle(1500); evs.push(...b.events()); }
      else if (st.setting) { b.test('setting', st.setting); b.settle(); evs.push(...b.events()); }
      else if (st.spec) { b.spec(...st.spec); b.settle(); evs.push(...b.events()); }  // send / revert a spec (core/main.cpp fw_spec)
      else if (st.render) { b.rebuild(c.id, props); b.settle(1500); b.events(); }  // build it again, state kept (what the device does after a deck change)
    }
    const x = t.expect || {};
    if (x.events) { const miss = matchEvents(evs, x.events); if (miss) msgs.push(`${t.miss || 'missing-event'}: expected ${miss[0]}${miss[1] ? ` ~ ${miss[1]}` : ''}; got ${evs.filter((e) => e.kind === 'event').map(fmtEv).join(', ') || 'nothing'}`); }
    if (x.no_events) for (const n of x.no_events) { const e = evs.find((e) => e.kind === 'event' && e.name === n); if (e) msgs.push(`${t.miss || 'unexpected-event'}: ${fmtEv(e)} fired`); }
    const sounds = evs.filter((e) => e.kind === 'sound' && !isTick(e)).map((e) => e.name);
    if (x.sounds && JSON.stringify(sounds) !== JSON.stringify(x.sounds)) msgs.push(`${t.miss || 'wrong-sound'}: expected sounds [${x.sounds.join(', ')}], got [${sounds.join(', ')}]`);
    if (x.ticks_min && evs.filter((e) => e.kind === 'sound' && isTick(e)).length < x.ticks_min) msgs.push(`${t.miss || 'few-ticks'}: expected >= ${x.ticks_min} step ticks`);
    if (x.silent && evs.length) msgs.push(`${t.miss || 'update-noise'}: expected nothing, got ${evs.map(fmtEv).join(', ')}`);
    if (x.stack) { const s = b.stack(); if (s && JSON.stringify(s) !== JSON.stringify(x.stack)) msgs.push(`${t.miss || 'nav-wrong'}: stack [${s.join(', ')}], expected [${x.stack.join(', ')}]`); }
    if (x.exists && !resolveSel(b, x.exists)) msgs.push(`${t.miss || 'missing-part'}: ${JSON.stringify(x.exists)} not shown`);
    if (x.redraw_max !== undefined && b.rec) {
      const worst = Math.max(0, ...b.rec.filter((f) => f.renders).map((f) => f.px)) / (b.S * b.S);
      if (worst > x.redraw_max) msgs.push(`${t.miss || 'redraw-too-big'}: a frame redrew ${Math.round(worst * 100)}% of the screen, budget ${Math.round(x.redraw_max * 100)}%`);
      b.rec = null;
    }
    if (x.absent && resolveSel(b, x.absent)) msgs.push(`${t.miss || 'still-shown'}: ${JSON.stringify(x.absent)} still shown`);
    judge(c.id, den, t.kind, t.name, 'custom', msgs);
  }
}


// ---------------------------------------------------------------- expected fails, the report, the rule
const specs = {};
export function xfailOf(r) {  // the id's xfail entry matching this result's key, if any
  const sp = (specs[r.id] ||= specOf(r.id));
  const k = Object.keys(sp.xfail || {}).find((p) => glob(p, r.key));
  return k ? sp.xfail[k] : null;
}
export function crash(c, den, e) { judge(c.id, den, 'interaction', 'crash', 'default', [`runner-crash: ${String((e && e.message) || e).slice(0, 200)}`]); }
const all = [];
function report(rs, r) {
  for (const x of rs) {
    const xf = xfailOf(x), at = `${x.id} [${x.den}] ${x.key}`;
    if (process.env.FW_BEHAVIOUR_JSON) all.push({ ...x, ...(xf ? { xfail: xf } : {}) });
    if (xf) { r.check(true, at); if (x.ok) r.note(`XPASS ${at} (${xf}): passes now, remove it from tests/behaviour/${x.id}.json xfail`); continue; }
    r.check(x.ok, x.ok ? at : `${at}: ${x.msgs[0]}`);
    for (const msg of x.ok ? [] : x.msgs.slice(1, 3)) r.note(`FAIL ${at}: ${msg}`);
  }
}
const ids = new Map();  // `${den}|${id}` -> the id's shared state, from its first case to its last
export default {
  id: 'behaviour',
  about: 'Taps, long presses, presses slid off, sounds, the update path and scrolling do what they should (no lag: bench --lag)',
  tiers: ['fast', 'gate'],
  ownStage: true,
  looks: [0],
  wants: (c) => ['components', 'screens'].includes(c.section) && !B.skip.includes(c.id),
  async run(x, r) {
    const { m, den, c, v, vi } = x, k = `${den}|${c.id}`;
    setKinds(['interaction', 'scroll', 'sound']);
    if (!ids.has(k)) ids.set(k, { s: open(c, den, m), picks: new Set(picks(c)) });
    const o = ids.get(k);
    try {
      if (!o.dead && o.picks.has(vi)) await variant(o.s, v);
      if (!o.dead && vi === (c.meta.variants || []).length) await close(o.s);
    } catch (e) { crash(c, den, e); o.dead = true; }  // one crash ends the id's tests
    if (vi === (c.meta.variants || []).length) ids.delete(k);
    report(take(), r);
  },
  finish() { if (process.env.FW_BEHAVIOUR_JSON) writeFileSync(process.env.FW_BEHAVIOUR_JSON, JSON.stringify(all, null, 1)); },
};
