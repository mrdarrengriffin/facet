// Toast layer (components/toast_layer.cpp): toasts over a screen. A push = event toast + notify sound, newest at the
// edge, at most 2 (oldest pushed out silently); taps reach the screen around them; a toast tap dismisses it; the
// timer empties the layer; top / bottom position.
import { D, comps } from './_controls.mjs';
const P = { position: 'top', push: 'none', stack: 1, timer: false, action: false, screen: true };
const toasts = (t) => comps(t, 'toast').filter((c) => c.y < t.b.S && c.y + c.h > 0);
const push = (t, row) => t.tap(row);
export default {
  id: 'toast_layer', family: 'controls', about: 'Stack of up to two toasts over a screen.',
  tests: [
    { name: 'empty at rest (push none)', mount: ['toast_layer', P], run(t) { t.eq(toasts(t).length, 0, 'toasts'); } },
    { name: 'a push: event toast + notify sound, one toast under the header', mount: ['toast_layer', P], run(t, { den }) {
      const ev = push(t, 'Agent started'); t.emitted(ev, 'toast'); t.has(ev.filter((e) => e.kind === 'sound').map((e) => e.name), /notify/, 'notify sound');
      const s = toasts(t); t.eq(s.length, 1, 'toasts'); if (s[0]) t.eq(s[0].y, D(den, 96, 64), 'top slot');
    } },
    { name: 'two pushes: both shown, newest at the edge, the older one moved along', mount: ['toast_layer', P], run(t, { den }) {
      push(t, 'Agent started'); push(t, 'Washing done'); const s = toasts(t).sort((a, b) => a.y - b.y); t.eq(s.length, 2, 'toasts');
      if (s.length === 2) t.ok(s[1].y >= s[0].y + s[0].h, 'stacked, not overlapping');
    } },
    { name: 'three pushes: still two shown (oldest pushed out)', mount: ['toast_layer', P], run(t) {
      push(t, 'Agent started'); push(t, 'Washing done'); const ev = push(t, 'Vacuum docked'); t.eq(toasts(t).length, 2, 'toasts');
      t.eq(ev.filter((e) => e.kind === 'sound' && e.name === 'back').length, 0, 'the push-out is silent');
    } },
    { name: 'the layer is not a target: a tap beside the toasts reaches the screen', mount: ['toast_layer', P], run(t) {
      push(t, 'Agent started'); t.emitted(push(t, 'Vacuum docked'), 'toast', undefined, 'the row under the layer still works');
    } },
    { name: 'tap a toast: dismiss(tap) + back, it leaves', mount: ['toast_layer', P], run(t) {
      push(t, 'Agent started'); const s = toasts(t)[0]; const ev = t.tap({ at: [s.x + s.w / 2, s.y + s.h / 2] });
      t.emitted(ev, 'dismiss', 'tap'); t.sounds(ev, ['back']); t.eq(toasts(t).length, 0, 'toasts');
    } },
    { name: 'dismiss the newest of two: the other one restacks into the top slot', mount: ['toast_layer', P], run(t, { den }) {
      push(t, 'Agent started'); push(t, 'Washing done'); const s = toasts(t).sort((a, b) => a.y - b.y); t.tap({ at: [s[0].x + s[0].w / 2, s[0].y + s[0].h / 2] });
      const r = toasts(t); t.eq(r.length, 1, 'toasts'); if (r[0]) t.eq(r[0].y, D(den, 96, 64), 'restacked to the top slot');
    } },
    { name: 'bottom position: toasts sit at the bottom', mount: ['toast_layer', { ...P, position: 'bottom' }], run(t, { S }) {
      push(t, 'Agent started'); const s = toasts(t)[0]; t.ok(s && s.y > S / 2, `toast at ${s && s.y}`);
    } },
    { name: 'timer on: the layer empties itself after the toast life', mount: ['toast_layer', { ...P, timer: true }], run(t) {
      push(t, 'Agent started'); t.eq(toasts(t).length, 1, 'shown'); t.wait(7500); t.eq(toasts(t).length, 0, 'gone after 7.5 s');
    } },
    { name: 'push on build: stack 2 arrives as two toasts', mount: ['toast_layer', { ...P, push: 'ok', stack: 2 }], run(t) { t.wait(1000); t.eq(toasts(t).length, 2, 'toasts'); } },
  ],
};
