// Facet select (Darren 2026-10-22: no browser-default selects). Enhances every <select data-fselect>: the native
// select stays in the page (hidden, still the source of truth: its value + "change" event, so existing listeners
// keep working) and a button + listbox in the Facet look takes its place.
// Accessibility: button with aria-haspopup="listbox" / aria-expanded, list role="listbox" with role="option" +
// aria-selected, aria-activedescendant while open. Keys: Enter / Space / Alt+Down / Up / Down open; Up / Down / Home /
// End move; typing jumps to the first match; Enter / Space choose; Escape closes; Tab chooses nothing and moves on.
// A script that changes the native select's value or disabled state dispatches "fselect:sync" on it to update.
let uid = 0;

export function initSelects(root: ParentNode = document) {
  for (const sel of root.querySelectorAll<HTMLSelectElement>('select[data-fselect]')) enhance(sel);
}

function enhance(sel: HTMLSelectElement) {
  if (sel.dataset.fselectReady) return;
  sel.dataset.fselectReady = '1';
  const id = `fs${++uid}`;
  const wrap = document.createElement('span');
  wrap.className = `fselect ${sel.dataset.fselect || ''}`.trim();
  const btn = document.createElement('button');
  btn.type = 'button';
  btn.className = 'fselect__button';
  btn.setAttribute('aria-haspopup', 'listbox');
  btn.setAttribute('aria-expanded', 'false');
  btn.setAttribute('aria-controls', `${id}-list`);
  const label = sel.getAttribute('aria-label') ?? (sel.id ? document.querySelector(`label[for="${sel.id}"]`)?.textContent : '') ?? '';
  if (sel.id) {
    // the visible <label for> now names the button
    btn.id = `${sel.id}-button`;
    const l = document.querySelector<HTMLLabelElement>(`label[for="${sel.id}"]`);
    if (l) { l.htmlFor = btn.id; l.id ||= `${btn.id}-label`; btn.setAttribute('aria-labelledby', `${l.id} ${btn.id}`); }
  } else if (label) btn.setAttribute('aria-label', label);
  const text = document.createElement('span');
  text.className = 'fselect__text';
  const chev = document.createElement('span');
  chev.className = 'fselect__chevron';
  chev.setAttribute('aria-hidden', 'true');
  btn.append(text, chev);
  const list = document.createElement('ul');
  list.className = 'fselect__list';
  list.id = `${id}-list`;
  list.setAttribute('role', 'listbox');
  list.tabIndex = -1;
  if (label) list.setAttribute('aria-label', label);
  list.hidden = true;
  sel.hidden = true;
  sel.tabIndex = -1;
  sel.after(wrap);
  wrap.append(btn, list, sel);

  let active = -1;
  const opts = () => [...list.querySelectorAll<HTMLLIElement>('[role="option"]')];
  const build = () => {
    list.replaceChildren(
      ...[...sel.options].map((o, i) => {
        const li = document.createElement('li');
        li.id = `${id}-o${i}`;
        li.setAttribute('role', 'option');
        li.dataset.value = o.value;
        li.textContent = o.textContent;
        if (o.title) li.title = o.title;
        li.setAttribute('aria-selected', String(o.selected));
        return li;
      }),
    );
  };
  const sync = () => {
    text.textContent = sel.selectedOptions[0]?.textContent ?? '';
    btn.disabled = sel.disabled;
    for (const li of opts()) li.setAttribute('aria-selected', String(li.dataset.value === sel.value));
  };
  const setActive = (i: number) => {
    const os = opts();
    active = Math.max(0, Math.min(os.length - 1, i));
    os.forEach((o, k) => o.classList.toggle('is-active', k === active));
    btn.setAttribute('aria-activedescendant', os[active]?.id ?? '');
    os[active]?.scrollIntoView({ block: 'nearest' });
  };
  const open = () => {
    if (btn.disabled || !list.hidden) return;
    list.hidden = false;
    btn.setAttribute('aria-expanded', 'true');
    wrap.classList.add('is-open');
    setActive(sel.selectedIndex);
  };
  const close = () => {
    if (list.hidden) return;
    list.hidden = true;
    btn.setAttribute('aria-expanded', 'false');
    btn.removeAttribute('aria-activedescendant');
    wrap.classList.remove('is-open');
  };
  const choose = (i: number) => {
    const o = sel.options[i];
    close();
    btn.focus();
    if (!o || o.value === sel.value) return;
    sel.value = o.value;
    sync();
    sel.dispatchEvent(new Event('change', { bubbles: true }));
  };

  btn.addEventListener('click', () => (list.hidden ? open() : close()));
  let typed = '', typedAt = 0;
  btn.addEventListener('keydown', (e) => {
    const isOpen = !list.hidden;
    if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
      e.preventDefault();
      if (!isOpen) open();
      else setActive(active + (e.key === 'ArrowDown' ? 1 : -1));
    } else if (isOpen && (e.key === 'Home' || e.key === 'End')) {
      e.preventDefault();
      setActive(e.key === 'Home' ? 0 : opts().length - 1);
    } else if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      if (isOpen) choose(active); else open();
    } else if (e.key === 'Escape' && isOpen) {
      e.preventDefault();
      e.stopPropagation();
      close();
    } else if (e.key === 'Tab') close();
    else if (e.key.length === 1 && /\S/.test(e.key)) {
      const now = performance.now();
      typed = now - typedAt > 700 ? e.key.toLowerCase() : typed + e.key.toLowerCase();
      typedAt = now;
      const i = opts().findIndex((o) => (o.textContent ?? '').toLowerCase().startsWith(typed));
      if (i >= 0) { if (!isOpen) open(); setActive(i); }
    }
  });
  list.addEventListener('mousemove', (e) => {
    const li = (e.target as HTMLElement).closest<HTMLLIElement>('[role="option"]');
    if (li) setActive(opts().indexOf(li));
  });
  list.addEventListener('mousedown', (e) => e.preventDefault()); // keep focus on the button
  list.addEventListener('click', (e) => {
    const li = (e.target as HTMLElement).closest<HTMLLIElement>('[role="option"]');
    if (li) choose(opts().indexOf(li));
  });
  document.addEventListener('pointerdown', (e) => { if (!wrap.contains(e.target as Node)) close(); });
  sel.addEventListener('fselect:sync', sync);
  sel.addEventListener('change', sync);
  build();
  sync();
}
