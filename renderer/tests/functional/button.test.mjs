// Button (components/button.cpp): tap, hold, slide-off, disabled, kinds, icon, width, update path; the hold prop
// (was the Hold button): hold for hold_time = confirm(value) + sound confirm, let go early = nothing, disabled blocks.
const HL = 'HOLD TO OPEN', HP = { kind: 'secondary', label: 'Hold to open', value: 'open', hold: true, width: 'fill' };
const confirms = (ev) => ev.filter((e) => e.kind === 'event' && e.name === 'confirm').length;
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
    { name: 'hold: 1.5 s = one confirm(value) + confirm sound, no tap', mount: ['button', HP], run(t) {
      const ev = t.hold(HL, 1500); t.emitted(ev, 'confirm', 'open'); t.eq(confirms(ev), 1, 'confirm count'); t.sounds(ev, ['confirm']); t.notEmitted(ev, 'tap');
    } },
    { name: 'hold: 4 s is still exactly one confirm', mount: ['button', HP], run(t) { t.eq(confirms(t.hold(HL, 4000)), 1, 'confirm count'); } },
    { name: 'hold: a plain tap does nothing (no tap event, no sound)', mount: ['button', HP], run(t) { const ev = t.tap(HL); t.notEmitted(ev, 'confirm'); t.notEmitted(ev, 'tap'); t.silent(ev); } },
    { name: 'hold: let go at 0.8 s = nothing, the next full hold still confirms', mount: ['button', HP], run(t) {
      const ev = t.hold(HL, 800); t.notEmitted(ev, 'confirm', 'early'); t.hasNot(ev.filter((e) => e.kind === 'sound').map((e) => e.name), 'confirm', 'no confirm sound');
      t.emitted(t.hold(HL, 1500), 'confirm', 'open', 'full hold after');
    } },
    { name: 'hold: no value = the label is the confirm detail', mount: ['button', { ...HP, value: '' }], run(t) { t.emitted(t.hold(HL, 1500), 'confirm', 'Hold to open'); } },
    { name: 'hold: hold_time 3000 = no confirm at 1.5 s, one at 3.5 s', mount: ['button', { ...HP, hold_time: 3000 }], run(t) {
      t.notEmitted(t.hold(HL, 1500), 'confirm', 'at 1.5 s'); t.emitted(t.hold(HL, 3500), 'confirm', 'open', 'at 3.5 s');
    } },
    { name: 'hold: disabled = blocked + error, no confirm', mount: ['button', { ...HP, state: 'disabled' }], run(t) {
      const ev = t.hold(HL, 1500); t.notEmitted(ev, 'confirm'); t.emitted(ev, 'blocked'); t.sounds(ev, ['error']);
    } },
    { name: 'hold: hold_label shows while held, the label is back after', mount: ['button', { ...HP, hold_label: 'Keep holding' }], run(t) {
      const o = t.find(HL), x = o.x + o.w / 2, y = o.y + o.h / 2;
      t.b.touch(x, y, 1); t.wait(300); t.has(t.texts(), 'KEEP HOLDING', 'held words'); t.b.touch(x, y, 0);
      const ev = t.wait(600); t.has(t.texts(), HL, 'label back'); t.notEmitted(ev, 'confirm', 'early'); t.emitted(t.hold(HL, 1500), 'confirm', 'open', 'still confirms');
    } },
    { name: 'hold: update on / off in place, silent', mount: ['button', { kind: 'secondary', label: 'Open' }], run(t) {
      t.silent(t.update({ hold: true, value: 'open' }), 'on'); t.notEmitted(t.tap('OPEN'), 'tap', 'no tap when hold'); t.emitted(t.hold('OPEN', 1500), 'confirm', 'open');
      t.silent(t.update({ hold: false }), 'off'); t.emitted(t.tap('OPEN'), 'tap', 'OPEN', 'taps again');
    } },
    { name: 'hold: a primary hold button is drawn as secondary', async run(t) {
      await t.mount('button', { label: 'Go', kind: 'secondary', hold: true }); const s = t.part('button').w;
      await t.mount('button', { label: 'Go', hold: true }); t.eq(t.part('button').w, s, 'same width'); t.emitted(t.hold('GO', 1500), 'confirm', 'Go');
    } },
  ],
};
