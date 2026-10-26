// Icon button (components/icon_button.cpp): round / square = tap(icon) + tap; play toggles play / pause with
// toggle-on / toggle-off; disabled + unavailable block; sizes per style; update path silent.
import { D } from './_controls.mjs';
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
  ],
};
