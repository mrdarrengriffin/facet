// Weather screen (specs/weather.json + weather_days.json: Page > Header > Body: the weather now (Weather icon, hero), the
// rain line (Status line), the next eight hours (Grid of Weather icon columns), "next 5 days" (More row) > the 5 days
// (Grid of Weather icon rows)), on the weather feed panel.weather (core/weather_source.cpp, CONTRACT.md C51) and its
// Home card (C52). Home Assistant is faked (tools/functional.mjs FakeHA); W is NOT a real entity. The forecast asks
// (weather.get_forecasts) are answered here, as a host or the panel's API would.
// Approved original: Astro packages/ui/src/screens/weather.ts (Weather view + daysView).
const W = 'weather.test_home';
const asks = (t) => JSON.parse(t.b.m.UTF8ToString(t.b.m._fw_ha_asks()) || '[]');
const answer = (t, id, obj) => { t.b.str(JSON.stringify(obj), (p) => t.b.m._fw_ha_answer(id, 1, p)); t.b.settle(1500); };
const hourly = (t0, n, wetFrom = 99) => Array.from({ length: n }, (_, i) => ({
  datetime: new Date(t0 + i * 3600e3).toISOString(), condition: i >= wetFrom ? 'rainy' : 'cloudy', temperature: 10 + i, precipitation: i >= wetFrom ? 0.8 : 0 }));
const wxComps = (t) => t.comps().filter((c) => c.ids.includes('weather_icon') && t.visible(c));
async function open(t, params) { await t.mount('spec', { screen: 'weather', ...params }); return t; }
// Open-Meteo on the fake web (FakeWeb): a recorded reply (api.open-meteo.com/v1/forecast, timeformat=unixtime), its times
// moved to now; codes 3 overcast, 0 clear, 51 drizzle, 63 rain, 45 fog
const OM = /^https:\/\/api\.open-meteo\.com\/v1\/forecast\?/;
const omReply = ({ code = 3, day = 1, temp = 9.0, feels = 5.3, wetFrom = 9 } = {}) => {
  const h0 = Math.floor(Date.now() / 3600e3) * 3600, d0 = Math.floor(Date.now() / 86400e3) * 86400;
  return {
    latitude: 52.19465, longitude: -1.209259, utc_offset_seconds: 0, timezone: 'GMT',
    current: { time: h0, interval: 900, temperature_2m: temp, apparent_temperature: feels, weather_code: code, is_day: day },
    hourly: { time: Array.from({ length: 13 }, (_, i) => h0 + i * 3600), temperature_2m: [9.2, 8.8, 8.2, 7.9, 7.6, 7.3, 7.4, 7.6, 7.7, 8.1, 7.8, 7.6, 8.0],
      precipitation: Array.from({ length: 13 }, (_, i) => (i >= wetFrom && i < wetFrom + 2 ? 0.4 : 0)), weather_code: [3, 3, 3, 0, 0, 0, 1, 3, 3, 51, 51, 3, 3] },
    daily: { time: Array.from({ length: 6 }, (_, i) => d0 + i * 86400), weather_code: [63, 51, 63, 51, 45, 61],
      temperature_2m_max: [10.2, 9.8, 7.8, 7.1, 6.7, 8.6], temperature_2m_min: [2.5, 5.8, 5.5, -1.5, -3.3, 4.6], precipitation_sum: [19.8, 0.7, 8.25, 0.1, 0.0, 8.0] },
  };
};
const home = (t) => { t.ha.s['zone.home'] = { '': 'zoning', latitude: '52.2', longitude: '-1.2' }; };
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
    { name: 'ha: a failed forecast ask (a boot before Home Assistant is linked) is asked again a minute on', async run(t) {
      t.ha.s[W] = { '': 'cloudy', temperature: '11' };
      await open(t, { source: 'ha', weather: W });
      const first = asks(t).filter((a) => a.service === 'weather.get_forecasts');
      t.eq(first.length >= 2, true, 'hourly + daily asked');
      for (const a of first) t.b.str('', (p) => t.b.m._fw_ha_answer(a.id, 0, p));
      t.b.settle(800);
      t.hasNot(t.texts(), /^now$/i, 'no hours after the failure');
      t.wait(61000); t.b.settle(1500);
      const again = asks(t).filter((a) => a.service === 'weather.get_forecasts' && !first.some((f) => f.id === a.id));
      t.eq(again.map((a) => a.data.type).sort(), ['daily', 'hourly'], 'asked again within a minute, not at the next hour');
      const now = Math.floor(Date.now() / 3600e3) * 3600e3;
      answer(t, again.find((a) => a.data.type === 'hourly').id, { [W]: { forecast: hourly(now, 12) } });
      t.has(t.texts(), /^now$/i, 'the hours once it answers');
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
    { name: 'ha: unavailable shows Unavailable and --°, no hours or rain line (Not found: missing_entities)', async run(t) {
      t.ha.s[W] = { '': 'unavailable' };
      await open(t, { source: 'ha', weather: W });
      t.has(t.texts(), /^Unavailable$/, 'unavailable'); t.hasNot(t.texts(), /^(offline|not found)$/i, 'not the other words'); t.has(t.texts(), /^--°$/, 'no temperature');
      t.hasNot(t.texts(), /next 5 days|dry for|rain from/i, 'no rain line or more row');
    } },
    { name: '5 days: next 5 days opens five day rows on the same feed (Today first)', async run(t) {
      await t.device(deck({ source: 'sample' }));
      t.show('w');
      t.tap(t.find(/next 5 days/i));
      t.eq(t.where(), 'weather_days', 'the days screen');
      t.has(t.texts(), /^today$/i, 'Today'); t.eq(wxComps(t).length, 5, 'five rows');
    } },
    { name: '5 days: the weather over the rain, each one line, whole at both sizes (Thunderstorms · 6.8 mm)', async run(t) {
      await t.mount('spec', { screen: 'weather_days', source: 'sample' });
      const o = t.objs().find((x) => /^thunderstorms/i.test(x.text || '')), r = t.objs().find((x) => x.text === '6.8 mm'), one = t.b.S > 600 ? 32 : 20;
      t.ok(o && r, 'the condition and the rain, each its own line'); t.eq([o.h, r.h], [one, one], 'one body line tall each');
      t.eq(o.text, 'Thunderstorms', 'whole, no ellipsis'); t.ok(r.y >= o.y + o.h, 'the rain under the weather');
    } },
    { name: 'open_meteo: the panel reads Open-Meteo for Home Assistant\'s home: the weather now, eight hours, 5 days', async run(t) {
      home(t); t.web.serve(OM, omReply());
      await open(t, { source: 'open_meteo' });
      const u = t.web.asked.filter((x) => OM.test(x));
      t.eq(u.length, 1, 'one read');
      t.ok(/latitude=52\.20&longitude=-1\.20&/.test(u[0]), `home, 2 decimals (${u[0]})`);
      t.ok(/timeformat=unixtime/.test(u[0]) && /forecast_hours=13/.test(u[0]) && /forecast_days=6/.test(u[0]), 'only what the screen shows');
      t.has(t.texts(), /^9°$/, 'the temperature now'); t.has(t.texts(), /^cloudy$/i, 'WMO 3 in words'); t.has(t.texts(), /feels 5°/i, 'feels like');
      t.has(t.texts(), /high 10°, low 3°/i, 'today\'s high and low'); t.has(t.texts(), /^now$/i, 'the hours');
      t.has(t.texts(), /dry for the next hour · rain at/i, 'the next wet hour');
      t.eq(wxComps(t).length, 9, 'the hero icon + eight hour columns');
      await t.mount('spec', { screen: 'weather_days', source: 'open_meteo' });
      t.has(t.texts(), /^today$/i, 'Today'); t.has(t.texts(), /^rain · 19\.8 mm$|^rain$/i, 'WMO 63 in words'); t.has(t.texts(), /^fog$/i, 'WMO 45 in words');
    } },
    { name: 'open_meteo: a read every 15 minutes; a failure keeps the forecast and backs off; no home = Offline', async run(t) {
      home(t); t.web.serve(OM, omReply({ code: 0, day: 0, temp: 4.4 }));
      await open(t, { source: 'open_meteo', lat: '51.5', lon: '-0.12' });
      const n = () => t.web.asked.filter((x) => OM.test(x)).length;
      t.ok(/latitude=51\.50&longitude=-0\.12&/.test(t.web.asked.filter((x) => OM.test(x))[0]), 'the place set here wins over zone.home');
      t.has(t.texts(), /^clear$/i, 'clear at night: Clear'); t.has(t.texts(), /^4°$/, 'the temperature');
      t.wait(10 * 60000); t.eq(n(), 1, 'no read before 15 minutes');
      t.web.status(OM, 'timeout'); t.wait(5 * 60000 + 1000);
      t.eq(n(), 2, 'read again at 15 minutes'); t.has(t.texts(), /^4°$/, 'a failed read keeps the forecast');
      t.wait(61000); t.eq(n(), 3, 'after a failure: a minute on');
      t.wait(61000); t.eq(n(), 3, 'then two minutes'); t.wait(61000); t.eq(n(), 4, 'two minutes on');
      t.web.serve(OM, omReply({ code: 63, temp: 7.6 })); t.wait(4 * 60000 + 1000);
      t.has(t.texts(), /^8°$/, 'a good read: the new weather'); t.has(t.texts(), /^rain$/i, 'WMO 63');
      t.ha.s['zone.home'] = { '': 'zoning', latitude: '', longitude: '' };
      await open(t, { source: 'open_meteo' });
      t.has(t.texts(), /^offline$/i, 'no home location: Offline');
    } },
    { name: 'open_meteo: an unreadable or failed first read = Offline, not a made-up forecast', async run(t) {
      home(t); t.web.status(OM, 'timeout');
      await open(t, { source: 'open_meteo' });
      t.has(t.texts(), /^offline$/i, 'Offline'); t.has(t.texts(), /^--°$/, 'no temperature'); t.hasNot(t.texts(), /^now$/i, 'no hours');
    } },
    { name: 'auto (the default): Home Assistant\'s forecast when it has the weather entity, no Open-Meteo read', async run(t) {
      home(t); t.web.serve(OM, omReply()); t.ha.s[W] = { '': 'rainy', temperature: '9.6' };
      await open(t, { weather: W });
      t.has(t.texts(), /^10°$/, 'the entity\'s temperature'); t.has(t.texts(), /^rain$/i, 'its condition');
      t.ok(asks(t).some((a) => a.service === 'weather.get_forecasts'), 'its forecast asked');
      t.wait(61000);
      t.eq(t.web.asked.filter((x) => OM.test(x)).length, 0, 'Open-Meteo never read');
    } },
    { name: 'auto (the default): no weather entity in Home Assistant = Open-Meteo; the entity coming back = Home Assistant again', async run(t) {
      home(t); t.web.serve(OM, omReply({ temp: 4.4 }));
      await open(t, {});
      t.wait(61000);
      t.ok(t.web.asked.some((x) => OM.test(x)), 'Open-Meteo read once Home Assistant has said it has none');
      t.has(t.texts(), /^4°$/, 'Open-Meteo\'s weather');
      t.ha.set('weather.forecast_home', 'sunny', { temperature: '21.4' }); t.wait(61000);
      t.has(t.texts(), /^21°$/, 'the entity, once Home Assistant has it'); t.has(t.texts(), /^sunny$/i, 'its condition');
    } },
    { name: 'Home card (C52): the weather now on the same feed, the real Weather icon as its preview', async run(t) {
      await t.device(deck({ source: 'sample' }));
      const c = t.part('card');
      t.ok(c, 'the page card'); t.ok(c.texts.some((x) => /^-?\d+°$/.test(x)), 'the temperature');
      t.ok(wxComps(t).some((w) => w.x >= c.x && w.y >= c.y && w.x + w.w <= c.x + c.w), 'a Weather icon inside the card');
    } },
  ],
};
