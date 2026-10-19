// Formatting helpers shared by the screens. Times use 24-hour en-GB formatting, like the panels.
const LOCALE = 'en-GB';

/** "14:05" */
export const hhmm = (d: Date) =>
  d.toLocaleTimeString(LOCALE, { hour: '2-digit', minute: '2-digit', hour12: false });
/** "14:05" from an ISO string, "--:--" when missing. */
export const hm = (iso?: string | null) => (iso ? hhmm(new Date(iso)) : '--:--');
/** "18°" */
export const deg = (v: number | null | undefined) =>
  v === null || v === undefined ? '--' : `${Math.round(v)}°`;
/** Pounds: "£2.42" */
export const money = (gbp: number | null | undefined) =>
  gbp === null || gbp === undefined ? '£--' : `£${gbp.toFixed(2)}`;
/** Pence from pounds: "25.4p" */
export const pence = (gbp: number | null | undefined, d = 1) =>
  gbp === null || gbp === undefined ? '--' : `${(gbp * 100).toFixed(d)}p`;
/** Compact age: "20m", "4h", "3d", "2w". */
export function age(iso: string | number, now = Date.now()) {
  const t = typeof iso === 'number' ? iso : Date.parse(iso);
  const m = Math.max(0, Math.round((now - t) / 60000));
  return m < 60
    ? `${m}m`
    : m < 48 * 60
      ? `${Math.round(m / 60)}h`
      : m < 14 * 1440
        ? `${Math.round(m / 1440)}d`
        : `${Math.round(m / 10080)}w`;
}
/** "just now", "4 min ago", "2 h ago" */
export const ago = (t: number, now = Date.now()) => {
  const m = Math.max(0, Math.round((now - t) / 60_000));
  return m < 1 ? 'just now' : m < 60 ? `${m} min ago` : `${Math.round(m / 60)} h ago`;
};
/** Drop placeholder strings some sources use for "no value". */
export const clean = (v?: string | null) =>
  !v || ['None', 'unknown', 'unavailable'].includes(v) ? '' : v;
