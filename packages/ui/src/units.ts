// Units: Settings > Units > Distance (km | miles) and Speed (mph | km/h | knots), the `distanceUnit` / `speedUnit`
// settings of @facet/settings. Screens format distances and speeds through these, so one setting changes every one.
import type { PanelData } from './types.js';

export type DistanceUnit = 'km' | 'mi';
export type SpeedUnit = 'mph' | 'kmh' | 'kn';
export const KM_PER_MI = 1.609344;

/** The units in a store snapshot's settings (defaults km + mph). */
export function unitsOf(s?: Pick<PanelData, 'settings'> | null): {
  distance: DistanceUnit;
  speed: SpeedUnit;
} {
  const d = s?.settings?.distanceUnit,
    v = s?.settings?.speedUnit;
  return {
    distance: d === 'mi' ? 'mi' : 'km',
    speed: v === 'kmh' || v === 'kn' ? v : 'mph',
  };
}

/** km in the unit (no label). */
export const fromKm = (km: number, u: DistanceUnit) => (u === 'mi' ? km / KM_PER_MI : km);

/** "4.3 km", "2.7 mi": one decimal under 10. */
export function distance(km: number, u: DistanceUnit) {
  const v = fromKm(km, u);
  return `${v < 10 ? v.toFixed(1) : Math.round(v)} ${u}`;
}

const SPEED: Record<SpeedUnit, { perKt: number; label: string }> = {
  mph: { perKt: 1.15078, label: 'mph' },
  kmh: { perKt: 1.852, label: 'km/h' },
  kn: { perKt: 1, label: 'kn' },
};
/** A speed in knots (what ADS-B reports) in the unit: "520 mph", "837 km/h", "452 kn". */
export const speedKt = (kt: number | null | undefined, u: SpeedUnit) =>
  kt === null || kt === undefined ? '--' : `${Math.round(kt * SPEED[u].perKt)} ${SPEED[u].label}`;
/** A speed in km/h in the unit (wind, a storm's approach). */
export const speedKmh = (kmh: number | null | undefined, u: SpeedUnit) =>
  kmh === null || kmh === undefined ? '--' : speedKt(kmh / 1.852, u);
