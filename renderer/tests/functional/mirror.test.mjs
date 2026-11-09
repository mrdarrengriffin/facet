// The editor a panel serves runs this renderer in the browser in mirror mode (core/ha.h mirror, fw_ha_mirror): the
// panel's own sources (panel.*: the feeds, the timers) come from the panel like Home Assistant's values and are never
// made here, so the editor shows what the glass shows and nothing is fetched twice (#180).
const asks = (t) => JSON.parse(t.b.m.UTF8ToString(t.b.m._fw_ha_asks()) || '[]');
const wants = (t) => JSON.parse(t.b.m.UTF8ToString(t.b.m._fw_ha_wants()) || '[]');

export default {
  id: 'mirror', family: 'device', about: 'Mirror mode: the panel.* feeds come from the host, never fetched in the module.',
  tests: [
    { name: 'mirrored: the Weather screen asks for panel.weather, fetches nothing, and shows the values it is given', async run(t) {
      await t.mount('spec', { screen: 'clock' });  // the module first
      t.b.m._fw_ha_mirror(1);
      try {
        t.ha.s['weather.test_home'] = { '': 'rainy', temperature: '9.6' };
        t.b.rebuild('spec', { screen: 'weather', source: 'ha', weather: 'weather.test_home' });  // the same module: mirrored
        t.b.settle(1500); t.ha.push(); t.b.settle(1500);
        t.ok(wants(t).some(([e]) => e === 'panel.weather'), `asks the host for panel.weather: ${JSON.stringify(wants(t)).slice(0, 200)}`);
        t.eq(asks(t).length, 0, 'no forecast asks of its own');
        t.hasNot(t.texts(), /^10°$/, 'nothing of its own made from the entity');
        t.b.haPut('panel.weather', 'temp', '23°');
        t.b.haPut('panel.weather', 'condition', 'Sunny');
        t.b.haPut('panel.weather', '', 'ok');
        t.b.settle(1500);
        t.has(t.texts(), /^23°$/, `the panel's temperature: ${t.texts().slice(0, 12).join(' / ')}`);
        t.has(t.texts(), /^Sunny$/, "the panel's condition");
      } finally { t.b.m._fw_ha_mirror(0); }
    } },
    { name: 'not mirrored (the workbench, the panel itself): the feed runs as before and never asks the host for panel.weather', async run(t) {
      await t.mount('spec', { screen: 'weather', source: 'sample' });
      t.b.settle(1500);
      t.ok(!wants(t).some(([e]) => e.startsWith('panel.')), 'panel.* stays local');
      t.ok(t.texts().some((x) => /^-?\d+°$/.test(x)), 'its own sample temperature');
    } },
  ],
};
