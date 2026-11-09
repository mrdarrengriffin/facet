// Settings > Home Assistant (screens/ha_link.cpp): the screen is built again in its place when the link changes
// (hl::rev; its timer is its root's, fx::every), and leaves nothing behind when it goes.
const act = (t, a) => { const r = JSON.parse(t.b.spec('act', a) || '{}'); t.b.tick(40); t.b.settle(600); return r; };  // tick: the screen's 250 ms look at the link (settle stops when nothing moves)

export default {
  id: 'ha_link', family: 'device', about: 'The Home Assistant link screen follows the link: built again in place on a change.',
  tests: [
    { name: 'a link set up: the units from Home Assistant\'s country (GB: miles and mph; FR: km and km/h)', async run(t) {
      await t.device();
      t.ha.answers = true; t.ha.country = 'GB';
      act(t, { do: 'ha.connect', url: 'http://ha.test:8123', token: 'abcdefgh1234' }); t.b.tick(8); t.b.settle(600);
      t.eq([t.setting('distanceUnit'), t.setting('speedUnit')], ['mi', 'mph'], 'GB');
      t.ha.country = 'FR';
      act(t, { do: 'ha.connect', url: 'http://ha2.test:8123', token: 'abcdefgh5678' }); t.b.tick(8); t.b.settle(600);
      t.eq([t.setting('distanceUnit'), t.setting('speedUnit')], ['km', 'kmh'], 'FR');
    } },
    { name: 'a new address: the screen shows it, built again in its place, once', async run(t) {
      await t.mount('ha_link', {});
      const r = act(t, { do: 'ha.connect', url: 'http://ha.test:8123', token: 'abcdefgh1234' });
      t.ok(r.ok, `ha.connect: ${r.why}`);
      t.ok(t.texts().some((s) => s.includes('ha.test')), 'the new address shows');
      const one = (w) => t.texts().filter((s) => s === w).length;
      t.eq(one('Address'), 1, 'built again in place: one screen, not a second one');
      const n = t.objs().length;
      act(t, { do: 'ha.connect', url: 'http://ha.other:8123' });
      t.ok(t.texts().some((s) => s.includes('ha.other')), 'and again');
      t.eq([one('Address'), t.objs().length], [1, n], 'still one screen, the same tree');
    } },
    { name: 'Settings row: Not set up, then "Linked, <host>" before any entity read, Not reached when a read fails', async run(t) {
      await t.device();
      const row = () => { act(t, { do: 'nav.open', screen: 'settings' }); return t.texts().find((s) => /^(Not set up|Not reached|Linked, )/.test(s)); };
      t.eq(row(), 'Not set up', 'no link: Not set up');
      act(t, { do: 'nav.back' });
      act(t, { do: 'ha.connect', url: 'https://homeassistant.local:8123/', token: 'abcdefgh1234' });
      t.eq(row(), 'Linked, homeassistant.local', 'linked: the host, no scheme, port or path');
      act(t, { do: 'nav.back' });
      t.ha.answers = true, t.ha.down = true;
      act(t, { do: 'ha.entities', domains: 'light' }); t.ha.serve(); t.b.settle(600);
      t.eq(row(), 'Not reached', 'a failed read: Not reached');
    } },
      { name: 'the link check: linked, a refused token, an unreachable address (status, Settings row, the screen\'s words)', async run(t) {
      await t.device();
      const link = () => JSON.parse(t.b.m.UTF8ToString(t.b.m._fw_ha_link()));
      const row = () => { act(t, { do: 'nav.open', screen: 'settings' }); const r = t.texts().find((s) => /^(Not set up|Not reached|Linked, )/.test(s)); act(t, { do: 'nav.back' }); return r; };
      t.ha.answers = true;
      act(t, { do: 'ha.connect', url: 'http://ha.test:8123', token: 'abcdefgh1234' }); t.b.tick(4); t.b.settle(600);
      t.eq([link().status, link().why], ['idle', ''], 'a good token: checked, nothing wrong');
      t.eq(row(), 'Linked, ha.test');
      t.ha.refuse = true;
      act(t, { do: 'ha.connect', url: 'http://ha.test:8123', token: 'wrongtoken99' }); t.b.tick(4); t.b.settle(600);
      t.eq([link().status, link().why], ['fail', 'Home Assistant refused the token'], 'a refused token: said in words');
      t.eq(row(), 'Not reached');
      t.ok(!JSON.stringify(link()).includes('wrongtoken99'), 'the token is never shown back');
      t.eq(link().token, '\u2026en99', 'its last 4 characters only');
      t.ha.refuse = false, t.ha.down = true;
      act(t, { do: 'ha.connect', url: 'http://nowhere.test:8123' }); t.b.tick(4); t.b.settle(600);
      t.eq([link().status, link().why], ['fail', 'cannot reach Home Assistant at that address'], 'an unreachable address: said in words');
      act(t, { do: 'nav.open', screen: 'ha_link' });
      t.ok(t.texts().includes('Cannot reach Home Assistant at that address.'), `the screen says why, whole: ${t.texts().join(' / ')}`);
    } },
    { name: 'no "Entity lists / Read now": a picker reads its own domain on open, and again in the background when its list is old (a new entity shows up)', async run(t) {
      t.ha.answers = true;
      t.ha.s['weather.test_home'] = { '': 'sunny', friendly_name: 'Test home' };
      await t.device({ v: 2, rev: 9, start: 'h', pages: [
        { id: 'h', kind: 'home', name: 'Test', cards: [{ t: 'page', page: 'weather', size: 'm' }] }, { id: 'weather', kind: 'weather', params: { source: 'ha_weather' } }] });
      act(t, { do: 'ha.connect', url: 'http://ha.test:8123', token: 'abcdefgh1234' }); t.b.tick(8); t.b.settle(600);
      act(t, { do: 'nav.open', screen: 'ha_link' });
      t.hasNot(t.texts(), /^(Entity lists|Read now)$/, 'the screen: the link only, nothing to read by hand');
      const link = () => JSON.parse(t.b.m.UTF8ToString(t.b.m._fw_ha_link()));
      const picker = () => {
        act(t, { do: 'nav.open', screen: 'settings' });
        act(t, { do: 'nav.open', screen: 'screen_set', params: { id: 'weather' } });
        t.drag({ at: [t.b.S / 2, t.b.S * 0.8] }, { by: [0, -t.b.S / 2] }); t.b.settle(600);
        t.tap(/^Forecast$/);
        const first = t.texts();  // straight after the tap, before Home Assistant answers
        t.wait(800); t.b.settle(600);
        return { first, after: t.texts() };
      };
      let p = picker();
      t.has(p.after, /^Test home$/, 'read on open: the weather list');
      t.eq(link().status, 'ok', `read: ${JSON.stringify(link())}`);
      const reads = link().count;
      t.ha.s['weather.test_new'] = { '': 'rainy', friendly_name: 'New place' };
      p = picker();
      t.hasNot(p.after, /^New place$/, 'a fresh list: not read again');
      t.wait(5 * 60 * 1000 + 1000);
      p = picker();
      t.hasNot(p.first, /^Reading Home Assistant$/, 'an old list: shown at once, no Reading row');
      t.has(p.first, /^Test home$/, 'the old list meanwhile');
      t.has(p.after, /^New place$/, 'read again in the background: the new entity shows');
      t.eq(link().count, reads + 1, 'only the weather domain read');
    } },
    { name: 'the add list\'s search on a large house: only the matches of that type and device class, never the whole list', async run(t) {
      t.ha.answers = true;
      for (let i = 0; i < 1500; i++) {  // a large house: 1,500 sensors, a tenth of them temperatures
        const room = ['Kitchen', 'Hall', 'Office', 'Bedroom', 'Garage'][Math.floor(i / 10) % 5];
        t.ha.s[`sensor.s${i}`] = { '': i % 10 === 0 ? '21.5' : '1', unit_of_measurement: i % 10 === 0 ? '°C' : 'W', friendly_name: `${room} ${i % 10 === 0 ? 'Temperature' : 'Power'} ${i}`, device_class: i % 10 === 0 ? 'temperature' : 'power' };
      }
      t.ha.s['light.kitchen_ceiling'] = { '': 'on', friendly_name: 'Kitchen Ceiling' };
      await t.device();
      act(t, { do: 'ha.connect', url: 'http://ha.test:8123', token: 'abcdefgh1234' }); t.b.tick(8); t.b.settle(600);
      const find = (doms, dc, q) => {
        const m = t.b.m, a = m.stringToNewUTF8(doms), b = m.stringToNewUTF8(dc), c = m.stringToNewUTF8(q);
        try {
          let r;
          for (let i = 0; i < 20; i++) { r = JSON.parse(m.UTF8ToString(m._fw_ha_find(a, b, c))); if (r.state !== 'loading') break; t.b.tick(2); }
          return r;
        } finally { m._free(a), m._free(b), m._free(c); }
      };
      const link = () => JSON.parse(t.b.m.UTF8ToString(t.b.m._fw_ha_link()));
      let r = find('sensor,binary_sensor', 'temperature', 'kitchen');
      t.eq(r.state, 'ok', JSON.stringify(r).slice(0, 200));
      t.eq([r.found.length, r.more], [30, 0], 'the kitchen temperatures: 30, nothing else');
      t.ok(r.found.every((e) => e.dc === 'temperature' && /^Kitchen /.test(e.name)), 'each one a kitchen temperature');
      t.eq([r.found[0].st, r.found[0].unit], ['21.5', '°C'], 'with its value and unit, as Home Assistant has them');
      r = find('sensor', '', 'KITCHEN');
      t.eq([r.found.length, r.more], [40, 260], 'any kitchen sensor: a page of 40, the other 260 counted (case ignored)');
      r = find('light,sensor', '', 'ceiling');
      t.eq(r.found.map((e) => e.id), ['light.kitchen_ceiling'], 'by name across domains');
      r = find('sensor', '', 's1499');
      t.eq(r.found.map((e) => e.id), ['sensor.s1499'], 'by entity id');
      t.ok(link().peak < 16 * 1024, `held while reading: ${link().peak} bytes (a page of matches, never the list)`);
      t.eq(link().count, 0, 'the pickers\' lists untouched (nothing cached)');
    } },
    { name: 'the link page as a QR code while the panel is not linked, gone once it is', async run(t) {
      t.ha.answers = true;
      await t.mount('ha_link', { page: 'http://192.168.1.42/link' });
      const qr = () => t.texts().some((s) => /^open on your phone$/i.test(s));
      t.ok(qr() && t.texts().includes('http://192.168.1.42/link'), 'not linked: the link page, its address and its code');
      act(t, { do: 'ha.connect', url: 'http://ha.test:8123', token: 'abcdefgh1234' }); t.b.tick(4); t.b.settle(600);
      t.ok(!qr(), 'linked: no code');
      t.ha.refuse = true;
      act(t, { do: 'ha.connect', url: 'http://ha.test:8123', token: 'wrongtoken99' }); t.b.tick(4); t.b.settle(600);
      t.ok(qr(), 'the check failed: the code again');
    } },
    { name: 'Reset pairing: hold clears the key Home Assistant set (encryption key None, a toast); no button without one', async run(t) {
      await t.mount('ha_link', { pairing: 'paired' });
      t.has(t.texts(), 'Set by Home Assistant', 'the key Home Assistant set');
      t.hold(/^Reset pairing$/i, 1600); t.b.tick(40); t.b.settle(600);
      t.has(t.texts(), 'None', 'cleared: no key');
      t.hasNot(t.texts(), /^Reset pairing$/i, 'nothing left to reset');
      t.eq(t.texts().filter((s) => s === 'Encryption key').length, 1, 'built again in place, once');
      await t.mount('ha_link', { pairing: 'fixed' });
      t.has(t.texts(), 'Built in', 'a key in the firmware');
      t.hasNot(t.texts(), /^Reset pairing$/i, 'a built-in key: no reset');
      await t.mount('ha_link', { pairing: 'none' });
      t.hasNot(t.texts(), 'Encryption key', 'no pairing on this build: no group');
    } },
  ],
};
