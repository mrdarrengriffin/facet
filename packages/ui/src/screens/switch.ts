// Switch: the simplest screen. One big round button is the hero and the control: its state ("On" / "Off") inside,
// tap anywhere on it to flip. Under it one quiet line: the power draw, if it measures it, and how long it has been in
// that state. The tap shows the new state at once and holds it for 3 s, so a slow device doesn't flicker back; if the
// store still disagrees after that, the store wins. Works for any entity with an on and an off, so it can be the
// fallback screen for things without a dedicated one.
import { bigSwitch } from '../controls.js';
import { hhmm } from '../format.js';
import { at, content, header, page, text } from '../layout.js';
import { sizeAt } from '@facet/tokens';
import type { SwitchData, SwitchKind } from '../types.js';
import { geo, missing, type Screen, type ScreenContext, type ScreenOptions } from './common.js';

const KIND_ICON: Record<SwitchKind, string> = {
  switch: 'power',
  plug: 'power',
  light: 'bulb',
  fan: 'power',
};

/** A power draw: 0.4 → "0.4 W", 1240 → "1,240 W", 12400 → "12.4 kW". */
export function wattText(w: number | null | undefined) {
  if (w === null || w === undefined || !Number.isFinite(w)) return '';
  if (Math.abs(w) >= 10_000) return `${(w / 1000).toFixed(1)} kW`;
  if (Math.abs(w) < 10) return `${Math.round(w * 10) / 10} W`;
  return `${Math.round(w).toLocaleString('en-GB')} W`;
}

/** How long it has been in its state: "just now", "for 12 min", "for 2 h 14 min", "since Mon 14:05". */
export function sinceText(iso: string | null | undefined, now = Date.now()) {
  if (!iso) return '';
  const t = Date.parse(iso);
  if (!Number.isFinite(t)) return '';
  const m = Math.max(0, Math.floor((now - t) / 60_000));
  if (m < 1) return 'just now';
  if (m < 60) return `for ${m} min`;
  if (m < 24 * 60) return `for ${Math.floor(m / 60)} h${m % 60 ? ` ${m % 60} min` : ''}`;
  const d = new Date(t);
  return `since ${d.toLocaleDateString('en-GB', { weekday: 'short' })} ${hhmm(d)}`;
}

/** The quiet line: "212 W · for 2 h 14 min" (power only while on). */
export function switchLine(sw: SwitchData, now = Date.now()) {
  if (sw.on === null) return 'unavailable';
  return [sw.on ? wattText(sw.power) : '', sinceText(sw.changedAt, now)]
    .filter(Boolean)
    .join(' · ');
}

export function switchScreen(ctx: ScreenContext, o: ScreenOptions = {}): Screen {
  const A = ctx.actions;
  // the button fills what's left of the content box after the line under it (384 on the 720 panel)
  const G = geo(ctx),
    CW = G.contentW,
    CH = G.contentH;
  const snap = (v: number) => Math.round((v * G.scale) / G.grid) * G.grid; // reference px at this density
  const LINE = snap(40),
    GAP = snap(32);
  const B = Math.min(sizeAt(G.density)['switch-btn'], CW, CH - LINE - GAP);
  const top = Math.floor((CH - B - GAP - LINE) / 2 / G.grid) * G.grid;
  let sw: SwitchData | null | undefined = null;
  let holdUntil = 0,
    held: boolean | null = null;

  const shown = () => (Date.now() < holdUntil && held !== null ? held : (sw?.on ?? null));
  const btn = bigSwitch({
    size: B,
    iconSize: snap(48),
    onTap: () => {
      const on = shown();
      if (on === null) return;
      held = !on;
      holdUntil = Date.now() + 3000;
      A?.switchSet?.(held);
      render();
    },
  });
  const line = text('title', 'fs-sw-line', 'loading');
  const el = page(
    'fs-switch',
    header({ label: 'Switch', onBack: o.onBack }),
    content(at((CW - B) / 2, top, B, B, btn), at(0, top + B + GAP, CW, LINE, line)),
  );
  const hd = el.querySelector<HTMLElement>('.fs-hlabel .f-fit');

  function render(now = Date.now()) {
    if (!sw) {
      btn.set({ on: null });
      line.textContent = missing(ctx.store);
      el.dataset.on = '';
      return;
    }
    const on = shown();
    btn.set({ on, icon: KIND_ICON[sw.kind ?? 'switch'] });
    el.dataset.on = on === null ? '' : String(on);
    // a held flip that the store hasn't confirmed yet: no power or duration, they belong to the old state
    line.textContent = on !== sw.on ? (on ? 'turning on' : 'turning off') : switchLine(sw, now);
  }

  return {
    el,
    update(s) {
      sw = s.switch;
      if (sw && held !== null && (sw.on === held || Date.now() >= holdUntil)) held = null;
      if (hd) hd.textContent = sw?.name ?? 'Switch';
      render();
    },
    tick(now) {
      if (held !== null && now.getTime() >= holdUntil) held = null;
      render(now.getTime());
    },
  };
}
