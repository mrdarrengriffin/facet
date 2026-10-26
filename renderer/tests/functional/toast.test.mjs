// Toast (components/toast.cpp): tap / swipe dismiss + back; a short drag springs back; the action button dismisses;
// the timer runs down (6 s, urgent 12 s) to dismiss(timeout) silently; timer off stays; update path.
import { D } from './_controls.mjs';
const P = { icon: 'timer', tone: 'ok', title: 'Pasta is ready', body: 'Your 9 min timer has finished.', action: '', timer: false, life: 'normal' };
const shown = (t) => t.texts().includes('Pasta is ready');
const box = (t) => t.part('toast');
export default {
  id: 'toast', family: 'controls', about: 'One notification: icon, title, body, optional action, timer bar.',
  tests: [
    { name: 'shows title and body; box content width x 120 / 80', mount: ['toast', P], run(t, { den }) {
      t.has(t.texts(), 'Pasta is ready', 'title'); t.has(t.texts(), 'Your 9 min timer has finished.', 'body'); const b = box(t); t.eq([b.w, b.h], D(den, [656, 120], [440, 80]), 'box');
    } },
    { name: 'tap: dismiss(tap) + back, it leaves', mount: ['toast', P], run(t) { const ev = t.tap('Pasta is ready'); t.emitted(ev, 'dismiss', 'tap'); t.sounds(ev, ['back']); t.ok(!shown(t), 'gone'); } },
    { name: 'swipe right past the threshold: dismiss(swipe) + back', mount: ['toast', P], run(t, { den }) {
      const ev = t.drag('Pasta is ready', { by: [D(den, 200, 140), 0] }); t.emitted(ev, 'dismiss', /swipe/); t.sounds(ev, ['back']); t.ok(!shown(t), 'gone');
    } },
    { name: 'swipe left also dismisses', mount: ['toast', P], run(t, { den }) { t.emitted(t.drag('Pasta is ready', { by: [-D(den, 200, 140), 0] }), 'dismiss', /swipe/); } },
    { name: 'a short sideways drag springs back: no dismiss, back in place', mount: ['toast', P], run(t, { den }) {
      const x = box(t).x, ev = t.drag('Pasta is ready', { by: [D(den, 40, 28), 0] }); t.notEmitted(ev, 'dismiss'); t.silent(ev); t.ok(shown(t), 'still there'); t.eq(box(t).x, x, 'back in place');
    } },
    { name: 'action button: dismiss(action), the button plays its own tap', mount: ['toast', { ...P, action: 'Open' }], run(t) {
      t.has(t.texts(), 'OPEN', 'button'); const ev = t.tap('OPEN'); t.emitted(ev, 'dismiss', 'action'); t.ok(!shown(t), 'gone');
      t.hasNot(ev.filter((e) => e.kind === 'sound').map((e) => e.name), 'back', 'no back sound for the action');
    } },
    { name: 'timer on: dismiss(timeout) after ~6 s, silently', mount: ['toast', { ...P, timer: true }], run(t) {
      let ev = t.wait(2000); t.notEmitted(ev, 'dismiss', 'too early (the mount settles ~3 s of its life)'); ev = t.wait(2500); t.emitted(ev, 'dismiss', 'timeout'); t.silent(ev); t.ok(!shown(t), 'gone');
    } },
    { name: 'urgent: still there at 8 s, gone by 13 s', mount: ['toast', { ...P, timer: true, life: 'urgent' }], run(t) {
      t.notEmitted(t.wait(8000), 'dismiss', 'at 8 s'); t.emitted(t.wait(5000), 'dismiss', 'timeout');
    } },
    { name: 'timer off: stays (13 s)', mount: ['toast', P], run(t) { t.notEmitted(t.wait(13000), 'dismiss'); t.ok(shown(t), 'still there'); } },
    { name: 'long title stays inside the box', mount: ['toast', { ...P, title: 'Lightning 9 km W, closing at 48 km/h from the coast and heading inland' }], run(t) {
      const b = box(t), l = t.find(/Lightning/); t.ok(l.x + l.w <= b.x + b.w, 'title inside');
    } },
    { name: 'update title / body: new words, silent', mount: ['toast', P], run(t) {
      t.silent(t.update({ title: 'Rice is ready', body: 'Done.' })); t.has(t.texts(), 'Rice is ready', 'title'); t.has(t.texts(), 'Done.', 'body');
    } },
  ],
};
