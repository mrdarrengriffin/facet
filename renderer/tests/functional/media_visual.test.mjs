// Media visual (components/media_visual.cpp): the cassette, CD and vinyl drawn with the Pen from real sizes, and the
// square art; its box from the mv-* layout tokens (proved against the real-size formula here); a golden per shape and
// size; the record's label and (spin) the CD turning with their album art, stopping where they are on pause.
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

// a two-colour album art (top half red, bottom half blue): upright, a point above the centre reads red; turned half a
// turn, blue. "Art" at a point = red, blue or their blend (never the plain drawing's paper / clear inks)
const TURN_URL = 'http://ha.test:8123/api/media_player_proxy/media_player.x?token=a&cache=turn';
const RED = [230, 30, 30, 255], BLUE = [30, 30, 230, 255];
const at565 = (t, x, y) => { const q = frame565(t)[Math.round(y) * (t.b.W || t.b.S) + Math.round(x)]; return [(q >> 11) << 3, ((q >> 5) & 63) << 2, (q & 31) << 3]; };
const isArt = (c) => c[1] < 90 && c[0] + c[2] > 180;
const tone = (c) => (!isArt(c) ? 'none' : c[0] > 160 && c[2] < 100 ? 'red' : c[2] > 160 && c[0] < 100 ? 'blue' : 'mix');
// where the art is printed (px on the stage) and its radius: the CD's printed band, the record's label
function artAt(t, o, visual) {
  const H = tok(t, 'mv-h');
  if (visual === 'cd') return { x: o.x + o.w / 2, y: o.y + o.h / 2, r: (117 / 120) * H / 2 };
  const k = H / 301.6, Px = (H / 2 - tok(t, '5', 'space')) / k, oy = -Math.sqrt(215 * 215 - Px * Px) - 34;
  return { x: o.x + 150.8 * k, y: o.y - oy * k, r: 50 * k };
}
// the areas LVGL flushed since the last read
const areas = (t) => { const m = t.b.m, q = new Int32Array(m.HEAPU8.buffer), p = m._fw_areas(t.b.stage) >> 2, out = []; for (let i = 0; i < q[p]; i++) { const [x1, y1, x2, y2] = q.subarray(p + 1 + i * 4, p + 5 + i * 4); out.push((x2 - x1 + 1) * (y2 - y1 + 1)); } return out; };
// watch ms of frames at the point (0.6 of the art's radius above its centre): the tones seen, frames without art, the
// biggest redraw
function watch(t, a, ms) {
  const seen = new Set(), x = a.x, y = a.y - 0.6 * a.r;
  let gaps = 0, frames = 0, biggest = 0;
  for (let e = 0; e < ms; e += 20) { t.wait(20); frames++; const c = tone(at565(t, x, y)); seen.add(c); if (c === 'none') gaps++; biggest = Math.max(biggest, ...areas(t)); }
  return { seen, gaps, frames, biggest, last: tone(at565(t, x, y)) };
}
async function turning(t, visual, props = {}) {
  t.img.serve(TURN_URL, 64, 64, (x, y) => (y < 32 ? RED : BLUE));
  const o = await at(t, { visual, art: TURN_URL, playing: true, progress: 40, ...props });
  t.wait(600);
  areas(t);
  return { o, a: artAt(t, o, visual), props: { visual, art: TURN_URL, playing: true, progress: 40, ...props } };
}

export default {
  id: 'media_visual', family: 'media', about: 'The Media screen picture: each shape at its token size, a golden per shape.',
  tests: [
    { name: 'vinyl: the record\'s label turns with its album art while playing; pause stops it where it is (no rebuild, no new request, the art in every frame); play turns it again', async run(t) {
      const { a, props } = await turning(t, 'vinyl');
      const asked = t.img.asked.length;
      let w = watch(t, a, 2000);
      t.ok(w.seen.has('red') && w.seen.has('blue'), `turns while playing: ${[...w.seen]}`);
      t.eq(w.gaps, 0, `the art in all ${w.frames} frames`);
      const label = Math.ceil(2 * a.r) + 8;
      t.ok(w.biggest <= label * label, `only the label redrawn: biggest ${w.biggest} px <= ${label}x${label}`);
      t.eq(t.b.update('media_visual', { ...props, playing: false }), '', 'pause: in place (not rebuilt)');
      w = watch(t, a, 4000);
      t.eq(w.gaps, 0, `the art in all ${w.frames} frames while it slows`);
      const rest = w.last;
      w = watch(t, a, 1500);
      t.ok(w.seen.size === 1 && w.seen.has(rest) && w.biggest === 0, `stopped where it was: ${[...w.seen]} (was ${rest}), nothing redrawn (${w.biggest})`);
      t.eq(t.b.m._fw_anims(), 0, 'the frame clock stopped');
      t.eq(t.b.update('media_visual', { ...props, playing: true }), '', 'play: in place');
      w = watch(t, a, 2500);
      t.ok(w.seen.size >= 2 && w.gaps === 0, `turns again: ${[...w.seen]}, ${w.gaps} frames without art`);
      t.eq(t.img.asked.length, asked, `no new picture request: ${t.img.asked.slice(asked)}`);
    } },
    { name: 'cd: spin off = the art stands still (the mark turns); spin on (the default) = the CD turns with its art; spin toggles in place', async run(t) {
      const { o, a, props } = await turning(t, 'cd', { spin: false });
      let w = watch(t, a, 2000);
      t.ok(w.seen.size === 1 && w.seen.has('red'), `spin off: upright, still: ${[...w.seen]}`);
      t.ok(w.biggest > 0 && w.biggest < (o.w * o.h) / 20, `spin off: only the print mark redrawn (${w.biggest} px)`);
      const { spin, ...dflt } = props;
      t.eq(t.b.update('media_visual', dflt), '', 'spin not given (on): in place');
      areas(t);
      w = watch(t, a, 3000);
      t.ok(w.seen.has('red') && w.seen.has('blue'), `spin on: turns: ${[...w.seen]}`);
      t.eq(w.gaps, 0, `the art in all ${w.frames} frames`);
      t.ok(w.biggest <= o.w * o.h, `the printed circle redrawn, never more than the CD (${w.biggest} px <= ${o.w}x${o.h})`);
      t.eq(t.b.update('media_visual', { ...props, spin: true, playing: false }), '', 'pause: in place');
      watch(t, a, 5000);
      w = watch(t, a, 1000);
      t.ok(w.seen.size === 1 && w.biggest === 0 && w.gaps === 0, `paused: stopped where it was (${[...w.seen]}), nothing redrawn`);
      t.eq(t.b.update('media_visual', { ...props, spin: false }), '', 'spin off: in place');
      w = watch(t, a, 1500);
      t.ok(w.seen.has('red') && w.seen.size === 1, `spin off: upright again: ${[...w.seen]}`);
    } },
    { name: 'disc (the plain disc, folded into the CD): draws the CD, screen and card', async run(t) {
      for (const size of ['screen', 'card']) {
        const box = await at(t, { visual: 'cd', size, progress: 45 });
        const cd = frame565(t).slice();
        const o = await at(t, { visual: 'disc', size, progress: 45 });
        t.ok(o.w === box.w && o.h === box.h, `${size}: the CD's box ${o.w}x${o.h}`);
        const f = frame565(t);
        let d = 0;
        for (let i = 0; i < f.length; i++) d += f[i] !== cd[i];
        t.eq(d, 0, `${size}: the same pixels as the CD`);
      }
    } },
    { name: 'art: the album art alone, a square (mv-h, card mv-card); nothing drawn with none or after a 404 (no empty box); it never turns', async run(t) {
      const URL = 'http://ha.test:8123/api/media_player_proxy/media_player.x?token=a&cache=sq';
      const mid = (o) => { const q = frame565(t)[Math.round(o.y + o.h / 2) * (t.b.W || t.b.S) + Math.round(o.x + o.w / 2)]; return [(q >> 11) << 3, ((q >> 5) & 63) << 2, (q & 31) << 3]; };
      const red = (c) => c[0] > 180 && c[1] < 90;
      let o = await at(t, { visual: 'art' });
      t.ok(o.w === tok(t, 'mv-h') && o.h === tok(t, 'mv-h'), `screen: ${o.w} x ${o.h} = mv-h square`);
      const px = (x, y) => { const q = frame565(t)[Math.round(y) * (t.b.W || t.b.S) + Math.round(x)]; return [(q >> 11) << 3, ((q >> 5) & 63) << 2, (q & 31) << 3]; };
      const bg = px(o.x - 2, o.y + o.h / 2), plain = mid(o);
      t.eq(plain, bg, 'no art: nothing drawn in the middle (the background, not a white box)');
      t.eq(px(o.x + 1, o.y + o.h / 2), bg, 'no art: no edge line either');
      t.img.serve(URL, 80, 80, [230, 30, 30, 255]);
      o = await at(t, { visual: 'art', art: URL, playing: true }); t.wait(600);
      t.ok(red(mid(o)), `the art: ${mid(o)}`);
      t.eq(t.b.m._fw_anims(), 0, 'nothing turns while playing');
      o = await at(t, { visual: 'art', size: 'card', art: URL }); t.wait(600);
      t.ok(o.w === tok(t, 'mv-card') && red(mid(o)), `card: ${o.w} x ${o.h}, ${mid(o)}`);
      t.img.status(URL.replace('sq', 'gone'), 404);
      o = await at(t, { visual: 'art', art: URL.replace('sq', 'gone') }); t.wait(600);
      t.eq(mid(o), plain, 'a 404: nothing drawn');
      t.eq(t.texts(), [], 'no error words');
    } },
    { name: 'the box sizes are the tokens, and the tokens are the real sizes scaled to mv-h', async run(t) {
      const H = tok(t, 'mv-h'), G = den(t) === '1x' ? 4 : 8, s5 = tok(t, '5', 'space');
      const k = H / 301.6, Px = (H / 2 - s5) / k, oy = -Math.sqrt(215 * 215 - Px * Px) - 34;
      t.eq(tok(t, 'mv-cassette-w'), up(Math.ceil(100.4 * (H - 1) / 63.8), G), 'mv-cassette-w = the cassette at mv-h (a pixel spare)');
      t.eq(tok(t, 'mv-vinyl-w'), up(Math.ceil((301.6 / 2 + Px + 36) * k), G), 'mv-vinyl-w = the record + the arm');
      t.eq(tok(t, 'mv-vinyl-h'), up(Math.ceil((301.6 / 2 - oy) * k), G), 'mv-vinyl-h = the record + the arm pivot above it');
      t.eq(tok(t, 'mv-bleed'), tok(t, 'mv-cassette-w') - H, 'mv-bleed = how far the cassette runs past its mv-h slot');
      for (const [v, w, h] of [['cd', 'mv-h', 'mv-h'], ['cassette', 'mv-cassette-w', 'mv-h'], ['vinyl', 'mv-vinyl-w', 'mv-vinyl-h']]) {
        const o = await at(t, { visual: v });
        t.ok(o && o.w === tok(t, w) && o.h === tok(t, h), `${v}: ${o?.w} x ${o?.h} = ${w} x ${h}`);
        const c = await at(t, { visual: v, size: 'card' });
        t.ok(c && c.w === tok(t, 'mv-card') && c.h === tok(t, 'mv-card'), `${v} card: ${c?.w} x ${c?.h} = mv-card square`);
      }
    } },
    { name: 'golden images: every shape, screen and card (dark + light): reviewed, never auto-accepted', async run(t, { den: d }) {
      const out = [];
      const cases = [['cassette', { visual: 'cassette', progress: 30 }], ['cd', { visual: 'cd', progress: 45 }],
        ['vinyl', { visual: 'vinyl', progress: 60 }], ['cassette-card', { visual: 'cassette', size: 'card' }],
        ['cd-card', { visual: 'cd', size: 'card' }], ['vinyl-card', { visual: 'vinyl', size: 'card' }]];
      for (const [n, p] of cases) for (const light of [0, 1]) { await at(t, p, light); out.push(...golden(t, 'media_visual', `${n}-${light ? 'light' : 'dark'}`, d)); }
      t.eq(out, [], 'goldens');
    } },
  ],
};
