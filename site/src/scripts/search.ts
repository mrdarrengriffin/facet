// Header search: the page list (name + one-line description) embedded at build time, filtered as you type.
type Hit = { n: string; d: string; s: string; h: string };

export function initSearch() {
  const box = document.querySelector<HTMLElement>('[data-search]');
  if (!box) return;
  const input = box.querySelector('input')!, list = box.querySelector('ul')!;
  const all: Hit[] = JSON.parse(box.querySelector('script')!.textContent || '[]');
  let sel = -1;
  const render = () => {
    const q = input.value.trim().toLowerCase();
    sel = -1;
    if (!q) { list.hidden = true; return; }
    const hits = all
      .map((x) => ({ x, r: x.n.toLowerCase().startsWith(q) ? 0 : x.n.toLowerCase().includes(q) ? 1 : x.d.toLowerCase().includes(q) ? 2 : 3 }))
      .filter((y) => y.r < 3).sort((a, b) => a.r - b.r).slice(0, 8);
    list.replaceChildren(...(hits.length ? hits.map(({ x }) => {
      const li = document.createElement('li'), a = document.createElement('a');
      a.href = x.h; a.setAttribute('role', 'option');
      const n = document.createElement('span'), d = document.createElement('span');
      n.className = 'search__name'; n.textContent = x.n;
      d.className = 'search__desc'; d.textContent = `${x.s === 'components' ? 'Component' : 'Foundation'} · ${x.d}`;
      a.append(n, d); li.append(a); return li;
    }) : [Object.assign(document.createElement('li'), { className: 'search__none', textContent: 'Nothing found' })]));
    list.hidden = false;
  };
  const links = () => [...list.querySelectorAll('a')];
  input.addEventListener('input', render);
  input.addEventListener('keydown', (e) => {
    const ls = links();
    if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
      e.preventDefault();
      sel = (sel + (e.key === 'ArrowDown' ? 1 : -1) + ls.length) % Math.max(ls.length, 1);
      ls.forEach((a, i) => a.classList.toggle('is-sel', i === sel));
    } else if (e.key === 'Enter' && ls.length) { location.href = (ls[sel] ?? ls[0]).href; }
    else if (e.key === 'Escape') { input.value = ''; list.hidden = true; }
  });
  document.addEventListener('click', (e) => { if (!box.contains(e.target as Node)) list.hidden = true; });
  document.addEventListener('keydown', (e) => { if (e.key === '/' && document.activeElement !== input && !(e.target as HTMLElement).closest('input,select,textarea')) { e.preventDefault(); input.focus(); } });
}
