/**
 * The screen pages under /docs/screens/. Each one gets a live preview of that screen on the mock house, what it shows,
 * the data it reads (as TypeScript) and how to configure it.
 */
export interface ScreenDoc {
  /** URL slug and preview id. */
  slug: string;
  /** Screen id for the preview (`toasts` is a layer over Home). */
  preview: string;
  /** Screen icon name. */
  icon: string;
  title: string;
  description: string;
  /** Paragraphs about what the screen is for. */
  intro: string[];
  /** What's on it, top to bottom. */
  shows: string[];
  /** How to build it. */
  usage: string;
  /** The data it reads, as TypeScript. */
  data: string;
  /** Configuration notes. */
  config: string[];
}

export const screenDocs: ScreenDoc[] = [
  {
    slug: 'home',
    preview: 'home',
    icon: 'home',
    title: 'Home',
    description: "The room's cards: one value and one line each, four to a view.",
    intro: [
      "Home is where a panel rests. It shows the room's cards two by two, four to a view, and the grid scrolls one row at a time. Each card says one thing: a value or a short text, with one line under it.",
      'A card lights its small orange dot when it matters right now: someone needs you, music is playing, the price is at its peak, lightning is close. Everything else stays quiet.',
    ],
    shows: [
      'The header: the room name on the left, the time on the right. No back button.',
      'Cards of 320 × 280: a screen icon and caps label, a 72 px preview top right, one value (64 px, light) or a two-line text (28 px), and a line of body text.',
      "Tapping a card opens its screen. In this preview the cards are pressable but don't navigate; try the full demo for that.",
    ],
    usage: `import { homeScreen } from '@facet/ui';

const home = homeScreen(ctx, {
  cards: ['media', 'light', 'weather', 'energy'],
  open: (id) => device.go(id),
});`,
    data: `interface PanelData {
  room?: string;            // header label
  agents?: Agent[];
  media?: MediaState | null;
  mediaTarget?: MediaTarget | null;
  light?: LightState | null;
  lightName?: string;
  github?: GithubData | null;
  energy?: EnergyData | null;
  weather?: WeatherData | null;
  lightning?: LightningData | null;
}`,
    config: [
      '`cards` picks which cards show and in what order (`agents`, `media`, `light`, `clock`, `github`, `energy`, `weather`, `lightning`). Four fill one view; more scroll.',
      'Every key in the data is optional. A card with no data shows a dash and "no data" rather than disappearing, so the layout never jumps.',
    ],
  },
  {
    slug: 'agents',
    preview: 'agents',
    icon: 'agents',
    title: 'Agents',
    description: 'Background jobs and assistants as tiles with a mascot, sorted by what needs you.',
    intro: [
      'The Agents screen is for anything that runs in the background and sometimes needs a decision: an assistant working through a task, a backup, a build. The hero is the number that need you.',
      'Each agent gets a tile with the mascot in its mood: working (eyes scanning, progress dots), needs you (wide eyes and an orange "!"), done (a happy hop), idle (asleep, with z\'s drifting up). Tiles sort needs you, then working, done and idle.',
    ],
    shows: [
      'The hero: how many need you (96 px; orange when it\'s more than none), with "need you" and a line of counts beside it.',
      'Tiles of 208 × 208, three across: the mascot, a two-line title, and a status row with the state on the left and the age or detail on the right.',
    ],
    usage: `import { agentsScreen } from '@facet/ui';

const agents = agentsScreen(ctx, { onBack: () => device.go('home') });`,
    data: `interface Agent {
  title: string;
  kind: 'done' | 'working' | 'needs' | 'idle';
  /** "<state> · <detail>", e.g. "working · 31% ctx", "done · 4h ago" */
  status: string;
  /** colour token for the mascot, if not the kind's own */
  mascot?: string;
}

interface PanelData {
  agents?: Agent[];
}`,
    config: [
      'The mood comes from `kind` and `status`: "done" within the last two hours is happy, older is idle; "error" or "failed" in the status makes the mascot droop.',
      'The status splits on " · ": the first part is the state word, the second the detail (" ago" and " ctx" are trimmed to keep it short).',
    ],
  },
  {
    slug: 'media',
    preview: 'media',
    icon: 'media',
    title: 'Media',
    description:
      'Now playing on a spinning disc, with transport controls, live volume and a speaker picker.',
    intro: [
      "The Media screen shows what's playing and where. The disc's orange arc is the track's progress and a small blue marker circles it while it plays.",
      'The speaker button in the header opens a sheet to choose where the controls go: Auto follows whatever is playing, or pick one speaker, group, TV or streaming account.',
      'Instead of the disc, the screen can show a cassette, a CD, a vinyl record or a reel-to-reel deck. Each one is drawn to the real thing\'s size (a C60 cassette, a 120 mm CD, a 12" LP, 7" reels) and turns at its real speed while the track plays: the cassette and reel spools wind the tape across as the track goes on, the CD and record turn with the cover art on them, and the tonearm moves in towards the label. The cassette label gets its colour stripes from the cover. Pick one under Visual below.',
    ],
    shows: [
      'The disc (256) with the state, the title (40 px, up to two lines) and the artist beside it.',
      'Previous, Play/Pause (96, orange; shows pause and a ring while playing) and Next.',
      'The volume slider. It sends changes while you drag (at most every 200 ms, never the same value twice) and sweeps smoothly when the volume changes elsewhere.',
      "With a visual picked: the visual in the disc's place (right edge at x 288, 256 tall), running off the left edge of the screen. The cassette sits 16 px lower than the others.",
    ],
    usage: `import { mediaScreen } from '@facet/ui';

const media = mediaScreen(
  {
    store,
    actions: {
      mediaPlayPause: () => ha.call('media_player', 'media_play_pause'),
      mediaVolume: (v) => ha.call('media_player', 'volume_set', { volume_level: v / 100 }),
      setMediaTarget: (mode, id) => saveTarget(mode, id),
    },
  },
  { visual: 'cassette' }, // 'disc' (default), 'cassette', 'cd', 'vinyl' or 'reel'
);

media.setVisual('vinyl'); // swap it later`,
    data: `interface MediaState {
  state: 'playing' | 'paused' | 'idle' | 'off' | 'unavailable';
  title?: string | null;
  artist?: string | null;
  art?: string | null;        // cover art URL or data URI
  duration?: number | null;   // seconds
  position?: number | null;   // seconds, as of positionAt
  positionAt?: number | null; // ms since the epoch
  volume?: number | null;     // 0-1
}

interface Player {
  id: string;
  name: string;
  state: MediaState['state'];
  kind: 'speaker' | 'group' | 'tv' | 'stream';
  title?: string | null;
  artist?: string | null;
  members?: number;           // speakers in a group
  source?: string | null;     // where a stream is playing
}

interface PanelData {
  media?: MediaState | null;
  players?: Player[];
  mediaTarget?: { mode: 'auto' | 'manual'; id: string; name: string } | null;
}`,
    config: [
      'Actions: `mediaPlayPause`, `mediaNext`, `mediaPrevious`, `mediaVolume(0-100)` and `setMediaTarget(mode, id?)`. Update the store when the player reports back; the screen reflects the store, so optimistic updates belong in the adapter.',
      'While playing, the position moves on from `position` at `positionAt`, so the adapter only needs to report it when it changes.',
      "`art` can be any image URL. To read the cassette's colours from it, the image has to come from the same origin or allow CORS; otherwise the stripes use the accent, blue, warn and steel colours. The mock house has a small made-up cover for each track.",
      'With reduced motion on, nothing spins; the tape and the tonearm still follow the track.',
    ],
  },
  {
    slug: 'light',
    preview: 'light',
    icon: 'light',
    title: 'Light',
    description: 'One light on a dial: drag the ring for brightness, tap the button to switch it.',
    intro: [
      'The Light screen is a dial: a 0-100 scale with an orange arc and knob. Drag around the ring to set the brightness; the centre stays free for the value and the ON/OFF button.',
      "Changes that aren't your own drag sweep the arc to the new value over 400 ms, and opening the screen sweeps up from zero, so the dial always feels physical.",
    ],
    shows: [
      "The header with the light's name.",
      'The dial (560): caption, the value in the display style (144 px, light), the unit and an ON/OFF button.',
    ],
    usage: `import { lightScreen } from '@facet/ui';

const light = lightScreen({
  store,
  actions: {
    lightBrightness: (pct) => ha.call('light', 'turn_on', { brightness_pct: pct }),
    lightToggle: () => ha.call('light', 'toggle'),
  },
});`,
    data: `interface LightState {
  on: boolean;
  brightness: number; // 0-100
}

interface PanelData {
  light?: LightState | null;
  lightName?: string; // header label, e.g. "Desk lamp"
}`,
    config: [
      'Dragging sends `lightBrightness` live, at most every 200 ms plus the final value on release. Brightness 0 means off.',
      "For three seconds after a drag the screen ignores updates from the store, so a slow light doesn't pull the dial back while it catches up.",
    ],
  },
  {
    slug: 'clock',
    preview: 'clock',
    icon: 'clock',
    title: 'Clock',
    description: 'The time as the one hero on the screen, in several faces. Also the idle screen.',
    intro: [
      'The Clock screen is the time and nothing else competing with it: a tick ring with three dots orbiting at different speeds, the time in the display style, the date and the weather.',
      'The clock family has more faces to choose from: Hero, Ring, Analogue and Clock + context (weather, the next event and a running timer). After a while without a touch the panel shows a clock as its idle screen.',
    ],
    shows: [
      'Orbit (the default face): the ring, time (144 px, light), date in caps and the weather line.',
      'The other faces use JetBrains Mono digits in fixed cells, so the time never shifts sideways as it changes.',
    ],
    usage: `import { clockScreen } from '@facet/ui';

const clock = clockScreen(ctx, {
  face: 'context', // 'orbit' | 'hero' | 'ring' | 'analogue' | 'context'
  props: { font: 'mono', format: '24h', date: 'short' },
});`,
    data: `interface PanelData {
  weather?: WeatherData | null; // weather line, and "Outside" on the context face
  nextEvent?: {                 // "Next" on the context face
    summary: string;
    start: string;
    end: string;
    allDay: boolean;
  } | null;
  timers?: { label: string; endsAt: number; total: number }[];
}`,
    config: [
      'Faces: `orbit`, `hero`, `ring`, `analogue`, `context`. Pick one, and the font, format and date style, in the options under the preview.',
      'Font `mono` (default) or `inter`, format `24h` (default) or `12h`, date `short` ("MON 19 OCT") or `long` ("Monday 19 October").',
      'Seconds move with one small rotating or scaling element per clock, re-synced to the wall clock every second.',
    ],
  },
  {
    slug: 'github',
    preview: 'github',
    icon: 'github',
    title: 'GitHub',
    description:
      'Pull requests that need you, your own open ones, and a QR code to carry on on your phone.',
    intro: [
      'The GitHub screen answers one question: does anything need me? The hero is the count of pull requests waiting on you (review requests and assignments), with the top three below it.',
      '"All pull requests" opens the full list in two sections, and any row opens the detail view: branch, author, size, chips for review and CI, the first lines of the description, the checks, and a QR code to open it on your phone.',
    ],
    shows: [
      'Summary: the hero, then three hairline rows (repo · number · age, a status on the right, the title) and a "more" row.',
      'List: Needs you and Your PRs, each with a count badge, as 144 px rows with chips.',
      'Detail: title, meta, chips, description, QR block and up to 14 checks. Back steps out one level at a time.',
    ],
    usage: `import { githubScreen } from '@facet/ui';

const github = githubScreen({
  store,
  actions: { refreshGithub: () => fetchPullRequests().then((g) => store.patch({ github: g })) },
});`,
    data: `interface PullRequest {
  id: string;
  repo: string;      // short name
  repoFull: string;  // owner/name
  number: number;
  title: string;
  url: string;       // shown as a QR code
  author: string;
  created: string;
  updated: string;
  draft: boolean;
  head: string;
  base: string;
  additions: number;
  deletions: number;
  files: number;
  body: string;
  review: 'approved' | 'changes' | 'pending';
  ci: 'pass' | 'fail' | 'pending' | 'none';
  checks: { name: string; state: 'pass' | 'fail' | 'pending' | 'skipped' }[];
  reason?: 'review' | 'assigned' | 'both';
}

interface GithubData {
  at: string;
  needs: PullRequest[];
  mine: PullRequest[];
  counts: { review: number; assigned: number; needs: number; mine: number };
  error?: string;
  stale?: boolean;
}`,
    config: [
      '`refreshGithub` is called when the screen opens. Keep the fetching (and the token) in your adapter; the screen never talks to GitHub.',
      "The QR block stays dark on light in both themes, because that's what scanners expect.",
    ],
  },
  {
    slug: 'energy',
    preview: 'energy',
    icon: 'energy',
    title: 'Energy',
    description: "Today's cost so far, the price band you're in, and today's half-hours by price.",
    intro: [
      "The Energy screen leads with what you've spent today and keeps the rest to one line each: how much you've used against this time yesterday, and the current price with when it changes.",
      "One chart shows today's half-hours, coloured by price band (cheap blue, normal grey, peak orange), with a strip of the bands below and an hour axis. Tap a bar to read it.",
    ],
    shows: [
      "The label (or the tapped half-hour's reading), the hero in pounds (96 px), the use line and the rate line with a band dot.",
      'Live power in the header.',
      'Bars that grow from the baseline when the screen opens.',
    ],
    usage: `import { energyScreen } from '@facet/ui';

const energy = energyScreen(ctx);`,
    data: `type Band = 'cheap' | 'normal' | 'peak';

interface EnergyData {
  today?: {
    kwh: number | null;
    cost: number | null; // pounds
    slots: { start: string; kwh: number | null; rate: number; cost: number | null; band: Band }[];
  };
  rate?: { now: number | null; band: Band | null; until: string | null; next: number | null; next_band: Band | null };
  power?: { w: number | null } | null;
  yesterday_to_now?: number | null;
  error?: string;
}`,
    config: [
      "Slots are half-hours from midnight; slots that haven't happened yet have `kwh: null` and draw empty.",
      'Prices are shown in pounds and pence for now. Band colours always come with a label, never colour alone.',
    ],
  },
  {
    slug: 'weather',
    preview: 'weather',
    icon: 'weather',
    title: 'Weather',
    description:
      'The temperature, one line, rain in the next hour, and the next eight hours. Five days behind a tap.',
    intro: [
      "The Weather screen shows the temperature as the hero with an animated icon beside it, the condition, one line (feels like, high and low) and whether it'll rain in the next hour.",
      'Below is a row of the next eight hours. "Next 5 days" opens a day-by-day view.',
    ],
    shows: [
      'Temperature (144 px, light) and a 144 px icon that animates: rays turn, clouds drift, rain falls.',
      'The hourly row: hour, a 32 px icon and the temperature.',
      'The 5-day view: day, icon, condition with rain, low and high.',
    ],
    usage: `import { weatherScreen } from '@facet/ui';

const weather = weatherScreen(ctx);`,
    data: `interface WeatherData {
  current?: {
    condition: string; // Home Assistant names: sunny, partlycloudy, rainy ...
    temp: number | null;
    feels: number | null;
    humidity: number | null;
    wind: number | null;
    wind_bearing: number | null;
    uv: number | null;
    unit: string;
    wind_unit: string;
  };
  sun?: { next_rising: string; next_setting: string; up: boolean } | null;
  next_hour?: { steps: { t: string; mm: number }[] } | null;
  next_rain?: { t: string; mm: number } | null;
  hourly?: { t: string; condition: string; temp: number; precip: number }[];
  daily?: { t: string; condition: string; hi: number; lo: number | null; precip: number }[];
}

interface PanelData {
  weather?: WeatherData | null;
  place?: string; // shown in the header
}`,
    config: [
      "Conditions use Home Assistant's names, so a weather entity's forecast maps across directly. Sunrise and sunset pick day or night icons.",
      'The icon animation only moves a few small parts and stops with reduced motion.',
    ],
  },
  {
    slug: 'lightning',
    preview: 'lightning',
    icon: 'lightning',
    title: 'Lightning',
    description: 'The nearest strike, the trend and a radar of the last hour.',
    intro: [
      'The Lightning screen shows how close a storm is and whether it\'s coming your way. The hero is the nearest strike of the last 15 minutes, or "Quiet".',
      'The radar uses a square-root distance scale, so the near rings (10 and 25 km) get room. New strikes flash, and dots fade from white to orange to grey as they age.',
    ],
    shows: [
      'Left: the nearest strike (96 px; orange within 25 km), its direction, and three stats (strikes in the last 15 minutes, the trend, the last strike).',
      'Right: the radar (384) with rings at 10, 25, 50 and 100 km, the 25 km ring in orange, and a ring around the nearest strike.',
    ],
    usage: `import { lightningScreen } from '@facet/ui';

const lightning = lightningScreen(ctx);`,
    data: `interface LightningData {
  radius: number; // km covered
  strikes: { id: string; d: number; az: number; t: number }[]; // km, bearing, ms; newest first
  counts: { m5: number; m15: number; m60: number; near25: number };
  nearest: { d: number; az: number; dir: string; t: number } | null;
  last: number | null;
  trend: { state: 'approaching' | 'receding' | 'steady' | 'none'; kmh: number };
  storm: boolean;
}`,
    config: [
      'Work out the counts, nearest strike and trend in your adapter; the screen only draws them.',
      'The preview runs a fictional storm that crosses the area once an hour.',
    ],
  },
  {
    slug: 'timer',
    preview: 'timer',
    icon: 'timer',
    title: 'Timer',
    description: 'A running timer as the hero, with one-tap presets when nothing is running.',
    intro: [
      'The Timer screen shows the time left in the display style, inside a ring that empties as it runs. Two buttons add a minute or cancel.',
      'With nothing running it offers one-tap presets. When a timer finishes, the panel shows a toast too.',
    ],
    shows: [
      "The timer's name, the time left (144 px, light) and when it ends, inside a 432 ring.",
      'Buttons: +1 min and Cancel while running; presets otherwise.',
    ],
    usage: `import { timerScreen } from '@facet/ui';

const timer = timerScreen(ctx, { presets: [3, 5, 10, 20] });`,
    data: `interface Timer {
  label: string;
  endsAt: number; // ms since the epoch
  total: number;  // seconds
}

interface PanelData {
  timers?: Timer[]; // the first one shows
}`,
    config: [
      'Actions: `startTimer(seconds, label)`, `extendTimer(seconds)` and `cancelTimer()`.',
      '`presets` sets the buttons offered when nothing is running, in minutes.',
    ],
  },
  {
    slug: 'toasts',
    preview: 'toasts',
    icon: 'toasts',
    title: 'Toasts',
    description:
      'Short messages over any screen: a timer finishing, someone at the door, an agent that needs you.',
    intro: [
      'Toasts appear over whatever screen is showing. Up to two stack at once; a third pushes the oldest out. Each one has an icon in a tinted square, a title, a line of body text and an optional action.',
      'A thin bar along the bottom shows how long it has left (six seconds by default) and pauses while you press. Tap a toast to dismiss it, or swipe it sideways.',
    ],
    shows: [
      'Toasts of 688 × 96 entering from the top, under the header (or from the bottom).',
      'Use the buttons under the preview to send some.',
    ],
    usage: `import { createDevice } from '@facet/ui';

const device = createDevice(ctx, { toastPosition: 'top' });
device.toast({
  icon: 'timer',
  tone: 'ok',
  title: 'Pasta is ready',
  body: 'Your 9 min timer has finished.',
});`,
    data: `interface ToastData {
  icon: string; // any UI icon name
  tone: 'ok' | 'warn' | 'error' | 'working' | 'info' | 'idle';
  title: string;
  body?: string;
  action?: { label: string; href?: string };
  duration?: number; // ms, default 6000
}`,
    config: [
      'Position `top` (default) or `bottom`; up to `toast.max` (2) at once; swipe 64 px to dismiss. All of these are tokens.',
      'The toast layer plays the Notify sound when a toast shows, if sounds are on.',
    ],
  },
];
