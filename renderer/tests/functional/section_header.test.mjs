// Section header (components/section_header.cpp): its action is the Button component (C8b), flush with the right edge.
import { D } from './_controls.mjs';
const A = { title: 'Your pull requests', count: 2, tone: 'info', right: 'action', action: 'See all' };
export default {
  id: 'section_header', family: 'lists', about: 'Caption over a group of rows, with a count and a meta or action.',
  tests: [
    { name: 'size: 48 / 32 tall', mount: ['section_header', A], run(t, { den }) { t.eq(t.part('section_header').h, D(den, 48, 32), 'height'); } },
    { name: 'action = the Button: ends on the right edge, the full height', mount: ['section_header', A], run(t) {
      const h = t.part('section_header'), b = t.part('button');
      t.eq([b.x + b.w, b.h], [h.x + h.w, h.h], 'button right edge + height');
    } },
    { name: 'action tap: tap(see all)', mount: ['section_header', A], run(t) { t.emitted(t.tap('SEE ALL'), 'tap', /see all/i); } },
    { name: 'action disabled: blocked + error', mount: ['section_header', { ...A, state: 'disabled' }], run(t) {
      const ev = t.tap('SEE ALL'); t.notEmitted(ev, 'tap'); t.emitted(ev, 'blocked'); t.sounds(ev, ['error']);
    } },
  ],
};
