// Home: the room's cards, two by two (320 × 280), four per view; the grid scrolls a row at a time. Each card shows
// one value and one line, and lights its dot when it matters (something needs you, music playing, peak price, a
// nearby strike).
import { weatherIcon, wxIcon, WX_TEXT } from '@facet/icons';
import { BAND_NAME } from '../charts.js';
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
} from '../previews.js';
import { lightningSummary } from '../radar.js';
import { h } from '../dom.js';
import type { PanelData } from '../types.js';
import { isDay, speakerName, type Screen, type ScreenContext } from './common.js';

export type CardId =
  'agents' | 'media' | 'light' | 'clock' | 'github' | 'energy' | 'weather' | 'lightning';
export const CARD_IDS: CardId[] = [
  'agents',
  'media',
  'light',
  'clock',
  'github',
  'energy',
  'weather',
  'lightning',
];

export interface HomeOptions {
  /** Called when a card is tapped. Leave it out and the cards don't navigate. */
  open?: (id: CardId) => void;
  /** Which cards, in order. Default: all eight. */
  cards?: CardId[];
}

export function homeScreen(ctx: ScreenContext, o: HomeOptions = {}): Screen {
  const go = (id: CardId) => () => o.open?.(id);
  const pDial = previewDial();
  const pEnergy = previewEnergy();
  const pWeather = weatherIcon('partly-day', { size: 72, animated: true });
  const pLightning = previewLightning();
  const cards = {
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
        : { value: 'Quiet', sub: d ? `nothing within ${d.radius} km` : 'no data', accent: false },
    );
  };

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
      else cards.github.set({ value: '--', sub: g?.error ? 'offline' : 'no data' });
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
      } else cards.energy.set({ value: '£--', sub: e?.error ? 'offline' : 'no data' });
      // weather
      const c = s.weather?.current;
      if (c) {
        pWeather.set(wxIcon(c.condition, isDay(s.weather)));
        const rain = s.weather?.next_hour?.steps?.some((x) => x.mm >= 0.1);
        cards.weather.set({
          value: deg(c.temp),
          sub: rain ? 'rain within the hour' : (WX_TEXT[c.condition] ?? c.condition),
        });
      } else cards.weather.set({ value: '--°', sub: 'no data' });
      setLightning(s.lightning);
    },
    tick(now) {
      const c = st.weather?.current;
      cards.clock.set({
        value: hhmm(now),
        sub: c ? `${(WX_TEXT[c.condition] ?? c.condition).toLowerCase()} · ${deg(c.temp)}` : '',
      });
      setLightning(st.lightning);
    },
  };
}
