// Screen channels (RELEASING.md "Screen channels"): features.json's since fields, and the README pictures' chip.
//   node --test tools/channels.test.mjs   (check.sh: every tier; no build needed)
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { inflateSync } from 'node:zlib';
import { channelErrors, channelOf, channels, cmpVer, panelFeatures, R } from './features.mjs';
import { CHIP, chipBox, chipColours, drawChip } from './lib/chip.mjs';

const man = () => JSON.parse(readFileSync(join(R, 'features.json'), 'utf8'));

test('versions order as semver (a beta before its release)', () => {
  const v = ['0.4.0', '0.5.0-beta.1', '0.5.0-beta.10', '0.5.0-beta.2', '0.5.0', '0.4.1'].sort(cmpVer);
  assert.deepEqual(v, ['0.4.0', '0.4.1', '0.5.0-beta.1', '0.5.0-beta.2', '0.5.0-beta.10', '0.5.0']);
});

test('a channel from since and the stable version', () => {
  assert.equal(channelOf('0.4.0', '0.4.0'), 'stable');
  assert.equal(channelOf('0.5.0-beta.3', '0.4.0'), 'beta');
  assert.equal(channelOf('0.5.0-beta.3', '0.5.0'), 'stable');
  assert.equal(channelOf('next', '0.4.0'), 'soon');
  assert.equal(channelOf(null, '0.4.0', false), 'soon');
});

test('features.json: every released feature has a channel', () => {
  assert.deepEqual(channelErrors(man()), []);
  const m = man();
  delete m.features.find((f) => f.firmware && f.screens?.length).since;
  assert.match(channelErrors(m).join('\n'), /no since/);
  const m2 = man();
  m2.features.find((f) => f.id === 'clock').screen_since = { nope: '0.5.0-beta.1' };
  assert.match(channelErrors(m2).join('\n'), /screen_since nope is not one of its screens/);
  const m3 = man();
  m3.features.find((f) => f.id === 'clock').since = 'soon';
  assert.match(channelErrors(m3).join('\n'), /is a version/);
});

test('every screen has a channel', () => {
  const c = channels();
  for (const [id, x] of Object.entries(c)) assert.ok(['stable', 'beta', 'soon'].includes(x.channel), id);
});

// --- the README pictures: decoded here (8-bit RGBA, as tools/readme_assets.mjs writes them)
function decode(file) {
  const b = readFileSync(file);
  let p = 8, W = 0, H = 0;
  const idat = [];
  while (p < b.length) {
    const len = b.readUInt32BE(p), type = b.toString('latin1', p + 4, p + 8), d = b.subarray(p + 8, p + 8 + len);
    if (type === 'IHDR') { W = d.readUInt32BE(0); H = d.readUInt32BE(4); assert.equal(d[9], 6, `${file}: RGBA`); }
    if (type === 'IDAT') idat.push(d);
    p += 12 + len;
  }
  const raw = inflateSync(Buffer.concat(idat)), out = new Uint8Array(W * H * 4), st = W * 4;
  for (let y = 0; y < H; y++) {
    const f = raw[y * (st + 1)];
    for (let x = 0; x < st; x++) {
      const v = raw[y * (st + 1) + 1 + x], a = x >= 4 ? out[y * st + x - 4] : 0, u = y ? out[(y - 1) * st + x] : 0, c = x >= 4 && y ? out[(y - 1) * st + x - 4] : 0;
      const pa = Math.abs(u - c), pb = Math.abs(a - c), pc = Math.abs(a + u - 2 * c);
      out[y * st + x] = (v + [0, a, u, (a + u) >> 1, pa <= pb && pa <= pc ? a : pb <= pc ? u : c][f]) & 255;
    }
  }
  return { W, H, px: out };
}
// a chip is there when the pill's fill shows at both ends (beside the label, inside the outline), in the picture's
// own scale (rendered at 720, halved)
function hasChip(img, channel, theme) {
  const S = 720, k = img.W / S, box = chipBox(channel, S), fill = chipColours(channel, theme).fill;
  const inset = (CHIP.pad / 2) * (S / 480);
  return [box.x + inset, box.x + box.w - inset].every((x) => {
    const i = (Math.round((box.y + box.h / 2) * k) * img.W + Math.round(x * k)) * 4;
    return [0, 1, 2].every((j) => Math.abs(img.px[i + j] - fill[j]) <= 6);
  });
}

test('the chip draws the same at 1x and 1.5x (dp-sized)', () => {
  for (const ch of ['beta', 'soon']) {
    const a = chipBox(ch, 480), b = chipBox(ch, 720);
    for (const k of ['x', 'y', 'w', 'h']) assert.ok(Math.abs(a[k] * 1.5 - b[k]) <= 1.5, `${ch} ${k}: ${a[k]} at 1x, ${b[k]} at 1.5x`);
    for (const S of [480, 720]) {
      const f = new Uint8Array(S * S * 4).fill(255);
      drawChip(f, S, ch, 'dark');
      const box = chipBox(ch, S), fill = chipColours(ch, 'dark').fill;
      const i = (Math.round(box.y + box.h / 2) * S + Math.round(box.x + (CHIP.pad / 2) * (S / 480))) * 4;
      assert.deepEqual([...f.subarray(i, i + 3)], fill, `${ch} at ${S}: fill beside the label`);
    }
  }
});

test('README pictures: the chip is on exactly the screens that are not Stable', () => {
  const readme = readFileSync(join(R, '..', 'README.md'), 'utf8');
  const ids = [...new Set([...readme.matchAll(/assets\/screen-([a-z0-9_]+)-dark\.png/g)].map((x) => x[1]))];
  const c = channels();
  assert.ok(ids.length > 0);
  for (const id of ids) {
    assert.ok(c[id], `${id}: in the README but no channel`);
    for (const theme of ['dark', 'light']) {
      const img = decode(join(R, '..', '.github', 'assets', `screen-${id}-${theme}.png`));
      for (const ch of ['beta', 'soon']) {
        const want = c[id].channel === ch;
        assert.equal(hasChip(img, ch, theme), want, `screen-${id}-${theme}.png: ${want ? 'no' : 'a'} ${ch} chip (channel ${c[id].channel}; render again: tools/readme_assets.mjs)`);
      }
    }
  }
});

test('features.json: a panel feature is a screen or a setting', () => {
  const m = man();
  m.panel_features.list.push({ id: 'x', label: 'X', about: 'x', screen: 'nope' });
  assert.match(channelErrors(m).join('\n'), /nope is not a screen/);
  const m2 = man();
  m2.panel_features.list.push({ id: 'y', label: 'Y', about: 'y', setting: 'updateChannel' });
  assert.match(channelErrors(m2).join('\n'), /a setting says since/);
});

test('README: the screens table has screens only; On every panel lists each panel feature with its mark', () => {
  const readme = readFileSync(join(R, '..', 'README.md'), 'utf8');
  const c = channels();
  for (const [, id] of readme.matchAll(/assets\/screen-([a-z0-9_]+)-dark\.png/g)) assert.equal(c[id]?.kind, 'screen', `${id}: a panel feature in the screens table`);
  const sec = readme.slice(readme.indexOf('### On every panel'));
  assert.ok(readme.includes('### On every panel'), 'README: no On every panel section');
  const MARK = { stable: '', beta: ' `BETA`', soon: ' `COMING SOON`' };
  for (const p of panelFeatures()) {
    const row = sec.split('\n').find((l) => l.startsWith(`| [${p.label}](`));
    assert.ok(row, `README On every panel: no row for ${p.label}`);
    assert.match(row, new RegExp(`^\\| \\[[^\\]]+\\]\\([^)]+\\)${MARK[p.channel].replace(/`/g, '`')} \\|`), `README On every panel: ${p.label} is ${p.channel}`);
  }
});
