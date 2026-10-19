// Energy: today's cost so far (the hero), use against yesterday, the current price band and when it changes, and one
// chart: today's half-hours coloured by price band, with the band strip and an hour axis. Tap a bar for its reading.
// "Last 7 days" opens the week: its cost as the hero, use per day against the week before, and pairs of bars per day.
import { fit8, setText, statusDot } from '../atoms.js';
import { axis, BAND_COLOUR, BAND_NAME, barChart, legend, rateStrip, type Bar } from '../charts.js';
import { hm, money, pence } from '../format.js';
import { at, CH, CW, content, header, layout, moreRow, page, subView, text } from '../layout.js';
import { nowIndex } from '../previews.js';
import { h } from '../dom.js';
import type { Band, EnergyData, EnergyDay } from '../types.js';
import { missing, type Screen, type ScreenContext, type ScreenOptions } from './common.js';

export function energyScreen(ctx: ScreenContext, o: ScreenOptions = {}): Screen {
  const H = layout['chart-h'];
  const lbl = text('label', 'fs-e-label', 'today so far');
  const big = fit8(text('hero', 'fs-e-big', '£--'));
  const line1 = text('body', 'fs-e-line', 'loading');
  const rateDot = statusDot('idle');
  const rateTxt = text('body', 'fs-e-rate', '');
  const now = fit8(text('label', 'fs-e-now', ''));
  const bars = barChart({ w: CW, h: H, bars: [], gap: 2, radius: 2, onTap: (i) => showSlot(i) });
  const strip = rateStrip({ w: CW, h: 8, bands: [] });
  const ax = axis({
    w: CW,
    ticks: [0, 6, 12, 18, 24].map((hr) => ({ at: hr / 24, text: String(hr).padStart(2, '0') })),
  });
  ax.classList.add('fs-axis');
  const more = moreRow('last 7 days', () => {
    sumView.hidden = true;
    weekView.hidden = false;
    requestAnimationFrame(() => wkBars.play(24, 420));
  });
  const sumView = subView(
    header({ label: 'Energy', onBack: o.onBack, right: [now] }),
    content(
      at(0, 0, CW, 24, lbl),
      at(0, 32, CW, 96, big),
      at(0, 144, CW, 32, line1),
      at(0, 184, CW, 32, h('div', { class: 'fs-line' }, rateDot, rateTxt)),
      at(0, 240, CW, H, bars),
      at(0, 248 + H, CW, 8, strip),
      at(0, 272 + H, CW, 24, ax),
      at(0, CH - layout['more-row-h'], CW, layout['more-row-h'], more),
    ),
  );
  // the week: pairs per day (the week before dashed, this week solid), today last
  const wkLbl = text('label', 'fs-e-label', 'this week');
  const wkBig = fit8(text('hero', 'fs-e-big', '£--'));
  const wkLine = text('body', 'fs-e-line', 'loading');
  let wkLeg = legend([]);
  const wkLegBox = at(0, 184, CW, 32, h('div', { class: 'fs-e-legend' }, wkLeg));
  const wkBars = barChart({
    w: CW,
    h: H,
    bars: [],
    gap: 2,
    radius: 2,
    group: 2,
    groupGap: 16,
    onTap: (i) => showDay(i),
  });
  let wkAx = axis({ w: CW, ticks: [] });
  const wkAxBox = at(0, 272 + H, CW, 24, wkAx);
  const weekView = subView(
    header({
      label: 'Last 7 days',
      onBack: () => {
        weekView.hidden = true;
        sumView.hidden = false;
      },
    }),
    content(
      at(0, 0, CW, 24, wkLbl),
      at(0, 32, CW, 96, wkBig),
      at(0, 144, CW, 32, wkLine),
      wkLegBox,
      at(0, 240, CW, H, wkBars),
      wkAxBox,
    ),
  );
  weekView.hidden = true;
  const el = page('fs-energy', sumView, weekView);
  let d: EnergyData | null | undefined = null,
    last: EnergyData | null | undefined,
    wasLoading = true,
    sel = -1,
    wkSel = -1,
    shown = false;
  const weeks = () => {
    const days = d?.days ?? [];
    return { cur: days.slice(-7), prev: days.slice(-14, -7) };
  };
  const dayName = (x: EnergyDay, long = false) =>
    new Date(`${x.date}T12:00:00`).toLocaleDateString('en-GB', {
      weekday: long ? 'long' : 'short',
    });
  function showDay(i: number) {
    wkSel = i;
    const { cur, prev } = weeks();
    const x = i < 0 ? undefined : (i % 2 ? cur : prev)[i >> 1];
    if (!x) {
      wkLbl.textContent = 'this week';
      return;
    }
    const name = i % 2 && i >> 1 === cur.length - 1 ? 'today' : dayName(x, true);
    wkLbl.textContent = `${i % 2 ? name : `${name} before`} · ${x.kwh === null ? 'no data' : `${x.kwh.toFixed(1)} kWh`}${x.cost === null ? '' : ` · ${money(x.cost)}`}`;
  }
  function renderWeek() {
    const { cur, prev } = weeks();
    const sum = (xs: (number | null)[]) => {
      const v = xs.filter((x): x is number => x !== null);
      return v.length ? v.reduce((a, b) => a + b, 0) : null;
    };
    // average per complete day (only days with data, today excluded), so gaps and a partial today stay honest
    const avg = (xs: (number | null)[]) => {
      const v = xs.filter((x): x is number => x !== null);
      return v.length ? { a: v.reduce((p, q) => p + q, 0) / v.length, n: v.length } : null;
    };
    const cost = sum(cur.map((x) => x.cost)),
      kwh = sum(cur.map((x) => x.kwh));
    setText(wkBig, cost === null ? '£--' : money(cost));
    const a = avg(cur.slice(0, -1).map((x) => x.kwh)),
      p = avg(prev.map((x) => x.kwh));
    // compare only with at least 3 days on each side; one odd day says nothing about a week
    const delta = a && p && a.n >= 3 && p.n >= 3 ? Math.round(((a.a - p.a) / p.a) * 100) : null;
    wkLine.textContent =
      kwh === null
        ? d?.error
          ? `offline · ${d.error}`
          : d
            ? 'no data'
            : missing(ctx.store)
        : `${kwh.toFixed(1)} kWh${delta === null ? '' : ` · ${Math.abs(delta)}% ${delta <= 0 ? 'less' : 'more'} a day than the week before`}`;
    const nl = legend([
      { c: 'text-dim', text: 'this week' },
      // the week before may have gaps (recorder history): say how many days it has
      {
        c: 'line-hi',
        text: p && p.n < 7 ? `week before · ${p.n} ${p.n === 1 ? 'day' : 'days'}` : 'week before',
        ghost: true,
      },
    ]);
    wkLeg.replaceWith(nl);
    wkLeg = nl;
    const pairs: Bar[] = [];
    for (let i = 0; i < 7; i++) {
      pairs.push({ v: prev[i]?.kwh ?? null, c: 'line-hi', ghost: true });
      pairs.push({ v: cur[i]?.kwh ?? null, c: 'text-dim' });
    }
    wkBars.set(pairs);
    const na = axis({
      w: CW,
      ticks: cur.map((x, i) => ({
        at: (i + 0.5) / 7,
        text: i === cur.length - 1 ? 'Today' : dayName(x),
      })),
    });
    na.classList.add('fs-axis');
    wkAx.replaceWith(na);
    wkAx = na;
    if (wkSel >= 0) showDay(wkSel);
  }
  function showSlot(i: number) {
    sel = i;
    const s = d?.today?.slots?.[i];
    if (i < 0 || !s) {
      lbl.textContent = 'today so far';
      return;
    }
    lbl.textContent = `${hm(s.start)} · ${s.kwh === null ? 'no data' : `${s.kwh.toFixed(2)} kWh`} · ${pence(s.rate)}`;
  }
  function render() {
    const t = d?.today,
      sl = t?.slots ?? [];
    setText(big, t ? money(t.cost) : '£--');
    const y = d?.yesterday_to_now;
    const delta = t?.kwh != null && y ? Math.round(((t.kwh - y) / y) * 100) : null;
    line1.textContent = t
      ? `${t.kwh?.toFixed(1) ?? '--'} kWh${delta === null ? '' : ` · ${Math.abs(delta)}% ${delta <= 0 ? 'less' : 'more'} than yesterday by now`}`
      : d?.error
        ? `offline · ${d.error}`
        : missing(ctx.store);
    const r = d?.rate,
      band = (r?.band ?? 'normal') as Band;
    rateDot.dataset.tone = band === 'peak' ? 'working' : band === 'cheap' ? 'info' : 'idle';
    const name = BAND_NAME[band];
    rateTxt.textContent =
      r?.now != null
        ? `${name[0]!.toUpperCase()}${name.slice(1)} ${pence(r.now)} until ${r.until ? hm(r.until) : '--'}${r.next != null ? `, then ${pence(r.next)}` : ''}`
        : '';
    const w = d?.power?.w;
    setText(
      now,
      w != null ? (w >= 1000 ? `${(w / 1000).toFixed(1)} kw now` : `${Math.round(w)} w now`) : '',
    );
    const ni = nowIndex(sl);
    bars.set(
      sl.map((s): Bar => ({ v: s.kwh, c: BAND_COLOUR[s.band] })),
      ni,
    );
    strip.set(
      sl.map((s) => s.band),
      ni,
    );
    if (sel >= 0) showSlot(sel);
    renderWeek();
  }
  return {
    el,
    update(s) {
      // a missing value re-renders once loading runs out ("loading" -> "no data")
      if (s.energy === last && (s.energy || wasLoading === ctx.store.loading())) return;
      wasLoading = ctx.store.loading();
      last = d = s.energy;
      render();
    },
    enter() {
      if (shown) return;
      shown = true;
      requestAnimationFrame(() => bars.play(10, 420));
    },
    leave() {
      shown = false;
      weekView.hidden = true;
      sumView.hidden = false;
    },
  };
}
