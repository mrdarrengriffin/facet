// Tiny DOM helpers. There's no framework: every component is a function that returns an element.
type Kid = Node | string | number | null | undefined | false;
type Attrs = Record<string, unknown> | null | undefined;

const SVG_NS = 'http://www.w3.org/2000/svg';

function apply(el: Element, attrs: Attrs, kids: Kid[]) {
  for (const [k, v] of Object.entries(attrs || {})) {
    if (v === undefined || v === null || v === false) continue;
    if (k === 'style' && typeof v === 'object') {
      const st = (el as HTMLElement).style;
      for (const [sk, sv] of Object.entries(v as Record<string, string>)) {
        if (sk.startsWith('--')) st.setProperty(sk, sv);
        else (st as unknown as Record<string, string>)[sk] = sv;
      }
    } else if (k.startsWith('on') && typeof v === 'function')
      el.addEventListener(k.slice(2), v as EventListener);
    else el.setAttribute(k, v === true ? '' : String(v));
  }
  for (const c of kids.flat()) {
    if (c === null || c === undefined || c === false) continue;
    el.append(c instanceof Node ? c : document.createTextNode(String(c)));
  }
  return el;
}

/** HTML element: `h('div', { class: 'x', onclick: fn }, child, 'text')`. */
export function h<K extends keyof HTMLElementTagNameMap>(
  tag: K,
  attrs?: Attrs,
  ...kids: Kid[]
): HTMLElementTagNameMap[K] {
  return apply(document.createElement(tag), attrs, kids) as HTMLElementTagNameMap[K];
}

/** SVG element. */
export function s(tag: string, attrs?: Attrs, ...kids: Kid[]): SVGElement {
  return apply(document.createElementNS(SVG_NS, tag), attrs, kids) as SVGElement;
}

/** Point on a circle. Angles follow LVGL: 0° = 3 o'clock, clockwise. */
export function polar(cx: number, cy: number, r: number, deg: number) {
  const a = (deg * Math.PI) / 180;
  return [cx + r * Math.cos(a), cy + r * Math.sin(a)] as const;
}

/** SVG path for an arc, clockwise from `start` to `end` (LVGL angles). */
export function arcPath(cx: number, cy: number, r: number, start: number, end: number) {
  let sweep = (end - start + 360) % 360;
  if (sweep === 0 && end !== start) sweep = 360;
  if (sweep < 0.01) return '';
  if (sweep >= 359.99) sweep = 359.99;
  const [x1, y1] = polar(cx, cy, r, start);
  const [x2, y2] = polar(cx, cy, r, start + sweep);
  return `M${x1.toFixed(2)} ${y1.toFixed(2)} A${r} ${r} 0 ${sweep > 180 ? 1 : 0} 1 ${x2.toFixed(2)} ${y2.toFixed(2)}`;
}

/** True when the user asked for less motion. */
export const reducedMotion = () =>
  typeof matchMedia !== 'undefined' && matchMedia('(prefers-reduced-motion: reduce)').matches;
