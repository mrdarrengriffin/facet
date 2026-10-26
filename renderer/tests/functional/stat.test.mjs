import fs from 'node:fs';
const C = JSON.parse(fs.readFileSync(new URL('../../tokens/colour.json', import.meta.url))).colours;
const is = (hex, tok) => !!hex && [C[tok].dark, C[tok].light].some((v) => v.toUpperCase() === String(hex).toUpperCase());
const inBox = (o, b) => o.x >= b.x && o.y >= b.y && o.x + o.w <= b.x + b.w && o.y + o.h <= b.y + b.h;
// Stat (components/stat.cpp): caps label over the value; the value is the heavier line (type title, not body).
export default {
  id: 'stat', family: 'cards', about: 'Caps label over one value line; the value reads heavier than the label.',
  tests: [
    { name: 'value line is taller than the label and in title type', mount: ['stat', { label: 'Lit', value: '63%' }], run(t) {
      const os = t.objs().filter((o) => o.text && t.visible(o)), l = os.find((o) => /^lit$/i.test(o.text)), v = os.find((o) => o.text === '63%');
      t.ok(l && v, 'both lines'); t.ok(v && l && v.h > l.h, `value ${v?.h} > label ${l?.h}`);
      t.ok(v && v.y > l.y, 'value under the label');
    } },
    { name: 'tone warn = warn ink; default dim', async run(t) {
      await t.mount('stat', { label: 'Lit', value: '63%', tone: 'warn' }); t.ok(is(t.objs().find((o) => o.text === '63%')?.ink, 'warn'), 'warn');
      await t.mount('stat', { label: 'Lit', value: '63%' }); t.ok(is(t.objs().find((o) => o.text === '63%')?.ink, 'text-dim'), 'dim');
    } },
  ],
};
