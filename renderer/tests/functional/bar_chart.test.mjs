import fs from 'node:fs';
const C = JSON.parse(fs.readFileSync(new URL('../../tokens/colour.json', import.meta.url))).colours;
const is = (hex, tok) => !!hex && [C[tok].dark, C[tok].light].some((v) => v.toUpperCase() === String(hex).toUpperCase());
const inBox = (o, b) => o.x >= b.x && o.y >= b.y && o.x + o.w <= b.x + b.w && o.y + o.h <= b.y + b.h;
// Bar chart (components/bar_chart.cpp): hour labels on the axis (ticks prop), inside the chart width.
export default {
  id: 'bar_chart', family: 'charts', about: 'Bars on a baseline with a now marker; optional hour labels on the axis.',
  tests: [
    { name: 'ticks 6: 00 06 12 18 under the bars, left to right, inside the chart', mount: ['bar_chart', { ticks: 6 }], run(t) {
      const b = t.part('bar_chart'), ls = t.objs().filter((o) => o.text && t.visible(o) && /^\d\d$/.test(o.text));
      t.eq(ls.map((o) => o.text), ['00', '06', '12', '18'], 'labels');
      t.ok(ls.every((o) => inBox(o, b)), 'inside the chart box');
      t.ok(ls.every((o, i) => !i || o.x > ls[i - 1].x), 'left to right');
    } },
    { name: 'ticks 0: no axis', mount: ['bar_chart', { ticks: 0 }], run(t) {
      t.eq(t.objs().filter((o) => o.text && t.visible(o) && /^\d\d$/.test(o.text)).length, 0, 'no hour labels');
    } },
  ],
};
