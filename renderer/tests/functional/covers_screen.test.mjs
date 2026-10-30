// Blinds and curtains screen (specs/covers.json: Page > Header (the window's name, the windows Icon button) > Body:
// Split: the Cover visual as tall as the column; the value Hero block (state, value, kind), the slats Stat + Slider
// (Venetian blinds), Open / Stop / Close Buttons at the foot; the windows Bottom sheet of Picker rows on the Overlay),
// on the covers feed panel.covers (core/covers_source.cpp, CONTRACT.md C51) and its Home card (C52). Home Assistant is
// faked (tools/functional.mjs FakeHA); C1 / C2 are NOT Darren's entities. Approved original: Astro
// packages/ui/src/screens/covers.ts (was components/covers_view.cpp).
const C1 = 'cover.test_study', C2 = 'cover.test_lounge';
async function open(t, params) { await t.mount('spec', { screen: 'covers', ...params }); return t; }
const deck = (params) => ({ v: 2, rev: 9, start: 'h', pages: [
  { id: 'h', kind: 'home', name: 'Test', cards: [{ t: 'page', page: 'c', size: 'm' }] },
  { id: 'c', kind: 'covers', params }] });

export default {
  id: 'covers_screen', family: 'screens', about: 'The Blinds and curtains screen on the covers feed: the window, the value, the buttons, the windows sheet, the HA adapter, Home card.',
  tests: [
    { name: 'sample: the study blind at 60 %, the window as tall as the body', async run(t) {
      await open(t, { source: 'sample' });
      const tx = t.texts();
      t.has(tx, /^study blind$/i, 'the name in the header'); t.has(tx, /^60%$/, 'the value'); t.has(tx, /^60% open$/i, 'the state');
      t.has(tx, /^roller blind$/i, 'the kind'); t.has(tx, /^open$/i, 'Open'); t.has(tx, /^close$/i, 'Close');
      const w = t.part('cover_visual');
      t.ok(w && w.h > 2 * w.w * 0.8 / 2 && w.h > 300 * t.b.S / 720, 'the window fills its column');
    } },
    { name: 'sample: Close moves the blind; nothing goes to Home Assistant', async run(t) {
      await open(t, { source: 'sample' });
      t.tap(t.find(/^close$/i));
      t.wait(400);
      t.has(t.texts(), /^closing$/i, 'closing');
      t.wait(15000);
      t.has(t.texts(), /^closed$/i, 'closed'); t.has(t.texts(), /^Closed$/, 'the value');
      t.eq(t.ha.calls.length, 0, 'no HA call on the sample');
    } },
    { name: 'windows sheet: the button opens it; a pick shows that window (Venetian: the slats)', async run(t) {
      await open(t, { source: 'sample' });
      const b = t.comps().find((c) => c.ids.includes('icon_button') && c.y < 100 && c.x > t.b.S / 2);
      t.ok(b, 'the windows button'); t.tap(b);
      t.has(t.texts(), /^lounge curtains$/i, 'a window row');
      t.tap(t.find(/^kitchen blind$/i));
      t.wait(400);
      t.has(t.texts(), /^venetian blind$/i, 'the kitchen blind'); t.has(t.texts(), /^slats$/i, 'the slats');
    } },
    { name: 'ha: the cover live; Open and a drag go out as cover services', async run(t) {
      t.ha.s[C1] = { '': 'open', friendly_name: 'Test study', current_position: '30', supported_features: '15' };
      t.ha.s[C2] = { '': 'closed', friendly_name: 'Test lounge', device_class: 'curtain', current_position: '0', supported_features: '15' };
      await open(t, { source: 'ha', entity: C1, covers: `${C1},${C2}` });
      t.has(t.texts(), /^test study$/i, 'the name'); t.has(t.texts(), /^30%$/, 'the value');
      t.tap(t.find(/^open$/i));
      t.ok(t.ha.calls.some((c) => c.service === 'cover.open_cover' && c.entity === C1), 'open_cover on the cover');
      const w = t.part('cover_visual');
      t.drag(null, { from: [w.x + w.w / 4, w.y + w.h / 3], by: [0, Math.round(w.h / 3)] });
      t.ok(t.ha.calls.some((c) => c.service === 'cover.set_cover_position' && c.entity === C1), 'set_cover_position from the drag');
    } },
    { name: 'ha: an unavailable cover says offline', async run(t) {
      t.ha.s[C1] = { '': 'unavailable' };
      await open(t, { source: 'ha', entity: C1 });
      t.has(t.texts(), /^offline$/i, 'offline'); t.has(t.texts(), /^(--|\u2014)$/, 'no value');
    } },
    { name: 'Home card (C52): the value and the window on the same feed', async run(t) {
      await t.device(deck({ source: 'sample' }));
      const c = t.part('card');
      t.ok(c, 'the page card'); t.ok(c.texts.some((x) => /^60%$/.test(x)), 'the value on the card');
      t.ok(c.texts.some((x) => /study blind · 3 of 3 open/i.test(x)), 'the line on the card');
      t.ok(c.texts.some((x) => /^blinds$/i.test(x)), 'the card says Blinds (docs home.ts)');
      t.ok(t.part('cover_visual'), 'the window on the card');
    } },
  ],
};
