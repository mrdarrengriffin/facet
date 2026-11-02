// Settings > Home Assistant (screens/ha_link.cpp): the screen is built again in its place when the link changes
// (hl::rev; its timer is its root's, fx::every), and leaves nothing behind when it goes.
const act = (t, a) => { const r = JSON.parse(t.b.spec('act', a) || '{}'); t.b.tick(40); t.b.settle(600); return r; };  // tick: the screen's 250 ms look at the link (settle stops when nothing moves)

export default {
  id: 'ha_link', family: 'device', about: 'The Home Assistant link screen follows the link: built again in place on a change.',
  tests: [
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
  ],
};
