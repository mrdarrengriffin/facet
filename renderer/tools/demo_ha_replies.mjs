// Demo Home Assistant replies, shared by the functional tests' fake (tools/functional.mjs FakeHA) and the docs site's
// previews (site/src/lib/sample-ha.ts): the HA actions a screen wants the reply of (weather.get_forecasts,
// calendar.get_events) and the panel's own link's WebSocket reads (Energy's dashboard). Made-up data, not a real home.
// Plain JS, no imports: node and the site's bundler both load it.
const H = 3600000;
const iso = (t) => new Date(t).toISOString();

/** the reply to an HA action for entity e (a weather forecast, a calendar's events), else null */
export function demoReply(service, data, now = Date.now()) {
  const e = [].concat(data.entity_id || [])[0];
  if (!e) return null;
  if (service === 'weather.get_forecasts') {
    const daily = data.type === 'daily';
    const forecast = Array.from({ length: daily ? 7 : 24 }, (_, i) => ({
      datetime: iso(now + (daily ? i * 24 : i + 1) * H), condition: ['cloudy', 'rainy', 'partlycloudy', 'sunny'][i % 4],
      temperature: 12 - (i % 5), templow: 5 + (i % 3), precipitation_probability: (i * 13) % 90, precipitation: (i % 3) * 0.4 }));
    return { [e]: { forecast } };
  }
  if (service === 'calendar.get_events') {
    const ev = (h, d, summary, location) => ({ start: iso(now + h * H), end: iso(now + (h + d) * H), summary, location });
    return { [e]: { events: [ev(2, 1, 'Dentist', 'High Street surgery'), ev(26, 2, 'Dinner with Sam', 'The Old Bank'), ev(50, 1, 'Swimming lesson', 'Leisure centre')] } };
  }
  return null;
}

/** the grid's statistic ids the demo Energy dashboard names (prefix: the tests' 'test_') */
export const demoGrid = (p = '') => ({ in: `sensor.${p}grid_in`, cost: `sensor.${p}grid_cost`, price: `sensor.${p}grid_price`,
  power: `sensor.${p}grid_power`, device: `sensor.${p}kettle` });

/** HA's result message for a WebSocket read (Energy's dashboard), else null; empty = an Energy dashboard not set up */
export function demoWs(cmd, { empty = false, prefix = '', now = Date.now() } = {}) {
  const ok = (result) => ({ id: 1, type: 'result', success: true, result });
  const g = demoGrid(prefix);
  if (cmd.type === 'energy/get_prefs')
    return ok(empty ? { energy_sources: [], device_consumption: [] } : {
      energy_sources: [{ type: 'grid', stat_energy_from: g.in, stat_energy_to: null, stat_cost: g.cost, entity_energy_price: g.price, stat_rate: g.power }],
      device_consumption: [{ stat_consumption: g.device }] });
  if (cmd.type === 'recorder/statistics_during_period') {
    const step = cmd.period === 'day' ? 24 * H : cmd.period === 'hour' ? H : 300000;
    const midnight = new Date(now); midnight.setHours(0, 0, 0, 0);
    // a lighter yesterday: today reads "more than yesterday by now" whatever the minute
    const rows = (k) => { const out = []; for (let t = Date.parse(cmd.start_time); t < now; t += step) out.push({ start: t, end: t + step, change: +((t < +midnight ? 0.7 : 1) * k * step / 300000).toFixed(4) }); return out; };
    const r = {};
    for (const id of cmd.statistic_ids || []) r[id] = rows(id.endsWith('_cost') ? 0.011 : 0.04);
    return ok(r);
  }
  if (cmd.type === 'recorder/list_statistic_ids') return ok([]);
  return null;
}
