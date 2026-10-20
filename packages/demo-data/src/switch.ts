// The mock house's switch: a dehumidifier on a smart plug in the study, drawing about 210 W while it runs.
import type { SwitchData, SwitchKind } from '@facet/ui';

/** What the docs' Try it can show. */
export type SwitchScene = 'on' | 'off' | 'unavailable';

/** Its draw while running, with a little wobble. */
export const switchWatts = (now: number) => Math.round(206 + 9 * Math.sin(now / 7000));

/** The switch in a scene. `minutes` = how long it has been in that state. */
export function switchWith(
  now: number,
  scene: SwitchScene = 'on',
  minutes = 134,
  kind: SwitchKind = 'plug',
): SwitchData {
  const on = scene === 'unavailable' ? null : scene === 'on';
  return {
    name: kind === 'light' ? 'Shelf lights' : 'Dehumidifier',
    kind,
    on,
    power: on ? switchWatts(now) : on === false ? 0.4 : null,
    changedAt: new Date(now - minutes * 60_000).toISOString(),
  };
}
