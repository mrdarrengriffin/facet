// Facet patch for ESP Web Tools 10.4.0: replaces the flash() step of the install dialog (see PATCH.md).
// Same flow and states as upstream src/flash.ts, plus:
//  1. flashes at FAST_BAUD (460800) and, if connecting/switching speed or writing fails, starts again once at 115200;
//  2. "Erase device" erases only what the new image does not write (every data partition outside the written range:
//     NVS, otadata, phy_init, coredump, ...), read from the partition table inside the image. No table found = full
//     chip erase, as upstream;
//  3. checks each written part with the flasher stub's MD5 (upstream does not), so a bad fast write is caught and
//     retried at 115200.
// Compression stays on (esptool-js deflate). Pure helpers are exported for the unit test (site/scripts/test-install-flash.mjs).

export const FAST_BAUD = 460800;
export const SAFE_BAUD = 115200;
export const PART_TABLE_OFFSET = 0x8000;
const SECTOR = 0x1000;

// ---- partition table -------------------------------------------------------------------------------------------
// ESP-IDF table: 32-byte entries, magic AA 50, type, subtype, offset u32, size u32, label[16], flags u32; ends at an
// MD5 entry (EB EB) or blank flash (FF FF). Returns [] when the bytes are not a table.
export function parsePartitionTable(bytes) {
  const out = [];
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  for (let i = 0; i + 32 <= bytes.length && i < 0xc00; i += 32) {
    if (bytes[i] !== 0xaa || bytes[i + 1] !== 0x50) break;
    let label = '';
    for (let k = 12; k < 28 && bytes[i + k]; k++) label += String.fromCharCode(bytes[i + k]);
    out.push({ label, type: bytes[i + 2], subtype: bytes[i + 3], offset: view.getUint32(i + 4, true), size: view.getUint32(i + 8, true) });
  }
  return out;
}

// Finds the table inside the parts being written (the factory image holds it at 0x8000).
export function findPartitionTable(fileArray) {
  for (const f of fileArray) {
    const rel = PART_TABLE_OFFSET - f.address;
    if (rel >= 0 && rel + 32 <= f.data.length) {
      const parts = parsePartitionTable(f.data.subarray(rel, rel + 0xc00));
      if (parts.length) return parts;
    }
  }
  return null;
}

// What "Erase device" has to wipe so the result matches a full chip erase for the firmware: every DATA partition
// (type 1: nvs, otadata, phy_init, coredump, nvs_keys, spiffs/littlefs/fat ...) not fully rewritten by the image.
// The written ranges themselves are erased by the write (flash_defl_begin). App slots that are not written keep their
// old bytes but cannot boot: otadata is wiped/rewritten, so the bootloader starts the first app slot (the new image).
// Returns { full: true } (fall back to chip erase) or { full: false, regions: [{label, offset, size}] }.
export function planErase(fileArray) {
  const table = findPartitionTable(fileArray);
  if (!table) return { full: true, reason: 'no partition table in the image' };
  const written = fileArray.map((f) => [f.address, f.address + f.data.length]);
  const covered = (a, b) => written.some(([s, e]) => s <= a && b <= e);
  const regions = [];
  for (const p of table) {
    if (p.type !== 1) continue;
    if (p.offset % SECTOR || p.size % SECTOR || p.size === 0) return { full: true, reason: `unaligned partition ${p.label}` };
    if (covered(p.offset, p.offset + p.size)) continue;
    regions.push({ label: p.label, offset: p.offset, size: p.size });
  }
  if (!table.some((p) => p.type === 1 && p.subtype === 2)) return { full: true, reason: 'no nvs partition' };
  return { full: false, regions };
}

// ---- MD5 (RFC 1321), for the write check ------------------------------------------------------------------------
const K = new Int32Array(64).map((_, i) => Math.floor(Math.abs(Math.sin(i + 1)) * 2 ** 32) | 0);
const R = [7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21];
export function md5hex(data) {
  const n = data.length;
  const len = ((n + 8) >>> 6) * 64 + 64;
  const buf = new Uint8Array(len);
  buf.set(data);
  buf[n] = 0x80;
  const dv = new DataView(buf.buffer);
  dv.setUint32(len - 8, (n * 8) >>> 0, true);
  dv.setUint32(len - 4, Math.floor(n / 0x20000000), true);
  let a0 = 0x67452301, b0 = 0xefcdab89 | 0, c0 = 0x98badcfe | 0, d0 = 0x10325476;
  const M = new Int32Array(16);
  for (let off = 0; off < len; off += 64) {
    for (let j = 0; j < 16; j++) M[j] = dv.getInt32(off + j * 4, true);
    let a = a0, b = b0, c = c0, d = d0;
    for (let i = 0; i < 64; i++) {
      let f, g;
      if (i < 16) { f = (b & c) | (~b & d); g = i; }
      else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) & 15; }
      else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) & 15; }
      else { f = c ^ (b | ~d); g = (7 * i) & 15; }
      const t = d; d = c; c = b;
      const x = (a + f + K[i] + M[g]) | 0, s = R[(i >> 4) * 4 + (i & 3)];
      b = (b + ((x << s) | (x >>> (32 - s)))) | 0;
      a = t;
    }
    a0 = (a0 + a) | 0; b0 = (b0 + b) | 0; c0 = (c0 + c) | 0; d0 = (d0 + d) | 0;
  }
  const out = new DataView(new ArrayBuffer(16));
  [a0, b0, c0, d0].forEach((v, i) => out.setInt32(i * 4, v, true));
  return Array.from(new Uint8Array(out.buffer), (x) => x.toString(16).padStart(2, '0')).join('');
}

// ---- the flash step -----------------------------------------------------------------------------------------------
class StageError extends Error {
  constructor(stage, cause) {
    super(cause && cause.message ? cause.message : String(cause));
    this.stage = stage;
    this.cause = cause;
  }
}

async function eraseRegion(loader, offset, size) {
  const data = new Uint8Array([...loader._intToByteArray(offset), ...loader._intToByteArray(size)]);
  const timeout = Math.max(loader.DEFAULT_TIMEOUT, loader.timeoutPerMb(loader.ERASE_REGION_TIMEOUT_PER_MB, size));
  await loader.checkCommand('erase region', loader.ESP_ERASE_REGION, data, undefined, undefined, timeout);
}

// deps = { Transport, ESPLoader, hardReset } from the bundle; the rest are upstream flash()'s arguments.
// opts.baud: first speed to try (default FAST_BAUD; the test page passes ?baud=).
export async function facetFlash(deps, onEvent, port, manifestPath, manifest, eraseFirst, opts = {}) {
  const { Transport, ESPLoader, hardReset } = deps;
  const first = opts.baud || globalThis.FACET_FLASH_BAUD || FAST_BAUD;
  const t0 = Date.now();
  const report = { baud: first, fallback: null, erase: null, seconds: null, ok: false };
  let build, chipFamily, files = null, totalSize = 0;
  const fire = (u) => onEvent({ ...u, manifest, build, chipFamily });
  const finish = (ok) => {
    report.ok = ok;
    report.seconds = Math.round((Date.now() - t0) / 100) / 10;
    console.info('[facet-flash]', JSON.stringify(report));
    try { globalThis.dispatchEvent?.(new CustomEvent('facet-flash-report', { detail: report })); } catch { /* no window */ }
  };
  const closeAll = async (transport, loader) => {
    try { await hardReset(transport, loader); } catch (e) { console.warn('[facet-flash] reset', e); }
    try { await transport.disconnect(); } catch (e) { console.warn('[facet-flash] disconnect', e); }
  };

  const attempt = async (baud) => {
    const transport = new Transport(port);
    const info = port.getInfo();
    const isCdc = info && info.usbVendorId === 0x303a && info.usbProductId !== undefined &&
      [0x1001, 0x1002, 0x1003, 0x0002, 0x0003].includes(info.usbProductId);
    const loader = new ESPLoader({ transport, baudrate: baud, enableTracing: false });
    globalThis.esploader = loader;
    fire({ state: 'initializing', message: baud === SAFE_BAUD ? 'Initializing...' : `Initializing (${baud} baud)...`, details: { done: false } });
    try {
      await loader.main(); // connects at 115200, loads the stub, then switches to `baud`
      await loader.flashId();
    } catch (e) {
      throw Object.assign(new StageError('init', e), { transport, loader });
    }
    chipFamily = loader.chip.CHIP_NAME;
    fire({ state: 'initializing', message: `Initialized. Found ${chipFamily}`, details: { done: true } });
    const serialType = isCdc ? 'cdc' : 'uart';
    build = manifest.builds.find((b) => b.chipFamily === chipFamily && b.serialType === serialType) ||
      manifest.builds.find((b) => b.chipFamily === chipFamily && b.serialType === undefined);
    if (!build) throw Object.assign(new StageError('not_supported', new Error(chipFamily)), { transport, loader });

    if (!files) {
      fire({ state: 'preparing', message: 'Preparing installation...', details: { done: false } });
      const base = manifestPath.startsWith('blob:') || manifestPath.startsWith('data:')
        ? location.toString() : new URL(manifestPath, location.toString()).toString();
      try {
        files = await Promise.all(build.parts.map(async (part) => {
          const resp = await fetch(new URL(part.path, base).toString());
          if (!resp.ok) throw new Error(`Downloading firmware ${part.path} failed: ${resp.status}`);
          return { data: new Uint8Array(await resp.arrayBuffer()), address: part.offset };
        }));
      } catch (e) {
        files = null;
        throw Object.assign(new StageError('download', e), { transport, loader });
      }
      totalSize = files.reduce((n, f) => n + f.data.length, 0);
      fire({ state: 'preparing', message: 'Installation prepared', details: { done: true } });
    }

    try {
      if (eraseFirst) {
        const plan = planErase(files);
        report.erase = plan.full ? { full: true, reason: plan.reason } : plan.regions.map((r) => `${r.label}@0x${r.offset.toString(16)}+0x${r.size.toString(16)}`);
        fire({ state: 'erasing', message: 'Erasing device...', details: { done: false } });
        if (plan.full) await loader.eraseFlash();
        else for (const r of plan.regions) await eraseRegion(loader, r.offset, r.size);
        fire({ state: 'erasing', message: 'Device erased', details: { done: true } });
      }
      fire({ state: 'writing', message: 'Writing progress: 0%', details: { bytesTotal: totalSize, bytesWritten: 0, percentage: 0 } });
      let totalWritten = 0;
      await loader.writeFlash({
        fileArray: files.map((f) => ({ data: f.data, address: f.address })),
        flashSize: 'keep', flashMode: 'keep', flashFreq: 'keep', eraseAll: false, compress: true,
        calculateMD5Hash: md5hex, // stub MD5 of what landed in flash must match, or writeFlash throws
        reportProgress: (i, written, total) => {
          const uncompressed = (written / total) * files[i].data.length;
          const pct = Math.floor(((totalWritten + uncompressed) / totalSize) * 100);
          if (written === total) { totalWritten += uncompressed; return; }
          fire({ state: 'writing', message: `Writing progress: ${pct}%`, details: { bytesTotal: totalSize, bytesWritten: totalWritten + written, percentage: pct } });
        },
      });
    } catch (e) {
      throw Object.assign(new StageError('write', e), { transport, loader });
    }
    fire({ state: 'writing', message: 'Writing complete', details: { bytesTotal: totalSize, bytesWritten: totalSize, percentage: 100 } });
    await hardReset(transport, loader);
    await transport.disconnect();
  };

  const fail = (err) => {
    const map = {
      init: ['failed_initialize', 'Failed to initialize. Try resetting your device or holding the BOOT button while clicking INSTALL.'],
      not_supported: ['not_supported', `Your ${chipFamily} board is not supported.`],
      download: ['failed_firmware_download', err.message],
      write: ['write_failed', err.message],
    };
    const [code, message] = map[err.stage] || ['write_failed', err.message];
    fire({ state: 'error', message, details: { error: code, details: err.stage === 'download' ? err.message : err.cause } });
  };

  try {
    await attempt(first);
  } catch (err) {
    console.error('[facet-flash]', err.stage, err.cause || err);
    if (err.transport) await closeAll(err.transport, err.loader);
    const retry = first !== SAFE_BAUD && (err.stage === 'init' || err.stage === 'write');
    if (!retry) { fail(err); finish(false); return; }
    report.fallback = { from: first, stage: err.stage, error: String(err.message).slice(0, 200) };
    report.baud = SAFE_BAUD;
    fire({ state: 'initializing', message: `Retrying at ${SAFE_BAUD} baud...`, details: { done: false } });
    try {
      await attempt(SAFE_BAUD);
    } catch (err2) {
      console.error('[facet-flash] retry', err2.stage, err2.cause || err2);
      if (err2.transport) await closeAll(err2.transport, err2.loader);
      fail(err2); finish(false); return;
    }
  }
  console.log('DISCONNECT');
  fire({ state: 'finished', message: 'All done!' });
  finish(true);
}
