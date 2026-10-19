/**
 * Live playback: one AudioContext (created on the first user gesture, because of browser autoplay rules), a master
 * volume, an on/off switch and the flavour in use. Sounds are rendered once per parameter set and cached.
 */
import {
  DEFAULT_FLAVOUR,
  FLAVOUR_KEYS,
  render,
  soundParams,
  SOUND_IDS,
  type SoundId,
  type SoundParams,
} from './synth.js';

export interface SoundSettings {
  /** Off by default: nothing plays until something turns it on. */
  enabled: boolean;
  /** Master volume, 0-1. */
  volume: number;
  /** Flavour key in use, e.g. "v1.crisp". */
  flavour: string;
}

export const settings: SoundSettings = { enabled: false, volume: 0.7, flavour: DEFAULT_FLAVOUR };

const listeners = new Set<(s: SoundSettings) => void>();
/** Get told when a setting changes. Returns an unsubscribe function. */
export function onSettingsChange(fn: (s: SoundSettings) => void): () => void {
  listeners.add(fn);
  return () => listeners.delete(fn);
}
const changed = () => listeners.forEach((fn) => fn(settings));

export function setEnabled(on: boolean): void {
  settings.enabled = on;
  if (on) ensure();
  changed();
}
export function setVolume(v: number): void {
  settings.volume = Math.max(0, Math.min(1, v));
  if (master) master.gain.value = settings.volume;
  changed();
}
export function setFlavour(key: string): void {
  if (!FLAVOUR_KEYS.includes(key)) return;
  settings.flavour = key;
  changed();
}

let ac: AudioContext | null = null;
let master: GainNode | null = null;
function ensure(): AudioContext | null {
  if (typeof AudioContext === 'undefined') return null;
  if (!ac) {
    ac = new AudioContext({ latencyHint: 'interactive' });
    master = ac.createGain();
    master.gain.value = settings.volume;
    master.connect(ac.destination);
  }
  if (ac.state === 'suspended') ac.resume().catch(() => {});
  return ac;
}

/** Create or resume the audio context inside the first real gesture (autoplay rules). Safe to call more than once. */
export function unlockOnGesture(): void {
  if (typeof window === 'undefined') return;
  const go = () => {
    if (settings.enabled) ensure();
  };
  for (const ev of ['pointerdown', 'keydown', 'touchstart'])
    window.addEventListener(ev, go, { capture: true, passive: true });
}

const cache = new Map<string, Promise<AudioBuffer>>();
function bufferFor(ctx: AudioContext, p: SoundParams) {
  const key = JSON.stringify(p);
  let b = cache.get(key);
  if (!b) {
    b = render(p, ctx.sampleRate).then((r) => {
      const buf = ctx.createBuffer(1, r.data.length, r.sr);
      buf.copyToChannel(r.data as Float32Array<ArrayBuffer>, 0);
      return buf;
    });
    cache.set(key, b);
    if (cache.size > 200) cache.delete(cache.keys().next().value!);
  }
  return b;
}

/** Render a flavour ahead of time, so the first tap has no delay. */
export function warm(flavour = settings.flavour): void {
  const ctx = ensure();
  if (!ctx) return;
  for (const id of SOUND_IDS) void bufferFor(ctx, soundParams(flavour, id));
}

let lastTick = 0;
/**
 * Play a sound. Does nothing while sounds are off, unless `always` is set (e.g. a "play" button next to a sound).
 * `value` (0-100) shifts the pitch for sounds that track a value, like slider ticks.
 */
export async function play(
  id: SoundId,
  o: { value?: number; flavour?: string; always?: boolean } = {},
): Promise<void> {
  if (!o.always && !settings.enabled) return;
  const p = soundParams(o.flavour ?? settings.flavour, id);
  if (id === 'slider-tick') {
    // rate-limit detents so a fast drag doesn't buzz
    const now = performance.now();
    if (now - lastTick < 28) return;
    lastTick = now;
  }
  const ctx = ensure();
  if (!ctx || !master) return;
  const buf = await bufferFor(ctx, p);
  const src = ctx.createBufferSource();
  src.buffer = buf;
  if (o.value !== undefined && p.track)
    src.playbackRate.value = Math.pow(2, (((o.value - 50) / 100) * p.track) / 12);
  src.connect(master);
  src.start();
}
