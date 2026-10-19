// Media: the disc (256) with the track beside it, then the controls and a live volume slider. The header's speaker
// button opens a sheet to pick where media goes: Auto (follow whatever is playing) or one player. In place of the disc
// the screen can show a cassette, CD, vinyl LP or reel-to-reel deck (`visual`, see media-visuals/).
import { setState } from '../atoms.js';
import { iconButton, setGlyph } from '../buttons.js';
import { disc, slider } from '../controls.js';
import { h } from '../dom.js';
import { clean } from '../format.js';
import { at, CW, content, header, layout, page, text } from '../layout.js';
import { mediaVisualSlot, type MediaVisualId } from '../media-visuals/index.js';
import { bottomSheet, pickerRow, speakerButton } from '../pickers.js';
import type { PanelActions, PanelData } from '../types.js';
import {
  mediaPos,
  playerIcon,
  playerStatus,
  speakerName,
  type Screen,
  type ScreenContext,
  type ScreenOptions,
} from './common.js';

/**
 * Live volume: while dragging, send at most every 200 ms plus a trailing send, the final value on release, never the
 * same value twice. Updates from elsewhere are ignored for 2.5 s after the last drag.
 */
export function volumeControl(actions?: PanelActions) {
  let sent = -1,
    last = 0,
    hold = 0;
  let trail: ReturnType<typeof setTimeout> | undefined;
  const send = (v: number) => {
    clearTimeout(trail);
    hold = Date.now() + 2500;
    if (v === sent) return;
    sent = v;
    last = Date.now();
    actions?.mediaVolume?.(v);
  };
  return {
    send,
    input(v: number) {
      hold = Date.now() + 2500;
      const wait = 200 - (Date.now() - last);
      clearTimeout(trail);
      if (wait <= 0) send(v);
      else trail = setTimeout(() => send(v), wait);
    },
    free: () => Date.now() > hold,
  };
}

export interface MediaOptions extends ScreenOptions {
  /** What sits in the hero slot. Default `disc`. */
  visual?: MediaVisualId;
}

export type MediaScreen = Screen & { setVisual(id: MediaVisualId): void };

export function mediaScreen(ctx: ScreenContext, o: MediaOptions = {}): MediaScreen {
  const A = ctx.actions;
  const D = layout.disc;
  const sheet = bottomSheet({ title: 'speaker' });
  const choose = (mode: 'auto' | 'manual', id?: string) => {
    sheet.close();
    A?.setMediaTarget?.(mode, id);
  };
  const autoRow = pickerRow({
    icon: 'auto',
    name: 'Auto',
    status: "follow what's playing",
    onTap: () => choose('auto'),
  });
  const rows = new Map<string, ReturnType<typeof pickerRow>>();
  function renderSheet(s: PanelData) {
    const t = s.mediaTarget,
      auto = !t || t.mode === 'auto';
    autoRow.set({
      selected: auto,
      status: t ? `follow what's playing · now ${t.name}` : "follow what's playing",
    });
    const list = (s.players ?? []).map((p) => {
      let r = rows.get(p.id);
      if (!r) {
        r = pickerRow({
          icon: playerIcon(p),
          name: p.name,
          status: '',
          onTap: () => choose('manual', p.id),
        });
        rows.set(p.id, r);
      }
      r.set({
        name: p.name,
        icon: playerIcon(p),
        status: playerStatus(p),
        tone: p.state === 'playing' ? 'working' : 'dim',
        selected: !auto && t?.id === p.id,
      });
      return r;
    });
    sheet.list.replaceChildren(autoRow, ...list);
  }
  const spk = speakerButton({
    name: 'auto',
    onTap: () => {
      renderSheet(ctx.store.get());
      sheet.open();
    },
  });
  const d = disc({ progress: 0, playing: false, size: D });
  const mv = mediaVisualSlot(d, o.visual); // the disc, or a visual in its place that runs off the left edge
  const state = text('label', 'fs-m-state', 'idle');
  const title = h('div', { class: 'fst-headline fs-m-title' }, 'Nothing playing');
  const artist = text('body', 'fs-m-artist', '');
  const play = iconButton({
    style: 'play',
    icon: 'play',
    title: 'Play / pause',
    onTap: () => A?.mediaPlayPause?.(),
  });
  const controls = h(
    'div',
    { class: 'fs-m-ctl' },
    iconButton({
      style: 'round',
      icon: 'prev',
      title: 'Previous',
      onTap: () => A?.mediaPrevious?.(),
    }),
    play,
    iconButton({ style: 'round', icon: 'next', title: 'Next', onTap: () => A?.mediaNext?.() }),
  );
  const vc = volumeControl(A);
  const vLab = text('label', 'fs-vol-v', '--');
  const vol = slider({
    width: CW - 2 * 72 - 2 * 16,
    value: 0,
    onSweep: (v) => (vLab.textContent = String(v)),
    onInput: (v) => {
      vLab.textContent = String(v);
      vc.input(v);
    },
    onChange: (v) => vc.send(v),
  });
  const col = h('div', { class: 'fs-m-col' }, state, title, artist);
  const el = page(
    'fs-media',
    mv.layer,
    header({ label: 'Media', onBack: o.onBack, right: [spk], clock: false }),
    content(
      at(0, 0, D, D, d),
      at(D + 32, 0, CW - D - 32, D, col),
      at(0, 320, CW, 96, controls),
      at(
        0,
        464,
        CW,
        48,
        h('div', { class: 'fs-vol' }, text('label', 'fs-vol-k', 'vol'), vol, vLab),
      ),
    ),
    sheet,
  );
  let st = ctx.store.get();
  return {
    el,
    setVisual: (id) => mv.setVisual(id),
    enter() {
      mv.enter();
    },
    leave() {
      sheet.close(true);
    },
    update(s) {
      st = s;
      const m = s.media,
        t = s.mediaTarget,
        playing = m?.state === 'playing';
      spk.set({
        name: `${!t || t.mode === 'auto' ? 'auto · ' : ''}${speakerName(t?.name) || 'none'}`,
        playing,
      });
      if (sheet.isOpen) renderSheet(s);
      setState(play, playing ? 'active' : 'default');
      setGlyph(play, playing ? 'pause' : 'play');
      state.textContent = playing ? 'now playing' : m?.state === 'paused' ? 'paused' : 'idle';
      state.dataset.tone = playing ? 'working' : '';
      title.textContent = clean(m?.title) || 'Nothing playing';
      artist.textContent = clean(m?.artist);
      if (!vol.dragging && vc.free() && m?.volume != null) vol.sweep(Math.round(m.volume * 100));
      d.set({ progress: mediaPos(m).frac * 100, playing });
      mv.set(m);
    },
    tick() {
      d.set({ progress: mediaPos(st.media).frac * 100, playing: st.media?.state === 'playing' });
      mv.tick();
    },
  };
}
