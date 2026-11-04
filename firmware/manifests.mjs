#!/usr/bin/env node
// The public firmware's two manifests per board, from its images (.github/workflows/release.yml, esphome/release.sh):
// manifest.json (ESP Web Tools, the Install page: the factory image at offset 0) and ota.json (the ESPHome update
// entity: the OTA image + its md5). Release channels: a version with a pre-release part (0.5.0-beta.1) is the beta
// channel and writes manifest-beta.json + ota-beta.json beside the stable pair (the panel's Update channel setting and
// the Install page's Beta switch read those); a plain version (1.0.0) is stable. ota.summary is the short version of
// release-notes/<version>.md (firmware/release-notes.mjs: Home Assistant shows it as the release notes) and
// ota.release_url the version's GitHub release. ota.json's notes: one short summary per version since the last stable
// release, newest first (release-notes.mjs entries): the panel shows those newer than its own (esphome/board/fw_notes.h).
//   node firmware/manifests.mjs <dir> <version>
// dir/<board>/facet-<board>-<version>.{factory,ota}.bin must exist; the board's name and chip come from boards/<board>.json.
// Refuses a version with "+wip", a missing image and missing release notes. Prints one line per board.
import { createHash } from 'node:crypto';
import { existsSync, readFileSync, readdirSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { REPO, entries, summary as notesSummary } from './release-notes.mjs';

const ROOT = dirname(dirname(fileURLToPath(import.meta.url)));
const [dir, version] = process.argv.slice(2);
if (!dir || !version) { console.error('usage: node firmware/manifests.mjs <dir> <version>'); process.exit(2); }
if (version.includes('+wip')) { console.error(`refused: ${version} is a build from a changed tree`); process.exit(1); }
let summary, notes;
try { summary = notesSummary(version); notes = entries(version); } catch (e) { console.error(e.message); process.exit(1); }
export const channel = (v) => (/^\d+\.\d+\.\d+-/.test(v) ? 'beta' : 'stable');
const SUFFIX = channel(version) === 'beta' ? '-beta' : '';
const boards = readdirSync(dir).filter((b) => existsSync(join(ROOT, 'boards', `${b}.json`)));
if (!boards.length) { console.error(`no <board>/ in ${dir}`); process.exit(1); }
for (const b of boards) {
  const meta = JSON.parse(readFileSync(join(ROOT, 'boards', `${b}.json`), 'utf8'));
  const name = `Facet for the ${meta.name}`, chip = meta.chip;
  if (!meta.name || !chip) { console.error(`boards/${b}.json: needs name and chip`); process.exit(1); }
  const fac = `facet-${b}-${version}.factory.bin`, ota = `facet-${b}-${version}.ota.bin`;
  for (const f of [fac, ota]) if (!existsSync(join(dir, b, f))) { console.error(`${b}: missing ${f}`); process.exit(1); }
  const md5 = createHash('md5').update(readFileSync(join(dir, b, ota))).digest('hex');
  writeFileSync(join(dir, b, `manifest${SUFFIX}.json`), JSON.stringify({ name, version, home_assistant_domain: 'esphome', new_install_prompt_erase: true,
    builds: [{ chipFamily: chip, parts: [{ path: fac, offset: 0 }] }] }, null, 2) + '\n');
  writeFileSync(join(dir, b, `ota${SUFFIX}.json`), JSON.stringify({ name, version, builds: [{ chipFamily: chip, ota: { path: ota, md5,
    release_url: `${REPO}/releases/tag/v${version}`, summary } }], notes }, null, 2) + '\n');
  console.log(`${b} ${version}: manifest${SUFFIX}.json + ota${SUFFIX}.json (ota md5 ${md5})`);
}
