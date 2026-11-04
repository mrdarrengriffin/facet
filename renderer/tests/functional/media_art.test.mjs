// Media art (components/media_art.cpp): the album art full screen with the track and the artist over its shaded foot,
// only while playing with art in; otherwise nothing drawn (what is under it shows). The idle screen's Album art
// (idle.test.mjs) is the screen it serves. Pictures from the fake image host (t.img).
import { fuzz, monkey, leak, frame565, stage, boundsAll } from './_release.mjs';

const URL = 'http://ha.test:8123/api/media_player_proxy/media_player.x?token=a&cache=1';
const WHITE = [250, 250, 250, 255], RED = [230, 30, 30, 255];
const rgb = (t, x, y) => { const q = frame565(t)[y * (t.b.W || t.b.S) + x]; return [(q >> 11) << 3, ((q >> 5) & 63) << 2, (q & 31) << 3]; };
const lum = ([r, g, b]) => { const f = (v) => { v /= 255; return v <= 0.04045 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4; }; return 0.2126 * f(r) + 0.7152 * f(g) + 0.0722 * f(b); };
async function at(t, props, fill = RED) {
  t.img.serve(URL, 120, 120, fill);
  t.b.W = t.b.H = undefined;
  await t.mount('media_art', { art: URL, title: 'Harbour Lights', artist: 'The Lanterns', playing: true, ...props });
  t.wait(600);
  return t.part('media_art');
}

export default {
  id: 'media_art', family: 'media', about: 'Media art: the album art full screen while playing, nothing otherwise; words over a shaded foot.',
  tests: [
    { name: 'fills its parent; with art and playing the picture covers it edge to edge', async run(t) {
      const o = await at(t, {}), c = t.part('media_art');
      for (const [w, h] of [[333, 251], [97, 401]]) { stage(t, w, h); t.wait(300); const p = t.part('media_art'); t.ok(p.w <= w && p.h === h && p.w >= w - 2 * 32, `fills ${w}x${h}: ${p.w}x${p.h}`); }
      await at(t, {});
      for (const [x, y] of [[o.x + 1, o.y + 1], [o.x + o.w - 2, o.y + 1], [o.x + (o.w >> 1), o.y + (o.h >> 2)]]) t.ok(rgb(t, x, y)[0] > 180 && rgb(t, x, y)[1] < 90, `art at ${x},${y}: ${rgb(t, x, y)}`);
      t.ok(c.w > 0, 'built');
      t.ok(t.img.asked.includes(URL), 'fetched');
    } },
    { name: 'nothing drawn while paused, without art, or when the art fails (no error words)', async run(t) {
      const bg = async (p) => { await at(t, p); return rgb(t, t.b.S >> 1, t.b.S >> 2); };
      const none = await bg({ art: '' });
      t.eq(await bg({ playing: false }), none, 'paused: nothing');
      t.img.status(URL, 404);
      t.b.W = t.b.H = undefined;
      await t.mount('media_art', { art: URL, title: 'T', artist: 'A', playing: true }); t.wait(600);
      t.eq(rgb(t, t.b.S >> 1, t.b.S >> 2), none, 'a 404: nothing');
      t.hasNot(t.texts(), /404|not found|fail/i, 'no error words');
    } },
    { name: 'the words read over the whitest art: the foot under them is shaded to art-shade or darker', async run(t) {
      await at(t, {}, WHITE);
      const o = t.part('media_art'), X0 = o.x + 1, X1 = o.x + o.w - 2, Y1 = o.y + o.h - 2, YT = o.y + Math.round(o.h * 0.86);
      const worst = Math.max(...[[X0, Y1], [o.x + (o.w >> 1), Y1], [X0, YT], [X1, YT]].map(([x, y]) => lum(rgb(t, x, y))));
      const shade = lum([0x59, 0x59, 0x59]);
      t.ok(worst <= shade * 1.08, `the lightest foot pixel (L ${worst.toFixed(3)}) <= art-shade (L ${shade.toFixed(3)})`);
      t.ok((1.05) / (worst + 0.05) >= 4.5, `white text on it: ${((1.05) / (worst + 0.05)).toFixed(1)}:1`);
      t.ok(lum(rgb(t, o.x + (o.w >> 1), o.y + 4)) > 0.8, 'the top is the picture as it is');
    } },
    { name: 'update in place: a new track swaps the picture (the old stays until it is in), a pause hides it, silently', async run(t) {
      await at(t, {});
      const B = URL.replace('cache=1', 'cache=2');
      t.img.serve(B, 60, 60, [30, 30, 230, 255]);
      const evs = t.update({ art: B, title: 'Paper Moons' }); t.wait(600);
      t.ok(rgb(t, t.b.S >> 1, t.b.S >> 2)[2] > 180, `the new art: ${rgb(t, t.b.S >> 1, t.b.S >> 2)}`);
      t.silent(evs.concat(t.update({ playing: false })));
      t.ok(rgb(t, t.b.S >> 1, t.b.S >> 2)[2] < 100, 'paused: hidden');
      t.ok(t.img.stats().views <= 1, `only the current picture is kept: ${JSON.stringify(t.img.stats())}`);
    } },
    { name: 'bounds at odd sizes, and fuzz (sizes, long and empty words)', async run(t) {
      const bad = [];
      for (const [w, h] of [[t.b.S, t.b.S], [333, 251], [97, 401], [64, 64]]) { await at(t, {}); stage(t, w, h); t.wait(300); bad.push(...boundsAll(t).map((e) => `${w}x${h}: ${e}`)); }
      t.eq(bad.slice(0, 5), [], 'bounds');
      t.eq(await fuzz(t, 12, 7, async (r) => { await at(t, { title: 'x'.repeat(r.int(0, 300)), artist: r.int(0, 1) ? '' : 'Ä'.repeat(r.int(1, 80)) }); stage(t, r.int(64, t.b.S), r.int(64, t.b.S)); }), [], 'fuzz');
    } },
    { name: 'monkey: no touch handled, never stuck', async run(t) {
      await at(t, {});
      t.eq(monkey(t, 40, 3), [], 'monkey');
    } },
    { name: 'leak: 30 builds and teardowns keep the objects and the heap', async run(t) {
      await at(t, {});
      t.eq(leak(t, 30, () => { t.b.rebuild('media_art', { art: URL, title: 'T', artist: 'A', playing: true }); t.b.settle(300); },
        () => { t.b.rebuild('icon', { name: 'bulb' }); t.b.settle(300); }), [], 'leak');
      t.eq(t.img.stats().views, 0, 'no picture kept once it is gone');
    } },
  ],
};
