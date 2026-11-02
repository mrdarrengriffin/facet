// Feeds (core/feed.h, CONTRACT.md C51a): every feed on the Feed base. Its fields as sources/sources.json lists them
// (required / optional), its sample words (one sample round each), and Home Assistant dropping and coming back: the
// screen shows the base's words (Offline: the host says Home Assistant is not reached), then the feed refreshes (the weather asks its forecast again). Home
// Assistant is faked (tools/functional.mjs FakeHA); W is NOT a real entity.
import fs from 'node:fs';

const SRC = JSON.parse(fs.readFileSync(new URL('../../sources/sources.json', import.meta.url))).feeds;
const W = 'weather.test_home';
const lab = (t, cmd) => JSON.parse(t.b.str(cmd, (c) => t.b.str('', (a) => t.b.m.UTF8ToString(t.b.m._fw_lab(t.b.stage, c, a)))));
const asks = (t) => JSON.parse(t.b.m.UTF8ToString(t.b.m._fw_ha_asks()) || '[]');  // the asks sent since the last call

export default {
  id: 'feeds', family: 'device', about: 'Every feed on the Feed base: fields = sources.json, sample words, Offline and back.',
  tests: [
    { name: 'every feed: its fields are sources.json\'s (required / optional), and its sample puts sample words', async run(t) {
      await t.mount('spec', { screen: 'weather', source: 'sample' });
      const feeds = lab(t, 'feeds');
      t.ok(feeds.length >= 6, `${feeds.length} feeds`);
      for (const f of feeds) {
        const want = SRC[f.entity];
        t.ok(want, `${f.entity} is in sources.json feeds`);
        if (!want) continue;
        t.eq(f.fields.filter((x) => x.required).map((x) => x.name), want.required, `${f.entity} required`);
        t.eq(f.fields.filter((x) => !x.required).map((x) => x.name), want.optional, `${f.entity} optional`);
        t.ok(f.samples.length > 0, `${f.entity}: ${f.samples.length} sample words`);
      }
      t.eq(Object.keys(SRC).sort(), feeds.map((f) => f.entity).sort(), 'no feed in sources.json that the code does not have');
    } },
    { name: 'Home Assistant drops: the base\'s words (Offline); it comes back: the feed refreshes and asks again', async run(t) {
      t.ha.s[W] = { '': 'cloudy', temperature: '10.2' };
      await t.mount('spec', { screen: 'weather', source: 'ha', weather: W });
      t.has(t.texts(), /^10°$/, 'the weather now');
      t.eq(asks(t).length, 2, 'the forecast asked (hourly + daily), never answered');
      t.ha.disconnect();
      t.has(t.texts(), /^offline$/i, 'Offline (feed::words) while Home Assistant is away');
      t.has(t.texts(), /^--°$/, 'the temperature its placeholder');
      t.ha.reconnect();
      t.has(t.texts(), /^10°$/, 'the weather back');
      const again = asks(t);
      t.eq(again.map((a) => a.data.type).sort(), ['daily', 'hourly'], 'the forecast asked again on the way back (the first asks died with the link)');
    } },
  ],
};
