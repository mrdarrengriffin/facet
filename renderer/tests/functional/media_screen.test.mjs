// Media screen (specs/media.json: Page > Header (the speakers Icon button) > Body: Media hero (Media visual + the
// track), a Group of the prev / play / next Icon buttons, the vol Slider row; the speaker Bottom sheet of Picker rows on
// the Overlay), on the media feed panel.media (core/media_source.cpp, CONTRACT.md C51) and its Home card (C52). Home
// Assistant is faked (tools/functional.mjs FakeHA); P1 / P2 are NOT Darren's entities. Approved original: Astro
// packages/ui/src/screens/media.ts (was components/media_view.cpp + media_disc.cpp).
const P1 = 'media_player.test_kitchen', P2 = 'media_player.test_office';
async function open(t, params) { await t.mount('spec', { screen: 'media', ...params }); return t; }
const deck = (params) => ({ v: 2, rev: 9, start: 'h', pages: [
  { id: 'h', kind: 'home', name: 'Test', cards: [{ t: 'page', page: 'm', size: 'l' }] },
  { id: 'm', kind: 'media', params }] });
const btn = (t, i) => t.comps().filter((c) => c.ids.includes('icon_button') && c.y > 200).sort((a, b) => a.x - b.x)[i];

export default {
  id: 'media_screen', family: 'screens', about: 'The Media screen on the media feed: the hero, the controls, the volume, the speaker sheet, the HA adapter, Home card.',
  tests: [
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
    { name: 'ha: the player live; play / pause and next go out as media_player services', async run(t) {
      t.ha.s[P1] = { '': 'playing', friendly_name: 'Kitchen', media_title: 'Test Song', media_artist: 'Test Band', volume_level: '0.3', media_duration: '200', media_position: '20' };
      await open(t, { source: 'ha', entity: P1, speakers: `${P1},${P2}` });
      t.has(t.texts(), /^test song$/i, 'the title'); t.has(t.texts(), /^test band$/i, 'the artist'); t.has(t.texts(), /^30$/, 'the volume');
      t.tap(btn(t, 1));
      t.ok(t.ha.calls.some((c) => c.service === 'media_player.media_play_pause' && c.entity === P1), 'play_pause on the player');
      t.tap(btn(t, 2));
      t.ok(t.ha.calls.some((c) => c.service === 'media_player.media_next_track' && c.entity === P1), 'next on the player');
    } },
    { name: 'ha: an unavailable player says so', async run(t) {
      t.ha.s[P1] = { '': 'unavailable' };
      await open(t, { source: 'ha', entity: P1 });
      t.has(t.texts(), /^unavailable$/i, 'unavailable');
    } },
    { name: 'Home card (C52): the track on the same feed', async run(t) {
      await t.device(deck({ source: 'sample' }));
      const c = t.part('card');
      t.ok(c, 'the page card'); t.ok(c.texts.some((x) => /harbour lights/i.test(x)), 'the track on the card');
    } },
  ],
};
