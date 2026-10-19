/** Escape text and turn `code` spans into <code>. Enough for the short notes in src/lib/screens.ts. */
export function inlineMarkdown(text: string): string {
  const esc = text.replace(
    /[&<>"]/g,
    (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]!,
  );
  return esc.replace(/`([^`]+)`/g, '<code>$1</code>');
}
