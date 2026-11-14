// The panel's editor files (fw_web.bin, workbench/scripts/editor-device.mjs) appended to the firmware images after
// `esphome compile`, so they stay in flash: linked into rodata they were copied to PSRAM at boot on a board with
// CONFIG_SPIRAM_RODATA (the Guition: ~1.4 MB, #185). The panel reads them from its own app partition when it serves
// them (firmware/fw_device.h "web UI").
//   node firmware/web_append.mjs <fw_web.bin> <image.bin> [<image.bin> ...]
// Each image (the app .bin, firmware.ota.bin, firmware.factory.bin: the factory image ends with the app, at a 4 KB
// aligned offset, so the same rule holds): zero-padded to a 4 KB boundary, then a 16-byte header {"FWWB", len, sum,
// 0} (little endian; sum = FNV-1a 32 of the blob, the same as fw_web_gen.h BLOB_SUM) and the blob. Run again on an
// image that already has one (an unchanged build left the old image): the old blob is cut off first. The OTA and the
// web installer write the whole file; the bootloader and the OTA check read only the app image in front of it.
import { readFileSync, writeFileSync } from 'node:fs';

const [blobPath, ...images] = process.argv.slice(2);
if (!blobPath || !images.length) { console.error('usage: web_append.mjs <fw_web.bin> <image.bin> ...'); process.exit(2); }
const blob = readFileSync(blobPath);
export const fnv1a = (b) => { let h = 0x811c9dc5; for (const x of b) h = Math.imul(h ^ x, 0x01000193) >>> 0; return h; };
const head = Buffer.alloc(16);
head.write('FWWB', 0, 'latin1');
head.writeUInt32LE(blob.length, 4);
head.writeUInt32LE(fnv1a(blob), 8);
const PAGE = 4096;
for (const f of images) {
  let img = readFileSync(f);
  // an earlier append: a header on a 4 KB boundary whose blob ends the file
  for (let o = Math.floor((img.length - 16) / PAGE) * PAGE; o > 0; o -= PAGE) {
    if (img.toString('latin1', o, o + 4) === 'FWWB' && o + 16 + img.readUInt32LE(o + 4) === img.length) { img = img.subarray(0, o); break; }
    if (img.length - o > blob.length + 2 * PAGE + 16) break;  // past where an old blob could start
  }
  const pad = (PAGE - (img.length % PAGE)) % PAGE;
  writeFileSync(f, Buffer.concat([img, Buffer.alloc(pad), head, blob]));
  console.log(`web_append: ${f}: editor files at ${img.length + pad} (+${blob.length} bytes)`);
}
