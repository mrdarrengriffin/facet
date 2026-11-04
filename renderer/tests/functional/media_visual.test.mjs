// Media visual (components/media_visual.cpp): the disc, cassette, CD and vinyl drawn with the Pen from real sizes; its
// box from the mv-* layout tokens (proved against the real-size formula here); a golden per shape and size.
import fs from 'node:fs';
import { look, golden, stage, frame565 } from './_release.mjs';

const SP = JSON.parse(fs.readFileSync(new URL('../../tokens/space.json', import.meta.url))).tokens;
const den = (t) => (t.b.S > 600 ? '1.5x' : '1x');
const tok = (t, n, g = 'layout') => { const v = SP[g][n]; return den(t) === '1x' ? v['1x'] : v.v; };
const up = (v, g) => Math.ceil(v / g) * g;
async function at(t, props, light = 0) {
  t.b.W = t.b.H = undefined;
  await t.mount('media_visual', { playing: false, ...props });
  if (light) look(t, 1);
  stage(t, t.b.S, Math.round(t.b.S * 0.45)); t.b.settle(400);
  return t.part('media_visual');
}

export default {
  id: 'media_visual', family: 'media', about: 'The Media screen picture: each shape at its token size, a golden per shape.',
  tests: [
    { name: 'art: the album art alone, a square (mv-h, card mv-card); a plain square with none, while it loads or after a 404; it never turns', async run(t) {
      const URL = 'http://ha.test:8123/api/media_player_proxy/media_player.x?token=a&cache=sq';
      const mid = (o) => { const q = frame565(t)[Math.round(o.y + o.h / 2) * (t.b.W || t.b.S) + Math.round(o.x + o.w / 2)]; return [(q >> 11) << 3, ((q >> 5) & 63) << 2, (q & 31) << 3]; };
      const red = (c) => c[0] > 180 && c[1] < 90;
      let o = await at(t, { visual: 'art' });
      t.ok(o.w === tok(t, 'mv-h') && o.h === tok(t, 'mv-h'), `screen: ${o.w} x ${o.h} = mv-h square`);
      const plain = mid(o);
      t.ok(!red(plain), `no art: the plain square (${plain})`);
      t.img.serve(URL, 80, 80, [230, 30, 30, 255]);
      o = await at(t, { visual: 'art', art: URL, playing: true }); t.wait(600);
      t.ok(red(mid(o)), `the art: ${mid(o)}`);
      t.eq(t.b.m._fw_anims(), 0, 'nothing turns while playing');
      o = await at(t, { visual: 'art', size: 'card', art: URL }); t.wait(600);
      t.ok(o.w === tok(t, 'mv-card') && red(mid(o)), `card: ${o.w} x ${o.h}, ${mid(o)}`);
      t.img.status(URL.replace('sq', 'gone'), 404);
      o = await at(t, { visual: 'art', art: URL.replace('sq', 'gone') }); t.wait(600);
      t.eq(mid(o), plain, 'a 404: the plain square');
      t.eq(t.texts(), [], 'no error words');
    } },
    { name: 'the box sizes are the tokens, and the tokens are the real sizes scaled to mv-h', async run(t) {
      const H = tok(t, 'mv-h'), G = den(t) === '1x' ? 4 : 8, s5 = tok(t, '5', 'space');
      const k = H / 301.6, Px = (H / 2 - s5) / k, oy = -Math.sqrt(215 * 215 - Px * Px) - 34;
      t.eq(tok(t, 'mv-cassette-w'), up(Math.ceil(100.4 * (H - 1) / 63.8), G), 'mv-cassette-w = the cassette at mv-h (a pixel spare)');
      t.eq(tok(t, 'mv-vinyl-w'), up(Math.ceil((301.6 / 2 + Px + 36) * k), G), 'mv-vinyl-w = the record + the arm');
      t.eq(tok(t, 'mv-vinyl-h'), up(Math.ceil((301.6 / 2 - oy) * k), G), 'mv-vinyl-h = the record + the arm pivot above it');
      t.eq(tok(t, 'mv-bleed'), tok(t, 'mv-cassette-w') - H, 'mv-bleed = how far the cassette runs past its mv-h slot');
      for (const [v, w, h] of [['disc', 'mv-h', 'mv-h'], ['cd', 'mv-h', 'mv-h'], ['cassette', 'mv-cassette-w', 'mv-h'], ['vinyl', 'mv-vinyl-w', 'mv-vinyl-h']]) {
        const o = await at(t, { visual: v });
        t.ok(o && o.w === tok(t, w) && o.h === tok(t, h), `${v}: ${o?.w} x ${o?.h} = ${w} x ${h}`);
        const c = await at(t, { visual: v, size: 'card' });
        t.ok(c && c.w === tok(t, 'mv-card') && c.h === tok(t, 'mv-card'), `${v} card: ${c?.w} x ${c?.h} = mv-card square`);
      }
    } },
    { name: 'golden images: every shape, screen and card (dark + light): reviewed, never auto-accepted', async run(t, { den: d }) {
      const out = [];
      const cases = [['disc', { visual: 'disc', progress: 38 }], ['cassette', { visual: 'cassette', progress: 30 }], ['cd', { visual: 'cd', progress: 45 }],
        ['vinyl', { visual: 'vinyl', progress: 60 }], ['disc-card', { visual: 'disc', size: 'card' }], ['cassette-card', { visual: 'cassette', size: 'card' }],
        ['cd-card', { visual: 'cd', size: 'card' }], ['vinyl-card', { visual: 'vinyl', size: 'card' }]];
      for (const [n, p] of cases) for (const light of [0, 1]) { await at(t, p, light); out.push(...golden(t, 'media_visual', `${n}-${light ? 'light' : 'dark'}`, d)); }
      t.eq(out, [], 'goldens');
    } },
  ],
};
