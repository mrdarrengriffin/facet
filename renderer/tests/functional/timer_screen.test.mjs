// Timer screen (specs/timer.json, Darren 2026-10-28 concept T1; notes/facet-design/timer.md): the page's flows on the
// panel's real timer list (core/timers.cpp): idle presets, start, + New = the lengths again (picking) and Cancel
// back, switching timers with a chip, cancelling the shown one only, two of the same length kept apart, a done one
// stopped. Every test fails without the behaviour (the flows Darren found broken: chips did nothing, Cancel hit the
// wrong timer).
const tm = (t, q) => JSON.parse(t.b.str("timer", (c) => t.b.str(JSON.stringify(q), (a) => t.b.m.UTF8ToString(t.b.m._fw_test(c, a)))));
const list = (t) => tm(t, { op: 'list' }).timers;
const has = (t, re) => t.texts().some((s) => re.test(s));
const shownName = (t) => tm(t, { op: 'list' }).store.name;
async function open(t) {
  await t.device(); tm(t, { op: 'reset' });
  t.show('timer'); t.wait(500);
  return t;
}
// the Timer bar's chips left to right (no names on them: the countdown only); chip(t, i) taps the i-th, chipFor(t, id) the
// one whose timer has that id (the bar is in the list's order, soonest first)
const chips = (t) => t.comps().filter((c) => c.ids.includes('timer_chip')).sort((a, z) => a.x - z.x);
const chip = (t, i) => { const c = chips(t)[i]; return t.tap({ at: [c.x + c.w / 2, c.y + c.h / 2] }); };
const chipFor = (t, id) => chip(t, list(t).findIndex((x) => String(x.id) === String(id)));

export default {
  id: 'timer_screen', family: 'screens', about: 'The Timer screen: presets, + New, switching with a chip, cancel one, stop a done one.',
  tests: [
    { name: 'idle: the four presets, no running buttons; a preset starts a timer named by its length', async run(t) {
      await open(t);
      for (const m of [3, 5, 10, 20]) t.ok(has(t, new RegExp(`^${m} min$`, 'i')), `${m} min preset shown`);
      t.ok(!has(t, /^\+1 min$/i) && !has(t, /^cancel$/i), 'no +1 min / Cancel while idle');
      t.tap(/^5 min$/i); t.wait(300);
      t.eq(list(t).map((x) => x.name), ['5 min'], 'one timer, named 5 min');
      t.ok(has(t, /^\+1 min$/i) && has(t, /^cancel$/i) && has(t, /^\+ new$/i), 'running: +1 min, Cancel, + New');
      t.ok(!has(t, /^3 min$/i), 'presets gone while running');
    } },
    { name: '+ New shows the lengths over the running timer; Cancel goes back; a length starts a second timer', async run(t) {
      await open(t); t.tap(/^5 min$/i); t.wait(300);
      t.tap(/^\+ new$/i); t.wait(300);
      t.ok(has(t, /^10 min$/i) && has(t, /^cancel$/i), 'picking: the lengths and Cancel');
      t.ok(!has(t, /^\+1 min$/i), 'running buttons hidden while picking');
      t.tap(/^cancel$/i); t.wait(300);
      t.eq(list(t).length, 1, 'Cancel while picking cancels no timer');
      t.ok(has(t, /^\+1 min$/i), 'back to the running buttons');
      t.tap(/^\+ new$/i); t.wait(300); t.tap(/^10 min$/i); t.wait(300);
      t.eq(list(t).map((x) => x.name).sort(), ['10 min', '5 min'], 'two timers');
      t.ok(has(t, /^\+1 min$/i), 'picking ends with the start');
    } },
    { name: 'a chip switches the ring to its timer', async run(t) {
      await open(t); t.tap(/^5 min$/i); t.wait(300); t.tap(/^\+ new$/i); t.wait(300); t.tap(/^10 min$/i); t.wait(300);
      const before = shownName(t);
      t.eq(chips(t).length, 2, 'both timers in the bar');
      const other = list(t).find((x) => x.name !== before);
      chipFor(t, other.id); t.wait(300);
      t.ok(shownName(t) !== before, `the ring shows the chip's timer (was ${before}, now ${shownName(t)})`);
    } },
    { name: 'Cancel cancels only the timer in the ring', async run(t) {
      await open(t); t.tap(/^5 min$/i); t.wait(300); t.tap(/^\+ new$/i); t.wait(300); t.tap(/^10 min$/i); t.wait(300);
      chipFor(t, list(t).find((x) => x.name === '10 min').id); t.wait(300);
      t.eq(shownName(t), '10 min', '10 min in the ring');
      t.tap(/^cancel$/i); t.wait(300);
      t.eq(list(t).map((x) => x.name), ['5 min'], 'only 10 min cancelled');
    } },
    { name: 'two timers of the same length: the chip picks its own one', async run(t) {
      await open(t); t.tap(/^5 min$/i); t.wait(1200); tm(t, { op: 'start', seconds: 300 }); t.wait(300);  // the 2nd 5 min (the ring's "5 min" label would match a text tap)
      const ids = list(t).map((x) => x.id);
      t.eq(ids.length, 2, 'two 5 min timers');
      const ring = tm(t, { op: 'list' }).store.id;
      const ev = chipFor(t, ids.find((x) => String(x) !== String(ring))); t.wait(300);
      t.ok(tm(t, { op: 'list' }).store.id !== ring, `the chip brought the other 5 min into the ring (events ${JSON.stringify(ev.filter((e) => e.kind === 'event').map((e) => [e.name, e.detail]))}, ring ${ring})`);
      t.tap(/^cancel$/i); t.wait(300);
      t.eq(list(t).map((x) => String(x.id)), [String(ring)], 'Cancel removed the shown one, the other stays');
    } },
    { name: 'Darren\'s freeze (2026-10-28 21:36): 10 min, + New, 3 min, + New, 20 min stays responsive', async run(t) {
      await open(t);
      for (const step of [/^10 min$/i, /^\+ new$/i, /^3 min$/i, /^\+ new$/i, /^20 min$/i]) { t.tap(step); t.wait(400); }
      t.eq(list(t).length, 3, 'three timers');
      t.eq(chips(t).length, 3, 'three chips in the bar');
      t.wait(3000);
      t.ok(has(t, /^\+1 min$/i), 'still drawing and answering after 3 s');
    } },
    { name: 'a done timer: Stop removes it', async run(t) {
      await open(t); tm(t, { op: 'start', seconds: 1 }); t.wait(2500);
      t.ok(has(t, /^stop$/i), 'done: Stop shown');
      t.tap(/^stop$/i); t.wait(300);
      t.eq(list(t).length, 0, 'stopped');
      t.ok(has(t, /^3 min$/i), 'idle presets again');
    } },
  ],
};
