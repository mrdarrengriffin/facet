// From the original Facet site (main: site/src/scripts/docs-nav.ts); the mobile drawer moved to the menu (menu.ts).
/** Table-of-contents highlighting for docs pages. */
export function initDocsNav(): void {
  // Highlight the section currently in view.
  const links = [...document.querySelectorAll<HTMLAnchorElement>('[data-toc-link]')];
  const targets = links
    .map((link) => document.getElementById(decodeURIComponent(link.hash.slice(1))))
    .filter((el): el is HTMLElement => el !== null);
  if (targets.length === 0) return;

  const update = () => {
    let active = targets[0];
    for (const target of targets) {
      if (target.getBoundingClientRect().top < 120) active = target;
    }
    for (const link of links) {
      const isActive = link.hash === `#${active?.id}`;
      link.classList.toggle('is-active', isActive);
    }
  };

  update();
  document.addEventListener('scroll', update, { passive: true });
}
