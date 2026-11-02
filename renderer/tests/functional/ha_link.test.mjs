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
  ],
};
