// Settings (screens/settings.cpp): a control on the glass changes its setting through the one set() path, so what reads
// the setting (the panel's sounds read "volume" on every sound) follows at once.
const act = (t, a) => { t.b.spec('act', a); t.b.tick(10); t.b.settle(800); };
export default {
  id: 'settings_screen', family: 'device', about: 'Settings on the glass: a control changes its setting at once.',
  tests: [
    { name: 'Volume: dragging the slider on the glass changes the volume setting', async run(t) {
      await t.device();
      act(t, { do: 'nav.open', screen: 'settings' });
      const slider = () => { const v = t.texts().includes('Volume') && t.find('Volume'); return v && t.comps().find((c) => c.ids.includes('slider') && t.visible(c) && c.y > v.y && c.y + c.h <= t.b.S); };
      for (let i = 0; i < 8 && !slider(); i++) { t.drag({ at: [t.b.S / 2, t.b.S * 0.8] }, { by: [0, -t.b.S * 0.25] }); t.wait(600); }
      t.has(t.texts(), 'Volume', 'the Volume row');
      const was = t.setting('volume');
      const sl = slider();
      t.ok(sl, 'the Volume slider');
      t.drag({ at: [sl.x + sl.w * 0.8, sl.y + sl.h / 2] }, { by: [-sl.w * 0.6, 0] }); t.wait(600);
      t.ok(t.setting('volume') !== was, `volume ${was} -> ${t.setting('volume')}`);
    } },
    { name: 'Update: up to date + a host that cannot check = a tap says why, opens nothing', async run(t) {
      await t.device(); t.b.test('update', 'none,0.5.0,0.5.0');
      act(t, { do: 'nav.open', screen: 'settings' });
      t.has(t.texts(), 'Up to date', 'the value');
      t.has(t.texts(), 'Installed 0.5.0', 'the version installed, under it');
      const evs = t.tap('Up to date'); t.wait(600);
      t.emitted(evs, 'update', 'this panel cannot check');  // the workbench has no checker
      t.hasNot(t.texts(), 'Checking\u2026', 'no check');
      t.hasNot(t.texts(), 'UPDATE', 'no sheet');
    } },
    { name: 'Update: a tap checks now: Checking..., repeated taps ignored, then the result (#159)', async run(t) {
      await t.device(); t.b.test('update', 'none,0.5.0,0.5.0'); t.b.test('update', 'host');
      act(t, { do: 'nav.open', screen: 'settings' });
      let evs = t.tap('Up to date'); t.wait(300);
      t.emitted(evs, 'update', 'check');
      t.has(t.texts(), 'Checking\u2026', 'Checking... at once');
      evs = t.tap('Checking\u2026'); t.wait(300);
      t.notEmitted(evs, 'update');  // a tap while it runs asks nothing
      // the same version: Up to date again
      t.b.test('update', 'none,0.5.0,0.5.0'); t.b.test('update', 'checked:'); t.wait(600);
      t.has(t.texts(), 'Up to date', 'up to date');
      t.hasNot(t.texts(), 'Checking\u2026', 'the check ended');
      // no network: why, in words
      t.tap('Up to date'); t.wait(300);
      t.b.test('update', 'checked:no Wi-Fi'); t.wait(600);
      t.has(t.texts(), "Couldn't check", 'the value');
      t.has(t.texts(), "Couldn't check: no Wi-Fi", 'the reason, under it');
      // a newer version: the value, and the sheet with Update opens on its own (this tap asked)
      t.tap("Couldn't check"); t.wait(300);
      t.b.test('update', 'available,0.5.0,0.6.0'); t.b.test('update', 'checked:'); t.wait(1200);
      t.has(t.texts(), '0.6.0 available', 'available');
      t.ok(t.objs().some((o) => o.text === 'UPDATE' && t.visible(o)), 'the Update sheet opened with its button');
      t.b.test('update', 'nohost');
    } },
    { name: 'Update: available = the version in the row; its sheet has the Update button, which asks the host', async run(t) {
      await t.device(); t.b.test('update', 'available,0.5.0,0.6.0');
      act(t, { do: 'nav.open', screen: 'settings' });
      t.has(t.texts(), '0.6.0 available', 'the value');
      t.tap('0.6.0 available'); t.wait(800);
      t.has(t.texts(), 'Installed 0.5.0', 'the sheet: the installed version');
      t.has(t.texts(), '0.6.0', 'the sheet: the new one');
      const b = t.objs().filter((o) => o.text === 'UPDATE' && t.visible(o)).pop();  // the Button (the title says it too, above)
      const evs = t.tap({ at: [b.x + b.w / 2, b.y + b.h / 2] }); t.wait(800);
      t.emitted(evs, 'update', 'this panel cannot install');  // the workbench has no installer
      t.ok(!t.objs().some((o) => o.text === 'UPDATE' && t.visible(o)), 'the sheet closed');
    } },
    { name: 'Update: the preview prop moves the row and the sheet in place (the playground)', async run(t) {
      await t.mount('settings', { update: 'up_to_date' });
      t.has(t.texts(), 'Up to date', 'up to date');
      t.update({ update: 'available' }); t.wait(600);
      t.has(t.texts(), '0.6.0 available', 'available');
      t.update({ update: 'available', sheet: 'update' }); t.wait(800);
      t.has(t.texts(), 'Installed 0.5.0', 'the Update sheet');
      t.update({ update: 'installing', sheet: 'closed' }); t.wait(800);
      t.has(t.texts(), 'Installing', 'installing');
      t.update({ update: 'checking' }); t.wait(600);
      t.has(t.texts(), 'Checking\u2026', 'checking');
      t.update({ update: 'failed' }); t.wait(600);
      t.has(t.texts(), "Couldn't check: no Wi-Fi", 'failed, with why');
    } },
    { name: 'Update password: no row without one (the workbench)', async run(t) {
      // drags in the left gutter: the full-width sliders (Volume, Backlight) would take a drag in the middle at 480
      await t.device(); t.b.test('update_password', '');
      act(t, { do: 'nav.open', screen: 'settings' });
      for (let i = 0; i < 12; i++) { t.drag({ at: [t.b.S * 0.02, t.b.S * 0.8] }, { by: [0, -t.b.S * 0.25] }); t.wait(300); }
      t.has(t.texts(), 'Report a problem', 'scrolled to the end');
      t.hasNot(t.texts(), 'Update password', 'no row');
    } },
    { name: 'Update password: hidden until held, then shown', async run(t) {
      await t.device(); t.b.test('update_password', 'abcdefghjkmnpqrs');
      act(t, { do: 'nav.open', screen: 'settings' });
      for (let i = 0; i < 12; i++) { t.drag({ at: [t.b.S * 0.02, t.b.S * 0.8] }, { by: [0, -t.b.S * 0.25] }); t.wait(300); }
      t.has(t.texts(), 'Update password', 'the row');
      t.has(t.texts(), 'Hold to show', 'hidden');
      t.hasNot(t.texts(), 'abcdefghjkmnpqrs', 'not shown before a hold');
      t.tap('Hold to show'); t.wait(300);
      t.hasNot(t.texts(), 'abcdefghjkmnpqrs', 'a tap does not show it');
      t.hold('Hold to show', 1200); t.wait(300);
      t.has(t.texts(), 'abcdefghjkmnpqrs', 'shown after a hold');
      t.b.test('update_password', '');
    } },
    { name: 'Update: the row follows the host while Settings shows', async run(t) {
      await t.device(); t.b.test('update', 'none,0.5.0,0.5.0');
      act(t, { do: 'nav.open', screen: 'settings' });
      t.b.test('update', 'available,0.5.0,0.6.0'); t.wait(1500);
      t.has(t.texts(), '0.6.0 available', 'now available');
    } },
  ],
};
