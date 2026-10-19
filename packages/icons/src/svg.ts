const SVG_NS = 'http://www.w3.org/2000/svg';

type Attrs = Record<string, unknown> | null | undefined;

/** Create an SVG element. `style` takes an object (custom properties allowed); other attributes are set as strings. */
export function svg(
  tag: string,
  attrs?: Attrs,
  ...kids: (Node | null | undefined | false)[]
): SVGElement {
  const el = document.createElementNS(SVG_NS, tag);
  for (const [k, v] of Object.entries(attrs ?? {})) {
    if (v === undefined || v === null || v === false) continue;
    if (k === 'style' && typeof v === 'object') {
      for (const [sk, sv] of Object.entries(v as Record<string, string | number>)) {
        if (sk.startsWith('--')) el.style.setProperty(sk, String(sv));
        else (el.style as unknown as Record<string, string>)[sk] = String(sv);
      }
    } else el.setAttribute(k, v === true ? '' : String(v));
  }
  for (const kid of kids) if (kid) el.append(kid);
  return el;
}
