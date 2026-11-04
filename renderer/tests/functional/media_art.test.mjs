// Media art (components/media_art.cpp): the album art full screen, nothing over it (no words, no shade),
// only while playing with art in; otherwise nothing drawn (what is under it shows). The idle screen's Album art
// (idle.test.mjs) is the screen it serves. Pictures from the fake image host (t.img).
import { fuzz, monkey, leak, mem, frame565, stage, boundsAll } from './_release.mjs';

const URL = 'http://ha.test:8123/api/media_player_proxy/media_player.x?token=a&cache=1';
const WHITE = [250, 250, 250, 255], RED = [230, 30, 30, 255];
const rgb = (t, x, y) => { const q = frame565(t)[y * (t.b.W || t.b.S) + x]; return [(q >> 11) << 3, ((q >> 5) & 63) << 2, (q & 31) << 3]; };
const lum = ([r, g, b]) => { const f = (v) => { v /= 255; return v <= 0.04045 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4; }; return 0.2126 * f(r) + 0.7152 * f(g) + 0.0722 * f(b); };
async function at(t, props, fill = RED) {
  t.img.serve(URL, 120, 120, fill);
  t.b.W = t.b.H = undefined;
  await t.mount('media_art', { art: URL, playing: true, ...props });
  t.wait(600);
  return t.part('media_art');
}

export default {
  id: 'media_art', family: 'media', about: 'Media art: the album art full screen while playing, nothing over it; nothing otherwise.',
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
      await t.mount('media_art', { art: URL, playing: true }); t.wait(600);
      t.eq(rgb(t, t.b.S >> 1, t.b.S >> 2), none, 'a 404: nothing');
      t.hasNot(t.texts(), /404|not found|fail/i, 'no error words');
    } },
    { name: 'only the art: no words and no shade, even over the whitest art (its foot is the picture as it is)', async run(t) {
      await at(t, {}, WHITE);
      const o = t.part('media_art'), foot = rgb(t, o.x + (o.w >> 1), o.y + o.h - 3), top = rgb(t, o.x + (o.w >> 1), o.y + 4);
      t.ok(lum(foot) > 0.8 && lum(top) > 0.8, `white top to bottom: ${top} / ${foot}`);
      t.eq(t.texts(), [], 'no text');
    } },
    { name: 'update in place: a new track swaps the picture (the old stays until it is in), a pause hides it, silently', async run(t) {
      await at(t, {});
      const B = URL.replace('cache=1', 'cache=2');
      t.img.serve(B, 60, 60, [30, 30, 230, 255]);
      const evs = t.update({ art: B }); t.wait(600);
      t.ok(rgb(t, t.b.S >> 1, t.b.S >> 2)[2] > 180, `the new art: ${rgb(t, t.b.S >> 1, t.b.S >> 2)}`);
      t.silent(evs.concat(t.update({ playing: false })));
      t.ok(rgb(t, t.b.S >> 1, t.b.S >> 2)[2] < 100, 'paused: hidden');
      t.ok(t.img.stats().views <= 1, `only the current picture is kept: ${JSON.stringify(t.img.stats())}`);
    } },
    { name: 'swap order: while the next track\'s art is coming the old one stays drawn; once it is in, only the new one is kept', async run(t) {
      await at(t, {});
      const mid = () => rgb(t, t.b.S >> 1, t.b.S >> 2);
      const B = URL.replace('cache=1', 'cache=slow');
      t.img.hang(B);
      t.update({ art: B }); t.wait(600);
      t.ok(mid()[0] > 180 && mid()[2] < 90, `the old art while the new one comes: ${mid()}`);
      t.eq(t.img.stats().views, 2, 'two views meanwhile (shown + coming)');
      const m = t.b.m, px = new Uint8Array(64 * 64 * 4);
      for (let i = 0; i < px.length; i += 4) px.set([30, 30, 230, 255], i);
      const p = m._malloc(px.length); m.HEAPU8.set(px, p);
      t.b.str(B, (pu) => m._fw_img_put(pu, 64, 64, p)); m._free(p);
      t.wait(300);
      t.ok(mid()[2] > 180, `the new art once it is in: ${mid()}`);
      t.eq(t.img.stats().views, 1, 'the old one gone after the swap');
    } },
    { name: 'track after track: 25 changes each show the new art, never a gap, one picture kept, the heap flat', async run(t) {
      await at(t, {});
      const mid = () => rgb(t, t.b.S >> 1, t.b.S >> 2);
      const fill = (i) => [40 + (i * 37) % 200, 200 - (i * 53) % 180, 60 + (i * 71) % 190, 255];
      const near = (a, b) => Math.abs(a[0] - b[0]) < 24 && Math.abs(a[1] - b[1]) < 24 && Math.abs(a[2] - b[2]) < 24;
      const skip = (i) => { const u = URL.replace('cache=1', `cache=t${i}`); t.img.serve(u, 640, 640, fill(i)); t.update({ art: u }); t.wait(400); return near(mid(), fill(i)); };
      for (let i = 0; i < 3; i++) skip(i);  // warm up
      const m0 = mem(t);
      let shown = 0;
      for (let i = 3; i < 28; i++) shown += skip(i) ? 1 : 0;
      t.b.settle(1000);
      const m1 = mem(t), st = t.img.stats();
      t.eq(shown, 25, `each new track's art shows (${shown} of 25)`);
      t.eq(st.views, 1, `one picture kept: ${JSON.stringify(st)}`);
      t.eq(st.refused, 0, 'nothing refused');
      t.ok(m1.heap - m0.heap < 64 * 1024 && m1.objs === m0.objs, `heap ${Math.round((m1.heap - m0.heap) / 1024)} KB, objects ${m0.objs} -> ${m1.objs}`);
      const side = Math.max(t.part('media_art').w, t.part('media_art').h), cap = t.img.stats().board === 'guition-4848s040' ? 320 : 0;
      const k = cap && side > cap ? cap / side : 1, ow = Math.round(t.part('media_art').w * k), oh = Math.round(t.part('media_art').h * k);
      t.log.push(`25 tracks: heap ${m1.heap - m0.heap} B, kept ${st.bytes} B (${ow}x${oh}), peak cache ${st.peak} B`);
      t.ok(st.bytes <= ow * oh * 2 + 256, `kept at ${ow}x${oh} RGB565 (${st.bytes} B; the box ${t.part('media_art').w}x${t.part('media_art').h})`);
    } },
    { name: 'bounds at odd sizes, and fuzz (sizes, art or none)', async run(t) {
      const bad = [];
      for (const [w, h] of [[t.b.S, t.b.S], [333, 251], [97, 401], [64, 64]]) { await at(t, {}); stage(t, w, h); t.wait(300); bad.push(...boundsAll(t).map((e) => `${w}x${h}: ${e}`)); }
      t.eq(bad.slice(0, 5), [], 'bounds');
      t.eq(await fuzz(t, 12, 7, async (r) => { await at(t, { art: r.int(0, 1) ? URL : '' }); stage(t, r.int(64, t.b.S), r.int(64, t.b.S)); }), [], 'fuzz');
    } },
    { name: 'monkey: no touch handled, never stuck', async run(t) {
      await at(t, {});
      t.eq(monkey(t, 40, 3), [], 'monkey');
    } },
    { name: 'leak: 30 builds and teardowns keep the objects and the heap', async run(t) {
      await at(t, {});
      t.eq(leak(t, 30, () => { t.b.rebuild('media_art', { art: URL, playing: true }); t.b.settle(300); },
        () => { t.b.rebuild('icon', { name: 'bulb' }); t.b.settle(300); }), [], 'leak');
      t.eq(t.img.stats().views, 0, 'no picture kept once it is gone');
    } },
  ],
};
