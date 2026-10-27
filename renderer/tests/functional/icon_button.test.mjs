// Icon button (components/icon_button.cpp): round / square = tap(icon) + tap; play toggles play / pause with
// toggle-on / toggle-off; disabled + unavailable block; sizes per style; update path silent.
import { D } from './_controls.mjs';
import { leak, golden, stage, boundsAll, liveLook, lab, tune, frame565, diff565 } from './_release.mjs';
const sq = (den) => D(den, 48, 32);
async function at(t, props, w, h) { t.b.W = t.b.H = undefined; await t.mount('icon_button', props); stage(t, w, h ?? w); t.b.settle(800); return t.part('icon_button'); }
const STATES = ['default', 'pressed', 'active', 'active-pressed', 'disabled', 'unavailable'];
const den = (t) => (t.b.S > 600 ? '1.5x' : '1x');
const toggles = (ev) => ev.filter((e) => e.kind === 'event' && e.name === 'toggle').map((e) => e.detail);
export default {
  id: 'icon_button', family: 'controls', about: 'Icon-only button: round, square or the big play / pause.',
  tests: [
    { name: 'round: tap(icon) + tap sound', mount: ['icon_button', { style: 'round', icon: 'next' }], run(t) {
      const ev = t.tap({ clk: 0 }); t.emitted(ev, 'tap', 'next'); t.sounds(ev, ['tap']);
    } },
    { name: 'square close: tap(close)', mount: ['icon_button', { style: 'square', icon: 'close' }], run(t) { t.emitted(t.tap({ clk: 0 }), 'tap', 'close'); } },
    { name: 'play: tap = toggle(play) + toggle-on, again = toggle(pause) + toggle-off', mount: ['icon_button', { style: 'play', icon: 'play' }], run(t) {
      let ev = t.tap({ clk: 0 }); t.eq(toggles(ev), ['play'], 'first'); t.sounds(ev, ['toggle-on']);
      ev = t.tap({ clk: 0 }); t.eq(toggles(ev), ['pause'], 'second'); t.sounds(ev, ['toggle-off']);
    } },
    { name: 'play active (playing): first tap pauses', mount: ['icon_button', { style: 'play', icon: 'play', state: 'active' }], run(t) {
      t.eq(toggles(t.tap({ clk: 0 })), ['pause'], 'toggle');
    } },
    { name: 'play: the glyph swaps play <-> pause', mount: ['icon_button', { style: 'play', icon: 'play' }], run(t) {
      const shown = () => t.objs().filter((o) => o.t === 'image').map((o) => (o.opa !== 0 ? 1 : 0)).join('');  // which glyph is visible
      const a = shown(); t.tap({ clk: 0 }); const b = shown(); t.ok(a !== b, `glyph did not change (${a})`); t.tap({ clk: 0 }); t.eq(shown(), a, 'back to play');
    } },
    { name: 'hold then release: one tap only', mount: ['icon_button', { style: 'round', icon: 'next' }], run(t) {
      t.eq(t.hold({ clk: 0 }, 1200).filter((e) => e.name === 'tap' && e.kind === 'event').length, 1, 'taps');
    } },
    { name: 'press, slide off: nothing', mount: ['icon_button', { style: 'round', icon: 'next' }], run(t) { t.notEmitted(t.drag({ clk: 0 }, { by: [0, 200] }), 'tap'); } },
    { name: 'disabled: blocked(icon) + error', mount: ['icon_button', { style: 'round', icon: 'next', state: 'disabled' }], run(t) {
      const ev = t.tap({ clk: 0 }); t.notEmitted(ev, 'tap'); t.emitted(ev, 'blocked', 'next'); t.sounds(ev, ['error']);
    } },
    { name: 'play disabled: no toggle', mount: ['icon_button', { style: 'play', icon: 'play', state: 'disabled' }], run(t) {
      const ev = t.tap({ clk: 0 }); t.notEmitted(ev, 'toggle'); t.sounds(ev, ['error']);
    } },
    { name: 'unavailable: blocked(unavailable) + error', mount: ['icon_button', { style: 'round', icon: 'next', state: 'unavailable' }], run(t) {
      const ev = t.tap({ clk: 0 }); t.notEmitted(ev, 'tap'); t.emitted(ev, 'blocked', 'unavailable'); t.sounds(ev, ['error']);
    } },
    { name: 'pressed / active looks still tap', async run(t) {
      for (const state of ['pressed', 'active', 'active-pressed']) { await t.mount('icon_button', { style: 'round', icon: 'next', state }); t.emitted(t.tap({ clk: 0 }), 'tap', 'next', state); }
    } },
    { name: 'sizes: play 96 / 64, round 80 / 52, square 48 / 32', async run(t, { den }) {
      for (const [style, a, b] of [['play', 96, 64], ['round', 80, 52], ['square', 48, 32]]) {
        await t.mount('icon_button', { style, icon: style === 'play' ? 'play' : 'next' }); const p = t.part('icon_button'); t.eq([p.w, p.h], [D(den, a, a), D(den, a, a)].map((v, i) => D(den, a, b)), style);
      }
    } },
    { name: 'update state to disabled and back, silent', mount: ['icon_button', { style: 'round', icon: 'next' }], run(t) {
      t.silent(t.update({ state: 'disabled' })); t.notEmitted(t.tap({ clk: 0 }), 'tap', 'disabled');
      t.silent(t.update({ state: 'default' })); t.emitted(t.tap({ clk: 0 }), 'tap', 'next', 'enabled');
    } },
    // ---------------------------------------------------------------- contract (Header round 3)
    { name: 'hugs: the same square at full, odd and wide parents, centred glyph; never takes the parent width', async run(t, { den }) {
      for (const [w, h] of [[200, 200], [97, 61], [700, 90]]) { const b = await at(t, { style: 'square', icon: 'plus' }, w, h); t.eq([b.w, b.h], [sq(den), sq(den)], `${w}x${h}`); }
    } },
    { name: 'settings icon: the screen-settings icon in the square (= Home\'s settings button), tap = tap(settings); role settings = navigate', async run(t, { den }) {
      const b = await at(t, { style: 'square', icon: 'settings' }, 200); t.eq([b.w, b.h], [sq(den), sq(den)], 'square');
      t.emitted(t.tap({ clk: 0 }), 'tap', 'settings');
      await at(t, { style: 'square', role: 'settings' }, 200); const ev = t.tap({ clk: 0 }); t.emitted(ev, 'navigate', 'settings'); t.sounds(ev, ['open']);
    } },
    { name: 'bounds proof: every style x state at its own size and in a wide parent; below the square it FAILS', async run(t) {
      const bad = [];
      for (const style of ['play', 'round', 'square']) for (const state of STATES) for (const w of [160, 400]) {
        await at(t, { style, icon: style === 'play' ? 'play' : 'next', state }, w); for (const e of boundsAll(t)) bad.push(`${style} ${state} ${w}: ${e}`);
      }
      t.eq(bad.slice(0, 5), [], 'bounds');
      await at(t, { style: 'square', icon: 'plus' }, 2 * D(den(t), 32, 20) + 12); t.ok(boundsAll(t).length > 0, 'a 12 px content box is reported');
    } },
    { name: 'states: each state looks different from default (pixels), active play shows the ring outside the box', async run(t) {
      for (const style of ['round', 'square', 'play']) {
        await at(t, { style, icon: style === 'play' ? 'play' : 'next' }, 160); const f0 = frame565(t);
        for (const state of STATES.slice(1)) { await at(t, { style, icon: style === 'play' ? 'play' : 'next', state }, 160); t.ok(diff565(f0, frame565(t)) > 0, `${style} ${state} differs`); }
      }
    } },
    { name: 'update in place: a state change keeps the same objects and fades (not one frame), silent', async run(t) {
      await at(t, { style: 'square', icon: 'plus' }, 160); const n0 = t.objs().length, f0 = frame565(t);
      t.b.update('icon_button', { style: 'square', icon: 'plus', state: 'active' }); t.b.tick(2); const mid = frame565(t); t.b.settle(1500);
      t.eq(t.objs().length, n0, 'same objects'); t.ok(diff565(f0, frame565(t)) > 0, 'ends active'); t.silent(t.b.events());
      t.ok(diff565(mid, frame565(t)) > 0 || den(t) === '1x', 'still fading after 2 ticks');
    } },
    { name: 'colour + pixel proof: a live dark <-> light switch = a fresh build (square, active, play active + ring, disabled, settings)', async run(t) {
      const out = [];
      for (const p of [{ style: 'square', icon: 'plus' }, { style: 'square', icon: 'plus', state: 'active' }, { style: 'play', icon: 'play', state: 'active' },
        { style: 'round', icon: 'next', state: 'disabled' }, { style: 'square', icon: 'settings', state: 'pressed' }]) out.push(...await liveLook(t, 'icon_button', p, 160, 160));
      t.eq(out.slice(0, 4), [], 'live = fresh');
    } },
    { name: 'tunables: the square, radius and hair repaint it live and come back', async run(t) {
      await at(t, { style: 'square', icon: 'plus' }, 160); const f0 = frame565(t), bad = [];
      for (const [n, v] of [['layout:ibtn-square', 64], ['radius:square', 0], ['stroke:hair', 2]]) {
        tune(t, n, v); t.b.settle(300); if (!diff565(f0, frame565(t))) bad.push(`${n} did not move`); tune(t, n, -1); t.b.settle(300);
        if (diff565(f0, frame565(t))) bad.push(`${n} did not come back`);
      }
      t.eq(bad, [], 'tune');
    } },
    { name: 'golden images: style x state (dark + light), settings + back squares: reviewed, never auto-accepted', async run(t, { den: d }) {
      const out = [];
      for (const style of ['play', 'round', 'square']) for (const state of ['default', 'pressed', 'active', 'disabled']) for (const light of [0, 1]) {
        await at(t, { style, icon: style === 'play' ? 'play' : style === 'round' ? 'next' : 'plus', state }, 160); if (light) { t.b.W = t.b.H = undefined; await t.b.render('icon_button', { style, icon: style === 'play' ? 'play' : style === 'round' ? 'next' : 'plus', state }, 1); stage(t, 160, 160); t.b.settle(800); }
        out.push(...golden(t, 'icon_button', `${style}-${state}-${light ? 'light' : 'dark'}`, d));
      }
      for (const icon of ['settings', 'back']) for (const light of [0, 1]) { t.b.W = t.b.H = undefined; await t.b.render('icon_button', { style: 'square', icon }, light); stage(t, 160, 160); t.b.settle(800); out.push(...golden(t, 'icon_button', `square-${icon}-${light ? 'light' : 'dark'}`, d)); }
      t.eq(out, [], 'goldens');
    } },
    { name: 'leak: 150 build / clear cycles: objects back, heap flat', async run(t) {
      await at(t, { style: 'play', icon: 'play' }, 160);
      t.eq(leak(t, 150, () => t.b.rebuild('icon_button', { style: 'play', icon: 'play', state: 'active' }), () => t.b.rebuild('placeholder', {}), 48 * 1024), [], 'leak');
    } },
  ],
};
