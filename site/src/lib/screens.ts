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
      'The Weather card is contextual: one card for the weather, the moon and lightning. A storm nearby takes it over (the mini radar, "8 km", "NW · 12 strikes in 15 min", in orange), then rain within the hour ("rain in 20 min"), then the moon at night ("clear · waxing gibbous 62%"); in the day it shows the animated weather icon. Try every state with "Weather card" under the preview.',
      'Some things only report a state: someone in the study, the garden door open, the shed temperature, who is home. These are state readouts, and the rule is simple: a box is a button, no box is a readout. A readout sits straight on the background with no fill, no press, no sound and no chevron, and a tap does nothing. Two share one card slot, split by a hairline, so the grid keeps its rhythm. The state says how long ("Open · for 4 min"); an open door or window gets the orange dot, a value out of range turns amber, an offline sensor goes grey.',
    ],
    shows: [
      'The header: the room name on the left, the Settings button and the time on the right. No back button.',
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
  lightning?: LightningData | null; // the Weather card's storm state
  moon?: MoonData | null;           // the Weather card's night state
  readouts?: ReadoutSource[] | null; // state readouts, two per slot
}`,
    config: [
      '`cards` picks which cards show and in what order (`timer`, `calendar`, `agents`, `media`, `light`, `thermostat`, `covers`, `switch`, `readouts`, `clock`, `github`, `energy`, `weather`). Four fill one view; more scroll. The Timer card comes first while a timer runs; otherwise it shows \`No timer\` after Clock, so a timer can always be started.',
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
  { visual: 'cassette' }, // 'cd' (default), 'disc', 'cassette', 'vinyl' or 'reel'
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
    slug: 'thermostat',
    preview: 'thermostat',
    icon: 'thermostat',
    title: 'Thermostat',
    description:
      'A heating ring: the room temperature as the hero, the set point as the knob, orange while it heats. Modes and presets underneath.',
    intro: [
      'The ring is the whole screen. The big number is how warm the room is now; the knob on the ring is what you asked for; while the boiler runs, the arc between the two glows orange (blue while cooling), so "is it heating?" reads from across the room.',
      'Drag the ring or tap − / + to change the set point. The change goes out when you let go (− / + wait 800 ms for more taps), and for three seconds the screen ignores updates so a slow thermostat cannot pull the knob back.',
      'The quiet line in the middle is the schedule\'s next change ("17° from 22:30"), or the preset when there is no schedule.',
    ],
    shows: [
      'The header with the zone name.',
      'The ring (480): a tick a degree, longer every 5°; the measured temperature as a long tick; the set point as the knob.',
      'The centre: what it is doing (heating, idle, off), the room temperature (display 144, light), the set point, the next change and − / +.',
      'Under the ring: up to three mode buttons (the current one marked) and a Presets button that opens a sheet.',
    ],
    usage: `import { thermostatScreen } from '@facet/ui';

const thermostat = thermostatScreen({
  store,
  actions: {
    climateTarget: (t) => ha.call('climate', 'set_temperature', { temperature: t }),
    climateMode: (m) => ha.call('climate', 'set_hvac_mode', { hvac_mode: m }),
    climatePreset: (p) => ha.call('climate', 'set_preset_mode', { preset_mode: p }),
  },
});`,
    data: `interface ThermostatData {
  name?: string | null;          // header, e.g. "Hallway"
  current: number | null;        // current_temperature
  target: number | null;         // temperature; null when off
  min: number;                   // min_temp
  max: number;                   // max_temp
  step: number;                  // target_temp_step
  mode: HvacMode;                // hvac_mode
  modes: HvacMode[];             // hvac_modes (the first three get buttons)
  action?: 'heating' | 'cooling' | 'idle' | 'off' | null; // hvac_action
  preset?: string | null;        // preset_mode
  presets?: string[];            // preset_modes
  humidity?: number | null;
  next?: { at: string; target: number } | null; // the schedule's next change
}`,
    config: [
      "Everything maps to a Home Assistant `climate` entity's attributes. `hvac_action` is what lights the arc; without it the ring never glows.",
      "`next` is optional: few thermostats publish their schedule, so fill it from a schedule helper or the thermostat's own integration when there is one.",
      'The mock house has a made-up hallway thermostat with a schedule (17° overnight, 20.5° mornings and evenings) and a room that warms 0.1° a minute while it heats.',
    ],
  },
  {
    slug: 'switch',
    preview: 'switch',
    icon: 'switch',
    title: 'Switch',
    description:
      'The simplest screen: one big round button with the state inside. Tap to flip it; the power draw and how long it has been on underneath.',
    intro: [
      'One thing, on or off: a smart plug, a fan, a light without a dimmer, a helper. The button is the hero and the control at once, so the state reads from across the room: filled orange when on, an outline when off.',
      'Tap anywhere on it. The new state shows straight away and holds for three seconds while the device answers, with "turning on" as the line until it does; if it still disagrees after that, the device wins.',
      'Because it needs nothing but an on and an off, it works as the fallback screen for any entity that has no screen of its own.',
    ],
    shows: [
      'The header with the name of the thing it switches.',
      'The button (384, round): its icon (power, or a bulb for a light) and the state in the hero type.',
      'One quiet line: the power draw while on, if it measures it, and how long it has been in this state ("212 W · for 2 h 14 min").',
      'Unavailable: the button dims, shows "--" and does nothing.',
    ],
    usage: `import { switchScreen } from '@facet/ui';

const plug = switchScreen({
  store,
  actions: {
    switchSet: (on) => ha.call('homeassistant', on ? 'turn_on' : 'turn_off', { entity_id: 'switch.dehumidifier' }),
  },
});`,
    data: `interface SwitchData {
  name?: string | null;          // header, e.g. "Dehumidifier"
  kind?: 'switch' | 'plug' | 'light' | 'fan' | null; // the icon
  on: boolean | null;            // state; null = unavailable
  power?: number | null;         // W, from the plug's power sensor
  changedAt?: string | null;     // last_changed
}`,
    config: [
      "`on` is the entity's state (`on` / `off`; `unavailable` or `unknown` = null). `homeassistant.turn_on` / `turn_off` work for every domain that has them, so one action covers switches, lights, fans and input booleans.",
      "`power` is optional and usually a separate sensor (a smart plug's `sensor.*_power`). Leave it out and the line only says how long.",
      'The button size is the smaller of the `switch-btn` token (384) and what the content box leaves, so it fits smaller screens without its own layout.',
      'The mock house has a dehumidifier on a smart plug in the study, about 210 W while it runs, that takes 0.6 s to answer.',
    ],
  },
  {
    slug: 'covers',
    preview: 'covers',
    icon: 'covers',
    title: 'Blinds',
    description:
      'Blinds and curtains: the window is the hero, drawn for the kind of cover. Drag it to a position; Open, Stop and Close beside it, and a tilt slider for Venetian blinds.',
    intro: [
      'The window on the left is the screen. It is drawn for what hangs in it: a roller blind drops its fabric from the rail, a Venetian blind lowers a run of slats, and curtains part from the middle with their pleats bunching up. The accent marks the part you drag: the bottom bar of a blind, the leading edges of the curtains.',
      'Drag a blind down or up, and curtains sideways. The position goes out when you let go, and for three seconds the screen ignores updates so a slow motor cannot pull the drawing back. While the cover moves, a thin line marks where it is heading.',
      'Venetian blinds that tilt get a Slats slider: the slats get thinner as they open and close up into a wall as they shut.',
      'With more than one cover, an "N windows" button in the header opens a sheet to pick one.',
    ],
    shows: [
      "The header with the cover's name, and the windows button when there are several.",
      'The window (320 × 576): frame, glass and the cover, at its position.',
      'On the right: the state (closed, opening, 42% open), the position as the hero (96), what kind of cover it is, the Slats slider (Venetian) and Open / Stop / Close.',
    ],
    usage: `import { coversScreen } from '@facet/ui';

const blinds = coversScreen({
  store,
  actions: {
    coverPosition: (id, p) => ha.call('cover', 'set_cover_position', { entity_id: id, position: p }),
    coverTilt: (id, t) => ha.call('cover', 'set_cover_tilt_position', { entity_id: id, tilt_position: t }),
    coverCommand: (id, c) => ha.call('cover', \`\${c}_cover\`, { entity_id: id }),
  },
});`,
    data: `interface CoverData {
  id: string;                    // entity_id
  name: string;                  // friendly_name
  kind?: 'curtain' | 'roller' | 'venetian' | null; // how to draw it (else from deviceClass / features)
  deviceClass?: string | null;   // device_class
  state: 'open' | 'closed' | 'opening' | 'closing' | 'unavailable';
  position: number | null;       // current_position, 0 = closed
  tilt?: number | null;          // current_tilt_position
  features?: number;             // supported_features (default 15)
}

interface PanelData {
  covers?: CoverData[] | null;   // in the order the picker lists them
}`,
    config: [
      'Everything maps to Home Assistant `cover` entities. The drawing comes from `device_class`: `curtain` draws curtains, anything that can tilt (feature 128) draws a Venetian blind, everything else (shade, blind, shutter, awning ...) a roller blind. Set `kind` to override it.',
      'Buttons follow `supported_features`: no Stop button without 8, no dragging without 4 (the drawing then shows fully open or closed from the state).',
      'The Home card shows the first cover and how many of the rest are open; its dot lights while any of them moves.',
      'The mock house has a study roller blind, lounge curtains and a kitchen Venetian blind, with motors that move about 5 % a second.',
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
      '"All pull requests" opens the full list in two sections, and any row opens the detail view: branch, title, author and size, a status line for review and CI, the first lines of the description, the checks, and a QR code to open it on your phone.',
    ],
    shows: [
      'Summary: the hero, then three hairline rows (repo · number · age, a status on the right, the title) and a "more" row.',
      'List: Need you and Your PRs, each under a caps section label with its count (a dot when something needs you), as the same 112 px hairline rows.',
      'Detail: a 400 px left column (repo and branches, a two-line title, author and size, status dots, three lines of description), the QR block on the right, and up to 10 checks in two columns, failing first. Back steps out one level at a time.',
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
    description:
      "Today's cost so far, the price band you're in, and today's half-hours by price. The last 7 days behind a tap.",
    intro: [
      "The Energy screen leads with what you've spent today and keeps the rest to one line each: how much you've used against this time yesterday, and the current price with when it changes.",
      "One chart shows today's half-hours, coloured by price band (cheap blue, normal grey, peak orange), with a strip of the bands below and an hour axis. Tap a bar to read it.",
      '"Last 7 days" opens the week: its cost as the hero, use per day against the week before, and a pair of bars for each day (the week before dashed, this week solid, today last). Tap a bar to read that day.',
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
  days?: { date: string; kwh: number | null; cost: number | null }[]; // oldest first, today last
  error?: string;
}`,
    config: [
      "Slots are half-hours from midnight; slots that haven't happened yet have `kwh: null` and draw empty.",
      '`days` = daily totals for the last 14 days with today last (partial). The week view compares the average per complete day, so gaps and a part-day today stay fair; a week before with missing days says how many it has.',
      'Prices are shown in pounds and pence for now. Band colours always come with a label, never colour alone.',
    ],
  },
  {
    slug: 'weather',
    preview: 'weather',
    icon: 'weather',
    title: 'Weather',
    description:
      'The weather, the sun, the moon and lightning in one screen. Home shows one Weather card that changes with what matters now.',
    intro: [
      'The Weather screen has four views, picked by a row of buttons under the header: Weather · Sun · Moon · Lightning. The Lightning button is only a bolt until a strike is near; then it shows the distance in orange, and opening Weather during a storm lands on Lightning.',
      'Weather: the temperature as the hero with an animated icon beside it, the condition, one line (feels like, high and low), whether it\'ll rain in the next hour, and a row of the next eight hours. "Next 5 days" opens a day-by-day view.',
      "Sun: the next sunrise or sunset as the hero with how long until it, the day's arc with the sun on it, the day length, how it changed since yesterday, and first and last light. Worked out on the device with `sunData(now, { lat, lon })`, no API.",
      'Moon and Lightning are the [Moon](../moon/) and [Lightning](../lightning/) screens as views (both still work as screens of their own).',
      'Home\'s Weather card is contextual, highest first: a storm nearby (the mini radar and the distance, in orange) > rain within the hour > night (the moon phase) > day (the animated icon). Every state is under "State" below the preview.',
    ],
    shows: [
      'The view buttons (48 tall), then the view from 72 px down.',
      'Weather: temperature (144 px, light) and a 144 px icon that animates; the hourly row: hour, a 32 px icon and the temperature; the 5-day view: day, icon, condition with rain, low and high.',
      'Sun: "sunset" over the time (96 px) and "in 2 h 14 min" on the left; the arc (320) with a 24 px sun on it while it is up, the horizon and the rise and set times on the right; four stats under them.',
      'Moon: the moon (304), the phase line and Details. Lightning: the nearest strike, three stats and the radar (384).',
    ],
    usage: `import { moonData, sunData, weatherScreen, weatherState } from '@facet/ui';

store.patch({ sun: sunData(Date.now(), here), moon: moonData(Date.now(), here) });
const weather = weatherScreen(ctx, { view: 'weather' }); // or 'sun' | 'moon' | 'lightning'
weatherState(store.get()); // 'storm' | 'rain' | 'night' | 'day' | 'loading' | 'offline'`,
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

interface SunData {
  rise: string | null; // today; null = the sun doesn't rise or set
  set: string | null;
  dawn: string | null; // first and last light (sun 6° below the horizon)
  dusk: string | null;
  length: number | null; // ms
  delta: number | null;  // ms vs yesterday
  next_rise: string | null; // tomorrow's sunrise
}

interface PanelData {
  weather?: WeatherData | null;
  sun?: SunData | null;
  moon?: MoonData | null;           // the Moon view
  lightning?: LightningData | null; // the Lightning view and the storm state
  place?: string; // shown in the header
}`,
    config: [
      "Conditions use Home Assistant's names, so a weather entity's forecast maps across directly. `sun.up` picks day or night icons and the card's night state.",
      'A storm = a strike within 25 km, or one closing in (`stormNear`). Rain soon = a 15-minute step of the next hour with 0.1 mm or more (`rainIn`).',
      'Call `sunData` and `moonData` from your adapter every few minutes; they only need the home location.',
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
      'On Home it is the Lightning view of the [Weather](../weather/) screen: a nearby storm takes over the Weather card and lands Weather on this view. `lightningParts()` builds it into any screen.',
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
    slug: 'moon',
    preview: 'moon',
    icon: 'moon',
    title: 'Moon',
    description:
      'The moon tonight, drawn from its phase, and one line: the phase, how much is lit, the next full or new moon.',
    intro: [
      'The Moon screen is one picture and one line. The hero is the moon itself, drawn from the phase: the lit part grows from the right while it waxes (mirrored south of the equator).',
      'Under it: "Waxing gibbous · 78% · full on Fri". Details opens the numbers: how much is lit, the age, the next moonrise and moonset, the next full and new moon, and the next four phases.',
      'Everything is worked out on the device, no API: `moonData(now, { lat, lon })` gives phase times to about a minute and rise and set times to a few minutes.',
      'On Home it is the Moon view of the [Weather](../weather/) screen, and the Weather card shows the phase at night. `moonParts()` builds it into any screen.',
    ],
    shows: [
      'The moon (384), centred. The lit part is one shape: a half circle and half an ellipse.',
      'One line (28 px): the phase name, then how much is lit and when the next full or new moon is, dimmed.',
      'Details: six stats (lit, age, the next moonrise and moonset, next full, next new) and the next four phases with 48 px glyphs.',
      'Next 75 days (from details): one bar per evening for 82 days, 7 before today, each as tall as the moon is lit. Full moons in accent, a 24 px moon above every new and full moon, month names under the 1st. The line on top says tonight; tap a bar for that day. `moonRangeChart()` on `moonDays()`.',
    ],
    usage: `import { moonData, moonScreen } from '@facet/ui';

store.patch({ moon: moonData(Date.now(), { lat: 52.2, lon: -1.2 }) });
const moon = moonScreen(ctx);`,
    data: `interface MoonData {
  phase: MoonPhase;     // Home Assistant's moon states: 'new_moon' … 'waning_crescent'
  frac: number;         // 0-1 through the month: 0 new, 0.25 first quarter, 0.5 full
  illumination: number; // 0-100
  age: number;          // days since the new moon
  next_new: string;
  next_full: string;
  upcoming?: { phase: 'new_moon' | 'first_quarter' | 'full_moon' | 'last_quarter'; t: string }[];
  rise?: string | null; // today's moonrise, null when it doesn't rise today
  set?: string | null;
  next_rise?: string | null; // the next ones from now, for the details view
  next_set?: string | null;
  south?: boolean;      // mirror the glyph
}`,
    config: [
      "Call `moonData` from your adapter every few minutes. Home Assistant's `moon` sensor only gives the phase name, so the rest comes from the calculation.",
      'Without a location there are no rise and set times, and the glyph assumes the northern hemisphere.',
      'The glyph on its own: `moonIcon(frac, { size })` from @facet/icons.',
    ],
  },
  {
    slug: 'calendar',
    preview: 'calendar',
    icon: 'calendar',
    title: 'Calendar',
    description:
      'The agenda for the next two weeks: the next event as the hero, then the days, each with its events.',
    intro: [
      'The Calendar screen answers "what\'s next?" from across the room. The hero is the next event: when it starts as the big number ("16:50" today, "Now" while it runs, otherwise the day), with its title and one quiet line.',
      'Below it, the days. Each has a caps label ("Today · Tue 20 Oct") and a row per event: the time, the calendar\'s dot and the title. Finished events go dim, and an event that lasts several days shows on each of them.',
      'With a `setCalendar` action, the header gets an "N calendars" button. It opens a sheet where each calendar can be shown or hidden, so the bin collections or a work calendar can stay off the wall.',
      "Agenda | Month in the header switches to a month grid: six weeks of day cells, each with a dot per event in its calendar's colour. Tap a day for its events; the arrows move a month at a time and Today comes back.",
    ],
    shows: [
      'The hero (96): the start time, "Now" or the day, in the accent while the event is on or starts within 15 minutes; the title (28) over one line: until when, where, which calendar.',
      "The days, 32 apart: a caps day label, then 64 px rows (time 112, the calendar's 8 px dot, the title on one line).",
      'The Home card: a little calendar page with today\'s date, the next event\'s title, and "in 25 min · 16:50", "today 16:50" or "now · until 17:50".',
      'Month view: a month bar (title, Today, previous / next), the weekday row, then 6 × 7 cells of 80 × 72. Today\'s number sits in an accent disc; up to three dots, or two and "+N"; other months are mute.',
    ],
    usage: `import { calendarScreen } from '@facet/ui';

store.patch({ calendar: { at, days: 14, calendars, events } });
const calendar = calendarScreen({ store, actions: { setCalendar: (id, on) => save(id, on) } });`,
    data: `interface CalendarData {
  at: string;
  days: number;             // how far ahead the events go
  calendars: { id: string; name: string; on: boolean; colour?: string }[]; // colour = a colour token name
  events: AgendaEvent[];    // from the calendars that are on, from today's midnight, by start
  error?: string;
}

interface AgendaEvent {
  summary: string;
  start: string;            // ISO date-time, or YYYY-MM-DD for all-day events
  end: string;              // all-day: the day after the last day
  allDay: boolean;
  calendar?: string;        // the calendar's id
  location?: string;
}`,
    config: [
      "Home Assistant: call `calendar.get_events` on each calendar that's on (today's midnight, 14 days), and keep the on/off choices wherever your adapter keeps settings.",
      "Sort all-day events first within a day. The screen picks the hero itself: the running or next timed event, or an all-day one from tomorrow on, so a holiday doesn't hide the day's appointments.",
      "Month view: implement `loadCalendarMonth(month)` and patch `calendarMonth` with that month's six weeks (`CalendarMonth`: `month`, `start` = the Monday on or before the 1st, 42 days of events). Without it the month view shows only what `calendar` covers.",
    ],
  },
  {
    slug: 'overhead',
    preview: 'overhead',
    icon: 'overhead',
    title: 'Overhead',
    description:
      "What's flying over the house: the selected aircraft (the closest, until you pick another) as the hero, a sky radar, and the rest in a list.",
    intro: [
      'The Overhead screen answers "what\'s that plane?". The hero is the selected aircraft, the one in the accent on the radar. Until you pick one it is the closest. It shows where to look ("high in the NW · 38°", in the accent from 30° up), its route in big type ("Catania → Leeds", or the callsign when there is no believable route), the airline, flight and type, then its altitude, speed and distance.',
      'A small sky radar sits beside it: home in the middle, rings on a square-root scale, a plane for each aircraft turned the way it is flying. Below come the next two aircraft and "all aircraft". Tap any of them for the details: aircraft, registration, operator, squawk, heading and vertical speed.',
      'Now | Radar in the header swaps to the radar at full size, with callsigns on the five closest. Tap a plane or a row to select it: it becomes the hero and the accent. Tap the selected one again for its details. An emergency squawk (7700 and friends) shows in the error colour.',
      'Distances and speeds follow Settings > Units: the circle is 50 km (rings 5, 15, 30, 50) or 30 miles (rings 3, 10, 20, 30), speeds in mph, km/h or knots.',
    ],
    shows: [
      'The hero (656 × 264): look line (caps) + status, route (headline 40, two lines), who (body), three stats 144 / 120 / 120 (altitude, which reads climbing or descending past ±300 ft/min; speed; distance + compass), radar 208 on the right.',
      'Rows (112): "B738 · 32,000 ft · 26 km SE" over the route; emergency, military and helicopters get a status.',
      'Radar (576): rings 1 px, the edge 2 px, N/E/S/W, planes 32 px (24 at 208), the hero in the accent with a 2 px ring.',
      'The Home card: the radar at 72 with dots, the route and "32,000 ft · 26 km SE"; while something is 30° up it says "look up" and shows the dot.',
    ],
    usage: `import { overheadScreen } from '@facet/ui';

store.patch({ overhead: { at, radiusKm: 50, source: 'adsb.lol', aircraft } });
const overhead = overheadScreen({ store });`,
    data: `interface OverheadData {
  at: string;
  radiusKm: number;
  source: string;            // shown in the details
  aircraft: Aircraft[];      // any order; the screen sorts by km
  error?: string;
}

interface Aircraft {
  hex: string;               // ICAO address
  callsign: string | null; reg: string | null;
  type: string | null;       // "B738"
  model: string | null;      // "Boeing 737-800"
  operator: string | null; airline: string | null; flightNo: string | null;
  from: Airport | null; to: Airport | null; // only when the route is believable
  alt: number | null;        // ft
  vs: number | null;         // ft/min
  gs: number | null;         // knots
  track: number | null;      // degrees
  km: number; brg: number;   // from home, along the ground
  elev: number;              // degrees above the horizon from home
  squawk: string | null; emergency: string | null;
  kind: 'jet' | 'prop' | 'heli' | 'light' | 'other'; military: boolean;
}`,
    config: [
      'No receiver needed: the community ADS-B feeds (adsb.lol, adsb.fi) answer "everything within N nautical miles of a point" without a key. Your own receiver (readsb / tar1090) gives the same JSON.',
      "Work out each aircraft's distance, bearing and elevation (atan of altitude over ground distance) from home, drop ground traffic. The screen picks the closest itself.",
      'Routes: look the callsign up (adsbdb.com, for one), cache it, and only fill `from` / `to` when the aircraft is roughly on the way between the two airports; schedules go stale.',
      'Fetch while someone is looking, about every 15 seconds; ask for 50 km, or 48 km when Distance is set to miles.',
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
    slug: 'print',
    preview: 'print',
    icon: 'print',
    title: '3D print',
    description:
      'The print in an isometric view, filling up from the bed: printed part solid, the newest layers in orange, the rest a ghost.',
    intro: [
      'A glanceable "how far is it" from across the room. The model sits in an isometric view: what is printed is solid, the last few layers wear the accent, and what is still to come is a ghost you can see the print through.',
      "The fill cuts at the current layer, so the picture is what is physically on the plate. The big number is the printer's own %, so it agrees with the printer and its app.",
      "The model is drawn in software from a voxel grid, so it works for any source: sliced gcode (Bambu printers send a `.gcode.3mf` with no mesh in it, only gcode), an STL, or a shape function. A device that can't draw it gets the same picture as frames rendered on a server.",
    ],
    shows: [
      'The model (656 × 408), fitted so it never moves while it fills. Flat tones per face direction; the accent marks the newest layers.',
      'The hero (96 px, light): the % while printing, a word otherwise (Done, Idle).',
      'On the right: status and finish time, the job name, then the layer and the time left.',
    ],
    usage: `import { fromGcode, printScreen } from '@facet/ui';

const model = fromGcode(await (await fetch(gcodeUrl)).text(), { name: 'Lighthouse' });
store.patch({ print: { status: 'printing', task: 'Lighthouse', layer: 214, layers: 300, progress: 62, model } });
const print = printScreen(ctx, { shade: 'faces', solid: 'neutral', hot: 4 });`,
    data: `interface PrintData {
  status: 'printing' | 'paused' | 'preparing' | 'done' | 'failed' | 'idle';
  stage?: string | null;      // while preparing: "heating the bed"
  task?: string | null;
  layer: number;              // the fill cuts here
  layers: number;
  progress: number;           // the printer's own %, the hero
  remainingMin?: number | null;
  endTs?: number | null;      // unix seconds
  filament?: string | null;   // "#RRGGBB", for solid: 'filament'
  model?: Grid | null;        // fromGcode / fromStl / fromShape
}`,
    config: [
      "Home Assistant's bambu_lab integration gives `current_layer`, `total_layer_count`, `print_progress`, `remaining_time` and `end_time`, and saves each print's gcode under `/config/www/media/ha-bambulab/<serial>/prints/cache/` (served at `/local/`). Build the grid once per print and cache it.",
      'Options: `shade` (`faces`, the default, or `smooth`), `solid` (`neutral`, `steel` or `filament`) and `hot` (layers in the accent, default 4).',
      "Rendering a frame takes about 150 ms. To show it on a device that can't, call `render(grid, …)` on a server and send the image each time the layer changes.",
    ],
  },
  {
    slug: 'settings',
    preview: 'settings',
    icon: 'settings',
    title: 'Settings',
    description:
      'Every personalisation setting on one scrolling list, built from the @facet/settings schema: theme, accent, text size, sounds, the idle clock and how screens move.',
    intro: [
      "Settings opens from the Settings button in Home's header. It is one calm list in four groups (Look, Sound, Clock and idle, Behaviour) with a hairline between rows: the setting's name on the left, its control on the right.",
      "Nothing here is written by hand. The screen is built from `@facet/settings`, the same schema that makes the panel's Home Assistant entities and the ESPHome web page, so a setting added there shows up everywhere at once.",
      'A change shows straight away and holds for three seconds while the device answers; after that the store wins. The look settings apply to the whole device at once: try Accent or Text size in the preview.',
    ],
    shows: [
      'The header "Settings" with the back button.',
      'Per group a caps label, then rows of 96: up to three options as a segmented control (Theme, Text size, Transition, Toasts), the five accent swatches, an 80 × 48 switch (UI sounds).',
      'Choices with more options (Sound pack, Clock face) and one-tap presets (Clock after, Screen off) show the value with a chevron and open a bottom sheet with radio rows. The sound pack sheet stays open so the next pack can be heard straight away.',
      'Sliders (Volume, Idle brightness) take a row of 144: the name and value on top, the slider across the full width under it.',
    ],
    usage: `import { settingsScreen, createDevice } from '@facet/ui';

// on its own
const settings = settingsScreen({
  store,
  actions: { setSetting: (id, value) => store.patch({ settings: { ...store.get().settings, [id]: value } }) },
});

// or in a device: Home's header gets the Settings button, and the device applies the settings it reads
const device = createDevice({ store, actions });`,
    data: `// ids and values from @facet/settings; a setting left out = its default
type Values = Record<string, string | number | boolean>;

interface PanelData {
  settings?: Partial<Values> | null;
}

interface PanelActions {
  setSetting?(id: string, value: string | number | boolean): void;
}`,
    config: [
      'The device applies the look to its frame: `data-facet-theme` for Light or Dark (Auto leaves it to the page), `data-facet-accent` (the accent set swaps `--facet-color-accent*` with `light-dark()`, so it follows the theme) and `data-facet-text` (Large makes the title, body and label roles one step bigger in the same line boxes). `applySettings(el, values)` does the same for any element.',
      'Behaviour settings (transition, toast position, clock after, idle brightness, clock face) apply when their value changes, so `createDevice` options hold until the store sets them.',
      'Sounds (UI sounds, sound pack, volume) belong to the host: pass them on to `@facet/sounds` (`setEnabled`, `setPack`, `setVolume`) in your `setSetting`.',
      'On a panel the values are its own Home Assistant entities (`toEsphome()` in `@facet/settings`); in the mock house they start empty and every change is kept.',
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
      'Toasts of 656 × 120 at the screen inset, entering from the top just under the header (or from the bottom).',
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
