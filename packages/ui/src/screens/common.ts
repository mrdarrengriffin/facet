// Shared types and helpers for the screens.
import type { Store } from '../store.js';
import type { MediaState, PanelActions, PanelData, Player, WeatherData } from '../types.js';

/** What every screen gets: the store it reads and the actions it can call. */
export interface ScreenContext {
  store: Store<PanelData>;
  actions?: PanelActions;
}

/** A screen: its element plus lifecycle hooks the device (or a preview) calls. */
export interface Screen {
  el: HTMLElement;
  /** New data. */
  update?(data: PanelData): void;
  /** Once a second. */
  tick?(now: Date): void;
  /** The screen was shown. */
  enter?(): void;
  /** The screen was hidden. */
  leave?(): void;
}

export interface ScreenOptions {
  /** Called by the header's back button. Leave it out to hide the button (e.g. in a docs preview). */
  onBack?: () => void;
}

/** Hook a screen up to its store: updates now and on every change. Returns an unsubscribe function. */
export function bindScreen(screen: Screen, store: Store<PanelData>): () => void {
  return store.subscribe((s) => screen.update?.(s));
}

/** "Kitchen speaker" → "Kitchen". */
export const speakerName = (n?: string | null) => (n ? n.replace(/\s+speaker$/i, '') || n : '');

/** The icon for a player. */
export const playerIcon = (p: Player) =>
  p.kind === 'stream'
    ? 'stream'
    : p.kind === 'group'
      ? 'speakers'
      : p.kind === 'tv'
        ? 'tv'
        : 'speaker';

/** A player's status line: "playing · Title · Artist", "paused", "idle · 3 speakers". */
export function playerStatus(p: Player) {
  const what = [p.title, p.artist].filter(Boolean).join(' · ');
  const base =
    p.state === 'playing' || p.state === 'paused'
      ? `${p.state}${what ? ` · ${what}` : ''}`
      : p.state;
  if (p.kind === 'stream') return p.source ? `${base} · on ${p.source}` : base;
  return p.kind === 'group' && p.members ? `${base} · ${p.members} speakers` : base;
}

/** Where a track is now, moving the last known position on while it plays. */
export function mediaPos(m: MediaState | null | undefined, now = Date.now()) {
  if (!m || !m.duration) return { pos: 0, dur: 0, frac: 0 };
  let pos = m.position ?? 0;
  if (m.state === 'playing' && m.positionAt) pos += (now - m.positionAt) / 1000;
  pos = Math.min(Math.max(pos, 0), m.duration);
  return { pos, dur: m.duration, frac: pos / m.duration };
}

const mins = (iso: string) => {
  const d = new Date(iso);
  return d.getHours() * 60 + d.getMinutes();
};
/** Day or night at a time, from the next sunrise and sunset (time of day only, which is enough for icons). */
export function isDay(d: WeatherData | null | undefined, t = new Date().toISOString()) {
  if (!d?.sun) {
    const hr = new Date(t).getHours();
    return hr >= 7 && hr < 19;
  }
  const m = mins(t),
    r = mins(d.sun.next_rising),
    s = mins(d.sun.next_setting);
  return m >= r && m < s;
}
