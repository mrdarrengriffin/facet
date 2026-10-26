// Checkbox (components/checkbox.cpp): open / doing / done; interactive tap cycles open -> done -> open, doing -> done;
// read-only takes no touch; disabled blocks; hit area touch-min tall; update path silent.
import { D, px, diff } from './_controls.mjs';
const box = (t) => { const k = t.objs().filter((o) => o.w === o.h && (o.w === 24 || o.w === 16)); return k[0]; };  // the box (24 / 16)
const filled = (t) => { const b = box(t), bg = px(t, b.x - 12, b.y - 12); return diff(px(t, b.x + b.w / 2, b.y + 3), bg); };  // top middle, just inside the edge
const dot = (t) => { const b = box(t), bg = px(t, b.x - 12, b.y - 12); return diff(px(t, b.x + b.w / 2, b.y + b.h / 2), bg); };
const L = 'Water the plants';
export default {
  id: 'checkbox', family: 'controls', about: 'Open / doing / done box; a tap moves it between open and done.',
  tests: [
    { name: 'open: empty box, label shown', mount: ['checkbox', { check: 'open', label: L }], run(t) {
      t.ok(!filled(t), 'not filled'); t.ok(!dot(t), 'no dot'); t.has(t.texts(), L, 'label');
    } },
    { name: 'doing: dot, no fill', mount: ['checkbox', { check: 'doing', label: L }], run(t) { t.ok(dot(t), 'dot'); t.ok(!filled(t), 'not filled'); } },
    { name: 'done: filled', mount: ['checkbox', { check: 'done', label: L }], run(t) { t.ok(filled(t), 'filled'); } },
    { name: 'tap open -> done: toggle(done) + toggle-on', mount: ['checkbox', { check: 'open', label: L }], run(t) {
      const ev = t.tap({ clk: 0 }); t.emitted(ev, 'toggle', 'done'); t.sounds(ev, ['toggle-on']); t.ok(filled(t), 'filled after');
    } },
    { name: 'tap done -> open: toggle(open) + toggle-off', mount: ['checkbox', { check: 'done', label: L }], run(t) {
      const ev = t.tap({ clk: 0 }); t.emitted(ev, 'toggle', 'open'); t.sounds(ev, ['toggle-off']); t.ok(!filled(t), 'empty after');
    } },
    { name: 'tap doing -> done', mount: ['checkbox', { check: 'doing', label: L }], run(t) {
      const ev = t.tap({ clk: 0 }); t.emitted(ev, 'toggle', 'done'); t.sounds(ev, ['toggle-on']); t.ok(filled(t), 'filled');
    } },
    { name: 'tap the label also toggles (whole row is the target)', mount: ['checkbox', { check: 'open', label: L }], run(t) {
      t.emitted(t.tap(L), 'toggle', 'done');
    } },
    { name: 'hit area: a tap just above the box (inside touch-min) toggles', mount: ['checkbox', { check: 'open', label: L }], run(t, { den }) {
      const b = box(t), y = b.y + b.h / 2 - D(den, 48, 32) / 2 + 3; t.emitted(t.tap({ at: [b.x + b.w / 2, y] }), 'toggle', 'done');
    } },
    { name: 'read-only (interactive off): tap does nothing at all', mount: ['checkbox', { check: 'open', label: L, interactive: false }], run(t) {
      const ev = t.tap(L); t.notEmitted(ev, 'toggle'); t.notEmitted(ev, 'blocked'); t.silent(ev); t.ok(!filled(t), 'still open');
    } },
    { name: 'disabled: blocked + error, stays done', mount: ['checkbox', { check: 'done', label: L, state: 'disabled' }], run(t) {
      const ev = t.tap(L); t.notEmitted(ev, 'toggle'); t.emitted(ev, 'blocked', 'done'); t.sounds(ev, ['error']);
    } },
    { name: 'update check: open -> done -> doing, silent', mount: ['checkbox', { check: 'open', label: L }], run(t) {
      let ev = t.update({ check: 'done', label: L }); t.silent(ev); t.notEmitted(ev, 'toggle'); t.ok(filled(t), 'filled');
      ev = t.update({ check: 'doing', label: L }); t.silent(ev); t.ok(dot(t) && !filled(t), 'doing');
    } },
    { name: 'update label: new words in place', mount: ['checkbox', { check: 'open', label: L }], run(t) {
      t.update({ check: 'open', label: 'Feed the cat' }); t.has(t.texts(), 'Feed the cat', 'new label'); t.hasNot(t.texts(), L, 'old label');
    } },
  ],
};
