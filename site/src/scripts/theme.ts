type Theme = 'dark' | 'light';

const STORAGE_KEY = 'facet-theme';

function currentTheme(): Theme {
  return document.documentElement.dataset.facetTheme === 'light' ? 'light' : 'dark';
}

function setTheme(theme: Theme): void {
  document.documentElement.dataset.facetTheme = theme;
  try {
    localStorage.setItem(STORAGE_KEY, theme);
  } catch {
    // Storage can be unavailable (private windows, blocked cookies). The toggle still works.
  }
}

export function initTheme(): void {
  for (const button of document.querySelectorAll<HTMLButtonElement>('[data-theme-toggle]')) {
    button.addEventListener('click', () => {
      setTheme(currentTheme() === 'dark' ? 'light' : 'dark');
    });
  }
}
