// Firmware update view (components/fw_update_view.cpp, facet-196): the 2 s notice before the screen turns off for a
// firmware install (firmware/fw_device.h ota_*), and the failure state. Each says what is happening; no taps react.
export default {
  id: 'fw_update_view', family: 'components', about: 'Firmware update notice: updating (screen turns off next), failed.',
  tests: [
    { name: 'updating says the screen turns off and not to unplug, with the version', mount: ['fw_update_view', { version: '0.4.0.2610261200' }], run(t) {
      const tx = t.texts();
      t.has(tx, /^Updating$/, 'title'); t.has(tx, '0.4.0.2610261200', 'version'); t.has(tx, /turns off until it restarts\. Don't unplug\./, 'notice');
    } },
    { name: 'no version: the line is left out', mount: ['fw_update_view', { version: '' }], run(t) {
      t.ok(!t.texts().some((s) => /^\d+\.\d+/.test(s)), 'no version line');
    } },
    { name: 'failed says so and keeps the old version', mount: ['fw_update_view', { state: 'failed' }], run(t) {
      const tx = t.texts(); t.has(tx, /^Update failed$/, 'title'); t.has(tx, /keeps the version it has/, 'reassurance');
    } },
    { name: 'updating -> failed by update, with a note', mount: ['fw_update_view', {}], run(t) {
      t.update({ state: 'failed', note: 'The download stopped.' }); t.wait(600);
      const tx = t.texts(); t.has(tx, /^Update failed$/, 'title'); t.has(tx, /The download stopped\./, 'note');
    } },
    { name: 'a tap does nothing', mount: ['fw_update_view', {}], run(t) {
      const ev = t.tap(/Updating/); t.eq(ev.filter((e) => e.kind === 'event' && e.name !== 'screen').length, 0, 'events'); t.silent(ev);
    } },
  ],
};
