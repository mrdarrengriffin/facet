// The built site as Markdown, for agents (scripts/llms.mjs writes it, scripts/check-dist.mjs checks it). One source:
// each page's Markdown is made from its own built HTML (its <article>, else its <main>), so the two never differ.
// Left out: the site's chrome and anything that only works in a browser (nav, aside, footer, buttons, forms, scripts,
// the live preview stages and their switches, [hidden], aria-hidden, data-md-skip). Links to other pages point at
// their .md twin; tables become pipe tables, code blocks fenced blocks with their language.
//   docPages(dist)        the pages that get a twin: every .html except redirects and noindex test pages
//   pageInfo(html)        { title, description, robots, refresh }
//   toMarkdown(html, base) the page's Markdown
//   mdPath(page)          '/screens/clock/' -> '/screens/clock.md', '/' -> '/index.md'
import { readdirSync, readFileSync, statSync } from 'node:fs';
import { join, relative } from 'node:path';

const VOID = new Set(['area', 'base', 'br', 'col', 'embed', 'hr', 'img', 'input', 'link', 'meta', 'source', 'track', 'wbr', 'path', 'circle', 'rect', 'line', 'use', 'polyline', 'polygon', 'ellipse', 'stop']);
const SKIP = new Set(['script', 'style', 'svg', 'template', 'noscript', 'button', 'nav', 'aside', 'footer', 'form', 'input', 'select', 'textarea', 'canvas', 'dialog', 'iframe', 'video', 'audio', 'head']);
const SKIP_CLASS = /(^|\s)(preview-box|skip-link|live|pager|docs__footer|search|sr-only|visually-hidden)(\s|$)/;
const ent = (s) => s.replace(/&nbsp;|&#160;/g, ' ').replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&quot;/g, '"')
  .replace(/&#39;|&#x27;|&apos;/g, "'").replace(/&#(\d+);/g, (_, n) => String.fromCodePoint(+n)).replace(/&#x([0-9a-f]+);/gi, (_, n) => String.fromCodePoint(parseInt(n, 16))).replace(/&amp;/g, '&');
const attrs = (s) => Object.fromEntries([...s.matchAll(/([\w:-]+)(?:\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))?/g)].map((m) => [m[1].toLowerCase(), ent(m[2] ?? m[3] ?? m[4] ?? '')]));

/** a tolerant tree of the HTML: { tag, a, kids } and text strings */
function parse(html) {
  const root = { tag: '#root', a: {}, kids: [] }, stack = [root];
  const re = /<!--[\s\S]*?-->|<!doctype[^>]*>|<(\/?)([a-zA-Z][\w-]*)((?:[^>"']|"[^"]*"|'[^']*')*)>|([^<]+)/gi;
  let m;
  while ((m = re.exec(html))) {
    if (m[4] !== undefined) { stack.at(-1).kids.push(ent(m[4])); continue; }
    if (!m[2]) continue;
    const tag = m[2].toLowerCase();
    if (m[1]) {  // a close: up to the matching open (a stray one is ignored)
      const i = stack.findLastIndex((n) => n.tag === tag);
      if (i > 0) stack.length = i;
      continue;
    }
    const n = { tag, a: attrs(m[3]), kids: [] };
    stack.at(-1).kids.push(n);
    if (VOID.has(tag) || m[3].trimEnd().endsWith('/')) continue;
    if (tag === 'script' || tag === 'style' || tag === 'textarea') {  // raw text up to its close
      const end = html.toLowerCase().indexOf(`</${tag}`, re.lastIndex);
      n.kids.push(html.slice(re.lastIndex, end < 0 ? html.length : end));
      re.lastIndex = end < 0 ? html.length : html.indexOf('>', end) + 1;
      continue;
    }
    stack.push(n);
  }
  return root;
}
const find = (n, f) => { if (typeof n === 'string') return null; if (f(n)) return n; for (const k of n.kids) { const r = find(k, f); if (r) return r; } return null; };
const text = (n) => (typeof n === 'string' ? n : n.kids.map(text).join(''));
const skip = (n) => SKIP.has(n.tag) || 'hidden' in n.a || n.a['aria-hidden'] === 'true' || 'data-md-skip' in n.a || 'data-live' in n.a || SKIP_CLASS.test(n.a.class ?? '');

export function pageInfo(html) {
  const meta = (k) => html.match(new RegExp(`<meta\\s+name="${k}"\\s+content="([^"]*)"`))?.[1];
  return {
    title: ent(html.match(/<title>([^<]*)<\/title>/)?.[1] ?? '').replace(/ · Facet$/, ''),
    description: ent(meta('description') ?? ''),
    robots: meta('robots') ?? '',
    refresh: /http-equiv="refresh"/.test(html),
  };
}

export const mdPath = (page) => (page === '/' ? '/index.md' : page.replace(/\/$/, '') + '.md');

/** every built page that is a page of the docs (not a redirect, not a noindex test page), as '/path/' */
export function docPages(dist) {
  const out = [];
  const walk = (dir) => {
    for (const f of readdirSync(dir).sort()) {
      const p = join(dir, f);
      if (statSync(p).isDirectory()) walk(p);
      else if (f === 'index.html') {
        const i = pageInfo(readFileSync(p, 'utf8'));
        if (!i.refresh && !/noindex/.test(i.robots)) out.push('/' + relative(dist, p).replace(/index\.html$/, '').replace(/\\/g, '/'));
      }
    }
  };
  walk(dist);
  return out;
}

/** the page's Markdown: base = the site's base path ('/facet-docs/'), pages = the doc pages (links to them get .md) */
export function toMarkdown(html, base, pages) {
  const doc = parse(html);
  const body = find(doc, (n) => n.tag === 'article') ?? find(doc, (n) => n.tag === 'main') ?? find(doc, (n) => n.tag === 'body') ?? doc;
  const known = new Set(pages);
  const link = (href) => {
    if (!href.startsWith(base)) return href;
    const [path, frag] = href.slice(base.length - 1).split('#');
    return known.has(path) ? base.slice(0, -1) + mdPath(path) + (frag ? `#${frag}` : '') : href;
  };
  const inl = (n) => {  // inline Markdown of a node
    if (typeof n === 'string') return n.replace(/\s+/g, ' ');
    if (skip(n)) return '';
    const kids = () => n.kids.map(inl).join('');
    switch (n.tag) {
      case 'br': return '\n';
      case 'code': case 'kbd': case 'samp': { const t = text(n).replace(/\s+/g, ' '); return t.trim() ? (t.includes('`') ? `\`\` ${t} \`\`` : `\`${t}\``) : ''; }
      case 'strong': case 'b': { const t = kids(); return t.trim() ? `**${t.trim()}**` : t; }
      case 'em': case 'i': { const t = kids(); return t.trim() ? `_${t.trim()}_` : t; }
      case 'a': { const t = kids().trim(), h = n.a.href; return h && t ? `[${t}](${link(h)})` : t; }
      case 'img': return n.a.alt ? `![${n.a.alt}](${n.a.src})` : '';
      // a block inside a line (a list item's figure and caption, a cell's paragraphs): kept apart by a space
      case 'p': case 'div': case 'figure': case 'figcaption': case 'li': case 'dt': case 'dd': case 'section': return ` ${kids()} `;
      default: return kids();
    }
  };
  const clean = (s) => s.replace(/[ \t]+/g, ' ').replace(/ *\n */g, '\n').trim();
  const cell = (n) => clean(inl(n)).replace(/\n/g, ' ').replace(/\|/g, '\\|');
  const blocks = [];
  const block = (n, indent = '') => {
    if (typeof n === 'string') { if (n.trim()) blocks.push(indent + clean(n)); return; }
    if (skip(n)) return;
    const h = n.tag.match(/^h([1-6])$/);
    if (h) { const t = clean(inl(n)); if (t) blocks.push(`${'#'.repeat(+h[1])} ${t}`); return; }
    switch (n.tag) {
      case 'p': case 'figcaption': case 'summary': case 'dt': case 'dd': case 'caption': { const t = clean(inl(n)); if (t) blocks.push(indent + (n.tag === 'summary' || n.tag === 'dt' ? `**${t}**` : t)); return; }
      case 'pre': { const lang = n.a['data-language'] ?? ''; blocks.push(`${indent}\`\`\`${lang === 'plaintext' ? '' : lang}\n${text(n).replace(/\n+$/, '')}\n${indent}\`\`\``); return; }
      case 'hr': blocks.push('---'); return;
      case 'blockquote': { const t = toMarkdownKids(n); if (t) blocks.push(t.split('\n').map((l) => `> ${l}`).join('\n')); return; }
      case 'ul': case 'ol': {
        let i = 0;
        const lines = [];
        for (const li of n.kids.filter((k) => typeof k !== 'string' && k.tag === 'li' && !skip(k))) {
          const mark = n.tag === 'ol' ? `${++i}.` : '-';
          const inner = li.kids.filter((k) => typeof k === 'string' || !['ul', 'ol'].includes(k.tag));
          const subs = li.kids.filter((k) => typeof k !== 'string' && ['ul', 'ol'].includes(k.tag));
          // a block inside an item (a paragraph, a heading): its text on the item's line
          const t = clean(inner.map((k) => (typeof k === 'string' ? inl(k) : /^(p|div|h\d|figure|figcaption|span|a|strong|code|em|b|i)$/.test(k.tag) ? ` ${inl(k)} ` : inl(k))).join(''));
          lines.push(`${indent}${mark} ${t}`);
          for (const s of subs) { const save = blocks.length; block(s, indent + '  '); lines.push(...blocks.splice(save)); }
        }
        if (lines.length) blocks.push(lines.join('\n'));
        return;
      }
      case 'table': {
        const rows = [];
        const walk = (k) => { if (typeof k === 'string' || skip(k)) return; if (k.tag === 'tr') rows.push(k.kids.filter((c) => typeof c !== 'string' && (c.tag === 'td' || c.tag === 'th')).map(cell)); else k.kids.forEach(walk); };
        walk(n);
        if (!rows.length) return;
        const w = Math.max(...rows.map((r) => r.length));
        const row = (r) => `| ${[...r, ...Array(w - r.length).fill('')].join(' | ')} |`;
        blocks.push([row(rows[0]), `| ${Array(w).fill('---').join(' | ')} |`, ...rows.slice(1).map(row)].join('\n'));
        return;
      }
      default: {
        // a block container: its inline runs as paragraphs, its blocks as they are
        let run = [];
        const flush = () => { const t = clean(run.map(inl).join('')); if (t) blocks.push(indent + t); run = []; };
        for (const k of n.kids) {
          if (typeof k === 'string' || /^(a|span|strong|b|em|i|code|kbd|samp|img|br|small|abbr|time|sup|sub|mark|label)$/.test(k.tag)) run.push(k);
          else { flush(); block(k, indent); }
        }
        flush();
      }
    }
  };
  const toMarkdownKids = (n) => { const save = blocks.length; n.kids.forEach((k) => block(k)); return blocks.splice(save).join('\n\n'); };
  body.kids.forEach((k) => block(k));
  return blocks.join('\n\n').replace(/\n{3,}/g, '\n\n').trim() + '\n';
}
