// Lets docs link to site pages with root-relative paths ("/design-system/") and still work
// when the site is served from a sub-path.

/** @param {{ base: string }} options */
export function rehypeBaseLinks({ base }) {
  const prefix = base.replace(/\/$/, '');

  /** @param {any} node */
  function walk(node) {
    if (node.type === 'element' && node.tagName === 'a') {
      const href = node.properties?.href;
      if (typeof href === 'string' && href.startsWith('/') && !href.startsWith('//')) {
        node.properties.href = prefix + href;
      }
    }
    for (const child of node.children ?? []) walk(child);
  }

  return (/** @type {any} */ tree) => walk(tree);
}
