// Check row (components/check_row.cpp): read-only status line: name, a dot, the state word; width prop; update path.
import { D } from './_controls.mjs';
export default {
  id: 'check_row', family: 'controls', about: 'A CI check line: name, dot and pass / fail / pending / skipped.',
  tests: [
    ...[['pass', 'PASS'], ['fail', 'FAIL'], ['pending', 'PENDING'], ['skipped', 'SKIPPED']].map(([state, w]) => ({ name: `${state}: name + ${w} + a dot`, mount: ['check_row', { name: 'build', state }], run(t) {
      t.has(t.texts(), 'build', 'name'); t.has(t.texts(), w, 'word'); t.ok(t.comps().some((c) => c.ids.at(-1) === 'status_dot'), 'dot');
    } })),
    { name: 'read-only: a tap does nothing', mount: ['check_row', { name: 'build', state: 'fail' }], run(t) { const ev = t.tap('build'); t.silent(ev); t.eq(ev.filter((e) => e.kind === 'event').length, 0, 'events'); } },
    { name: 'width prop sets the row width', async run(t, { den }) {
      for (const w of [160, 224, 440]) { await t.mount('check_row', { name: 'build', state: 'pass', width: w }); t.eq(t.part('check_row').w, D(den, w * 1.5, w), `width ${w}`); }
    } },
    { name: 'long name stays inside the row, word visible', mount: ['check_row', { name: 'integration tests on every supported board and toolchain', state: 'pass', width: 160 }], run(t) {
      const b = t.part('check_row'), w = t.find('PASS'); t.ok(w.x + w.w <= b.x + b.w, 'word inside'); t.ok(t.find(/integration/).x + t.find(/integration/).w <= w.x, 'name ends before the word');
    } },
    { name: 'update state: word changes, silent', mount: ['check_row', { name: 'build', state: 'pending' }], run(t) {
      t.silent(t.update({ state: 'pass' })); t.has(t.texts(), 'PASS', 'word'); t.hasNot(t.texts(), 'PENDING', 'old word');
    } },
  ],
};
