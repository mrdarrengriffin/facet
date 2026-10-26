// Hold button (components/hold_button.cpp): hold for hold-confirm (1.2 s) = confirm(value) + sound confirm; let go
// early = nothing; disabled blocks; features / mask hide it; width hug / fill; update path.
const L = 'HOLD TO OPEN', P = { label: 'Hold to open', value: 'open' };
const confirms = (ev) => ev.filter((e) => e.kind === 'event' && e.name === 'confirm').length;
export default {
  id: 'hold_button', family: 'controls', about: 'A button that needs a 1.2 s hold before it acts (door latch).',
  tests: [
    { name: 'hold 1.5 s: one confirm(value) + confirm sound', mount: ['hold_button', P], run(t) {
      const ev = t.hold(L, 1500); t.emitted(ev, 'confirm', 'open'); t.eq(confirms(ev), 1, 'confirm count'); t.sounds(ev, ['confirm']);
    } },
    { name: 'hold 4 s: still exactly one confirm', mount: ['hold_button', P], run(t) { t.eq(confirms(t.hold(L, 4000)), 1, 'confirm count'); } },
    { name: 'a plain tap: nothing (no tap event, no sound)', mount: ['hold_button', P], run(t) { const ev = t.tap(L); t.notEmitted(ev, 'confirm'); t.notEmitted(ev, 'tap'); t.silent(ev); } },
    { name: 'let go at 0.8 s: nothing, and the next full hold still confirms', mount: ['hold_button', P], run(t) {
      const ev = t.hold(L, 800); t.notEmitted(ev, 'confirm', 'early'); t.hasNot(ev.filter((e) => e.kind === 'sound').map((e) => e.name), 'confirm', 'no confirm sound');
      t.emitted(t.hold(L, 1500), 'confirm', 'open', 'full hold after');
    } },
    { name: 'value prop is the confirm detail', mount: ['hold_button', { label: 'Hold to unlock', value: 'unlock' }], run(t) {
      t.emitted(t.hold('HOLD TO UNLOCK', 1500), 'confirm', 'unlock');
    } },
    { name: 'disabled: blocked + error, no confirm', mount: ['hold_button', { ...P, state: 'disabled' }], run(t) {
      const ev = t.hold(L, 1500); t.notEmitted(ev, 'confirm'); t.emitted(ev, 'blocked'); t.sounds(ev, ['error']);
    } },
    { name: 'features without the mask bit: hidden', mount: ['hold_button', { ...P, features: 1, mask: 2 }], run(t) {
      t.hasNot(t.texts(), L, 'label hidden'); t.eq(t.objs().filter((o) => o.clk && t.visible(o)).length, 0, 'no visible target');
    } },
    { name: 'features with the mask bit: shown and works', mount: ['hold_button', { ...P, features: 3, mask: 2 }], run(t) {
      t.has(t.texts(), L, 'label'); t.emitted(t.hold(L, 1500), 'confirm', 'open');
    } },
    { name: 'hug is narrower than fill and hugs the label', async run(t) {
      await t.mount('hold_button', { ...P, width: 'fill' }); const f = t.part('hold_button').w;
      await t.mount('hold_button', { ...P, width: 'hug' }); const h = t.part('hold_button').w, l = t.find(L).w;
      t.ok(h < f, `hug ${h} >= fill ${f}`); t.ok(h > l && h % 4 === 0, `hug ${h} vs label ${l}`);
    } },
    { name: 'update features: hides, then shows again; silent', mount: ['hold_button', { ...P, features: 2, mask: 2 }], run(t) {
      t.silent(t.update({ features: 1 }), 'hide'); t.hasNot(t.texts(), L, 'hidden');
      t.silent(t.update({ features: 2 }), 'show'); t.has(t.texts(), L, 'shown'); t.emitted(t.hold(L, 1500), 'confirm', 'open', 'works after');
    } },
    { name: 'update label: new words, silent', mount: ['hold_button', P], run(t) {
      t.silent(t.update({ label: 'Hold to close' })); t.has(t.texts(), 'HOLD TO CLOSE', 'label');
    } },
  ],
};
