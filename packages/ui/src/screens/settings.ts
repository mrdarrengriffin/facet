// Settings: every personalisation setting, built from the @facet/settings schema, so the screen, the panel's Home
// Assistant entities, the ESPHome web page and the docs never drift. Opened from the Settings button in Home's header.
// One scrolling column of groups (caps group label, then hairline rows 96 tall): title left, control right.
// Controls by the schema's `control`: segmented (up to 3 options inline), swatches (the accent dots), toggle (80 × 48
// switch), sheet / presets (the value, dim, and a chevron → bottom sheet with radio rows), slider (row 144: title and
// value on top, full-width slider under it). Calm: neutrals; the accent only where a setting is "on" and in the
// swatches themselves.
// Values: `data.settings` (missing = the schema default). A change shows at once and is held for 3 s while the store
// catches up, then the store wins (like the Switch screen).
import { icon } from '@facet/icons';
import {
  format,
  groups,
  inGroup,
  resolve,
  type NumberSetting,
  type Setting,
  type Value,
  type Values,
} from '@facet/settings';
import { fit8 } from '../atoms.js';
import { slider } from '../controls.js';
import { h } from '../dom.js';
import { CW, content, header, layout as G, page, text } from '../layout.js';
import { bottomSheet, pickerRow } from '../pickers.js';
import { accentSwatches, segmented, toggleSwitch } from '../settings.js';
import type { Screen, ScreenContext, ScreenOptions } from './common.js';

/** The icon on a sheet's rows. */
const SHEET_ICON: Record<string, string> = {
  soundPack: 'volume',
  clockFace: 'timer',
  idleAfter: 'timer',
  screenOff: 'power',
};
const HOLD_MS = 3000;

export function settingsScreen(ctx: ScreenContext, o: ScreenOptions = {}): Screen {
  let values: Values = resolve();
  const held = new Map<string, { v: Value; until: number }>();
  const get = (id: string): Value => {
    const hv = held.get(id);
    return hv && Date.now() < hv.until ? hv.v : values[id]!;
  };
  const set = (id: string, v: Value) => {
    held.set(id, { v, until: Date.now() + HOLD_MS });
    ctx.actions?.setSetting?.(id, v);
    render();
  };

  const sheet = bottomSheet({ title: 'setting' });
  let sheetFor: Setting | null = null;
  const renderSheet = () => {
    const s = sheetFor;
    if (!s) return;
    sheet.setTitle(s.label.toLowerCase());
    const v = get(s.id);
    const opts: { v: Value; name: string; status: string }[] =
      s.kind === 'select'
        ? s.options.map((x) => ({ v: x.id, name: x.label, status: x.hint ?? '' }))
        : ((s as NumberSetting).presets ?? []).map((n) => ({
            v: n,
            name: format(s, n),
            status: '',
          }));
    sheet.list.replaceChildren(
      ...opts.map((x) =>
        pickerRow({
          icon: SHEET_ICON[s.id] ?? 'check',
          name: x.name,
          status: x.status,
          selected: x.v === v,
          onTap: () => {
            set(s.id, x.v);
            // a sound pack: stay open, so the next one can be heard straight after
            if (s.id !== 'soundPack') sheet.close();
          },
        }),
      ),
    );
  };

  const updaters: (() => void)[] = [];
  const rowFor = (s: Setting): HTMLElement => {
    const title = text('title', 'fs-set-title', s.label);
    if (s.control === 'slider' && s.kind === 'number') {
      const val = text('body', 'fs-set-val');
      // the slider is 0..100: map it to the setting's min..max, snapped to its step
      const fromPct = (p: number) =>
        Math.round(((p / 100) * (s.max - s.min)) / s.step) * s.step + s.min;
      const toPct = (n: number) => ((n - s.min) / (s.max - s.min)) * 100;
      const sl = slider({
        width: CW - G.gutter,
        value: 0,
        onInput: (p) => (val.textContent = format(s, fromPct(p))),
        onChange: (p) => set(s.id, fromPct(p)),
      });
      updaters.push(() => {
        const n = Number(get(s.id));
        val.textContent = format(s, n);
        sl.set(toPct(n));
      });
      return h(
        'div',
        { class: 'fs-set fs-set--slider', 'data-setting': s.id },
        h('div', { class: 'fs-set-top' }, title, h('span', { class: 'f-grow' }), val),
        sl,
      );
    }
    if (s.control === 'sheet' || s.control === 'presets') {
      const val = text('body', 'fs-set-val');
      updaters.push(() => (val.textContent = format(s, get(s.id))));
      const el = h(
        'div',
        { class: 'fs-set live', role: 'button', 'data-snd': 'open', 'data-setting': s.id },
        title,
        h('span', { class: 'f-grow' }),
        val,
        h('span', { class: 'fs-set-chev' }, icon('chevron', 24)),
      );
      el.addEventListener('click', () => {
        sheetFor = s;
        renderSheet();
        sheet.open();
      });
      return el;
    }
    let ctl: HTMLElement & { set(v: never): void };
    if (s.control === 'toggle')
      ctl = toggleSwitch({ on: !!get(s.id), label: s.label, onToggle: (on) => set(s.id, on) });
    else if (s.control === 'swatches')
      ctl = accentSwatches({ value: String(get(s.id)), onPick: (id) => set(s.id, id) });
    else
      ctl = segmented({
        options: s.kind === 'select' ? s.options : [],
        value: String(get(s.id)),
        onPick: (id) => set(s.id, id),
      });
    updaters.push(() => (ctl.set as (v: Value) => void)(get(s.id)));
    return h(
      'div',
      { class: 'fs-set', 'data-setting': s.id },
      title,
      h('span', { class: 'f-grow' }),
      ctl,
    );
  };

  const list = h(
    'div',
    { class: 'fs-set-list' },
    ...groups.map((g) =>
      h(
        'section',
        { class: 'fs-set-group' },
        fit8(text('label', 'fs-set-glabel', g.label)),
        ...inGroup(g.id).map(rowFor),
      ),
    ),
  );
  const el = page(
    'fs-settings',
    header({ label: 'Settings', onBack: o.onBack }),
    content(list),
    sheet,
  );

  function render() {
    updaters.forEach((f) => f());
    if (sheet.isOpen) renderSheet();
  }
  render();

  return {
    el,
    update(d) {
      values = resolve(d.settings ?? {});
      for (const [id, hv] of held)
        if (values[id] === hv.v || Date.now() >= hv.until) held.delete(id);
      render();
    },
    leave() {
      sheet.close(true);
    },
  };
}
