// States foundation (foundations/states.cpp): the pages of real components in each state. 2026-10-27: the Card
// samples went straight into a content-sized column; a Card fills its parent (fx "fill"), so LVGL's layout pass never
// settled (ASan hung, then overflowed a size). Each Card sample now gets a card-w x card-h cell.
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const SPACE = JSON.parse(readFileSync(join(dirname(fileURLToPath(import.meta.url)), '..', '..', 'tokens', 'space.json'), 'utf8')).tokens;
const tk = (t, n) => (t.b.S > 600 ? SPACE.layout[n].v : SPACE.layout[n]['1x']);
const cards = async (t, section, n) => {
  const t0 = Date.now();
  await t.mount('states', { section });
  t.b.settle(400);
  const ms = Date.now() - t0;
  t.ok(ms < 10000, `render + settle took ${ms} ms (a layout that never settles hangs here)`);
  const cs = t.comps().filter((c) => c.ids.includes('card'));
  t.eq(cs.length, n, `Card samples on the ${section} page`);
  for (const c of cs) t.ok(c.w === tk(t, 'card-w') && c.h === tk(t, 'card-h'), `Card ${c.w} x ${c.h}, want ${tk(t, 'card-w')} x ${tk(t, 'card-h')}`);
};
export default {
  id: 'states', family: 'foundations', about: 'States pages: the Card samples settle at card-w x card-h.',
  tests: [
    { name: 'pressed section: settles, both Card samples at card-w x card-h', run: (t) => cards(t, 'pressed', 2) },
    { name: 'unavailable section: settles, the Card sample at card-w x card-h', run: (t) => cards(t, 'unavailable', 1) },
  ],
};
