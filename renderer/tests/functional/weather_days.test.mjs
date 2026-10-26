// Weather, 5 days (components/weather_days.cpp on specs/weather_days.json). facet-185: the rows sit inside the screen
// content inset (like List rows), never edge to edge.
export default {
  id: 'weather_days', family: 'home', about: 'Weather next 5 days: one row per day inside the content inset.',
  tests: [
    { name: 'rows and their texts sit inside the content inset', async run(t, { S }) {
      await t.device(); t.show('weather_days');
      const p = t.part('weather_days');
      t.ok(!!p, 'weather_days on screen');
      if (!p) return;
      const inset = Math.round(S * 0.04);
      t.ok(p.x >= inset && p.x + p.w <= S - inset, `block ${p.x}..${p.x + p.w} inside ${inset}..${S - inset}`);
      const tx = t.objs().filter((o) => o.text && t.visible(o) && o.y >= p.y && o.y < p.y + p.h);
      t.ok(tx.length >= 5, `texts in the rows: ${tx.length}`);
      t.ok(tx.every((o) => o.x >= p.x + 8 && o.x + o.w <= p.x + p.w - 8), `texts padded inside the rows: ${tx.filter((o) => o.x < p.x + 8 || o.x + o.w > p.x + p.w - 8).map((o) => `${o.text}@${o.x}`).join(' ')}`);
    } },
  ],
};
