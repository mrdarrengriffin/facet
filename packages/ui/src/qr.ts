// A small QR code encoder (byte mode, error correction L or M, versions 1-10, so up to 213 bytes at M), following
// Nayuki's reference algorithm. Used by qrBlock() for "open on your phone" links.

type Ecl = 'L' | 'M';
const ECC_PER_BLOCK: Record<Ecl, number[]> = {
  L: [-1, 7, 10, 15, 20, 26, 18, 20, 24, 30, 18],
  M: [-1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26],
};
const NUM_BLOCKS: Record<Ecl, number[]> = {
  L: [-1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4],
  M: [-1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5],
};
const FORMAT_BITS: Record<Ecl, number> = { L: 1, M: 0 };
const MAX_VER = 10;

const rawModules = (v: number) => {
  let r = (16 * v + 128) * v + 64;
  if (v >= 2) {
    const n = Math.floor(v / 7) + 2;
    r -= (25 * n - 10) * n - 55;
    if (v >= 7) r -= 36;
  }
  return r;
};
const dataCodewords = (v: number, e: Ecl) =>
  Math.floor(rawModules(v) / 8) - ECC_PER_BLOCK[e][v] * NUM_BLOCKS[e][v];

function gfMul(x: number, y: number) {
  let z = 0;
  for (let i = 7; i >= 0; i--) {
    z = (z << 1) ^ ((z >>> 7) * 0x11d);
    z ^= ((y >>> i) & 1) * x;
  }
  return z;
}
function rsDivisor(deg: number) {
  const r = new Array(deg).fill(0);
  r[deg - 1] = 1;
  let root = 1;
  for (let i = 0; i < deg; i++) {
    for (let j = 0; j < r.length; j++) {
      r[j] = gfMul(r[j], root);
      if (j + 1 < r.length) r[j] ^= r[j + 1];
    }
    root = gfMul(root, 0x02);
  }
  return r;
}
function rsRemainder(data: number[], div: number[]) {
  const r = div.map(() => 0);
  for (const b of data) {
    const f = b ^ (r.shift() as number);
    r.push(0);
    div.forEach((c, i) => (r[i] ^= gfMul(c, f)));
  }
  return r;
}

export type Qr = { size: number; version: number; mask: number; modules: boolean[][] };

export function qrEncode(text: string, o: { ecl?: Ecl; mask?: number; version?: number } = {}): Qr {
  const ecl = o.ecl ?? 'M';
  const bytes = Array.from(new TextEncoder().encode(text));
  let v = o.version ?? 1;
  for (; v <= MAX_VER; v++)
    if (4 + (v < 10 ? 8 : 16) + bytes.length * 8 <= dataCodewords(v, ecl) * 8) break;
  if (v > MAX_VER) throw new Error(`QR: text too long (${bytes.length} bytes)`);
  // data bits: mode 0100 (byte), count, bytes, terminator, pad
  const bits: number[] = [];
  const put = (val: number, n: number) => {
    for (let i = n - 1; i >= 0; i--) bits.push((val >>> i) & 1);
  };
  put(4, 4);
  put(bytes.length, v < 10 ? 8 : 16);
  bytes.forEach((b) => put(b, 8));
  const cap = dataCodewords(v, ecl) * 8;
  put(0, Math.min(4, cap - bits.length));
  put(0, (8 - (bits.length % 8)) % 8);
  for (let p = 0xec; bits.length < cap; p ^= 0xec ^ 0x11) put(p, 8);
  const data: number[] = [];
  for (let i = 0; i < bits.length; i += 8) data.push(parseInt(bits.slice(i, i + 8).join(''), 2));
  // blocks + ECC, interleaved
  const nb = NUM_BLOCKS[ecl][v],
    ecc = ECC_PER_BLOCK[ecl][v],
    raw = Math.floor(rawModules(v) / 8);
  const nShort = nb - (raw % nb),
    shortLen = Math.floor(raw / nb),
    div = rsDivisor(ecc);
  const blocks: number[][] = [];
  for (let i = 0, k = 0; i < nb; i++) {
    const dat = data.slice(k, k + shortLen - ecc + (i < nShort ? 0 : 1));
    k += dat.length;
    const e = rsRemainder(dat, div);
    if (i < nShort) dat.push(0);
    blocks.push(dat.concat(e));
  }
  const cw: number[] = [];
  for (let i = 0; i < blocks[0].length; i++)
    blocks.forEach((b, j) => {
      if (i !== shortLen - ecc || j >= nShort) cw.push(b[i]);
    });

  const size = v * 4 + 17;
  const m: boolean[][] = Array.from({ length: size }, () => new Array(size).fill(false));
  const fn: boolean[][] = Array.from({ length: size }, () => new Array(size).fill(false));
  const set = (x: number, y: number, d: boolean) => {
    m[y][x] = d;
    fn[y][x] = true;
  };
  for (let i = 0; i < size; i++) {
    set(6, i, i % 2 === 0);
    set(i, 6, i % 2 === 0);
  }
  const finder = (cx: number, cy: number) => {
    for (let dy = -4; dy <= 4; dy++)
      for (let dx = -4; dx <= 4; dx++) {
        const d = Math.max(Math.abs(dx), Math.abs(dy)),
          x = cx + dx,
          y = cy + dy;
        if (x >= 0 && x < size && y >= 0 && y < size) set(x, y, d !== 2 && d !== 4);
      }
  };
  finder(3, 3);
  finder(size - 4, 3);
  finder(3, size - 4);
  const align: number[] = [];
  if (v > 1) {
    const n = Math.floor(v / 7) + 2,
      step = Math.ceil((v * 4 + 4) / (n * 2 - 2)) * 2;
    align.push(6);
    for (let p = size - 7; align.length < n; p -= step) align.splice(1, 0, p);
  }
  align.forEach((ax, i) =>
    align.forEach((ay, j) => {
      if (
        (i === 0 && j === 0) ||
        (i === 0 && j === align.length - 1) ||
        (i === align.length - 1 && j === 0)
      )
        return;
      for (let dy = -2; dy <= 2; dy++)
        for (let dx = -2; dx <= 2; dx++)
          set(ax + dx, ay + dy, Math.max(Math.abs(dx), Math.abs(dy)) !== 1);
    }),
  );
  const format = (mask: number) => {
    const d = (FORMAT_BITS[ecl] << 3) | mask;
    let r = d;
    for (let i = 0; i < 10; i++) r = (r << 1) ^ ((r >>> 9) * 0x537);
    const b = ((d << 10) | r) ^ 0x5412,
      bit = (i: number) => ((b >>> i) & 1) !== 0;
    for (let i = 0; i <= 5; i++) set(8, i, bit(i));
    set(8, 7, bit(6));
    set(8, 8, bit(7));
    set(7, 8, bit(8));
    for (let i = 9; i < 15; i++) set(14 - i, 8, bit(i));
    for (let i = 0; i < 8; i++) set(size - 1 - i, 8, bit(i));
    for (let i = 8; i < 15; i++) set(8, size - 15 + i, bit(i));
    set(8, size - 8, true);
  };
  format(0); // reserve the format areas
  if (v >= 7) {
    let r = v;
    for (let i = 0; i < 12; i++) r = (r << 1) ^ ((r >>> 11) * 0x1f25);
    const b = (v << 12) | r;
    for (let i = 0; i < 18; i++) {
      const d = ((b >>> i) & 1) !== 0,
        a = size - 11 + (i % 3),
        c = Math.floor(i / 3);
      set(a, c, d);
      set(c, a, d);
    }
  }
  // codewords, zig-zag
  let i = 0;
  for (let right = size - 1; right >= 1; right -= 2) {
    if (right === 6) right = 5;
    for (let vert = 0; vert < size; vert++)
      for (let j = 0; j < 2; j++) {
        const x = right - j,
          up = ((right + 1) & 2) === 0,
          y = up ? size - 1 - vert : vert;
        if (!fn[y][x] && i < cw.length * 8) {
          m[y][x] = ((cw[i >>> 3] >>> (7 - (i & 7))) & 1) !== 0;
          i++;
        }
      }
  }
  const inv = (k: number, x: number, y: number) =>
    [
      (x + y) % 2 === 0,
      y % 2 === 0,
      x % 3 === 0,
      (x + y) % 3 === 0,
      (Math.floor(x / 3) + Math.floor(y / 2)) % 2 === 0,
      ((x * y) % 2) + ((x * y) % 3) === 0,
      (((x * y) % 2) + ((x * y) % 3)) % 2 === 0,
      (((x + y) % 2) + ((x * y) % 3)) % 2 === 0,
    ][k];
  const applyMask = (k: number) => {
    for (let y = 0; y < size; y++)
      for (let x = 0; x < size; x++) if (!fn[y][x] && inv(k, x, y)) m[y][x] = !m[y][x];
  };
  // mask choice: simplified penalty (runs of 5+, 2x2 blocks, dark balance); any mask decodes, this only helps scanners
  const penalty = () => {
    let p = 0,
      dark = 0;
    for (let a = 0; a < size; a++) {
      let rr = 1,
        rc = 1;
      for (let b = 1; b < size; b++) {
        if (m[a][b] === m[a][b - 1]) {
          if (++rr === 5) p += 3;
          else if (rr > 5) p++;
        } else rr = 1;
        if (m[b][a] === m[b - 1][a]) {
          if (++rc === 5) p += 3;
          else if (rc > 5) p++;
        } else rc = 1;
      }
    }
    for (let y = 0; y < size; y++)
      for (let x = 0; x < size; x++) {
        if (m[y][x]) dark++;
        if (
          x < size - 1 &&
          y < size - 1 &&
          m[y][x] === m[y][x + 1] &&
          m[y][x] === m[y + 1][x] &&
          m[y][x] === m[y + 1][x + 1]
        )
          p += 3;
      }
    return p + Math.floor(Math.abs(dark * 20 - size * size * 10) / (size * size)) * 10;
  };
  let mask = o.mask ?? -1;
  if (mask < 0) {
    let best = Infinity;
    for (let k = 0; k < 8; k++) {
      applyMask(k);
      format(k);
      const p = penalty();
      if (p < best) {
        best = p;
        mask = k;
      }
      applyMask(k);
    }
  }
  applyMask(mask);
  format(mask);
  return { size, version: v, mask, modules: m };
}
