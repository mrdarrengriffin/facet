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
async function linked(t, params, show = 'media') {
  t.ha.answers = true;
  await t.device({ v: 2, rev: 9, start: 'h', pages: [
    { id: 'h', kind: 'home', name: 'Test', cards: [{ t: 'page', page: 'media', size: 'm' }] },
    { id: 'media', kind: 'media', params }] });
  t.b.spec('act', { do: 'ha.connect', url: 'http://ha.test:8123', token: 'abcdefgh1234' }); t.b.tick(8); t.b.settle(600);
  t.show(show); for (let i = 0; i < 4; i++) { t.wait(1500); t.b.tick(8); t.ha.push(); t.b.settle(400); }
  return t;
}
import { frame565 } from './_release.mjs';
// album art: what the CD shows at 0.7 of its radius, straight up from its centre (inside the printed band, clear of the
// hub and the print mark): RGB 0-255 from the RGB565 frame
function cdBand(t) {
  const v = t.part('media_visual'), f = frame565(t), x = Math.round(v.x + v.w / 2), y = Math.round(v.y + v.h / 2 - 0.7 * v.w / 2);
  const q = f[y * t.b.S + x];
  return [(q >> 11) << 3, ((q >> 5) & 63) << 2, (q & 31) << 3];
}
const red = (c) => c[0] > 180 && c[1] < 90 && c[2] < 90, blue = (c) => c[2] > 180 && c[0] < 90 && c[1] < 120;
const ART = (n, tok = 'a1') => `/api/media_player_proxy/${P1}?token=${tok}&cache=${n}`;  // as HA gives entity_picture
const HA_URL = 'http://ha.test:8123';
const sheet = (t) => { const b = t.comps().find((c) => c.ids.includes('icon_button') && c.y < 100 && c.x > t.b.S / 2); t.tap(b); return t.texts(); };
const btn = (t, i) => t.comps().filter((c) => c.ids.includes('icon_button') && c.y > 200).sort((a, b) => a.x - b.x)[i];

export default {
  id: 'media_screen', family: 'screens', about: 'The Media screen on the media feed: the hero, the controls, the volume, the speaker sheet, the HA adapter, Home card.',
  tests: [
    { name: 'album art: the playing player\'s picture, fetched over the panel\'s HA link, is printed on the CD (plain while it loads, after a 404, with none)', async run(t) {
      t.ha.s[P1] = { '': 'playing', friendly_name: 'Kitchen', media_title: 'Red Song', media_artist: 'Band', media_duration: '200', media_position: '20', entity_picture: ART('red') };
      t.img.serve(HA_URL + ART('red'), 64, 64, [230, 30, 30, 255]);
      await linked(t, { source: 'ha', visual: 'cd' });
      t.ok(t.img.asked.includes(HA_URL + ART('red')), `fetched with the link's address in front: ${t.img.asked}`);
      t.ok(red(cdBand(t)), `the CD shows the art: ${cdBand(t)}`);
      t.ha.set(P1, undefined, { entity_picture: ART('gone') });  // HA has no picture there (404)
      t.wait(1500);
      const plain = cdBand(t);
      t.ok(!red(plain), `a 404: the plain CD again, no error (${plain})`);
      t.hasNot(t.texts(), /404|not found|failed/i, 'no error words');
      t.ha.set(P1, undefined, { entity_picture: null });
      t.wait(1500);
      t.eq(cdBand(t), plain, 'no art: the plain CD');
    } },
    { name: 'album art: a new track swaps it (the old one stays until the new one is in); HA\'s token turning over fetches nothing', async run(t) {
      t.ha.s[P1] = { '': 'playing', friendly_name: 'Kitchen', media_title: 'Red Song', media_duration: '200', media_position: '20', entity_picture: ART('red') };
      t.img.serve(HA_URL + ART('red'), 64, 64, [230, 30, 30, 255]).serve(HA_URL + ART('blue'), 48, 48, [30, 30, 230, 255]).hang(HA_URL + ART('slow'));
      await linked(t, { source: 'ha', visual: 'cd' });
      t.ok(red(cdBand(t)), `red first: ${cdBand(t)}`);
      const n = t.img.asked.length;
      t.ha.set(P1, undefined, { entity_picture: ART('red', 'a2') }); t.wait(1500);  // HA's 5-minute token rotation
      t.eq(t.img.asked.length, n, 'a new token alone: nothing fetched');
      t.ok(red(cdBand(t)), 'still red');
      t.ha.set(P1, undefined, { media_title: 'Slow Song', entity_picture: ART('slow') }); t.wait(1500);
      t.ok(red(cdBand(t)), `the next art still loading: the old one stays (${cdBand(t)})`);
      t.wait(9000);
      t.ok(!red(cdBand(t)) && !blue(cdBand(t)), `it never came (timeout): the plain CD (${cdBand(t)})`);
      t.ha.set(P1, undefined, { media_title: 'Blue Song', entity_picture: ART('blue') }); t.wait(1500);
      t.ok(blue(cdBand(t)), `the new track's art: ${cdBand(t)}`);
    } },
    { name: 'album art: play -> pause -> play with the same art fetches nothing, rebuilds nothing, and the art is drawn in every frame', async run(t) {
      t.ha.s[P1] = { '': 'playing', friendly_name: 'Kitchen', media_title: 'Red Song', media_artist: 'Band', media_duration: '200', media_position: '20', entity_picture: ART('red') };
      t.img.serve(HA_URL + ART('red'), 64, 64, [230, 30, 30, 255]);
      await linked(t, { source: 'ha', visual: 'cd' });
      t.ok(red(cdBand(t)), `red first: ${cdBand(t)}`);
      t.img.hang(HA_URL + ART('red'));  // a second fetch would take its time (a panel's Wi-Fi): the art would go missing meanwhile
      const m = t.b.m, v = t.part('media_visual'), hero = t.part('media_hero'), asked = t.img.asked.length, st0 = t.img.stats();
      // the areas LVGL flushed since the last read (read every 20 ms: under the stage's 64-area log, so a big one is real)
      const areas = () => { const q = new Int32Array(m.HEAPU8.buffer), p = m._fw_areas(t.b.stage) >> 2, out = []; for (let i = 0; i < q[p]; i++) { const [x1, y1, x2, y2] = q.subarray(p + 1 + i * 4, p + 5 + i * 4); out.push((x2 - x1 + 1) * (y2 - y1 + 1)); } return out; };
      let frames = 0, gaps = 0, biggest = 0;
      const watch = (ms) => { for (let e = 0; e < ms; e += 20) { t.ha.react(t.wait(20)); frames++; if (!red(cdBand(t))) gaps++; biggest = Math.max(biggest, ...areas()); } };
      const press = () => { const b = btn(t, 1); t.b.touch(b.x + b.w / 2, b.y + b.h / 2, 1); watch(80); t.b.touch(b.x + b.w / 2, b.y + b.h / 2, 0); watch(200); };
      const echo = (st) => { t.ha.s[P1][''] = st; t.ha.push(P1); };  // HA's new state, no settling: every frame is watched
      areas();
      press(); echo('paused'); watch(4000);  // the panel's own tap, then HA's echo
      t.has(t.texts(), /^paused$/i, 'paused');
      press(); echo('playing'); watch(4000);
      t.has(t.texts(), /^now playing$/i, 'playing again');
      echo('paused'); watch(2000); echo('playing'); watch(2000);  // from Home Assistant (no hold)
      t.eq(t.ha.calls.filter((c) => c.service === 'media_player.media_play_pause').length, 2, 'both taps went out');
      t.eq(t.img.asked.length, asked, `no image request: ${t.img.asked.slice(asked)}`);
      const st = t.img.stats();
      t.eq([st.fetches, st.dropped], [st0.fetches, st0.dropped], `the art's view kept (no re-decode): ${JSON.stringify(st)}`);
      t.eq(gaps, 0, `the art drawn in all ${frames} frames`);
      t.ok(biggest < (v.w * v.h) / 2, `no rebuild: the biggest redraw ${biggest} px < half the picture (${v.w}x${v.h}; the hero ${hero.w}x${hero.h})`);
    } },
    { name: 'spin on: the CD turns with its art while playing; pause stops it in place: nothing fetched, nothing rebuilt, the art in every frame', async run(t) {
      t.ha.s[P1] = { '': 'playing', friendly_name: 'Kitchen', media_title: 'Red Song', media_artist: 'Band', media_duration: '200', media_position: '20', entity_picture: ART('red') };
      t.img.serve(HA_URL + ART('red'), 64, 64, [230, 30, 30, 255]);
      await linked(t, { source: 'ha', visual: 'cd', spin: 'true' });
      t.ok(red(cdBand(t)), `red first: ${cdBand(t)}`);
      t.img.hang(HA_URL + ART('red'));
      const m = t.b.m, v = t.part('media_visual'), hero = t.part('media_hero'), asked = t.img.asked.length, st0 = t.img.stats();
      const areas = () => { const q = new Int32Array(m.HEAPU8.buffer), p = m._fw_areas(t.b.stage) >> 2, out = []; for (let i = 0; i < q[p]; i++) { const [x1, y1, x2, y2] = q.subarray(p + 1 + i * 4, p + 5 + i * 4); out.push((x2 - x1 + 1) * (y2 - y1 + 1)); } return out; };
      let frames = 0, gaps = 0, biggest = 0, redraws = 0;
      const watch = (ms) => { for (let e = 0; e < ms; e += 20) { t.ha.react(t.wait(20)); frames++; if (!red(cdBand(t))) gaps++; const a = areas(); redraws += a.length; biggest = Math.max(biggest, ...a); } };
      const echo = (st) => { t.ha.s[P1][''] = st; t.ha.push(P1); };
      areas();
      watch(1000);
      t.ok(redraws > 10 && biggest > (v.w * v.h) / 2, `playing: the CD's printed circle redrawn as it turns (${redraws} areas, biggest ${biggest} px)`);
      echo('paused'); watch(6000);
      redraws = 0; biggest = 0; watch(1000);
      t.eq(redraws, 0, 'paused: stopped, nothing redrawn');
      echo('playing'); watch(2000);
      t.eq(t.img.asked.length, asked, `no image request: ${t.img.asked.slice(asked)}`);
      const st = t.img.stats();
      t.eq([st.fetches, st.dropped], [st0.fetches, st0.dropped], `the art's view kept (no re-decode): ${JSON.stringify(st)}`);
      t.eq(gaps, 0, `the art drawn in all ${frames} frames`);
      t.ok(biggest <= v.w * v.h, `no rebuild: the biggest redraw ${biggest} px <= the picture (${v.w}x${v.h}; the hero ${hero.w}x${hero.h})`);
    } },
    { name: 'Home card: play -> pause -> play with the same art fetches nothing, rebuilds nothing, and the art is drawn in every frame', async run(t) {
      t.ha.s[P1] = { '': 'playing', friendly_name: 'Kitchen', media_title: 'Red Song', media_duration: '200', media_position: '20', entity_picture: ART('red') };
      t.img.serve(HA_URL + ART('red'), 64, 64, [230, 30, 30, 255]);
      await linked(t, { source: 'ha', visual: 'cd' }, 'home');
      const c = t.part('card'), v = t.part('media_visual');
      t.ok(c && v && v.w < c.w, `the Home card's picture: ${JSON.stringify(v)}`);
      t.ok(red(cdBand(t)), `red first: ${cdBand(t)}`);
      t.img.hang(HA_URL + ART('red'));  // a second fetch would take its time: the art would go missing meanwhile
      const m = t.b.m, asked = t.img.asked.length, st0 = t.img.stats();
      const areas = () => { const q = new Int32Array(m.HEAPU8.buffer), p = m._fw_areas(t.b.stage) >> 2, out = []; for (let i = 0; i < q[p]; i++) { const [x1, y1, x2, y2] = q.subarray(p + 1 + i * 4, p + 5 + i * 4); out.push((x2 - x1 + 1) * (y2 - y1 + 1)); } return out; };
      let frames = 0, gaps = 0, biggest = 0;
      const watch = (ms) => { for (let e = 0; e < ms; e += 20) { t.wait(20); frames++; if (!red(cdBand(t))) gaps++; biggest = Math.max(biggest, ...areas()); } };
      const echo = (st) => { t.ha.s[P1][''] = st; t.ha.push(P1); };
      areas();
      echo('paused'); watch(2000);
      t.ok(t.part('card').texts.some((x) => /paused/i.test(x)), `the sub says paused: ${t.part('card').texts}`);
      echo('playing'); watch(2000); echo('paused'); watch(2000); echo('playing'); watch(2000);
      t.ok(!t.part('card').texts.some((x) => /paused/i.test(x)), `playing again: ${t.part('card').texts}`);
      t.eq(t.img.asked.length, asked, `no image request: ${t.img.asked.slice(asked)}`);
      const st = t.img.stats();
      t.eq([st.fetches, st.dropped], [st0.fetches, st0.dropped], `the art's view kept (no re-decode): ${JSON.stringify(st)}`);
      t.eq(gaps, 0, `the art drawn in all ${frames} frames`);
      t.ok(biggest < (c.w * c.h) / 2, `no rebuild: the biggest redraw ${biggest} px < half the card (${c.w}x${c.h})`);
    } },
    { name: 'album art: memory stays flat over 240 track changes (an hour of 15 s tracks)', async run(t) {
      t.ha.s[P1] = { '': 'playing', friendly_name: 'Kitchen', media_title: 'Song 0', media_duration: '200', media_position: '20', entity_picture: ART('t0') };
      for (let i = 0; i < 240; i++) t.img.serve(HA_URL + ART('t' + i), 300, 300, [i % 256, 128, 255 - (i % 256), 255]);
      await linked(t, { source: 'ha', visual: 'cd' });
      const heap = () => t.b.m.HEAPU8.length, st0 = t.img.stats();
      let h10 = 0, peak = 0;
      for (let i = 1; i < 240; i++) {
        t.ha.set(P1, undefined, { media_title: 'Song ' + i, entity_picture: ART('t' + i) });
        t.wait(1500);
        const st = t.img.stats();
        peak = Math.max(peak, st.bytes);
        if (i === 10) h10 = heap();
      }
      const st = t.img.stats();
      t.log.push(`img stats after 240 tracks: ${JSON.stringify(st)}; WASM heap ${h10} -> ${heap()} bytes`);
      t.ok(st.views <= 2 && st.bytes <= st0.bytes * 2 + 4096, `at most the shown + the coming art: ${JSON.stringify(st)} (start ${st0.bytes} B)`);
      t.ok(peak <= st0.bytes * 2 + 4096, `peak ${peak} B`);
      t.eq(heap(), h10, 'the WASM heap did not grow after the first 10 tracks');
    } },
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
