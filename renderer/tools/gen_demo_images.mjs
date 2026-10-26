// Made-up demo pictures for the Image component (workbench, docs, functional tests): drawn here, no third-party art.
// node renderer/tools/gen_demo_images.mjs -> site/public/demo/images/*.png (served on the docs site; the hub proxy
// serves the same files to the workbench before they are published). Deterministic: same bytes every run.
import { writeFileSync, mkdirSync } from 'node:fs';
import { deflateSync } from 'node:zlib';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
const OUT = join(resolve(dirname(fileURLToPath(import.meta.url)), '../..'), 'site/public/demo/images');
const crcT = Array.from({ length: 256 }, (_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
const crc = (b) => { let c = ~0; for (const x of b) c = crcT[(c ^ x) & 255] ^ (c >>> 8); return ~c >>> 0; };
const chunk = (t, d) => { const l = Buffer.alloc(4); l.writeUInt32BE(d.length); const td = Buffer.concat([Buffer.from(t), d]); const c = Buffer.alloc(4); c.writeUInt32BE(crc(td)); return Buffer.concat([l, td, c]); };
function png(w, h, px) {  // px(x, y) -> [r, g, b, a]
  const raw = Buffer.alloc((w * 4 + 1) * h);
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) { const p = px(x, y).map((v) => Math.max(0, Math.min(255, Math.round(v)))); p.forEach((v, i) => { raw[y * (w * 4 + 1) + 1 + x * 4 + i] = v; }); }
  const ih = Buffer.alloc(13); ih.writeUInt32BE(w, 0); ih.writeUInt32BE(h, 4); ih[8] = 8; ih[9] = 6;
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ih), chunk('IDAT', deflateSync(raw, { level: 9 })), chunk('IEND', Buffer.alloc(0))]);
}
const mix = (a, b, t) => a.map((v, i) => v + (b[i] - v) * t);
const S = {
  // rolling hills under an evening sky
  hills: [320, 240, (x, y) => {
    let c = mix([250, 196, 140, 255], [92, 120, 196, 255], y / 150);
    if (Math.hypot(x - 230, y - 80) < 26) c = [255, 236, 190, 255];
    if (y > 150 + 18 * Math.sin(x / 40)) c = mix([96, 150, 110, 255], [60, 110, 80, 255], (y - 140) / 100);
    if (y > 185 + 14 * Math.sin(x / 28 + 2)) c = mix([70, 128, 86, 255], [40, 84, 60, 255], (y - 180) / 60);
    return c;
  }],
  // a soft abstract avatar
  avatar: [128, 128, (x, y) => {
    const d = Math.hypot(x - 64, y - 54);
    let c = mix([120, 170, 230, 255], [70, 100, 180, 255], y / 128);
    if (d < 24) c = [250, 222, 196, 255];
    if (Math.hypot(x - 64, y - 128) < 50) c = [244, 140, 90, 255];
    return c;
  }],
  // 16 x 16 pixel-art ghost on a transparent ground
  sprite: [16, 16, (x, y) => {
    const rows = ['................', '.....######.....', '...##########...', '..############..', '..###..###..##..', '.####..###..###.', '.##############.', '.##############.', '.##############.', '.##############.', '.##############.', '.##############.', '.##############.', '.###.####.####..', '.#....##...##...', '................'];
    const ch = rows[y][x];
    if (ch !== '#') return [0, 0, 0, 0];
    return (y === 4 || y === 5) && (x === 5 || x === 6 || x === 10 || x === 11) ? [40, 40, 60, 255] : [180, 210, 255, 255];
  }],
};
// six 32 x 32 pixel-art gems (the grid items demo)
const HUES = [[230, 80, 90], [250, 170, 60], [240, 220, 80], [90, 200, 120], [80, 160, 240], [170, 110, 230]];
HUES.forEach((h, i) => { S[`gem-${i + 1}`] = [32, 32, (x, y) => { const px = Math.floor(x / 2), py = Math.floor(y / 2), d = Math.abs(px - 7.5) + Math.abs(py - 7.5); return d > 7 ? [0, 0, 0, 0] : d > 6 ? mix(h, [0, 0, 0], 0.45).concat(255) : px + py < 12 ? mix(h, [255, 255, 255], 0.35).concat(255) : h.concat(255); }]; });
mkdirSync(OUT, { recursive: true });
for (const [name, [w, h, f]] of Object.entries(S)) writeFileSync(join(OUT, `${name}.png`), png(w, h, f));
console.log(`demo images: ${Object.keys(S).length} -> ${OUT}`);
