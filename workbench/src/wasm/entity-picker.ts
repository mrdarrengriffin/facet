// The ONE searchable entity picker: the editor lab's card settings and the /wasm workbench's
// entity props both use it. Closed = the current entity (a button .ep-cur); open = a search field + the entities of
// the field's domain(s) (filter by name or id, arrows + Enter, Escape, or click); picking closes it. A full entity id
// of the right domain can be used even when the list doesn't have it. Styles: entity-picker.css.
import './entity-picker.css';

export type EntOpt = { id: string; name: string };
export type PickerOpts = {
  doms: string[];
  cur: string;
  /** this house's entities per domain (GET /api/wasm/sources `options`) */
  options: Record<string, EntOpt[]>;
  pick: (id: string) => void;
  /** open state owned by the caller: `setOpen(v, redraw)` stores it and (redraw) re-renders the picker's host.
   *  Left out = the picker keeps its own state and redraws itself in place. */
  open?: boolean;
  setOpen?: (v: boolean, redraw: boolean) => void;
  /** a first "no entity" row with this label (picks ''); its label also shows when nothing is picked */
  none?: string;
};

function h<K extends keyof HTMLElementTagNameMap>(tag: K, cls = '', text = ''): HTMLElementTagNameMap[K] {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text) e.textContent = text;
  return e;
}

export const entityName = (options: Record<string, EntOpt[]>, id?: string) => Object.values(options).flat().find((o) => o.id === id)?.name ?? '';

export function entityPicker(o: PickerOpts): HTMLElement {
  const wrap = h('div', 'ep');
  wrap.dataset.testid = 'entity-picker';
  let open = !!o.open;
  const setOpen = o.setOpen ?? ((v: boolean, redraw: boolean) => { open = v; if (redraw) { draw(); if (v) wrap.querySelector('input')?.focus(); } });
  const { doms, options } = o;
  let cur = o.cur;
  const choose = (id: string) => { setOpen(false, false); open = false; cur = id; draw(); o.pick(id); };
  function draw() {
    wrap.className = 'ep' + (open ? ' open' : '');
    const now = h('button', 'ep-cur');
    now.type = 'button';
    now.setAttribute('aria-expanded', String(open));
    now.append(h('b', '', entityName(options, cur) || cur || o.none || 'None'), h('span', 'id', cur));
    now.onclick = () => setOpen(!open, true);
    wrap.replaceChildren(now);
    if (!open) return;
    const q = h('input', 'search');
    q.type = 'search';
    q.placeholder = `Search ${doms.join(' or ')} entities`;
    q.setAttribute('aria-label', 'Search entities');
    const list = h('ul', 'ep-list');
    list.setAttribute('role', 'listbox');
    const opts = doms.flatMap((d) => options[d] ?? []);
    let act = 0, hits: EntOpt[] = [];
    const fill = () => {
      const t = q.value.trim().toLowerCase();
      hits = opts.filter((x) => !t || x.name.toLowerCase().includes(t) || x.id.includes(t)).slice(0, 60);
      if (/^[a-z_]+\.[a-z0-9_]+$/.test(t) && doms.includes(t.split('.')[0]) && !hits.some((x) => x.id === t)) hits.push({ id: t, name: `Use ${t}` });
      if (o.none !== undefined && !t) hits.unshift({ id: '', name: o.none });
      act = Math.min(act, Math.max(0, hits.length - 1));
      const more = t ? 0 : opts.length - (hits.length - (o.none !== undefined ? 1 : 0));
      list.replaceChildren(...(hits.length ? hits.map((x, k) => {
        const li = h('li'), b = h('button', (x.id === cur ? 'on' : '') + (k === act ? ' act' : ''));
        b.type = 'button';
        b.dataset.id = x.id;
        b.append(h('span', 'nm', x.name), h('span', 'id', x.id));
        b.onclick = () => choose(x.id);
        li.append(b);
        return li;
      }) : [h('li', 'muted ep-none', opts.length ? 'Nothing matches.' : `No ${doms.join(' or ')} entities known here: type a full entity id.`)]),
      ...(more > 0 ? [h('li', 'muted ep-none', `${more} more: type to narrow the list.`)] : []));
    };
    q.oninput = () => { act = 0; fill(); };
    q.onkeydown = (ev) => {
      if (ev.key === 'ArrowDown' || ev.key === 'ArrowUp') { ev.preventDefault(); act = (act + (ev.key === 'ArrowDown' ? 1 : -1) + hits.length) % Math.max(1, hits.length); fill(); list.querySelector('.act')?.scrollIntoView({ block: 'nearest' }); }
      else if (ev.key === 'Enter' && hits[act]) { ev.preventDefault(); choose(hits[act].id); }
      else if (ev.key === 'Escape') { ev.preventDefault(); ev.stopPropagation(); setOpen(false, true); }
    };
    fill();
    wrap.append(q, list);
  }
  draw();
  return wrap;
}
