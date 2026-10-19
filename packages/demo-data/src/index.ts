/**
 * A mock house for demos, docs and tests: fictional rooms, speakers, weather, an energy tariff with price bands,
 * pull requests, a passing thunderstorm, agents, timers and toasts. It fills a Facet store and implements the panel
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
import { createStore, type Band, type PanelActions, type PanelData, type Store } from '@facet/ui';
import * as D from './data.js';
import { energyWith, lightningWith, type LightningScene } from './scenes.js';

export * from './data.js';
export * from './scenes.js';

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
    timers: timer > 0 ? [{ label: 'Bread', endsAt: now + timer * 1000, total: 12 * 60 }] : [],
    nextEvent: D.nextEvent(now),
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
  };
  const scene: Required<LiveScene> = {
    lightning: o.quietSky ? 'quiet' : 'live',
    band: 'live',
  };
  let watts = 420;
  const liveParts = (now: number): Partial<PanelData> => ({
    energy: scene.band === 'live' ? D.energy(now, watts) : energyWith(now, scene.band, watts),
    lightning: lightningWith(now, scene.lightning),
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
      }, 4000),
    );
    // next track when one ends
    timers.push(
      setInterval(() => {
        const m = media();
        if (m.state === 'playing' && m.duration && pos() >= m.duration) setTrack(trackIx + 1);
      }, 1000),
    );
  }
  return { store, actions, setScene, destroy: () => timers.forEach(clearInterval) };
}
