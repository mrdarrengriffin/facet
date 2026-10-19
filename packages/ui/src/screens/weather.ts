// Weather: the temperature (display 144) with the animated icon, the condition, one line (feels like, high, low),
// rain in the next hour, and the next eight hours. "Next 5 days" opens a day-by-day view.
import { weatherIcon, wxIcon, WX_TEXT } from '@facet/icons';
import { fit8, setText, statusDot } from '../atoms.js';
import { h } from '../dom.js';
import { deg, hm } from '../format.js';
import { at, CH, CW, content, header, layout, moreRow, page, subView, text } from '../layout.js';
import type { WeatherData } from '../types.js';
import { isDay, type Screen, type ScreenContext, type ScreenOptions } from './common.js';

export function weatherScreen(ctx: ScreenContext, o: ScreenOptions = {}): Screen {
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
  const more = moreRow('next 5 days', () => {
    sumView.hidden = true;
    daysView.hidden = false;
  });
  const place = fit8(text('label', 'fs-dim', ''));
  const sumView = subView(
    header({ label: 'Weather', onBack: o.onBack, right: [place] }),
    content(
      at(0, 0, CW - 160, 144, temp),
      at(CW - 144, 0, 144, 144, ic),
      at(0, 160, CW, 48, cond),
      at(0, 216, CW, 32, line),
      at(0, 256, CW, 32, h('div', { class: 'fs-line' }, rainDot, rain)),
      at(SX, 336, N * COL, 104, strip),
      at(0, CH - layout['more-row-h'], CW, layout['more-row-h'], more),
    ),
  );
  const daysView = subView(
    header({
      label: '5 days',
      onBack: () => {
        daysView.hidden = true;
        sumView.hidden = false;
      },
    }),
    content(dayRows),
  );
  daysView.hidden = true;
  let last: WeatherData | null | undefined,
    wasLoading = true;
  const render = (d: WeatherData | null | undefined) => {
    const c = d?.current;
    if (!c) {
      cond.textContent = d?.error ? 'Offline' : ctx.store.loading() ? 'Loading' : 'No data';
      return;
    }
    ic.set(wxIcon(c.condition, isDay(d)));
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
      if (v) x.ic.set(wxIcon(v.condition, isDay(d, v.t), v.precip));
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
  return {
    el: page('fs-weather', sumView, daysView),
    update(s) {
      setText(place, s.place ?? '');
      // a missing value re-renders once loading runs out ("Loading" -> "No data")
      if (s.weather === last && (s.weather || wasLoading === ctx.store.loading())) return;
      wasLoading = ctx.store.loading();
      last = s.weather;
      render(s.weather);
    },
    leave() {
      daysView.hidden = true;
      sumView.hidden = false;
    },
  };
}
