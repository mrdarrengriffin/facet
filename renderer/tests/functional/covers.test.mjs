// Blinds and curtains screen (specs/covers.json: covers_view + cover_visual + slider + buttons) on the spec builder,
// bound to a cover in the fake Home Assistant: Open / Stop / Close, a drag on the window (position), the slats (tilt),
// an outside change, unavailable. Feature "covers" = WIP, not in the firmware (features.json).
const E = 'cover.study_blind';
const screen = (extra = {}) => ['spec', { screen: 'covers', entity: E, covers: `${E},cover.office_curtains`, live: true, ...extra }];
const last = (t, service) => t.ha.calls.filter((c) => c.service === service).at(-1);
const hero = (t) => t.part('covers_view')?.texts || [];
export default {
  id: 'covers', family: 'home', about: 'Blinds and curtains screen: the window is the hero; Open / Stop / Close, drag to a position, slats.',
  tests: [
    { name: 'shows the HA position', mount: screen(), run(t) {
      t.ha.set(E, 'open', { current_position: 60 }); t.wait(600);
      t.has(hero(t), /^60%$/, 'position');
    }, xfail: 'facet-51' },  // covers_view reads position / tilt with `| 40`: a bound number arrives as a double -> default
    { name: 'Open calls open_cover', mount: screen(), run(t) {
      t.ha.set(E, 'open', { current_position: 30 }); t.wait(600);
      t.tap('OPEN'); t.call({ service: 'cover.open_cover', entity: E }, 'open call');
    } },
    { name: 'position follows HA after Open (facet-51)', mount: screen(), run(t) {
      t.ha.set(E, 'open', { current_position: 30 }); t.wait(600);
      t.tap('OPEN'); t.wait(1200); t.has(hero(t), /^100%$/, 'position after open');
    }, xfail: 'facet-51' },
    { name: 'Close calls close_cover', mount: screen(), run(t) {
      t.ha.set(E, 'open', { current_position: 60 }); t.wait(600);
      t.tap('CLOSE'); t.call({ service: 'cover.close_cover', entity: E }, 'close call');
    } },
    { name: 'Stop calls stop_cover', mount: screen(), run(t) {
      t.tap('STOP'); t.call({ service: 'cover.stop_cover', entity: E }, 'stop call');
    } },
    { name: 'drag on the window sets a position (0-100)', mount: screen(), run(t) {
      t.ha.set(E, 'open', { current_position: 100 }); t.wait(600);
      const w = t.part('cover_visual');
      t.drag(null, { from: [w.x + w.w / 2, w.y + 24], by: [0, Math.round(w.h / 2)] });
      const c = last(t, 'cover.set_cover_position');
      t.ok(c && c.entity === E && c.data.position >= 0 && c.data.position < 100, `set_cover_position after a drag down: ${JSON.stringify(c)}`);
    } },
    { name: 'Venetian: slats drag calls set_cover_tilt_position', mount: screen(), run(t) {
      t.ha.set(E, 'open', { current_position: 100, current_tilt_position: 20 }); t.wait(600);
      t.has(t.texts(), /^slats$/i, 'slats label');
      const s = t.part('slider');
      t.ok(!!s, 'slats slider');
      if (s) t.drag(null, { from: [s.x + 8, s.y + s.h / 2], toX: s.x + s.w - 4 });
      const c = last(t, 'cover.set_cover_tilt_position');
      t.ok(c && c.entity === E && c.data.tilt_position > 20, `tilt call: ${JSON.stringify(c)}`);
    } },
    { name: 'curtains: device_class curtain draws curtains', mount: screen(), run(t) {
      t.ha.set(E, 'open', { current_position: 50, device_class: 'curtain' }); t.wait(600);
      t.has(hero(t), /curtain/i, 'kind line');
    } },
    { name: 'outside change animates to the new position, silently', mount: screen(), run(t) {
      t.ha.set(E, 'open', { current_position: 60 }); t.wait(600);
      t.ha.set(E, 'open', { current_position: 25 }); const ev = t.wait(1200);
      t.has(hero(t), /^25%$/, 'new position'); t.silent(ev);
    }, xfail: 'facet-51' },
    { name: 'unavailable blocks the controls, back again works', mount: screen(), run(t) {
      t.ha.unavailable(E); t.wait(600); const n = t.ha.calls.length;
      t.tap('OPEN'); t.tap('CLOSE'); t.eq(t.ha.calls.length, n, 'no call while unavailable');
      t.ha.set(E, 'open', { current_position: 40 }); t.wait(600);
      t.tap('OPEN'); t.call({ service: 'cover.open_cover', entity: E }, 'open after back');
    } },
  ],
};
