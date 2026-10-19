/**
 * Prop controls shared by the UI explorer and the screen playgrounds: a select, text field, range or toggle, each with
 * its label. Change events hand the new value to `onChange`.
 */
export type Control =
  | { key: string; label: string; type: 'select'; options: readonly string[]; value: string }
  | { key: string; label: string; type: 'text'; value: string }
  | {
      key: string;
      label: string;
      type: 'range';
      min: number;
      max: number;
      step?: number;
      value: number;
    }
  | { key: string; label: string; type: 'toggle'; value: boolean };

export type Props = Record<string, string | number | boolean>;

const el = <K extends keyof HTMLElementTagNameMap>(
  tag: K,
  attrs: Record<string, string> = {},
  ...kids: (Node | string)[]
) => {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
  e.append(...kids);
  return e;
};

/** One labelled control. `idPrefix` keeps the ids unique when several sets of controls share a page. */
export function controlEl(
  c: Control,
  value: string | number | boolean,
  onChange: (v: string | number | boolean) => void,
  idPrefix: string,
): HTMLElement {
  const id = `${idPrefix}-${c.key}`;
  const wrap = el('div', { class: `ex-control ex-control--${c.type}` });
  const lbl = el('label', { for: id }, c.label);
  if (c.type === 'select') {
    const s = el('select', { id });
    for (const o of c.options) s.append(el('option', { value: o }, o));
    s.value = String(value);
    s.addEventListener('change', () => onChange(s.value));
    wrap.append(lbl, s);
  } else if (c.type === 'text') {
    const i = el('input', { id, type: 'text', value: String(value) });
    i.addEventListener('input', () => onChange(i.value));
    wrap.append(lbl, i);
  } else if (c.type === 'range') {
    const out = el('output', { for: id }, String(value));
    const i = el('input', {
      id,
      type: 'range',
      min: String(c.min),
      max: String(c.max),
      step: String(c.step ?? 1),
      value: String(value),
    });
    i.addEventListener('input', () => {
      out.textContent = i.value;
      onChange(Number(i.value));
    });
    wrap.append(el('div', { class: 'ex-control__row' }, lbl, out), i);
  } else {
    const i = el('input', { id, type: 'checkbox' });
    i.checked = !!value;
    i.addEventListener('change', () => onChange(i.checked));
    wrap.append(el('div', { class: 'ex-control__row ex-control__row--toggle' }, i, lbl));
  }
  return wrap;
}
