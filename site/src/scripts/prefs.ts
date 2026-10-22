// Page-level preview switches (header): theme dark / light, device 720 / 480, accent set. Kept per browser
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

/** the header controls reflect the current prefs */
function sync() {
  const p = getPrefs();
  for (const b of document.querySelectorAll<HTMLButtonElement>('[data-pref]')) {
    const [k, v] = [b.dataset.pref as keyof Prefs, b.dataset.value!];
    b.setAttribute('aria-pressed', String(String(p[k]) === v));
  }
  const sel = document.querySelector<HTMLSelectElement>('[data-pref-accent]');
  if (sel) sel.value = p.accent;
  const pk = document.querySelector<HTMLSelectElement>('[data-pref-pack]');
  if (pk) { pk.value = p.pack; pk.disabled = !p.sound; }
}

export function initPrefs() {
  for (const b of document.querySelectorAll<HTMLButtonElement>('[data-pref]')) {
    b.addEventListener('click', () => {
      const k = b.dataset.pref as keyof Prefs, v = b.dataset.value!;
      setPrefs({ [k]: k === 'device' ? (Number(v) as 720 | 480) : k === 'sound' ? v === 'true' : v } as Partial<Prefs>);
    });
  }
  document.querySelector<HTMLSelectElement>('[data-pref-accent]')?.addEventListener('change', (e) => setPrefs({ accent: (e.target as HTMLSelectElement).value }));
  document.querySelector<HTMLSelectElement>('[data-pref-pack]')?.addEventListener('change', (e) => setPrefs({ pack: (e.target as HTMLSelectElement).value }));
  sync();
}
