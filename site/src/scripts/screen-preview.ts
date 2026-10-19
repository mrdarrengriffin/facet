import { createMockHouse, TOASTS } from '@facet/demo-data';
import {
  mountScreen,
  toastLayer,
  type ClockFace,
  type ClockProps,
  type MediaScreen,
  type MediaVisualId,
  type ScreenId,
} from '@facet/ui';
import { controlEl, type Props } from './controls';
import { fitFrame, initSounds, initThemeOverride, soundToggleFor } from './facet-runtime';
import { screenOptions, type ScreenOptionsContext } from './screen-options';

/** Mount every `[data-preview]` on the page, with its playground options underneath. */
export function initScreenPreviews(): void {
  initSounds();
  for (const root of document.querySelectorAll<HTMLElement>('[data-preview]')) {
    if (root.dataset.mounted) continue;
    root.dataset.mounted = '1';
    const id = root.dataset.preview!;
    const viewport = root.querySelector<HTMLElement>('[data-viewport]')!;
    const frame = root.querySelector<HTMLElement>('[data-frame]')!;
    const opts = screenOptions[id];
    let house = createMockHouse();
    let current: ReturnType<typeof mountScreen> | null = null;
    let layer: ReturnType<typeof toastLayer> | null = null;
    let clock: { face?: ClockFace; props?: Partial<ClockProps> } | undefined;
    let mediaVisual: MediaVisualId = 'disc';

    const mount = () => {
      current?.destroy();
      current?.el.remove();
      current = mountScreen(id === 'toasts' ? 'home' : (id as ScreenId), house, {
        clock,
        mediaVisual,
      });
      if (layer) current.el.append(layer);
      frame.replaceChildren(current.el);
    };
    if (id === 'toasts') layer = toastLayer();
    const ctx: ScreenOptionsContext = {
      get house() {
        return house;
      },
      remount(o) {
        clock = {
          face: o.face,
          props: {
            font: o.font as ClockProps['font'],
            format: o.format as ClockProps['format'],
            date: o.date as ClockProps['date'],
          },
        };
        mount();
      },
      mediaVisual(v) {
        mediaVisual = v;
        (current?.screen as MediaScreen | undefined)?.setVisual?.(v);
      },
      toasts: layer ?? undefined,
    };
    mount();
    if (layer) setTimeout(() => layer!.push(TOASTS[0]!), 600);

    // ---- options
    const box = root.querySelector<HTMLElement>('[data-options]');
    if (box && opts) {
      const fields = box.querySelector<HTMLElement>('[data-fields]')!;
      const buttons = box.querySelector<HTMLElement>('[data-buttons]')!;
      let props: Props = {};
      const render = () => {
        props = Object.fromEntries(opts.controls.map((c) => [c.key, c.value]));
        fields.replaceChildren(
          ...opts.controls.map((c) =>
            controlEl(
              c,
              props[c.key]!,
              (v) => {
                props[c.key] = v;
                opts.apply(c.key, props, ctx);
              },
              `pv-${id}`,
            ),
          ),
        );
      };
      render();
      buttons.replaceChildren(
        ...(opts.buttons ?? []).map((b) => {
          const btn = document.createElement('button');
          btn.type = 'button';
          btn.className = 'chip-button';
          btn.textContent = b.label;
          btn.addEventListener('click', () => b.run(ctx));
          return btn;
        }),
      );
      buttons.hidden = !opts.buttons?.length;
      box.querySelector('[data-reset]')?.addEventListener('click', () => {
        house.destroy();
        house = createMockHouse();
        clock = undefined;
        mediaVisual = 'disc';
        layer?.clear();
        layer?.setPosition('top');
        mount();
        render();
      });
      box.hidden = false;
    }

    fitFrame(viewport, frame);
    initThemeOverride(root, frame);
    soundToggleFor(root, frame);
  }
}
