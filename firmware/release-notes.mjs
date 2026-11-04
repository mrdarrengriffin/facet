#!/usr/bin/env node
// A version's release notes, written for the people who install the panel: release-notes/<version>.md (RELEASING.md).
// The file is a headline paragraph, then these sections in this order, each optional except that one of New,
// Improved or Fixed has bullets:
//   ## New  ## Improved  ## Fixed  ## After updating  ## Known issues
// Every bullet of New, Improved, Fixed and Known issues starts with a bold lead ("- **Calendar shows every
// calendar.** More detail ... #8"): the lead is what Home Assistant's update card lists.
//   node firmware/release-notes.mjs check <version>          fails when the file is missing or malformed
//   node firmware/release-notes.mjs body <version> [<prev>]  the GitHub release body (the file + full-changelog link)
//   node firmware/release-notes.mjs summary <version>        the OTA manifest's ota.summary (manifests.mjs uses it)
//   node firmware/release-notes.mjs entries <version>        the OTA manifest's notes: one short summary per version
// A section with nothing under it is left out (of the body and the summary), never shown as an empty heading.
// The summary is Markdown: Home Assistant's ESPHome update entity has no release_summary; it serves ota.summary as
// the release notes in the update dialog (RELEASE_NOTES feature), under the title and versions, with ota.release_url
// as the "release announcement" link. It is held under SUMMARY_MAX characters (the panel keeps it in RAM and sends it
// on every update state): the headline, After updating in full, then each section's leads until the limit (Home Assistant shows all of it:
// only release_summary, which ESPHome doesn't fill, is cut at 255).
// The manifest's notes (entries) are the per-version summaries the panel joins into the update dialog for the versions
// between the installed one and the offered one (esphome/board/fw_notes.h): every version with notes on the channel
// since the last stable release (stable: the stable versions), newest first, at most ENTRY_MAX characters each;
// Known issues only in the newest (still-open ones are listed there again). ota.summary stays, for older firmware.
import { existsSync, readFileSync, readdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = dirname(dirname(fileURLToPath(import.meta.url)));
export const REPO = 'https://github.com/mrdarrengriffin/facet';
export const SUMMARY_MAX = 1200;
export const ENTRY_MAX = 1000;
const SECTIONS = ['New', 'Improved', 'Fixed', 'After updating', 'Known issues'];
const LEAD = /^\*\*(.+?)\*\*/;

export const file = (v) => join(ROOT, 'release-notes', `${v}.md`);

export function parse(v) {
  const f = file(v);
  if (!existsSync(f)) throw new Error(`release-notes/${v}.md is missing: write the notes for ${v} first (RELEASING.md)`);
  const text = readFileSync(f, 'utf8').replace(/\r/g, '');
  const parts = text.split(/^## +/m);
  const headline = parts.shift().trim();
  const errs = [];
  if (!headline || headline.startsWith('#')) errs.push('no headline paragraph before the first section');
  const sections = {};
  let last = -1;
  for (const p of parts) {
    const name = p.slice(0, p.indexOf('\n') < 0 ? p.length : p.indexOf('\n')).trim();
    const at = SECTIONS.indexOf(name);
    if (at < 0) { errs.push(`unknown section "## ${name}" (${SECTIONS.join(', ')})`); continue; }
    if (at <= last) errs.push(`"## ${name}" is out of order (${SECTIONS.join(', ')})`);
    last = at;
    const body = p.slice(name.length).trim();
    if (!body) continue;  // an empty section is left out, never an empty heading
    const bullets = body.split('\n').filter((l) => /^- /.test(l)).map((l) => l.slice(2).trim());
    if (!bullets.length && name !== 'After updating') errs.push(`"## ${name}" has no bullets`);
    if (name !== 'After updating') for (const b of bullets) if (!LEAD.test(b)) errs.push(`"## ${name}": no bold lead: ${b.slice(0, 60)}`);
    sections[name] = { body, bullets };
  }
  if (!['New', 'Improved', 'Fixed'].some((s) => sections[s]?.bullets.length)) errs.push('nothing under New, Improved or Fixed');
  if (errs.length) throw new Error(`release-notes/${v}.md:\n  ${errs.join('\n  ')}`);
  const out = [headline, ...SECTIONS.filter((s) => sections[s]).map((s) => `## ${s}\n\n${sections[s].body}`)].join('\n\n');
  return { headline, sections, text: out };
}

export function body(v, prev) {
  const n = parse(v);
  const link = prev ? `${REPO}/compare/${prev}...v${v}` : `${REPO}/commits/v${v}`;
  return `${n.text}\n\n**Full changelog**: ${link}\n`;
}

// issue refs and links mean nothing in the update dialog: plain text
const plain = (s) => s.replace(/\[([^\]]+)\]\([^)]+\)/g, '$1').replace(/\s*\((?:#\d+[,\s]*)+\)/g, '').replace(/\s+#\d+\b/g, '').trim();

export function summary(v, { max = SUMMARY_MAX, known = true } = {}) {
  const n = parse(v);
  let out = plain(n.headline);
  const add = (s) => (out.length + s.length <= max ? ((out += s), true) : false);
  if (n.sections['After updating']) add(`\n\n**After updating**\n\n${plain(n.sections['After updating'].body)}`);
  for (const s of ['New', 'Improved', 'Fixed', ...(known ? ['Known issues'] : [])]) {
    const leads = (n.sections[s]?.bullets ?? []).map((b) => `\n- ${plain(b.match(LEAD)[1])}`);
    if (!leads.length || !add(`\n\n**${s}**\n${leads[0]}`)) continue;  // a section with no room is left out whole
    let i = 1;
    for (; i < leads.length; i++) if (!add(leads[i])) break;
    if (i < leads.length) out += `\n- and ${leads.length - i} more`;
  }
  return out;
}

// X.Y.Z or X.Y.Z-tag.N: a pre-release sorts before its release, tags dev < alpha < beta < rc
const TAGS = ['dev', 'alpha', 'beta', 'rc'];
const key = (v) => {
  const m = /^(\d+)\.(\d+)\.(\d+)(?:-([a-z]+)(?:\.(\d+))?)?$/.exec(v);
  if (!m) throw new Error(`not a version: ${v}`);
  return [+m[1], +m[2], +m[3], m[4] ? TAGS.indexOf(m[4]) : TAGS.length, +(m[5] ?? 0)];
};
export const cmp = (a, b) => { const x = key(a), y = key(b); for (let i = 0; i < 5; i++) if (x[i] !== y[i]) return x[i] - y[i]; return 0; };
const stable = (v) => !v.includes('-');

// the manifest's notes for <version>: [{ version, summary }], newest first
export function entries(v) {
  const all = readdirSync(join(ROOT, 'release-notes')).filter((f) => f.endsWith('.md')).map((f) => f.slice(0, -3))
    .filter((x) => /^\d+\.\d+\.\d+(-[a-z]+(\.\d+)?)?$/.test(x) && cmp(x, v) <= 0).sort(cmp).reverse();
  const floor = stable(v) ? null : all.find((x) => stable(x));  // the last stable release below a beta
  const list = all.filter((x) => (stable(v) ? stable(x) : !floor || cmp(x, floor) > 0));
  if (list[0] !== v) parse(v);  // the version's own notes must exist (throws)
  return list.map((x, i) => ({ version: x, summary: summary(x, { max: ENTRY_MAX, known: i === 0 }) }));
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const [cmd, v, prev] = process.argv.slice(2);
  if (!['check', 'body', 'summary', 'entries'].includes(cmd) || !v) {
    console.error('usage: node firmware/release-notes.mjs check|body|summary|entries <version> [<previous tag>]');
    process.exit(2);
  }
  try {
    if (cmd === 'check') {
      const s = summary(v), e = entries(v);
      console.log(`release-notes/${v}.md: ok, summary ${s.length} chars, notes ${e.map((x) => `${x.version} ${x.summary.length}`).join(', ')}`);
    } else if (cmd === 'entries') process.stdout.write(JSON.stringify(entries(v), null, 2) + '\n');
    else process.stdout.write(cmd === 'body' ? body(v, prev) : summary(v) + '\n');
  } catch (e) { console.error(`::error::${e.message}`); process.exit(1); }
}
