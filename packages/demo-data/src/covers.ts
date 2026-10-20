// The mock house's blinds and curtains: a roller blind in the study, curtains in the lounge and a Venetian blind in
// the kitchen. Their motors move at about 5 % a second, like real ones, so the screens see opening / closing first.
import type { CoverData } from '@facet/ui';

/** What the docs' Try it can show. */
export type CoverScene = 'live' | 'roller' | 'curtain' | 'venetian' | 'moving' | 'offline' | 'none';

export const WINDOW_COVERS: CoverData[] = [
  {
    id: 'cover.study_blind',
    name: 'Study blind',
    deviceClass: 'shade',
    state: 'open',
    position: 60,
    features: 15,
  },
  {
    id: 'cover.lounge_curtains',
    name: 'Lounge curtains',
    deviceClass: 'curtain',
    state: 'open',
    position: 100,
    features: 15,
  },
  {
    id: 'cover.kitchen_blind',
    name: 'Kitchen blind',
    deviceClass: 'blind',
    state: 'open',
    position: 100,
    tilt: 60,
    features: 255,
  },
];

/** The covers in a scene. `position` / `tilt` override the first one's. */
export function coversWith(
  scene: CoverScene = 'live',
  position?: number,
  tilt?: number,
): CoverData[] | null {
  if (scene === 'none') return null;
  const pick = (id: string) => WINDOW_COVERS.find((c) => c.id === id)!;
  let list = WINDOW_COVERS.map((c) => ({ ...c }));
  if (scene === 'roller') list = [pick('cover.study_blind')];
  if (scene === 'curtain') list = [pick('cover.lounge_curtains')];
  if (scene === 'venetian') list = [pick('cover.kitchen_blind')];
  list = list.map((c) => ({ ...c }));
  const first = list[0]!;
  if (position !== undefined) {
    first.position = position;
    first.state = position <= 0 ? 'closed' : 'open';
  }
  if (tilt !== undefined && first.tilt !== undefined) first.tilt = tilt;
  if (scene === 'moving') first.state = 'closing';
  if (scene === 'offline') first.state = 'unavailable';
  return list;
}

/** One step of the motors: covers heading somewhere move `step` % towards it. */
export function coversStep(list: CoverData[], goals: Map<string, number>, step = 5): CoverData[] {
  return list.map((c) => {
    const goal = goals.get(c.id);
    if (goal === undefined || c.position === null) return c;
    const p = c.position;
    const next = Math.abs(goal - p) <= step ? goal : p + Math.sign(goal - p) * step;
    if (next === goal) goals.delete(c.id);
    return {
      ...c,
      position: next,
      state: next === goal ? (next <= 0 ? 'closed' : 'open') : goal > p ? 'opening' : 'closing',
    };
  });
}
