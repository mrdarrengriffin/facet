// Thermostat: the ring is the hero. The measured temperature sits big in the middle (display 144), the set point is
// the knob on the ring, and the arc between them glows orange while it heats. Drag the ring or tap − / + to change the
// set point; the change goes out when the drag ends (− / + wait 800 ms for more taps), and updates from elsewhere are
// held off for 3 s so the knob doesn't jump back. Under the ring: the modes (Heat, Auto, Off ...) and a Presets
// button that opens a sheet. The schedule's next change is the quiet line in the centre ("17° from 22:30").
import { setState } from '../atoms.js';
import { button, iconButton } from '../buttons.js';
import { thermostatDial } from '../controls.js';
import { h } from '../dom.js';
import { at, CH, CW, content, header, page, text } from '../layout.js';
import { bottomSheet, pickerRow } from '../pickers.js';
import { size as Z } from '@facet/tokens';
import type { HvacMode, ThermostatData } from '../types.js';
import { missing, type Screen, type ScreenContext, type ScreenOptions } from './common.js';

export const HVAC_TEXT: Record<HvacMode, string> = {
  heat: 'Heat',
  cool: 'Cool',
  heat_cool: 'Heat/Cool',
  auto: 'Auto',
  dry: 'Dry',
  fan_only: 'Fan',
  off: 'Off',
};

const PRESET_ICON: Record<string, string> = {
  home: 'person',
  away: 'back',
  eco: 'bolt',
  boost: 'plus',
  sleep: 'timer',
  comfort: 'person',
};

/** "Eco", "Home": HA's preset ids as words. */
export const presetText = (p: string) =>
  p ? p[0]!.toUpperCase() + p.slice(1).replace(/_/g, ' ') : p;

/** A temperature: 19.25 → "19.3°". Set points (`step` given) drop a trailing .0: "17°", "17.5°". */
export function tempText(v: number | null | undefined, step?: number) {
  if (v === null || v === undefined || !Number.isFinite(v)) return '--';
  const r = Math.round(v * 10) / 10;
  if (step === undefined) return `${r.toFixed(1)}°`;
  return `${step >= 1 ? Math.round(v) : Number.isInteger(r) ? r : r.toFixed(1)}°`;
}

/** The centre lines: what it is doing, and the quiet line (the schedule's next change, else the preset). */
export function thermostatSummary(t: ThermostatData, now = Date.now()) {
  const doing =
    t.mode === 'off'
      ? 'off'
      : t.action === 'heating'
        ? 'heating'
        : t.action === 'cooling'
          ? 'cooling'
          : 'idle';
  let quiet = '';
  if (t.mode !== 'off' && t.next && Date.parse(t.next.at) > now) {
    const at = new Date(t.next.at);
    const soon = at.getTime() - now < 20 * 3_600_000;
    const when = at.toLocaleTimeString('en-GB', { hour: '2-digit', minute: '2-digit' });
    quiet = `${tempText(t.next.target, t.step)} from ${soon ? when : `${at.toLocaleDateString('en-GB', { weekday: 'short' })} ${when}`}`;
  } else if (t.preset && t.preset !== 'none') quiet = presetText(t.preset);
  return { doing, quiet };
}

export function thermostatScreen(ctx: ScreenContext, o: ScreenOptions = {}): Screen {
  const A = ctx.actions;
  const S = Z['thermo-scale'];
  let t: ThermostatData | null | undefined = null;
  let holdUntil = 0,
    held: number | null = null;
  let pending: ReturnType<typeof setTimeout> | undefined;

  const hold = (v: number) => {
    holdUntil = Date.now() + 3000;
    held = v;
  };
  const send = (v: number) => {
    clearTimeout(pending);
    hold(v);
    A?.climateTarget?.(v);
  };

  // built on the first data (range and step come from the entity), rebuilt if they change
  let dial: ReturnType<typeof thermostatDial> | null = null;
  let range = '';
  const ring = h('div', { class: 'fs-th-ring' });
  const makeDial = (d: ThermostatData) => {
    dial = thermostatDial({
      min: d.min,
      max: d.max,
      step: d.step,
      target: d.target,
      current: d.current,
      onInput: (v) => {
        hold(v);
        render(true);
      },
      onChange: (v) => {
        send(v);
        render();
      },
    });
    ring.replaceChildren(dial);
    range = `${d.min}/${d.max}/${d.step}`;
  };

  const cap = text('label', 'fs-th-cap', 'loading');
  const val = text('display', 'fs-th-val', '--');
  const setLine = text('title', 'fs-th-set', '');
  const quiet = text('body', 'fs-th-quiet', '');
  const nudge = (dir: 1 | -1) => () => {
    if (!t || target() === null) return;
    const v = Math.min(t.max, Math.max(t.min, +(target()! + dir * t.step).toFixed(2)));
    hold(v);
    render();
    clearTimeout(pending);
    pending = setTimeout(() => send(v), 800);
  };
  const minus = iconButton({ style: 'square', icon: 'minus', title: 'Colder', onTap: nudge(-1) });
  const plus = iconButton({ style: 'square', icon: 'plus', title: 'Warmer', onTap: nudge(1) });
  const mid = h(
    'div',
    { class: 'fs-th-mid' },
    at(0, 104, S, 24, cap),
    at(0, 136, S, 144, val),
    at(0, 288, S, 40, setLine),
    at(0, 328, S, 32, quiet),
    at(S / 2 - 72, 384, 48, 48, minus),
    at(S / 2 + 24, 384, 48, 48, plus),
  );

  // modes on the left, presets on the right
  const modes = h('div', { class: 'fs-th-modes' });
  let modeKey = '';
  const modeBtns = new Map<HvacMode, HTMLElement>();
  const sheet = bottomSheet({ title: 'preset' });
  const presetLbl = (p?: string | null) => (p && p !== 'none' ? presetText(p) : 'Presets');
  const presetBtn = button({
    kind: 'secondary',
    text: 'Presets',
    icon: 'chevron-down',
    width: 208,
    onTap: () => openSheet(),
  });
  const presetSlot = h('div', { class: 'fs-th-preset' }, presetBtn);
  const rows = new Map<string, ReturnType<typeof pickerRow>>();
  const openSheet = () => {
    if (!t?.presets?.length) return;
    rows.clear();
    sheet.list.replaceChildren(
      ...t.presets.map((p) => {
        const r = pickerRow({
          icon: PRESET_ICON[p] ?? 'auto',
          name: presetText(p),
          status: p === t?.preset ? 'in use' : 'tap to use',
          tone: p === t?.preset ? 'working' : 'dim',
          selected: p === t?.preset,
          onTap: () => {
            A?.climatePreset?.(p);
            sheet.close();
          },
        });
        rows.set(p, r);
        return r;
      }),
    );
    sheet.open();
  };
  const buildModes = (d: ThermostatData) => {
    const key = d.modes.join(',');
    if (key === modeKey) return;
    modeKey = key;
    modeBtns.clear();
    const list = d.modes.slice(0, 3);
    modes.replaceChildren(
      ...list.map((m) => {
        const b = button({
          kind: 'secondary',
          text: HVAC_TEXT[m],
          width: 128,
          onTap: () => A?.climateMode?.(m),
        });
        modeBtns.set(m, b);
        return b;
      }),
    );
  };

  const target = () => (Date.now() < holdUntil && held !== null ? held : (t?.target ?? null));

  const el = page(
    'fs-thermo',
    header({ label: 'Thermostat', onBack: o.onBack }),
    content(at((CW - S) / 2, 0, S, S, ring, mid), at(0, CH - 64, CW, 64, modes, presetSlot)),
    sheet,
  );
  const hd = el.querySelector<HTMLElement>('.fs-hlabel .f-fit');

  function render(dragging = false) {
    if (!t) {
      cap.textContent = missing(ctx.store);
      val.textContent = '--';
      setLine.textContent = '';
      quiet.textContent = '';
      minus.hidden = plus.hidden = true;
      modes.replaceChildren();
      modeKey = '';
      presetBtn.hidden = true;
      return;
    }
    const sum = thermostatSummary(t);
    const off = t.mode === 'off' || t.target === null;
    const tg = target();
    el.dataset.doing = sum.doing;
    if (dragging) {
      cap.textContent = 'set to';
      val.textContent = tempText(tg, t.step);
      setLine.textContent = `now ${tempText(t.current)}`;
    } else {
      cap.textContent = sum.doing;
      val.textContent = tempText(t.current);
      setLine.textContent = off ? '' : `set ${tempText(tg, t.step)}`;
    }
    quiet.textContent = sum.quiet;
    minus.hidden = plus.hidden = off;
    for (const [m, b] of modeBtns) setState(b, m === t.mode ? 'active' : 'default');
    presetBtn.hidden = !t.presets?.length;
    presetBtn.querySelector('.f-btn-label')!.textContent = presetLbl(t.preset);
    if (!dial?.dragging)
      dial?.set({
        target: off ? null : tg,
        current: t.current,
        active: t.action === 'heating' ? 'heat' : t.action === 'cooling' ? 'cool' : null,
      });
  }

  return {
    el,
    update(s) {
      t = s.thermostat;
      if (t) {
        if (`${t.min}/${t.max}/${t.step}` !== range) makeDial(t);
        buildModes(t);
        if (hd) hd.textContent = t.name ?? 'Thermostat';
        if (Date.now() >= holdUntil) held = null;
      }
      if (!dial?.dragging) render();
    },
    enter() {
      if (t && dial && Date.now() >= holdUntil) {
        const off = t.mode === 'off' || t.target === null;
        dial.set(
          {
            target: off ? null : t.target,
            current: t.current,
            active: t.action === 'heating' ? 'heat' : t.action === 'cooling' ? 'cool' : null,
          },
          true,
        );
      }
    },
    leave() {
      sheet.close(true);
    },
  };
}
