// Energy: today's cost so far (the hero), use against yesterday, the current price band and when it changes, and one
// chart: today's half-hours coloured by price band, with the band strip and an hour axis. Tap a bar for its reading.
import { fit8, setText, statusDot } from '../atoms.js';
import { axis, BAND_COLOUR, BAND_NAME, barChart, rateStrip, type Bar } from '../charts.js';
import { hm, money, pence } from '../format.js';
import { at, CW, content, header, layout, page, text } from '../layout.js';
import { nowIndex } from '../previews.js';
import { h } from '../dom.js';
import type { Band, EnergyData } from '../types.js';
import type { Screen, ScreenContext, ScreenOptions } from './common.js';

export function energyScreen(_ctx: ScreenContext, o: ScreenOptions = {}): Screen {
  const H = layout['chart-h'];
  const lbl = text('label', 'fs-e-label', 'today so far');
  const big = fit8(text('hero', 'fs-e-big', '£--'));
  const line1 = text('body', 'fs-e-line', 'no data');
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
  const el = page(
    'fs-energy',
    header({ label: 'Energy', onBack: o.onBack, right: [now] }),
    content(
      at(0, 0, CW, 24, lbl),
      at(0, 32, CW, 96, big),
      at(0, 144, CW, 32, line1),
      at(0, 184, CW, 32, h('div', { class: 'fs-line' }, rateDot, rateTxt)),
      at(0, 280, CW, H, bars),
      at(0, 288 + H, CW, 8, strip),
      at(0, 312 + H, CW, 24, ax),
    ),
  );
  let d: EnergyData | null | undefined = null,
    last: EnergyData | null | undefined,
    sel = -1,
    shown = false;
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
        : 'no data';
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
  }
  return {
    el,
    update(s) {
      if (s.energy === last) return;
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
    },
  };
}
