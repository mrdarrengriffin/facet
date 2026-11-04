// Media screen (specs/media.json: Page > Header (the speakers Icon button) > Body: Media hero (Media visual + the
// track), a Group of the prev / play / next Icon buttons, the vol Slider row; the speaker Bottom sheet of Picker rows on
// the Overlay), on the media feed panel.media (core/media_source.cpp, CONTRACT.md C51) and its Home card (C52). Home
// Assistant is faked (tools/functional.mjs FakeHA); P1 / P2 are NOT real entities. Approved original: Astro
// packages/ui/src/screens/media.ts.
const P1 = 'media_player.test_kitchen', P2 = 'media_player.test_office';
async function open(t, params) { await t.mount('spec', { screen: 'media', ...params }); return t; }
const deck = (params) => ({ v: 2, rev: 9, start: 'h', pages: [
  { id: 'h', kind: 'home', name: 'Test', cards: [{ t: 'page', page: 'm', size: 'l' }] },
  { id: 'm', kind: 'media', params }] });
// the device with the Media page, linked to the fake HA (the registries: each entity's _platform / _area), on Media
async function linked(t, params) {
  t.ha.answers = true;
  await t.device({ v: 2, rev: 9, start: 'h', pages: [
    { id: 'h', kind: 'home', name: 'Test', cards: [{ t: 'page', page: 'media', size: 'm' }] },
    { id: 'media', kind: 'media', params }] });
  t.b.spec('act', { do: 'ha.connect', url: 'http://ha.test:8123', token: 'abcdefgh1234' }); t.b.tick(8); t.b.settle(600);
  t.show('media'); for (let i = 0; i < 4; i++) { t.wait(1500); t.b.tick(8); t.ha.push(); t.b.settle(400); }
  return t;
}
const sheet = (t) => { const b = t.comps().find((c) => c.ids.includes('icon_button') && c.y < 100 && c.x > t.b.S / 2); t.tap(b); return t.texts(); };
const btn = (t, i) => t.comps().filter((c) => c.ids.includes('icon_button') && c.y > 200).sort((a, b) => a.x - b.x)[i];

export default {
  id: 'media_screen', family: 'screens', about: 'The Media screen on the media feed: the hero, the controls, the volume, the speaker sheet, the HA adapter, Home card.',
  tests: [
    { name: 'nothing picked (a fresh panel): every Home Assistant media player, never Offline', async run(t) {
      t.ha.s['media_player.test_kitchen'] = { '': 'idle', friendly_name: 'Kitchen' };
      t.ha.s['media_player.test_office'] = { '': 'idle', friendly_name: 'Office' };
      t.ha.answers = true;
      await t.device({ v: 2, rev: 9, start: 'h', pages: [
        { id: 'h', kind: 'home', name: 'Test', cards: [{ t: 'page', page: 'media', size: 'm' }] },
        { id: 'media', kind: 'media', params: { source: 'ha' } }] });
      t.b.spec('act', { do: 'ha.connect', url: 'http://ha.test:8123', token: 'abcdefgh1234' }); t.b.tick(8); t.b.settle(600);
      t.show('media'); t.wait(61000); t.b.tick(8); t.b.settle(800);
      t.hasNot(t.texts(), /^Offline$/i, `not Offline: ${t.texts().slice(0, 10)}`);
    } },
    { name: 'sample: the track playing, its artist, the volume', async run(t) {
      await open(t, { source: 'sample' });
      const tx = t.texts();
      t.has(tx, /^now playing$/i, 'the state'); t.has(tx, /^harbour lights$/i, 'the title'); t.has(tx, /^the lanterns$/i, 'the artist');
      t.has(tx, /^vol$/i, 'the volume label'); t.has(tx, /^42$/, 'the volume');
      t.ok(t.part('media_visual'), 'the picture');
    } },
    { name: 'sample: play pauses, next skips, nothing goes to Home Assistant', async run(t) {
      await open(t, { source: 'sample' });
      t.tap(btn(t, 1));
      t.has(t.texts(), /^paused$/i, 'paused at once');
      t.tap(btn(t, 2));
      t.has(t.texts(), /^paper moons$/i, 'the next track');
      t.eq(t.ha.calls.length, 0, 'no HA call on the sample');
    } },
    { name: 'speaker sheet: the button opens it; a pick names the speaker and closes it', async run(t) {
      await open(t, { source: 'sample' });
      const b = t.comps().find((c) => c.ids.includes('icon_button') && c.y < 100 && c.x > t.b.S / 2);
      t.ok(b, 'the speakers button'); t.tap(b);
      t.has(t.texts(), /^auto$/i, 'the Auto row'); t.has(t.texts(), /^office speaker$/i, 'a speaker row');
      t.tap(t.find(/^office speaker$/i));
      t.has(t.texts(), /^harbour lights$/i, 'back on the screen');
    } },
    { name: 'ha: the playing player shows by itself; play / pause and next go out on it', async run(t) {
      t.ha.s[P2] = { '': 'idle', friendly_name: 'Office' };
      t.ha.s[P1] = { '': 'playing', friendly_name: 'Kitchen', media_title: 'Test Song', media_artist: 'Test Band', volume_level: '0.3', media_duration: '200', media_position: '20' };
      await linked(t, { source: 'ha' });
      t.has(t.texts(), /^test song$/i, 'the title'); t.has(t.texts(), /^test band$/i, 'the artist'); t.has(t.texts(), /^30$/, 'the volume');
      t.tap(btn(t, 1));
      t.ok(t.ha.calls.some((c) => c.service === 'media_player.media_play_pause' && c.entity === P1), 'play_pause on the player');
      t.tap(btn(t, 2));
      t.ok(t.ha.calls.some((c) => c.service === 'media_player.media_next_track' && c.entity === P1), 'next on the player');
    } },
    { name: 'ha: the sheet lists every speaker, playing first, then by area', async run(t) {
      t.ha.s['media_player.test_a'] = { '': 'idle', friendly_name: 'Attic', _area: 'Upstairs' };
      t.ha.s['media_player.test_b'] = { '': 'idle', friendly_name: 'Bedroom', _area: 'Downstairs' };
      t.ha.s['media_player.test_c'] = { '': 'idle', friendly_name: 'Cellar', _area: 'Upstairs' };
      t.ha.s['media_player.test_d'] = { '': 'playing', friendly_name: 'Den', media_title: 'Song', _area: 'Upstairs' };
      await linked(t, { source: 'ha' });
      const tx = sheet(t).filter((x) => /^(attic|bedroom|cellar|den)$/i.test(x)).map((x) => x.toLowerCase());
      t.eq(tx.join(','), 'den,attic,cellar,bedroom', `the order: ${tx}`);
    } },
    { name: 'an integration source: only its players', async run(t) {
      t.ha.s['media_player.test_ma'] = { '': 'idle', friendly_name: 'Lounge MA', _platform: 'music_assistant' };
      t.ha.s['media_player.test_cast'] = { '': 'idle', friendly_name: 'Lounge Cast', _platform: 'cast' };
      await linked(t, { source: 'music_assistant' });
      const tx = sheet(t);
      t.has(tx, /^lounge ma$/i, 'the Music Assistant player'); t.hasNot(tx, /^lounge cast$/i, 'not the Cast one');
    } },
    { name: 'spotify: the Connect devices; a pick plays there (select_source)', async run(t) {
      t.ha.s['media_player.test_spotify'] = { '': 'playing', friendly_name: 'Spotify', media_title: 'Spot Song', source: 'Kitchen', source_list: "['Kitchen', 'Office']", _platform: 'spotify' };
      await linked(t, { source: 'spotify' });
      t.has(t.texts(), /^spot song$/i, 'the track');
      const tx = sheet(t);
      t.has(tx, /^office$/i, 'a Connect device');
      t.tap(t.find(/^office$/i));
      t.ok(t.ha.calls.some((c) => c.service === 'media_player.select_source' && c.entity === 'media_player.test_spotify'), `select_source: ${JSON.stringify(t.ha.calls)}`);
    } },
    { name: 'ha: an unavailable player says so', async run(t) {
      t.ha.s[P1] = { '': 'unavailable' };
      await linked(t, { source: 'ha' });
      t.has(t.texts(), /^unavailable$/i, 'unavailable');
    } },
    { name: 'Home card (C52): the track on the same feed', async run(t) {
      await t.device(deck({ source: 'sample' }));
      const c = t.part('card');
      t.ok(c, 'the page card'); t.ok(c.texts.some((x) => /harbour lights/i.test(x)), 'the track on the card');
    } },
  ],
};
