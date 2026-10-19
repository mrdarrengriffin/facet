/**
 * Shared client code for the pages that run Facet live: sounds (off until you turn them on, remembered), the 720 px
 * frame scaled to fit its container, and per-preview theme overrides.
 */
import {
  hasSound,
  installSoundHooks,
  onSettingsChange,
  settings,
  setEnabled,
  setPack,
  setVolume,
  unlockOnGesture,
} from '@facet/sounds';

const SOUND_KEY = 'facet-sounds';
const PACK_KEY = 'facet-sound-pack';
let soundsReady = false;

/** The sound pack the site's live pages use (the visitor's pick from the Sounds page, else the default). */
export const getSitePack = (): string => settings.pack;
/** Use another pack on every live page of the site, remembered in this browser. */
export function setSitePack(pack: string): void {
  if (!setPack(pack)) return;
  try {
    localStorage.setItem(PACK_KEY, settings.pack);
  } catch {
    // storage blocked: the pick lasts for this page only
  }
}
/** Get told when the pack changes. */
export function onSitePackChange(fn: (pack: string) => void): void {
  let last = settings.pack;
  onSettingsChange((s) => {
    if (s.pack !== last) fn((last = s.pack));
  });
}

/**
 * Installs the sound hooks once per page, restores the visitor's pack and on/off choice, and wires the site's sound
 * toggle. `defaultOn` turns sounds on when the visitor hasn't chosen yet (the demo does this;
 * an explicit "off" from the toggle is always respected). `volume` is the master volume, 0-1.
 */
export function initSounds(opts: { defaultOn?: boolean; volume?: number } = {}): void {
  if (soundsReady) return;
  soundsReady = true;
  if (opts.volume !== undefined) setVolume(opts.volume);
  try {
    const pack = localStorage.getItem(PACK_KEY);
    if (pack) setPack(pack);
  } catch {
    // storage blocked: keep the default pack
  }
  unlockOnGesture();
  installSoundHooks(document, { scope: '.f-screen, .f-sound-scope' });
  let saved: string | null = null;
  try {
    saved = localStorage.getItem(SOUND_KEY);
  } catch {
    // storage blocked: fall back to the default
  }
  if (saved === 'on' || (saved === null && opts.defaultOn)) setEnabled(true);
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

/**
 * Show the sound switches in `bar` only while something in `target` makes a sound (anything the sound hooks play for,
 * see `hasSound`). Re-checked whenever `target` changes. `force` overrides the check: true always shows, false hides.
 * Returns a function that re-checks with a new `force`.
 */
export function soundToggleFor(
  bar: ParentNode,
  target: HTMLElement,
  force?: boolean,
): (force?: boolean) => void {
  const toggles = [...bar.querySelectorAll<HTMLElement>('[data-sound-toggle]')];
  let forced = force;
  let queued = false;
  const check = () => {
    queued = false;
    const on = forced ?? hasSound(target);
    for (const t of toggles) t.hidden = !on;
  };
  new MutationObserver(() => {
    if (queued) return;
    queued = true;
    requestAnimationFrame(check);
  }).observe(target, {
    childList: true,
    subtree: true,
    attributes: true,
    attributeFilter: ['class', 'disabled'],
  });
  check();
  return (f?: boolean) => {
    forced = f;
    check();
  };
}
