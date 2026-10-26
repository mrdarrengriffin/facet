// Blinds and curtains screen (specs/covers.json: covers_view + cover_visual + slider + buttons) on the spec builder,
// bound to a cover in the fake Home Assistant: Open / Stop / Close, a drag on the window (position), the slats (tilt),
// an outside change, unavailable. Feature "covers" = WIP, not in the firmware (features.json).
const E = 'cover.study_blind';
const screen = (extra = {}) => ['spec', { screen: 'covers', entity: E, covers: `${E},cover.office_curtains`, live: true, ...extra }];
const last = (t, service) => t.ha.calls.filter((c) => c.service === service).at(-1);
const hero = (t) => t.part('covers_view')?.texts || [];
// one pixel of the stage (r,g,b): the ghost bar / curtain fabric are drawn, not objects
const px = (t, x, y) => { const b = t.b; b.m._fw_refresh(b.stage); const p = b.m._fw_frame(b.stage) + (Math.round(y) * b.S + Math.round(x)) * 4; return Array.from(b.m.HEAPU8.slice(p, p + 3)).join(','); };
// drag the roller from the top of its glass halfway down; HA does not react (the ghost waits)
const ghostDrag = (t) => {
  t.ha.auto = false; t.ha.set(E, 'open', { current_position: 100 }); t.wait(600);
  const w = t.part('cover_visual'), x = w.x + w.w / 4, y0 = w.y + 24, y1 = y0 + Math.round(w.h / 2);
  const n = t.ha.calls.length; t.drag(null, { from: [x, y0], by: [0, y1 - y0] });
  const c = t.ha.calls.slice(n).filter((k) => k.service === 'cover.set_cover_position');
  return { w, x, y1, c };
};
export default {
  id: 'covers', family: 'home', about: 'Blinds and curtains screen: the window is the hero; Open / Stop / Close, drag to a position, slats.',
  tests: [
    { name: 'shows the HA position', mount: screen(), run(t) {
      t.ha.set(E, 'open', { current_position: 60 }); t.wait(600);
      t.has(hero(t), /^60%$/, 'position');
    }, },
    { name: 'Open calls open_cover', mount: screen(), run(t) {
      t.ha.set(E, 'open', { current_position: 30 }); t.wait(600);
      t.tap('OPEN'); t.call({ service: 'cover.open_cover', entity: E }, 'open call');
    } },
    { name: 'position follows HA after Open (facet-51)', mount: screen(), run(t) {
      t.ha.set(E, 'open', { current_position: 30 }); t.wait(600);
      t.tap('OPEN'); t.wait(1200); t.has(hero(t), /^Open$/, 'position after open (100 = Open)'); t.hasNot(hero(t), /^30%$/, 'old position');
    }, },
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
    { name: 'drag moves the ghost, not the real blind: one call on release (facet-51)', mount: screen(), run(t) {
      const { c } = ghostDrag(t);
      t.eq(c.length, 1, 'set_cover_position calls after one drag');
      t.ok(c[0] && c[0].data.position > 20 && c[0].data.position < 80, `position ~50: ${JSON.stringify(c[0])}`);
      t.has(hero(t), /^Open$/, 'value = the real cover (HA still 100)');
      t.has(hero(t), /^set to \d+%$/i, 'caption says where it was sent');
    } },
    { name: 'real blind follows HA; the ghost fades into it (facet-51)', mount: screen(), run(t) {
      const { x, y1, c } = ghostDrag(t);
      const ghost = px(t, x, y1);
      t.ha.set(E, 'open', { current_position: c[0]?.data.position ?? 50 }); t.wait(1200);
      t.has(hero(t), new RegExp(`^${c[0]?.data.position}%$`), 'value = HA position');
      t.hasNot(hero(t), /^set to/i, 'caption back to the state');
      t.eq(px(t, x, y1), ghost, 'the real bar now sits where the ghost was, same accent');
    } },
    { name: 'ghost snaps back when HA never moves (facet-51)', mount: screen(), run(t) {
      const { x, y1 } = ghostDrag(t);
      const ghost = px(t, x, y1);
      t.wait(7000);
      t.ok(px(t, x, y1) !== ghost, 'ghost bar gone after cover-ghost-wait');
      t.has(hero(t), /^Open$/, 'still the real position');
    } },
    { name: 'curtains: fabric trails the finger, settles within 400 ms (facet-51)', mount: screen(), run(t) {
      t.ha.auto = false; t.ha.set(E, 'open', { current_position: 100, device_class: 'curtain' }); t.wait(600);
      const w = t.part('cover_visual'), b = t.b, y = w.y + w.h * 0.6, probe = w.x + w.w * 0.4;
      const glass = px(t, probe, y);
      b.touch(w.x + w.w * 0.3, y, 1); b.tick(3); b.touch(w.x + w.w * 0.46, y, 1); b.tick(1);
      t.eq(px(t, probe, y), glass, 'one frame in: the fabric has not caught the finger');
      b.tick(25);
      t.ok(px(t, probe, y) !== glass, 'after 400 ms the fabric is there');
      b.touch(w.x + w.w * 0.46, y, 0); b.tick(2); t.ha.react(b.events()); t.wait(300);
      t.eq(t.ha.calls.filter((k) => k.service === 'cover.set_cover_position').length, 1, 'one set_cover_position');
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
    }, },
    { name: 'unavailable blocks the controls, back again works', mount: screen(), run(t) {
      t.ha.unavailable(E); t.wait(600); const n = t.ha.calls.length;
      t.tap('OPEN'); t.tap('CLOSE'); t.eq(t.ha.calls.length, n, 'no call while unavailable');
      t.ha.set(E, 'open', { current_position: 40 }); t.wait(600);
      t.tap('OPEN'); t.call({ service: 'cover.open_cover', entity: E }, 'open after back');
    } },
  ],
};
