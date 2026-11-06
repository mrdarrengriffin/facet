// Settings > Report a problem (screens/report.cpp): on Wi-Fi a "Report page" row with the panel's own report page
// (GET /fw/report, firmware/report.html) to open on a computer; not on Wi-Fi yet, no address and a line that says so.
const ON = { version: '0.5.0-beta.7', board: 'guition-4848s040', page: 'http://192.168.1.50/link' };
const OFF = { version: '0.5.0-beta.7', board: 'waveshare-p4-86' };

export default {
  id: 'report', family: 'device', about: 'Report a problem: the address of the panel\'s own report page, as a settings row.',
  tests: [
    { name: 'on Wi-Fi: the Report page row with the address, as typed; no QR code', mount: ['report', ON], run(t) {
      const x = t.texts();
      t.has(x, 'To report a problem, open this address on a computer on the same Wi-Fi. It has the report and the steps.', 'what to do');
      const r = t.part('list_row', 'Report page');
      t.has(r.texts, 'http://192.168.1.50/fw/report', 'the address in the row, not in capitals');
      t.ok(!t.comps().some((c) => c.id === 'qr_block'), 'no QR code');
      t.has(x, '0.5.0-beta.7', 'the version');
      t.hasNot(x, /github/i, 'no GitHub address on the glass');
    } },
    { name: 'no Wi-Fi yet: no address, a line that says so', mount: ['report', OFF], run(t) {
      const x = t.texts();
      t.has(x, /Wi-Fi first/, 'says why there is no address');
      t.hasNot(x, /\/fw\/report/, 'no report address');
      t.hasNot(x, 'Report page', 'no row');
      t.ok(!t.comps().some((c) => c.id === 'qr_block'), 'no QR code');
    } },
  ],
};
