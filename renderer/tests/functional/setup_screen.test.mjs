// Set up (screens/setup.cpp), the link step: after the panel is added to Home Assistant and while it has no link of
// its own, a QR code to its link page (prop link) and Later; Later says "setup" not-now (the firmware then opens Home).
export default {
  id: 'setup', family: 'screens', about: 'Set up\'s link step: the link page as a QR code, and Later.',
  tests: [
    { name: 'link: the link page\'s code and words, Later = setup not-now', async run(t) {
      await t.mount('setup', { state: 'link', link: 'http://192.168.1.42/link' });
      t.ok(t.texts().includes('Link the screens to Home Assistant'), 'the title');
      t.ok(t.texts().some((s) => /^link page$/i.test(s)), 'the code\'s caption');
      const ev = t.tap(/^later$/i);
      t.emitted(ev, 'setup', 'not-now');
    } },
    { name: 'the address is whole under the code on every step with one (connected, link)', async run(t) {
      for (const state of ['connected', 'link']) {
        await t.mount('setup', { state, ip: '192.168.100.200' });
        t.ok(t.texts().includes('192.168.100.200'), `${state}: the address whole: ${t.texts().join(' / ')}`);
      }
    } },
  ],
};
