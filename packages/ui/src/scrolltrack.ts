// Scroll track: a 4 px bar on the right edge of a scrollable area (.f-scroll, inside its padding), the area's height; the thumb
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
      if (ch && !sc.classList.contains('f-has-track'))
        // full-width positioned children (at(0, y, CW, ...)) narrow by the gutter too while the track shows
        for (const c of sc.children)
          if (
            c instanceof HTMLElement &&
            c.classList.contains('f-abs') &&
            c.offsetLeft === 0 &&
            c.offsetWidth >= sc.clientWidth
          )
            c.classList.add('f-full');
      const on = !!ch && sh > ch + 1;
      track.hidden = !on;
      // the track lives inside the area: it reserves one gutter on the right (4 px bar + 12 px gap), so the margin
      // to the screen edge stays the same as on the left
      sc.classList.toggle('f-has-track', on);
      if (!on) return;
      // flush with the area's right edge
      Object.assign(track.style, {
        left: `${sc.offsetLeft + sc.offsetWidth - 4}px`,
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
