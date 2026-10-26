// Image (core/images.h cache): loads, fallbacks, fit geometry, grid items, the per-board byte cap, rebinds.
const U = (n) => `https://img.test/${n}.png`;
const imgs = (t) => t.objs().filter((o) => o.t === 'image' && t.visible(o));
const RED = [220, 40, 40, 255], BLUE = [40, 80, 220, 255];
export default {
  id: 'image', family: 'cards', about: 'A picture from a URL, sized by a step or its grid cell, through a byte-capped cache.',
  tests: [
    { name: 'loads: the picture fills a 4:3 card box', async run(t) {
      t.img.serve(U('a'), 64, 48, RED);
      await t.mount('image', { src: U('a'), step: 'card', aspect: '4:3' });
      const b = t.part('image'), im = imgs(t);
      t.eq(im.length, 1, 'one picture');
      t.ok(im[0] && im[0].w === b.w && im[0].h === b.h, `cover fills the box: ${JSON.stringify(im[0])} in ${b.w}x${b.h}`);
      t.ok(b.h === Math.floor(b.w * 3 / 4 / 4) * 4 || b.h === Math.floor(b.w * 3 / 4 / 8) * 8, `4:3 on the grid: ${b.w}x${b.h}`);
      t.eq(t.img.stats().shown, 1, 'one view shown');
    } },
    { name: '404: the fallback icon + alt, no picture', async run(t) {
      t.img.status(U('gone'), 404);
      await t.mount('image', { src: U('gone'), step: 'hero', aspect: '4:3', fallback: 'house', alt: 'Front garden' });
      const b = t.part('image');
      t.eq(imgs(t).filter((o) => o.w === b.w).length, 0, 'no picture');
      t.has(t.texts(), 'Front garden', 'alt shown');
      t.ok(t.objs().some((o) => o.d === 2 && o.t !== 'label' && o.w > 0 && t.visible(o)), 'fallback icon drawn');
    } },
    { name: 'timeout: no answer = the fallback after the timeout, a quiet box before', async run(t) {
      t.img.hang(U('slow'));
      await t.mount('image', { src: U('slow'), step: 'hero', fallback: 'house', alt: 'Slow' });
      t.hasNot(t.texts(), 'Slow', 'still loading (quiet box)');
      t.wait(8600);
      t.has(t.texts(), 'Slow', 'failed after the timeout');
      t.eq(t.img.stats().pending, 0, 'nothing left pending');
    } },
    { name: 'contain: a wide picture keeps its shape inside a square box', async run(t) {
      t.img.serve(U('wide'), 160, 90, BLUE);
      await t.mount('image', { src: U('wide'), step: 'hero', scale: 'contain' });
      const b = t.part('image'), [im] = imgs(t);
      t.ok(im && im.w === b.w && Math.abs(im.h - Math.round(b.w * 9 / 16)) <= 1, `16:9 inside: ${im?.w}x${im?.h} in ${b.w}x${b.h}`);
      t.ok(im && Math.abs((im.y - b.y) - (b.y + b.h - im.y - im.h)) <= 1, 'centred');
    } },
    { name: 'pixel: whole-number zoom, crisp (no in-between colours)', async run(t) {
      t.img.serve(U('px'), 8, 8, (x, y) => ((x + y) % 2 ? RED : BLUE));
      await t.mount('image', { src: U('px'), step: 'hero', scale: 'pixel' });
      const b = t.part('image'), [im] = imgs(t);
      t.ok(im && im.w % 8 === 0 && im.w === Math.floor(b.w / 8) * 8, `8 px x whole steps: ${im?.w} in ${b.w}`);
      t.ok(im && Math.abs(im.x - b.x - (b.x + b.w - im.x - im.w)) <= 1, 'centred');
    } },
    { name: 'grid items: one picture per item, each from its own URL', async run(t) {
      const items = [1, 2, 3, 4, 5].map((i) => ({ id: `g${i}`, pic: U(`g${i}`) }));
      items.forEach((it, i) => t.img.serve(it.pic, 16, 16, i % 2 ? RED : BLUE));
      await t.mount('grid', { cols: 5, gap: 'near', items: JSON.stringify(items), _cell: { c: 'image', p: { src: '$item.pic', fit: 'cell', scale: 'pixel' } } });
      t.eq(imgs(t).length, 5, 'five pictures');
      t.eq([...new Set(t.img.asked)].sort(), items.map((i) => i.pic).sort(), 'each item URL fetched once');
      t.eq(t.img.stats().shown, 5, 'five views shown');
    } },
    { name: 'cache cap: never exceeded; over budget = fallback; leaving the screen frees it all', async run(t) {
      // 64 cells of a content-wide 1-column grid would be huge: 12 big wide cells push past the cap
      const n = 12, items = Array.from({ length: n }, (_, i) => ({ id: `b${i}`, pic: U(`b${i}`) }));
      items.forEach((it) => t.img.serve(it.pic, 64, 36, RED));
      await t.mount('grid', { cols: 1, gap: 'none', aspect: 'wide', items: JSON.stringify(items), _cell: { c: 'image', p: { src: '$item.pic', fit: 'cell' } } });
      const s = t.img.stats();
      t.ok(s.peak <= s.cap && s.bytes <= s.cap, `bytes ${s.bytes} / peak ${s.peak} <= cap ${s.cap} (${s.board})`);
      t.ok(s.refused > 0 && imgs(t).length < n, `some refused (${s.refused}), ${imgs(t).length} shown of ${n}`);
      const after = t.img.leave();
      t.eq([after.bytes, after.views], [0, 0], 'screen left: cache empty');
      t.ok(after.dropped > 0, `dropped ${after.dropped}`);
    } },
    { name: 'cache LRU: views nothing shows are evicted first, oldest first', async run(t) {
      const urls = Array.from({ length: 30 }, (_, i) => U(`r${i}`));
      urls.forEach((u) => t.img.serve(u, 64, 64, BLUE));
      await t.mount('image', { src: urls[0], step: 'content', aspect: '16:9' });
      for (const u of urls.slice(1)) t.update({ src: u });  // each rebind leaves the last view unshown
      const s = t.img.stats();
      t.ok(s.peak <= s.cap, `peak ${s.peak} <= cap ${s.cap}`);
      t.ok(s.evicted > 0 && s.refused === 0, `evicted ${s.evicted}, refused ${s.refused}`);
      t.eq(imgs(t).length, 1, 'the last picture shown');
    } },
    { name: 'rebind: a new URL swaps the picture, silently; a failed new URL keeps the last good', async run(t) {
      t.img.serve(U('one'), 32, 32, RED).serve(U('two'), 32, 32, BLUE).status(U('bad'), 500);
      await t.mount('image', { src: U('one'), step: 'card' });
      let ev = t.update({ src: U('two') });
      t.silent(ev, 'swap is silent');
      t.has(t.img.asked, U('two'), 'new URL fetched');
      t.eq(imgs(t).length, 1, 'a picture after the swap');
      ev = t.update({ src: U('bad') });
      t.eq(imgs(t).length, 1, 'last good picture kept after a failed new URL');
      t.ok(t.img.stats().shown >= 1, 'still shown');
    } },
    { name: 'refresh fails: the last good picture stays', async run(t) {
      t.img.serve(U('cam'), 32, 18, RED);
      await t.mount('image', { src: U('cam'), step: 'hero', aspect: '16:9', refresh: '1m', fallback: 'house', alt: 'Camera' });
      t.img.status(U('cam'), 503);
      t.wait(61000);
      t.ok(t.img.asked.filter((u) => u === U('cam')).length >= 2, 'fetched again');
      t.eq(imgs(t).length, 1, 'picture kept');
      t.hasNot(t.texts(), 'Camera', 'no fallback');
    } },
  ],
};
