// The built site's text and links (npm run build runs it after astro build; renderer/watch.mjs too): fails on
//   words run together: text that touches an inline tag with no space where a reader expects one (a letter or digit
//     both sides: "press<b>Install</b>", "named<code>Facet-</code>"), two <code> runs touching, a lowercase letter
//     straight into an uppercase one, punctuation straight into a letter (prose only: never inside code / pre)
//   a typed count: a number before screens / components / foundations / tokens / routes / settings / bindings
//     that is not a generated one (<Count>, lib/counts.ts: read from the schema and catalogue, marked data-count)
//   a broken internal link: a page or file that is not in the build, or a #fragment with no such id on its page
//   agent docs (scripts/llms.mjs): a page with no .md twin, not linked from llms.txt or not in llms-full.txt, or a
//     link in llms.txt to a file that is not in the build
// Usage: node scripts/check-dist.mjs [dist folder] (default FACET_DOCS_OUT or ./dist); base = FACET_DOCS_BASE or /facet-docs/
import { existsSync, readdirSync, readFileSync, statSync } from 'node:fs';
import { join, relative } from 'node:path';
import { docPages, mdPath } from './md.mjs';

const DIST = process.argv[2] ?? process.env.FACET_DOCS_OUT ?? './dist';
const BASE = process.env.FACET_DOCS_BASE ?? '/facet-docs/';
// words that are spelled with a capital inside (names), and dotted words people write as prose
const NAMES = /^(GitHub|ESPHome|PlatformIO|JavaScript|TypeScript|WebSocket|WebSockets|LilyGO|YouTube|iPhone|iPad|macOS|iOS|PSRAM|LVGL|WebAssembly|OpenStreetMap|ArduinoJson|HomeKit|AirPlay|TiviMate|MiB|KiB|kWh|MWh|GBP|mAh|IoT|SoC|SoCs|McCartney|OpenType|JetBrains|LiDAR|iCal|FlightRadar24|FlightRadar|OpenSky|easyJet)$/;
const DOTTED = /^(e\.g|i\.e|etc|vs|v\d|\d)/;
const INLINE = new Set(['a', 'b', 'strong', 'em', 'i', 'code', 'kbd', 'samp', 'span', 'abbr', 'small', 'mark', 'sup', 'sub', 'u', 's', 'q', 'time', 'var', 'cite', 'dfn']);
const SKIP = new Set(['script', 'style', 'svg', 'template', 'noscript', 'textarea', 'canvas']);
const CODE = new Set(['code', 'pre', 'kbd', 'samp']);
const VOID = new Set(['br', 'hr', 'img', 'input', 'meta', 'link', 'source', 'wbr', 'area', 'base', 'col', 'embed', 'track', 'path', 'circle', 'rect', 'line', 'use']);
const ent = (s) => s.replace(/&nbsp;|&#160;/g, ' ').replace(/&amp;/g, '&').replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&quot;/g, '"')
  .replace(/&#39;|&#x27;|&apos;/g, "'").replace(/&#(\d+);/g, (_, n) => String.fromCodePoint(+n)).replace(/&#x([0-9a-f]+);/gi, (_, n) => String.fromCodePoint(parseInt(n, 16)));
const COUNTED = /(?<![\w.])(\d[\d,.]*)\s+(?:entity\s+)?(screens|components|foundations|tokens|routes|settings|bindings)\b/gi;
const word = (c) => /[\p{L}\p{N}]/u.test(c ?? '');

function pages(dir) {
  const out = [];
  for (const f of readdirSync(dir)) {
    const p = join(dir, f);
    if (statSync(p).isDirectory()) out.push(...pages(p));
    else if (f.endsWith('.html')) out.push(p);
  }
  return out;
}

/** the faults in one page's text: [what, around] */
function textFaults(html) {
  const faults = [];
  const body = html.slice(html.search(/<body[\s>]/i) + 1);
  const re = /<!--[\s\S]*?-->|<(\/?)([a-zA-Z][\w-]*)([^>]*)>|([^<]+)/g;
  let skip = 0, code = 0, prev = '', edge = null, tail = '', m, cnt = false, blk = '';
  // blk = this block's prose, a generated count's digits as C (so only typed numbers are left as digits)
  const counts = () => { for (const x of blk.matchAll(COUNTED)) faults.push(['typed count', x[0]]); blk = ''; };
  // prev = the last visible character; edge = an inline tag since it (tag name, closing), else null
  const say = (what, next) => faults.push([what, (tail + ' ⟨' + edge?.[0] + '⟩ ' + next).replace(/\s+/g, ' ').slice(-70)]);
  while ((m = re.exec(body))) {
    if (m[0].startsWith('<!--')) continue;
    if (m[2]) {
      const tag = m[2].toLowerCase(), close = m[1] === '/';
      if (SKIP.has(tag)) { if (!m[3].endsWith('/')) skip += close ? -1 : 1; continue; }
      if (skip) continue;
      if (CODE.has(tag)) code += close ? -1 : 1;
      if (INLINE.has(tag)) {
        // </code><code>: two code runs touching
        if (!close && tag === 'code' && edge?.[0] === 'code' && edge[1]) faults.push(['code runs touching', tail.slice(-50)]);
        edge = [tag, close];
        if (tag === 'span') cnt = !close && / data-count=/.test(m[3]);
      } else if (!VOID.has(tag) || tag === 'br' || tag === 'img') { prev = ' '; edge = null; tail = ''; counts(); }  // a block edge: a break
      continue;
    }
    if (skip || !m[4]) continue;
    const t = ent(m[4]);
    if (edge && word(prev) && word(t[0])) say('no space at a tag', t.slice(0, 30));
    if (!code) {
      blk += cnt ? t.replace(/\d/g, 'C') : t;
      for (const w of t.split(/\s+/)) {
        const bare = w.replace(/^[^\p{L}\p{N}]+|[^\p{L}\p{N}]+$/gu, '');
        if (/\p{Ll}\p{Lu}/u.test(bare) && !bare.split(/[^\p{L}]+/u).every((x) => !/\p{Ll}\p{Lu}/u.test(x) || NAMES.test(x))) faults.push(['lowercase into uppercase', w]);
        const p = bare.match(/[.,;:!?)](?=\p{L})/u);
        if (p && !DOTTED.test(bare) && !/^[\w.-]+\.[a-z]{2,4}$/i.test(bare) && !/[/@]|::/.test(w) && !/^[\w-]+(\.[\w-]+)+$/.test(bare)) faults.push(['punctuation into a letter', w]);
      }
    }
    prev = t.trimEnd() === t ? t.at(-1) : ' ', edge = null;
    tail = (tail + t).slice(-60);
  }
  counts();
  return faults;
}

const files = pages(DIST);
const ids = new Map();  // page path -> its ids
const page = (f) => '/' + relative(DIST, f).replace(/index\.html$/, '').replace(/\\/g, '/');
for (const f of files) ids.set(page(f), new Set([...readFileSync(f, 'utf8').matchAll(/\sid="([^"]+)"/g)].map((x) => x[1])));

let bad = 0;
for (const f of files) {
  const html = readFileSync(f, 'utf8'), here = page(f);
  for (const [what, at] of textFaults(html)) { bad++; console.log(`text  ${here}  ${what}: ${at}`); }
  for (const [, href] of html.matchAll(/<a\s[^>]*?href="([^"]+)"/g)) {
    const h = ent(href);
    if (/^(https?:|mailto:|tel:|data:|javascript:)/.test(h) || h.startsWith('//')) continue;
    let [path, frag] = h.split('#');
    if (!path) path = here;
    else if (path.startsWith(BASE)) path = '/' + path.slice(BASE.length);
    else if (path.startsWith('/')) { bad++; console.log(`link  ${here}  outside the site's base ${BASE}: ${h}`); continue; }
    else path = new URL(path, 'http://x' + here).pathname;
    path = decodeURIComponent(path.split('?')[0]);
    const file = join(DIST, path), known = ids.get(path) ?? ids.get(path + '/');
    if (!known && !(existsSync(file) && statSync(file).isFile())) { bad++; console.log(`link  ${here}  no such page: ${h}`); continue; }
    if (frag && known && !known.has(decodeURIComponent(frag))) { bad++; console.log(`link  ${here}  no #${frag} on ${path}`); }
  }
}
// agent docs: every doc page has its .md twin, a line in llms.txt and its part of llms-full.txt
const read = (f) => (existsSync(join(DIST, f)) ? readFileSync(join(DIST, f), 'utf8') : null);
const llms = read('llms.txt'), full = read('llms-full.txt'), at = (p) => BASE.slice(0, -1) + p;
if (llms === null || full === null) { bad++; console.log('llms  no llms.txt / llms-full.txt (scripts/llms.mjs)'); }
else {
  for (const p of docPages(DIST)) {
    if (!existsSync(join(DIST, mdPath(p)))) { bad++; console.log(`llms  ${p}  no Markdown twin ${mdPath(p)}`); }
    if (!llms.includes(`](${at(mdPath(p))})`)) { bad++; console.log(`llms  ${p}  not in llms.txt`); }
    if (!full.includes(`<!-- page: ${at(p)} `)) { bad++; console.log(`llms  ${p}  not in llms-full.txt`); }
  }
  for (const [, h] of llms.matchAll(/\]\(([^)]+)\)/g)) {
    if (!h.startsWith(BASE) || !existsSync(join(DIST, decodeURIComponent(h.slice(BASE.length).split('#')[0])))) { bad++; console.log(`llms  llms.txt links to a file not in the build: ${h}`); }
  }
}
console.log(`check-dist: ${files.length} pages, ${bad} fault${bad === 1 ? '' : 's'}`);
process.exit(bad ? 1 : 0);
