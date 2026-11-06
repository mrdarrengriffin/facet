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

// a 640 px detail cover served at u; quality(): the media_art on screen against a reference resample of it (PSNR, dB),
// and the same for a two-thirds copy blown up by nearest (the pixelated look)
function serveDetail(t, u) {
  const S0 = 640, pat = (x, y) => [(x * 255) / S0 | 0, (y * 255) / S0 | 0, ((x ^ y) * 3) & 255, 255];
  t.img.serve(u, S0, S0, pat);
}
function quality(t) {
  // fine detail (an x ^ y pattern, as the docs' cover fixture): area-averaged down / bilinear up in doubles here
  const S0 = 640, pat = (x, y) => [(x * 255) / S0 | 0, (y * 255) / S0 | 0, ((x ^ y) * 3) & 255, 255];
  const o = t.part('media_art'), k = Math.max(o.w / S0, o.h / S0), x0 = (S0 - o.w / k) / 2, y0 = (S0 - o.h / k) / 2;
  const ref = (X, Y, c) => {  // the cover-scaled source at output pixel X, Y
    if (k < 1) {  // area: the source span this pixel covers, fractional edges weighted
      let sum = 0, wt = 0;
      const ax = x0 + X / k, bx = x0 + (X + 1) / k, ay = y0 + Y / k, by = y0 + (Y + 1) / k;
      for (let sy = Math.floor(ay); sy < Math.ceil(by); sy++) for (let sx = Math.floor(ax); sx < Math.ceil(bx); sx++) {
        const w = (Math.min(sx + 1, bx) - Math.max(sx, ax)) * (Math.min(sy + 1, by) - Math.max(sy, ay));
        sum += pat(Math.min(S0 - 1, sx), Math.min(S0 - 1, sy))[c] * w; wt += w;
      }
      return sum / wt;
    }
    const fx = Math.min(S0 - 1, Math.max(0, x0 + (X + 0.5) / k - 0.5)), fy = Math.min(S0 - 1, Math.max(0, y0 + (Y + 0.5) / k - 0.5));
    const xa = Math.floor(fx), ya = Math.floor(fy), xb = Math.min(xa + 1, S0 - 1), yb = Math.min(ya + 1, S0 - 1), ax = fx - xa, ay = fy - ya;
    return (pat(xa, ya)[c] * (1 - ax) + pat(xb, ya)[c] * ax) * (1 - ay) + (pat(xa, yb)[c] * (1 - ax) + pat(xb, yb)[c] * ax) * ay;
  };
  // PSNR over a grid of samples: the frame, and a two-thirds copy blown up by nearest neighbour (the pixelated look)
  const n = Math.round(o.w * 2 / 3);
  let e = 0, eb = 0, cnt = 0;
  for (let Y = 1; Y < o.h - 1; Y += 3) for (let X = 1; X < o.w - 1; X += 3) {
    const got = rgb(t, o.x + X, o.y + Y), bx = Math.floor(Math.floor(X * n / o.w) * o.w / n), by = Math.floor(Math.floor(Y * n / o.w) * o.w / n);
    for (let c = 0; c < 3; c++) { const r = ref(X, Y, c); e += (got[c] - r) ** 2; eb += (ref(bx, by, c) - r) ** 2; cnt++; }
  }
  const db = (v) => 10 * Math.log10(255 * 255 / Math.max(1e-9, v / cnt));
  return { o, n, got: db(e), blocky: db(eb) };
}

export default {
  id: 'media_art', family: 'media', about: 'Media art: the album art full screen while playing, nothing over it; nothing otherwise.',
  tests: [
    { name: 'Apple Music art is asked at the box size, capped; other URLs as they are (#130)', async run(t) {
      const A = 'https://is1-ssl.mzstatic.com/image/thumb/Music126/v4/ab/cd/ef/source/100x100bb.jpg';
      const T = 'https://is1-ssl.mzstatic.com/image/thumb/Music126/v4/ab/cd/ef/source/{w}x{h}bb.jpg';
      const P = 'http://ha.test:8123/api/media_player_proxy/media_player.x?token=a&cache=' + encodeURIComponent('https://is1-ssl.mzstatic.com/image/thumb/a/b/600x600bb.jpg');
      const O = 'http://ha.test:8123/art/100x100bb.jpg';  // not Apple's: as it is
      const asked = [];
      for (const u of [A, T, P, O]) {
        t.img.asked.length = 0;
        t.b.W = t.b.H = undefined;
        await t.mount('media_art', { art: u, playing: true });
        t.wait(600);
        asked.push(t.img.asked.at(-1) || '');
      }
      const b = t.part('media_art'), side = Math.min(1000, Math.max(b?.w || 0, b?.h || 0));
      t.ok(side > 100, `the box (${side} px)`);
      t.eq(asked[0], A.replace('100x100bb', `${side}x${side}bb`), 'a sized Apple URL: the box size');
      t.eq(asked[1], T.replace('{w}x{h}bb', `${side}x${side}bb`), 'a template Apple URL: the box size');
      t.eq(asked[2], P.replace('600x600bb', `${side}x${side}bb`), 'inside the proxy URL (encoded): the box size');
      t.eq(asked[3], O, 'not Apple: as it is');
    } },
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
      const ow = t.part('media_art').w, oh = t.part('media_art').h;  // every board keeps the art at its box size
      t.log.push(`25 tracks: heap ${m1.heap - m0.heap} B, kept ${st.bytes} B (${ow}x${oh}), peak cache ${st.peak} B`);
      t.ok(st.bytes <= ow * oh * 2 + 256 && st.bytes >= ow * oh * 2, `kept at ${ow}x${oh} RGB565 (${st.bytes} B; the box ${t.part('media_art').w}x${t.part('media_art').h})`);
    } },
    { name: 'quality: a 640 px cover drawn at the box\'s own size (480 on the Guition, 720 on the Waveshare) matches a reference resample; a pixelated copy would not', async run(t) {
      const u = URL.replace('cache=1', 'cache=detail');
      serveDetail(t, u);
      t.b.W = t.b.H = undefined;
      await t.mount('media_art', { art: u, playing: true }); t.wait(600);
      const { o, n, got, blocky } = quality(t);
      t.log.push(`quality ${o.w}x${o.h}: PSNR ${got.toFixed(1)} dB; a pixelated copy ${blocky.toFixed(1)} dB`);
      t.ok(got >= 30, `drawn at its box size: PSNR ${got.toFixed(1)} dB >= 30 (a ${n} px copy blown up by nearest: ${blocky.toFixed(1)} dB)`);
      t.ok(got - blocky >= 4, `the bar tells pixelated art apart (${got.toFixed(1)} vs ${blocky.toFixed(1)} dB)`);
      const b = t.img.stats().bytes;
      t.ok(b >= o.w * o.h * 2 && b <= o.w * o.h * 2 + 256, `kept at ${o.w}x${o.h} RGB565 (${b} B)`);
    } },
    { name: 'after the Media screen\'s spinning CD and 12 track changes the idle art is still at its box size, smooth, in a reserved slot', async run(t) {
      // the CD turns its art (nearest-pixel while it turns on the Guition, #119): none of that reaches the Media art
      t.img.serve(URL, 640, 640, RED);
      t.b.W = t.b.H = undefined;
      await t.mount('media_visual', { visual: 'cd', art: URL, playing: true, spin: true }); t.wait(1500);
      await t.mount('media_art', { art: URL, playing: true }); t.wait(600);
      for (let i = 0; i < 12; i++) { const u = URL.replace('cache=1', `cache=n${i}`); t.img.serve(u, 640, 640, [40 + i * 15, 90, 200 - i * 10, 255]); t.update({ art: u }); t.wait(400); }
      const u = URL.replace('cache=1', 'cache=detail2');
      serveDetail(t, u);
      t.update({ art: u }); t.wait(600);
      const { o, n, got, blocky } = quality(t), st = t.img.stats();
      t.log.push(`after a spin + 12 tracks ${o.w}x${o.h}: PSNR ${got.toFixed(1)} dB; ${JSON.stringify(st)}`);
      t.ok(got >= 30 && got - blocky >= 4, `still at its box size, smooth: PSNR ${got.toFixed(1)} dB (a ${n} px copy blown up: ${blocky.toFixed(1)} dB)`);
      t.eq(st.views, 1, `only the shown picture kept (the CD's art let go): ${JSON.stringify(st)}`);
      t.ok(st.bytes >= o.w * o.h * 2 && st.bytes <= o.w * o.h * 2 + 256, `kept at ${o.w}x${o.h} RGB565 (${st.bytes} B)`);
      t.ok(st.slots === 2 && st.slots_free === 1, `in a reserved slot: ${st.slots} slots, ${st.slots_free} free`);
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
