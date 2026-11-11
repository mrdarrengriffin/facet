// The renderer in the browser (engine.ts Stage.wheel, renderer fw_scroll): a mouse wheel over a scrolling list scrolls
// the list and not the page; over content that doesn't scroll, or a list already at its end, the page scrolls.
import { describe, it, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { launch, close, HUB, settle } from './lib.mjs';

describe('the wheel over the renderer', () => {
  let ctx, page;
  before(async () => {
    const b = await launch();
    ctx = await b.newContext({ viewport: { width: 1200, height: 600 } });
    page = await ctx.newPage();
    await page.goto(`${HUB}/wasm/controls/scroll_track`);
    await page.waitForFunction(() => document.querySelector('canvas.wb-canvas')?.width > 0, null, { timeout: 60000 });
    await settle(page, 1500);
  });
  after(async () => { await ctx?.close(); await close(); });
  // the component page's variants: [1] = eight rows (scrolls), [2] = two rows (nothing to scroll)
  /** variant canvas i's box in the viewport and a digest of its pixels */
  const look = (i) => page.evaluate((i) => {
    const c = document.querySelectorAll('canvas.wb-canvas')[i + 1], r = c.getBoundingClientRect();
    const d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data;
    let h = 0; for (let i = 0; i < d.length; i += 4) h = (h * 31 + d[i] + d[i + 1] * 7 + d[i + 2] * 13) | 0;
    return { x: r.x, y: r.y, w: r.width, h: r.height, px: h, sy: scrollY };
  }, i);
  /** the page scrolled so variant i starts 100 px down the viewport */
  const at = async (i) => {
    await page.evaluate((i) => scrollTo(0, document.querySelectorAll('canvas.wb-canvas')[i + 1].getBoundingClientRect().top + scrollY - 100), i);
    await settle(page, 200); return look(i);
  };
  const wheel = async (i, x, y, dy) => { await page.mouse.move(x, y); await page.mouse.wheel(0, dy); await settle(page, 400); return look(i); };

  it('over the list, the list scrolls and the page stays', async () => {
    const a = await at(1), b = await wheel(1, a.x + a.w / 2, a.y + a.h / 2, 120);
    assert.equal(b.sy, a.sy, 'the page stayed');
    assert.notEqual(b.px, a.px, 'the list moved');
    const c = await wheel(1, a.x + a.w / 2, a.y + a.h / 2, -2000);  // back to its top, the rest goes to the page
    assert.equal(c.px, a.px, 'the list back at its top');
  });
  it('over the list at its top, a wheel up scrolls the page', async () => {
    const a = await at(1), b = await wheel(1, a.x + a.w / 2, a.y + a.h / 2, -60);
    assert.ok(b.sy < a.sy, `the page scrolled (scrollY ${a.sy} -> ${b.sy})`);
    assert.equal(b.px, a.px, 'the list stayed');
  });
  it('over a list with nothing to scroll, the page scrolls', async () => {
    const a = await at(2), b = await wheel(2, a.x + a.w / 2, a.y + a.h / 2, 120);
    assert.ok(b.sy > a.sy, `the page scrolled (scrollY ${a.sy} -> ${b.sy})`);
  });
  it('outside the list (the stage around it), the page scrolls', async () => {
    const a = await at(1), b = await wheel(1, a.x + 4, a.y + 4, 120);
    assert.ok(b.sy > a.sy, `the page scrolled (scrollY ${a.sy} -> ${b.sy})`);
  });
});
