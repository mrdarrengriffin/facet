// The mock house's 3D print: a little lighthouse (made up, drawn from a shape function, so no third-party model ships
// with Facet), 40 x 40 x 60 mm at 0.2 mm layers. Hollow tower with windows, a gallery and a lantern room, so the cut
// face shows walls the way a real print does.
import { fromShape, type Grid, type PrintData } from '@facet/ui';

const C = 20; // centre (mm)
let model: Grid | null = null;

/** The lighthouse grid (built once, ~50 ms). */
export function lighthouse(): Grid {
  if (model) return model;
  model = fromShape(
    (x, y, z) => {
      const dx = x - C,
        dy = y - C,
        r = Math.hypot(dx, dy),
        a = Math.atan2(dy, dx);
      if (z < 3) return Math.max(Math.abs(dx), Math.abs(dy)) < 17 - Math.max(0, z - 2) * 1.5; // plinth, chamfered
      if (z < 44) {
        const R = 11 - ((z - 3) / 41) * 3.5; // tower tapers 11 -> 7.5
        if (r > R) return false;
        if (z < 6) return true; // solid foot
        if (r < R - 1.6) return false; // hollow
        const win = (zc: number) =>
          Math.abs(z - zc) < 2.4 && Math.abs(Math.sin(a - Math.PI / 4)) < 0.18; // windows
        return !(win(17) || win(31));
      }
      if (z < 46) return r < 11; // gallery floor
      if (z < 49.5) return (r > 10.2 && r < 11 && Math.cos(a * 16) > -0.6) || r < 6.2; // railing posts + lantern
      if (z < 53) return r < 6.2 && (r > 5 ? Math.abs(Math.sin(a * 3)) > 0.35 : r < 1.2); // lantern room, glazing gaps
      if (z < 58) return r < 7 - ((z - 53) / 5) * 6.2; // roof
      return r < 0.9; // finial
    },
    { w: 40, d: 40, h: 60, cell: 0.5, layer: 0.2, name: 'Lighthouse' },
  );
  return model;
}

export type PrintScene = 'printing' | 'paused' | 'preparing' | 'done' | 'none';

/** The lighthouse print at `progress` (0-1): a 3 h 20 min job, layer and time left to match. */
export function printWith(
  now: number,
  state: PrintScene = 'printing',
  progress = 0.45,
): PrintData | null {
  if (state === 'none') return null;
  const g = lighthouse();
  const total = 200,
    f = state === 'done' ? 1 : state === 'preparing' ? 0 : Math.max(0, Math.min(1, progress));
  const left = Math.round((1 - f) * total);
  return {
    status: state,
    stage: state === 'preparing' ? 'heating the bed' : null,
    task: 'Lighthouse',
    layer: Math.round(f * g.layerZ.length),
    layers: g.layerZ.length,
    progress: Math.round(f * 100),
    remainingMin: state === 'done' ? 0 : left,
    endTs: Math.round(now / 1000 + left * 60),
    filament: '#3E7CB1',
    model: g,
  };
}
