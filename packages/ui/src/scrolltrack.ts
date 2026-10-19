// Scroll track: a 4 px bar in the margin to the right of a scrollable area (.f-scroll), the area's height; the thumb
// shows how much is in view and where. Hidden when nothing scrolls. On the panel this is a styled LVGL scrollbar.
import { h } from './dom.js';

/** Give every `.f-scroll` inside `root` a track. `page()` calls it for every screen. */
export function addScrollTracks(root: HTMLElement): void {
  root.querySelectorAll<HTMLElement>('.f-scroll').forEach((sc) => {
    if (sc.closest('[class*="bsheet"]')) return; // sheets are short lists
    const thumb = h('div', { class: 'f-track-thumb' });
    const track = h('div', { class: 'f-track', 'aria-hidden': 'true' }, thumb);
    track.hidden = true;
    sc.after(track);
    const sync = () => {
      const { scrollTop: t, scrollHeight: sh, clientHeight: ch } = sc;
      track.hidden = !ch || sh <= ch + 1;
      if (track.hidden) return;
      // centred in the margin between the area's right edge and the page's right edge (32 px on v3 screens)
      Object.assign(track.style, {
        left: `${Math.round((sc.offsetLeft + sc.offsetWidth + ((sc.offsetParent as HTMLElement | null) ?? sc).clientWidth) / 2) - 2}px`,
        top: `${sc.offsetTop}px`,
        height: `${ch}px`,
      });
      Object.assign(thumb.style, { height: `${(ch / sh) * 100}%`, top: `${(t / sh) * 100}%` });
    };
    sc.addEventListener('scroll', sync, { passive: true });
    if (typeof ResizeObserver === 'undefined') return;
    const ro = new ResizeObserver(sync);
    const watch = () => [...sc.children].forEach((c) => ro.observe(c));
    ro.observe(sc);
    watch();
    new MutationObserver(() => {
      watch();
      sync();
    }).observe(sc, { childList: true, subtree: true });
    requestAnimationFrame(sync);
  });
}
