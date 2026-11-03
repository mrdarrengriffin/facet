// The fake panel (tools/fake_panel.mjs) end to end, as the /ai prompt has an agent use it:
//   node --test renderer/tools/fake_panel.test.mjs
// request -> Allow on the glass -> poll gets the key -> check -> send -> show (with the key); a send without it is refused.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { start } from './fake_panel.mjs';

test('an agent gets a key and sends a screen; without the key it is refused', async () => {
  const p = await start({ port: 8199 + Math.floor(Math.random() * 500), quiet: true });
  try {
    const post = async (path, body, key) => {
      const r = await fetch(p.url + path, { method: 'POST', headers: { 'Content-Type': 'application/json', ...(key ? { Authorization: `Bearer ${key}` } : {}) }, body: JSON.stringify(body) });
      return { code: r.status, body: await r.json() };
    };
    const ai = await (await fetch(`${p.url}/ai`)).text();
    assert.ok(ai.includes(`${p.url}/fw/keys/request`) && !ai.includes('{panel}'), 'the prompt has the address in');
    const schema = await (await fetch(`${p.url}/schema.json`)).json();
    assert.ok(schema.routes.some((r) => r.path === '/fw/keys/request'), 'schema.json lists the key routes');

    const req = await post('/fw/keys/request', { name: 'Test agent' });
    assert.equal(req.body.state, 'waiting');
    assert.equal((await post('/fw/keys/poll', { id: req.body.id })).body.state, 'waiting');
    assert.equal((await post('/fake/tap', { allow: true })).code, 200, 'the Allow button was on the glass');
    const got = await post('/fw/keys/poll', { id: req.body.id });
    assert.equal(got.body.state, 'allowed');
    const key = got.body.key;
    assert.match(key, /^fk-[0-9a-f]{32}$/);

    const spec = { v: 1, rev: 1, id: 'agent_test', name: 'Agent test', layout: 'list', header: { label: 'Agent test' }, nodes: [{ c: 'text', p: { text: 'Hello from an agent', style: 'body' } }] };
    const refused = await post('/fw/spec', { kind: 'screen', spec }, '');
    assert.equal(refused.code, 401, 'no key: refused');
    assert.equal(refused.body.agents, 'key');
    const check = await post('/fw/spec', { kind: 'check', spec }, key);
    assert.equal(check.body.status, 'valid', JSON.stringify(check.body));
    const sent = await post('/fw/spec', { kind: 'screen', spec }, key);
    assert.equal(sent.body.status, 'applied', JSON.stringify(sent.body));
    const shown = await post('/fw/show', { screen: 'agent_test' }, key);
    assert.equal(shown.code, 200, JSON.stringify(shown.body));
    const texts = p.bench.audit().map((o) => String(o.text ?? ''));
    assert.ok(texts.includes('Hello from an agent'), 'the screen is on the glass');
    assert.ok(texts.includes('By Test agent'), 'the change toasted with the key\'s name');
    const png = await fetch(`${p.url}/fake/shot`);
    assert.equal(png.headers.get('content-type'), 'image/png');
    const log = await (await fetch(`${p.url}/fake/log`)).json();
    assert.ok(log.some((l) => l.path === '/fw/spec' && l.status === 401), 'the log has the refused send');
  } finally { await p.close(); }
});
