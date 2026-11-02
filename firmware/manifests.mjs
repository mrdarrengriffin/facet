#!/usr/bin/env node
// The public firmware's two manifests per board, from its images (esphome/release.sh): manifest.json (ESP Web Tools,
// the Install page: the factory image at offset 0) and ota.json (the ESPHome update entity: the OTA image + its md5).
//   node firmware/manifests.mjs <dir> <version> <commit>
// dir/<board>/facet-<board>-<version>.{factory,ota}.bin must exist; the board's name and chip come from boards/<board>.json.
// Refuses a version with "+wip" and a missing image. Prints one line per board.
import { createHash } from 'node:crypto';
import { existsSync, readFileSync, readdirSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = dirname(dirname(fileURLToPath(import.meta.url)));
const [dir, version, commit = ''] = process.argv.slice(2);
if (!dir || !version) { console.error('usage: node firmware/manifests.mjs <dir> <version> <commit>'); process.exit(2); }
if (version.includes('+wip')) { console.error(`refused: ${version} is a build from a changed tree`); process.exit(1); }
const boards = readdirSync(dir).filter((b) => existsSync(join(ROOT, 'boards', `${b}.json`)));
if (!boards.length) { console.error(`no <board>/ in ${dir}`); process.exit(1); }
for (const b of boards) {
  const meta = JSON.parse(readFileSync(join(ROOT, 'boards', `${b}.json`), 'utf8'));
  const name = `Facet for the ${meta.name}`, chip = meta.chip;
  if (!meta.name || !chip) { console.error(`boards/${b}.json: needs name and chip`); process.exit(1); }
  const fac = `facet-${b}-${version}.factory.bin`, ota = `facet-${b}-${version}.ota.bin`;
  for (const f of [fac, ota]) if (!existsSync(join(dir, b, f))) { console.error(`${b}: missing ${f}`); process.exit(1); }
  const md5 = createHash('md5').update(readFileSync(join(dir, b, ota))).digest('hex');
  writeFileSync(join(dir, b, 'manifest.json'), JSON.stringify({ name, version, home_assistant_domain: 'esphome', new_install_prompt_erase: true,
    builds: [{ chipFamily: chip, parts: [{ path: fac, offset: 0 }] }] }, null, 2) + '\n');
  writeFileSync(join(dir, b, 'ota.json'), JSON.stringify({ name, version, builds: [{ chipFamily: chip, ota: { path: ota, md5,
    release_url: `https://github.com/mrdarrengriffin/facet/releases/tag/v${version}`, summary: `Facet ${version}${commit ? ` (${commit})` : ''}` } }] }, null, 2) + '\n');
  console.log(`${b} ${version}: manifest.json + ota.json (ota md5 ${md5})`);
}
