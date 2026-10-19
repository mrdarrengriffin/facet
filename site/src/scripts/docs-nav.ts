/** Mobile sidebar drawer and table-of-contents highlighting for docs pages. */
export function initDocsNav(): void {
  const toggle = document.querySelector<HTMLButtonElement>('[data-nav-toggle]');
  const sidebar = document.querySelector<HTMLElement>('[data-sidebar]');

  const setOpen = (open: boolean) => {
    document.body.classList.toggle('nav-open', open);
    toggle?.setAttribute('aria-expanded', String(open));
    toggle?.setAttribute('aria-label', open ? 'Close navigation' : 'Open navigation');
  };

  toggle?.addEventListener('click', () => setOpen(!document.body.classList.contains('nav-open')));
  document.querySelector('[data-nav-close]')?.addEventListener('click', () => setOpen(false));
  sidebar?.addEventListener('click', (event) => {
    if ((event.target as HTMLElement).closest('a')) setOpen(false);
  });
  document.addEventListener('keydown', (event) => {
    if (event.key === 'Escape') setOpen(false);
  });

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
