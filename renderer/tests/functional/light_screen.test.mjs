// Light screen (specs/light.json: Page > Header (back + the light's name) > Body (centre) > Dial), release slice 1
// (2026-10-26). Each test is one row of the release gate and fails without the behaviour. Home Assistant is faked
// (tools/functional.mjs FakeHA); E is NOT one of the demo entities (any light works, not Darren's ids).
// Approved original: Astro packages/ui/src/screens/light.ts (dial centred in the content box; brightness live at most
// every 200 ms + the final value; HA updates held off 3 s after a local change).
import { bounds, fixtures, faults, fuzz, monkey, leak, look, frame565, diff565, golden, frames, seeded } from './_release.mjs';

const E = 'light.test_lamp', SW = 'switch.test_plug', ONOFF = 'light.test_onoff';
const LAMP = { '': 'on', brightness: '179', friendly_name: 'Desk lamp', supported_color_modes: '["brightness"]' };  // 70 %
const den = (t) => (t.b.S > 600 ? '1.5x' : '1x');
const TOK = { '1.5x': { inset: 32, top: 112, scale: 560, arc: 432, target: 64 }, '1x': { inset: 20, top: 72, scale: 376, arc: 288, target: 44 } };
const K = (t) => TOK[den(t)];
async function open(t, ent = E, st = LAMP, extra = {}) {
  t.ha.s[ent] = { ...st };
  await t.mount('spec', { screen: 'light', entity: ent, ...extra });
  return t;
}
const dial = (t) => t.part('dial');
const val = (t) => { const v = t.texts().filter((x) => /^-?\d+$/.test(x)); return v.length ? Number(v[0]) : null; };  // the scale numbers are drawn (Pen), so the one numeric text is the value
// a point at fraction f of the 270 deg range, r px from the dial's centre
const pt = (t, f, r) => { const b = dial(t), a = ((135 + 270 * f) * Math.PI) / 180; return [Math.round(b.x + b.w / 2 + r * Math.cos(a)), Math.round(b.y + b.h / 2 + r * Math.sin(a))]; };
const onRing = (t, f) => pt(t, f, K(t).arc / 2 - 3);
// the dial's own pixels (the stage around it is the workbench's, not the component's)
const crop = (t, w, b) => { const out = []; for (let y = b.y; y < b.y + b.h; y++) for (let x = b.x; x < b.x + b.w; x++) out.push(w[y * t.b.S + x]); return out; };
const calls = (t, svc) => t.ha.calls.filter((c) => !svc || c.service === svc);
const pctOf = (c) => c?.data?.brightness_pct;
// drag along the ring from f0 to f1 over ms, one touch per tick; returns [{at, service, pct}] of the HA calls as sent
function ringDrag(t, f0, f1, ms) {
  const n = Math.max(2, Math.round(ms / 16)), sent = [], all = [];
  const grab = (evs) => { for (const e of evs) { all.push(e); if (e.kind === 'event' && e.name === 'ha') { const d = JSON.parse(e.detail); sent.push({ at: t.b.t, service: d.service, pct: pctOf(d) }); } } };
  const [x0, y0] = onRing(t, f0); t.b.touch(x0, y0, 1); t.b.tick(3); grab(t.b.events());
  for (let i = 1; i <= n; i++) { const [x, y] = onRing(t, f0 + ((f1 - f0) * i) / n); t.b.touch(x, y, 1); t.b.tick(); grab(t.b.events()); }
  const [x1, y1] = onRing(t, f1); t.b.touch(x1, y1, 0); t.b.tick(2); grab(t.b.events());
  for (let i = 0; i < 40; i++) { t.b.tick(); grab(t.b.events()); }
  t.ha.react(all); t.b.settle(1500); t.b.events();
  return sent;
}
const deck = (tap = 'detail') => ({ v: 2, rev: 9, start: 'p', pages: [{ id: 'p', kind: 'home', name: 'Test', cards: [{ t: 'light', e: E, size: 's', tap, label: 'Desk lamp' }] }] });

export default {
  id: 'light_screen', family: 'screens', about: 'The Light screen: header + the dial centred; states, HA calls, hold-off, flows, proofs.',
  tests: [
    // ---------------------------------------------------------------- 1 contract: layout + bounds
    { name: 'layout: header = back + the light\'s friendly name (bound), dial centred in the body, capped by dial-scale', async run(t) {
      await open(t);
      t.has(t.texts(), /^desk lamp$/i, 'header name from HA'); t.ok(t.part('back_button'), 'back button');
      const b = dial(t), k = K(t), S = t.b.S, w = Math.min(S - 2 * k.inset, k.scale);
      t.eq([b.w, b.h], [w, w], 'dial = min(content width, dial-scale), square');
      t.eq([b.x + b.w / 2, b.y + b.h / 2], [S / 2, (k.top + S - k.inset) / 2], 'centred in the body content box');
    } },
    { name: 'layout: the header follows a rename mid-view (bound name)', async run(t) {
      await open(t); t.ha.set(E, undefined, { friendly_name: 'Study lamp' }); t.has(t.texts(), /^study lamp$/i, 'renamed'); t.hasNot(t.texts(), /^desk lamp$/i, 'old name gone');
    } },
    { name: 'bounds proof: 0 fails at insets 0 .. 64 (contract part: the body)', async run(t) {
      await open(t); t.eq(bounds(t, [null, 0, 8, 16, 20, 24, 32, 48, 64]), [], 'bounds');
    } },
    { name: 'hit areas: the whole inner edge of the ring is the ring, the button is the button (no overlap)', async run(t) {
      await open(t);
      const k = K(t), r = k.arc / 2 - k.target + 2, bad = [];
      for (let i = 0; i <= 20; i++) { const ev = t.tap({ at: pt(t, i / 20, r) }); if (!ev.some((e) => e.name === 'change')) bad.push(`f ${i / 20}: not the ring`); }
      t.eq(bad, [], 'every point of the ring\'s inner edge sets the value (none swallowed by the button)');
      t.tap({ at: onRing(t, 0.7) });
      const btn = t.find('ON'), box = t.objs().filter((o) => o.clk && o.x <= btn.x && o.y <= btn.y && o.x + o.w >= btn.x + btn.w && o.y + o.h >= btn.y + btn.h).sort((a, z) => a.w * a.h - z.w * z.h)[0];
      const d = dial(t), cx = d.x + d.w / 2, cy = d.y + d.h / 2;
      for (const [x, y] of [[box.x, box.y], [box.x + box.w - 1, box.y], [box.x, box.y + box.h - 1], [box.x + box.w - 1, box.y + box.h - 1]]) {
        const dist = Math.hypot(x - cx, y - cy), deg = ((Math.atan2(y - cy, x - cx) * 180) / Math.PI + 360) % 360;
        t.ok(dist < r || (deg > 45 && deg < 135), `button corner ${x},${y}: inside the dead zone (${Math.round(dist)} < ${r}) or in the bottom gap (${Math.round(deg)} deg)`);
      }
      // the button's edges (mid points, 2 px in: its rounded corners are not part of it, as on any LVGL object)
      for (const [x, y] of [[box.x + 2, box.y + box.h / 2], [box.x + box.w - 3, box.y + box.h / 2], [box.x + box.w / 2, box.y + 2], [box.x + box.w / 2, box.y + box.h - 3]]) {
        const ev = t.tap({ at: [x, y] }); t.emitted(ev, 'toggle', undefined, `edge ${x},${y} = the button`); t.notEmitted(ev, 'change', `edge ${x},${y} not the ring`);
      }
    } },
    // ---------------------------------------------------------------- 2 functional: states
    { name: 'state on: brightness % shown, ON', async run(t) { await open(t); t.eq(val(t), 70, 'value'); t.has(t.texts(), '%', 'unit'); t.has(t.texts(), 'ON', 'button'); } },
    { name: 'state off: 0, OFF', async run(t) { await open(t, E, { ...LAMP, '': 'off', brightness: null }); t.eq(val(t), 0, 'value'); t.has(t.texts(), 'OFF', 'button'); } },
    { name: 'state unavailable: disabled look, "Unavailable", no button, ring blocked, nothing sent', async run(t) {
      await open(t, E, { ...LAMP, '': 'unavailable' }); t.has(t.texts(), 'Unavailable', 'word'); t.hasNot(t.texts(), 'ON', 'no button');
      const ev = t.tap({ at: onRing(t, 0.5) }); t.emitted(ev, 'blocked'); t.sounds(ev, ['error']); t.eq(calls(t), [], 'no HA call');
    } },
    { name: 'state on/off-only light: On / Off, no unit, drag sends nothing, the button toggles', async run(t) {
      await open(t, ONOFF, { '': 'on', friendly_name: 'Hall', supported_color_modes: '["onoff"]' });
      t.has(t.texts(), 'On', 'On'); t.hasNot(t.texts(), '%', 'no unit');
      ringDrag(t, 0.2, 0.8, 600); t.eq(calls(t), [], 'drag sends nothing');
      t.tap('ON'); t.wait(600); t.call({ service: 'light.turn_off', entity: ONOFF }, 'button off'); t.has(t.texts(), 'Off', 'Off');
    } },
    { name: 'state switch.*: toggle only, switch.turn_off / switch.turn_on', async run(t) {
      await open(t, SW, { '': 'on', friendly_name: 'Plug' });
      t.has(t.texts(), 'On', 'On'); ringDrag(t, 0.2, 0.8, 400); t.eq(calls(t), [], 'no drag');
      t.tap('ON'); t.wait(600); t.call({ service: 'switch.turn_off', entity: SW }, 'off');
      t.tap('OFF'); t.wait(600); t.call({ service: 'switch.turn_on', entity: SW }, 'on');
      t.eq(calls(t).filter((c) => c.service.startsWith('light.')).length, 0, 'never a light service on a switch');
    } },
    { name: 'state missing entity: "Not found", no crash, nothing sent', async run(t) {
      await open(t, 'light.nope', { '': '' }); t.has(t.texts(), 'Not found', 'empty state'); t.tap({ at: onRing(t, 0.5) }); t.eq(calls(t), [], 'no call');
    } },
    // ---------------------------------------------------------------- 2 functional: interaction + HA calls
    { name: 'nothing is sent on open (4 s)', async run(t) { await open(t); const ev = t.wait(4000); t.notEmitted(ev, 'ha', 'no HA call on open'); } },
    { name: 'drag: light.turn_on brightness at most every 200 ms + the final value', async run(t) {
      await open(t);
      const s = ringDrag(t, 0.3, 0.9, 1400).filter((c) => c.service === 'light.turn_on');
      t.ok(s.length >= 4, `live sends while dragging: ${s.length}`);
      const live = s.slice(0, -1), gaps = live.slice(1).map((c, i) => c.at - live[i].at), tooFast = gaps.filter((g) => g < 200 - 16);
      t.eq(tooFast, [], 'no two live sends closer than 200 ms (one 16 ms tick of slack); the release sends the final value at once');
      t.ok(Math.abs(s.at(-1).pct - 90) <= 1, `final value sent: ${s.at(-1).pct}`); t.eq(val(t), s.at(-1).pct, 'shown = sent');
    } },
    { name: 'tap the centre button: light.turn_off, then light.turn_on back at the value', async run(t) {
      await open(t); t.tap('ON'); t.wait(500); t.call({ service: 'light.turn_off', entity: E }, 'off'); t.eq(val(t), 0, 'shows 0');
      t.tap('OFF'); t.wait(600); t.call({ service: 'light.turn_on', entity: E }, 'on'); t.eq(val(t), 70, 'back at 70');
    } },
    { name: 'hold-off: a stale HA echo within 3 s does not jump the dial back; after 3 s HA wins', async run(t) {
      await open(t); t.ha.auto = false;
      ringDrag(t, 0.7, 0.9, 400); const mine = val(t); t.ok(Math.abs(mine - 90) <= 1, `dragged to ${mine}`);
      t.ha.set(E, 'on', { brightness: 128 }); t.eq(val(t), mine, 'stale 50 % held off');
      t.wait(3200); t.ha.set(E, 'on', { brightness: 128 }); t.eq(val(t), 50, 'after 3 s: what HA says');
    } },
    { name: 'external change: animates to the new value silently, sends nothing', async run(t) {
      await open(t); const before = t.ha.calls.length;
      t.ha.set(E, 'on', { brightness: 64 }); const ev = t.wait(800); t.silent(ev); t.eq(val(t), 25, 'new value'); t.eq(t.ha.calls.length, before, 'nothing sent');
      t.ha.set(E, 'off', { brightness: null }); t.eq(val(t), 0, 'off from outside'); t.has(t.texts(), 'OFF', 'button follows');
    } },
    // ---------------------------------------------------------------- 6 robustness: HA faults
    { name: 'HA faults: wrong types, out of range, dropped attribute, out-of-order echoes: no crash, value in range', async run(t) {
      await open(t);
      for (const v of ['abc', '999', '-5', '1e9']) { faults.outOfRange(t, E, 'brightness', v); const x = val(t); t.ok(x !== null && x >= 0 && x <= 100, `brightness ${v} -> ${x}`); }
      faults.dropped(t, E, 'brightness'); t.ok(val(t) !== null, 'dropped brightness');
      faults.outOfOrder(t, E, { brightness: 255 }, { brightness: 26 }); t.eq(val(t), 10, 'the last report wins (HA order)');
      faults.unknown(t, E); t.has(t.texts(), 'Unavailable', 'unknown = unavailable');
      t.ha.set(E, 'on', { brightness: 179 }); t.eq(val(t), 70, 'recovers');
    } },
    { name: 'HA disconnect / reconnect: Unavailable, then back by itself', async run(t) {
      await open(t); t.ha.disconnect(); t.has(t.texts(), 'Unavailable', 'down'); t.ha.reconnect(); t.eq(val(t), 70, 'back'); t.has(t.texts(), 'ON', 'button back');
    } },
    { name: 'long and non-ASCII names: shown or cut by rule, never outside the header', async run(t) {
      for (const [k, n] of Object.entries(fixtures).filter(([k]) => k !== 'empty')) {
        await open(t, E, { ...LAMP, friendly_name: n });
        const hdr = t.part('header'); const lab = t.objs().find((o) => o.text && t.visible(o) && o.y < K(t).top && o.text !== 'ON' && !/^\d/.test(o.text) && o.x > 40);
        t.ok(hdr && lab && lab.x + lab.w <= hdr.x + hdr.w, `${k}: label inside the header`);
        t.eq(bounds(t), [], `${k}: bounds`);
      }
    } },
    { name: 'fuzz: 150 random HA states + names on one live screen (seed 11): no crash, bounds clean', async run(t) {
      await open(t);
      const st = ['on', 'off', 'unavailable', 'unknown', '', 'on', 'on'], br = [null, '0', '1', '128', '255', '256', '-4', 'abc', '1e9'];
      const cm = [null, '["onoff"]', '["brightness"]', '["hs","color_temp"]', '[]', 'garbage'];
      t.eq(await fuzz(t, 150, 11, (r) => t.ha.set(E, r.pick(st), { brightness: r.pick(br), supported_color_modes: r.pick(cm), friendly_name: r.pick(Object.values(fixtures)) })), [], 'fuzz');
    } },
    { name: 'monkey: 250 random taps / holds / drags / flicks (seed 7): no crash, no stuck animation', async run(t) {
      await open(t);
      t.eq(monkey(t, 250, 7, (t) => (t.objs().length ? null : 'the stage is empty')), [], 'monkey');
      t.b.settle(3000); t.eq(t.b.m._fw_anims(), 0, 'animations settle');
    } },
    // ---------------------------------------------------------------- 10 product level: flows
    { name: 'flow: a Home tile opens the light screen for its entity, nothing sent; Back = Home; reopen shows HA now', async run(t) {
      t.ha.s[E] = { ...LAMP }; await t.device(deck());
      const n0 = t.stack().length; let ev = t.tap('Desk lamp');
      t.eq(t.where(), 'light', 'opened'); t.eq(t.stack().length, n0 + 1, 'one level up'); t.notEmitted(ev.concat(t.wait(1500)), 'ha', 'nothing sent on open');
      t.has(t.texts(), /^desk lamp$/i, 'header'); t.eq(val(t), 70, 'value');
      t.tap(t.part('back_button')); t.eq(t.stack().length, n0, 'Back = Home');
      t.ha.set(E, 'on', { brightness: 64 }); t.tap('Desk lamp'); t.eq(val(t), 25, 'reopen shows HA now');
    } },
    { name: 'flow: deep link (HA show action) opens it; hold Back = Home; 20 rapid open / back: no crash, stack sane', async run(t) {
      t.ha.s[E] = { ...LAMP }; await t.device(deck());
      const n0 = t.stack().length; t.show('light'); t.eq(t.where(), 'light', 'shown');
      t.hold(t.part('back_button')); t.eq(t.stack().length, n0, 'hold Back = Home');
      for (let i = 0; i < 20; i++) { t.b.tap(...[t.find('Desk lamp')].map((o) => [o.x + o.w / 2, o.y + o.h / 2])[0], 16); t.b.tick(3); const b = t.part('back_button'); if (b) t.b.tap(b.x + b.w / 2, b.y + b.h / 2, 16); t.b.tick(3); }
      t.b.settle(3000); t.eq(t.stack().length, n0, 'back at Home'); t.ok(t.texts().includes('Desk lamp'), 'Home shows');
    } },
    { name: 'leak: 150 open / close cycles of the light screen: objects back, heap flat', async run(t) {
      t.ha.s[E] = { ...LAMP }; await t.device(deck());
      const go = () => { t.show('light'); }, back = () => { t.b.str('', (p) => t.b.m._fw_bench_nav(p, 1)); t.b.settle(600); };
      t.eq(leak(t, 150, go, back, 48 * 1024), [], 'leak');
    } },
    // ---------------------------------------------------------------- 3/4/5 proofs: theme x accent, golden
    { name: 'colour + pixel proof: the dial switched LIVE through every mode x accent = a fresh build, every pixel', async run(t) {
      const P = { value: 64, opening: false }, bad = [];
      await t.mount('dial', P);
      for (const light of [0, 1]) for (let a = 0; a < 5; a++) {
        look(t, light, a); const box = t.part('dial'), live = crop(t, frame565(t), box);
        await t.b.render('dial', P, light); t.b.str('dial', (c) => t.b.str(JSON.stringify(P), (p) => t.b.m._fw_render(t.b.stage, c, p, light, a))); t.b.settle(1500);
        const fresh = crop(t, frame565(t), box), n = diff565(live, fresh); if (n) bad.push(`${light ? 'light' : 'dark'} accent ${a}: ${n} px differ`);
        await t.mount('dial', P); look(t, light ? 0 : 1, (a + 2) % 5);  // the next switch starts from another look
      }
      t.eq(bad, [], 'live switch = fresh build');
    } },
    { name: 'colour + pixel proof: the whole screen switched live = a fresh build', xfail: 'the Header is not on the contract yet (it holds its colours): next slice', async run(t) {
      await open(t); look(t, 1, 2); const live = frame565(t);
      await t.b.render('spec', { screen: 'light', entity: E }, 1); t.b.str('spec', (c) => t.b.str(JSON.stringify({ screen: 'light', entity: E }), (p) => t.b.m._fw_render(t.b.stage, c, p, 1, 2)));
      t.ha.push(); t.b.settle(1500); t.eq(diff565(live, frame565(t)), 0, 'px differ');
    } },
    { name: 'golden images: on, off, unavailable, on/off only, missing (dark + light)', async run(t, { den }) {
      const out = [];
      const cases = [['on', E, LAMP], ['off', E, { ...LAMP, '': 'off', brightness: null }], ['unavailable', E, { ...LAMP, '': 'unavailable' }],
        ['onoff', ONOFF, { '': 'on', friendly_name: 'Hall', supported_color_modes: '["onoff"]' }], ['missing', 'light.nope', { '': '' }]];
      for (const light of [0, 1]) for (const [n, e, s] of cases) {
        t.ha.s[e] = { ...s }; await t.b.render('spec', { screen: 'light', entity: e, opening: false }, light); t.b.settle(1500); t.ha.push(); t.b.settle(1500);
        out.push(...golden(t, 'light_screen', `${n}-${light ? 'light' : 'dark'}`, den, [t.part('screen_chrome')].filter(Boolean)));  // the live clock masked
      }
      t.eq(out, [], 'goldens');
    } },
    // ---------------------------------------------------------------- 6 performance (WASM proxy; device fps = later)
    { name: 'perf: a drag repaints only the arc span + knob (no full redraw), per-frame dirty area small', async run(t) {
      await open(t); let f;
      const s = frames(t, () => { f = ringDrag(t, 0.3, 0.6, 800); });
      t.log.push(`perf ${den(t)}: ${s.n} frames, WASM ${s.ms50.toFixed(2)} ms p50 / ${s.ms95.toFixed(2)} p95, dirty px p50 ${s.px50}, max ${s.pxMax}, full ${s.full}`);
      t.eq(s.full, 0, 'no full-screen redraw while dragging');
      const S = t.b.S, d = dial(t); t.ok(s.px50 < d.w * d.h * 0.25, `p50 dirty ${s.px50} px < a quarter of the dial (a whole-dial repaint = ${d.w * d.h})`);
    } },
  ],
};
