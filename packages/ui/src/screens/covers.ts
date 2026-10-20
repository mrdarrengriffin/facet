// Covers (blinds and curtains): the window on the left is the hero, drawn for the kind of cover (roller, Venetian,
// curtains) and dragged to a position: down on a blind, sideways on curtains. The position goes out when the drag ends
// and updates are held off for 3 s; while the cover moves, a line marks where it is heading. On the right: the state,
// the position, a tilt slider for Venetian blinds and Open / Stop / Close. With more than one cover, an "N windows"
// button in the header opens a sheet to pick one.
import { fit8, live, setState, setText } from '../atoms.js';
import { button } from '../buttons.js';
import { slider } from '../controls.js';
import {
  coverCaps,
  coverKind,
  coverMoving,
  coverPos,
  coverStatus,
  coverValue,
  coverVisual,
  COVER_KIND_TEXT,
} from '../covers.js';
import { h } from '../dom.js';
import { at, CH, CW, content, header, page, text } from '../layout.js';
import { bottomSheet, pickerRow } from '../pickers.js';
import type { CoverData } from '../types.js';
import { missing, type Screen, type ScreenContext, type ScreenOptions } from './common.js';

export interface CoversOptions extends ScreenOptions {
  /** The cover to show first (its id). Default: the first one. */
  cover?: string;
}

const VW = 320; // the window
const RX = VW + 32,
  RW = CW - RX; // the column on the right (304)

export function coversScreen(ctx: ScreenContext, o: CoversOptions = {}): Screen {
  const A = ctx.actions;
  let list: CoverData[] = [];
  let sel = o.cover ?? '';
  let c: CoverData | undefined;
  let holdUntil = 0,
    held: number | null = null,
    heading: number | null = null;
  let tiltHoldUntil = 0;

  // the window, rebuilt when the cover (or its kind) changes
  const win = h('div', { class: 'fs-cv-win' });
  let vis: ReturnType<typeof coverVisual> | null = null;
  let visKey = '';
  const makeVisual = (d: CoverData) => {
    const kind = coverKind(d),
      key = `${d.id}/${kind}/${coverCaps(d).position}`;
    if (key === visKey) return;
    visKey = key;
    heading = null;
    vis = coverVisual({
      kind,
      width: VW,
      height: CH,
      position: coverPos(d),
      tilt: d.tilt ?? 50,
      draggable: coverCaps(d).position,
      onInput: (v) => {
        held = v;
        holdUntil = Date.now() + 3000;
        render(true);
      },
      onChange: (v) => {
        held = v;
        holdUntil = Date.now() + 3000;
        heading = v;
        A?.coverPosition?.(d.id, v);
        render();
      },
    });
    win.replaceChildren(vis);
  };

  const cap = text('label', 'fs-cv-cap', 'loading');
  const val = text('hero', 'fs-cv-val', '--');
  const kindLine = text('body', 'fs-cv-kind', '');

  const tiltCap = text('label', 'fs-cv-tcap', 'slats');
  const tiltVal = text('label', 'fs-cv-tval', '');
  const tilt = slider({
    width: RW,
    value: 50,
    onInput: (v) => {
      tiltHoldUntil = Date.now() + 3000;
      tiltVal.textContent = `${v}%`;
      vis?.set({ tilt: v }, true);
    },
    onChange: (v) => {
      tiltHoldUntil = Date.now() + 3000;
      if (c) A?.coverTilt?.(c.id, v);
    },
  });
  const tiltBox = h(
    'div',
    { class: 'fs-cv-tilt' },
    at(0, 0, RW, 24, tiltCap, tiltVal),
    at(0, 32, RW, 48, tilt),
  );

  const cmd = (command: 'open' | 'close' | 'stop') => () => {
    if (!c) return;
    held = null;
    holdUntil = Date.now() + 3000;
    heading = command === 'open' ? 100 : command === 'close' ? 0 : null;
    A?.coverCommand?.(c.id, command);
  };
  const bOpen = button({ kind: 'secondary', text: 'Open', width: RW, onTap: cmd('open') });
  const bStop = button({ kind: 'secondary', text: 'Stop', width: RW, onTap: cmd('stop') });
  const bClose = button({ kind: 'secondary', text: 'Close', width: RW, onTap: cmd('close') });
  const btns = h('div', { class: 'fs-cv-btns' }, bOpen, bStop, bClose);

  // the picker, when there's more than one
  const sheet = bottomSheet({ title: 'windows' });
  const pick = fit8(
    h('button', { class: 'fst-label fs-cv-pick', type: 'button', 'data-snd': 'open' }, 'windows'),
  );
  live(pick, () => openSheet());
  const openSheet = () => {
    sheet.list.replaceChildren(
      ...list.map((d) =>
        pickerRow({
          icon: coverKind(d),
          name: d.name,
          status: coverStatus(d),
          tone: coverMoving(d) ? 'working' : 'dim',
          selected: d.id === c?.id,
          onTap: () => {
            sel = d.id;
            sheet.close();
            update();
          },
        }),
      ),
    );
    sheet.open();
  };

  const hd = header({ label: 'Blinds', onBack: o.onBack, right: [pick] });
  const hdLabel = hd.querySelector<HTMLElement>('.fs-hlabel .f-fit');
  const el = page(
    'fs-covers',
    hd,
    content(
      at(0, 0, VW, CH, win),
      at(RX, 0, RW, 24, cap),
      at(RX, 32, RW, 96, val),
      at(RX, 136, RW, 32, kindLine),
      at(RX, 232, RW, 80, tiltBox),
      at(RX, CH - 208, RW, 208, btns),
    ),
    sheet,
  );

  const pos = () => (Date.now() < holdUntil && held !== null ? held : c ? coverPos(c) : 0);

  function render(dragging = false) {
    if (!c) {
      cap.textContent = missing(ctx.store);
      val.textContent = '--';
      kindLine.textContent = '';
      tiltBox.hidden = true;
      btns.hidden = true;
      return;
    }
    const caps = coverCaps(c),
      kind = coverKind(c),
      off = c.state === 'unavailable';
    el.dataset.state = c.state;
    btns.hidden = false;
    if (dragging) {
      cap.textContent = 'set to';
      val.textContent = coverValue({ ...c, position: held, state: 'open' });
    } else {
      cap.textContent = coverStatus(c);
      val.textContent = coverValue(c);
    }
    kindLine.textContent = COVER_KIND_TEXT[kind];
    // tilt: Venetian blinds that can tilt to a position
    tiltBox.hidden = !(kind === 'venetian' && caps.tilt);
    if (!tiltBox.hidden && Date.now() >= tiltHoldUntil && !tilt.dragging) {
      tilt.sweep(c.tilt ?? 50);
      tiltVal.textContent = c.tilt === null || c.tilt === undefined ? '--' : `${c.tilt}%`;
    }
    bOpen.hidden = !caps.open;
    bClose.hidden = !caps.close;
    bStop.hidden = !caps.stop;
    const p = c.position;
    setState(bOpen, off || (!coverMoving(c) && p !== null && p >= 100) ? 'disabled' : 'default');
    setState(bClose, off || (!coverMoving(c) && p !== null && p <= 0) ? 'disabled' : 'default');
    setState(bStop, coverMoving(c) ? 'default' : 'disabled');
    if (!coverMoving(c) && Date.now() >= holdUntil) heading = null; // 3 s for the motor to start
    if (vis && !vis.dragging) {
      vis.set({ position: pos(), ...(Date.now() >= tiltHoldUntil ? { tilt: c.tilt ?? 50 } : {}) });
      vis.setTarget(coverMoving(c) ? heading : null);
    }
  }

  function update() {
    list = ctx.store.get().covers ?? [];
    c = list.find((d) => d.id === sel) ?? list[0];
    pick.hidden = list.length < 2;
    setText(pick, `${list.length} windows`);
    if (hdLabel) hdLabel.textContent = c?.name ?? 'Blinds';
    if (c) makeVisual(c);
    else {
      win.replaceChildren();
      vis = null;
      visKey = '';
    }
    // once the motor runs, draw where the cover really is (the target line shows where it's going)
    if (coverMoving(c) && heading !== null) holdUntil = 0;
    if (Date.now() >= holdUntil) held = null;
    if (!vis?.dragging) render();
  }

  return {
    el,
    update,
    enter() {
      if (c && vis && Date.now() >= holdUntil)
        vis.set({ position: coverPos(c), tilt: c.tilt ?? 50 }, true);
    },
    leave() {
      sheet.close(true);
    },
  };
}
