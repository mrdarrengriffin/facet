// Settings > Screens > a screen (screens/screen_set.cpp): every row's words show whole (no title or value shortened
// to "...": the lab proof's "cut", core/main.cpp), on every screen's page and in every sheet it opens, at both sizes;
// and the Overhead page as reviewed: one Source row with a short value and a sheet of one-line sources, the sensor
// found by itself (a choice only when Home Assistant has more than one), the radar centred on Home Assistant's home
// with no Latitude / Longitude rows, Range and Lowest altitude as value rows in the distance unit's words.
// The fake Home Assistant only (tools/lib/fakeha.mjs); never a real one.
import { stage } from './_release.mjs';

const IDS = ['calendar', 'clock', 'energy', 'media', 'moon', 'overhead', 'timer', 'weather'];
const lab = (t, cmd, arg = '') => t.b.str(cmd, (c) => t.b.str(arg, (a) => t.b.m.UTF8ToString(t.b.m._fw_lab(t.b.stage, c, a))));
const cut = (t) => JSON.parse(lab(t, 'proof')).cut.map((c) => c.why);
const set = (t, id, v) => t.b.str(id, (p) => t.b.str(v, (q) => t.b.m._fw_setting(p, q)));
// the page tall enough to show every row at once (no scrolling), then its proof
async function page(t, id) {
  t.ha.s['zone.home'] = { '': 'zoning', latitude: '52.0', longitude: '0.0', friendly_name: 'Home' };
  await t.mount('screen_set', { id });
  stage(t, t.b.S, t.b.S * 3); t.wait(400);
  return t;
}
const rows = (t) => t.comps().filter((c) => c.ids.includes('list_row'));
const rowOf = (t, title) => rows(t).find((r) => r.texts?.[0] === title);
// the device linked to the fake HA (its entity registry: each entity's _platform), then Settings > Screens > Overhead
const act = (t, a) => { const r = JSON.parse(t.b.spec('act', a) || '{}'); t.b.tick(10); t.b.settle(800); return r; };
async function linked(t, extra) {
  t.ha.answers = true;
  Object.assign(t.ha.s, extra);
  t.ha.s['zone.home'] = { '': 'zoning', latitude: '52.0', longitude: '0.0' };
  await t.device();
  act(t, { do: 'ha.connect', url: 'http://ha.test:8123', token: 'abcdefgh1234' });
  act(t, { do: 'nav.open', screen: 'screen_set', params: { id: 'overhead' } });
  for (let i = 0; i < 4; i++) { t.wait(800); t.b.tick(8); t.ha.push(); }
  act(t, { do: 'nav.back' });
  act(t, { do: 'nav.open', screen: 'screen_set', params: { id: 'overhead' } }); t.wait(600); t.ha.push();
  act(t, { do: 'nav.back' });
  act(t, { do: 'nav.open', screen: 'screen_set', params: { id: 'overhead' } }); t.wait(600);
  return t;
}

export default {
  id: 'settings_fit', family: 'device', about: 'Every screen\'s settings page shows its words whole; the Overhead page as reviewed.',
  tests: [
    { name: 'every screen\'s settings and every sheet: nothing cut to "..."', async run(t) {
      for (const id of IDS) {
        await page(t, id);
        t.eq(cut(t), [], `${id}: the page`);
        for (const r of rows(t).filter((x) => x.texts?.length && t.find(x.texts[0]))) {
          const before = t.texts().length;
          t.tap(r.texts[0]); t.wait(500);
          if (t.texts().length !== before) t.eq(cut(t), [], `${id}: the ${r.texts[0]} sheet`);
          await page(t, id);
        }
      }
    } },
    { name: 'the proof\'s cut finds a shortened row (break it: a long source name)', async run(t) {
      await t.mount('list_row', { title: 'Aircraft sensor', trailing: 'value-chevron', value: 'sensor.flightradar24_current_in_area_and_more' });
      stage(t, 320, 120); t.wait(200);
      t.ok(cut(t).length > 0, `a cut row is reported: ${JSON.stringify(cut(t))}`);
    } },
    { name: 'Overhead: one Source row, a short value, the sources one line each in its sheet', async run(t) {
      await page(t, 'overhead'); t.wait(1200);  // the feed's word follows Home Assistant's home (asked every 0.5 s)
      t.ok(t.find('Source'), 'a Source row'); t.has(t.texts(), /^adsb\.lol( · centred on Home)?$/, 'its value: the short name (+ where it is centred)');
      for (const gone of ['Aircraft sensor', 'Latitude', 'Longitude']) t.ok(!t.texts().includes(gone), `no ${gone} row`);
      t.tap('Source'); t.wait(500);
      for (const w of [/^FlightRadar24$/, /^ADS-B receiver$/, /^Sample sky$/, /OVER THE INTERNET/i]) t.has(t.texts(), w, `the sheet: ${w}`);
      t.eq(cut(t), [], 'no line cut');
    } },
    { name: 'Overhead: centred on Home Assistant\'s home, said in the Source row (no From row)', async run(t) {
      await page(t, 'overhead'); t.wait(1200);
      t.has(t.texts(), /^adsb\.lol · centred on Home$/, 'the Source value says where it is centred');
      t.ok(!t.texts().some((x) => /^From /.test(x)), 'no From row');
      t.ha.s['zone.home'] = { '': 'zoning', latitude: '', longitude: '' };
      await t.mount('screen_set', { id: 'overhead' }); t.wait(1200);
      t.has(t.texts(), /^adsb\.lol · no home location$/, 'none: it says so');
    } },
    { name: 'Overhead: Range and Lowest altitude are value rows in the unit\'s words', async run(t) {
      await page(t, 'overhead');
      t.has(t.texts(), /^50 km$/, 'Range in km'); t.has(t.texts(), /^Any height$/, 'Lowest altitude: any');
      t.tap('Lowest altitude'); t.wait(500);
      t.has(t.texts(), /^900 m$/, 'metres with km');
      set(t, 'distanceUnit', 'mi');
      t.b.rebuild('screen_set', { id: 'overhead' }); t.wait(400);
      t.has(t.texts(), /^50 mi$/, 'Range in miles');
      t.tap('Lowest altitude'); t.wait(500);
      t.has(t.texts(), /^3,000 ft$/, 'feet with miles');
      const ev = t.tap('3,000 ft'); t.wait(500);
      t.ok(ev.some((e) => e.name === 'deck' && /min_alt=3000/.test(e.detail)), 'the pick keeps 3000 (feet)');
      set(t, 'distanceUnit', 'km');
    } },
    { name: 'Overhead: FlightRadar24 finds its sensor; two candidates = a choice in the Source sheet', async run(t) {
      await linked(t, { 'sensor.flightradar24_current_in_area': { '': '3', friendly_name: 'FlightRadar24 Current in area', _platform: 'flightradar24' },
        'sensor.flightradar24_additional_tracked': { '': '0', friendly_name: 'FlightRadar24 Additional tracked', _platform: 'flightradar24' } });
      t.tap('Source'); t.wait(500);
      t.eq(t.texts().filter((x) => x === 'FlightRadar24').length, 1, 'one FlightRadar24 row (one sensor in its area)');
      await linked(t, { 'sensor.flightradar24_current_in_area': { '': '3', friendly_name: 'FR24 home', _platform: 'flightradar24' },
        'sensor.flightradar24_current_in_area_2': { '': '1', friendly_name: 'FR24 cottage', _platform: 'flightradar24' } });
      t.tap('Source'); t.wait(500);
      t.eq(t.texts().filter((x) => x === 'FlightRadar24').length, 2, 'two areas: two rows');
      t.has(t.texts(), /^FR24 COTTAGE$/i, 'each says its sensor');
      const ev = t.tap(/^FR24 COTTAGE$/i); t.wait(500);
      t.ok(ev.some((e) => e.name === 'deck' && /sensor=sensor\.flightradar24_current_in_area_2/.test(e.detail)), 'the pick sets the sensor');
      t.ok(ev.some((e) => e.name === 'deck' && /source=fr24/.test(e.detail)), 'and the source');
    } },
  ],
};
