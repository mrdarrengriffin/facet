// The config backup (#170; core/device.h config_json, GET /fw/config on the panels, fw_test "config" here): the deck the
// panel runs and its settings, as one JSON with a format version, never a secret; what it exports goes back in.
const config = (t) => t.b.test('config', '');
const SECRET = /token|password|passwd|secret|psk|api_?key|ssid|wifi|bearer|authorization/i;
const keysOf = (v, out = []) => {  // every object key at any depth
  if (Array.isArray(v)) v.forEach((x) => keysOf(x, out));
  else if (v && typeof v === 'object') for (const [k, x] of Object.entries(v)) { out.push(k); keysOf(x, out); }
  return out;
};
const DECK = { v: 2, rev: 7, start: 'h', pages: [
  { id: 'h', kind: 'home', name: 'Kitchen', cards: [{ t: 'page', page: 'w', size: 'l' }] },
  { id: 'w', kind: 'weather', params: { source: 'sample' } }] };

export default {
  id: 'config_backup', family: 'device', about: 'The config backup: the deck + settings with a format version, no secrets, and it restores.',
  tests: [
    { name: 'the export: format and version, the deck the panel runs, every setting', async run(t) {
      await t.device(DECK);
      t.setting('theme', 'light');
      const c = config(t);
      t.eq([c.facet, c.version], ['config', 1], 'format + version');
      t.eq(c.deck.pages.map((p) => p.id), ['h', 'w'], 'the deck the panel runs');
      t.eq(c.deck.pages[0].name, 'Kitchen', 'its page names');
      t.eq(c.settings.theme, 'light', 'the settings, as they are now');
      t.ok(Object.keys(c.settings).length >= 10, 'every setting');
    } },
    { name: 'no secrets: no key at any depth names a token, password, Wi-Fi or API key', async run(t) {
      await t.device();
      t.eq(keysOf(config(t)).filter((k) => SECRET.test(k)), [], 'the factory config');
      const r = JSON.parse(t.b.spec('deck', { ...DECK, pages: [DECK.pages[0], { ...DECK.pages[1], params: { source: 'sample', api_key: 'abc', ha_token: 'xyz' } }] }) || '{}');
      t.b.settle(800);
      const c = config(t), s = JSON.stringify(c);
      t.eq(keysOf(c).filter((k) => SECRET.test(k)), [], `a deck that carried some (${r.ok === false ? 'refused' : 'kept'}): dropped`);
      t.ok(!/abc|xyz/.test(s), 'their values are not in it');
    } },
    { name: 'round trip: the export goes back in as the deck and the settings', async run(t) {
      await t.device(DECK);
      t.setting('theme', 'light'); t.setting('distanceUnit', 'mi');
      const c = config(t);
      await t.device();
      t.setting('theme', 'dark'); t.setting('distanceUnit', 'km');
      const d = JSON.parse(t.b.spec('deck', c.deck) || '{}'); t.b.settle(800);
      t.ok(!d.errors?.length, `the deck is taken (${JSON.stringify(d.errors ?? []).slice(0, 120)})`);
      const r = t.b.test('keys', JSON.stringify({ op: 'write', route: '/fw/settings', body: { values: c.settings }, editor: true }));
      t.eq(r.code, 200, `the settings are taken (${JSON.stringify(r.body).slice(0, 120)})`);
      const back = config(t);
      t.eq(back.deck.pages.map((p) => p.id), ['h', 'w'], 'the same pages');
      t.eq([back.settings.theme, back.settings.distanceUnit], ['light', 'mi'], 'the same settings');
    } },
  ],
};
