// Home variety (#200): the slim and wide Home sizes (2xhalf, 3xhalf, 3x1: CONTRACT.md C8c, core/spec.h GRID_EXTRA) only
// for a card type whose component lists them, and the two new card types: Stat (components/stat.cpp: a big value and
// its name) and Header (components/section_header.cpp: a full-width title row, no entity). Home Assistant is faked.
const T = 'sensor.test_energy_today';
const DECK = { v: 2, rev: 9, start: 'h', pages: [{ id: 'h', kind: 'home', name: 'Test', cards: [] }] };
const deck = (cards, rev = 10) => ({ ...DECK, rev, pages: [{ ...DECK.pages[0], cards }] });
const send = (t, d) => JSON.parse(t.b.spec('deck', d) || '{}');
const refused = (r, code) => !r.ok && JSON.stringify(r.errors ?? '').includes(code);

export default {
  id: 'home_variety', family: 'screens', about: 'Home\'s slim and wide sizes and the Stat and Header cards.',
  tests: [
    { name: 'a Header row and Stats render on Home: the title, the value and its name', async run(t) {
      t.ha.s[T] = { '': '12.4', friendly_name: 'Energy today', unit_of_measurement: 'kWh' };
      await t.device(deck([
        { t: 'header', label: 'Energy', size: '3xhalf' },
        { t: 'stat', e: T, size: 'half' }, { t: 'stat', e: T, size: '1x1', label: 'Today' }, { t: 'stat', e: T, size: '2xhalf' }]));
      t.b.settle(800); t.ha.push(); t.b.settle(600);
      const tx = t.texts();
      t.has(tx, /^energy$/i, 'the header title'); t.has(tx, /12\.4/, 'the stat value'); t.has(tx, /^today$/i, 'a stat label');
    } },
    { name: 'the slim and wide sizes only where the component lists them; a Header has a label and no entity', async run(t) {
      await t.device(DECK);
      let rev = 20;
      for (const [c, why] of [
        [{ t: 'light', e: 'light.test_lamp', size: '3x1' }, 'a light at 3x1'],
        [{ t: 'entity', e: 'light.test_lamp', size: '2xhalf' }, 'an entity tile at 2xhalf'],
        [{ t: 'stat', e: T, size: '3xhalf' }, 'a stat as a full-width row'],
        [{ t: 'header', label: 'Lights', size: '1x1' }, 'a header at 1x1']]) t.ok(refused(send(t, deck([c], rev++)), 'bad_size'), `${why}: refused`);
      t.ok(refused(send(t, deck([{ t: 'header', e: T, label: 'Lights' }], rev++)), 'bad_card'), 'a header with an entity: refused');
      t.ok(refused(send(t, deck([{ t: 'header', size: '3xhalf' }], rev++)), 'bad_card'), 'a header without a label: refused');
      const ok = send(t, deck([{ t: 'header', label: 'Lights' }, { t: 'stat', e: T, size: '2xhalf' }, { t: 'stat', e: T, size: 'half' }], rev++));
      t.ok(ok.ok, `taken: ${JSON.stringify(ok.errors ?? '')}`);
    } },
    { name: 'the editor\'s schema: Stat and Header offer only their own sizes', async run(t) {
      await t.device(DECK);
      const s = JSON.parse(t.b.m.UTF8ToString(t.b.m._fw_catalogue()));
      t.eq(s.cards?.header?.grid, '3xhalf', 'Header: the full-width row');
      t.eq(s.cards?.stat?.grid, 'half,1x1,2xhalf', 'Stat: half, 1x1, 2xhalf');
      t.eq(s.grid_sizes, '1x1,2x1,2x2,half', 'every type keeps the usual four');
    } },
  ],
};
