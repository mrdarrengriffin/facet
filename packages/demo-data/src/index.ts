/**
 * A mock house for demos, docs and tests: fictional rooms, speakers, weather, an energy tariff with price bands,
 * pull requests, a passing thunderstorm, the real moon, a 3D print, a thermostat, a switch, agents, timers and toasts. It fills a Facet store and implements the panel
 * actions, so the screens behave as they would on a real house: play and pause, change the volume, dim the light,
 * start a timer.
 *
 * ```ts
 * import { createMockHouse } from '@facet/demo-data';
 * import { createDevice } from '@facet/ui';
 * const house = createMockHouse();
 * document.body.append(createDevice(house).el);
 * ```
 */
import {
  createStore,
  moonData,
  sunData,
  type Band,
  type PanelActions,
  type PanelData,
  type Store,
  type ThermostatData,
} from '@facet/ui';
import * as D from './data.js';
import { printWith } from './print.js';
import { switchWatts, switchWith } from './switch.js';
import { readoutsWith } from './readouts.js';
import { thermostatStep, thermostatWith, withAction } from './thermostat.js';
import { coversStep, coversWith } from './covers.js';
import {
  energyWith,
  lightningWith,
  moonWith,
  type LightningScene,
  type MoonScene,
} from './scenes.js';

export * from './data.js';
export * from './scenes.js';
export * from './print.js';
export * from './thermostat.js';
export * from './covers.js';
export * from './switch.js';
export * from './readouts.js';

export interface MockHouseOptions {
  /** Keep things moving: track progress, live power, the storm. Default true. */
  live?: boolean;
  /** Start with the music paused. */
  paused?: boolean;
  /** No lightning. */
  quietSky?: boolean;
  /** Start with a timer running (seconds left). Default 6 min 40 s; 0 = none. */
  timer?: number;
}

/** Parts of the house that keep changing on their own: pin them to a scene and the live updates follow it. */
export interface LiveScene {
  /** `live` (default) = the storm crossing once an hour. */
  lightning?: LightningScene;
  /** Force the current price band; `live` (default) follows the clock. */
  band?: Band | 'live';
  /** Show another moon (phase, days ahead, hemisphere); default = tonight's. */
  moon?: MoonScene;
}

export interface MockHouse {
  store: Store<PanelData>;
  actions: Required<PanelActions>;
  /** Pin the live parts (lightning, price band) to a scene. Applies now and on every live update. */
  setScene(scene: LiveScene): void;
  /** Stop the live updates. */
  destroy(): void;
}

/** The initial state of the mock house at `now`. */
export function mockState(now = Date.now(), o: MockHouseOptions = {}): PanelData {
  const track = D.TRACKS[0]!;
  const timer = o.timer ?? 400;
  return {
    room: D.ROOM,
    place: D.PLACE,
    lightName: D.LIGHT_NAME,
    media: {
      state: o.paused ? 'paused' : 'playing',
      title: track.title,
      artist: track.artist,
      art: track.art,
      duration: track.duration,
      position: 74,
      positionAt: now,
      volume: 0.42,
    },
    players: D.PLAYERS.map((p) =>
      p.id === 'study'
        ? { ...p, state: o.paused ? 'paused' : 'playing', title: track.title, artist: track.artist }
        : p,
    ),
    mediaTarget: { mode: 'auto', id: 'study', name: 'Study speaker' },
    light: { on: true, brightness: 64 },
    agents: D.AGENTS,
    weather: D.weather(now),
    energy: D.energy(now),
    github: D.github(now),
    lightning: D.lightning(now, o.quietSky),
    moon: moonData(now, D.PLACE_AT),
    sun: sunData(now, D.PLACE_AT),
    print: printWith(now),
    thermostat: thermostatWith(now),
    covers: coversWith(),
    switch: switchWith(now),
    readouts: readoutsWith(now),
    timers: timer > 0 ? [{ label: 'Bread', endsAt: now + timer * 1000, total: 12 * 60 }] : [],
    nextEvent: D.nextEvent(now),
    calendar: D.calendar(now),
  };
}

export function createMockHouse(o: MockHouseOptions = {}): MockHouse {
  const store = createStore<PanelData>(mockState(Date.now(), o));
  let trackIx = 0;
  let lastLight = 64;
  const media = () => store.get().media!;
  const setTrack = (i: number) => {
    trackIx = (i + D.TRACKS.length) % D.TRACKS.length;
    const t = D.TRACKS[trackIx]!;
    store.patch({
      media: {
        ...media(),
        title: t.title,
        artist: t.artist,
        art: t.art,
        duration: t.duration,
        position: 0,
        positionAt: Date.now(),
      },
    });
    syncPlayers();
  };
  const syncPlayers = () => {
    const s = store.get(),
      m = s.media!,
      id = s.mediaTarget?.id;
    store.patch({
      players: (s.players ?? []).map((p) =>
        p.id === id
          ? { ...p, state: m.state, title: m.title, artist: m.artist }
          : p.id !== id && p.state === 'playing'
            ? { ...p, state: 'paused' }
            : p,
      ),
    });
  };
  const pos = () => {
    const m = media();
    return Math.min(
      m.duration ?? 0,
      (m.position ?? 0) +
        (m.state === 'playing' && m.positionAt ? (Date.now() - m.positionAt) / 1000 : 0),
    );
  };
  const thermo = (f: (t: ThermostatData) => ThermostatData) => {
    const t = store.get().thermostat;
    if (t) store.patch({ thermostat: withAction(f(t)) });
  };
  // cover motors: where each moving cover is heading
  const goals = new Map<string, number>();
  let motor: ReturnType<typeof setInterval> | undefined;
  const moveCover = (id: string, goal: number) => {
    goals.set(id, goal);
    if (!motor)
      motor = setInterval(() => {
        const cv = store.get().covers;
        if (cv) store.patch({ covers: coversStep(cv, goals) });
        if (!goals.size) {
          clearInterval(motor);
          motor = undefined;
        }
      }, 200);
  };
  const actions: Required<PanelActions> = {
    mediaPlayPause() {
      const m = media();
      store.patch({
        media: {
          ...m,
          state: m.state === 'playing' ? 'paused' : 'playing',
          position: pos(),
          positionAt: Date.now(),
        },
      });
      syncPlayers();
    },
    mediaNext: () => setTrack(trackIx + 1),
    mediaPrevious: () =>
      pos() > 5
        ? store.patch({ media: { ...media(), position: 0, positionAt: Date.now() } })
        : setTrack(trackIx - 1),
    mediaVolume: (v) => store.patch({ media: { ...media(), volume: v / 100 } }),
    setMediaTarget(mode, id) {
      const players = store.get().players ?? [];
      const p =
        players.find((x) => x.id === id) ??
        players.find((x) => x.state === 'playing') ??
        players[0]!;
      store.patch({ mediaTarget: { mode, id: p.id, name: p.name } });
      syncPlayers();
    },
    lightBrightness(v) {
      if (v > 0) lastLight = v;
      store.patch({ light: { on: v > 0, brightness: v } });
    },
    lightToggle() {
      const l = store.get().light;
      store.patch({
        light: l?.on
          ? { on: false, brightness: l.brightness }
          : { on: true, brightness: lastLight || 60 },
      });
    },
    refreshGithub: () => store.patch({ github: D.github(Date.now()) }),
    startTimer: (seconds, label) =>
      store.patch({ timers: [{ label, endsAt: Date.now() + seconds * 1000, total: seconds }] }),
    extendTimer(seconds) {
      const t = store.get().timers?.[0];
      if (t)
        store.patch({
          timers: [{ ...t, endsAt: t.endsAt + seconds * 1000, total: t.total + seconds }],
        });
    },
    cancelTimer: () => store.patch({ timers: [] }),
    setCalendar(id, on) {
      const off = new Set(
        (store.get().calendar?.calendars ?? D.CALENDARS).filter((c) => !c.on).map((c) => c.id),
      );
      if (on) off.delete(id);
      else off.add(id);
      const m = store.get().calendarMonth?.month;
      store.patch({
        calendar: D.calendar(Date.now(), off),
        ...(m ? { calendarMonth: D.calendarMonth(Date.now(), m, off) } : {}),
      });
    },
    loadCalendarMonth(month) {
      const off = new Set(
        (store.get().calendar?.calendars ?? D.CALENDARS).filter((c) => !c.on).map((c) => c.id),
      );
      store.patch({ calendarMonth: D.calendarMonth(Date.now(), month, off) });
    },
    // the thermostat answers like a real one: a new set point in auto drops to heat (a manual override)
    climateTarget: (target) =>
      thermo((t) => ({ ...t, target, mode: t.mode === 'auto' ? 'heat' : t.mode, next: null })),
    climateMode: (mode) =>
      thermo((t) =>
        mode === 'off'
          ? { ...t, mode, target: null, next: null }
          : { ...t, mode, target: t.target ?? 19, preset: 'none' },
      ),
    coverPosition: (id, position) => moveCover(id, position),
    coverTilt: (id, tilt) =>
      store.patch({
        covers: (store.get().covers ?? []).map((c) => (c.id === id ? { ...c, tilt } : c)),
      }),
    coverCommand(id, command) {
      if (command === 'open') moveCover(id, 100);
      else if (command === 'close') moveCover(id, 0);
      else {
        goals.delete(id);
        store.patch({
          covers: (store.get().covers ?? []).map((c) =>
            c.id === id ? { ...c, state: (c.position ?? 0) <= 0 ? 'closed' : 'open' } : c,
          ),
        });
      }
    },
    climatePreset: (preset) =>
      thermo((t) => ({
        ...t,
        preset,
        mode: t.mode === 'off' ? 'heat' : t.mode,
        target: { home: 20.5, away: 15, eco: 17, boost: 23 }[preset] ?? t.target ?? 19,
      })),
    // a smart plug takes a moment to answer, like a real one
    switchSet: (on) => {
      setTimeout(() => {
        const sw = store.get().switch;
        if (!sw || sw.on === null || sw.on === on) return;
        const now = Date.now();
        store.patch({
          switch: {
            ...sw,
            on,
            power: on ? switchWatts(now) : 0.4,
            changedAt: new Date(now).toISOString(),
          },
        });
      }, 600);
    },
    // the house's settings start empty (every setting at its default, the page's own theme); a change is kept
    setSetting: (id, value) => store.patch({ settings: { ...store.get().settings, [id]: value } }),
  };
  const scene: Required<LiveScene> = {
    lightning: o.quietSky ? 'quiet' : 'live',
    band: 'live',
    moon: {},
  };
  let watts = 420;
  const liveParts = (now: number): Partial<PanelData> => ({
    energy: scene.band === 'live' ? D.energy(now, watts) : energyWith(now, scene.band, watts),
    lightning: lightningWith(now, scene.lightning),
    moon: moonWith(now, scene.moon),
  });
  const setScene = (s: LiveScene) => {
    Object.assign(scene, s);
    store.patch(liveParts(Date.now()));
  };
  const timers: ReturnType<typeof setInterval>[] = [];
  if (o.live !== false) {
    // live power wobble + a fresh storm snapshot every few seconds
    timers.push(
      setInterval(() => {
        const now = Date.now();
        watts = Math.round(380 + 140 * Math.abs(Math.sin(now / 9000)));
        store.patch(liveParts(now));
        const sw = store.get().switch;
        if (sw?.on) store.patch({ switch: { ...sw, power: switchWatts(now) } });
      }, 4000),
    );
    // the room warms up and cools down, a step a minute
    timers.push(
      setInterval(() => {
        const t = store.get().thermostat;
        if (t) store.patch({ thermostat: thermostatStep(t, Date.now()) });
      }, 60_000),
    );
    // next track when one ends
    timers.push(
      setInterval(() => {
        const m = media();
        if (m.state === 'playing' && m.duration && pos() >= m.duration) setTrack(trackIx + 1);
      }, 1000),
    );
  }
  return {
    store,
    actions,
    setScene,
    destroy: () => {
      timers.forEach(clearInterval);
      clearInterval(motor);
    },
  };
}
