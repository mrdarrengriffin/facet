/**
 * The UI explorer's catalogue: every component, its controls (props), how to render it, the code to create it, and
 * a grid of variants. Everything here builds real @facet/ui components on mock data.
 */
import {
  createMockHouse,
  energy,
  lightning,
  readoutSamples,
  TOASTS,
  TRACKS,
} from '@facet/demo-data';
const readoutNames = () => readoutSamples(Date.now()).map(([k]) => k);
import {
  iconNames,
  icon,
  moonIcon,
  screenIcon,
  screenIconNames,
  weatherIcon,
  WX_ICONS,
  WX_LABEL,
  type WxIcon,
} from '@facet/icons';
import * as F from '@facet/ui';
import type { Tone } from '@facet/ui';

import type { Control, Props } from '../controls';
export type { Control, Props };

export interface Entry {
  id: string;
  name: string;
  group: string;
  description: string;
  controls: Control[];
  render(p: Props): HTMLElement | SVGElement;
  code(p: Props): string;
  variants?(): { label: string; el: HTMLElement | SVGElement }[];
  /** Natural size, for scaling big components to fit the stage (a function when the props change it). */
  size?: [number, number] | ((p: Props) => [number, number]);
  /** Re-render on a timer (live clocks). */
  live?: boolean;
  /**
   * Whether the component makes a sound. Leave it out: the explorer works it out from what the sound hooks pick up
   * (see `hasSound` in @facet/sounds). Set it only for a component that plays sounds itself, or to hide the switch.
   */
  sound?: boolean;
}

const TONES = ['ok', 'warn', 'error', 'working', 'info', 'idle'] as const;
const STATES = F.STATES;
const MOODS = F.MOODS;
const TYPES = [
  'title',
  'card-title',
  'tile',
  'body',
  'label',
  'label-sm',
  'meta',
  'meta-sm',
  'caps-sm',
  'subtitle',
  'date',
  'weather',
  'number',
];
const COLOURS = ['accent', 'steel', 'warn', 'blue', 'text-mute', 'error', 'ok'];
const house = createMockHouse({ live: false });

const sel = (
  key: string,
  label: string,
  options: readonly string[],
  value = options[0]!,
): Control => ({ key, label, type: 'select', options, value });
const txt = (key: string, label: string, value: string): Control => ({
  key,
  label,
  type: 'text',
  value,
});
const rng = (
  key: string,
  label: string,
  min: number,
  max: number,
  value: number,
  step = 1,
): Control => ({ key, label, type: 'range', min, max, step, value });
const tog = (key: string, label: string, value: boolean): Control => ({
  key,
  label,
  type: 'toggle',
  value,
});

/** Format a call like `button({ kind: 'primary', text: 'Start' })`. */
export function call(
  fn: string,
  args: Record<string, unknown> | unknown[] | null,
  extra = '',
): string {
  const v = (x: unknown): string =>
    typeof x === 'string'
      ? `'${x.replace(/'/g, "\\'")}'`
      : Array.isArray(x)
        ? `[${x.map(v).join(', ')}]`
        : x && typeof x === 'object'
          ? obj(x as Record<string, unknown>)
          : String(x);
  const obj = (o: Record<string, unknown>) => {
    const parts = Object.entries(o)
      .filter(([, x]) => x !== undefined && x !== '')
      .map(([k, x]) => `${/^[a-z_$][\w$]*$/i.test(k) ? k : `'${k}'`}: ${v(x)}`);
    return parts.length ? `{ ${parts.join(', ')} }` : '{}';
  };
  const a = args === null ? '' : Array.isArray(args) ? args.map(v).join(', ') : obj(args);
  return `${fn}(${a}${extra})`;
}
const imp = (names: string[], from = '@facet/ui') =>
  `import { ${names.join(', ')} } from '${from}';\n\n`;
const stack = (...kids: (Node | null)[]) => F.h('div', { class: 'ex-stack' }, ...kids);

/** A small fixed-size frame for things that need a screen behind them (bottom sheet, toast layer). */
const framed = (w: number, hh: number, ...kids: Node[]) =>
  F.h('div', { class: 'f-screen ex-mini', style: { width: `${w}px`, height: `${hh}px` } }, ...kids);

const barsData = () => {
  const e = energy(Date.now());
  return e.today!.slots.map((s) => ({ v: s.kwh, c: F.BAND_COLOUR[s.band] }));
};

export const entries: Entry[] = [
  // ---------------------------------------------------------------- atoms
  {
    id: 'label',
    name: 'Label',
    group: 'Atoms',
    description: 'A text label in one of the component type styles.',
    controls: [txt('text', 'Text', 'done · 4h ago'), sel('type', 'Type', TYPES, 'meta')],
    render: (p) => F.label(String(p.text), String(p.type)),
    code: (p) => imp(['label']) + call('label', [p.text, p.type]),
    variants: () => TYPES.map((t) => ({ label: t, el: F.label('Desk lamp', t) })),
  },
  {
    id: 'status-dot',
    name: 'Status dot',
    group: 'Atoms',
    description: 'An 8 px dot in a status tone.',
    controls: [sel('tone', 'Tone', TONES, 'working')],
    render: (p) => F.statusDot(p.tone as Tone),
    code: (p) => imp(['statusDot']) + call('statusDot', [p.tone]),
    variants: () => TONES.map((t) => ({ label: t, el: F.statusDot(t) })),
  },
  {
    id: 'status-chip',
    name: 'Status chip',
    group: 'Atoms',
    description: 'A tinted chip with a dot and caps text: the small way to show status.',
    controls: [sel('tone', 'Tone', TONES, 'working'), txt('text', 'Text', '3 working')],
    render: (p) => F.statusChip(p.tone as Tone, String(p.text)),
    code: (p) => imp(['statusChip']) + call('statusChip', [p.tone, p.text]),
    variants: () =>
      TONES.map((t) => ({
        label: t,
        el: F.statusChip(t, t === 'warn' ? 'needs you' : `sys: ${t}`),
      })),
  },
  {
    id: 'corner-ticks',
    name: 'Corner ticks',
    group: 'Atoms',
    description: 'L-shaped accent ticks on the corners of an active button or card.',
    controls: [sel('corners', 'Corners', ['all', 'tr-bl'])],
    render: (p) => {
      const box = F.h('div', { class: 'ex-box' }, F.cornerTicks(p.corners as 'all'));
      box.querySelector<HTMLElement>('.f-ticks')!.style.display = 'block';
      return box;
    },
    code: (p) => imp(['cornerTicks']) + call('cornerTicks', [p.corners]),
  },
  {
    id: 'focus-ring',
    name: 'Focus ring',
    group: 'Atoms',
    description: 'A 1 px accent ring a few px outside its owner, e.g. on an active Primary button.',
    controls: [rng('offset', 'Offset', 0, 16, 8, 4)],
    render: (p) => {
      const box = F.h('div', { class: 'ex-box' }, F.focusRing(Number(p.offset)));
      box.querySelector<HTMLElement>('.f-ring')!.style.display = 'block';
      return box;
    },
    code: (p) => imp(['focusRing']) + call('focusRing', [p.offset]),
  },
  {
    id: 'mascot',
    name: 'Mascot',
    group: 'Atoms',
    description: 'A 32 × 24 body whose eyes and small movements show a mood. Shown at 1× and 4×.',
    controls: [
      sel('mood', 'Mood', MOODS, 'working'),
      sel('colour', 'Colour', ['(mood)', ...COLOURS]),
      sel(
        'work',
        'Working style',
        F.WORK_VARIANTS.map((w) => w.id),
        'c',
      ),
    ],
    render: (p) => {
      const make = () =>
        F.mascotMood({
          mood: p.mood as F.Mood,
          colour: p.colour === '(mood)' ? undefined : String(p.colour),
          work: p.work as F.WorkVariant,
        });
      return F.h('div', { class: 'ex-row' }, make(), F.h('div', { class: 'ex-zoom4' }, make()));
    },
    code: (p) =>
      imp(['mascotMood']) +
      call('mascotMood', { mood: p.mood, colour: p.colour === '(mood)' ? undefined : p.colour }),
    variants: () =>
      MOODS.map((m) => ({
        label: m,
        el: F.h('div', { class: 'ex-zoom2' }, F.mascotMood({ mood: m })),
      })),
  },
  // ---------------------------------------------------------------- icons
  {
    id: 'icon',
    name: 'Icon',
    group: 'Icons',
    description: 'A UI icon from the master SVGs: 24 grid, 2 px round strokes in the text colour.',
    controls: [sel('name', 'Icon', iconNames, 'timer'), rng('size', 'Size', 16, 96, 48, 8)],
    render: (p) => icon(String(p.name), Number(p.size)),
    code: (p) => imp(['icon'], '@facet/icons') + call('icon', [p.name, p.size]),
    variants: () => iconNames.map((n) => ({ label: n, el: icon(n, 32) })),
  },
  {
    id: 'screen-icon',
    name: 'Screen icon',
    group: 'Icons',
    description:
      'One icon per screen, with a single detail in the accent colour. Used in the docs, the Home cards and screen headers.',
    controls: [
      sel('name', 'Screen', screenIconNames, 'weather'),
      rng('size', 'Size', 16, 96, 48, 8),
    ],
    render: (p) => screenIcon(String(p.name), Number(p.size)),
    code: (p) => imp(['screenIcon'], '@facet/icons') + call('screenIcon', [p.name, p.size]),
    variants: () => screenIconNames.map((n) => ({ label: n, el: screenIcon(n, 32) })),
  },
  {
    id: 'weather-icon',
    name: 'Weather icon',
    group: 'Icons',
    description:
      'Weather icons drawn in code on a 48 grid. Animated, they move a few small parts: rays turn, rain falls, the bolt flashes.',
    controls: [
      sel('id', 'Icon', WX_ICONS, 'thunder-rain'),
      rng('size', 'Size', 32, 192, 144, 16),
      tog('animated', 'Animated', true),
    ],
    render: (p) => weatherIcon(p.id as WxIcon, { size: Number(p.size), animated: !!p.animated }),
    code: (p) =>
      imp(['weatherIcon'], '@facet/icons') +
      call('weatherIcon', [p.id, { size: p.size, animated: p.animated }]),
    variants: () =>
      WX_ICONS.map((id) => ({
        label: WX_LABEL[id],
        el: weatherIcon(id, { size: 48, animated: true }),
      })),
  },
  {
    id: 'moon-icon',
    name: 'Moon phase',
    group: 'Icons',
    description:
      'The moon drawn from its phase on the weather grid: the dark disc, the lit part as one shape (a half circle and half an ellipse) and a 2 px outline.',
    controls: [
      rng('day', 'Day of the month', 0, 29, 10, 1),
      rng('size', 'Size', 32, 384, 144, 16),
      tog('south', 'Southern hemisphere', false),
    ],
    render: (p) => moonIcon(Number(p.day) / F.SYNODIC, { size: Number(p.size), south: !!p.south }),
    code: (p) =>
      imp(['moonIcon'], '@facet/icons') +
      `// frac = 0-1 through the month (moonData().frac)\n` +
      call('moonIcon', [
        Number((Number(p.day) / F.SYNODIC).toFixed(3)),
        { size: p.size, south: p.south },
      ]),
    variants: () =>
      [0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875].map((frac, i) => ({
        label:
          F.MOON_PHASE_TEXT[
            (
              [
                'new_moon',
                'waxing_crescent',
                'first_quarter',
                'waxing_gibbous',
                'full_moon',
                'waning_gibbous',
                'last_quarter',
                'waning_crescent',
              ] as const
            )[i]!
          ],
        el: moonIcon(frac, { size: 48 }),
      })),
  },
  // ---------------------------------------------------------------- buttons
  {
    id: 'button',
    name: 'Button',
    group: 'Buttons',
    description:
      'A 64 px text button in four kinds. Active Primary shows a focus ring, active Secondary its corner ticks.',
    controls: [
      sel('kind', 'Kind', F.BUTTON_KINDS),
      txt('text', 'Label', 'Start'),
      sel('state', 'State', STATES),
      sel('icon', 'Icon', ['(none)', ...iconNames]),
    ],
    render: (p) =>
      F.button({
        kind: p.kind as F.ButtonKind,
        text: String(p.text),
        state: p.state as F.State,
        icon: p.icon === '(none)' ? undefined : String(p.icon),
      }),
    code: (p) =>
      imp(['button']) +
      call('button', {
        kind: p.kind,
        text: p.text,
        state: p.state === 'default' ? undefined : p.state,
        icon: p.icon === '(none)' ? undefined : p.icon,
        onTap: undefined,
      }).replace(' })', ', onTap: () => start() })'),
    variants: () =>
      F.BUTTON_KINDS.flatMap((k) =>
        STATES.map((s) => ({
          label: `${k} · ${s}`,
          el: F.button({
            kind: k,
            text:
              k === 'primary'
                ? 'Start'
                : k === 'secondary'
                  ? 'Cancel'
                  : k === 'outline'
                    ? 'Save'
                    : 'More',
            state: s,
          }),
        })),
      ),
  },
  {
    id: 'icon-button',
    name: 'Icon button',
    group: 'Buttons',
    description: 'Play (96, orange), Round (80) and Square (48) icon buttons.',
    controls: [
      sel('style', 'Style', F.ICON_BUTTON_STYLES),
      sel('icon', 'Icon', iconNames, 'play'),
      sel('state', 'State', STATES),
    ],
    render: (p) =>
      F.iconButton({
        style: p.style as F.IconButtonStyle,
        icon: String(p.icon),
        state: p.state as F.State,
      }),
    code: (p) =>
      imp(['iconButton']) +
      call('iconButton', {
        style: p.style,
        icon: p.icon,
        state: p.state === 'default' ? undefined : p.state,
      }),
    variants: () =>
      F.ICON_BUTTON_STYLES.flatMap((st) =>
        STATES.map((s) => ({
          label: `${st} · ${s}`,
          el: F.iconButton({
            style: st,
            icon: st === 'play' ? 'play' : st === 'round' ? 'next' : 'plus',
            state: s,
          }),
        })),
      ),
  },
  {
    id: 'back-button',
    name: 'Back button',
    group: 'Buttons',
    description: 'The 48 px square back button. The header puts the screen name next to it.',
    controls: [sel('state', 'State', STATES)],
    render: (p) => F.backButton({ state: p.state as F.State }),
    code: () => imp(['backButton']) + "backButton({ onTap: () => device.go('home') })",
    variants: () => STATES.map((s) => ({ label: s, el: F.backButton({ state: s }) })),
  },
  // ---------------------------------------------------------------- controls
  {
    id: 'slider',
    name: 'Slider',
    group: 'Controls',
    description: 'A 48 px tall slider: drag anywhere on it. Changes from elsewhere sweep the knob.',
    controls: [rng('value', 'Value', 0, 100, 42), rng('width', 'Width', 160, 560, 400, 16)],
    render: (p) => F.slider({ width: Number(p.width), value: Number(p.value) }),
    code: (p) =>
      imp(['slider']) +
      call('slider', { width: p.width, value: p.value }).replace(
        ' })',
        ', onChange: (v) => setVolume(v) })',
      ),
  },
  {
    id: 'dial',
    name: 'Dial',
    group: 'Controls',
    description:
      'The light dial: drag the ring, tap ON/OFF. Changing the value here sweeps it, like an update from the light.',
    controls: [
      rng('value', 'Value', 0, 100, 64),
      tog('on', 'On', true),
      txt('caption', 'Caption', 'Brightness'),
    ],
    size: [560, 560],
    render: (p) => {
      const d = F.dial({ value: 0, on: !!p.on, caption: String(p.caption) });
      requestAnimationFrame(() => d.set({ value: p.on ? Number(p.value) : 0, on: !!p.on }, true));
      return d;
    },
    code: (p) =>
      imp(['dial']) +
      call('dial', {
        value: p.value,
        on: p.on,
        caption: p.caption === 'Brightness' ? undefined : p.caption,
      }).replace(' })', ', onChange: (v) => setBrightness(v) })'),
  },
  {
    id: 'arc',
    name: 'Arc',
    group: 'Controls',
    description: 'An arc with a track, a round-capped indicator and an optional knob.',
    controls: [
      rng('value', 'Value', 0, 100, 62),
      rng('size', 'Size', 64, 320, 160, 16),
      tog('knob', 'Knob', true),
    ],
    render: (p) =>
      F.arc({
        size: Number(p.size),
        start: 135,
        end: 45,
        track: { w: 2, c: 'line' },
        ind: { w: 6, c: 'accent' },
        value: Number(p.value),
        knob: p.knob ? { size: 26, border: 4 } : undefined,
      }),
    code: (p) =>
      imp(['arc']) +
      `arc({ size: ${p.size}, start: 135, end: 45, track: { w: 2, c: 'line' }, ind: { w: 6, c: 'accent' }, value: ${p.value}${p.knob ? ', knob: { size: 26, border: 4 }' : ''} })`,
  },
  {
    id: 'disc',
    name: 'Media disc',
    group: 'Controls',
    description:
      'Progress as an orange arc around a record; the blue marker circles while playing.',
    controls: [
      rng('progress', 'Progress', 0, 100, 38),
      tog('playing', 'Playing', true),
      sel('size', 'Size', ['256', '320']),
    ],
    render: (p) =>
      F.disc({ progress: Number(p.progress), playing: !!p.playing, size: Number(p.size) }),
    code: (p) =>
      imp(['disc']) +
      call('disc', { progress: p.progress, playing: p.playing, size: Number(p.size) }),
  },
  {
    id: 'media-visual',
    name: 'Media visuals',
    group: 'Controls',
    description:
      "A cassette, CD, vinyl record or reel-to-reel deck for the Media screen, drawn to the real sizes. They turn at the real speeds while playing, and the tape or tonearm follows the track's progress. Shown whole here; on the screen they run off the left edge.",
    controls: [
      sel(
        'visual',
        'Visual',
        F.MEDIA_VISUALS.filter((v) => v.id !== 'disc').map((v) => v.id),
      ),
      rng('progress', 'Progress', 0, 100, 38),
      tog('playing', 'Playing', true),
      sel(
        'track',
        'Cover',
        TRACKS.map((t) => t.title),
      ),
    ],
    render: (p) => {
      const t = TRACKS.find((x) => x.title === p.track) ?? TRACKS[0]!;
      // centred in the frame: the visuals aren't all 480 wide
      const box = document.createElement('div');
      box.style.cssText =
        'display: flex; align-items: center; justify-content: center; width: 100%; height: 100%';
      box.append(
        F.mediaVisual({
          visual: p.visual as F.MediaVisualProps['visual'],
          progress: Number(p.progress),
          playing: !!p.playing,
          title: t.title,
          artist: t.artist,
          art: t.art,
        }),
      );
      return box;
    },
    code: (p) =>
      imp(['mediaVisual']) +
      call('mediaVisual', {
        visual: p.visual,
        progress: p.progress,
        playing: p.playing,
        title: TRACKS.find((x) => x.title === p.track)?.title,
        art: 'cover.jpg',
      }) +
      `\n\n// on the Media screen\nmediaScreen(ctx, { visual: '${p.visual}' });`,
    variants: () =>
      F.MEDIA_VISUALS.filter((v) => v.id !== 'disc').map((v, i) => {
        const t = TRACKS[i % TRACKS.length]!;
        return {
          label: v.name,
          el: F.mediaVisual({
            visual: v.id as F.MediaVisualProps['visual'],
            progress: 30 + i * 15,
            playing: true,
            title: t.title,
            artist: t.artist,
            art: t.art,
          }),
        };
      }),
    size: [480, 336],
  },
  {
    id: 'tick-ring',
    name: 'Tick ring',
    group: 'Controls',
    description: 'A ring of ticks, the basis of the dial scale, the disc and the clocks.',
    controls: [
      rng('count', 'Ticks', 12, 120, 60, 12),
      rng('range', 'Range', 90, 357, 354, 3),
      rng('size', 'Size', 96, 400, 240, 16),
    ],
    render: (p) =>
      F.tickRing({
        size: Number(p.size),
        count: Number(p.count),
        range: Number(p.range),
        rotation: 270,
        minor: { w: 2, len: 8, c: 'line-hi' },
        major: { stride: 5, w: 2, len: 16, c: 'text-dim' },
      }),
    code: (p) =>
      imp(['tickRing']) +
      `tickRing({ size: ${p.size}, count: ${p.count}, range: ${p.range}, rotation: 270, minor: { w: 2, len: 8, c: 'line-hi' }, major: { stride: 5, w: 2, len: 16, c: 'text-dim' } })`,
  },
  // ---------------------------------------------------------------- screen parts
  {
    id: 'header',
    name: 'Header',
    group: 'Screen parts',
    description:
      'The screen header: a 48 px row at y 32 with an optional back button, the label, right-hand items and the time.',
    controls: [
      txt('label', 'Label', 'Weather'),
      tog('back', 'Back button', true),
      tog('clock', 'Time', true),
      tog('chip', 'Right chip', false),
    ],
    size: [720, 112],
    render: (p) =>
      F.h(
        'div',
        { class: 'f-screen ex-mini', style: { width: '720px', height: '112px' } },
        F.header({
          label: String(p.label),
          onBack: p.back ? () => {} : undefined,
          clock: !!p.clock,
          right: p.chip ? [F.statusChip('working', '2 working')] : [],
        }),
      ),
    code: (p) =>
      imp(['header']) +
      call('header', { label: p.label, clock: p.clock ? undefined : false }).replace(
        / }\)$/,
        p.back ? ", onBack: () => device.go('home') })" : ' })',
      ),
  },
  {
    id: 'card',
    name: 'Home card',
    group: 'Screen parts',
    description:
      'A Home card: screen icon and label, a 72 px preview, one value or a short text, and one line. The dot shows when it matters.',
    controls: [
      txt('label', 'Label', 'Weather'),
      sel('icon', 'Icon', screenIconNames, 'weather'),
      sel('mode', 'Shows', ['value', 'text']),
      txt('value', 'Value / text', '12°'),
      txt('sub', 'Line', 'dry for the next hour'),
      tog('accent', 'Matters now', false),
    ],
    size: [320, 280],
    render: (p) => {
      const c = F.card({
        label: String(p.label),
        icon: String(p.icon),
        preview: weatherIcon('partly-day', { size: 72, animated: true }),
        ...(p.mode === 'text' ? { text: String(p.value) } : { value: String(p.value) }),
        sub: String(p.sub),
      });
      c.set({ accent: !!p.accent });
      return c;
    },
    code: (p) =>
      imp(['card']) +
      call('card', {
        label: p.label,
        icon: p.icon,
        [p.mode === 'text' ? 'text' : 'value']: p.value,
        sub: p.sub,
      }).replace(' })', ", preview, onTap: () => device.go('weather') })"),
  },
  {
    id: 'readout',
    name: 'State readout',
    group: 'Screen parts',
    description:
      'Reports a state and opens nothing: someone in a room, a door open, one temperature, who is home. A box is a button, no box is a readout: no fill, no press, no sound, a tap does nothing. Two share a Home card slot.',
    controls: [
      sel('first', 'Top', readoutNames(), 'occupancy · on'),
      sel('second', 'Bottom', ['none', ...readoutNames()], 'door · open'),
    ],
    size: [320, 280],
    render: (p) => {
      const pick = (n: unknown) => readoutSamples(Date.now()).find(([k]) => k === n)?.[1];
      const src = [pick(p.first), p.second === 'none' ? undefined : pick(p.second)].filter(
        (x) => !!x,
      );
      const slot = F.readoutSlot(src.map((x) => F.readoutFrom(x!)));
      slot.style.width = '100%'; // on Home it takes half the grid; here the stage is the slot
      return slot;
    },
    code: () =>
      imp(['readoutSlot', 'readoutFrom']) +
      "readoutSlot([\n  readoutFrom({ kind: 'binary', label: 'Study', deviceClass: 'occupancy', on: true, since }),\n  readoutFrom({ kind: 'binary', label: 'Garden door', deviceClass: 'door', on: true, since }),\n]);\n// or on Home: store.patch({ readouts: [...] }) and keep 'readouts' in the cards list",
  },
  {
    id: 'tile',
    name: 'Agent tile',
    group: 'Screen parts',
    description: 'An agent: the mascot in its mood, a two-line title and a status row.',
    controls: [
      txt('title', 'Title', 'Photo backup'),
      sel('kind', 'Kind', ['working', 'needs', 'done', 'idle']),
      txt('status', 'Status', 'working · 42% ctx'),
    ],
    size: [208, 208],
    render: (p) =>
      F.tile({ title: String(p.title), kind: p.kind as F.AgentKind, status: String(p.status) }),
    code: (p) => imp(['tile']) + call('tile', { title: p.title, kind: p.kind, status: p.status }),
    variants: () =>
      house.store
        .get()
        .agents!.slice(0, 4)
        .map((a) => ({ label: a.kind, el: F.tile(a) })),
  },
  {
    id: 'hero-block',
    name: 'Hero block',
    group: 'Screen parts',
    description:
      "A screen's hero number with a label and one line beside it. Accent when it matters.",
    controls: [
      txt('n', 'Number', '3'),
      txt('label', 'Label', 'need you'),
      txt('line', 'Line', '2 reviews · 2 assigned'),
      tog('accent', 'Accent', true),
    ],
    size: [656, 96],
    render: (p) => {
      const hb = F.heroBlock({ label: String(p.label) });
      hb.set({ n: String(p.n), line: String(p.line), accent: !!p.accent });
      return F.h('div', { class: 'fs', style: { width: '656px' } }, hb);
    },
    code: (p) =>
      imp(['heroBlock']) +
      `const hero = heroBlock({ label: '${p.label}' });\nhero.set({ n: '${p.n}', line: '${p.line}', accent: ${p.accent} });`,
  },
  {
    id: 'row',
    name: 'Row',
    group: 'Screen parts',
    description:
      'A hairline row: a caps label line with a status on the right, and a one-line title.',
    controls: [
      txt('label', 'Label', 'lantern · #412 · 1d'),
      txt('title', 'Title', 'Add a quiet hours schedule'),
      sel('tone', 'Status tone', ['(none)', ...TONES], 'warn'),
      txt('status', 'Status', 'review'),
    ],
    size: [656, 112],
    render: (p) =>
      F.h(
        'div',
        { class: 'fs', style: { width: '656px' } },
        F.row({
          label: String(p.label),
          title: String(p.title),
          status: p.tone === '(none)' ? null : { tone: p.tone as Tone, text: String(p.status) },
          onTap: () => {},
        }),
      ),
    code: (p) =>
      imp(['row']) +
      call('row', { label: p.label, title: p.title }).replace(
        ' })',
        `${p.tone === '(none)' ? '' : `, status: { tone: '${p.tone}', text: '${p.status}' }`}, onTap: () => openPr() })`,
      ),
  },
  {
    id: 'more-row',
    name: 'More row',
    group: 'Screen parts',
    description: 'A 64 px row that opens the rest: a caps label and a chevron.',
    controls: [txt('text', 'Text', 'all pull requests · 3 more')],
    size: [656, 64],
    render: (p) =>
      F.h('div', { class: 'fs', style: { width: '656px' } }, F.moreRow(String(p.text))),
    code: (p) => imp(['moreRow']) + `moreRow('${p.text}', () => showList())`,
  },
  {
    id: 'stat',
    name: 'Stat',
    group: 'Screen parts',
    description: 'A caps label over one line of body text.',
    controls: [
      txt('label', 'Label', 'trend'),
      txt('value', 'Value', '48 km/h'),
      sel('tone', 'Tone', ['dim', 'warn', 'ok']),
    ],
    render: (p) => {
      const st = F.stat(String(p.label));
      st.set(String(p.value), p.tone as Tone);
      return F.h('div', { class: 'fs', style: { width: '240px' } }, st);
    },
    code: (p) =>
      imp(['stat']) + `const trend = stat('${p.label}');\ntrend.set('${p.value}', '${p.tone}');`,
  },
  {
    id: 'moon-facts',
    name: 'Moon facts',
    group: 'Screen parts',
    description:
      'What moonData() works out, without an API: the one line, how much is lit, the age, moonrise and moonset, the next full and new moon.',
    controls: [rng('ahead', 'Days ahead', 0, 29, 0, 1)],
    render: (p) => {
      const now = Date.now() + Number(p.ahead) * 86_400_000;
      const m = F.moonData(now, { lat: 52.2, lon: -1.2 });
      const l = F.moonLine(m, now);
      const st = (k: string, v: string) => {
        const x = F.stat(k);
        x.set(v, 'text' as Tone);
        return F.h('div', { style: { width: '240px' } }, x);
      };
      const t = (iso?: string | null) => (iso ? F.hm(iso) : '--');
      return F.h(
        'div',
        {
          class: 'fs',
          style: { display: 'grid', gridTemplateColumns: '240px 240px', gap: '32px 16px' },
        },
        F.h(
          'div',
          { style: { gridColumn: '1 / 3', display: 'flex', alignItems: 'center', gap: '16px' } },
          moonIcon(m.frac, { size: 48 }),
          F.h('span', { class: 'fst-title' }, `${l.name} · ${l.rest}`),
        ),
        st('lit', `${Math.round(m.illumination)}%`),
        st('age', `${m.age.toFixed(1)} days`),
        st('moonrise', m.next_rise ? `${F.moonDay(m.next_rise, now)} · ${t(m.next_rise)}` : '--'),
        st('moonset', m.next_set ? `${F.moonDay(m.next_set, now)} · ${t(m.next_set)}` : '--'),
        st('next full', `${F.moonDay(m.next_full, now)} · ${F.hm(m.next_full)}`),
        st('next new', `${F.moonDay(m.next_new, now)} · ${F.hm(m.next_new)}`),
      );
    },
    code: () =>
      imp(['moonData', 'moonLine']) +
      "const moon = moonData(Date.now(), { lat: 52.2, lon: -1.2 });\nconst { name, rest } = moonLine(moon); // 'Waxing gibbous', '78% · full on Fri'",
  },
  {
    id: 'previews',
    name: 'Card previews',
    group: 'Screen parts',
    description: 'The 72 px artwork on the Home cards.',
    controls: [],
    render: () => F.h('div', { class: 'ex-row' }, ...previewSet()),
    code: () =>
      imp([
        'previewAgents',
        'previewMedia',
        'previewDial',
        'previewClock',
        'previewGithub',
        'previewEnergy',
        'previewLightning',
        'previewTimer',
      ]) + 'const art = previewDial(64);',
  },
  // ---------------------------------------------------------------- lists
  {
    id: 'count-badge',
    name: 'Count badge',
    group: 'Lists',
    description: 'A 32 px tinted pill with a number.',
    controls: [txt('n', 'Number', '3'), sel('tone', 'Tone', TONES, 'warn')],
    render: (p) => F.countBadge(String(p.n), p.tone as Tone),
    code: (p) => imp(['countBadge']) + call('countBadge', [Number(p.n) || p.n, p.tone]),
    variants: () =>
      TONES.map((t, i) => ({ label: t, el: F.countBadge([3, 1, 12, 2, 128, 0][i]!, t) })),
  },
  {
    id: 'section-header',
    name: 'Section header',
    group: 'Lists',
    description: 'A 48 px section header: caps title, count badge, meta on the right.',
    controls: [
      txt('title', 'Title', 'Needs you'),
      rng('count', 'Count', 0, 20, 3),
      sel('tone', 'Tone', TONES, 'warn'),
      txt('right', 'Right', 'updated 2m'),
    ],
    size: [688, 48],
    render: (p) =>
      F.h(
        'div',
        { style: { width: '688px' } },
        F.sectionHeader({
          title: String(p.title),
          count: Number(p.count),
          tone: p.tone as Tone,
          right: String(p.right),
        }),
      ),
    code: (p) =>
      imp(['sectionHeader']) +
      call('sectionHeader', { title: p.title, count: p.count, tone: p.tone, right: p.right }),
  },
  {
    id: 'state-chip',
    name: 'State chip',
    group: 'Lists',
    description: 'Chips for pull requests: review state, CI and why it needs you.',
    controls: [sel('kind', 'Kind', Object.keys(F.CHIP), 'review')],
    render: (p) => F.stateChip(p.kind as F.ChipKind),
    code: (p) => imp(['stateChip']) + call('stateChip', [p.kind]),
    variants: () =>
      (Object.keys(F.CHIP) as F.ChipKind[]).map((k) => ({ label: k, el: F.stateChip(k) })),
  },
  {
    id: 'pr-row',
    name: 'Pull request row',
    group: 'Lists',
    description: 'A 144 px row: repo, number and age, a two-line title, and chips.',
    controls: [
      txt('repo', 'Repo', 'hearth-ui'),
      rng('number', 'Number', 1, 9999, 88),
      txt('title', 'Title', 'Card grid: snap scrolling one row at a time'),
      txt('age', 'Age', '5h'),
      tog('draft', 'Draft', false),
      sel('ci', 'CI', ['pass', 'fail', 'pending', 'none']),
    ],
    size: [688, 144],
    render: (p) => {
      const chips: F.ChipKind[] = [
        'review',
        ...(p.draft ? (['draft'] as const) : []),
        ...(p.ci === 'none' ? [] : [`ci-${p.ci}` as F.ChipKind]),
      ];
      return F.h(
        'div',
        { style: { width: '688px' } },
        F.prRow(
          {
            repo: String(p.repo),
            number: Number(p.number),
            title: String(p.title),
            age: String(p.age),
            chips,
          },
          () => {},
        ),
      );
    },
    code: (p) =>
      imp(['prRow']) +
      `prRow({ repo: '${p.repo}', number: ${p.number}, title: '${p.title}', age: '${p.age}', chips: ['review'${p.draft ? ", 'draft'" : ''}${p.ci === 'none' ? '' : `, 'ci-${p.ci}'`}] }, () => openPr())`,
  },
  {
    id: 'check-row',
    name: 'Check row',
    group: 'Lists',
    description: 'A 32 px row for one CI check.',
    controls: [
      txt('name', 'Name', 'test'),
      sel('state', 'State', ['pass', 'fail', 'pending', 'skipped']),
    ],
    size: [336, 32],
    render: (p) =>
      F.h('div', { style: { width: '336px' } }, F.checkRow(String(p.name), p.state as 'pass')),
    code: (p) => imp(['checkRow']) + call('checkRow', [p.name, p.state]),
    variants: () =>
      (['pass', 'fail', 'pending', 'skipped'] as const).map((s) => ({
        label: s,
        el: F.h('div', { style: { width: '240px' } }, F.checkRow('build', s)),
      })),
  },
  {
    id: 'empty-state',
    name: 'Empty state',
    group: 'Lists',
    description: 'A quiet box for "nothing here": an icon in the tone, a title and one line.',
    controls: [
      sel('icon', 'Icon', iconNames, 'done'),
      txt('title', 'Title', 'Nothing needs you'),
      txt('body', 'Body', 'No review requests or assignments'),
      sel('tone', 'Tone', TONES, 'ok'),
    ],
    size: [688, 136],
    render: (p) =>
      F.h(
        'div',
        { style: { width: '688px' } },
        F.emptyState({
          icon: String(p.icon),
          title: String(p.title),
          body: String(p.body),
          tone: p.tone as Tone,
        }),
      ),
    code: (p) =>
      imp(['emptyState']) +
      call('emptyState', { icon: p.icon, title: p.title, body: p.body, tone: p.tone }),
  },
  {
    id: 'qr-block',
    name: 'QR block',
    group: 'Lists',
    description: 'A QR code with a caption, dark on light in both themes so phones can read it.',
    controls: [
      txt('text', 'Text', 'https://example.com/orchard-labs/lantern/pull/412'),
      txt('caption', 'Caption', 'open on your phone'),
    ],
    render: (p) => F.qrBlock({ text: String(p.text), caption: String(p.caption) }),
    code: (p) => imp(['qrBlock']) + call('qrBlock', { text: p.text, caption: p.caption }),
  },
  // ---------------------------------------------------------------- pickers
  {
    id: 'radio',
    name: 'Radio',
    group: 'Pickers',
    description: 'A 24 px radio.',
    controls: [tog('on', 'Selected', true)],
    render: (p) => F.radio(!!p.on),
    code: (p) => imp(['radio']) + call('radio', [p.on]),
  },
  {
    id: 'picker-row',
    name: 'Picker row',
    group: 'Pickers',
    description:
      'A 96 px row for choosing one option, hairline on top: icon, name over a caps status, and a radio.',
    controls: [
      sel('icon', 'Icon', ['speaker', 'speakers', 'tv', 'stream', 'auto'], 'speaker'),
      txt('name', 'Name', 'Kitchen speaker'),
      txt('status', 'Status', 'playing · Northbound · Marlow & Finch'),
      sel('tone', 'Tone', ['dim', 'working']),
      tog('selected', 'Selected', false),
    ],
    size: [656, 96],
    render: (p) =>
      F.h(
        'div',
        { style: { width: '656px' } },
        F.pickerRow({
          icon: String(p.icon),
          name: String(p.name),
          status: String(p.status),
          tone: p.tone as Tone,
          selected: !!p.selected,
          onTap: () => {},
        }),
      ),
    code: (p) =>
      imp(['pickerRow']) +
      call('pickerRow', {
        icon: p.icon,
        name: p.name,
        status: p.status,
        tone: p.tone === 'dim' ? undefined : p.tone,
        selected: p.selected || undefined,
      }),
  },
  {
    id: 'speaker-button',
    name: 'Speaker button',
    group: 'Pickers',
    description:
      "The header button that opens the speaker picker. The icon turns orange while something's playing.",
    controls: [txt('name', 'Name', 'auto · study'), tog('playing', 'Playing', true)],
    render: (p) => F.speakerButton({ name: String(p.name), playing: !!p.playing }),
    code: (p) =>
      imp(['speakerButton']) +
      call('speakerButton', { name: p.name, playing: p.playing }).replace(
        ' })',
        ', onTap: () => sheet.open() })',
      ),
  },
  {
    id: 'bottom-sheet',
    name: 'Bottom sheet',
    group: 'Pickers',
    description:
      'A sheet over the screen with a scrolling list. Tap the button to open it; tap the scrim or Close to dismiss.',
    controls: [txt('title', 'Title', 'speaker')],
    size: [720, 720],
    render: (p) => {
      const sheet = F.bottomSheet({ title: String(p.title) });
      sheet.list.append(
        ...house.store.get().players!.map((pl, i) =>
          F.pickerRow({
            icon:
              pl.kind === 'stream'
                ? 'stream'
                : pl.kind === 'group'
                  ? 'speakers'
                  : pl.kind === 'tv'
                    ? 'tv'
                    : 'speaker',
            name: pl.name,
            status: pl.state,
            selected: i === 0,
            onTap: () => sheet.close(),
          }),
        ),
      );
      const open = F.button({ kind: 'primary', text: 'Open sheet', onTap: () => sheet.open() });
      const el = framed(720, 720, F.h('div', { class: 'ex-center' }, open), sheet);
      setTimeout(() => sheet.open(), 300);
      return el;
    },
    code: (p) =>
      imp(['bottomSheet', 'pickerRow']) +
      `const sheet = bottomSheet({ title: '${p.title}' });\nsheet.list.append(pickerRow({ icon: 'speaker', name: 'Kitchen speaker', status: 'idle', onTap: () => sheet.close() }));\nsheet.open();`,
  },
  {
    id: 'checkbox',
    name: 'Checkbox',
    group: 'Pickers',
    description:
      'A 20 px rounded box: open, in progress (an orange dot) or done (filled orange with a check). Read-only unless it has an onChange.',
    controls: [
      sel('state', 'State', F.CHECK_STATES, 'done'),
      txt('label', 'Label', 'Screens: Home, Media, Light'),
      tog('interactive', 'Interactive', true),
    ],
    render: (p) =>
      F.checkbox({
        state: p.state as F.CheckState,
        label: String(p.label),
        onChange: p.interactive ? () => {} : undefined,
      }),
    code: (p) =>
      imp(['checkbox']) +
      call('checkbox', { state: p.state, label: p.label }).replace(
        ' })',
        p.interactive ? ', onChange: (s) => save(s) })' : ' })',
      ),
    variants: () =>
      F.CHECK_STATES.map((st) => ({
        label: st,
        el: F.checkbox({
          state: st,
          label: st === 'done' ? 'Shipped' : st === 'doing' ? 'In progress' : 'Planned',
        }),
      })),
  },
  // ---------------------------------------------------------------- toasts
  {
    id: 'toast',
    name: 'Toast',
    group: 'Toasts',
    description:
      'A message over any screen: icon, title, body, optional action and a timer bar. Tap or swipe to dismiss.',
    controls: [
      sel('icon', 'Icon', iconNames, 'timer'),
      sel('tone', 'Tone', TONES, 'ok'),
      txt('title', 'Title', 'Pasta is ready'),
      txt('body', 'Body', 'Your 9 min timer has finished.'),
      txt('action', 'Action', ''),
    ],
    size: [720, 152],
    render: (p) => {
      const t = F.toast(
        {
          icon: String(p.icon),
          tone: p.tone as Tone,
          title: String(p.title),
          body: String(p.body),
          action: p.action ? { label: String(p.action) } : undefined,
        },
        { static: true },
      );
      t.style.top = '16px';
      return framed(720, 152, t);
    },
    code: (p) =>
      call('device.toast', {
        icon: p.icon,
        tone: p.tone,
        title: p.title,
        body: p.body,
        action: p.action ? { label: p.action } : undefined,
      }),
    variants: () =>
      TOASTS.slice(0, 5).map((t) => {
        const el = F.toast(t, { static: true });
        el.style.top = '16px';
        return { label: t.name, el: framed(720, 152, el) };
      }),
  },
  {
    id: 'toast-layer',
    name: 'Toast layer',
    group: 'Toasts',
    description:
      'The layer that stacks toasts (two at most) and animates them in and out. Use the button to send one.',
    controls: [sel('position', 'Position', ['top', 'bottom'])],
    size: [720, 720],
    render: (p) => {
      const layer = F.toastLayer({ position: p.position as 'top' });
      let i = 0;
      const send = () => layer.push(TOASTS[i++ % TOASTS.length]!);
      const el = framed(
        720,
        720,
        F.h(
          'div',
          { class: 'ex-center' },
          F.button({ kind: 'primary', text: 'Send a toast', onTap: send }),
        ),
        layer,
      );
      setTimeout(send, 300);
      return el;
    },
    code: (p) =>
      imp(['toastLayer']) +
      `const toasts = toastLayer({ position: '${p.position}' });\nscreen.append(toasts);\ntoasts.push({ icon: 'washer', tone: 'info', title: 'Washing machine done' });`,
  },
  // ---------------------------------------------------------------- charts
  {
    id: 'bar-chart',
    name: 'Bar chart',
    group: 'Charts',
    description:
      "Bars on a baseline, coloured by token, with a 'now' marker. Tap a bar to select it; Replay grows them again.",
    controls: [
      rng('width', 'Width', 240, 656, 480, 16),
      rng('height', 'Height', 64, 200, 120, 8),
      tog('now', 'Now marker', true),
    ],
    render: (p) => {
      const b = F.barChart({
        w: Number(p.width),
        h: Number(p.height),
        bars: barsData(),
        now: p.now ? F.nowIndex(energy(Date.now()).today!.slots) : null,
        onTap: () => {},
      });
      requestAnimationFrame(() => b.play(10, 420));
      return b;
    },
    code: (p) =>
      imp(['barChart']) +
      `const bars = barChart({ w: ${p.width}, h: ${p.height}, bars: slots.map((s) => ({ v: s.kwh, c: BAND_COLOUR[s.band] })) });\nbars.play();`,
  },
  {
    id: 'moon-range',
    name: 'Moon by day',
    group: 'Charts',
    description:
      "The Moon screen's long-range chart: one bar per evening, height = how much of the moon is lit. Full moons in accent, past days dimmed, a dotted line on today, a moon above every new and full moon and month names under the 1st. Tap a bar to pick a day.",
    controls: [
      rng('width', 'Width', 240, 656, 656, 8),
      rng('height', 'Height', 120, 360, 240, 8),
      tog('south', 'Southern hemisphere', false),
    ],
    render: (p) => {
      const w = Number(p.width),
        back = F.MOON_RANGE_BACK;
      const r = F.moonRangeChart({
        w,
        h: Number(p.height),
        days: F.moonDays(Date.now() - back * 86_400_000, Math.floor(w / 8)),
        today: back,
        south: !!p.south,
        onTap: () => {},
      });
      requestAnimationFrame(() => r.chart.play(6, 360));
      const el = document.createElement('div');
      el.style.cssText = `display: flex; flex-direction: column; gap: 16px; width: ${w}px`;
      el.append(r.marks, r.chart, r.axis);
      return el;
    },
    code: (p) =>
      imp(['moonDays', 'moonRangeChart']) +
      `const days = moonDays(Date.now() - 7 * 86_400_000, ${Math.floor(Number(p.width) / 8)}); // 8 px a day\n` +
      `const r = moonRangeChart({ w: ${p.width}, h: ${p.height}, days, today: 7${p.south ? ', south: true' : ''} });\n` +
      `// r.marks (24 high) above r.chart, r.axis (24) below it`,
    size: (p) => [Number(p.width), Number(p.height) + 80], // marks 24 + 16 + chart + 16 + axis 24
  },
  {
    id: 'rate-strip',
    name: 'Rate strip',
    group: 'Charts',
    description: 'Price bands as a strip of runs, with a tick at now.',
    controls: [rng('width', 'Width', 240, 656, 480, 16)],
    render: (p) => {
      const sl = energy(Date.now()).today!.slots;
      return F.rateStrip({
        w: Number(p.width),
        h: 8,
        bands: sl.map((s) => s.band),
        now: F.nowIndex(sl),
      });
    },
    code: (p) =>
      imp(['rateStrip']) + `rateStrip({ w: ${p.width}, bands: slots.map((s) => s.band), now })`,
  },
  {
    id: 'sparkline',
    name: 'Sparkline',
    group: 'Charts',
    description: 'A 2 px line with a dot on the latest value.',
    controls: [sel('c', 'Colour', ['steel', 'blue', 'accent', 'text-dim'])],
    render: (p) =>
      F.sparkline({
        w: 224,
        h: 48,
        values: [320, 300, 420, 380, 900, 1200, 640, 410, 380, 450, 300, 299],
        c: String(p.c),
      }),
    code: (p) => imp(['sparkline']) + `sparkline({ w: 224, h: 48, values, c: '${p.c}' })`,
  },
  {
    id: 'big-number',
    name: 'Big number',
    group: 'Charts',
    description: 'A caps label over a big value and its unit. It counts up when played.',
    controls: [
      txt('label', 'Label', 'today · so far'),
      rng('value', 'Value', 0, 40, 8.69, 0.01),
      txt('unit', 'Unit', 'kWh'),
      rng('decimals', 'Decimals', 0, 3, 2),
    ],
    render: (p) => {
      const b = F.bigNumber({
        label: String(p.label),
        value: Number(p.value),
        decimals: Number(p.decimals),
        unit: String(p.unit),
        width: 320,
      });
      requestAnimationFrame(() => b.play());
      return b;
    },
    code: (p) =>
      imp(['bigNumber']) +
      call('bigNumber', {
        label: p.label,
        value: p.value,
        decimals: p.decimals,
        unit: p.unit,
        width: 320,
      }),
  },
  {
    id: 'delta-chip',
    name: 'Delta chip',
    group: 'Charts',
    description: 'A change against a reference, as a chip with an arrow.',
    controls: [
      rng('pct', 'Change %', -60, 60, -12),
      txt('ref', 'Reference', 'vs yday'),
      tog('upIsGood', 'Up is good', false),
    ],
    render: (p) => F.deltaChip({ pct: Number(p.pct), ref: String(p.ref), upIsGood: !!p.upIsGood }),
    code: (p) =>
      imp(['deltaChip']) +
      call('deltaChip', { pct: p.pct, ref: p.ref, upIsGood: p.upIsGood || undefined }),
  },
  {
    id: 'donut',
    name: 'Donut',
    group: 'Charts',
    description: "One arc per part, from 12 o'clock, with small gaps.",
    controls: [
      rng('a', 'Cheap', 0, 10, 3.2, 0.1),
      rng('b', 'Normal', 0, 10, 4.1, 0.1),
      rng('c', 'Peak', 0, 10, 1.4, 0.1),
      rng('size', 'Size', 96, 240, 160, 16),
    ],
    render: (p) => {
      const d = F.donut({
        size: Number(p.size),
        t: 16,
        parts: [
          { v: Number(p.a), c: 'blue' },
          { v: Number(p.b), c: 'band-normal' },
          { v: Number(p.c), c: 'accent' },
        ],
      });
      requestAnimationFrame(() => d.play());
      return d;
    },
    code: (p) =>
      imp(['donut']) +
      `donut({ size: ${p.size}, t: 16, parts: [{ v: ${p.a}, c: 'blue' }, { v: ${p.b}, c: 'band-normal' }, { v: ${p.c}, c: 'accent' }] })`,
  },
  {
    id: 'legend',
    name: 'Legend and axis',
    group: 'Charts',
    description: 'Legend swatches and axis labels, both on the 8 px grid.',
    controls: [],
    render: () =>
      stack(
        F.legend([
          { c: 'blue', text: 'cheap 13.5p' },
          { c: 'band-normal', text: 'normal 26.2p' },
          { c: 'accent', text: 'peak 39.4p' },
        ]),
        F.axis({
          w: 480,
          ticks: [0, 6, 12, 18, 24].map((hr) => ({
            at: hr / 24,
            text: String(hr).padStart(2, '0'),
          })),
        }),
      ),
    code: () =>
      imp(['legend', 'axis']) +
      "legend([{ c: 'blue', text: 'cheap 13.5p' }, { c: 'accent', text: 'peak 39.4p' }]);\naxis({ w: 480, ticks: [{ at: 0, text: '00' }, { at: 0.5, text: '12' }, { at: 1, text: '24' }] });",
  },
  // ---------------------------------------------------------------- clock
  {
    id: 'time-text',
    name: 'Time',
    group: 'Clock',
    description: 'The time in fixed-width digit cells, so it never shifts as it changes.',
    controls: [
      sel('size', 'Size', ['compact', 'mid', 'hero'], 'mid'),
      sel('font', 'Font', ['mono', 'inter']),
      sel('format', 'Format', ['24h', '12h']),
    ],
    live: true,
    render: (p) => {
      const t = F.timeText({
        size: p.size as 'mid',
        font: p.font as 'mono',
        format: p.format as '24h',
      });
      t.tick(new Date());
      return t;
    },
    code: (p) =>
      imp(['timeText']) + call('timeText', { size: p.size, font: p.font, format: p.format }),
  },
  {
    id: 'date-line',
    name: 'Date line',
    group: 'Clock',
    description: 'Short (MON 19 OCT, mono caps) or long (Monday 19 October).',
    controls: [sel('style', 'Style', ['short', 'long'])],
    render: (p) => {
      const d = F.dateLine({ style: p.style as 'short' });
      d.tick(new Date());
      return d;
    },
    code: (p) => imp(['dateLine']) + call('dateLine', { style: p.style }),
  },
  {
    id: 'clock-face',
    name: 'Clock faces',
    group: 'Clock',
    description: 'The full 720 × 720 faces of the clock family.',
    controls: [
      sel('face', 'Face', ['hero', 'ring', 'analogue', 'context'], 'ring'),
      sel('font', 'Font', ['mono', 'inter']),
      sel('format', 'Format', ['24h', '12h']),
      sel('date', 'Date', ['short', 'long']),
    ],
    size: [720, 720],
    live: true,
    render: (p) => {
      const props = { font: p.font, format: p.format, date: p.date } as F.ClockProps;
      const el =
        p.face === 'hero'
          ? F.clockHero(props)
          : p.face === 'ring'
            ? F.clockRing(props)
            : p.face === 'analogue'
              ? F.clockAnalogue(props)
              : F.clockContext(props, () => house.store.get());
      F.tickClocks(el);
      return el;
    },
    code: (p) =>
      imp([
        p.face === 'hero'
          ? 'clockHero'
          : p.face === 'ring'
            ? 'clockRing'
            : p.face === 'analogue'
              ? 'clockAnalogue'
              : 'clockContext',
      ]) +
      `${p.face === 'hero' ? 'clockHero' : p.face === 'ring' ? 'clockRing' : p.face === 'analogue' ? 'clockAnalogue' : 'clockContext'}({ font: '${p.font}', format: '${p.format}', date: '${p.date}' }${p.face === 'context' ? ', () => store.get()' : ''})`,
  },
  // ---------------------------------------------------------------- radar
  {
    id: 'radar',
    name: 'Lightning radar',
    group: 'Radar',
    description:
      'Strikes by distance (square-root scale) and bearing, coloured by age. New ones flash.',
    controls: [tog('quiet', 'Quiet sky', false), rng('size', 'Size', 256, 512, 384, 128)],
    size: [384, 384],
    render: (p) => {
      const s = Number(p.size),
        k = s / 384;
      const r = F.lightningRadar({
        size: s,
        rings: [
          [10, 64],
          [25, 96],
          [50, 136],
          [100, 192],
        ].map(([km, px]) => [km!, Math.round((px! * k) / 8) * 8]) as [number, number][],
      });
      r.render(lightning(Date.now(), !!p.quiet), true);
      return r.el;
    },
    code: () =>
      imp(['lightningRadar']) +
      'const radar = lightningRadar({ size: 384 });\nradar.render(store.get().lightning, true);',
  },
  // ---------------------------------------------------------------- screens
  {
    id: 'screen',
    name: 'Screens',
    group: 'Screens',
    description:
      'Any whole screen on the mock house, interactive. Each one has its own docs page too.',
    controls: [sel('screen', 'Screen', F.SCREEN_IDS, 'home')],
    size: [720, 720],
    live: false,
    render: (p) => {
      const h = createMockHouse();
      const m = F.mountScreen(p.screen as F.ScreenId, h);
      return Object.assign(m.el, {
        destroy: () => {
          m.destroy();
          h.destroy();
        },
      });
    },
    code: (p) =>
      imp(['mountScreen']) +
      `const { el } = mountScreen('${p.screen}', { store, actions });\ncontainer.append(el);`,
  },
];

function previewSet() {
  const e = F.previewEnergy();
  e.set(energy(Date.now()));
  const l = F.previewLightning();
  l.set(lightning(Date.now()));
  return [
    F.previewAgents(),
    F.previewMedia(),
    F.previewDial(64),
    F.previewClock(),
    F.previewGithub(),
    e,
    l,
    F.previewTimer(65),
  ].map((el) => F.h('span', { class: 'ex-prev' }, el));
}
