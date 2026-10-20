// Weather: one screen for the weather, the sun, the moon and lightning. A row of buttons under the header picks the
// view: Weather · Sun · Moon · Lightning. The Lightning button is only a bolt until a strike is near, then it shows
// the distance in orange, and opening Weather during a storm lands on Lightning.
//   Weather    the temperature (display 144) with the animated icon, the condition, one line (feels like, high, low),
//              rain in the next hour and the next eight hours. "Next 5 days" opens a day-by-day view.
//   Sun        the next sunrise or sunset as the hero, the day's arc with the sun on it, day length, the change since
//              yesterday, first and last light.
//   Moon       the Moon screen's moon, line and details.
//   Lightning  the Lightning screen's nearest strike, stats and radar.
// Home's Weather card is contextual: a storm takes it over, then rain within the hour, then the moon at night.
import { icon, weatherIcon, wxIcon, WX_TEXT } from '@facet/icons';
import { fit8, setText, statusDot } from '../atoms.js';
import { arc } from '../controls.js';
import { h } from '../dom.js';
import { deg, hm } from '../format.js';
import {
  at,
  CH,
  CW,
  content,
  header,
  layout,
  moreRow,
  page,
  stat,
  subView,
  text,
} from '../layout.js';
import { MOON_PHASE_TEXT } from '../moon.js';
import { lightningSummary } from '../radar.js';
import { segmented } from '../settings.js';
import { dayDelta, dayLength, inTime, sunNow } from '../sun.js';
import type { LightningData, MoonData, PanelData, Tone, WeatherData } from '../types.js';
import { isDay, missing, type Screen, type ScreenContext, type ScreenOptions } from './common.js';
import { lightningParts } from './lightning.js';
import { moonParts } from './moon.js';

// ---------------------------------------------------------------- the card's state (highest wins)
export type WeatherView = 'weather' | 'sun' | 'moon' | 'lightning';
export const WEATHER_VIEWS: WeatherView[] = ['weather', 'sun', 'moon', 'lightning'];
export type WeatherState = 'storm' | 'rain' | 'night' | 'day' | 'loading' | 'offline';
export const WEATHER_STATES: WeatherState[] = [
  'day',
  'night',
  'rain',
  'storm',
  'loading',
  'offline',
];

/** A storm is near: a strike within 25 km, or one closing in. */
export function stormNear(l: LightningData | null | undefined) {
  const n = l?.nearest;
  return !!n && (n.d <= 25 || lightningSummary(l).active);
}
/** Minutes until the first wet 15-minute step of the next hour (0 = raining now), or null when it stays dry. */
export function rainIn(w: WeatherData | null | undefined, now = Date.now()) {
  const wet = (w?.next_hour?.steps ?? []).find((x) => x.mm >= 0.1);
  return wet ? Math.max(0, Math.round((Date.parse(wet.t) - now) / 60_000)) : null;
}
/** Day or night now: the sun's `up` when there is one, else the next sunrise and sunset. */
export const isDayNow = (w: WeatherData | null | undefined) => w?.sun?.up ?? isDay(w);

/** Which state Home's Weather card is in: storm > rain within the hour > night > day; loading or offline first. */
export function weatherState(s: PanelData, now = Date.now()): WeatherState {
  if (stormNear(s.lightning)) return 'storm';
  const w = s.weather;
  if (!w?.current) return w?.error ? 'offline' : 'loading';
  if (rainIn(w, now) !== null) return 'rain';
  return isDayNow(w) ? 'day' : 'night';
}

/** The card's value and line for a state: "8 km" / "NW · 12 strikes in 15 min", "13°" / "rain in 20 min" ... */
export function weatherCardText(
  st: WeatherState,
  s: PanelData,
  now = Date.now(),
): { value: string; sub: string; accent: boolean } {
  const c = s.weather?.current,
    cond = c ? (WX_TEXT[c.condition] ?? c.condition).toLowerCase() : '';
  switch (st) {
    case 'storm': {
      const l = s.lightning!,
        n = l.nearest!;
      const k = l.counts.m15;
      return {
        value: `${n.d} km`,
        sub: `${n.dir} · ${k} strike${k === 1 ? '' : 's'} in 15 min`,
        accent: true,
      };
    }
    case 'rain': {
      const n = rainIn(s.weather, now)!;
      return {
        value: deg(c?.temp),
        sub: n <= 0 ? 'raining now' : `rain in ${n} min`,
        accent: false,
      };
    }
    case 'night': {
      const m = s.moon;
      if (!m) return { value: deg(c?.temp), sub: cond, accent: false };
      // a card line holds about 22 characters: condition + phase + lit, else condition + phase, else phase + lit
      const name = MOON_PHASE_TEXT[m.phase].toLowerCase().replace(/ moon$/, ''),
        lit = `${Math.round(m.illumination)}%`;
      const sub =
        [`${cond} · ${name} ${lit}`, `${cond} · ${name}`].find((x) => x.length <= 22) ??
        `${name} · ${lit}`;
      return { value: deg(c?.temp), sub, accent: false };
    }
    case 'day':
      return { value: deg(c?.temp), sub: cond, accent: false };
    case 'offline':
      return { value: '--°', sub: 'offline', accent: false };
    default:
      return { value: '--°', sub: 'loading', accent: false };
  }
}

// ---------------------------------------------------------------- the screen
export interface WeatherOptions extends ScreenOptions {
  /** The view it opens on. Default Weather, or Lightning while a storm is near. */
  view?: WeatherView;
}

const T = 'text' as Tone;
/** Top of a view: the buttons (48) and 24 below them. */
const V = 72;

export function weatherScreen(ctx: ScreenContext, o: WeatherOptions = {}): Screen {
  let view: WeatherView = o.view ?? 'weather',
    touched = false,
    visible = false;
  const show = (els: HTMLElement[], on: boolean) =>
    els.forEach((e) => (e.style.display = on ? '' : 'none'));

  // ---- the buttons; Lightning = a bolt, plus the distance in orange while a strike is near
  const tabs = segmented({
    options: [
      { id: 'weather', label: 'Weather' },
      { id: 'sun', label: 'Sun' },
      { id: 'moon', label: 'Moon' },
      { id: 'lightning', label: 'Lightning' },
    ],
    value: view,
    onPick: (v) => {
      touched = true;
      pick(v as WeatherView);
    },
  });
  tabs.classList.add('fs-wx-tabs');
  const lxBtn = tabs.querySelector<HTMLElement>('[data-v="lightning"]')!;
  const lxLbl = fit8(text('label', '', ''));
  lxBtn.replaceChildren(icon('bolt', 24), lxLbl);
  lxBtn.setAttribute('aria-label', 'Lightning');
  const setLxBtn = (near: { d: number } | null) => {
    lxBtn.classList.toggle('is-alert', !!near);
    lxLbl.style.display = near ? '' : 'none';
    if (near) setText(lxLbl, `${near.d} km`);
  };
  setLxBtn(null);

  // ---- Weather
  const temp = fit8(text('display', 'fs-w-temp', '--°'));
  const ic = weatherIcon('partly-day', { size: 144, animated: true, stroke: 3 });
  const cond = text('headline', 'fs-w-cond', 'Loading');
  const line = text('body', 'fs-w-line', '');
  const rainDot = statusDot('idle');
  const rain = text('body', 'fs-w-rain', '');
  const COL = layout['hour-col-w'],
    N = 8,
    SX = (CW - N * COL) / 2;
  const hours = Array.from({ length: N }, () => ({
    hr: text('label', 'fs-w-hr', '--'),
    ic: weatherIcon('cloudy', { size: 32 }),
    t: text('body', 'fs-w-ht', '--'),
  }));
  const strip = h(
    'div',
    { class: 'fs-w-hours' },
    ...hours.map((c) =>
      h(
        'div',
        { class: 'fs-w-col', style: { width: `${COL}px` } },
        c.hr,
        h('span', { class: 'fs-w-hic' }, c.ic),
        c.t,
      ),
    ),
  );
  const rainLine = h('div', { class: 'fs-line' }, rainDot, rain);
  const days = Array.from({ length: 5 }, () => ({
    name: text('title', 'fs-d-name', '--'),
    ic: weatherIcon('cloudy', { size: 48 }),
    c: text('body', 'fs-d-cond', ''),
    lo: text('body', 'fs-d-lo', '--'),
    hi: text('title', 'fs-d-hi', '--'),
  }));
  const dayRows = h(
    'div',
    { class: 'fs-days' },
    ...days.map((r) =>
      h(
        'div',
        { class: 'fs-day' },
        r.name,
        h('span', { class: 'fs-d-ic' }, r.ic),
        r.c,
        h('span', { class: 'f-grow' }),
        r.lo,
        r.hi,
      ),
    ),
  );
  const vWeather = [
    at(0, V, CW - 160, 144, temp),
    at(CW - 144, V, 144, 144, ic),
    at(0, V + 160, CW, 48, cond),
    at(0, V + 216, CW, 32, line),
    at(0, V + 256, CW, 32, rainLine),
    at(SX, V + 320, N * COL, 104, strip),
    at(
      0,
      CH - layout['more-row-h'],
      CW,
      layout['more-row-h'],
      moreRow('next 5 days', () => sub('days')),
    ),
  ];

  // ---- Sun: the next sunrise or sunset (hero), the day's arc with the sun on it, four stats
  const A = 320,
    half = (CW - layout.gutter) / 2;
  const sKind = text('label', 'fs-dim', 'sunset');
  const sTime = fit8(text('hero', 'fs-sun-t', '--:--'));
  const sIn = text('body', 'fs-w-line', '');
  const sArc = arc({
    size: A,
    start: 180,
    end: 0,
    track: { w: 2, c: 'line' },
    ind: { w: 4, c: 'accent' },
    value: 0,
    knob: { size: 24, border: 4 },
  });
  sArc.style.overflow = 'visible';
  const knob = sArc.querySelector('circle');
  const aRise = text('label', 'fs-dim', '--:--'),
    aSet = text('label', 'fs-dim fs-sun-r', '--:--');
  const stLen = stat('day length'),
    stDelta = stat('vs yesterday'),
    stDawn = stat('first light'),
    stDusk = stat('last light');
  const vSun = [
    at(0, V, 304, 24, sKind),
    at(0, V + 32, 304, 96, sTime),
    at(0, V + 136, 304, 32, sIn),
    at(CW - A, V, A, A / 2 + 8, sArc),
    at(CW - A, V + A / 2, A, 8, h('i', { class: 'fs-sun-hz' })),
    at(CW - A, V + A / 2 + 8, A / 2, 24, aRise),
    at(CW - A / 2, V + A / 2 + 8, A / 2, 24, aSet),
    at(0, V + 256, half, 64, stLen),
    at(half + layout.gutter, V + 256, half, 64, stDelta),
    at(0, V + 336, half, 64, stDawn),
    at(half + layout.gutter, V + 336, half, 64, stDusk),
  ];
  const tm = (iso: string | null) => (iso ? hm(iso) : '--:--');
  const renderSun = (now = Date.now()) => {
    const d = st.sun;
    if (!d) {
      sKind.textContent = 'sun';
      setText(sTime, '--:--');
      sIn.textContent = missing(ctx.store);
      return;
    }
    const { next, frac } = sunNow(d, now);
    sKind.textContent = next?.kind ?? 'sun';
    setText(sTime, next ? hm(new Date(next.t).toISOString()) : '--:--');
    sIn.textContent = next ? inTime(next.t - now) : d.rise ? '' : 'the sun stays down today';
    aRise.textContent = tm(d.rise);
    aSet.textContent = tm(d.set);
    sArc.set(frac === null ? 0 : frac * 100);
    if (knob) knob.style.display = frac === null ? 'none' : '';
    stLen.set(d.length === null ? '--' : dayLength(d.length), T);
    stDelta.set(d.delta === null ? '--' : dayDelta(d.delta), 'dim');
    stDawn.set(tm(d.dawn), T);
    stDusk.set(tm(d.dusk), T);
  };

  // ---- Moon + Lightning: the Moon and Lightning screens' parts
  let el: HTMLElement | null = null;
  const moon = moonParts(ctx, {
    top: V,
    size: 304,
    onDetails: () => sub('moon'),
    onDetailsBack: () => sub(null),
  });
  const lx = lightningParts(ctx, { top: V, root: () => el });
  const vMoon = moon.nodes,
    vLx = lx.nodes;

  // ---- the page
  const place = fit8(text('label', 'fs-dim', ''));
  const mainView = subView(
    header({ label: 'Weather', onBack: o.onBack, right: [place] }),
    content(at(0, 0, null, 48, tabs), ...vWeather, ...vSun, ...vMoon, ...vLx),
  );
  const daysView = subView(header({ label: '5 days', onBack: () => sub(null) }), content(dayRows));
  daysView.hidden = true;
  el = page('fs-weather fs-lightning', mainView, daysView, ...moon.views);
  const views: Record<WeatherView, HTMLElement[]> = {
    weather: vWeather,
    sun: vSun,
    moon: vMoon,
    lightning: vLx,
  };
  function pick(v: WeatherView) {
    view = v;
    tabs.set(v);
    WEATHER_VIEWS.forEach((k) => show(views[k], k === v));
    show([place], v === 'weather');
    el!.dataset.view = v;
    lx.show(v === 'lightning' && visible);
    if (v === 'sun') renderSun();
  }
  function sub(which: 'days' | 'moon' | null) {
    mainView.hidden = which !== null;
    daysView.hidden = which !== 'days';
    if (which === 'moon') moon.openDetails();
    else moon.closeDetails();
  }

  // ---- data
  let st: PanelData = ctx.store.get(),
    lastW: WeatherData | null | undefined,
    lastM: MoonData | null | undefined,
    wasLoading = true,
    near = false;
  const renderWeather = (d: WeatherData | null | undefined) => {
    const c = d?.current;
    // loading / offline: only the words (no icon, rain line or hours)
    [ic, strip, rainLine].forEach((x) => (x.style.visibility = c ? '' : 'hidden'));
    if (!c) {
      setText(temp, '--°');
      cond.textContent = d?.error ? 'Offline' : ctx.store.loading() ? 'Loading' : 'No data';
      line.textContent = d?.error ? 'weather data unavailable' : '';
      return;
    }
    ic.set(wxIcon(c.condition, isDayNow(d)));
    setText(temp, deg(c.temp));
    cond.textContent = WX_TEXT[c.condition] ?? c.condition;
    const today = d.daily?.[0];
    line.textContent = `Feels ${deg(c.feels)} · high ${deg(today?.hi)}, low ${deg(today?.lo)}`;
    const wet = (d.next_hour?.steps ?? []).filter((x) => x.mm >= 0.1);
    rain.textContent = wet.length
      ? `Rain from ${hm(wet[0]!.t)}`
      : `Dry for the next hour${d.next_rain ? ` · rain at ${hm(d.next_rain.t)}` : ''}`;
    rainDot.dataset.tone = wet.length ? 'info' : 'idle';
    const hrs = (d.hourly ?? []).slice(0, N);
    hours.forEach((x, i) => {
      const v = hrs[i];
      x.hr.textContent = !v ? '--' : i === 0 ? 'now' : hm(v.t).slice(0, 2);
      x.hr.classList.toggle('is-now', i === 0);
      if (v) x.ic.set(wxIcon(v.condition, i === 0 ? isDayNow(d) : isDay(d, v.t), v.precip));
      x.t.textContent = v ? deg(v.temp) : '--';
    });
    const ds = d.daily ?? [];
    days.forEach((r, i) => {
      const x = ds[i];
      if (!x) return;
      r.name.textContent =
        i === 0 ? 'Today' : new Date(x.t).toLocaleDateString('en-GB', { weekday: 'long' });
      r.ic.set(wxIcon(x.condition, true, x.precip));
      r.c.textContent =
        (WX_TEXT[x.condition] ?? x.condition) +
        (x.precip > 0 ? ` · ${x.precip.toFixed(1)} mm` : '');
      r.lo.textContent = deg(x.lo);
      r.hi.textContent = deg(x.hi);
    });
  };
  pick(view);

  return {
    el,
    update(s) {
      st = s;
      setText(place, s.place ?? '');
      lx.update(s.lightning);
      const was = near;
      near = stormNear(s.lightning);
      setLxBtn(near ? s.lightning!.nearest : null);
      // the storm came in while Weather was open and nobody picked a view: go to Lightning
      if (visible && near && !was && !touched && view === 'weather') pick('lightning');
      if (s.moon !== lastM || (!s.moon && wasLoading !== ctx.store.loading())) {
        lastM = s.moon;
        moon.render(s.moon);
      }
      if (view === 'sun') renderSun();
      // a missing value re-renders once loading runs out ("Loading" -> "No data")
      if (s.weather === lastW && (s.weather || wasLoading === ctx.store.loading())) return;
      wasLoading = ctx.store.loading();
      lastW = s.weather;
      renderWeather(s.weather);
    },
    tick(now) {
      if (view === 'lightning') lx.render(now.getTime());
      if (view === 'sun' && now.getSeconds() === 0) renderSun(now.getTime());
    },
    enter() {
      visible = true;
      touched = false;
      sub(null);
      pick(o.view ?? (near ? 'lightning' : 'weather'));
    },
    leave() {
      visible = false;
      lx.show(false);
      sub(null);
    },
  };
}
