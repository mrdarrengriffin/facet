#!/usr/bin/env node
// A version's release notes, written for the people who install the panel: release-notes/<version>.md (RELEASING.md).
// The file is a headline paragraph, then these sections in this order, each optional except that one of New,
// Improved or Fixed has bullets:
//   ## New  ## Improved  ## Fixed  ## After updating  ## Known issues
// Every bullet of New, Improved, Fixed and Known issues starts with a bold lead ("- **Calendar shows every
// calendar.** More detail ... #8"): the lead is what Home Assistant's update card lists.
// Docs never lag a release: every bullet of New and Improved (with its sub-points) links the docs page that explains
// it (DOCS + a path whose page is in site/src/pages), or says "(no docs: <why>)"; check fails otherwise.
//   node firmware/release-notes.mjs check <version>          fails when the file is missing or malformed
//   node firmware/release-notes.mjs body <version> [<prev>]  the GitHub release body (the file + full-changelog link)
//   node firmware/release-notes.mjs summary <version>        the OTA manifest's ota.summary (manifests.mjs uses it)
//   node firmware/release-notes.mjs entries <version>        the OTA manifest's notes: one short summary per version
// A section with nothing under it is left out (of the body and the summary), never shown as an empty heading.
// A release's notes may also have "## Since <version>" after Fixed (what is new to that pre-release's testers): body
// only, never in the summary. Every version's notes end with the All changes list, a collapsed block, closed by default:
//   <details><summary>All changes (N)</summary>  + a blank line + the list + </details>
// N = its "- " lines: every closed issue of the version's milestone(s), every merged PR, then the commits neither
// covers. Generated, never hand-made:
//   node firmware/release-notes.mjs changes <version> <from tag> [<to ref>]   prints the block (gh + git)
// The block is body only: never in the summary or the manifest's notes; check fails without it (that version only).
// The summary is Markdown: Home Assistant's ESPHome update entity has no release_summary; it serves ota.summary as
// the release notes in the update dialog (RELEASE_NOTES feature), under the title and versions, with ota.release_url
// as the "release announcement" link. It is held under SUMMARY_MAX characters (the panel keeps it in RAM and sends it
// on every update state): the headline, After updating in full, then each section's leads until the limit (Home Assistant shows all of it:
// only release_summary, which ESPHome doesn't fill, is cut at 255).
// The manifest's notes (entries) are the per-version summaries the panel joins into the update dialog for the versions
// between the installed one and the offered one (esphome/board/fw_notes.h): every version with notes on the channel
// since the last stable release (stable: the stable versions), newest first, at most ENTRY_MAX characters each;
// Known issues only in the newest (still-open ones are listed there again). ota.summary stays, for older firmware.
import { execFileSync } from 'node:child_process';
import { existsSync, readFileSync, readdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = dirname(dirname(fileURLToPath(import.meta.url)));
export const REPO = 'https://github.com/mrdarrengriffin/facet';
export const SUMMARY_MAX = 1200;
export const ENTRY_MAX = 1000;
const SECTIONS = ['New', 'Improved', 'Fixed', 'Since', 'After updating', 'Known issues'];
const ALL = /\n*<details><summary>All changes \((\d+)\)<\/summary>\n\n([\s\S]*?)\n<\/details>\s*$/;
const LEAD = /^\*\*(.+?)\*\*/;
export const DOCS = 'https://mrdarrengriffin.github.io/facet/';  // the published docs (deploy.yml FACET_DOCS_BASE)
// a docs URL's page exists in the site's source (site/src/pages: <path>.astro, <path>/index.astro, or a [param] page)
export function docsPage(u) {
  const path = u.slice(DOCS.length).split(/[?#]/)[0].replace(/\/+$/, '');
  const pages = join(ROOT, 'site', 'src', 'pages');
  if (!path) return existsSync(join(pages, 'index.astro'));
  const segs = path.split('/');
  const walk = (dir, i) => {
    if (i === segs.length) return existsSync(join(dir, 'index.astro'));
    const last = i === segs.length - 1;
    if (last && existsSync(join(dir, `${segs[i]}.astro`))) return true;
    if (existsSync(join(dir, segs[i])) && walk(join(dir, segs[i]), i + 1)) return true;
    const dyn = existsSync(dir) && dir !== pages ? readdirSync(dir).filter((f) => /^\[.+\]/.test(f)) : [];  // not the root's catch-all
    return dyn.some((f) => (last && f.endsWith('.astro')) || (!f.endsWith('.astro') && walk(join(dir, f), i + 1)));
  };
  return walk(pages, 0);
}

export const file = (v) => join(ROOT, 'release-notes', `${v}.md`);

export function parse(v) {
  const f = file(v);
  if (!existsSync(f)) throw new Error(`release-notes/${v}.md is missing: write the notes for ${v} first (RELEASING.md)`);
  let text = readFileSync(f, 'utf8').replace(/\r/g, '');
  const am = ALL.exec(text);
  const all = am ? { n: +am[1], items: am[2].split('\n').filter((l) => /^- /.test(l)).length, block: am[0].trim() } : null;
  if (am) text = text.slice(0, am.index);
  const parts = text.split(/^## +/m);
  const headline = parts.shift().trim();
  const errs = [];
  if (!headline || headline.startsWith('#')) errs.push('no headline paragraph before the first section');
  const sections = {};
  let last = -1;
  for (const p of parts) {
    const name = p.slice(0, p.indexOf('\n') < 0 ? p.length : p.indexOf('\n')).trim();
    const at = SECTIONS.indexOf(/^Since \d+\.\d+\.\d+-[a-z]+\.\d+$/.test(name) ? 'Since' : name);
    if (at < 0) { errs.push(`unknown section "## ${name}" (${SECTIONS.join(', ')})`); continue; }
    if (at <= last) errs.push(`"## ${name}" is out of order (${SECTIONS.join(', ')})`);
    last = at;
    const body = p.slice(name.length).trim();
    if (!body) continue;  // an empty section is left out, never an empty heading
    const bullets = body.split('\n').filter((l) => /^- /.test(l)).map((l) => l.slice(2).trim());
    if (!bullets.length && name !== 'After updating') errs.push(`"## ${name}" has no bullets`);
    if (name !== 'After updating') for (const b of bullets) if (!LEAD.test(b)) errs.push(`"## ${name}": no bold lead: ${b.slice(0, 60)}`);
    sections[SECTIONS[at]] = { body, bullets, name };
  }
  if (!['New', 'Improved', 'Fixed'].some((s) => sections[s]?.bullets.length)) errs.push('nothing under New, Improved or Fixed');
  if (errs.length) throw new Error(`release-notes/${v}.md:\n  ${errs.join('\n  ')}`);
  const out = [headline, ...SECTIONS.filter((s) => sections[s]).map((s) => `## ${sections[s].name}\n\n${sections[s].body}`),
    ...(all ? [all.block] : [])].join('\n\n');
  return { headline, sections, all, text: out };
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

// the docs rule (check only, for the version being released): each bullet of New and Improved, with its sub-points,
// links a docs page that exists, or says "(no docs: why)"
export function docs(v) {
  const { sections } = parse(v), errs = [];
  for (const name of ['New', 'Improved']) {
    const body = sections[name]?.body;
    if (!body) continue;
    for (const blk of body.split(/^(?=- )/m).filter((x) => x.startsWith('- '))) {
      const lead = blk.match(/^- \*\*(.+?)\*\*/)?.[1] ?? blk.slice(2, 50);
      const links = [...blk.matchAll(/\((https:\/\/mrdarrengriffin\.github\.io\/facet\/[^)\s]*)\)/g)].map((m) => m[1]);
      if (!links.length && !/\(no docs: [^)]+\)/.test(blk)) errs.push(`"## ${name}": "${lead}" links no docs page (add a link to ${DOCS}..., or "(no docs: why)")`);
      for (const u of links) if (!docsPage(u)) errs.push(`"## ${name}": "${lead}": ${u} is no page in site/src/pages`);
    }
  }
  if (errs.length) throw new Error(`release-notes/${v}.md: docs:\n  ${errs.join('\n  ')}`);
}

// the All changes rule (check only, for the version being released): the collapsed list is there and its count is right
export function allChanges(v) {
  const { all } = parse(v);
  if (!all) throw new Error(`release-notes/${v}.md: no All changes list at the end: node firmware/release-notes.mjs changes ${v} <last tag> >> release-notes/${v}.md`);
  if (all.n !== all.items) throw new Error(`release-notes/${v}.md: All changes says (${all.n}) but lists ${all.items}`);
}

// the All changes block for <version>, from <from> to <to>: the closed issues of its milestone(s) (a release: X.Y.Z
// and every X.Y.Z-*; a beta: its own) and the issues its commits close, the merged PRs, then every other commit
// (no issue or PR of the list in its message), bookkeeping left out (Todo, Catalogue, Notes, Log, Release notes)
const BOOKKEEPING = /^(Todo|Catalogue|Notes|Log|Release notes)\b/;
export function changes(v, from, to = 'HEAD') {
  const run = (cmd, args) => execFileSync(cmd, args, { cwd: ROOT, encoding: 'utf8', maxBuffer: 64 << 20 });
  const gh = (args) => JSON.parse(run('gh', args));
  const range = `${from}..${to}`;
  const commits = run('git', ['log', '--no-merges', '--reverse', '--format=%h%x00%s%x00%B%x01', range]).split('\x01')
    .map((c) => c.trim()).filter(Boolean).map((c) => { const [sha, subject, msg] = c.split('\0'); return { sha, subject, msg }; });
  const inRange = new Set(run('git', ['rev-list', range]).split('\n').filter(Boolean));
  const stableV = !v.includes('-');
  const milestones = gh(['api', '--paginate', 'repos/{owner}/{repo}/milestones?state=all&per_page=100']).map((m) => m.title)
    .filter((t) => t === v || (stableV && t.startsWith(`${v}-`)));
  const issues = new Map();
  for (const m of milestones) for (const i of gh(['issue', 'list', '-m', m, '-s', 'closed', '-L', '500', '--json', 'number,title'])) issues.set(i.number, i.title);
  const prs = new Map(gh(['pr', 'list', '-s', 'merged', '-L', '500', '--json', 'number,title,mergeCommit'])
    .filter((p) => p.mergeCommit && inRange.has(p.mergeCommit.oid)).map((p) => [p.number, p.title]));
  for (const c of commits) for (const m of c.msg.matchAll(/\b(?:fix(?:es|ed)?|close[sd]?|resolve[sd]?) #(\d+)/gi)) {
    const n = +m[1];
    if (issues.has(n) || prs.has(n)) continue;
    const i = gh(['api', `repos/{owner}/{repo}/issues/${n}`]);
    if (i.state === 'closed' && !i.pull_request) issues.set(n, i.title);
  }
  const listed = new Set([...issues.keys(), ...prs.keys()]);
  const covered = (c) => [...c.msg.matchAll(/#(\d+)\b/g)].some((m) => listed.has(+m[1]));
  const other = commits.filter((c) => !covered(c) && !BOOKKEEPING.test(c.subject))
    .sort((a, b) => a.subject.split(':')[0].localeCompare(b.subject.split(':')[0]));  // by area, in order within one
  const byNum = (m) => [...m].sort((a, b) => a[0] - b[0]).map(([n, t]) => `- #${n} ${t}`);
  const parts = [];
  if (issues.size) parts.push(`**Issues (${issues.size})**\n\n${byNum(issues).join('\n')}`);
  if (prs.size) parts.push(`**Pull requests (${prs.size})**\n\n${byNum(prs).join('\n')}`);
  if (other.length) parts.push(`**Other commits (${other.length})**\n\n${other.map((c) => `- ${c.sha} ${c.subject}`).join('\n')}`);
  const n = issues.size + prs.size + other.length;
  return `<details><summary>All changes (${n})</summary>\n\n${parts.join('\n\n')}\n</details>\n`;
}

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
  if (!['check', 'body', 'summary', 'entries', 'changes'].includes(cmd) || !v || (cmd === 'changes' && !prev)) {
    console.error('usage: node firmware/release-notes.mjs check|body|summary|entries <version> [<previous tag>]\n' +
      '       node firmware/release-notes.mjs changes <version> <from tag> [<to ref>]');
    process.exit(2);
  }
  try {
    if (cmd === 'check') {
      const s = summary(v), e = entries(v);
      docs(v);
      allChanges(v);
      console.log(`release-notes/${v}.md: ok, summary ${s.length} chars, notes ${e.map((x) => `${x.version} ${x.summary.length}`).join(', ')}`);
    } else if (cmd === 'changes') process.stdout.write(changes(v, prev, process.argv[5]));
    else if (cmd === 'entries') process.stdout.write(JSON.stringify(entries(v), null, 2) + '\n');
    else process.stdout.write(cmd === 'body' ? body(v, prev) : summary(v) + '\n');
  } catch (e) { console.error(`::error::${e.message}`); process.exit(1); }
}
