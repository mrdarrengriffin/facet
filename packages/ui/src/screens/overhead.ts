// Overhead: what's flying over the house. The hero is the aircraft highest in the sky, the one you'd see looking up
// (an emergency squawk always wins): where to look ("high in the NW · 38°", accent from 30° up), the route in
// headline type ("Catania → Leeds", or the callsign when there's no believable route), airline · flight · type, then
// three stats (altitude, which reads climbing / descending past ±300 ft/min, speed, distance) and a 208 sky radar on
// the right. Below: "also up there" (two rows) and "all aircraft". Header Now | Radar: the radar at 576, every
// aircraft a plane turned to its track, the hero accent, callsigns on the five highest; tap one for its details.
// Distances and speeds follow Settings > Units (units.ts): the circle is 50 km (rings 5/15/30/50) or 30 miles
// (rings 3/10/20/30).
import { h, s } from '../dom.js';
import { fit8, live, setText, statusDot } from '../atoms.js';
import { compass } from '../radar.js';
import { at, CW, content, header, moreRow, page, row, stat, text } from '../layout.js';
import { segmented } from '../settings.js';
import { distance, fromKm, KM_PER_MI, speedKt, unitsOf, type DistanceUnit } from '../units.js';
import type { Aircraft, OverheadData, PanelData, Tone } from '../types.js';
import { missing, type Screen, type ScreenContext, type ScreenOptions } from './common.js';

/** From this many degrees up, it's "high": the accent, and the Home card says "look up". */
export const LOOK_UP = 30;
/** The circle per distance unit; rings in that unit, the last one is the edge. */
export const OVERHEAD_RANGE: Record<DistanceUnit, { km: number; rings: number[] }> = {
  km: { km: 50, rings: [5, 15, 30, 50] },
  mi: { km: 30 * KM_PER_MI, rings: [3, 10, 20, 30] },
};

// ---------------------------------------------------------------- wording
const n = (x: number) => x.toLocaleString('en-GB');
/** "32,000 ft" (to 25 ft). */
export const altText = (a: Pick<Aircraft, 'alt'>) =>
  a.alt === null ? '--' : `${n(Math.round(a.alt / 25) * 25)} ft`;
/** "26 km SE" / "16 mi SE". */
export const awayText = (a: Pick<Aircraft, 'km' | 'brg'>, u: DistanceUnit) =>
  `${distance(a.km, u)} ${compass(a.brg)}`;
/** The altitude stat's label: climbing / descending past ±300 ft/min, else altitude. */
export const climbText = (a: Pick<Aircraft, 'vs'>) =>
  a.vs === null || Math.abs(a.vs) < 300 ? 'altitude' : a.vs > 0 ? 'climbing' : 'descending';
/** Where to look: "right overhead" (60°+), "high in the NW" (30°+), "in the NW" (10°+), "low in the NW". */
export function lookText(a: Pick<Aircraft, 'elev' | 'brg'>) {
  const c = compass(a.brg);
  return a.elev >= 60
    ? 'right overhead'
    : a.elev >= LOOK_UP
      ? `high in the ${c}`
      : a.elev >= 10
        ? `in the ${c}`
        : `low in the ${c}`;
}
/** The big line: "Catania → Leeds", else the callsign / registration. */
export const routeText = (a: Aircraft) =>
  a.from && a.to ? `${a.from.city} → ${a.to.city}` : (a.callsign ?? a.reg ?? a.hex.toUpperCase());
const modelText = (a: Aircraft) => a.model ?? a.type;
/** "Jet2.com · LS13DK · Boeing 737-800": what's known. */
export const whoText = (a: Aircraft) =>
  [
    a.airline ?? (a.from ? null : a.operator),
    a.flightNo ?? (a.from && a.to ? a.callsign : null),
    modelText(a),
  ]
    .filter(Boolean)
    .join(' · ');
const EMERGENCY: Record<string, string> = {
  general: 'emergency',
  nordo: 'radio failure',
  unlawful: 'hijack',
  minfuel: 'low fuel',
  lifeguard: 'medical',
  downed: 'aircraft down',
};
const emText = (a: Aircraft) => EMERGENCY[a.emergency ?? ''] ?? 'emergency';
/** A row's / the hero's status: emergency (error), military, helicopter; else none. */
export function aircraftStatus(a: Aircraft): { tone: Tone; text: string } | null {
  if (a.emergency) return { tone: 'error', text: `${a.squawk ?? ''} ${emText(a)}`.trim() };
  if (a.military) return { tone: 'info', text: 'military' };
  if (a.kind === 'heli') return { tone: 'info', text: 'helicopter' };
  return null;
}

/** The moment: emergency > overhead (LOOK_UP+) > up > quiet / offline, and the hero aircraft. */
export function overheadNow(d: OverheadData | null | undefined) {
  const top = d?.aircraft[0] ?? null;
  const phase: 'emergency' | 'overhead' | 'up' | 'quiet' | 'offline' | 'missing' = !d
    ? 'missing'
    : !top
      ? d.error
        ? 'offline'
        : 'quiet'
      : top.emergency
        ? 'emergency'
        : top.elev >= LOOK_UP
          ? 'overhead'
          : 'up';
  return { phase, top };
}

/** The Home card's lines: route + "32,000 ft · 26 km SE" ("look up" while high; the squawk on an emergency). */
export function overheadCardText(
  s: Pick<PanelData, 'overhead' | 'settings'>,
  missingText = 'no data',
) {
  const d = s.overhead,
    u = unitsOf(s).distance,
    { phase, top } = overheadNow(d);
  const edge = `${OVERHEAD_RANGE[u].rings.at(-1)} ${u}`;
  if (!top)
    return {
      text: phase === 'offline' ? 'Feed offline' : phase === 'quiet' ? 'Clear skies' : 'Loading',
      sub: phase === 'quiet' ? `nothing within ${edge}` : phase === 'missing' ? missingText : '',
      accent: false,
      tone: undefined as Tone | undefined,
    };
  if (phase === 'emergency')
    return {
      text: routeText(top),
      sub: `${top.squawk} ${emText(top)} · ${distance(top.km, u).replace(/\.\d/, '')}`,
      accent: false,
      tone: 'error' as Tone,
    };
  return {
    text: routeText(top),
    sub: `${altText(top)} · ${phase === 'overhead' ? 'look up' : awayText(top, u)}`,
    accent: phase === 'overhead',
    tone: undefined as Tone | undefined,
  };
}

// ---------------------------------------------------------------- parts
/** A plane seen from above, nose up (24 grid, filled), turned to `track`. */
export function planeGlyph(size = 24, track = 0) {
  const svg = s(
    'svg',
    {
      class: 'fs-oh-plane',
      width: size,
      height: size,
      viewBox: '0 0 24 24',
      'aria-hidden': 'true',
    },
    s('path', {
      d: 'M12 2c.8 0 1.3 1 1.3 2.2V9l7.7 4.4v2l-7.7-2.3v4.6l2.4 1.8V21L12 20.2 8.3 21v-1.5l2.4-1.8v-4.6L3 15.4v-2L10.7 9V4.2C10.7 3 11.2 2 12 2z',
    }),
  );
  svg.style.transform = `rotate(${Math.round(track)}deg)`;
  return svg;
}

/**
 * The sky radar: home in the middle, rings on a √ scale in the distance unit, N/E/S/W, a plane per aircraft turned
 * to its track, the hero in the accent with a ring (an emergency in the error colour). 72 = the Home card preview
 * (two rings, dots), 208 = beside the hero, 576 = the Radar view (ring labels, callsigns on the `labels` highest).
 * `set(aircraft, hero, unit)`.
 */
export function skyRadar(size: number, o: { labels?: number; onTap?: (a: Aircraft) => void } = {}) {
  const R = size / 2,
    small = size < 120,
    glyph = size >= 400 ? 32 : 24;
  let unit: DistanceUnit | null = null;
  const edge = () => OVERHEAD_RANGE[unit ?? 'km'].rings.at(-1)!;
  const rPx = (km: number) =>
    (R - (small ? 4 : glyph / 2)) * Math.sqrt(Math.min(fromKm(km, unit ?? 'km'), edge()) / edge());
  const frame = h('div', { class: 'fs-oh-frame' });
  const drawFrame = (u: DistanceUnit) => {
    if (unit === u) return;
    unit = u;
    const g = OVERHEAD_RANGE[u],
      outer = edge(),
      ring = (v: number) => rPx(v * (u === 'mi' ? KM_PER_MI : 1));
    const svg = s('svg', {
      class: 'fs-oh-rsvg',
      width: size,
      height: size,
      viewBox: `0 0 ${size} ${size}`,
      'aria-hidden': 'true',
    });
    for (const v of small ? [g.rings[1]!, outer] : g.rings)
      svg.append(
        s('circle', {
          cx: R,
          cy: R,
          r: Math.max(1, ring(v) - 0.5),
          class: v === outer ? 'fs-oh-ring is-outer' : 'fs-oh-ring',
        }),
      );
    const kids: Node[] = [svg];
    if (!small) {
      const B = 24,
        L = (t: string, x: number, y: number) =>
          h('span', { class: 'fst-label fs-oh-cmp', style: { left: `${x}px`, top: `${y}px` } }, t);
      kids.push(
        L('N', R - B / 2, 0),
        L('E', size - B, R - B / 2),
        L('S', R - B / 2, size - B),
        L('W', 0, R - B / 2),
      );
      if (size >= 400)
        for (const v of g.rings)
          kids.push(
            h(
              'span',
              {
                class: 'fst-label fs-oh-rlbl',
                style: { left: `${R + 8}px`, top: `${R + ring(v) - 32}px` },
              },
              v === outer ? `${v} ${u}` : String(v),
            ),
          );
    }
    frame.replaceChildren(...kids);
  };
  drawFrame('km');
  const home = h('i', { class: 'fs-oh-home', style: { left: `${R - 4}px`, top: `${R - 4}px` } });
  const marks = h('div', { class: 'fs-oh-marks' });
  const el = h(
    'div',
    { class: 'fs-oh-radar', style: { width: `${size}px`, height: `${size}px` } },
    frame,
    marks,
    home,
  );
  return Object.assign(el, {
    set(list: Aircraft[], hero: Aircraft | null, u: DistanceUnit = 'km') {
      drawFrame(u);
      marks.replaceChildren(
        ...[...list].reverse().map((a, ri) => {
          // the hero (first) is painted last, on top
          const i = list.length - 1 - ri,
            rad = (a.brg * Math.PI) / 180,
            r = rPx(a.km);
          const sz = small ? 8 : glyph,
            isHero = a === hero;
          const m = h(
            'div',
            {
              class: `fs-oh-mark${isHero ? ' is-hero' : ''}${a.emergency ? ' is-em' : ''}`,
              style: {
                left: `${Math.round(R + r * Math.sin(rad) - sz / 2)}px`,
                top: `${Math.round(R - r * Math.cos(rad) - sz / 2)}px`,
                width: `${sz}px`,
                height: `${sz}px`,
              },
            },
            small ? h('i', { class: 'fs-oh-dot' }) : planeGlyph(sz, a.track ?? 0),
          );
          if (!small && isHero) m.append(h('i', { class: 'fs-oh-halo' }));
          if (o.labels && i < o.labels && a.callsign)
            m.append(text('label', 'fs-oh-cs', a.callsign));
          if (o.onTap) {
            m.setAttribute('data-snd', 'open');
            live(m, () => o.onTap!(a));
          }
          return m;
        }),
      );
    },
  });
}

/** The Home card preview (72): the radar with dots. */
export const previewOverhead = () => skyRadar(72);

/** The aircraft hero (656 × 264): look line + status, route (headline, 2 lines), who, 3 stats, the 208 radar. */
export function aircraftHero() {
  const look = text('label', 'fs-oh-look'),
    route = h('div', { class: 'fst-headline fs-oh-route' }),
    who = text('body', 'fs-oh-who');
  const stDot = statusDot('error'),
    stT = fit8(text('label', '', '')),
    st = h('span', { class: 'fs-status' }, stDot, stT);
  const sAlt = stat('altitude'),
    sSpd = stat('speed'),
    sAway = stat('distance');
  const radar = skyRadar(208);
  const el = h(
    'div',
    { class: 'fs-oh-hero', style: { position: 'relative', width: `${CW}px`, height: '264px' } },
    at(0, 0, 400, 24, h('div', { class: 'fs-oh-lookrow' }, look, st)),
    at(0, 40, 400, 96, route),
    at(0, 152, 400, 32, who),
    at(0, 200, 144, 64, sAlt),
    at(152, 200, 120, 64, sSpd),
    at(280, 200, 120, 64, sAway),
    at(CW - 208, 0, 208, 208, radar),
  );
  return Object.assign(el, {
    set(a: Aircraft, all: Aircraft[], s: Pick<PanelData, 'settings'>) {
      const u = unitsOf(s);
      setText(look, `${lookText(a)} · ${Math.round(a.elev)}°`);
      el.classList.toggle('is-accent', a.elev >= LOOK_UP && !a.emergency);
      const status = aircraftStatus(a);
      st.hidden = !status;
      if (status) {
        st.dataset.tone = status.tone;
        stDot.dataset.tone = status.tone;
        setText(stT, status.text);
      }
      route.textContent = routeText(a);
      who.textContent = whoText(a) || a.type || '';
      sAlt.set(altText(a), 'dim', climbText(a));
      sSpd.set(speedKt(a.gs, u.speed), 'dim');
      sAway.set(awayText(a, u.distance), 'dim');
      radar.set(all, a, u.distance);
    },
  });
}

/** An aircraft row (112): "B738 · 32,000 ft · 26 km SE" over the route; emergency / military / helicopter get a status. */
export function aircraftRow(a: Aircraft, u: DistanceUnit = 'km', onTap?: () => void) {
  return row({
    label: [a.type, altText(a), awayText(a, u)].filter(Boolean).join(' · '),
    title: routeText(a),
    status: aircraftStatus(a),
    onTap,
  });
}

// ---------------------------------------------------------------- the screen
export type OverheadView = 'now' | 'radar';
export interface OverheadOptions extends ScreenOptions {
  /** Which view it opens on. Default `now`. */
  view?: OverheadView;
}

export function overheadScreen(ctx: ScreenContext, o: OverheadOptions = {}): Screen {
  let st: PanelData = ctx.store.get(),
    view: OverheadView | 'list' | 'detail' = o.view ?? 'now',
    from: OverheadView | 'list' = 'now',
    picked: string | null = null;
  const sec = (t: string) => h('div', { class: 'fs-sec' }, text('label', '', t));
  // Now | Radar: one per header (a node sits in one place), kept in step by show()
  const mkTabs = () =>
    segmented({
      options: [
        { id: 'now', label: 'now' },
        { id: 'radar', label: 'radar' },
      ],
      value: view,
      onPick: (v) => show(v as OverheadView),
    });
  const tabs = [mkTabs(), mkTabs()];
  const stale = text('label', 'fs-oh-stale', 'offline · last known');

  // now
  const hero = aircraftHero();
  live(hero, () => {
    const t = st.overhead?.aircraft[0];
    if (t) openAircraft(t);
  });
  const emptyT = text('headline', 'fs-oh-et', 'Loading'),
    emptyS = text('body', 'fs-oh-es', '');
  const emptyRadar = skyRadar(208);
  const emptyBox = h(
    'div',
    {
      class: 'f-abs fs-oh-emptybox',
      style: { left: '0', top: '0', width: `${CW}px`, height: '208px' },
    },
    at(0, 0, 400, 160, h('div', { class: 'fs-oh-empty' }, emptyT, emptyS)),
    at(CW - 208, 0, 208, 208, emptyRadar),
  );
  const alsoHd = sec('also up there'),
    also = h('div', { class: 'fs-rows' }),
    more = moreRow('all aircraft', () => show('list'));
  const nowView = h(
    'div',
    { class: 'f-sub' },
    header({ label: 'Overhead', onBack: o.onBack, right: [stale, tabs[0]!] }),
    content(at(0, 0, CW, 264, hero), emptyBox, at(0, 312, CW, null, alsoHd, also, more)),
  );
  // radar
  const big = skyRadar(576, { labels: 5, onTap: (a) => openAircraft(a) });
  const radarView = h(
    'div',
    { class: 'f-sub', hidden: true },
    header({ label: 'Overhead', onBack: o.onBack, right: [tabs[1]!] }),
    content(at((CW - 576) / 2, 0, 576, 576, big)),
  );
  // list
  const listHd = sec('highest first'),
    listRows = h('div', { class: 'fs-rows' });
  const listView = h(
    'div',
    { class: 'f-sub', hidden: true },
    header({ label: 'All aircraft', onBack: () => show('now') }),
    content(at(0, 0, CW, null, listHd, listRows)),
  );
  // detail
  const dHero = aircraftHero();
  const dHead = header({ label: '-', onBack: () => show(from) });
  const facts = ['aircraft', 'registration', 'operator', 'squawk', 'heading', 'vertical speed'].map(
    stat,
  );
  const src = text('label', 'fs-oh-src', '');
  const detailView = h(
    'div',
    { class: 'f-sub', hidden: true },
    dHead,
    content(
      at(0, 0, CW, 264, dHero),
      at(0, 312, CW, 208, h('div', { class: 'fs-oh-facts' }, ...facts)),
      at(0, 544, CW, 24, src),
    ),
  );

  const el = page('fs-overhead', nowView, radarView, listView, detailView);

  function show(v: typeof view) {
    if (v === 'now' || v === 'radar') tabs.forEach((t) => t.set(v));
    view = v;
    nowView.hidden = v !== 'now';
    radarView.hidden = v !== 'radar';
    listView.hidden = v !== 'list';
    detailView.hidden = v !== 'detail';
    el.querySelectorAll<HTMLElement>('.f-content').forEach((c) => (c.scrollTop = 0));
    if (v === 'detail') renderDetail();
  }
  function openAircraft(a: Aircraft) {
    if (view !== 'detail') from = view;
    picked = a.hex;
    show('detail');
  }
  function renderDetail() {
    const list = st.overhead?.aircraft ?? [],
      a = list.find((x) => x.hex === picked);
    if (!a) return; // it left the circle: keep what's shown
    setText(dHead.querySelector('.fs-hlabel')!, a.callsign ?? a.reg ?? a.hex.toUpperCase());
    dHero.set(a, list, st);
    const vals = [
      modelText(a) ?? '--',
      a.reg ?? '--',
      a.operator ?? a.airline ?? '--',
      a.squawk ?? '--',
      a.track === null ? '--' : `${Math.round(a.track)}° ${compass(a.track)}`,
      a.vs === null
        ? '--'
        : Math.abs(a.vs) < 300
          ? 'level'
          : `${a.vs > 0 ? '+' : '−'}${n(Math.round(Math.abs(a.vs) / 100) * 100)} ft/min`,
    ];
    facts.forEach((f, i) => f.set(vals[i]!, i === 3 && a.emergency ? 'warn' : 'dim'));
    src.textContent = `${a.hex.toUpperCase()} · data ${st.overhead?.source ?? '--'}`;
  }
  function render() {
    const d = st.overhead,
      list = d?.aircraft ?? [],
      u = unitsOf(st).distance,
      { phase, top } = overheadNow(d);
    const edge = `${OVERHEAD_RANGE[u].rings.at(-1)} ${u}`;
    el.dataset.state = phase;
    stale.hidden = !(d?.error && list.length);
    hero.hidden = !top;
    emptyBox.hidden = !!top;
    big.set(list, top, u);
    if (!top) {
      setText(
        emptyT,
        phase === 'offline'
          ? 'Feed offline'
          : phase === 'quiet'
            ? 'Clear skies'
            : missing(ctx.store) === 'loading'
              ? 'Loading'
              : 'No data',
      );
      emptyS.textContent =
        phase === 'offline'
          ? (d?.error ?? '').slice(0, 80)
          : phase === 'quiet'
            ? `Nothing flying within ${edge} right now.`
            : '';
      emptyRadar.set([], null, u);
    } else hero.set(top, list, st);
    alsoHd.hidden = more.hidden = list.length < 2;
    also.replaceChildren(...list.slice(1, 3).map((a) => aircraftRow(a, u, () => openAircraft(a))));
    more.set(`all aircraft · ${list.length}`);
    setText(listHd.firstElementChild!, `highest first · within ${edge}`);
    listRows.replaceChildren(...list.map((a) => aircraftRow(a, u, () => openAircraft(a))));
    if (view === 'detail') renderDetail();
  }

  show(view);
  render();
  return {
    el,
    update(s) {
      st = s;
      render();
    },
    enter() {
      show(o.view ?? 'now');
    },
  };
}
