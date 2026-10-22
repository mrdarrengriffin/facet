// The component playground (the original site's UI explorer, main: site/src/scripts/explorer/explorer.ts + controls.ts):
// the props panel's controls (rendered by the page from the build's meta props) drive the live stage; Reset = the
// meta defaults. The stage is a sandbox: it never resets on its own (like the workbench playground).
import { setLiveProps } from './live';

export function initPlayground() {
  for (const root of document.querySelectorAll<HTMLElement>('[data-playground]')) {
    const stage = root.querySelector<HTMLElement>('figure[data-live]');
    if (!stage) continue;
    const defaults = JSON.parse(root.dataset.defaults ?? '{}') as Record<string, unknown>;
    const props: Record<string, unknown> = { ...defaults };
    const inputs = [...root.querySelectorAll<HTMLInputElement | HTMLSelectElement>('[data-prop]')];
    const read = (i: HTMLInputElement | HTMLSelectElement) =>
      i.dataset.kind === 'bool' ? (i as HTMLInputElement).checked : i.dataset.kind === 'range' ? Number(i.value) : i.value;
    const show = (i: HTMLInputElement | HTMLSelectElement) => {
      const out = root.querySelector<HTMLOutputElement>(`output[for="${i.id}"]`);
      if (out) out.textContent = i.value;
    };
    for (const i of inputs) {
      i.addEventListener(i.tagName === 'SELECT' || i.dataset.kind === 'bool' ? 'change' : 'input', () => {
        props[i.dataset.prop!] = read(i);
        show(i);
        setLiveProps(stage, { ...props });
      });
    }
    root.querySelector('[data-pg-reset]')?.addEventListener('click', () => {
      for (const i of inputs) {
        const v = defaults[i.dataset.prop!];
        if (i.dataset.kind === 'bool') (i as HTMLInputElement).checked = !!v;
        else i.value = String(v ?? '');
        i.dispatchEvent(new Event('fselect:sync'));
        show(i);
      }
      Object.assign(props, defaults);
      setLiveProps(stage, { ...props });
    });
  }
}

/** the components sidebar filter (as the old explorer's): hides links and empty groups */
export function initFilter() {
  const input = document.querySelector<HTMLInputElement>('[data-ex-filter]');
  if (!input) return;
  input.addEventListener('input', () => {
    const q = input.value.trim().toLowerCase();
    for (const li of document.querySelectorAll<HTMLElement>('[data-ex-item]')) li.hidden = !!q && !(li.dataset.exItem ?? '').includes(q);
    for (const sec of document.querySelectorAll<HTMLElement>('[data-ex-group]')) sec.hidden = ![...sec.querySelectorAll<HTMLElement>('[data-ex-item]')].some((li) => !li.hidden);
  });
}
