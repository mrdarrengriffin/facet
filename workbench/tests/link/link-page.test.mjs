// The panel's link page (firmware/link.html, GET /link on a panel) against a fake panel: GET /fw/link answers the
// link's state, POST /fw/act takes ha.connect and "checks" it like the panel (a good token links, a wrong one is
// refused, an unknown host is not reached). The page says each in words, clears the token after sending and never
// shows it back (the panel answers its last 4 only). At a phone's width and at a desktop's.
import { describe, it, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFileSync } from 'node:fs';
import { chromium } from 'playwright';

const HTML = readFileSync(new URL('../../../firmware/link.html', import.meta.url), 'utf8');
const GOOD = 'eyJgoodtoken.abc-123_XYZ', HA = 'http://homeassistant.local:8123';
let srv, base, browser, link;
const reset = () => { link = { state: 'off', url: '', token: '', why: '', polls: 0 }; };
function panel(req, res) {
  const send = (code, type, body) => { res.writeHead(code, { 'Content-Type': type }); res.end(body); };
  if (req.url === '/link') return send(200, 'text/html; charset=utf-8', HTML);
  if (req.url === '/fw/link') {  // "checking" for two reads, then the check's answer
    const out = { ...link, state: link.state === 'checking' && ++link.polls > 2 ? link.next : link.state };
    if (out.state !== 'checking') link.state = out.state;
    return send(200, 'application/json', JSON.stringify({ state: out.state, url: link.url, token: link.token, why: out.state === 'failed' ? link.why : '' }));
  }
  if (req.url === '/fw/act' && req.method === 'POST') {
    let b = '';
    req.on('data', (c) => (b += c));
    req.on('end', () => {
      const a = JSON.parse(b);
      if (a.do !== 'ha.connect' || !/^https?:\/\//.test(a.url)) return send(400, 'application/json', JSON.stringify({ ok: false, why: 'url: http(s)://host[:port], at most 128' }));
      const tok = a.token || link.full;
      link = { ...link, url: a.url, full: tok, token: tok ? '\u2026' + tok.slice(-4) : '', state: 'checking', polls: 0 };
      link.next = a.url !== HA ? 'failed' : tok === GOOD ? 'linked' : 'failed';
      link.why = a.url !== HA ? 'cannot reach Home Assistant at that address' : 'Home Assistant refused the token';
      send(200, 'application/json', '{"ok":true}');
    });
    return;
  }
  send(404, 'text/plain', 'not found');
}

for (const [label, width] of [['phone', 390], ['desktop', 1280]]) {
  describe(`The link page (${label}, ${width} wide)`, () => {
    let page;
    before(async () => {
      srv = createServer(panel);
      await new Promise((ok) => srv.listen(0, '127.0.0.1', ok));
      base = `http://127.0.0.1:${srv.address().port}`;
      browser ??= await chromium.launch({ executablePath: process.env.CHROMIUM_PATH ?? '/usr/bin/chromium' });
      page = await (await browser.newContext({ viewport: { width, height: 900 } })).newPage();
    });
    after(async () => { await page.context().close(); srv.close(); if (label === 'desktop') await browser.close(); });
    const state = () => page.locator('#state-text').textContent();
    const why = async () => ((await page.locator('#why').isHidden()) ? '' : page.locator('#why').textContent());
    const send = async (url, token) => {
      await page.fill('#url', url); await page.fill('#token', token);
      await page.click('#send');
      await page.waitForFunction(() => !document.querySelector('#send').disabled);
    };

    it('not set up: says so, and asks for a token', async () => {
      reset();
      await page.goto(`${base}/link`);
      await page.waitForFunction(() => !/Reading/.test(document.querySelector('#state-text').textContent));
      assert.equal(await state(), 'Not set up');
      await send(HA, '');
      assert.match(await why(), /Paste a long-lived access token/);
      assert.equal(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), true, 'no sideways scroll');
    });
    it('a wrong token: Not reached, why in words, the token cleared and never shown', async () => {
      await send(HA, 'wrongtoken-9876');
      assert.equal(await state(), 'Not reached');
      assert.equal(await why(), 'Home Assistant refused the token.');
      assert.equal(await page.inputValue('#token'), '', 'cleared after sending');
      assert.ok(!(await page.content()).includes('wrongtoken-9876'), 'the token is nowhere on the page');
      assert.match(await page.locator('#token-hint').textContent(), /ending 9876/);
    });
    it('an address that is not there: Not reached, said in words', async () => {
      await send('http://nowhere.local:8123', GOOD);
      assert.equal(await state(), 'Not reached');
      assert.equal(await why(), 'Cannot reach Home Assistant at that address.');
    });
    it('not an address: said before sending', async () => {
      await send('homeassistant.local', GOOD);
      assert.match(await why(), /starts with http:\/\/ or https:\/\//);
    });
    it('the right address and token: Linked, the host, the token\'s last 4 only', async () => {
      await send(HA, GOOD);
      assert.equal(await state(), 'Linked, homeassistant.local');
      assert.equal(await why(), '');
      assert.ok(!(await page.content()).includes(GOOD), 'the token is nowhere on the page');
      assert.match(await page.locator('#token-hint').textContent(), /ending _XYZ/);
    });
  });
}
