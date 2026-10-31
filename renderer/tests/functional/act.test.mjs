// One action from outside a spec (CONTRACT.md C50, core/spec.h act(): what POST /fw/act and HA's facet_act run):
// checked like a spec node's action (same limits), its value against ACTION_DOCS, run through the same path as a tap.
// Also the C50 check: every action of the closed list has a documented row + example that act() runs.
const tm = (t, q) => JSON.parse(t.b.str('timer', (c) => t.b.str(JSON.stringify(q), (a) => t.b.m.UTF8ToString(t.b.m._fw_test(c, a)))));
const act = (t, a) => {
  const r = JSON.parse(t.b.spec('act', a) || '{}');
  t.b.settle(500); const evs = t.b.events(); t.ha.react(evs); t.b.settle(500);
  t.log.push(`act ${typeof a === 'string' ? a : JSON.stringify(a)}: ${r.ok ? 'ok' : r.why}`);
  return r;
};
const PAGE = {
  v: 1, id: 'act_p', name: 'Act page', rev: 1, role: 'page', header: { label: 'Act page' }, layout: 'list',
  nodes: [{ c: 'label', p: { text: 'ACT PAGE' } }],
};
export default {
  id: 'act', family: 'layout', about: 'fw::spec::act: one action from an agent or HA, the same checks and path as a spec tap.',
  tests: [
    { name: 'a timer started via act shows up in panel.timer', async run(t) {
      await t.device(); tm(t, { op: 'reset' });
      const r = act(t, { do: 'timer.start', seconds: 121, name: 'Tea' });
      t.ok(r.ok, `started: ${r.why}`);
      t.eq(r.state?.timers?.map((x) => [x.name, x.total, x.state]), [['Tea', 121, 'running']], 'state = the timers');
      const l = tm(t, { op: 'list' });
      t.eq([l.store.name, l.store.total, l.store.count], ['Tea', '121', '1'], 'panel.timer binds');
      t.ok(act(t, { do: 'timer.extend', name: 'tea', s: 60 }).ok, 'extend by name (any case)');
      t.eq(tm(t, { op: 'list' }).store.total, '181', 'extended');
      t.ok(act(t, { do: 'timer.cancel', name: 'Tea' }).ok, 'cancel');
      t.eq(tm(t, { op: 'list' }).timers.length, 0, 'gone');
    } },
    { name: 'timer limits: 1 s..24 h, 4 at most, a named timer must exist', async run(t) {
      await t.device(); tm(t, { op: 'reset' });
      for (const s of [0, 86401, 'abc', 1.5]) t.ok(/1 to 86400/.test(act(t, { do: 'timer.start', seconds: s }).why || ''), `seconds ${s} refused`);
      t.ok(/1 to 86400/.test(act(t, { do: 'timer.extend', s: 0 }).why || ''), 'extend 0 refused');
      t.ok(act(t, { do: 'timer.start', seconds: 1 }).ok && act(t, { do: 'timer.start', seconds: 86400 }).ok, '1 s and 24 h start');
      act(t, { do: 'timer.start', seconds: 60 }); act(t, { do: 'timer.start', seconds: 90 });
      const r = act(t, { do: 'timer.start', seconds: 30 });
      t.ok(!r.ok && /4 timers/.test(r.why), `a 5th refused: ${r.why}`);
      t.hasNot(t.texts(), /No timer started/, 'act answers why: no toast for the agent');
      t.ok(/no timer by that name/.test(act(t, { do: 'timer.cancel', name: 'Nope' }).why || ''), 'unknown name refused');
      t.ok(/not done/.test(act(t, { do: 'timer.stop', name: '1 min' }).why || ''), 'stop a running one refused');
      t.ok(/at most 24/.test(act(t, { do: 'timer.start', seconds: 60, name: 'x'.repeat(25) }).why || ''), 'a long name refused');
      tm(t, { op: 'reset' });
    } },
    { name: 'bad actions are refused with a reason', async run(t) {
      await t.device();
      const why = (a) => act(t, a).why || '';
      t.ok(/unknown action/.test(why({ do: 'light.explode', e: 'light.x' })), 'not on the list');
      t.ok(/object/.test(why('[1]')), 'not an object');
      t.ok(/not JSON/.test(why('{nope')), 'not JSON');
      t.ok(/unknown key/.test(why({ do: 'nav.back', wobble: 1 })), 'unknown key');
      t.ok(/spec screen/.test(why({ do: 'screen.param', param: 'speaker' })), 'screen.param needs a screen');
      t.ok(/spec screen/.test(why({ do: 'screen.toggle', param: 'rooms' })), 'screen.toggle needs a screen');
      t.ok(/screen parameters/.test(why({ do: 'light.toggle', e: '$light' })), '$param entity refused');
      t.ok(/light entity/.test(why({ do: 'light.toggle', e: 'switch.x' })), 'wrong domain');
      t.ok(/value: one of on\|off/.test(why({ do: 'light.switch', e: 'light.x', value: 'maybe' })), 'bad value');
      t.ok(/value/.test(why({ do: 'light.switch', e: 'light.x' })), 'missing value');
      t.ok(/value/.test(why({ do: 'light.brightness', e: 'light.x', value: 101 })), 'brightness over 100');
      t.ok(/takes no value/.test(why({ do: 'nav.back', value: 'x' })), 'a value where none goes');
      t.ok(/value: one of/.test(why({ do: 'media.control', e: 'media_player.k', value: 'volume 200' })), 'volume over 100');
      t.ok(/screen id/.test(why({ do: 'nav.open' })), 'nav.open needs a screen');
      t.ok(/no such screen/.test(why({ do: 'nav.open', screen: 'nosuch' })), 'unknown screen');
      t.ok(/seconds \/ s go on timer/.test(why({ do: 'light.toggle', e: 'light.x', seconds: 3 })), 'timer fields off timers');
    } },
    { name: 'HA actions take the same path as a tap (the fake HA sees the calls)', async run(t) {
      await t.device('house');  // needs a light on Home
      t.ok(act(t, { do: 'light.switch', e: 'light.office', value: 'on' }).ok, 'light on');
      t.call({ service: 'light.turn_on', entity: 'light.office' });
      t.ok(act(t, { do: 'light.brightness', e: 'light.office', value: 40 }).ok, 'brightness (a number value)');
      t.ok(t.ha.calls.some((c) => c.service === 'light.turn_on' && c.data?.brightness_pct === 40), 'brightness_pct 40');
      t.ok(act(t, { do: 'media.control', e: 'media_player.kitchen', value: 'volume 30' }).ok, 'volume');
      t.ok(t.ha.calls.some((c) => c.service === 'media_player.volume_set' && Math.abs(c.data?.volume_level - 0.3) < 1e-6), 'volume_level 0.3');
      t.ok(act(t, { do: 'cover.control', e: 'cover.office_blind', value: 'position 50' }).ok, 'cover');
      t.call({ service: 'cover.set_cover_position', entity: 'cover.office_blind' });
      t.ok(act(t, { do: 'vacuum.control', e: 'vacuum.robot', value: 'clean', rooms: '1,3' }).ok, 'room clean');
      t.ok(t.ha.calls.some((c) => c.service === 'vacuum.send_command' && JSON.stringify(c.data).includes('[1,3]')), 'segments 1,3');
      const dead = t.ha.wants().map((w) => w[0]).find((e) => e.startsWith('light.'));  // a light Home shows (in the store)
      t.ha.unavailable(dead);
      const r = act(t, { do: 'light.toggle', e: dead });
      t.ok(!r.ok && /unavailable/.test(r.why), `unavailable refused: ${r.why}`);
    } },
    { name: 'nav.open / nav.back from act move the panel', async run(t) {
      await t.device(); t.send(PAGE);
      t.ok(act(t, { do: 'nav.open', screen: 'act_p' }).ok, 'open');
      t.eq(t.where(), 'act_p', 'opened');
      t.ok(act(t, { do: 'nav.back' }).ok, 'back');
      t.ok(t.where() !== 'act_p', `back: ${t.where()}`);
    } },
    { name: 'C50: every action on the list has a documented row and example that act() runs', async run(t) {
      await t.device(); tm(t, { op: 'reset' });
      const cat = JSON.parse(t.b.m.UTF8ToString(t.b.m._fw_catalogue()));
      const list = JSON.parse(t.b.spec('actions', '')).actions;
      t.eq(list.map((x) => x.do), cat.actions, 'one row per ACTIONS entry, same order');
      t.eq(cat.act?.length, cat.actions.length, 'the catalogue carries the list');
      for (const x of list) {
        t.ok(x.about && x.example?.do === x.do, `${x.do}: about + example`);
        t.ok((x.fields || []).every((f) => f.name && f.type && f.need), `${x.do}: fields typed`);
        const r = act(t, x.example);
        if (x.spec_only) t.ok(!r.ok && /spec screen/.test(r.why), `${x.do}: refused outside a spec`);
        else t.ok(r.ok || /^(no timer|not done|no such timer)/.test(r.why), `${x.do}: example runs (${r.why || 'ok'})`);
      }
      tm(t, { op: 'reset' });
    } },
  ],
};
