// Shared helpers for the family 1 (controls + inputs) functional tests. Not a test file (no .test.mjs).
export const D = (den, a, b) => (den === '1x' ? b : a);                 // a token at 720 (1.5x) / 480 (1x)
export const evs = (ev, name) => ev.filter((e) => e.kind === 'event' && e.name === name);
export const last = (ev, name) => { const e = evs(ev, name).pop(); return e ? String(e.detail ?? '') : null; };
export const num = (d) => (d === null || d === undefined ? null : parseFloat(String(d)));
export const snd = (ev) => ev.filter((e) => e.kind === 'sound').map((e) => e.name);
export const px = (t, x, y) => {  // the rendered pixel [r, g, b] at x, y (behaviour reads: drawn vs not, never a colour)
  const b = t.b; b.m._fw_refresh(b.stage); const h = b.m.HEAPU8, p = b.m._fw_frame(b.stage) + (Math.round(y) * b.S + Math.round(x)) * 4;
  return [h[p], h[p + 1], h[p + 2]];
};
export const diff = (a, c, d = 30) => Math.abs(a[0] - c[0]) + Math.abs(a[1] - c[1]) + Math.abs(a[2] - c[2]) > d;
export const ring = (box, f, inset = 3) => {  // a point on a 270 deg dial ring (open at the bottom) at fraction f of the range
  const cx = box.x + box.w / 2, cy = box.y + box.h / 2, r = box.w / 2 - inset, a = ((135 + 270 * f) * Math.PI) / 180;
  return [Math.round(cx + r * Math.cos(a)), Math.round(cy + r * Math.sin(a))];
};
export const frac = (box, x, y) => {  // the inverse: where on the ring a point is (0..1)
  let a = (Math.atan2(y - (box.y + box.h / 2), x - (box.x + box.w / 2)) * 180) / Math.PI; a = (a - 135 + 720) % 360; return a / 270;
};
export const comps = (t, id) => t.comps().filter((c) => c.ids.at(-1) === id && c.w > 0 && c.h > 0);
export const ctr = (o) => [Math.round(o.x + o.w / 2), Math.round(o.y + o.h / 2)];
