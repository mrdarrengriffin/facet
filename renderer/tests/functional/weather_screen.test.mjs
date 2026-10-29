// Weather screen (specs/weather.json + weather_days.json: Page > Header > Body: the weather now (Weather icon, hero), the
// rain line (Status line), the next eight hours (Grid of Weather icon columns), "next 5 days" (More row) > the 5 days
// (Grid of Weather icon rows)), on the weather feed panel.weather (core/weather_source.cpp, CONTRACT.md C51) and its
// Home card (C52). Home Assistant is faked (tools/functional.mjs FakeHA); W is NOT Darren's entity. The forecast asks
// (weather.get_forecasts) are answered here, as the hub / the panel's API would.
// Approved original: Astro packages/ui/src/screens/weather.ts (Weather view + daysView).
const W = 'weather.test_home';
const asks = (t) => JSON.parse(t.b.m.UTF8ToString(t.b.m._fw_ha_asks()) || '[]');
const answer = (t, id, obj) => { t.b.str(JSON.stringify(obj), (p) => t.b.m._fw_ha_answer(id, 1, p)); t.b.settle(1500); };
const hourly = (t0, n, wetFrom = 99) => Array.from({ length: n }, (_, i) => ({
  datetime: new Date(t0 + i * 3600e3).toISOString(), condition: i >= wetFrom ? 'rainy' : 'cloudy', temperature: 10 + i, precipitation: i >= wetFrom ? 0.8 : 0 }));
const wxComps = (t) => t.comps().filter((c) => c.ids.includes('weather_icon') && t.visible(c));
async function open(t, params) { await t.mount('spec', { screen: 'weather', ...params }); return t; }
const deck = (params) => ({ v: 2, rev: 9, start: 'h', pages: [
  { id: 'h', kind: 'home', name: 'Test', cards: [{ t: 'page', page: 'w', size: 'l' }] },
  { id: 'w', kind: 'weather', params }] });

export default {
  id: 'weather_screen', family: 'screens', about: 'The Weather screen on the weather feed: sample + HA adapters, asks, states, 5 days, Home card.',
  tests: [
    { name: 'sample: the weather now, the rain line, eight hours (now first) and next 5 days', async run(t) {
      await open(t, { source: 'sample' });
      const tx = t.texts();
      t.ok(tx.some((x) => /^-?\d+°$/.test(x)), 'a temperature'); t.has(tx, /^now$/i, 'the hour now');
      t.has(tx, /^(rain from|dry for the next hour)/i, 'the rain line'); t.has(tx, /next 5 days/i, 'the More row');
      t.eq(wxComps(t).length, 9, 'the hero icon + eight hour columns');
    } },
    { name: 'ha: the weather now is live from the entity; the forecast comes through two asks', async run(t) {
      t.ha.s[W] = { '': 'rainy', temperature: '9.6', apparent_temperature: '7.2' };
      await open(t, { source: 'ha', weather: W });
      t.has(t.texts(), /^10°$/, 'temperature (whole degrees)'); t.has(t.texts(), /^rain$/i, 'condition in words'); t.has(t.texts(), /feels 7°/i, 'feels like');
      t.hasNot(t.texts(), /^now$/i, 'no hours before the forecast');
      const q = asks(t);
      t.eq(q.map((a) => `${a.service} ${a.data.entity_id} ${a.data.type}`).sort(), [`weather.get_forecasts ${W} daily`, `weather.get_forecasts ${W} hourly`], 'hourly + daily asked');
      const now = Math.floor(Date.now() / 3600e3) * 3600e3, h = q.find((a) => a.data.type === 'hourly'), d = q.find((a) => a.data.type === 'daily');
      answer(t, h.id, { [W]: { forecast: hourly(now, 12, 3) } });
      answer(t, d.id, { [W]: { forecast: [0, 1, 2, 3, 4].map((i) => ({ datetime: new Date(now + i * 86400e3).toISOString(), condition: 'cloudy', temperature: 15 + i, templow: 5 + i, precipitation: 0 })) } });
      t.has(t.texts(), /^now$/i, 'hours after the reply'); t.has(t.texts(), /dry for the next hour · rain at/i, 'the next wet hour');
      t.has(t.texts(), /high 15°, low 5°/i, 'high / low from the daily forecast');
      t.ha.set(W, 'sunny', { temperature: '21.4' });
      t.has(t.texts(), /^21°$/, 'a new temperature in place'); t.has(t.texts(), /^sunny$/i, 'a new condition in place');
    } },
    { name: 'ha: no apparent temperature (Met.no) = no Feels part, never Feels 0°', async run(t) {
      t.ha.s[W] = { '': 'cloudy', temperature: '10.2' };
      await open(t, { source: 'ha', weather: W });
      t.has(t.texts(), /^10°$/, 'temperature'); t.hasNot(t.texts(), /feels/i, 'no Feels without the attribute');
    } },
    { name: 'layout: the hero starts at the top of the body; the rain line, the hours and next 5 days keep the original rhythm', async run(t) {
      await open(t, { source: 'sample' });
      const k = t.b.S > 600 ? { top: 112, line: 12, strip: 28, bottom: 688 } : { top: 72, line: 8, strip: 20, bottom: 460 };
      const os = t.objs().filter((o) => t.visible(o));
      const hero = os.find((o) => !o.text && o.w === o.h && o.w === (t.b.S > 600 ? 144 : 96));
      t.eq(hero.y, k.top, 'the hero icon on the body top');
      const feels = os.find((o) => /^feels/i.test(o.text || '')), rain = os.find((o) => /^(dry|rain from)/i.test(o.text || '')), now = os.find((o) => /^now$/i.test(o.text || ''));
      t.eq(rain.y - (feels.y + feels.h), k.line, 'space:3 above the rain line');
      const col = t.comps().filter((c) => c.ids.includes('weather_icon') && c.w < 100).sort((a, z) => a.y - z.y || a.x - z.x)[0];
      t.eq(col.y - (rain.y + rain.h), k.strip, 'space:8 above the hours');
      const more = t.part('more_row'); t.eq(more.y + more.h, k.bottom, 'next 5 days on the body foot');
    } },
    { name: 'ha: unavailable shows Offline and --°, no hours or rain line', async run(t) {
      t.ha.s[W] = { '': 'unavailable' };
      await open(t, { source: 'ha', weather: W });
      t.has(t.texts(), /^offline$/i, 'offline'); t.has(t.texts(), /^--°$/, 'no temperature');
      t.hasNot(t.texts(), /next 5 days|dry for|rain from/i, 'no rain line or more row');
    } },
    { name: '5 days: next 5 days opens five day rows on the same feed (Today first)', async run(t) {
      await t.device(deck({ source: 'sample' }));
      t.show('w');
      t.tap(t.find(/next 5 days/i));
      t.eq(t.where(), 'weather_days', 'the days screen');
      t.has(t.texts(), /^today$/i, 'Today'); t.eq(wxComps(t).length, 5, 'five rows');
    } },
    { name: '5 days: a condition too long for its room ends in a real ellipsis on one line (never wrapped into a hidden line)', async run(t) {
      await t.mount('spec', { screen: 'weather_days', source: 'sample' });
      const o = t.objs().find((x) => /^thunderstorms/i.test(x.text || '')), one = t.b.S > 600 ? 32 : 20;
      t.ok(o, 'the long condition'); t.eq(o.h, one, 'one body line tall');
      t.ok(/\.\.\.$|…$/.test(o.text) || /mm$/.test(o.text), 'ends in an ellipsis (or fits whole)');
    } },
    { name: 'Home card (C52): the weather now on the same feed, the real Weather icon as its preview', async run(t) {
      await t.device(deck({ source: 'sample' }));
      const c = t.part('card');
      t.ok(c, 'the page card'); t.ok(c.texts.some((x) => /^-?\d+°$/.test(x)), 'the temperature');
      t.ok(wxComps(t).some((w) => w.x >= c.x && w.y >= c.y && w.x + w.w <= c.x + c.w), 'a Weather icon inside the card');
    } },
  ],
};
