// Every HTTP status the panel answers reaches the client as itself: ESPHome's web_server_idf sends only the codes it
// knows (200 204 400 401 404 409 422) and any other as 500, so firmware/fw_device.h sets the line itself for the rest
// (its LINES). This reads every code core/keys.cpp and fw_device.h can answer and checks each is one or the other.
//   node --test renderer/tools/http_status.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const read = (p) => readFileSync(join(ROOT, p), 'utf8');
const SERVER = new Set([200, 204, 400, 401, 404, 409, 422]);

test('every status the panel answers is sent as itself, not as 500', () => {
  const dev = read('firmware/fw_device.h'), keys = read('renderer/core/keys.cpp');
  const lines = new Set([...dev.matchAll(/\{(\d{3}), "\1 [^"]+"\}/g)].map((m) => +m[1]));
  assert.ok(lines.has(403), 'fw_device.h sets the 403 line (API access off)');
  const used = new Set([...(dev + keys).matchAll(/\bcode = (\d{3})\b/g)].map((m) => +m[1]));
  assert.ok(used.has(403) && used.has(503), 'found the codes the panel answers');
  const lost = [...used].filter((c) => !SERVER.has(c) && !lines.has(c));
  assert.deepEqual(lost, [], `codes sent as 500: add them to LINES in firmware/fw_device.h`);
});
