/**
 * Shared client code for the pages that run Facet live: sounds (off until you turn them on, remembered), the 720 px
 * frame scaled to fit its container, and per-preview theme overrides.
 */
import {
  installSoundHooks,
  onSettingsChange,
  settings,
  setEnabled,
  unlockOnGesture,
} from '@facet/sounds';

const SOUND_KEY = 'facet-sounds';
let soundsReady = false;

/** Install the sound hooks once per page and restore the on/off choice. */
export function initSounds(): void {
  if (soundsReady) return;
  soundsReady = true;
  unlockOnGesture();
  installSoundHooks(document, { scope: '.f-screen, .f-sound-scope' });
  try {
    if (localStorage.getItem(SOUND_KEY) === 'on') setEnabled(true);
  } catch {
    // storage blocked: sounds just start off
  }
  const sync = () => {
    for (const b of document.querySelectorAll<HTMLButtonElement>('[data-sound-toggle]')) {
      b.setAttribute('aria-pressed', String(settings.enabled));
      const label = b.querySelector('[data-sound-label]');
      if (label) label.textContent = settings.enabled ? 'Sound on' : 'Sound off';
    }
  };
  onSettingsChange(sync);
  sync();
  document.addEventListener('click', (e) => {
    const b = (e.target as Element).closest?.('[data-sound-toggle]');
    if (!b) return;
    setEnabled(!settings.enabled);
    try {
      localStorage.setItem(SOUND_KEY, settings.enabled ? 'on' : 'off');
    } catch {
      // ignore
    }
  });
}

/**
 * Scale a fixed-size frame (default 720 × 720) to the width of its viewport, never above 1:1. The viewport gets the
 * scaled height so the page flows around it.
 */
export function fitFrame(
  viewport: HTMLElement,
  frame: HTMLElement,
  size = 720,
  height = size,
  max = 1,
): void {
  const apply = () => {
    const w = viewport.clientWidth;
    if (!w) return;
    const k = Math.min(max, w / size);
    frame.style.transform = `scale(${k})`;
    frame.style.transformOrigin = 'top left';
    viewport.style.height = `${Math.round(height * k)}px`;
    frame.style.marginLeft = `${Math.max(0, (w - size * k) / 2)}px`;
  };
  new ResizeObserver(apply).observe(viewport);
  apply();
}

/**
 * Theme override buttons inside `root` (`[data-theme-set="site|dark|light"]`) set `data-facet-theme` on `target`.
 * "site" removes it, so the target follows the page's light/dark toggle live.
 */
export function initThemeOverride(root: HTMLElement, target: HTMLElement): void {
  const buttons = [...root.querySelectorAll<HTMLButtonElement>('[data-theme-set]')];
  const set = (v: string) => {
    if (v === 'site') delete target.dataset.facetTheme;
    else target.dataset.facetTheme = v;
    for (const b of buttons) b.setAttribute('aria-pressed', String(b.dataset.themeSet === v));
  };
  for (const b of buttons) b.addEventListener('click', () => set(b.dataset.themeSet ?? 'site'));
  set('site');
}
