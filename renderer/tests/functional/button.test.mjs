// Button (components/button.cpp): tap, hold, slide-off, disabled, kinds, icon, width, update path.
const one = (evs) => evs.filter((e) => e.kind === 'event' && e.name === 'tap').length;
const H = { '1.5x': 64, '1x': 44 };  // touch button-h
export default {
  id: 'button', family: 'controls', about: 'Text button: one tap = one tap event + tap sound; disabled = error only.',
  tests: [
    { name: 'tap emits one tap with its label and plays tap', mount: ['button', { label: 'Start' }], run(t) {
      const ev = t.tap('START');
      t.emitted(ev, 'tap', 'START'); t.eq(one(ev), 1, 'tap count'); t.sounds(ev, ['tap']);
    } },
    { name: 'hold then release still fires exactly one tap', mount: ['button', { label: 'Start' }], run(t) {
      const ev = t.hold('START', 1200);
      t.eq(one(ev), 1, 'tap count after a 1.2 s hold'); t.ok(ev.filter((e) => e.kind === 'sound').length <= 1, 'at most one sound');
    } },
    { name: 'press, slide off, release: nothing fires', mount: ['button', { label: 'Start' }], run(t) {
      const b = t.part('button'); const ev = t.drag('START', { by: [b.w + 64, 0] });
      t.notEmitted(ev, 'tap', 'slid off');
    } },
    { name: 'disabled: no tap, blocked + error once', mount: ['button', { label: 'Start', state: 'disabled' }], run(t) {
      const ev = t.tap('START');
      t.notEmitted(ev, 'tap'); t.emitted(ev, 'blocked'); t.sounds(ev, ['error']);
      t.notEmitted(t.hold('START'), 'tap', 'disabled hold');
    } },
    ...['primary', 'secondary', 'outline', 'ghost'].map((kind) => ({ name: `${kind}: shows its label and taps`, mount: ['button', { label: 'Go', kind }], run(t) {
      t.has(t.texts(), 'GO', 'label'); t.emitted(t.tap('GO'), 'tap', 'GO');
    } })),
    { name: 'height is the touch button height', mount: ['button', { label: 'Start' }], run(t, { den }) {
      t.eq(t.part('button').h, H[den], 'height');
    } },
    { name: 'hug width: on the 4 px grid and wider than the label', mount: ['button', { label: 'Start' }], run(t) {
      const b = t.part('button'), l = t.find('START');
      t.ok(b.w % 4 === 0, `width ${b.w} not on the 4 px grid`); t.ok(b.w > l.w, `width ${b.w} <= label ${l.w}`);
      t.ok(Math.abs((l.x + l.w / 2) - (b.x + b.w / 2)) <= 1, 'label centred');
    } },
    // (contract v2: width is hug | fill, no pixel width; a set width = the parent's box, C5)
    { name: 'width fill spans the space it is in, label centred', async run(t) {
      await t.mount('button', { label: 'Ok' }); const hug = t.part('button').w;
      await t.mount('button', { label: 'Ok', width: 'fill' }); const b = t.part('button'), l = t.find('OK');
      t.ok(b.w > hug * 2, `fill ${b.w} wider than hug ${hug}`); t.ok(Math.abs((l.x + l.w / 2) - (b.x + b.w / 2)) <= 1, 'label centred');
    } },
    { name: 'icon adds width before the label', async run(t) {
      await t.mount('button', { label: 'Add' }); const w0 = t.part('button').w;
      await t.mount('button', { label: 'Add', icon: 'plus' }); const b = t.part('button'), l = t.find('ADD');
      t.ok(b.w > w0, `icon button ${b.w} not wider than ${w0}`); t.ok(l.x - b.x > b.x + b.w - (l.x + l.w), 'label pushed right by the icon');
      t.emitted(t.tap('ADD'), 'tap');
    } },
    { name: 'update: new label shows, silently', mount: ['button', { label: 'Start' }], run(t) {
      const ev = t.update({ label: 'Stop' }); t.silent(ev); t.has(t.texts(), 'STOP', 'new label'); t.hasNot(t.texts(), 'START', 'old label');
      t.emitted(t.tap('STOP'), 'tap', 'STOP');
    } },
    { name: 'update to disabled blocks taps; back to default taps again', mount: ['button', { label: 'Start' }], run(t) {
      t.silent(t.update({ state: 'disabled' }), 'update silent');
      t.notEmitted(t.tap('START'), 'tap', 'after disable');
      t.update({ state: 'default' }); t.emitted(t.tap('START'), 'tap', 'START', 'after enable');
    } },
    { name: 'active state still taps', mount: ['button', { label: 'Start', state: 'active' }], run(t) {
      t.emitted(t.tap('START'), 'tap');
    } },
  ],
};
