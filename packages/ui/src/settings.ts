// Settings controls (segmented, switch, accent swatches) and `applySettings`, which puts the look settings on an
// element. The accent sets and text sizes come from @facet/settings; their CSS is generated into styles.css at build
// time (scripts/build-css.mjs), keyed on `data-facet-accent` and `data-facet-text`.
import {
  accentColors,
  resolve,
  setting,
  type Option,
  type SelectSetting,
  type Value,
  type Values,
} from '@facet/settings';
import { fit8, live } from './atoms.js';
import { h } from './dom.js';

/**
 * A segmented control: 48 tall, hairline outline, one segment per option (caps label, 24 padding). Selected = pressed
 * surface, brighter outline and full text colour. `set(id)`.
 */
export function segmented(o: {
  options: { id: string; label: string }[];
  value: string;
  onPick?: (id: string) => void;
}) {
  const btns = o.options.map((op) =>
    live(
      h(
        'button',
        { class: 'f-seg-b', type: 'button', role: 'radio', 'data-v': op.id },
        fit8(h('span', { class: 'fst-label' }, op.label)),
      ),
      () => o.onPick?.(op.id),
    ),
  );
  const el = h('div', { class: 'f-seg', role: 'radiogroup' }, ...btns);
  const api = Object.assign(el, {
    set(v: string) {
      btns.forEach((b) => b.setAttribute('aria-checked', String(b.dataset.v === v)));
    },
  });
  api.set(o.value);
  return api;
}

/** A switch: 80 × 48 track, 32 knob 8 in from the edge. On = accent track, knob on the right. `set(on)`. */
export function toggleSwitch(o: { on: boolean; onToggle?: (on: boolean) => void; label?: string }) {
  const el = live(
    h(
      'button',
      { class: 'f-toggle', type: 'button', role: 'switch', 'aria-label': o.label },
      h('i'),
    ),
    () => o.onToggle?.(el.getAttribute('aria-checked') !== 'true'),
  );
  const api = Object.assign(el, {
    set(on: boolean) {
      el.setAttribute('aria-checked', String(on));
      el.dataset.snd = on ? 'toggle-off' : 'toggle-on';
    },
  });
  api.set(o.on);
  return api;
}

/** Accent swatches: 48 touch targets with a 32 dot in each accent (for the theme in use); selected = a 2 px ring. */
export function accentSwatches(o: { value: string; onPick?: (id: string) => void }) {
  const s = setting('accent') as SelectSetting;
  const dots = s.options.map((op: Option) => {
    const d = accentColors(op.id, 'dark').accent,
      l = accentColors(op.id, 'light').accent;
    return live(
      h(
        'button',
        { class: 'f-sw', type: 'button', role: 'radio', 'data-v': op.id, 'aria-label': op.label },
        h('i', { style: { background: `light-dark(${l}, ${d})` } }),
      ),
      () => o.onPick?.(op.id),
    );
  });
  const el = h('div', { class: 'f-sws', role: 'radiogroup' }, ...dots);
  const api = Object.assign(el, {
    set(v: string) {
      dots.forEach((b) => b.setAttribute('aria-checked', String(b.dataset.v === v)));
    },
  });
  api.set(o.value);
  return api;
}

/**
 * Put the look settings on an element (a device frame, a preview): `data-facet-accent`, `data-facet-text` and, for
 * Light or Dark, `data-facet-theme` (Auto removes it so the page decides). A setting left out isn't touched, so the
 * element follows its parent.
 */
export function applySettings(el: HTMLElement, values: Partial<Values> | null | undefined) {
  const v = values ?? {};
  const all = resolve(v);
  const put = (attr: string, on: boolean, val: Value) =>
    on ? el.setAttribute(attr, String(val)) : el.removeAttribute(attr);
  if ('accent' in v) put('data-facet-accent', true, all.accent!);
  if ('textSize' in v) put('data-facet-text', all.textSize !== 'standard', all.textSize!);
  if ('theme' in v) put('data-facet-theme', all.theme !== 'auto', all.theme!);
}
