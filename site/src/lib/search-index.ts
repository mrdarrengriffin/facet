import { render, type CollectionEntry } from 'astro:content';
import { docHref } from './nav';

/** One searchable chunk: a page, or a section of a page under a heading. */
export interface SearchRecord {
  /** Page title. */
  title: string;
  /** Section heading, empty for the page intro. */
  heading: string;
  /** Plain text of the chunk. */
  text: string;
  url: string;
}

/** Strip Markdown syntax down to readable text. Good enough for search, not a parser. */
function toPlainText(markdown: string): string {
  return markdown
    .replace(/^```.*$/gm, '')
    .replace(/<[^>]+>/g, ' ')
    .replace(/!\[([^\]]*)\]\([^)]*\)/g, '$1')
    .replace(/\[([^\]]+)\]\([^)]*\)/g, '$1')
    .replace(/^\s*>\s?/gm, '')
    .replace(/^\s*(?:[-*+]|\d+\.)\s+(?:\[[ x]\]\s+)?/gm, '')
    .replace(/^\s*\|?\s*:?-{3,}.*$/gm, '')
    .replace(/\|/g, ' ')
    .replace(/[`*_~]/g, '')
    .replace(/\s+/g, ' ')
    .trim();
}

export async function buildRecords(entry: CollectionEntry<'docs'>): Promise<SearchRecord[]> {
  const { headings } = await render(entry);
  const pageUrl = docHref(entry);
  const records: SearchRecord[] = [];

  // Walk the source line by line. Markdown headings appear in the same order as the rendered
  // ones, which gives each section its slug for the link.
  let headingIndex = 0;
  let current = { heading: '', slug: '' };
  let lines: string[] = [entry.data.description];
  let inFence = false;

  const flush = () => {
    const text = toPlainText(lines.join('\n'));
    if (text || current.heading) {
      records.push({
        title: entry.data.title,
        heading: current.heading,
        text,
        url: current.slug ? `${pageUrl}#${current.slug}` : pageUrl,
      });
    }
    lines = [];
  };

  for (const line of (entry.body ?? '').split('\n')) {
    if (line.startsWith('```')) inFence = !inFence;
    const match = !inFence && /^(#{1,6})\s+(.*)$/.exec(line);
    if (match) {
      const rendered = headings[headingIndex++];
      if (rendered && rendered.depth <= 3) {
        flush();
        current = { heading: rendered.text.replace(/\d+\/\d+$/, ''), slug: rendered.slug };
        continue;
      }
    }
    lines.push(line);
  }
  flush();

  return records;
}
