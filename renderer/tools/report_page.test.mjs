// The problem report page (firmware/report.html, GET /fw/report on the panel, #125): small enough to serve from flash,
// the steps first, and a GitHub issue address that stays short (the report never goes in it).
//   node --test renderer/tools/report_page.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { gzipSync } from 'node:zlib';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const html = readFileSync(join(ROOT, 'firmware', 'report.html'), 'utf8');
const script = html.slice(html.indexOf('<script>') + 8, html.indexOf('</script>'));
// the page's own helpers, without the DOM (everything before the first line that touches the page)
const fns = new Function(`${script.slice(0, script.indexOf('function state('))}; return { issueUrl, facts };`)();
const bug = readFileSync(join(ROOT, '.github', 'ISSUE_TEMPLATE', 'bug.yml'), 'utf8');

test('small: under 4 KB gzipped (served from flash in one piece)', () => {
  assert.ok(gzipSync(html, { level: 9 }).length < 4096, `${gzipSync(html, { level: 9 }).length} B`);
});
test('Copy report, Download report (.txt), Open a GitHub issue', () => {
  for (const b of ['Copy report', 'Download report (.txt)', 'Open a GitHub issue']) assert.ok(html.includes(`>${b}<`), b);
  assert.ok(html.includes("fetch('/fw/report.txt'"), 'reads the plain-text report');
});
test('the steps come first, the GitHub link in step 2, the buttons next to them', () => {
  const steps = ['Copy this report (or download it).', 'Open a new issue on GitHub:', 'Paste the report into the Report box (or attach the file), and say what happened.'];
  const at = steps.map((x) => html.indexOf(x));
  assert.ok(at.every((i) => i > 0), 'all three steps');
  assert.ok(at[0] < html.indexOf('id="copy"') && html.indexOf('id="copy"') < html.indexOf('id="state"'), 'steps, then the buttons, then the rest');
  assert.ok(html.includes('id="ghlink"') && script.includes("$('ghlink').href = issueUrl"), 'step 2 links the filled-in form');
});
test('says it is safe to paste, at the top, as the text does (#176)', () => {
  const safe = 'Safe to paste: no passwords, keys, tokens, addresses or names of your devices and entities.';
  assert.ok(html.includes(safe) && html.indexOf(safe) < html.indexOf('<ol>'), 'the line, before the steps');
  assert.ok(readFileSync(join(ROOT, 'firmware', 'fw_report.h'), 'utf8').includes(`"# ${safe}\\n`), 'the same line in the text');
});
test('the version and board come from the report\'s first lines', () => {
  const t = 'Facet problem report\nfirmware 0.5.0-beta.7, board guition-4848s040, built 2026-11-06\nuptime 3 min';
  assert.deepEqual(fns.facts(t), { version: '0.5.0-beta.7', board: 'guition-4848s040' });
  assert.deepEqual(fns.facts('not a report'), { version: '', board: '' });
});
test('the GitHub issue address: the Bug report form, title + version + board, short, no report in it', () => {
  const u = new URL(fns.issueUrl('0.5.0-beta6-dev202611061147+9d9ef80', 'guition-4848s040'));
  assert.equal(u.origin + u.pathname, 'https://github.com/mrdarrengriffin/facet/issues/new');
  assert.equal(u.searchParams.get('template'), 'bug.yml');
  assert.match(u.searchParams.get('title'), /guition-4848s040.*0\.5\.0-beta6-dev/);
  assert.equal(u.searchParams.get('version'), '0.5.0-beta6-dev202611061147+9d9ef80 on guition-4848s040');
  assert.match(u.searchParams.get('report'), /paste|attach/i);
  assert.ok(u.href.length < 600, `${u.href.length} characters`);
  for (const id of ['version', 'report']) assert.ok(bug.includes(`id: ${id}\n`), `bug.yml has the field ${id}`);
  assert.equal(new URL(fns.issueUrl('', '')).searchParams.get('title'), null, 'nothing known: no made-up title');
});
