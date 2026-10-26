// The mobile menu (Header.astro): a modal <dialog> sheet. Opens from the header's menu button; Escape, the close
// button, a tap on the backdrop or a link close it. While open the page behind is inert (showModal) and Tab wraps
// inside. Motion: the sheet slides in with the fade duration + ease-out tokens (none with reduced motion).
const FOCUSABLE = 'a[href], button:not([disabled]), input, select, [tabindex]:not([tabindex="-1"])';

export function initMenu(): void {
  const dlg = document.querySelector<HTMLDialogElement>('[data-menu]');
  const btn = document.querySelector<HTMLButtonElement>('[data-menu-open]');
  if (!dlg || !btn) return;

  // a docs / components page: its own sidebar list (current page marked) instead of the generic sections
  const own = document.querySelector<HTMLElement>('[data-sidebar] nav');
  const slot = dlg.querySelector<HTMLElement>('[data-menu-sections]');
  if (own && slot) {
    const copy = own.cloneNode(true) as HTMLElement;
    copy.querySelectorAll('[id]').forEach((n) => n.removeAttribute('id'));
    slot.replaceChildren(copy);
  }

  const reduced = () => matchMedia('(prefers-reduced-motion: reduce)').matches;
  let closing = 0;
  const open = () => {
    if (dlg.open) return;
    clearTimeout(closing);
    dlg.showModal();
    btn.setAttribute('aria-expanded', 'true');
    requestAnimationFrame(() => dlg.classList.add('is-open'));
    (dlg.querySelector<HTMLElement>('[aria-current="page"]') ?? dlg.querySelector<HTMLElement>(FOCUSABLE))?.focus({ preventScroll: false });
  };
  const close = () => {
    if (!dlg.open) return;
    dlg.classList.remove('is-open');
    btn.setAttribute('aria-expanded', 'false');
    const done = () => { dlg.close(); btn.focus(); };
    if (reduced()) done();
    else closing = window.setTimeout(done, parseFloat(getComputedStyle(dlg).transitionDuration) * 1000 || 0);
  };

  btn.addEventListener('click', open);
  dlg.querySelector('[data-menu-close]')?.addEventListener('click', close);
  dlg.addEventListener('cancel', (e) => { e.preventDefault(); close(); });
  dlg.addEventListener('click', (e) => {
    if ((e.target as HTMLElement).closest('a[href]')) { dlg.classList.remove('is-open'); dlg.close(); return; }
    if (e.target !== dlg) return;
    const r = dlg.getBoundingClientRect();
    if (e.clientX < r.left || e.clientX > r.right || e.clientY < r.top || e.clientY > r.bottom) close();
  });
  dlg.addEventListener('keydown', (e) => {
    if (e.key !== 'Tab') return;
    const f = [...dlg.querySelectorAll<HTMLElement>(FOCUSABLE)].filter((n) => n.offsetParent !== null);
    if (!f.length) return;
    const first = f[0], last = f[f.length - 1];
    if (e.shiftKey && document.activeElement === first) { e.preventDefault(); last.focus(); }
    else if (!e.shiftKey && document.activeElement === last) { e.preventDefault(); first.focus(); }
  });
  // wider than the menu breakpoint: the header nav is back, so the sheet goes
  matchMedia('(min-width: 861px)').addEventListener('change', (m) => { if (m.matches && dlg.open) { dlg.classList.remove('is-open'); dlg.close(); } });
}
