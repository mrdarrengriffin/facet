// The mock house's thermostat: a gas boiler zone in the hallway with a heating schedule (17° overnight, 20.5° from
// 06:30, 17° while out 09:00-16:30, 20.5° again until 22:30), presets and a room that warms up while it heats.
import type { ThermostatData } from '@facet/ui';

/** What the docs' Try it can show. `live` = follow the schedule. */
export type ThermostatScene = 'live' | 'heating' | 'idle' | 'off' | 'eco';

const SCHEDULE: [number, number][] = [
  [6.5, 20.5],
  [9, 17],
  [16.5, 20.5],
  [22.5, 17],
];

/** The schedule's set point at `now`, and its next change. */
export function scheduleAt(now: number) {
  const d = new Date(now),
    hr = d.getHours() + d.getMinutes() / 60;
  let ix = SCHEDULE.findIndex(([h]) => h > hr);
  const target = SCHEDULE[(ix === -1 ? SCHEDULE.length : ix) - 1]?.[1] ?? SCHEDULE.at(-1)![1];
  const next = new Date(now);
  if (ix === -1) {
    ix = 0;
    next.setDate(next.getDate() + 1);
  }
  const [h, t] = SCHEDULE[ix]!;
  next.setHours(Math.floor(h), Math.round((h % 1) * 60), 0, 0);
  return { target, next: { at: next.toISOString(), target: t } };
}

/** The thermostat in a scene. `current` = the room temperature to show (default: a little under the set point). */
export function thermostatWith(
  now: number,
  scene: ThermostatScene = 'live',
  current?: number,
): ThermostatData {
  const s = scheduleAt(now);
  const base: ThermostatData = {
    name: 'Hallway',
    current: current ?? 19.4,
    target: s.target,
    min: 5,
    max: 30,
    step: 0.5,
    mode: 'auto',
    modes: ['heat', 'auto', 'off'],
    action: 'idle',
    preset: 'none',
    presets: ['home', 'away', 'eco', 'boost'],
    humidity: 48,
    next: s.next,
  };
  switch (scene) {
    case 'heating':
      return {
        ...base,
        mode: 'heat',
        target: 21,
        current: current ?? 19.4,
        action: 'heating',
        next: null,
      };
    case 'idle':
      return {
        ...base,
        mode: 'heat',
        target: 19,
        current: current ?? 20.1,
        action: 'idle',
        next: null,
      };
    case 'off':
      return { ...base, mode: 'off', target: null, action: 'off', next: null };
    case 'eco':
      return {
        ...base,
        mode: 'heat',
        target: 16,
        preset: 'eco',
        current: current ?? 18.2,
        action: 'idle',
        next: null,
      };
    default:
      return withAction(base);
  }
}

/** What the boiler does for a thermostat's state: heat while the room is 0.2° or more under the set point. */
export function withAction(t: ThermostatData): ThermostatData {
  const on = t.mode !== 'off' && t.target !== null && (t.current ?? 99) < t.target - 0.2;
  return { ...t, action: t.mode === 'off' ? 'off' : on ? 'heating' : 'idle' };
}

/** One minute of house physics: the room warms 0.1° while heating, cools 0.05° otherwise; auto follows the schedule. */
export function thermostatStep(t: ThermostatData, now: number, outside = 9): ThermostatData {
  let next: ThermostatData = { ...t };
  if (t.mode === 'auto') {
    const s = scheduleAt(now);
    next = { ...next, target: s.target, next: s.next };
  }
  const cur = next.current ?? 19;
  const heating = next.mode !== 'off' && next.target !== null && cur < next.target - 0.2;
  const warmer = next.action === 'heating' && next.target !== null && cur < next.target + 0.3;
  const c = heating || warmer ? cur + 0.1 : Math.max(outside, cur - 0.05);
  return {
    ...next,
    current: Math.round(c * 10) / 10,
    action: next.mode === 'off' ? 'off' : heating || warmer ? 'heating' : 'idle',
  };
}
