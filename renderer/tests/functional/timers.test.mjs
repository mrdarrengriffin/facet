// The panel's timers as data (core/device.h "the panel's timers", core/timers.cpp; core/spec.h "local sources"
// panel.timer + the timer.* actions; Darren 2026-10-28, notes/facet-design/timer.md): up to 4, soonest first, a 5th
// refused with a reason, extend / cancel / stop / select, done = the timer-done sound once (whatever screen shows),
// the binds' values (name, left, share, line, others), Button taps, the legacy one-timer view (Home header).
const tm = (t, q) => JSON.parse(t.b.str("timer", (c) => t.b.str(JSON.stringify(q), (a) => t.b.m.UTF8ToString(t.b.m._fw_test(c, a)))));
const names = (r) => r.timers.map((x) => x.name);
const SPEC = {
  v: 1, id: 'tmr_t', name: 'Timers', rev: 1, role: 'page', header: { label: 'Timers' }, layout: 'list',
  nodes: [
    { c: 'label', p: { text: { e: 'panel.timer', a: 'name' } } },
    { c: 'label', p: { text: { e: 'panel.timer', a: 'left', tpl: 'left {}' } } },
    { c: 'label', p: { text: { e: 'panel.timer', a: 'line' } } },
    { c: 'label', p: { text: 'IS RUNNING' }, show: { e: 'panel.timer', map: { running: '1' }, else: '' } },
    { c: 'grid', p: { cols: 3, items: { e: 'panel.timer', a: 'others' } }, cell: { c: 'label', p: { text: 'chip $item.name' } } },
  ],
};
const BTNS = {
  v: 1, id: 'tmr_b', name: 'Timer buttons', rev: 1, role: 'page', header: { label: 'Timer buttons' }, layout: 'list',
  nodes: [
    { c: 'button', p: { label: 'Go 5', kind: 'primary' }, on: { tap: { do: 'timer.start', seconds: 300 } } },
    { c: 'button', p: { label: 'Go pasta', kind: 'secondary' }, on: { tap: { do: 'timer.start', seconds: 600, name: 'Pasta' } } },
    { c: 'button', p: { label: 'Plus one', kind: 'secondary' }, on: { tap: { do: 'timer.extend', s: 60 } } },
    { c: 'button', p: { label: 'Drop it', kind: 'outline' }, on: { tap: { do: 'timer.cancel' } } },
  ],
};
const fresh = (t) => tm(t, { op: 'reset' });
export default {
  id: 'timers', family: 'layout', about: 'The panel\'s timers as data: the list, done, the panel.timer binds and timer actions.',
  tests: [
    { name: 'up to 4, soonest first; a 5th is refused with a reason', async run(t) {
      await t.device(); fresh(t);
      tm(t, { op: 'start', name: 'Pasta', seconds: 600 }); tm(t, { op: 'start', seconds: 300 });
      tm(t, { op: 'start', name: 'Eggs', seconds: 420 }); let r = tm(t, { op: 'start', name: 'Roast', seconds: 5400 });
      t.eq(names(r), ['5 min', 'Eggs', 'Pasta', 'Roast'], 'soonest first, a preset named by its length');
      r = tm(t, { op: 'start', name: 'Tea', seconds: 240 });
      t.ok(/4 timers/.test(r.why), `5th refused with a reason: ${r.why}`); t.eq(r.timers.length, 4, 'still 4');
      t.eq(r.store.free, '0', 'free 0'); t.eq(r.store.count, '4', 'count 4');
      t.ok(/^\[\["5 min",/.test(r.ha_list) && r.ha_list.length < 255, `HA list fits a state: ${r.ha_list.length}`);
      t.ok(/1 second to 24 hours/.test(tm(t, { op: 'reset' }) && tm(t, { op: 'start', seconds: 0 }).why), 'a bad length refused');
    } },
    { name: 'a second "5 min" gets its own name', async run(t) {
      await t.device(); fresh(t);
      tm(t, { op: 'start', seconds: 300 }); const r = tm(t, { op: 'start', seconds: 300 });
      t.eq(names(r).sort(), ['5 min', '5 min 2'], 'unique names');
      t.eq(tm(t, { op: 'find', name: '5 MIN 2' }).found, r.timers.find((x) => x.name === '5 min 2').id, 'found by name, any case');
    } },
    { name: 'extend and cancel by id; the shown one = soonest unless selected', async run(t) {
      await t.device(); fresh(t);
      tm(t, { op: 'start', name: 'Pasta', seconds: 600 }); let r = tm(t, { op: 'start', name: 'Eggs', seconds: 120 });
      const eggs = r.timers[0].id, pasta = r.timers[1].id;
      t.eq(r.shown, eggs, 'soonest shown'); t.eq(r.store.name, 'Eggs', 'bind name = the shown');
      r = tm(t, { op: 'extend', id: eggs, seconds: 600 });
      t.eq(r.timers.map((x) => [x.name, x.total]), [['Pasta', 600], ['Eggs', 720]], 'extended: Eggs after Pasta now');
      t.eq(r.shown, pasta, 'the new soonest shown');
      const ch = [...r.changes];
      r = tm(t, { op: 'select', id: eggs }); t.eq([r.shown, r.store.name], [eggs, 'Eggs'], 'selected'); ch.push(...r.changes);
      t.eq(JSON.parse(r.store.others).map((x) => x.name), ['Pasta'], 'others = all but the shown');
      r = tm(t, { op: 'cancel', id: eggs }); t.eq([names(r), r.shown], [['Pasta'], pasta], 'cancelled: selection back to default');
      ch.push(...r.changes); t.eq(ch, ['extend Eggs', 'select Eggs', 'cancel Eggs'], 'one change each (observers: never per second)');
    } },
    { name: 'done: the sound once, state done until stopped; +1 min runs it again', async run(t) {
      await t.device(); fresh(t);
      tm(t, { op: 'start', name: 'Eggs', seconds: 3 });
      let ev = t.wait(4000);
      t.eq(ev.filter((e) => e.kind === 'sound' && e.name === 'timer-done').length, 1, 'timer-done once');
      let r = tm(t, { op: 'list' });
      t.eq([r.timers[0].state, r.store.state, r.store.line], ['done', 'done', 'of 3 s · done'], 'done');
      t.ok(r.changes.includes('done Eggs'), 'observers told: done');
      ev = t.wait(3000); t.ok(!ev.some((e) => e.name === 'timer-done'), 'no second sound');
      t.ok(/not done/.test(tm(t, { op: 'start', name: 'Pasta', seconds: 60 }) && tm(t, { op: 'stop', name: 'Pasta' }).why), 'stop refuses a running one');
      r = tm(t, { op: 'extend', name: 'Eggs', seconds: 60 }); t.eq(r.timers.find((x) => x.name === 'Eggs').state, 'running', '+1 min after it rang');
      tm(t, { op: 'reset' }); tm(t, { op: 'start', name: 'Eggs', seconds: 1 }); t.wait(2000);
      r = tm(t, { op: 'stop', name: 'Eggs' }); t.eq([r.why, r.timers.length, r.store.state], ['', 0, 'idle'], 'stopped: gone, idle');
    } },
    { name: 'keeps running after leaving the screen; the legacy view (Home header) reads the soonest', async run(t) {
      await t.device(); fresh(t);
      t.send(SPEC); t.show('tmr_t');
      tm(t, { op: 'start', name: 'Pasta', seconds: 125 });
      t.show('moon'); t.wait(5000);
      let r = tm(t, { op: 'list' });
      t.eq([r.timers[0].left <= 121, r.timers[0].left >= 119], [true, true], `ran while away: ${r.timers[0].left}`);
      t.eq([r.legacy.state, r.legacy.label], [1, 'Pasta'], 'legacy timer() = the soonest running');
      t.show('tmr_t'); t.has(t.texts(), 'Pasta', 'back: the name'); t.ok(t.texts().some((x) => /^left 2:0\d$/.test(x)), `left now: ${t.texts().join('|')}`);
      t.unsend('tmr_t');
    } },
    { name: 'binds: name, left m:ss / h:mm:ss, share, line, show by state, others in a Grid', async run(t) {
      await t.device(); fresh(t);
      t.send(SPEC); t.show('tmr_t');
      t.ok(!t.texts().includes('IS RUNNING'), 'show: hidden while idle');
      tm(t, { op: 'start', name: 'Roast', seconds: 5400 }); tm(t, { op: 'start', name: 'Pasta', seconds: 600 }); t.wait(100);
      const x = t.texts();
      t.has(x, 'Pasta', 'name'); t.has(x, 'left 10:00', 'left m:ss'); t.has(x, 'IS RUNNING', 'show: running');
      t.ok(x.some((s) => /^of 10 min · ends \d\d:\d\d$/.test(s)), `line: ${x.join('|')}`);
      t.has(x, 'chip Roast', 'the other timer in the Grid');
      let r = tm(t, { op: 'list' }); t.eq(r.store.share, '1000', 'share full at the start');
      t.wait(3000); r = tm(t, { op: 'list' }); t.eq([r.store.left, r.store.share], ['9:57', '995'], 'ticks while shown');
      tm(t, { op: 'select', name: 'Roast' }); t.wait(100); t.ok(t.texts().some((s) => /^left 1:29:5\d$/.test(s)), 'h:mm:ss'); t.has(t.texts(), 'chip Pasta', 'others follow the selection');
      t.unsend('tmr_t');
    } },
    { name: 'Button taps: timer.start / extend / cancel; full = a warn toast', async run(t) {
      await t.device(); fresh(t);
      t.send(BTNS); t.show('tmr_b');
      t.tap(/go 5/i); let r = tm(t, { op: 'list' }); t.eq(names(r), ['5 min'], 'start: named by its length');
      t.tap(/plus one/i); r = tm(t, { op: 'list' }); t.eq(r.timers[0].total, 360, 'extend the shown');
      t.tap(/go pasta/i); t.tap(/go pasta/i); t.tap(/go pasta/i); r = tm(t, { op: 'list' });
      t.eq(names(r), ['5 min', 'Pasta', 'Pasta 2', 'Pasta 3'], 'four');
      t.tap(/go pasta/i); t.wait(500); t.ok(t.texts().some((s) => /No timer started/.test(s)), 'full: the toast says so');
      t.tap(/drop it/i); r = tm(t, { op: 'list' }); t.eq(names(r), ['Pasta', 'Pasta 2', 'Pasta 3'], 'cancel the shown (soonest)');
      t.unsend('tmr_b');
    } },
    { name: 'spec checks: bad timer actions are refused', async run(t) {
      await t.device();
      const bad = (on) => t.send({ ...SPEC, id: 'tmr_bad', nodes: [{ c: 'button', p: { label: 'x' }, on: { tap: on } }] });
      t.ok(bad({ do: 'timer.start' }).errors?.some((e) => /seconds/.test(e.msg)), 'start needs seconds');
      t.ok(bad({ do: 'timer.extend', s: 0 }).errors?.some((e) => /1 to 86400/.test(e.msg)), 'extend s range');
      t.ok(bad({ do: 'light.toggle', e: 'light.x', seconds: 3 }).errors?.length, 'seconds only on timer actions');
      t.ok(bad({ do: 'timer.start', seconds: 60, id: '2' }).errors?.length, 'no id on start');
      t.ok(!bad({ do: 'timer.select', name: 'Pasta' }).errors, 'select by name ok');
      t.unsend('tmr_bad');
    } },
  ],
};
