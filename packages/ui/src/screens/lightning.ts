// Lightning: the nearest strike in the last 15 minutes (the hero; "Quiet" when there's nothing), three stats (strikes
// in 15 minutes, the trend, the last strike) and the radar. A strike within 25 km turns the hero orange.
import { fit8, setText } from '../atoms.js';
import { ago } from '../format.js';
import { at, CH, CW, content, header, layout, page, stat, text } from '../layout.js';
import { lightningRadar } from '../radar.js';
import type { LightningData, Tone } from '../types.js';
import type { Screen, ScreenContext, ScreenOptions } from './common.js';

export function lightningScreen(ctx: ScreenContext, o: ScreenOptions = {}): Screen {
  const R = layout.radar;
  const radar = lightningRadar({ size: R });
  radar.el.classList.add('fs-radar');
  const kN = text('label', 'fs-dim', 'nearest');
  const nNum = fit8(text('hero', 'fs-lx-n', '--'));
  const nUnit = text('body', 'fs-lx-u', '');
  const sCount = stat('last 15 min'),
    sTrend = stat('trend'),
    sLast = stat('last strike');
  const el = page(
    'fs-lightning',
    header({ label: 'Lightning', onBack: o.onBack }),
    content(
      at(0, 0, 240, 24, kN),
      at(0, 32, 240, 96, nNum),
      at(0, 136, 240, 32, nUnit),
      at(0, 216, 240, 64, sCount),
      at(0, 296, 240, 64, sTrend),
      at(0, 376, 240, 64, sLast),
      at(CW - R, (CH - R) / 2, R, R, radar.el),
    ),
  );
  let d: LightningData | null | undefined = ctx.store.get().lightning,
    visible = false;
  function render(now = Date.now()) {
    radar.render(d, visible, now);
    const n = d?.nearest;
    setText(nNum, n ? String(n.d) : d ? 'Quiet' : '--');
    nNum.classList.toggle('is-word', !n);
    nUnit.textContent = n ? `km ${n.dir}` : d ? `nothing within ${d.radius} km` : 'no data';
    el.classList.toggle('is-near', !!n && n.d <= 25);
    sCount.set(
      d ? `${d.counts.m15} strike${d.counts.m15 === 1 ? '' : 's'}` : '--',
      d?.counts.m15 ? ('text' as Tone) : 'dim',
    );
    const tr = d?.trend;
    if (!tr || tr.state === 'none') sTrend.set('--', 'dim', 'trend');
    else if (tr.state === 'approaching') sTrend.set(`${tr.kmh} km/h`, 'warn', 'approaching');
    else if (tr.state === 'receding') sTrend.set(`${Math.abs(tr.kmh)} km/h`, 'ok', 'moving away');
    else sTrend.set('steady', 'dim', 'trend');
    sLast.set(d?.last ? ago(d.last, now) : 'none', 'dim');
  }
  return {
    el,
    update(s) {
      d = s.lightning;
      render();
    },
    tick() {
      render();
    },
    enter() {
      visible = true;
    },
    leave() {
      visible = false;
    },
  };
}
