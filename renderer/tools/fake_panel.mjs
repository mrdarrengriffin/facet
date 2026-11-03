#!/usr/bin/env node
// A fake Facet panel on this machine: the WASM module (the device app, Home first) behind an HTTP server that answers
// the routes the /ai prompt names as the firmware does, so a fresh agent can be tried against it with no real panel.
//   node renderer/tools/fake_panel.mjs [--port 8099] [--den 1x|1.5x] [--auto-allow <s>]
// Allow API access is on. Build = FW_OUT (env or local.env), else renderer/out (renderer/build.sh <den>).
// Shared C++ (the core, through fw_test "keys" / fw_spec, the same calls the firmware's glue makes): the keys (request,
// poll, list, revoke, the gate, expiry), the glass's Allow sheet, keys::change + its Undo toast, spec check / screen /
// deck / revert, actions. Mirrored here in JS (firmware/fw_device.h is ESP-IDF only): the HTTP layer (routing, status
// codes, the 32 KB body cap, ESPHome dropping form-encoded bodies, CORS), GET /ai's {panel} fill, GET /llms.txt + /agents.md (tools/gen_agents.mjs), GET /schema.json
// (catalogue/schema.json as is), GET /fw/info (build, board, size; no heap), GET /fw/ha?want= (the fake house: tools/demo_ha.mjs DEMO, no HA link), and POST /fw/show {screen}, which runs the
// action nav.open (the firmware's screen_go also takes a page's name; here an id only).
// Test routes: POST /fake/tap {"allow": true|false} presses Allow / Deny on the glass's sheet; GET /fake/shot = the
// screen as a PNG; GET /fake/log = every request [{t, method, path, status, ms}]. Test: tools/fake_panel.test.mjs.
import { execFileSync } from 'node:child_process';
import { createServer } from 'node:http';
import { existsSync, readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { Bench, setBuild, shot } from './lib/fwbench.mjs';
import { DEMO } from './demo_ha.mjs';

const HERE = join(dirname(fileURLToPath(import.meta.url)), '..');
const env = (() => { try { return Object.fromEntries(readFileSync(join(HERE, '..', 'local.env'), 'utf8').split('\n').map((l) => l.match(/^([A-Z_]+)=(.*)$/)).filter(Boolean).map((m) => [m[1], m[2]])); } catch { return {}; } })();

export async function start({ port = 8099, den = '1.5x', autoAllow = 0, build = process.env.FW_OUT || env.FW_OUT || join(HERE, 'out'), quiet = false } = {}) {
  if (!existsSync(join(build, den, 'fw.wasm'))) throw new Error(`no ${den} build in ${build} (renderer/build.sh ${den})`);
  setBuild(build);
  const b = new Bench(null, den);
  await b.render('device', {}, 0);
  b.settle(1500);
  const K = (o) => b.test('keys', JSON.stringify(o));
  K({ op: 'switch', on: true });
  const clock = setInterval(() => { try { b.tick(2); b.events(); } catch { /* a request mid-tick */ } }, 32);  // LVGL time follows the wall clock
  const log = [];
  const base = `http://localhost:${port}`;
  const board = den === '1x' ? { id: 'guition-4848s040', w: 480, h: 480 } : { id: 'waveshare-p4-86', w: 720, h: 720 };
  const manifest = JSON.parse(readFileSync(join(build, den, 'manifest.json'), 'utf8'));
  // the glass: press Allow / Deny on the sheet (a finger on its button)
  const press = (allow) => {
    const o = b.audit().find((x) => x.text && (allow ? /^allow$/i : /^deny$/i).test(String(x.text)));
    if (!o) return false;
    b.tap(o.x + o.w / 2, o.y + o.h / 2);
    b.settle(1500);
    return true;
  };
  let auto = null;
  const asked = () => { if (autoAllow > 0) { clearTimeout(auto); auto = setTimeout(() => press(true), autoAllow * 1000); } };

  const route = (method, path, body, auth, query = '') => {  // -> [status, type, payload]
    const J = (code, o) => [code, 'application/json', typeof o === 'string' ? o : JSON.stringify(o)];
    if (method === 'OPTIONS' && path.startsWith('/fw/')) return [204, 'text/plain', ''];
    if (method === 'GET' && path === '/ai') return [200, 'text/plain; charset=utf-8', readFileSync(join(HERE, '..', 'firmware', 'ai-prompt.txt'), 'utf8').replaceAll('{panel}', base)];
    if (method === 'GET' && (path === '/schema.json' || path === '/fw/schema')) return [200, 'application/json', readFileSync(join(HERE, 'catalogue', 'schema.json'), 'utf8')];
    if (method === 'GET' && (path === '/llms.txt' || path === '/agents.md')) return [200, 'text/plain; charset=utf-8', execFileSync('node', [join(HERE, 'tools/gen_agents.mjs'), '--flavour', 'firmware', '--print', path.slice(1)], { encoding: 'utf8' })];
    if (method === 'GET' && path === '/fake/shot') return [200, 'image/png', shot(b)];
    if (method === 'GET' && path === '/fake/log') return J(200, log);
    if (method === 'POST' && path === '/fake/tap') { let a = true; try { a = JSON.parse(body || '{}').allow !== false; } catch { /* allow */ } return J(press(a) ? 200 : 404, { ok: true }); }
    if (path === '/fw/keys' || path.startsWith('/fw/keys/')) {
      const r = K({ op: 'http', method, path, body: body ?? '', auth });
      if (method === 'POST' && path === '/fw/keys/request' && r.code === 200) { b.settle(1500); asked(); }
      return J(r.code, r.body);
    }
    if (!path.startsWith('/fw/')) return J(404, { ok: false, error: 'not found' });
    if (method === 'GET') {
      if (path === '/fw/info') return J(200, { v: manifest.built, density: den, board: board.id, w: board.w, h: board.h, shown: { id: 'device' }, fake: true });
      if (path === '/fw/spec') return J(200, b.spec('hello', ''));
      if (path === '/fw/ha') {  // ?want=: the fake house's entities (tools/demo_ha.mjs DEMO), a missing one is absent
        const want = new URLSearchParams(query).get('want')?.split(',').filter(Boolean) ?? [];
        const values = {};
        for (const e of want) if (DEMO[e]) { values[e] = DEMO[e]['']; values[`${e}|friendly_name`] = DEMO[e].friendly_name ?? e; }
        return J(200, want.length ? { values, bound: 0 } : { wants: [], values: {}, bound: 0 });
      }
      if (path === '/fw/deck') { const l = JSON.parse(b.spec('list', '')); const d = JSON.parse(l.deck); return J(200, { deck: d, rev: d.rev, override: true }); }
      if (path === '/fw/act') return J(200, { usage: 'POST /fw/act {"do": "<action>", ...fields} with Authorization: Bearer <key> -> {ok, why?, state?}', actions: JSON.parse(b.spec('actions', '')).actions });
      return J(404, { ok: false, error: 'docs: /ai /llms.txt /schema.json; GET /fw/info, /fw/spec, /fw/deck, /fw/act, /fw/keys' });
    }
    if (body === undefined || body.length === 0) return J(400, { ok: false, error: 'body is not JSON' });
    if (body.length > 32768) return J(400, { ok: false, error: 'body over 32 KB' });
    let o;
    try { o = JSON.parse(body); } catch { return J(400, { ok: false, error: 'body is not JSON' }); }
    let r;
    if (path === '/fw/spec') {
      r = K({ op: 'write', route: '/fw/spec', body: o, auth });
    } else if (path === '/fw/show') r = K({ op: 'write', route: '/fw/act', body: { do: 'nav.open', screen: o.screen ?? o.id }, auth });
    else if (path === '/fw/act') r = K({ op: 'write', route: '/fw/act', body: o, auth });
    else if (path === '/fw/settings') r = K({ op: 'write', route: '/fw/settings', body: o, auth });
    else return J(404, { ok: false, error: 'not on the fake panel' });
    b.settle(1500);
    return J(r.code, r.body);
  };

  const server = createServer((req, res) => {
    const t0 = Date.now();
    const chunks = [];
    req.on('data', (c) => chunks.push(c));
    req.on('end', () => {
      const path = (req.url ?? '/').split('?')[0];
      // ESPHome's web server hands a handler the raw body only for other content types (form bodies become args)
      const form = /x-www-form-urlencoded|multipart\/form-data/i.test(req.headers['content-type'] ?? '');
      const body = req.method === 'POST' && !form ? Buffer.concat(chunks).toString('utf8') : undefined;
      let out;
      try { out = route(req.method ?? 'GET', path, body, String(req.headers.authorization ?? ''), (req.url ?? '').split('?')[1] ?? ''); } catch (e) { out = [500, 'application/json', JSON.stringify({ ok: false, error: String(e?.message ?? e) })]; }
      const [code, type, payload] = out;
      res.writeHead(code, { 'Content-Type': type, 'Cache-Control': 'no-store', 'Access-Control-Allow-Origin': req.headers.origin ?? '*', 'Access-Control-Allow-Methods': 'GET, POST, OPTIONS', 'Access-Control-Allow-Headers': 'Authorization, Content-Type', Vary: 'Origin' });
      res.end(payload);
      if (!path.startsWith('/fake/')) log.push({ t: new Date().toISOString(), method: req.method, path, status: code, ms: Date.now() - t0 });
      if (!quiet && !path.startsWith('/fake/')) console.log(`${req.method} ${path} ${code}`);
    });
  });
  await new Promise((ok) => server.listen(port, ok));
  return { url: base, bench: b, close: () => new Promise((ok) => { clearInterval(clock); clearTimeout(auto); server.close(ok); }) };
}

if (import.meta.url === pathToFileURL(process.argv[1] ?? '').href) {
  const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
  const p = await start({ port: Number(arg('--port', 8099)), den: arg('--den', '1.5x'), autoAllow: Number(arg('--auto-allow', 0)) });
  console.log(`fake panel at ${p.url} (prompt: ${p.url}/ai; tap Allow: POST ${p.url}/fake/tap {"allow": true}; screen: ${p.url}/fake/shot)`);
}
