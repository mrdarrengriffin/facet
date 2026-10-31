// Code blocks (src/components/CodeBlock.astro + markdown fences): a Shiki theme whose colours are CSS variables, set
// from the colour tokens in src/styles/global.css (--code-*), so one build-time highlight follows light + dark and every
// token colour is checked for contrast against the block (tokens contrast pairs: text, text-dim, accent-text, ok, warn,
// blue on surface). formatJson: 2-space JSON that keeps small objects/arrays on one line when they fit.
import type { ThemeRegistration } from 'shiki';

const v = (n: string) => `var(--code-${n})`;
export const facetCodeTheme: ThemeRegistration = {
  name: 'facet',
  type: 'dark',
  colors: { 'editor.foreground': v('fg'), 'editor.background': v('bg') },
  fg: v('fg'),
  bg: v('bg'),
  tokenColors: [
    { scope: ['comment', 'punctuation.definition.comment'], settings: { foreground: v('comment'), fontStyle: 'italic' } },
    { scope: ['punctuation', 'meta.brace', 'keyword.operator', 'punctuation.separator', 'punctuation.definition'], settings: { foreground: v('punct') } },
    { scope: ['support.type.property-name', 'entity.name.tag', 'entity.name.tag.yaml', 'meta.object-literal.key', 'variable.other.normal.shell', 'variable.other.assignment.shell', 'variable.other.readwrite'], settings: { foreground: v('key') } },
    { scope: ['string', 'string.quoted', 'string.unquoted', 'punctuation.definition.string'], settings: { foreground: v('string') } },
    { scope: ['constant', 'constant.numeric', 'constant.language', 'constant.other', 'support.constant'], settings: { foreground: v('constant') } },
    { scope: ['keyword', 'storage', 'storage.type', 'storage.modifier', 'support.function.builtin', 'entity.name.function', 'support.function'], settings: { foreground: v('keyword') } },
  ],
};

export const LANG_LABEL: Record<string, string> = { json: 'JSON', yaml: 'YAML', bash: 'Shell', cpp: 'C++', text: 'Text' };

const inline = (x: unknown): string => {
  if (Array.isArray(x)) return `[${x.map(inline).join(', ')}]`;
  if (x && typeof x === 'object') return `{${Object.entries(x).map(([k, y]) => `${JSON.stringify(k)}: ${inline(y)}`).join(', ')}}`;
  return JSON.stringify(x);
};

/** Pretty JSON: 2-space indent; an object or array stays on one line when the whole line fits in `width`. */
export function formatJson(x: unknown, width = 76): string {
  const fmt = (y: unknown, ind: string, used: number): string => {
    const s = inline(y);
    if (!y || typeof y !== 'object' || ind.length + used + s.length + 1 <= width) return s;
    const inner = ind + '  ';
    const items = Array.isArray(y)
      ? y.map((z) => inner + fmt(z, inner, 0))
      : Object.entries(y).map(([k, z]) => { const key = `${JSON.stringify(k)}: `; return inner + key + fmt(z, inner, key.length); });
    if (!items.length) return s;
    const [o, c] = Array.isArray(y) ? ['[', ']'] : ['{', '}'];
    return `${o}\n${items.join(',\n')}\n${ind}${c}`;
  };
  return fmt(x, '', 0);
}
