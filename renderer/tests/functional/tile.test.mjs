import fs from 'node:fs';
const C = JSON.parse(fs.readFileSync(new URL('../../tokens/colour.json', import.meta.url))).colours;
const is = (hex, tok) => !!hex && [C[tok].dark, C[tok].light].some((v) => v.toUpperCase() === String(hex).toUpperCase());
const inBox = (o, b) => o.x >= b.x && o.y >= b.y && o.x + o.w <= b.x + b.w && o.y + o.h <= b.y + b.h;
// Agent tile (components/tile.cpp): the status word's ink per kind: working is calm (text), only needs-you is coloured.
const word = (t, re) => t.objs().find((o) => o.text && re.test(o.text) && t.visible(o));
export default {
  id: 'tile', family: 'cards', about: 'Agent tile: mascot, title, status line; working reads calm, needs-you in warn.',
  tests: [
    { name: 'working: calm ink (text), not accent / warn', mount: ['tile', { title: 'Docs writer', kind: 'working', status: 'working · 42% ctx' }], run(t) {
      const w = word(t, /^working$/i); t.ok(w, 'state word');
      t.ok(w && is(w.ink, 'text'), `ink ${w?.ink}`); t.ok(w && !is(w.ink, 'accent-text') && !is(w.ink, 'warn'), 'not orange / warn');
    } },
    { name: 'needs you: warn ink', mount: ['tile', { title: 'Recipe importer', kind: 'needs', status: 'needs · 12m' }], run(t) {
      const w = word(t, /^needs/i); t.ok(w && is(w.ink, 'warn'), `ink ${w?.ink}`);
    } },
  ],
};
