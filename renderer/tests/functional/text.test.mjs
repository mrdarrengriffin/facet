// Text (components/text.cpp): words that wrap to the line count they declare (1 to 4), never more; what does not fit
// ends in "..." on the last line. The fit proof: the text's box is exactly that many line boxes of its type.
import fs from 'node:fs';
import { look, golden, stage } from './_release.mjs';

const TY = JSON.parse(fs.readFileSync(new URL('../../tokens/type.json', import.meta.url))).styles;
const den = (t) => (t.b.S > 600 ? '1.5x' : '1x');
const box = (t, style) => (den(t) === '1x' ? TY[style]['1x'].box : TY[style].box);
const LONG = 'Every word here will not fit in the lines it declares, so the last line ends in an ellipsis instead of growing the box past what it declares, however long it is.';
async function at(t, props, light = 0) {
  t.b.W = t.b.H = undefined;
  await t.mount('text', props);
  if (light) look(t, 1);
  stage(t, Math.round(t.b.S * 0.6), Math.round(t.b.S * 0.4)); t.b.settle(200);
  return t.objs().find((o) => o.text !== undefined && t.visible(o));
}

export default {
  id: 'text', family: 'components', about: 'Wrapping text: exactly its declared lines tall, the last line ends in "..." when it does not fit.',
  tests: [
    { name: 'the box is its line count of line boxes, every style, 1 to 4 lines', async run(t) {
      for (const style of ['body', 'headline', 'label']) for (const lines of [1, 2, 3, 4]) {
        const l = await at(t, { style, lines, text: 'Short' });
        t.eq(l?.h, lines * box(t, style), `${style} x ${lines}: ${l?.h} = ${lines} x ${box(t, style)}`);
      }
    } },
    { name: 'a long text stays in its lines and ends in "..."; a short one is whole', async run(t) {
      for (const lines of [1, 2, 3]) {
        const l = await at(t, { lines, text: LONG });
        t.ok(l && l.h === lines * box(t, 'body'), `${lines} lines: box ${l?.h}`);
        t.ok(!l.cut, `${lines} lines: no glyph below its box (cut ${l?.cut})`);
      }
      const s = await at(t, { lines: 2, text: 'Two words' });
      t.ok(s && s.text === 'Two words', 'a short text is whole');
    } },
    { name: 'update: new words in place, silently; a new tone in place', async run(t) {
      await at(t, { text: 'One' });
      t.silent(t.update({ text: 'Two' }));
      t.ok(t.objs().find((o) => o.text === 'Two'), 'the words changed');
      t.update({ text: 'Two', tone: 'dim' });
      t.ok(t.objs().find((o) => o.text === 'Two'), 'still there after a new tone');
    } },
    { name: 'golden images: every variant (dark + light): reviewed, never auto-accepted', async run(t, { den: d }) {
      const out = [];
      const cases = [['body-2', {}], ['headline-1', { style: 'headline', text: 'Add me in Home Assistant', lines: 1 }],
        ['dim-3', { tone: 'dim', lines: 3, text: 'Open Home Assistant on your phone. It finds this panel by itself and asks to add it; tap Add, then pick the area it is in.' }],
        ['label-1', { style: 'label', tone: 'dim', lines: 1, text: 'facet-panel-a1b2 · 192.168.4.1' }], ['long-2', { text: LONG }]];
      for (const [n, p] of cases) for (const light of [0, 1]) { await at(t, p, light); out.push(...golden(t, 'text', `${n}-${light ? 'light' : 'dark'}`, d)); }
      t.eq(out, [], 'goldens');
    } },
  ],
};
