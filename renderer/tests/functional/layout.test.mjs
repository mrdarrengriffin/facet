// Layout & devices foundation (foundations/layout.cpp), the "screen" section: a real Home at true size under the token
// layers. 2026-10-27: the Cards there went straight into a content-height wrap row; a Card fills its parent (fx "fill"),
// so LVGL's layout pass never settled (ASan sat at 100% CPU for hours). Each Card now gets a card-w x card-h cell.
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const SPACE = JSON.parse(readFileSync(join(dirname(fileURLToPath(import.meta.url)), '..', '..', 'tokens', 'space.json'), 'utf8')).tokens;
const tk = (t, n) => (t.b.S > 600 ? SPACE.layout[n].v : SPACE.layout[n]['1x']);
export default {
  id: 'layout', family: 'foundations', about: 'Layout page: the real Home under the layers settles, its Cards at card-w x card-h.',
  tests: [
    { name: 'screen section: renders in bounded time, four Cards at card-w x card-h', async run(t) {
      const t0 = Date.now();
      await t.mount('layout', { section: 'screen' });
      t.b.settle(400);
      const ms = Date.now() - t0;
      t.ok(ms < 10000, `render + settle took ${ms} ms (a layout that never settles hangs here)`);
      const cards = t.comps().filter((c) => c.ids.includes('card'));
      t.eq(cards.length, 4, 'Cards on the sample Home');
      for (const c of cards) t.ok(c.w === tk(t, 'card-w') && c.h === tk(t, 'card-h'), `Card ${c.w} x ${c.h}, want ${tk(t, 'card-w')} x ${tk(t, 'card-h')}`);
    } },
  ],
};
