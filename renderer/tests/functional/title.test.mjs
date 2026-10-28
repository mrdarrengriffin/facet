// Title (components/title.cpp, contract v2): the screen's name, the part in the Header's title slot. Header behaviour
// (implicit from header.label, explicit node, removable) is in header.test.mjs.
export default {
  id: 'title', family: 'layout', about: 'Title: caps label text, grid width alone, update in place.',
  tests: [
    { name: 'shows its text in caps; alone its width is on the 4 px grid', mount: ['title', { text: 'Living room' }], run(t) {
      const l = t.objs().find((o) => o.text && /living room/i.test(o.text));
      t.ok(!!l, 'text shown'); t.eq(l.w % 4, 0, `width ${l.w} on the grid`); t.ok(l.w > 0, 'not empty');
    } },
    { name: 'update: new words in place (same objects), silent', mount: ['title', { text: 'Weather' }], run(t) {
      const n0 = t.objs().length; t.silent(t.update({ text: 'Lights' }));
      t.eq([!!t.objs().find((o) => o.text && /lights/i.test(o.text)), t.objs().length], [true, n0], 'in place');
    } },
  ],
};
