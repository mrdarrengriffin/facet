// Toasts from Home Assistant (facet-190): the panel's facet_toast action {title, body, icon, kind, duration ms} puts
// the toast on the facet.toast pair (firmware/fw_device.h toast_from_ha); the device maps it (core/device.cpp
// toast_put, components/toast_layer.cpp push): kind = tone (unknown = info), icon = a Toast icon (unknown or empty =
// the tone's own), duration >= 10 s = urgent (12 s life, else 6 s). It arrives over whatever screen shows.
import { comps } from './_controls.mjs';
const shown = (t, title) => t.texts().includes(title);
// what arrived: the toast event's detail "<tone> <icon> <life>: <title>"
const got = (ev) => ev.filter((e) => e.kind === 'event' && e.name === 'toast').map((e) => e.detail);
export default {
  id: 'toast_ha', family: 'device', about: 'Toasts Home Assistant sends to the panel (facet_toast).',
  tests: [
    { name: 'title + body arrive over Home, with the notify sound', async run(t) {
      await t.device(); const ev = t.toast({ title: 'Washing done', body: 'Empty the machine', kind: 'ok', icon: 'washer', duration: 6000 });
      t.ok(shown(t, 'Washing done'), 'title'); t.ok(shown(t, 'Empty the machine'), 'body');
      t.has(ev.filter((e) => e.kind === 'sound').map((e) => e.name), /notify/, 'notify sound'); t.eq(t.where(), 'home', 'still on Home');
    } },
    { name: 'the same words twice = two toasts (each call is new)', async run(t) {
      await t.device(); t.toast({ title: 'Door', body: 'Someone is there', kind: 'warn' }); t.toast({ title: 'Door', body: 'Someone is there', kind: 'warn' });
      t.eq(t.texts().filter((x) => x === 'Door').length, 2, 'two toasts');
    } },
    { name: 'icon: a known icon is used', async run(t) {
      await t.device(); t.has(got(t.toast({ title: 'Vacuum', body: 'Docked', kind: 'ok', icon: 'vacuum' })), 'ok vacuum normal: Vacuum', 'tone icon life');
    } },
    { name: 'icon fallback: unknown or empty = the tone\'s own (ok done, warn alert, working agent, info bell)', async run(t) {
      await t.device();
      for (const [kind, icon, want] of [['ok', 'mdi:washing-machine', 'done'], ['warn', '', 'alert'], ['working', 'nope', 'agent'], ['info', '', 'bell']]) {
        t.has(got(t.toast({ title: `T ${kind}`, body: 'x', kind, icon })), `${kind} ${want} normal: T ${kind}`, `${kind} + "${icon}"`);
      }
    } },
    { name: 'unknown kind = info (bell)', async run(t) { await t.device(); t.has(got(t.toast({ title: 'Odd', body: 'x', kind: 'party' })), 'info bell normal: Odd', 'info + bell'); } },
    { name: 'duration under 10 s: there at 5 s, gone by 7 s after it arrived', async run(t) {
      await t.device(); const t0 = t.since(); t.has(got(t.toast({ title: 'Short', body: 'x', kind: 'info', duration: 9999 })), 'info bell normal: Short', 'normal under 10 s');
      t.until(t0 + 5000); t.ok(shown(t, 'Short'), 'there at 5 s'); t.until(t0 + 7000); t.ok(!shown(t, 'Short'), 'gone by 7 s');
    } },
    { name: 'duration >= 10 s = urgent: still there at 11 s, gone by 13 s', async run(t) {
      await t.device(); const t0 = t.since(); t.has(got(t.toast({ title: 'Door', body: 'Someone is there', kind: 'warn', duration: 10000 })), 'warn alert urgent: Door', 'urgent');
      t.until(t0 + 11000); t.ok(shown(t, 'Door'), 'there at 11 s'); t.until(t0 + 13000); t.ok(!shown(t, 'Door'), 'gone by 13 s');
    } },
    { name: 'arrives over a screen that is not Home', async run(t) {
      await t.device(); t.show('settings'); const w = t.where(); t.toast({ title: 'Timer', body: 'Pasta', kind: 'ok', icon: 'timer' });
      t.ok(shown(t, 'Timer'), 'shown'); t.eq(t.where(), w, 'screen kept');
    } },
  ],
};
