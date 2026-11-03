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
  ],
};
