// Home: the room's cards, two by two (320 × 280), four per view; the grid scrolls a row at a time. Each card shows
// one value and one line, and lights its dot when it matters (something needs you, music playing, peak price, a
// nearby strike). The Timer card comes first and only shows while a timer is running.
import { moonIcon, weatherIcon, wxIcon, WX_TEXT } from '@facet/icons';
import { BAND_NAME } from '../charts.js';
import { countdown } from '../clocks.js';
import { hhmm, clean, deg, money } from '../format.js';
import { card, content, header, page } from '../layout.js';
import {
  previewAgents,
  previewClock,
  previewDial,
  previewEnergy,
  previewGithub,
  previewLightning,
  previewMedia,
  previewTimer,
} from '../previews.js';
import { lightningSummary } from '../radar.js';
import { h } from '../dom.js';
import type { PanelData } from '../types.js';
import { moonLine } from './moon.js';
import { isDay, missing, speakerName, type Screen, type ScreenContext } from './common.js';

export type CardId =
  | 'timer'
  | 'agents'
  | 'media'
  | 'light'
  | 'clock'
  | 'github'
  | 'energy'
  | 'weather'
  | 'lightning'
  | 'moon';
export const CARD_IDS: CardId[] = [
  'timer',
  'agents',
  'media',
  'light',
  'clock',
  'github',
  'energy',
  'weather',
  'lightning',
  'moon',
];

export interface HomeOptions {
  /** Called when a card is tapped. Leave it out and the cards don't navigate. */
  open?: (id: CardId) => void;
  /** Which cards, in order. Default: all ten (Timer only while one runs). */
  cards?: CardId[];
}

export function homeScreen(ctx: ScreenContext, o: HomeOptions = {}): Screen {
  const go = (id: CardId) => () => o.open?.(id);
  const pDial = previewDial();
  const pEnergy = previewEnergy();
  const pWeather = weatherIcon('partly-day', { size: 72, animated: true });
  const pLightning = previewLightning();
  const pTimer = previewTimer();
  const pMoon = moonIcon(0.25, { size: 72 });
  const cards = {
    timer: card({
      label: 'Timer',
      icon: 'timer',
      preview: pTimer,
      value: '0:00',
      sub: '',
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
    weather: card({
      label: 'Weather',
      icon: 'weather',
      preview: pWeather,
      value: '--°',
      sub: 'loading',
      onTap: go('weather'),
    }),
    lightning: card({
      label: 'Lightning',
      icon: 'lightning',
      preview: pLightning,
      value: '--',
      sub: 'loading',
      onTap: go('lightning'),
    }),
    moon: card({
      label: 'Moon',
      icon: 'moon',
      preview: pMoon,
      value: '--',
      sub: 'loading',
      onTap: go('moon'),
    }),
  };
  const ids = o.cards ?? CARD_IDS;
  const grid = content(h('div', { class: 'fs-cards' }, ...ids.map((id) => cards[id])));
  grid.classList.add('fs-snap');
  const hd = header({ label: 'Home' });
  const el = page('fs-home', hd, grid);
  let st: PanelData = ctx.store.get();

  const setLightning = (d: PanelData['lightning']) => {
    pLightning.set(d);
    const n = d?.nearest;
    cards.lightning.set(
      n
        ? {
            value: `${n.d} km`,
            sub: `${n.dir} · ${d!.counts.m15} in 15 min`,
            accent: lightningSummary(d).active,
          }
        : {
            value: 'Quiet',
            sub: d ? `nothing within ${d.radius} km` : missing(ctx.store),
            accent: false,
          },
    );
  };

  // the first running timer; hidden when none is (the "done" toast covers the end)
  const setTimer = (now = Date.now()) => {
    const t = st.timers?.find((x) => x.endsAt > now);
    cards.timer.hidden = !t;
    if (!t) return;
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
      // weather
      const c = s.weather?.current;
      if (c) {
        pWeather.set(wxIcon(c.condition, isDay(s.weather)));
        const rain = s.weather?.next_hour?.steps?.some((x) => x.mm >= 0.1);
        cards.weather.set({
          value: deg(c.temp),
          sub: rain ? 'rain within the hour' : (WX_TEXT[c.condition] ?? c.condition),
        });
      } else cards.weather.set({ value: '--°', sub: missing(ctx.store) });
      setLightning(s.lightning);
      // moon
      const mo = s.moon;
      if (mo) {
        pMoon.set(mo.frac, { south: mo.south });
        const l = moonLine(mo);
        cards.moon.set({ value: `${Math.round(mo.illumination)}%`, sub: l.name.toLowerCase() });
      } else cards.moon.set({ value: '--', sub: missing(ctx.store) });
      setTimer();
    },
    tick(now) {
      const c = st.weather?.current;
      cards.clock.set({
        value: hhmm(now),
        sub: c ? `${(WX_TEXT[c.condition] ?? c.condition).toLowerCase()} · ${deg(c.temp)}` : '',
      });
      setLightning(st.lightning);
      setTimer(now.getTime());
    },
  };
}
