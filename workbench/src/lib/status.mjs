// Workbench status per item, from git alone at HEAD (never the working tree): done = on the contract
// (renderer/catalogue/contract.json), new = not on it yet.
// Item -> files: the .cpp that declares FW_COMPONENT(<id>, name, "<section>", ...) (all ids in one file share it),
// foundations also tokens/<f>.json; spec screens specs/<id>.json (behaviour tests never count); Icons owns
// icons/; HTML-only foundations with no .cpp (Sound) = tokens/<id>.json alone. Used by /api/wasm/status.
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';

const run = promisify(execFile);
import { REPO } from './repo';
export const WASM_DIR = REPO;
const BRANCH = 'HEAD';
const R = 'renderer/';
// files an item clearly owns beyond the name mapping
const EXTRA = { icons: [`${R}icons/`], settings: [`${R}tokens/settings.json`] };
const git = (dir, ...a) => run('git', ['-C', dir, ...a], { maxBuffer: 64 << 20 }).then((r) => r.stdout);

/** every item at a ref: { "<section>/<id>": { name?, files: [repo paths] } } */
export async function items(ref = BRANCH) {
  const tree = new Set((await git(WASM_DIR, 'ls-tree', '-r', '--name-only', ref, '--', `${R}components`, `${R}foundations`, `${R}screens`, `${R}specs`, `${R}tokens`, `${R}icons`)).split('\n').filter(Boolean));
  const grep = await git(WASM_DIR, 'grep', '-E', 'FW_COMPONENT\\(\\s*[A-Za-z0-9_]+\\s*,|fw::add\\(\\{\\s*"', ref, '--', `${R}components/*.cpp`, `${R}foundations/*.cpp`, `${R}screens/*.cpp`).catch(() => '');
  const out = {};
  const test = () => [];  // behaviour tests never mark an item Edited
  for (const line of grep.split('\n').filter(Boolean)) {
    const m = line.match(/^[^:]+:([^:]+):.*FW_COMPONENT\(\s*([A-Za-z0-9_]+)\s*,\s*"([^"]*)"\s*,\s*"([a-z]+)"/) ?? line.match(/^[^:]+:([^:]+):.*fw::add\(\{\s*"([A-Za-z0-9_]+)"\s*,\s*"([^"]*)"\s*,\s*"([a-z]+)"/);
    if (!m || m[1].includes('/core/')) continue;
    const [, path, id, name, section] = m, files = [path];
    const f = path.slice(path.lastIndexOf('/') + 1).replace(/\.cpp$/, '');
    if (path.includes('/foundations/') && tree.has(`${R}tokens/${f}.json`)) files.push(`${R}tokens/${f}.json`);
    if (EXTRA[id]) files.push(...EXTRA[id]);
    out[`${section}/${id}`] = { name, files: [...files, ...test(id)] };
  }
  for (const fs of Object.values(EXTRA)) for (const f of fs) if (f.endsWith('.json')) tree.delete(f);  // owned: no own item
  for (const p of tree) {
    const spec = p.match(/^renderer\/specs\/([a-z0-9_]+)\.json$/);
    if (spec && spec[1] !== 'deck' && !out[`screens/${spec[1]}`]) out[`screens/${spec[1]}`] = { files: [p, ...test(spec[1])] };
    const tok = p.match(/^renderer\/tokens\/([a-z0-9_]+)\.json$/);
    if (tok && !tree.has(`${R}foundations/${tok[1]}.cpp`) && !Object.values(out).some((x) => x.files.includes(p))) out[`foundations/${tok[1]}`] = { files: [p, ...(EXTRA[tok[1]] ?? [])] };
  }
  return out;
}

// contract.json = { "<section>/<id>": { commit, at } }: items rebuilt on the contract (renderer/CONTRACT.md); landing a piece adds its key
export async function readContract(ref = BRANCH) {
  try { return JSON.parse(await git(WASM_DIR, 'show', `${ref}:${R}catalogue/contract.json`)); } catch { return {}; }
}


// cache: recomputed only when the wasm branch moves
let cache = { head: '', data: null };
export async function status() {
  const head = (await git(WASM_DIR, 'rev-parse', BRANCH)).trim();
  if (cache.head === head && cache.data) return cache.data;
  const [its, con, log] = await Promise.all([items(head), readContract(head), git(WASM_DIR, 'log', '--format=@%H%x09%ct%x09%s', '--name-only', head, '--', R)]);
  // newest first: [{sha, t, subject, files}]
  const commits = [];
  for (const line of log.split('\n')) {
    if (line.startsWith('@')) { const [sha, t, ...s] = line.slice(1).split('\t'); commits.push({ sha, t: +t, subject: s.join('\t'), files: [] }); }
    else if (line) commits.at(-1)?.files.push(line);
  }
  const touches = (c, files) => c.files.some((f) => files.some((x) => (x.endsWith('/') ? f.startsWith(x) : f === x)));
  const out = {};
  for (const [key, it] of Object.entries(its)) {
    const mine = commits.filter((c) => touches(c, it.files)), last = mine[0];
    out[key] = { status: con[key] ? 'done' : 'new', name: it.name, file: it.files.map((f) => f.slice(R.length)).join(' + '), contract: !!con[key], last: last ? { sha: last.sha.slice(0, 7), subject: last.subject, at: new Date(last.t * 1000).toISOString() } : null };
  }
  cache = { head, data: out };
  return out;
}
