// Site + preview settings: theme dark / light (header toggle, as on the original site), sound on / off (the old
// "Sound off" chip in each preview bar), panel size 720 / 480, accent set and sound pack (preview bars). Kept per browser
// (localStorage, best effort) and mirrored on <html data-theme data-accent data-device> so the site's own colours
// follow the same tokens as the previews.
export type Prefs = { theme: 'dark' | 'light'; device: 720 | 480; accent: string; sound: boolean; pack: string };
const KEY = 'facet-docs-prefs';
const root = document.documentElement;
const subs: ((p: Prefs) => void)[] = [];

export function getPrefs(): Prefs {
  return {
    theme: root.dataset.theme === 'light' ? 'light' : 'dark',
    device: root.dataset.device === '480' ? 480 : 720,
    accent: root.dataset.accent || root.dataset.accentDefault || '',
    sound: root.dataset.sound !== 'off',
    pack: root.dataset.pack || root.dataset.packDefault || '',
  };
}
export function setPrefs(ch: Partial<Prefs>) {
  const p = { ...getPrefs(), ...ch };
  root.dataset.theme = p.theme;
  root.dataset.device = String(p.device);
  root.dataset.accent = p.accent;
  root.dataset.sound = p.sound ? 'on' : 'off';
  root.dataset.pack = p.pack;
  try { localStorage.setItem(KEY, JSON.stringify(p)); } catch { /* private mode: not kept */ }
  sync();
  for (const f of subs) f(p);
}
export function onPrefs(f: (p: Prefs) => void) { subs.push(f); }

// Pages whose previews sit in the page like figures (component pages: [data-live-auto]) have no preview switches:
// the theme follows the site, sounds stay off, and the panel size comes from the room the preview has (720 when the
// component fits at 1.5x on a wide page, else 480). Never stored: the other pages keep the visitor's own prefs.
let autoDev: 720 | 480 | 0 = 0;
function pickDevice(host: HTMLElement): 720 | 480 {
  if (!matchMedia('(min-width: 861px)').matches) return 480;
  const fig = host.querySelector<HTMLElement>('figure[data-live]');
  const fw = fig ? (JSON.parse(fig.dataset.frame ?? '[240,0]') as number[])[0] : 240;
  const cs = getComputedStyle(host);
  const room = host.clientWidth - parseFloat(cs.paddingLeft) - parseFloat(cs.paddingRight);
  return room >= Math.round(fw * 1.5) ? 720 : 480;
}
/** the prefs the live previews draw with: the stored prefs, with a figure page's own theme / sound / size rules */
export function livePrefs(): Prefs {
  const p = getPrefs();
  const host = document.querySelector<HTMLElement>('[data-live-auto]');
  if (!host) return p;
  if (!autoDev) {
    autoDev = pickDevice(host);
    let t = 0;
    addEventListener('resize', () => {
      clearTimeout(t);
      t = window.setTimeout(() => {
        const d = pickDevice(host);
        if (d !== autoDev) { autoDev = d; const q = livePrefs(); for (const f of subs) f(q); }
      }, 250);
    });
  }
  return { ...p, device: autoDev, sound: false };
}

/** the header controls reflect the current prefs */
function sync() {
  const p = getPrefs();
  for (const b of document.querySelectorAll<HTMLButtonElement>('[data-pref]')) {
    const [k, v] = [b.dataset.pref as keyof Prefs, b.dataset.value!];
    b.setAttribute('aria-pressed', String(String(p[k]) === v));
  }
  for (const sel of document.querySelectorAll<HTMLSelectElement>('[data-pref-accent]')) { sel.value = p.accent; sel.dispatchEvent(new Event('fselect:sync')); }
  for (const pk of document.querySelectorAll<HTMLSelectElement>('[data-pref-pack]')) { pk.value = p.pack; pk.dispatchEvent(new Event('fselect:sync')); }
  for (const b of document.querySelectorAll<HTMLButtonElement>('[data-sound-toggle]')) {
    b.setAttribute('aria-pressed', String(p.sound));
    const l = b.querySelector('[data-sound-label]');
    if (l) l.textContent = p.sound ? 'Sound on' : 'Sound off';
  }
  for (const b of document.querySelectorAll<HTMLButtonElement>('[data-theme-toggle]')) {
    b.setAttribute('aria-label', p.theme === 'dark' ? 'Switch to light theme' : 'Switch to dark theme');
  }
}

export function initPrefs() {
  for (const b of document.querySelectorAll<HTMLButtonElement>('[data-theme-toggle]')) {
    b.addEventListener('click', () => setPrefs({ theme: getPrefs().theme === 'dark' ? 'light' : 'dark' }));
  }
  for (const b of document.querySelectorAll<HTMLButtonElement>('[data-sound-toggle]')) {
    b.addEventListener('click', () => setPrefs({ sound: !getPrefs().sound }));
  }
  for (const b of document.querySelectorAll<HTMLButtonElement>('[data-pref]')) {
    b.addEventListener('click', () => {
      const k = b.dataset.pref as keyof Prefs, v = b.dataset.value!;
      setPrefs({ [k]: k === 'device' ? (Number(v) as 720 | 480) : k === 'sound' ? v === 'true' : v } as Partial<Prefs>);
    });
  }
  for (const s of document.querySelectorAll<HTMLSelectElement>('[data-pref-accent]')) s.addEventListener('change', () => setPrefs({ accent: s.value }));
  for (const s of document.querySelectorAll<HTMLSelectElement>('[data-pref-pack]')) s.addEventListener('change', () => setPrefs({ pack: s.value }));
  sync();
  initSettings();
}

/** the header settings popover: open / close (button, Escape, a click outside), focus moves in and back */
function initSettings() {
  const box = document.querySelector<HTMLElement>('[data-settings]');
  if (!box) return;
  const btn = box.querySelector<HTMLButtonElement>('[data-settings-open]')!;
  const panel = box.querySelector<HTMLElement>('.settings__panel')!;
  const set = (open: boolean, focusBack = false) => {
    panel.hidden = !open;
    btn.setAttribute('aria-expanded', String(open));
    if (open) panel.querySelector<HTMLElement>('button, select, input')?.focus();
    else if (focusBack) btn.focus();
  };
  btn.addEventListener('click', () => set(panel.hidden));
  panel.addEventListener('keydown', (e) => { if (e.key === 'Escape') { e.preventDefault(); set(false, true); } });
  document.addEventListener('pointerdown', (e) => { if (!panel.hidden && !box.contains(e.target as Node)) set(false); });
}
