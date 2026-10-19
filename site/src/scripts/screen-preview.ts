import { createMockHouse, TOASTS } from '@facet/demo-data';
import { mountScreen, toastLayer, type ClockFace, type ScreenId } from '@facet/ui';
import { fitFrame, initSounds, initThemeOverride } from './facet-runtime';

/** Mount every `[data-preview]` on the page. */
export function initScreenPreviews(): void {
  initSounds();
  for (const root of document.querySelectorAll<HTMLElement>('[data-preview]')) {
    if (root.dataset.mounted) continue;
    root.dataset.mounted = '1';
    const id = root.dataset.preview!;
    const viewport = root.querySelector<HTMLElement>('[data-viewport]')!;
    const frame = root.querySelector<HTMLElement>('[data-frame]')!;
    const house = createMockHouse();
    let current: ReturnType<typeof mountScreen> | null = null;
    const mount = (face?: ClockFace) => {
      current?.destroy();
      current?.el.remove();
      current = mountScreen(id === 'toasts' ? 'home' : (id as ScreenId), house, {
        clock: face ? { face } : undefined,
      });
      frame.replaceChildren(current.el);
    };
    mount();
    if (id === 'toasts') {
      const layer = toastLayer();
      current!.el.append(layer);
      for (const b of root.querySelectorAll<HTMLButtonElement>('[data-toast]')) {
        b.addEventListener('click', () => {
          const t = TOASTS.find((x) => x.key === b.dataset.toast);
          if (t) layer.push(t);
        });
      }
      setTimeout(() => layer.push(TOASTS[0]!), 600);
    }
    for (const b of root.querySelectorAll<HTMLButtonElement>('[data-face]')) {
      b.addEventListener('click', () => {
        mount(b.dataset.face as ClockFace);
        for (const o of root.querySelectorAll('[data-face]'))
          o.setAttribute('aria-pressed', String(o === b));
      });
    }
    fitFrame(viewport, frame);
    initThemeOverride(root, frame);
  }
}
