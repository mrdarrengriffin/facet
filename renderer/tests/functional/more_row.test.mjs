// More row (components/more_row.cpp): tap = tap(label) + open; disabled blocks; size; update path.
import { D } from './_controls.mjs';
const P = { text: 'all pull requests · 3 more' }, L = 'ALL PULL REQUESTS · 3 MORE';
export default {
  id: 'more_row', family: 'controls', about: 'Row that ends a short list and opens the rest.',
  tests: [
    { name: 'shows its words in caps', mount: ['more_row', P], run(t) { t.has(t.texts(), L, 'label'); } },
    { name: 'tap: tap(label) + open', mount: ['more_row', P], run(t) { const ev = t.tap(L); t.emitted(ev, 'tap', /3 more/i); t.sounds(ev, ['open']); } },
    { name: 'tap anywhere on the row (right end) works', mount: ['more_row', P], run(t) { const b = t.part('more_row'); t.emitted(t.tap({ at: [b.x + b.w - 6, b.y + b.h / 2] }), 'tap'); } },
    { name: 'slide off: nothing', mount: ['more_row', P], run(t) { t.notEmitted(t.drag(L, { by: [0, 200] }), 'tap'); } },
    { name: 'disabled: blocked + error', mount: ['more_row', { ...P, state: 'disabled' }], run(t) {
      const ev = t.tap(L); t.notEmitted(ev, 'tap'); t.emitted(ev, 'blocked'); t.sounds(ev, ['error']);
    } },
    { name: 'size: content width x 64 / 44', mount: ['more_row', P], run(t, { den }) { const b = t.part('more_row'); t.eq([b.w, b.h], D(den, [656, 64], [440, 44]), 'box'); } },
    { name: 'update text: new words, silent; tap detail follows', mount: ['more_row', P], run(t) {
      t.silent(t.update({ text: 'all 12 rooms' })); t.has(t.texts(), 'ALL 12 ROOMS', 'label'); t.emitted(t.tap('ALL 12 ROOMS'), 'tap', /12 rooms/i);
    } },
  ],
};
