// Unit test for the patched ESP Web Tools flash step (public/vendor/esp-web-tools-10.4.0-facet.1/facet-flash.js):
// MD5, partition table + erase plan (synthetic images for both boards' tables, plus the real factory images when
// public/firmware/ has them), and the 460800 -> 115200 fallback with a fake esptool-js loader. No hardware.
// Run: node site/scripts/test-install-flash.mjs
import { createHash } from 'node:crypto';
import { existsSync, readdirSync, readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const SITE = join(dirname(fileURLToPath(import.meta.url)), '..');
const { md5hex, parsePartitionTable, planErase, facetFlash, FAST_BAUD, SAFE_BAUD } =
  await import(join(SITE, 'public/vendor/esp-web-tools-10.4.0-facet.1/facet-flash.js'));

let fails = 0;
const ok = (cond, what) => { if (!cond) fails++; process.stdout.write(`${cond ? 'ok  ' : 'FAIL'} ${what}\n`); };
// the flash step logs to the console like upstream; keep the test output to the checks
for (const k of ['log', 'info', 'warn', 'error']) console[k] = () => {};

// ---- MD5 ----
for (const n of [0, 1, 55, 56, 63, 64, 65, 1000, 100003]) {
  const d = new Uint8Array(n).map((_, i) => (i * 31 + 7) & 255);
  ok(md5hex(d) === createHash('md5').update(d).digest('hex'), `md5 ${n} bytes`);
}

// ---- synthetic factory image: bootloader + table at 0x8000 + app at 0x10000 ----
const entry = (label, type, subtype, offset, size) => {
  const e = new Uint8Array(32); const v = new DataView(e.buffer);
  e[0] = 0xaa; e[1] = 0x50; e[2] = type; e[3] = subtype; v.setUint32(4, offset, true); v.setUint32(8, size, true);
  for (let i = 0; i < label.length; i++) e[12 + i] = label.charCodeAt(i);
  return e;
};
const image = (table, appLen = 0x2000) => {
  const img = new Uint8Array(0x10000 + appLen).fill(0xff);
  img[0] = 0xe9; img[0x10000] = 0xe9;
  table.forEach((e, i) => img.set(e, 0x8000 + i * 32));
  img.set([0xeb, 0xeb], 0x8000 + table.length * 32);
  return img;
};
// both boards' tables (0.4.0-public.9): Guition 16 MB, Waveshare 32 MB
const guition = [entry('otadata', 1, 0, 0x9000, 0x2000), entry('phy_init', 1, 1, 0xb000, 0x1000),
  entry('app0', 0, 16, 0x10000, 0x7c0000), entry('app1', 0, 17, 0x7d0000, 0x7c0000), entry('nvs', 1, 2, 0xf90000, 0x70000)];
const waveshare = [entry('otadata', 1, 0, 0x9000, 0x2000), entry('phy_init', 1, 1, 0xb000, 0x1000),
  entry('app0', 0, 16, 0x10000, 0xfc0000), entry('app1', 0, 17, 0xfd0000, 0xfc0000), entry('nvs', 1, 2, 0x1f90000, 0x70000)];

ok(parsePartitionTable(image(guition).subarray(0x8000)).length === 5, 'table parses 5 entries');
let p = planErase([{ data: image(guition), address: 0 }]);
ok(!p.full && p.regions.length === 1 && p.regions[0].label === 'nvs' && p.regions[0].offset === 0xf90000 && p.regions[0].size === 0x70000,
  `guition: erase nvs only (${JSON.stringify(p.regions)})`);
p = planErase([{ data: image(waveshare), address: 0 }]);
ok(!p.full && p.regions.length === 1 && p.regions[0].offset === 0x1f90000, 'waveshare: erase nvs only');
// image that stops before otadata (e.g. app-only parts) must wipe otadata + phy too
p = planErase([{ data: image(guition).subarray(0, 0x9000), address: 0 }, { data: new Uint8Array(0x1000), address: 0x10000 }]);
ok(!p.full && p.regions.map((r) => r.label).join() === 'otadata,phy_init,nvs', 'parts not covering otadata: otadata + phy_init + nvs');
p = planErase([{ data: image([...guition, entry('coredump', 1, 3, 0xf80000, 0x10000), entry('spiffs', 1, 0x82, 0xfa0000, 0x10000)]), address: 0 }]);
ok(!p.full && p.regions.map((r) => r.label).join() === 'nvs,coredump,spiffs', 'coredump + filesystem partitions erased too');
ok(planErase([{ data: new Uint8Array(0x20000).fill(0xff), address: 0 }]).full, 'no table: full chip erase');
ok(planErase([{ data: new Uint8Array(0x1000), address: 0x10000 }]).full, 'image without 0x8000: full chip erase');
ok(planErase([{ data: image([entry('nvs', 1, 2, 0xf90100, 0x70000)]), address: 0 }]).full, 'unaligned partition: full chip erase');

// real factory images, if present (untracked public/firmware/)
const fwDir = join(SITE, 'public/firmware');
if (existsSync(fwDir)) for (const board of readdirSync(fwDir)) {
  const mf = join(fwDir, board, 'manifest.json');
  if (!existsSync(mf)) continue;
  const m = JSON.parse(readFileSync(mf, 'utf8'));
  const files = m.builds[0].parts.map((x) => ({ data: new Uint8Array(readFileSync(join(fwDir, board, x.path))), address: x.offset }));
  const plan = planErase(files);
  ok(!plan.full && plan.regions.some((r) => r.label === 'nvs'), `${board} ${m.version}: ${JSON.stringify(plan.regions ?? plan)}`);
}

// ---- fallback with a fake loader ----
const BIN = image(guition);
globalThis.fetch = async () => ({ ok: true, arrayBuffer: async () => BIN.slice().buffer });
globalThis.location = new URL('https://example.test/install-test/');
const manifest = { name: 'x', builds: [{ chipFamily: 'ESP32-S3', parts: [{ path: 'f.bin', offset: 0 }] }] };

// script: { failInitAt: Set<baud>, failWriteAt: Set<baud>, md5Bad: Set<baud> }
function rig(script = {}) {
  const log = [];
  class Transport { constructor() { this.open = false; } async disconnect() { this.open = false; log.push('disconnect'); } }
  class ESPLoader {
    constructor({ baudrate }) { this.baud = baudrate; this.chip = { CHIP_NAME: 'ESP32-S3' }; this.DEFAULT_TIMEOUT = 3000;
      this.ERASE_REGION_TIMEOUT_PER_MB = 30000; this.ESP_ERASE_REGION = 209; log.push(`loader ${baudrate}`); }
    async main() { if (script.failInitAt?.has(this.baud)) throw new Error('sync failed'); }
    async flashId() {}
    _intToByteArray(n) { return [n & 255, (n >> 8) & 255, (n >> 16) & 255, (n >> 24) & 255]; }
    timeoutPerMb(ms, size) { return (ms * size) / 1e6; }
    async checkCommand(what, op, data) { const v = new DataView(data.buffer); log.push(`erase 0x${v.getUint32(0, true).toString(16)}+0x${v.getUint32(4, true).toString(16)}`); }
    async eraseFlash() { log.push('chip erase'); }
    async writeFlash(o) {
      if (script.failWriteAt?.has(this.baud)) throw new Error('timeout');
      const want = o.calculateMD5Hash(o.fileArray[0].data);
      const got = script.md5Bad?.has(this.baud) ? '00' : md5hex(o.fileArray[0].data);
      if (want !== got) throw new Error('MD5 of file does not match data in flash!');
      o.reportProgress(0, 50, 100); o.reportProgress(0, 100, 100);
      log.push(`write ${this.baud}`);
    }
  }
  const hardReset = async () => log.push('reset');
  return { deps: { Transport, ESPLoader, hardReset }, log };
}
const port = { getInfo: () => ({ usbVendorId: 0x1a86, usbProductId: 0x7523 }) }; // CH340
async function run(script, erase = true, opts = {}) {
  const { deps, log } = rig(script);
  const states = [];
  await facetFlash(deps, (s) => states.push(s), port, '/firmware/x/manifest.json', manifest, erase, opts);
  return { log, states, last: states.at(-1) };
}

let r = await run({});
ok(r.last.state === 'finished' && r.log.join('|') === `loader ${FAST_BAUD}|erase 0xf90000+0x70000|write ${FAST_BAUD}|reset|disconnect`,
  `fast path: ${r.log.join('|')}`);
ok(r.states.some((s) => s.state === 'writing' && s.details.percentage === 50), 'progress reported');
r = await run({ failInitAt: new Set([FAST_BAUD]) });
ok(r.last.state === 'finished' && r.log.includes(`loader ${SAFE_BAUD}`) && r.log.includes(`write ${SAFE_BAUD}`), `init fails fast: retried at 115200 (${r.log.join('|')})`);
r = await run({ failWriteAt: new Set([FAST_BAUD]) });
ok(r.last.state === 'finished' && r.log.filter((x) => x.startsWith('erase')).length === 2 && r.log.includes(`write ${SAFE_BAUD}`), 'write fails fast: erase + write again at 115200');
r = await run({ md5Bad: new Set([FAST_BAUD]) });
ok(r.last.state === 'finished' && r.log.includes(`write ${SAFE_BAUD}`), 'MD5 mismatch fast: retried at 115200');
r = await run({ failWriteAt: new Set([FAST_BAUD, SAFE_BAUD]) });
ok(r.last.state === 'error' && r.last.details.error === 'write_failed' && !r.states.some((s) => s.state === 'finished'), 'both fail: write_failed error');
r = await run({ failInitAt: new Set([FAST_BAUD, SAFE_BAUD]) });
ok(r.last.state === 'error' && r.last.details.error === 'failed_initialize', 'init fails both: failed_initialize error');
r = await run({ failInitAt: new Set([SAFE_BAUD]) }, true, { baud: SAFE_BAUD });
ok(r.last.state === 'error' && r.log.filter((x) => x.startsWith('loader')).length === 1, 'started at 115200: no second try');
r = await run({}, false);
ok(r.last.state === 'finished' && !r.log.some((x) => x.startsWith('erase') || x === 'chip erase'), 'erase unticked: nothing erased');

process.stdout.write(fails ? `${fails} FAILED\n` : 'all passed\n');
process.exit(fails ? 1 : 0);
