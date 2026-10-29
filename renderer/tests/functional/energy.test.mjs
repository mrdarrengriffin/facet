// Energy screen (specs/energy.json, CONTRACT C51 feed panel.energy): today's view, and the Week view the More row
// "last 7 days" opens on the same screen (docs energy.ts weekView: header "Last 7 days", this week's cost, the week
// against the week before as pairs of bars with the day names under them); the screen's Back closes the Week view
// first (screen.param "back"), then leaves the screen.
const back = (t) => t.part('back_button');
const vis = (t, re) => t.objs().some((o) => o.text && t.visible(o) && re.test(String(o.text)));
const DAYS = /^(Mon|Tue|Wed|Thu|Fri|Sat|Sun|Today)$/;
const week = async (t) => { await t.device(); t.show('energy'); t.tap(/^last 7 days$/i); t.wait(900); };
export default {
  id: 'energy', family: 'charts', about: 'Energy screen: today, and the last 7 days as a view of the same screen.',
  tests: [
    { name: 'today: hero, rate, More row; no week parts', async run(t) {
      await t.device(); t.show('energy');
      t.ok(vis(t, /^today so far$/i), 'today so far');
      t.ok(vis(t, /^last 7 days$/i), 'More row');
      t.ok(!vis(t, /^this week$/i), 'no week hero');
      t.ok(!vis(t, /^week before$/i), 'no week legend');
    } },
    { name: 'last 7 days opens the Week view on the same screen', async run(t) {
      await week(t);
      t.eq(t.where(), 'energy', 'still the Energy screen');
      t.ok(vis(t, /^Last 7 days$/i), 'header Last 7 days');
      t.ok(!vis(t, /^Energy$/i), 'header Energy gone');
      t.ok(vis(t, /^this week$/i), 'week hero label');
      t.ok(vis(t, /^week before$/i), 'legend: week before');
      t.ok(vis(t, /a day than the week before|kWh/), 'week line');
      t.ok(!vis(t, /^today so far$/i), 'today hidden');
      t.ok(!vis(t, /W now$/), 'now chip hidden');
      const ds = t.objs().filter((o) => o.text && t.visible(o) && DAYS.test(String(o.text)));
      t.eq(ds.length, 7, `7 day names: ${ds.map((o) => o.text).join(' ')}`);
      t.eq(String(ds.at(-1)?.text), 'Today', 'today last');
      t.ok(ds.every((o, i) => !i || o.x > ds[i - 1].x), 'left to right');
      const b = t.part('bar_chart');
      t.ok(b && ds.every((o) => o.x >= b.x - 1 && o.x + o.w <= b.x + b.w + 1), 'day names inside the chart width');
    } },
    { name: 'Back closes the Week view first, then leaves', async run(t) {
      await week(t);
      t.tap(back(t)); t.wait(900);
      t.eq(t.where(), 'energy', 'Back 1: still Energy');
      t.ok(vis(t, /^today so far$/i), 'today again');
      t.ok(vis(t, /^Energy$/i), 'header Energy');
      t.ok(!vis(t, /^this week$/i), 'week gone');
      t.tap(back(t)); t.wait(900);
      t.ok(t.where() !== 'energy', `Back 2 leaves (${t.stack().join(' > ')})`);
    } },
  ],
};
