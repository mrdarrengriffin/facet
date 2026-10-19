// Pickers: radio, picker row, bottom sheet and the speaker button (the Media screen's speaker picker).
import { icon } from '@facet/icons';
import { motion, size as Z } from '@facet/tokens';
import { fit8, label, live, setState, setText } from './atoms.js';
import { iconButton } from './buttons.js';
import { h } from './dom.js';
import { listRow } from './lists.js';
import type { Tone } from './types.js';

/** A 24 px radio: a 2 px ring; selected = orange ring and an 8 px dot. `set(on)`. */
export function radio(on = false) {
  const el = h('span', { class: 'f-radio', 'data-on': String(on) }, h('i'));
  return Object.assign(el, {
    set(v: boolean) {
      el.dataset.on = String(v);
    },
  });
}

export interface PickerRowProps {
  icon: string;
  name: string;
  status: string;
  /** Colours the icon, e.g. `working` while a speaker plays. */
  tone?: Tone;
  selected?: boolean;
  onTap?: (e: Event) => void;
}

/** A 72 px picker row: icon, name over status, and a radio. Selected = orange border. */
export function pickerRow(o: PickerRowProps) {
  const ic = h(
    'span',
    { class: 'f-prow-icon', 'data-tone': o.tone ?? 'dim' },
    icon(o.icon, Z.icon),
  );
  const name = h('span', { class: 'f-prow-name ft-tile' }, o.name);
  const status = h('span', { class: 'f-prow-status ft-meta' }, o.status);
  const r = radio(!!o.selected);
  const el = listRow({
    cls: 'f-prow',
    height: Z['sheet-row-h'],
    onTap: o.onTap,
    kids: [ic, h('span', { class: 'f-prow-text' }, name, status), r],
  });
  el.dataset.selected = String(!!o.selected);
  return Object.assign(el, {
    set(st: { name?: string; status?: string; tone?: Tone; selected?: boolean; icon?: string }) {
      if (st.name !== undefined) name.textContent = st.name;
      if (st.status !== undefined) status.textContent = st.status;
      if (st.tone) ic.dataset.tone = st.tone;
      if (st.icon && ic.firstElementChild?.getAttribute('data-icon') !== st.icon)
        ic.replaceChildren(icon(st.icon, Z.icon));
      if (st.selected !== undefined) {
        r.set(st.selected);
        el.dataset.selected = String(st.selected);
      }
    },
  });
}

/**
 * A bottom sheet over a screen: a scrim (tap to close) and a panel from y 96 to the bottom, with a caps title, a
 * Close button and a scrolling list (`sheet.list`). Opens with a 16 px rise and fade.
 */
export function bottomSheet(o: { title: string; onClose?: () => void }) {
  const D = motion.duration,
    ease = motion.easing['ease-out'].css;
  const list = h('div', { class: 'f-bsheet-list f-scroll' });
  const ttl = fit8(label(o.title, 'caps-sm', 'f-bsheet-title'));
  const close = iconButton({
    style: 'square',
    icon: 'close',
    title: 'Close',
    onTap: () => api.close(),
  });
  close.dataset.snd = 'back';
  const panel = h(
    'div',
    { class: 'f-bsheet-panel' },
    h('div', { class: 'f-bsheet-head' }, ttl, h('span', { class: 'f-grow' }), close),
    list,
  );
  const scrim = h('div', { class: 'f-bsheet-scrim live', 'data-snd': 'back' });
  scrim.addEventListener('click', () => api.close());
  const el = h('div', { class: 'f-bsheet', hidden: true }, scrim, panel);
  let isOpen = false;
  const api = Object.assign(el, {
    list,
    get isOpen() {
      return isOpen;
    },
    setTitle(t: string) {
      setText(ttl, t);
    },
    open() {
      if (isOpen) return;
      isOpen = true;
      el.hidden = false;
      scrim.animate([{ opacity: 0 }, { opacity: 1 }], { duration: D['sheet-in'], easing: ease });
      panel.animate(
        [
          { opacity: 0, transform: 'translateY(16px)' },
          { opacity: 1, transform: 'translateY(0)' },
        ],
        { duration: D['sheet-in'], easing: ease },
      );
    },
    close(instant = false) {
      if (!isOpen) return;
      isOpen = false;
      o.onClose?.();
      if (instant) {
        el.hidden = true;
        return;
      }
      scrim.animate([{ opacity: 1 }, { opacity: 0 }], {
        duration: D['sheet-out'],
        easing: ease,
        fill: 'forwards',
      });
      panel
        .animate(
          [
            { opacity: 1, transform: 'translateY(0)' },
            { opacity: 0, transform: 'translateY(16px)' },
          ],
          { duration: D['sheet-out'], easing: ease, fill: 'forwards' },
        )
        .finished.then(
          () => {
            if (!isOpen) {
              el.hidden = true;
              el.getAnimations({ subtree: true }).forEach((a) => a.cancel());
            }
          },
          () => {},
        );
    },
  });
  return api;
}

/** The speaker button for a header (48 tall): speaker icon, caps name and a chevron. Orange icon while playing. */
export function speakerButton(o: { name: string; playing?: boolean; onTap?: (e: Event) => void }) {
  const txt = fit8(h('span', { class: 'f-spk-name ft-caps-sm' }, o.name));
  const ic = h(
    'span',
    { class: 'f-spk-icon', 'data-tone': o.playing ? 'working' : 'dim' },
    icon('speaker', Z['speaker-btn-icon']),
  );
  const b = h(
    'button',
    { class: 'f-spk', type: 'button', 'aria-label': 'Choose speaker', 'data-snd': 'open' },
    ic,
    txt,
    icon('chevron-down', Z['speaker-btn-icon'], 'f-spk-chev'),
  );
  setState(live(b, o.onTap), 'default');
  return Object.assign(b, {
    set(st: { name?: string; playing?: boolean }) {
      if (st.name !== undefined) setText(txt, st.name);
      if (st.playing !== undefined) ic.dataset.tone = st.playing ? 'working' : 'dim';
    },
  });
}
