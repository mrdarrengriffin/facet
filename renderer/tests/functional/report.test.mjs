// Settings > Report a problem (screens/report.cpp): on Wi-Fi the QR code and the address under it open the panel's
// own report page (GET /fw/report, firmware/report.html); not on Wi-Fi yet, the code opens a GitHub bug report instead.
const ON = { version: '0.5.0-beta.7', board: 'guition-4848s040', page: 'http://192.168.1.50/link' };
const OFF = { version: '0.5.0-beta.7', board: 'waveshare-p4-86' };

export default {
  id: 'report', family: 'device', about: 'Report a problem: a QR code and address to the panel\'s own report page.',
  tests: [
    { name: 'on Wi-Fi: the report page address, as typed, under the code', mount: ['report', ON], run(t) {
      const x = t.texts();
      t.has(x, 'Scan with your phone, or open this address on a computer on the same Wi-Fi.', 'what to do');
      t.has(x, 'http://192.168.1.50/fw/report', 'the report page address, not in capitals');
      t.has(x, /^open on your phone$/i, 'the code\'s caption');
      t.has(x, '0.5.0-beta.7', 'the version');
      t.hasNot(x, /github/i, 'no GitHub address on the glass');
    } },
    { name: 'no Wi-Fi yet: the code opens a bug report instead', mount: ['report', OFF], run(t) {
      const x = t.texts();
      t.has(x, /isn't on Wi-Fi yet/, 'says why there is no report page');
      t.hasNot(x, /\/fw\/report/, 'no report address');
      t.has(x, /^open on your phone$/i, 'the code\'s caption');
    } },
  ],
};
