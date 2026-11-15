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
    { name: 'a Slider strip: the light\'s name and brightness; a drag sets it (light.brightness)', async run(t) {
      t.ha.s['light.test_lamp'] = { '': 'on', friendly_name: 'Desk lamp', brightness: '128' };
      await t.device(deck([{ t: 'slider', e: 'light.test_lamp', size: '2xhalf' }]));
      t.b.settle(800); t.ha.push(); t.b.settle(600);
      t.has(t.texts(), /^desk lamp$/i, 'its name'); t.has(t.texts(), /^50$/, 'its brightness');
      const r = t.part('slider_row');
      t.ok(r, 'a slider row');
      if (r) { t.ha.calls = []; t.drag(null, { from: [r.x + r.w * 0.55, r.y + r.h / 2], by: [r.w * 0.3, 0] }); t.b.settle(800); }
      t.ok(t.ha.calls.some((c) => /^light\./.test(c.service) && c.entity === 'light.test_lamp'), `a drag sets the light: ${JSON.stringify(t.ha.calls).slice(0, 200)}`);
    } },
    { name: 'a Chip row: one pill per entity, filled while on; a tap toggles it, a scene\'s runs it', async run(t) {
      Object.assign(t.ha.s, { 'light.test_a': { '': 'on', friendly_name: 'Lamp A' }, 'light.test_b': { '': 'off', friendly_name: 'Lamp B' },
        'scene.test_film': { '': 'scening', friendly_name: 'Film night' } });
      await t.device(deck([{ t: 'chips', es: ['light.test_a', 'light.test_b', 'scene.test_film'] }]));
      t.b.settle(800); t.ha.push(); t.b.settle(600);
      for (const n of [/^lamp a$/i, /^lamp b$/i, /^film night$/i]) t.has(t.texts(), n, `a pill: ${n}`);
      t.ha.calls = []; t.tap(/^lamp b$/i); t.b.settle(600);
      t.ok(t.ha.calls.some((c) => /^light\./.test(c.service) && c.entity === 'light.test_b'), `a tap toggles Lamp B: ${JSON.stringify(t.ha.calls).slice(0, 160)}`);
      t.ha.calls = []; t.tap(/^film night$/i); t.b.settle(600);
      t.ok(t.ha.calls.some((c) => c.service === 'scene.turn_on' && c.entity === 'scene.test_film'), `a tap runs the scene: ${JSON.stringify(t.ha.calls).slice(0, 160)}`);
    } },
    { name: 'a Stat row: up to three values and their names side by side', async run(t) {
      Object.assign(t.ha.s, { 'sensor.test_a': { '': '21.4', friendly_name: 'Inside' }, 'sensor.test_b': { '': '48', friendly_name: 'Humidity' },
        'sensor.test_c': { '': '3.1', friendly_name: 'Solar' } });
      await t.device(deck([{ t: 'stats', es: ['sensor.test_a', 'sensor.test_b', 'sensor.test_c'] }]));
      t.b.settle(800); t.ha.push(); t.b.settle(600);
      for (const n of [/^inside$/i, /^humidity$/i, /^solar$/i, /21\.4/, /^48/]) t.has(t.texts(), n, `shows ${n}`);
    } },
    { name: 'Chip row and Stat row: es, not e; their own sizes; their domains; at most 6 / 3', async run(t) {
      await t.device(DECK);
      let rev = 40;
      const bad = (c, code, why) => t.ok(refused(send(t, deck([c], rev++)), code), `${why}: refused (${code})`);
      bad({ t: 'chips', e: 'light.test_a' }, 'bad_card', 'chips with e');
      bad({ t: 'chips', es: ['light.test_a'], size: '1x1' }, 'bad_size', 'chips at 1x1');
      bad({ t: 'stats', es: ['sensor.a', 'sensor.b', 'sensor.c', 'sensor.d'] }, 'bad_card', 'four stats');
      bad({ t: 'chips', es: ['sensor.test_a'] }, 'bad_entity', 'a sensor chip');
      bad({ t: 'light', e: 'light.test_a', es: ['light.test_b'], size: '1x1' }, 'bad_card', 'es on a light card');
      const ok = send(t, deck([{ t: 'chips', es: ['light.test_a', 'scene.test_film'] }, { t: 'stats', es: ['sensor.test_a'], size: '3xhalf' }], rev++));
      t.ok(ok.ok, `taken: ${JSON.stringify(ok.errors ?? '')}`);
    } },
    { name: 'a Mode switch: the select\'s options, the chosen one = its state; a tap picks one (entity.option)', async run(t) {
      t.ha.s['input_select.test_mode'] = { '': 'Home', friendly_name: 'House mode', options: '["Home","Away","Night"]' };
      await t.device(deck([{ t: 'mode', e: 'input_select.test_mode' }]));
      t.b.settle(800); t.ha.push(); t.b.settle(600);
      for (const n of [/^home$/i, /^away$/i, /^night$/i]) t.has(t.texts(), n, `a choice: ${n}`);
      t.ha.calls = []; t.tap(/^night$/i); t.b.settle(600);
      t.ok(t.ha.calls.some((c) => c.service === 'input_select.select_option' && c.entity === 'input_select.test_mode' && (c.data?.option ?? '') === 'Night'),
        `a tap picks Night: ${JSON.stringify(t.ha.calls).slice(0, 200)}`);
    } },
    { name: 'a Sparkline: the value and its last 24 hours from Home Assistant\'s statistics (panel.history)', async run(t) {
      t.ha.s['sensor.test_temp'] = { '': '21.4', friendly_name: 'Inside', unit_of_measurement: '°C', state_class: 'measurement' };
      const means = [19.8, 20.1, 20.6, 21.0, 21.4, 21.2, 20.9, 21.4];
      let asked = null;
      t.ha.wsx = (cmd) => cmd.type !== 'recorder/statistics_during_period' ? null
        : (asked = cmd, { id: 1, type: 'result', success: true, result: { 'sensor.test_temp': means.map((m, i) => ({ start: i * 3600000, end: (i + 1) * 3600000, mean: m })) } });
      t.ha.answers = true;
      try {
        await t.device(deck([{ t: 'spark', e: 'sensor.test_temp' }]));
        t.b.spec('act', { do: 'ha.connect', url: 'http://ha.test:8123', token: 'abcdefgh1234' }); t.b.tick(8); t.b.settle(600);
        for (let i = 0; i < 3; i++) { t.wait(61000); t.b.tick(8); t.ha.push(); t.b.settle(400); }
        t.has(t.texts(), /^inside$/i, 'its name'); t.has(t.texts(), /21\.4/, 'its value');
        t.ok(asked && asked.statistic_ids?.includes('sensor.test_temp') && asked.period === 'hour', `read its hourly means: ${JSON.stringify(asked)?.slice(0, 160)}`);
        const v = t.b.m.UTF8ToString(t.b.m._fw_ha_get?.(t.b.m.stringToNewUTF8('panel.history'), t.b.m.stringToNewUTF8('sensor.test_temp')) ?? 0);
        t.ok(!t.b.m._fw_ha_get || v.split(',').length === means.length, `panel.history holds the means: ${v}`);
      } finally { t.ha.wsx = null; }
    } },
    { name: 'the slim and wide sizes only where the component lists them; a Header has a label and no entity', async run(t) {
      await t.device(DECK);
      let rev = 20;
      for (const [c, why] of [
        [{ t: 'light', e: 'light.test_lamp', size: '3x1' }, 'a light at 3x1'],
        [{ t: 'entity', e: 'light.test_lamp', size: '2xhalf' }, 'an entity tile at 2xhalf'],
        [{ t: 'stat', e: T, size: '3xhalf' }, 'a stat as a full-width row'],
        [{ t: 'header', label: 'Lights', size: '1x1' }, 'a header at 1x1'],
        [{ t: 'slider', e: 'light.test_lamp', size: '1x1' }, 'a slider at 1x1']]) t.ok(refused(send(t, deck([c], rev++)), 'bad_size'), `${why}: refused`);
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
      t.eq(s.cards?.chips?.grid, '3xhalf,2xhalf', 'Chip row: the full row or two wide');
      t.eq(s.cards?.stats?.grid, '3x1,3xhalf', 'Stat row: full width');
      t.eq(s.cards?.mode?.grid, '3xhalf,2xhalf', 'Mode switch: the full row or two wide');
      t.eq(s.cards?.spark?.grid, '2xhalf,3xhalf', 'Sparkline: two wide or the full row');
      t.eq(s.cards?.slider?.grid, '2xhalf,3xhalf', 'Slider strip: two wide or the full row');
      t.eq(s.grid_sizes, '1x1,2x1,2x2,half', 'every type keeps the usual four');
    } },
  ],
};
