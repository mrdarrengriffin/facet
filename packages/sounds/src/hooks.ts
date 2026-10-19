/**
 * UI sounds on real interactions, by event delegation, so components don't need to know about sound:
 *
 * - Home cards and "more" rows play Open, back buttons and sheet scrims play Back
 * - the light's ON/OFF button and Play/Pause play Toggle on / Toggle off
 * - other buttons play Tap, disabled ones play Error
 * - dragging the dial or a slider plays Slider tick per detent, pitched by the value
 *
 * Only real taps make a sound: a press that moved more than 8 px, or during which anything scrolled, stays silent.
 * On a panel that maps to LVGL's CLICKED event, which it drops after a scroll.
 */
import { play } from './engine.js';

export interface SoundHookOptions {
  /** Only interactions inside elements matching this selector make sounds. Default `.f-screen, .f-sound-scope`. */
  scope?: string;
}

const MOVE_PX = 8;
/** Elements a tap plays a sound for (Open, Back, Toggle, Tap, or Error when disabled). */
const LIVE = '.live';
/** Elements whose drag plays Slider tick. */
const DRAG = '.f-dial-arc, .f-slider';
/** Pressing a disabled button plays Error. */
const DISABLED = 'button:disabled';

/**
 * Everything the hooks make a sound for. `hasSound(el)` says whether anything inside `el` would make a sound, e.g.
 * to show a sound switch only next to components that use it.
 */
export const SOUND_SELECTOR = `${LIVE}, ${DRAG}, ${DISABLED}`;
export const hasSound = (root: Element): boolean =>
  root.matches(SOUND_SELECTOR) || root.querySelector(SOUND_SELECTOR) !== null;

/** Install the hooks on a document (or any element). Returns a function that removes them. */
export function installSoundHooks(
  root: Document | HTMLElement = document,
  options: SoundHookOptions = {},
): () => void {
  const SCOPE = options.scope ?? '.f-screen, .f-sound-scope';
  const last = new WeakMap<Element, number>();
  let downAt: { x: number; y: number } | null = null;
  let moved = false;

  const valueOf = (el: Element): number | null => {
    if (el.matches('.f-dial-arc')) {
      const t = el.closest('.f-dial')?.querySelector('.f-dial-value')?.textContent;
      return t != null && t !== '' ? Number(t) : null;
    }
    const left = el.querySelector<HTMLElement>('.f-slider-knob')?.style.left;
    return left ? parseFloat(left) : null;
  };
  const stepOf = (el: Element) => (el.matches('.f-dial-arc') ? 2 : 5); // dial: 51 ticks = one per 2 %; slider: every 5 %

  const onDown = (e: Event) => {
    const pe = e as PointerEvent;
    downAt = { x: pe.clientX, y: pe.clientY };
    moved = false;
    const t = pe.target as Element;
    if (t.closest?.(SCOPE) && t.closest(DISABLED)) void play('error');
    // remember where a dial or slider was, so a tap on the track ticks once
    const el = t.closest?.(DRAG);
    const v = el ? valueOf(el) : null;
    if (el && v != null) last.set(el, Math.round(v / stepOf(el)));
  };
  const onMove = (e: Event) => {
    const pe = e as PointerEvent;
    if (downAt && !moved && Math.hypot(pe.clientX - downAt.x, pe.clientY - downAt.y) > MOVE_PX)
      moved = true;
  };
  const onScroll = () => {
    if (downAt) moved = true;
  };
  const onClick = (e: Event) => {
    const t = e.target as Element;
    if (!t.closest?.(SCOPE)) return;
    const wasMoved = moved;
    downAt = null;
    moved = false;
    if (wasMoved && !t.closest(DRAG)) return; // a scroll or drag, not a tap
    const el = t.closest(LIVE);
    if (!el) return;
    if ((el as HTMLElement).dataset.state === 'disabled') return void play('error');
    if (el.matches('.f-back, [data-snd="back"]')) return void play('back');
    if (el.matches('[data-snd="open"], .fs-card')) return void play('open');
    if (el.closest('.f-dial-toggle, [data-snd="toggle"]'))
      return void play(el.textContent?.trim().toUpperCase() === 'ON' ? 'toggle-off' : 'toggle-on');
    if (el.matches('.f-ibtn--play'))
      return void play(
        el.querySelector('[data-icon]')?.getAttribute('data-icon') === 'pause'
          ? 'toggle-off'
          : 'toggle-on',
      );
    void play('tap');
  };
  // drags: after the component handled the event (bubble phase), the DOM shows the new value
  const onDrag = (e: Event) => {
    const pe = e as PointerEvent;
    if (pe.type === 'pointermove' && !pe.buttons) return;
    const el = (pe.target as Element).closest?.(DRAG);
    if (!el || !el.closest(SCOPE)) return;
    const v = valueOf(el);
    if (v == null || Number.isNaN(v)) return;
    const det = Math.round(v / stepOf(el));
    const prev = last.get(el);
    last.set(el, det);
    if (prev !== undefined && prev !== det) void play('slider-tick', { value: v });
  };

  root.addEventListener('pointerdown', onDown, true);
  root.addEventListener('pointermove', onMove, true);
  root.addEventListener('scroll', onScroll, true);
  root.addEventListener('click', onClick, true);
  root.addEventListener('pointerdown', onDrag);
  root.addEventListener('pointermove', onDrag);
  return () => {
    root.removeEventListener('pointerdown', onDown, true);
    root.removeEventListener('pointermove', onMove, true);
    root.removeEventListener('scroll', onScroll, true);
    root.removeEventListener('click', onClick, true);
    root.removeEventListener('pointerdown', onDrag);
    root.removeEventListener('pointermove', onDrag);
  };
}
