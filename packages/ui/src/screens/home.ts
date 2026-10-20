// Home: the room's cards, two by two (320 × 280), four per view; the grid scrolls a row at a time. Each card shows
// one value and one line, and lights its dot when it matters (something needs you, music playing, peak price, a
// nearby strike). The Timer card comes first while a timer runs; otherwise it sits after Clock as "No timer",
// so a timer can always be started. The Weather card is contextual: a nearby storm takes it over, then rain within
// the hour, then the moon at night (`weatherState`). `readouts` = the state readouts (`PanelData.readouts`: no box, no
// screen, tap does nothing), two per card slot, in the card's place.
import { moonIcon, screenIcon, weatherIcon, wxIcon, WX_TEXT } from '@facet/icons';
import { iconButton } from '../buttons.js';
import { BAND_NAME } from '../charts.js';
import { countdown } from '../clocks.js';
import { hhmm, clean, deg, money } from '../format.js';
import { card, content, header, page } from '../layout.js';
import {
  previewAgents,
  previewClock,
  previewDial,
  previewSwitch,
  previewEnergy,
  previewGithub,
  previewLightning,
  previewMedia,
  previewTimer,
} from '../previews.js';
import { h } from '../dom.js';
import type { PanelData } from '../types.js';
import { isDayNow, weatherCardText, weatherState, type WeatherState } from './weather.js';
import {
  calCardLine,
  CAL_SOON,
  calMs,
  isRunning,
  nextAgendaEvent,
  previewCalendar,
} from './calendar.js';
import { switchLine } from './switch.js';
import { overheadCardText, previewOverhead } from './overhead.js';
import { unitsOf } from '../units.js';
import { tempText, thermostatSummary } from './thermostat.js';
import { coverMoving, coverStatus, coverValue, previewCover } from '../covers.js';
import { readoutFrom, readoutSlot } from '../readouts.js';
import { isDay, missing, speakerName, type Screen, type ScreenContext } from './common.js';

export type CardId =
  | 'timer'
  | 'agents'
  | 'media'
  | 'light'
  | 'thermostat'
  | 'covers'
  | 'switch'
  | 'readouts'
  | 'clock'
  | 'github'
  | 'energy'
  | 'weather'
  | 'calendar'
  | 'overhead';
export const CARD_IDS: CardId[] = [
  'timer',
  'calendar',
  'agents',
  'media',
  'light',
  'thermostat',
  'covers',
  'switch',
  'readouts',
  'clock',
  'github',
  'energy',
  'weather',
  'overhead',
];

export interface HomeOptions {
  /** Called when a card is tapped. Leave it out and the cards don't navigate. */
  open?: (id: CardId) => void;
  /** Which cards, in order. Default: all of them (Timer moves to the front while one runs). */
  cards?: CardId[];
  /** Called by the Settings button in the header. Leave it out and there's no button. */
  openSettings?: () => void;
}

/** Home's Settings button: 48 square with the settings screen icon, left of the time. */
export function settingsButton(onTap: () => void) {
  const b = iconButton({ style: 'square', icon: 'auto', title: 'Settings', onTap });
  b.querySelector('.f-icon')?.replaceWith(screenIcon('settings', 24));
  b.dataset.snd = 'open';
  return b;
}

export function homeScreen(ctx: ScreenContext, o: HomeOptions = {}): Screen {
  const go = (id: CardId) => () => o.open?.(id);
  const pDial = previewDial();
  const pEnergy = previewEnergy();
  const pWeather = weatherIcon('partly-day', { size: 72, animated: true });
  const pLightning = previewLightning();
  const pTimer = previewTimer();
  const pMoon = moonIcon(0.25, { size: 72 });
  // the Weather card's three previews in one 72 box; the state shows one
  const box = (el: Element) => h('span', {}, el);
  const bWeather = box(pWeather),
    bMoon = box(pMoon),
    bLightning = box(pLightning);
  const pWx = h('span', { class: 'fs-wx-prev' }, bWeather, bMoon, bLightning);
  const pThermo = previewDial();
  const pCover = previewCover();
  const pCal = previewCalendar();
  const pSwitch = previewSwitch();
  const pOverhead = previewOverhead();
  const cards = {
    calendar: card({
      label: 'Calendar',
      icon: 'calendar',
      preview: pCal,
      text: 'Loading',
      sub: '',
      onTap: go('calendar'),
    }),
    timer: card({
      label: 'Timer',
      icon: 'timer',
      preview: pTimer,
      text: 'No timer',
      sub: 'tap to start one',
      onTap: go('timer'),
    }),
    agents: card({
      label: 'Agents',
      icon: 'agents',
      preview: previewAgents(),
      value: '--',
      sub: '',
      onTap: go('agents'),
    }),
    media: card({
      label: 'Media',
      icon: 'media',
      preview: previewMedia(),
      text: 'Nothing playing',
      sub: '',
      onTap: go('media'),
    }),
    light: card({
      label: 'Light',
      icon: 'light',
      preview: pDial,
      value: '--',
      sub: '',
      onTap: go('light'),
    }),
    thermostat: card({
      label: 'Thermostat',
      icon: 'thermostat',
      preview: pThermo,
      value: '--°',
      sub: 'loading',
      onTap: go('thermostat'),
    }),
    switch: card({
      label: 'Switch',
      icon: 'switch',
      preview: pSwitch,
      value: '--',
      sub: 'loading',
      onTap: go('switch'),
    }),
    covers: card({
      label: 'Blinds',
      icon: 'covers',
      preview: pCover,
      value: '--',
      sub: 'loading',
      onTap: go('covers'),
    }),
    clock: card({
      label: 'Clock',
      icon: 'clock',
      preview: previewClock(),
      value: '--:--',
      sub: '',
      onTap: go('clock'),
    }),
    github: card({
      label: 'GitHub',
      icon: 'github',
      preview: previewGithub(),
      value: '--',
      sub: 'loading',
      onTap: go('github'),
    }),
    energy: card({
      label: 'Energy',
      icon: 'energy',
      preview: pEnergy,
      value: '£--',
      sub: 'loading',
      onTap: go('energy'),
    }),
    overhead: card({
      label: 'Overhead',
      icon: 'overhead',
      preview: pOverhead,
      text: 'Loading',
      sub: '',
      onTap: go('overhead'),
    }),
    weather: card({
      label: 'Weather',
      icon: 'weather',
      preview: pWx,
      value: '--°',
      sub: 'loading',
      onTap: go('weather'),
    }),
  };
  const ids = o.cards ?? CARD_IDS;
  // readouts: an anchor in the list; the slots go after it (their number follows the data)
  const roAnchor = h('span', { hidden: true });
  let roSlots: ReturnType<typeof readoutSlot>[] = [];
  const list = h(
    'div',
    { class: 'fs-cards' },
    ...ids.map((id) => (id === 'readouts' ? roAnchor : cards[id])),
  );
  const setReadouts = (now = Date.now()) => {
    const src = ids.includes('readouts') ? (st.readouts ?? []) : [];
    const n = Math.ceil(src.length / 2);
    if (n !== roSlots.length) {
      roSlots.forEach((s) => s.remove());
      roSlots = Array.from({ length: n }, (_, i) =>
        readoutSlot(src.slice(i * 2, i * 2 + 2).map((x) => readoutFrom(x, now))),
      );
      roAnchor.after(...roSlots);
    }
    src.forEach((x, i) => roSlots[Math.floor(i / 2)]!.items[i % 2]!.set(readoutFrom(x, now)));
  };
  const grid = content(list);
  grid.classList.add('fs-snap');
  const hd = header({
    label: 'Home',
    right: o.openSettings ? [settingsButton(o.openSettings)] : [],
  });
  const el = page('fs-home', hd, grid);
  let st: PanelData = ctx.store.get();

  // calendar: the next event's title, "in 25 min · 16:50"; the dot while it's on or starting within 15 min
  let calMin = -1;
  const setCalendar = (now = Date.now()) => {
    calMin = Math.floor(now / 60_000);
    pCal.set(now);
    const d = st.calendar,
      e = nextAgendaEvent(d, now);
    if (!e)
      return cards.calendar.set({
        text: d
          ? d.error
            ? 'Offline'
            : 'Nothing coming up'
          : missing(ctx.store) === 'loading'
            ? 'Loading'
            : 'No data',
        sub: d && !d.error ? `next ${d.days} days` : '',
        accent: false,
      });
    cards.calendar.set({
      text: e.summary,
      sub: calCardLine(e, now),
      accent: isRunning(e, now) || calMs(e, 'start') - now <= CAL_SOON,
    });
  };
  // weather: storm > rain within the hour > night (the moon) > day; loading / offline
  const setWeather = (now = Date.now()) => {
    const ws: WeatherState = weatherState(st, now);
    cards.weather.dataset.wx = ws;
    bWeather.hidden = ws === 'storm' || ws === 'night';
    bMoon.hidden = ws !== 'night';
    bLightning.hidden = ws !== 'storm';
    const w = st.weather,
      c = w?.current;
    if (ws === 'storm') pLightning.set(st.lightning);
    else if (ws === 'night') pMoon.set(st.moon?.frac ?? 0.25, { south: st.moon?.south });
    else if (ws === 'rain')
      pWeather.set(
        c && /rain|pour|lightning/.test(c.condition)
          ? wxIcon(c.condition, isDayNow(w))
          : isDayNow(w)
            ? 'showers-day'
            : 'showers-night',
      );
    else if (c) pWeather.set(wxIcon(c.condition, isDayNow(w)));
    const t = weatherCardText(ws, st, now);
    cards.weather.set(ws === 'loading' ? { ...t, sub: missing(ctx.store) } : t);
  };

  // the first running timer, as the first card; with none running, "No timer" after Clock (tap = Timer screen)
  const idleAfter = ids.includes('clock') ? cards.clock : null;
  const setTimer = (now = Date.now()) => {
    const t = st.timers?.find((x) => x.endsAt > now);
    if (ids.includes('timer')) {
      if (t && list.firstChild !== cards.timer) list.prepend(cards.timer);
      if (!t && idleAfter && idleAfter.nextSibling !== cards.timer) idleAfter.after(cards.timer);
    }
    if (!t) {
      pTimer.set(0);
      cards.timer.set({ text: 'No timer', sub: 'tap to start one', accent: false });
      return;
    }
    const left = (t.endsAt - now) / 1000;
    const ends = new Date(t.endsAt).toLocaleTimeString('en-GB', {
      hour: '2-digit',
      minute: '2-digit',
    });
    cards.timer.set({
      value: countdown(left),
      sub: `${t.label.toLowerCase()} · ends ${ends}`,
      accent: true,
    });
    pTimer.set((left / t.total) * 100);
  };
  setTimer();

  return {
    el,
    update(s) {
      st = s;
      hd.querySelector('.fs-hlabel .f-fit')!.textContent = s.room ?? 'Home';
      // agents
      const ag = s.agents ?? [];
      const needs = ag.filter((a) => a.kind === 'needs').length,
        working = ag.filter((a) => a.kind === 'working').length;
      cards.agents.set({
        value: String(needs),
        sub: `need you · ${working} working`,
        accent: needs > 0,
      });
      // media
      const m = s.media,
        playing = m?.state === 'playing';
      const title = clean(m?.title);
      cards.media.set({
        text: title || (m && m.state !== 'unavailable' ? 'Nothing playing' : 'Offline'),
        sub: [clean(m?.artist), speakerName(s.mediaTarget?.name)].filter(Boolean).join(' · '),
        accent: playing,
      });
      // light
      const on = !!s.light?.on,
        v = s.light?.brightness ?? 0;
      const name = (s.lightName ?? 'Light').toLowerCase();
      cards.light.set({
        value: s.light ? (on ? `${v}%` : 'Off') : '--',
        sub: on ? `${name} · on` : name,
        accent: false,
      });
      pDial.set(on ? v : 0);
      // thermostat
      const th = s.thermostat;
      if (th) {
        const d = thermostatSummary(th).doing;
        cards.thermostat.set({
          value: tempText(th.current),
          sub:
            d === 'off'
              ? 'off'
              : `${d === 'heating' ? 'heating to' : d === 'cooling' ? 'cooling to' : 'idle · set'} ${tempText(th.target, th.step)}`,
          accent: d === 'heating' || d === 'cooling',
        });
        pThermo.set(
          th.target === null || th.mode === 'off'
            ? 0
            : ((th.target - th.min) / (th.max - th.min)) * 100,
        );
      } else cards.thermostat.set({ value: '--°', sub: missing(ctx.store) });
      // blinds and curtains: the first one, plus how many of the rest are open
      const cv = s.covers ?? [];
      const c0 = cv[0];
      if (c0) {
        const open = cv.filter(
          (c) => c.state !== 'unavailable' && coverStatus(c) !== 'closed',
        ).length;
        cards.covers.set({
          value: coverValue(c0),
          sub:
            cv.length > 1
              ? `${c0.name.toLowerCase()} · ${open} of ${cv.length} open`
              : `${c0.name.toLowerCase()} · ${coverStatus(c0)}`,
          accent: cv.some(coverMoving),
        });
        pCover.set(c0.position ?? (c0.state === 'open' ? 100 : 0));
      } else cards.covers.set({ value: '--', sub: missing(ctx.store) });
      // switch
      const sw = s.switch;
      if (sw)
        cards.switch.set({
          value: sw.on === null ? '--' : sw.on ? 'On' : 'Off',
          sub: switchLine(sw) || (sw.name ?? ''),
          accent: !!sw.on,
        });
      else cards.switch.set({ value: '--', sub: missing(ctx.store) });
      pSwitch.set(sw?.on ?? null);
      // GitHub
      const g = s.github;
      if (g?.counts)
        cards.github.set({
          value: String(g.counts.needs),
          sub: g.counts.needs
            ? `to review · ${g.counts.mine} open`
            : `nothing to review · ${g.counts.mine} open`,
          accent: g.counts.needs > 0,
        });
      else cards.github.set({ value: '--', sub: g?.error ? 'offline' : missing(ctx.store) });
      // energy
      const e = s.energy;
      pEnergy.set(e);
      if (e?.today) {
        const band = e.rate?.band ?? 'normal';
        cards.energy.set({
          value: money(e.today.cost),
          sub: `${e.today.kwh?.toFixed(1) ?? '--'} kWh · ${BAND_NAME[band]} now`,
          accent: band === 'peak',
        });
      } else cards.energy.set({ value: '£--', sub: e?.error ? 'offline' : missing(ctx.store) });
      setWeather();
      setCalendar();
      setTimer();
      setReadouts();
      // overhead: the highest aircraft's route, "32,000 ft · 26 km SE"; the dot while one is 30°+ up
      pOverhead.set(
        s.overhead?.aircraft ?? [],
        s.overhead?.aircraft[0] ?? null,
        unitsOf(s).distance,
      );
      const oh = overheadCardText(s, missing(ctx.store));
      cards.overhead.set({
        text: oh.text,
        sub: oh.sub,
        accent: oh.accent,
        ...(oh.tone ? { tone: oh.tone } : {}),
      });
    },
    tick(now) {
      const c = st.weather?.current;
      cards.clock.set({
        value: hhmm(now),
        sub: c ? `${(WX_TEXT[c.condition] ?? c.condition).toLowerCase()} · ${deg(c.temp)}` : '',
      });
      setWeather(now.getTime());
      setTimer(now.getTime());
      if (Math.floor(now.getTime() / 60_000) !== calMin) {
        setCalendar(now.getTime());
        setReadouts(now.getTime());
      }
    },
  };
}
