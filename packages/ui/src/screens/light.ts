// Light: the dial, centred in the content box. Dragging sends brightness live (at most every 200 ms, plus the final
// value), and updates from elsewhere are held off for 3 s after a drag so the dial doesn't jump back.
import { dial } from '../controls.js';
import { size as Z } from '@facet/tokens';
import { at, CH, header, layout, page } from '../layout.js';
import type { Screen, ScreenContext, ScreenOptions } from './common.js';

export function lightScreen(ctx: ScreenContext, o: ScreenOptions = {}): Screen {
  const A = ctx.actions;
  let holdUntil = 0,
    held = 0,
    lastSent = -1,
    lastAt = 0;
  let trail: ReturnType<typeof setTimeout> | undefined;
  const send = (v: number) => {
    clearTimeout(trail);
    holdUntil = Date.now() + 3000;
    held = v;
    if (v === lastSent) return;
    lastSent = v;
    lastAt = Date.now();
    A?.lightBrightness?.(v);
  };
  const dl = dial({
    value: 0,
    on: false,
    onInput: (v) => {
      holdUntil = Date.now() + 3000;
      held = v;
      const wait = 200 - (Date.now() - lastAt);
      clearTimeout(trail);
      if (wait <= 0) send(v);
      else trail = setTimeout(() => send(v), wait);
    },
    onChange: (v) => send(v),
    onToggle: () => {
      holdUntil = 0;
      lastSent = -1;
      A?.lightToggle?.();
    },
  });
  const S = Z['dial-scale'];
  const hd = header({ label: 'Light', onBack: o.onBack });
  const cur = () => {
    const l = ctx.store.get().light;
    return { value: l?.on ? l.brightness : 0, on: !!l?.on };
  };
  return {
    el: page('fs-dial', hd, at((720 - S) / 2, layout.top + (CH - S) / 2, S, S, dl)),
    update(s) {
      hd.querySelector('.fs-hlabel .f-fit')!.textContent = s.lightName ?? 'Light';
      if (Date.now() < holdUntil) return dl.set({ value: held, on: held > 0 });
      dl.set({ value: s.light?.on ? s.light.brightness : 0, on: !!s.light?.on });
    },
    enter() {
      if (Date.now() >= holdUntil) dl.set(cur(), true);
    },
  };
}
