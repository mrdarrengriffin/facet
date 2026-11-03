// API keys (core/keys.h) and Settings > API keys (screens/agent_keys.cpp), driven as the firmware's HTTP glue drives
// them (fw_test "keys": http = a /fw/keys route, write = the gate + a change through keys::change). The glass's
// "Allow <name>?" sheet is tapped like a person would.
const K = (t, o) => { const r = t.b.test('keys', JSON.stringify(o)); t.b.settle(600); return r; };
const on = (t, v = true) => K(t, { op: 'switch', on: v });
const ask = (t, name = 'Claude Code on Sam\'s laptop', expires) => K(t, { op: 'http', method: 'POST', path: '/fw/keys/request', body: { name, ...(expires ? { expires } : {}) } });
const poll = (t, id) => K(t, { op: 'http', method: 'POST', path: '/fw/keys/poll', body: { id } });
const keys = (t) => K(t, { op: 'http', method: 'GET', path: '/fw/keys' }).body;
const write = (t, auth, body = { values: { theme: 'light' } }, route = '/fw/settings') => K(t, { op: 'write', route, body, auth: auth ? `Bearer ${auth}` : '' });
const settle = (t) => { t.b.settle(1500); t.wait(400); };
// a key the glass allowed (the sheet tapped), as an agent would get it
const allowed = (t, name, expires) => {
  const r = ask(t, name, expires); settle(t);
  t.tap(/^Allow$/i); settle(t);
  return poll(t, r.body.id).body.key;
};
const DAY = 86400;

export default {
  id: 'agent_keys', family: 'device', about: 'API keys: asked for with a name, allowed on the glass, sent on every write, revoked, ended and dropped.',
  tests: [
    { name: 'a request shows Allow <name>? on the glass; Allow hands the key over once', async run(t) {
      await t.device(); K(t, { op: 'reset' }); on(t);
      const r = ask(t);
      t.eq([r.code, r.body.state, r.body.wait_s], [200, 'waiting', 120], 'the request is waiting');
      settle(t);
      t.has(t.texts(), 'Allow Claude Code on Sam\'s laptop?', 'the glass asks');
      t.has(t.texts(), /^Allow$/i, 'an Allow button');
      t.has(t.texts(), /^api key$/i, 'the sheet is labelled API key');
      t.has(t.texts(), /^Deny$/i, 'a Deny button');
      t.eq(poll(t, r.body.id).body.state, 'waiting', 'waiting until a tap');
      t.tap(/^Allow$/i); settle(t);
      t.hasNot(t.texts(), /^Allow Claude/, 'the sheet is gone');
      const p = poll(t, r.body.id).body;
      t.eq([p.state, /^fk-[0-9a-f]{32}$/.test(p.key || '')], ['allowed', true], 'the key, once');
      const again = poll(t, r.body.id);
      t.eq([again.code, again.body.key], [410, undefined], 'never twice');
      const l = keys(t);
      t.eq([l.keys.length, l.keys[0].name, l.keys[0].ends > l.keys[0].made], [1, 'Claude Code on Sam\'s laptop', true], 'listed with its name, a tap-issued key has an end (30 days unused)');
      t.ok(!JSON.stringify(l).includes(p.key) && !JSON.stringify(l).includes('hash'), 'the list never shows a key or its hash');
      const stored = K(t, { op: 'reboot' }).stored;
      t.ok(stored.includes(l.keys[0].id) && !stored.includes(p.key), 'stored as a hash, not the key');
      t.eq(write(t, p.key).code, 200, 'the key still works after a reboot');
    } },
    { name: 'Deny, closing the sheet and no tap in two minutes are each a plain no', async run(t) {
      await t.device(); K(t, { op: 'reset' }); on(t);
      const a = ask(t, 'Script A'); settle(t);
      t.tap(/^Deny$/i); settle(t);
      const pa = poll(t, a.body.id).body;
      t.eq([pa.state, pa.key], ['denied', undefined], 'Deny: no key');
      t.ok(/not allowed/.test(pa.error), `one plain line: ${pa.error}`);
      const b = ask(t, 'Script B'); settle(t);
      t.tap({ at: [t.b.S / 2, 8] }); settle(t);  // the scrim above the sheet: closes it, as Close and the panel key do
      t.eq(poll(t, b.body.id).body.state, 'denied', 'closed = Deny');
      const c = ask(t, 'Script C'); settle(t);
      t.wait(121000); settle(t);
      const pc = poll(t, c.body.id).body;
      t.eq(pc.state, 'denied', 'no tap in two minutes = Deny');
      t.ok(/two minutes/.test(pc.error), `says why: ${pc.error}`);
      t.hasNot(t.texts(), /^Allow Script/, 'the sheet closed by itself');
      t.eq(keys(t).keys.length, 0, 'no key was made');
    } },
    { name: 'the header: a key is accepted, none or a wrong one is refused, reads need nothing', async run(t) {
      await t.device(); K(t, { op: 'reset' }); on(t);
      const key = allowed(t, 'Bench');
      settle(t);
      const w = write(t, key, { values: { theme: 'light' } });
      t.eq([w.code, t.setting('theme')], [200, 'light'], 'a write with the key lands');
      const none = write(t, '');
      t.eq([none.code, none.body.agents], [401, 'key'], 'no header: 401');
      t.ok(/POST \/fw\/keys\/request/.test(none.body.error), `says how to get one: ${none.body.error}`);
      const bad = write(t, 'fk-' + '0'.repeat(32));
      t.eq([bad.code, bad.body.agents], [401, 'key'], 'a wrong key: 401');
      t.eq(K(t, { op: 'write', route: '/fw/settings', body: { values: { theme: 'dark' } }, editor: true }).code, 200, 'the editor needs no key');
      t.eq(keys(t).keys[0].used > 0, true, 'last used is kept');
    } },
    { name: 'the switch off refuses every key and every request', async run(t) {
      await t.device(); K(t, { op: 'reset' });
      const r0 = ask(t);
      t.eq([r0.code, r0.body.agents], [403, 'off'], 'off on a new panel: no request');
      on(t);
      const key = allowed(t, 'Bench');
      on(t, false);
      const w = write(t, key);
      t.eq([w.code, w.body.agents], [403, 'off'], 'off: the key is refused');
      t.ok(/Allow API access/.test(w.body.error), `one plain line: ${w.body.error}`);
      t.eq(K(t, { op: 'write', route: '/fw/settings', body: { values: { aiAgents: true } }, auth: `Bearer ${key}` }).code, 403, 'a key cannot turn it on');
      on(t);
      t.eq(write(t, key).code, 200, 'on again: the key works');
    } },
    { name: 'a 30-day key ends after 30 days unused: each use moves its end on', async run(t) {
      await t.device(); K(t, { op: 'reset' }); on(t);
      const key = allowed(t, 'Thirty days');
      t.eq(keys(t).keys[0].ends_words, 'Ends after 30 days unused', 'the words say unused, not a fixed date');
      for (let i = 0; i < 3; i++) { K(t, { op: 'skew', s: 20 * DAY }); t.eq(write(t, key).code, 200, `used again after 20 days (${(i + 1) * 20} days old)`); }
      K(t, { op: 'skew', s: 29 * DAY });
      t.eq(write(t, key).code, 200, '29 days unused: still works (the use moved the end on again)');
      K(t, { op: 'skew', s: 30 * DAY + 60 });
      t.eq(write(t, key).code, 401, '30 days unused: refused');
      t.eq(keys(t).keys.length, 0, 'and dropped');
    } },
    { name: 'a 1-day key ends on time even if used; until revoked stays, 90 days unused is dropped', async run(t) {
      await t.device(); K(t, { op: 'reset' }); on(t);
      const day = allowed(t, 'One day', '1d'), kept = allowed(t, 'Kept', 'never');
      t.eq(keys(t).keys.map((k) => k.ends_words), ['Ends within a day', 'Until revoked'], 'the ends words');
      K(t, { op: 'skew', s: 12 * 3600 });
      t.eq(write(t, day).code, 200, 'used at 12 h');
      K(t, { op: 'skew', s: 12 * 3600 + 60 });
      t.eq([write(t, day).code, write(t, kept).code], [401, 200], 'a day on: the 1-day key ended though it was used');
      K(t, { op: 'skew', s: 89 * DAY });
      t.eq(write(t, kept).code, 200, '89 days unused: kept');
      K(t, { op: 'skew', s: 91 * DAY });
      t.eq(write(t, kept).code, 401, 'unused for 91 days: dropped');
      t.eq(keys(t).keys.length, 0, 'gone from the list');
    } },
    { name: 'revoke one, revoke all on the glass (hold), Settings > API keys lists them', async run(t) {
      await t.device(); K(t, { op: 'reset' }); on(t);
      const a = allowed(t, 'Claude Code'), b = allowed(t, 'Backup script'), c = allowed(t, 'Spare');
      const id = keys(t).keys.find((k) => k.name === 'Spare').id;
      t.eq(K(t, { op: 'http', method: 'POST', path: '/fw/keys/revoke', body: { id } }).code, 401, 'revoke needs a key');
      t.eq(K(t, { op: 'http', method: 'POST', path: '/fw/keys/revoke', body: { id }, auth: `Bearer ${a}` }).body.revoked, 1, 'revoked with a key');
      t.eq(write(t, c).code, 401, 'the revoked key is refused');
      t.b.spec('act', { do: 'nav.open', screen: 'settings' }); settle(t);
      const all = t.objs().map((o) => String(o.text ?? ''));
      t.ok(all.includes('API keys') && all.includes('2 keys'), 'Settings: an API keys row counting the keys');
      t.b.spec('act', { do: 'nav.open', screen: 'agent_keys' }); settle(t);
      t.eq(t.where(), 'agent_keys', 'Settings > API keys');
      t.has(t.texts(), /^api keys$/i, 'the screen is called API keys');
      t.has(t.texts(), 'Claude Code', 'a row per key');
      t.has(t.texts(), 'Backup script', 'a row per key');
      t.has(t.texts(), /^Used just now · Ends after 30 days unused$/, 'used and ends words');
      t.hold(/^Revoke all$/i, 1600); settle(t);
      t.eq(keys(t).keys.length, 0, 'Revoke all: none left');
      t.has(t.texts(), /^No keys yet/, 'the page follows: none');
      t.eq([write(t, a).code, write(t, b).code], [401, 401], 'both refused');
    } },
    { name: 'Settings: the API keys count follows a key allowed while Settings shows', async run(t) {
      await t.device(); K(t, { op: 'reset' }); on(t);
      t.b.spec('act', { do: 'nav.open', screen: 'settings' }); settle(t);
      for (let i = 0; i < 8 && !t.texts().includes('API keys'); i++) { t.drag({ at: [4, t.b.S * 0.8] }, { by: [0, -t.b.S * 0.4] }); t.wait(500); }
      t.has(t.texts(), 'No keys', 'none yet');
      const r = ask(t, 'Kitchen script'); settle(t);
      t.tap(/^Allow$/i); settle(t); t.wait(600); settle(t);
      t.eq(poll(t, r.body.id).body.state, 'allowed', 'allowed');
      t.has(t.texts(), '1 key', 'the row says 1 key without leaving Settings');
    } },
    { name: 'Allow while Settings > API keys shows, after Revoke all: the page follows, the panel stays up', async run(t) {
      await t.device(); K(t, { op: 'reset' }); on(t);
      allowed(t, 'Old');
      t.b.spec('act', { do: 'nav.open', screen: 'agent_keys' }); settle(t);
      t.hold(/^Revoke all$/i, 1600); settle(t);
      t.has(t.texts(), /^No keys yet/, 'none after Revoke all');
      const r = ask(t, 'claude-workbench', 'never'); settle(t);
      t.has(t.texts(), 'Allow claude-workbench?', 'the glass asks over the page');
      t.tap(/^Allow$/i); settle(t); t.wait(600); settle(t);
      t.eq(poll(t, r.body.id).body.state, 'allowed', 'allowed');
      t.eq(t.where(), 'agent_keys', 'still on API keys');
      t.has(t.texts(), 'claude-workbench', 'the page lists the new key');
    } },
    { name: 'a change made with a key toasts with Undo; Undo puts it back', async run(t) {
      await t.device(); K(t, { op: 'reset' }); on(t);
      const key = allowed(t, 'Claude Code');
      settle(t); t.wait(13000);
      const was = t.setting('theme');
      write(t, key, { values: { theme: was === 'light' ? 'dark' : 'light' } }); t.b.settle(800);
      t.has(t.texts(), /^Mode set to (Dark|Light)$/, 'the toast says what changed');
      t.has(t.texts(), 'By Claude Code', 'and which key');
      t.tap(/^Undo$/i); settle(t);
      t.eq(t.setting('theme'), was, 'Undo: the setting is back');
      t.has(t.texts(), 'Undone', 'said so');
      t.wait(13000);
      const deck = JSON.parse(t.b.spec('list', '')).deck;
      const d = JSON.parse(deck); d.rev = (d.rev || 0) + 1;
      const i = d.pages.findIndex((p) => p.kind !== 'home'); d.pages.push(d.pages.splice(i, 1)[0]);  // a page moved to the end
      const w = write(t, key, { kind: 'deck', spec: d }, '/fw/spec');
      t.eq(w.code, 200, `the deck lands: ${JSON.stringify(w.body).slice(0, 160)}`);
      t.b.settle(800);
      t.has(t.texts(), 'Page list changed', 'the deck change toasts');
      t.tap(/^Undo$/i); settle(t);
      t.eq(JSON.parse(t.b.spec('list', '')).deck, deck, 'Undo: the deck is back');
      t.wait(13000);
      act(t, key, 'http://ha.one:8123', 'tokenone1234');
      t.wait(13000);
      act(t, key, 'http://ha.two:8123', 'tokentwo5678');
      t.tap(/^Undo$/i); settle(t);
      const link = JSON.parse(t.b.m.UTF8ToString(t.b.m._fw_ha_link()));
      t.eq([link.url, link.token], ['http://ha.one:8123', '…1234'], 'Undo: the HA link is back, its token too');
    } },
  ],
};
function act(t, key, url, token) {
  const r = write(t, key, { do: 'ha.connect', url, token }, '/fw/act'); t.b.settle(800);
  t.ok(r.code === 200 && t.texts().includes('Home Assistant link changed'), `ha.connect toasts: ${r.code} ${JSON.stringify(r.body).slice(0, 120)}`);
}
