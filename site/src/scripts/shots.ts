// Screen pictures (components/Shot.astro): each <img data-shot> shows the file for the panel size the visitor picked
// (<html data-device>) and the theme its box shows (the closest [data-preview-scope]'s preview theme, else the site's
// <html data-theme>). Re-picked whenever one of those attributes changes, so the size and theme switches work on
// pictures as on the live demo. A component picture also takes the size of its file (720 = 1.5x the pixels of 480).
type Pic = string | { src: string; w: number; h: number };
type Srcs = Record<string, Record<string, Pic>>;
const root = document.documentElement;

function pick(img: HTMLImageElement) {
  const srcs = JSON.parse(img.dataset.shot ?? '{}') as Srcs;
  const size = root.dataset.device === '480' ? '480' : '720';
  const box = img.closest<HTMLElement>('[data-preview-scope]')?.dataset.previewTheme;
  const theme = box === 'dark' || box === 'light' ? box : root.dataset.theme === 'light' ? 'light' : 'dark';
  const pic = srcs[size]?.[theme];
  if (!pic) return;
  const src = typeof pic === 'string' ? pic : pic.src;
  if (typeof pic !== 'string' && img.getAttribute('width') !== String(pic.w)) { img.width = pic.w; img.height = pic.h; }  // a component: its own size
  if (img.getAttribute('src') !== src) img.src = src;
}
const all = () => document.querySelectorAll<HTMLImageElement>('img[data-shot]').forEach(pick);

export function initShots() {
  all();
  const watch = new MutationObserver(all);
  watch.observe(root, { attributes: true, attributeFilter: ['data-device', 'data-theme'] });
  for (const scope of document.querySelectorAll('[data-preview-scope]')) watch.observe(scope, { attributes: true, attributeFilter: ['data-preview-theme'] });
}
