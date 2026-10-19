/**
 * Procedural UI sounds: no samples. One sound is one flat set of parameters (`SoundParams`), rendered with Web Audio
 * into a buffer (OfflineAudioContext), cleaned up (DC offset, edge fades) and loudness-aligned. The same buffer is
 * what plays live and what you'd export to WAV for a panel, so what you hear in the browser is what the firmware gets.
 */
import { sound as SOUND } from '@facet/tokens';
import data from './packs.json' with { type: 'json' };

export type Wave = 'sine' | 'triangle' | 'square' | 'sawtooth' | 'pulse';
export type NoiseType = 'bandpass' | 'highpass' | 'lowpass';
/**
 * `basic`: oscillators with partials, filtered noise and a feedback echo.
 * `modal`: a short noise strike that excites a few damped sine modes (a wood, glass or felt body), plus a few
 * milliseconds of early reflections.
 */
export type Engine = 'basic' | 'modal';

/** Everything one sound is made of. Each engine reads the shared values plus its own. */
export interface SoundParams {
  engine: Engine;
  /** Hz, the first note (modal: the body's fundamental). */
  freq: number;
  /** Semitone offsets of each note, e.g. "0 7" (a fifth up) or "7 0" (falling). */
  notes: string;
  /** Seconds between note starts. */
  gap: number;
  /** Level multiplier for each following note. */
  fall: number;
  /** Pitch envelope: start at freq × pitchFrom and glide to freq over pitchTime seconds. */
  pitchFrom: number;
  pitchTime: number;
  /** Seconds. */
  attack: number;
  /** Seconds: the exponential time constant after the attack (modal: of the first mode). */
  decay: number;
  /** Seconds: total length (hard end with a short fade). */
  length: number;
  /** Hz: low-pass over everything, i.e. brightness. */
  cutoff: number;
  /** Relative loudness after normalisation (1 = reference). */
  gain: number;
  /** Semitones across the 0-100 value range (slider ticks follow the value); 0 = fixed pitch. */
  track: number;
  /** Basic: oscillator wave. */
  wave: Wave;
  /** Basic: pulse wave duty cycle. */
  duty: number;
  /** Basic: "ratio:gain" partials, e.g. "1:1 2.76:0.2". */
  partials: string;
  /** Basic: filtered noise level, type, centre, Q and decay (seconds). */
  noise: number;
  noiseType: NoiseType;
  noiseFreq: number;
  noiseQ: number;
  noiseDecay: number;
  /** Basic: feedback echo level and delay (seconds). */
  echo: number;
  echoTime: number;
  /** Modal: "ratio:gain:decayMul" per mode, e.g. "1:1:1 1.58:0.45:0.55". Inharmonic ratios sound like wood or glass. */
  modes: string;
  /** Modal: level of the strike (a band-passed noise burst). */
  exNoise: number;
  /** Modal: Hz, centre of the strike's band. */
  exFreq: number;
  exQ: number;
  /** Modal: seconds, the strike's time constant (1-4 ms). */
  exDecay: number;
  /** Modal: the strike's band starts at exFreq × exDrop and falls to exFreq. */
  exDrop: number;
  /** Modal: early reflections level (a few milliseconds, not reverb). */
  room: number;
  /** Modal: reflection time scale (1 = 3 / 7 / 12 / 17 ms). */
  roomSize: number;
}

export const SOUND_IDS = [
  'tap',
  'open',
  'back',
  'toggle-on',
  'toggle-off',
  'slider-tick',
  'confirm',
  'timer-done',
  'error',
  'notify',
] as const;
export type SoundId = (typeof SOUND_IDS)[number];
export const SOUND_LABELS: Record<SoundId, string> = {
  tap: 'Tap',
  open: 'Open',
  back: 'Back',
  'toggle-on': 'Toggle on',
  'toggle-off': 'Toggle off',
  'slider-tick': 'Slider tick',
  confirm: 'Confirm',
  'timer-done': 'Timer done',
  error: 'Error',
  notify: 'Notify',
};

/** A named set of the ten sounds, all made by one engine. */
export interface SoundPack {
  /** Lower-case id, e.g. "keycap". */
  id: string;
  /** Display name, e.g. "Keycap". */
  name: string;
  engine: Engine;
  /** One or two sentences on how the pack sounds. */
  about: string;
  /** Values shared by every sound in the pack. */
  defaults: Partial<SoundParams>;
  /** Each sound's own values, plus a short note on what it sounds like (`use`). */
  sounds: Record<SoundId, Partial<SoundParams> & { use?: string }>;
}

/** Every pack: Keycap, Halo, Arcade (basic engine), Graphite, Crystal, Felt (modal engine). */
export const PACKS: readonly SoundPack[] = (data as unknown as { packs: SoundPack[] }).packs;
export type PackId = 'keycap' | 'halo' | 'arcade' | 'graphite' | 'crystal' | 'felt';
export const PACK_IDS: readonly string[] = PACKS.map((p) => p.id);

/** The pack in use unless you pick another: tokens `sound.pack` (Keycap). */
export const DEFAULT_PACK: string = PACK_IDS.includes(SOUND.pack) ? SOUND.pack : PACK_IDS[0]!;

/** Look a pack up by id (case-insensitive). Unknown ids return undefined. */
export const getPack = (id: string): SoundPack | undefined => {
  const k = id.trim().toLowerCase();
  return PACKS.find((p) => p.id === k);
};
const packOrDefault = (id: string) => getPack(id) ?? getPack(DEFAULT_PACK)!;

const BASE: SoundParams = {
  engine: 'basic',
  wave: 'sine',
  duty: 0.25,
  freq: 880,
  notes: '0',
  gap: 0.06,
  fall: 0.9,
  partials: '1:1',
  pitchFrom: 1,
  pitchTime: 0.01,
  attack: 0.002,
  decay: 0.05,
  length: 0.15,
  cutoff: 12000,
  noise: 0,
  noiseType: 'bandpass',
  noiseFreq: 3000,
  noiseQ: 1,
  noiseDecay: 0.004,
  echo: 0,
  echoTime: 0.08,
  gain: 1,
  track: 0,
  modes: '1:1:1',
  exNoise: 0.4,
  exFreq: 3000,
  exQ: 1.2,
  exDecay: 0.0012,
  exDrop: 1.5,
  room: 0.1,
  roomSize: 1,
};

/** The full parameters of one sound in a pack (engine defaults + pack defaults + the sound's own values). */
export function soundParams(pack: string, id: SoundId): SoundParams {
  const p = packOrDefault(pack);
  const { use: _use, ...own } = p.sounds[id] ?? {};
  return { ...BASE, ...p.defaults, ...own, engine: p.engine };
}
/** A short note on what a sound in a pack sounds like. */
export const soundUse = (pack: string, id: SoundId) => getPack(pack)?.sounds[id]?.use ?? '';

const parseList = (s: string) =>
  String(s)
    .trim()
    .split(/[\s,]+/)
    .filter(Boolean)
    .map(Number)
    .filter(Number.isFinite);
const parsePartials = (s: string): [number, number][] => {
  const out = String(s)
    .trim()
    .split(/[\s,]+/)
    .filter(Boolean)
    .map((x) => {
      const [r = 0, g] = x.split(':').map(Number);
      return [r, Number.isFinite(g) ? (g as number) : 1] as [number, number];
    })
    .filter(([r, g]) => r > 0 && Number.isFinite(r) && Number.isFinite(g));
  return out.length ? out : [[1, 1]];
};

function rng(seed: number) {
  return () => {
    seed |= 0;
    seed = (seed + 0x6d2b79f5) | 0;
    let t = Math.imul(seed ^ (seed >>> 15), 1 | seed);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

function pulseWave(ctx: BaseAudioContext, duty: number) {
  const n = 64;
  const real = new Float32Array(n);
  const imag = new Float32Array(n);
  for (let k = 1; k < n; k++) real[k] = (2 / (k * Math.PI)) * Math.sin(k * Math.PI * duty);
  return ctx.createPeriodicWave(real, imag);
}

/**
 * Modal engine: per note, a short band-passed noise strike (its band falls from exFreq × exDrop to exFreq) excites a
 * few damped sine modes (ratio:gain:decayMul), all through a gentle low-pass, plus a few ms of early reflections.
 */
function buildModal(ctx: BaseAudioContext, p: SoundParams, rate = 1): AudioNode {
  const sr = ctx.sampleRate;
  const nyq = sr / 2 - 100;
  const out = ctx.createGain();
  const lp = ctx.createBiquadFilter();
  lp.type = 'lowpass';
  lp.frequency.value = Math.min(p.cutoff, nyq);
  lp.Q.value = 0.5;
  lp.connect(out);
  if (p.room > 0) {
    const damp = ctx.createBiquadFilter();
    damp.type = 'lowpass';
    damp.frequency.value = Math.min(3500, nyq);
    damp.connect(out);
    for (const [s, g] of [
      [0.0031, 0.5],
      [0.0073, 0.35],
      [0.0119, 0.25],
      [0.0173, 0.15],
    ] as const) {
      const d = ctx.createDelay(0.2);
      const dg = ctx.createGain();
      d.delayTime.value = s * p.roomSize;
      dg.gain.value = g * p.room;
      lp.connect(d);
      d.connect(dg);
      dg.connect(damp);
    }
  }
  const notes = parseList(p.notes);
  const modes = String(p.modes)
    .trim()
    .split(/[\s,]+/)
    .filter(Boolean)
    .map((x) => {
      const [r = 0, g, dm] = x.split(':').map(Number);
      return [
        r,
        Number.isFinite(g) ? (g as number) : 1,
        Number.isFinite(dm) ? (dm as number) : 1,
      ] as [number, number, number];
    })
    .filter(([r]) => r > 0 && Number.isFinite(r));
  const end = p.length;
  const rand = rng(4321);
  (notes.length ? notes : [0]).forEach((semi, i) => {
    const t = i * p.gap;
    if (t >= end) return;
    const lvl = Math.pow(p.fall, i);
    const f0 = p.freq * rate * Math.pow(2, semi / 12);
    for (const [r, g, dm] of modes.length ? modes : [[1, 1, 1] as [number, number, number]]) {
      const f = f0 * r;
      if (f >= nyq) continue;
      const o = ctx.createOscillator();
      o.type = 'sine';
      o.frequency.setValueAtTime(Math.min(nyq, f * p.pitchFrom), t);
      if (p.pitchFrom !== 1)
        o.frequency.exponentialRampToValueAtTime(f, t + Math.max(0.001, p.pitchTime));
      const env = ctx.createGain();
      env.gain.setValueAtTime(0, t);
      env.gain.linearRampToValueAtTime(g * lvl, t + p.attack);
      env.gain.setTargetAtTime(0, t + p.attack, Math.max(0.0005, p.decay * dm));
      o.connect(env);
      env.connect(lp);
      o.start(t);
      o.stop(end);
    }
    if (p.exNoise > 0) {
      const len = Math.max(1, Math.ceil(Math.min(end - t, p.exDecay * 10 + 0.002) * sr));
      const buf = ctx.createBuffer(1, len, sr);
      const ch = buf.getChannelData(0);
      for (let j = 0; j < len; j++) ch[j] = rand() * 2 - 1;
      const src = ctx.createBufferSource();
      src.buffer = buf;
      const bf = ctx.createBiquadFilter();
      bf.type = 'bandpass';
      bf.Q.value = p.exQ;
      const fx = Math.min(p.exFreq * rate, nyq);
      bf.frequency.setValueAtTime(Math.min(fx * p.exDrop, nyq), t);
      if (p.exDrop !== 1)
        bf.frequency.exponentialRampToValueAtTime(fx, t + Math.max(0.0005, p.exDecay * 3));
      const ng = ctx.createGain();
      const at = Math.min(p.attack, 0.0008);
      ng.gain.setValueAtTime(0, t);
      ng.gain.linearRampToValueAtTime(
        p.exNoise * lvl * Math.sqrt(Math.max(1, p.exQ)) * 1.5,
        t + at,
      );
      ng.gain.setTargetAtTime(0, t + at, Math.max(0.0002, p.exDecay));
      src.connect(bf);
      bf.connect(ng);
      ng.connect(lp);
      src.start(t);
    }
  });
  return out;
}

/** Basic engine: oscillators with partials, filtered noise and a feedback echo. */
function build(ctx: BaseAudioContext, p: SoundParams, rate = 1): AudioNode {
  const sr = ctx.sampleRate;
  const nyq = sr / 2 - 100;
  const out = ctx.createGain();
  const mix = ctx.createGain();
  const lp = ctx.createBiquadFilter();
  lp.type = 'lowpass';
  lp.frequency.value = Math.min(p.cutoff, nyq);
  lp.Q.value = 0.5;
  lp.connect(mix);
  mix.connect(out);
  if (p.echo > 0) {
    const d = ctx.createDelay(1);
    const fb = ctx.createGain();
    const wet = ctx.createGain();
    const damp = ctx.createBiquadFilter();
    d.delayTime.value = p.echoTime;
    fb.gain.value = 0.38;
    wet.gain.value = p.echo;
    damp.type = 'lowpass';
    damp.frequency.value = Math.min(4500, nyq);
    mix.connect(d);
    d.connect(damp);
    damp.connect(fb);
    fb.connect(d);
    damp.connect(wet);
    wet.connect(out);
  }
  const notes = parseList(p.notes);
  const partials = parsePartials(p.partials);
  const pw = p.wave === 'pulse' ? pulseWave(ctx, Math.min(0.5, Math.max(0.02, p.duty))) : null;
  const end = p.length;
  const rand = rng(1234);
  (notes.length ? notes : [0]).forEach((semi, i) => {
    const t = i * p.gap;
    if (t >= end) return;
    const lvl = Math.pow(p.fall, i);
    const f0 = p.freq * rate * Math.pow(2, semi / 12);
    const env = ctx.createGain();
    env.gain.setValueAtTime(0, t);
    env.gain.linearRampToValueAtTime(lvl, t + p.attack);
    env.gain.setTargetAtTime(0, t + p.attack, Math.max(0.0005, p.decay));
    env.connect(lp);
    for (const [r, g] of partials) {
      const f = f0 * r;
      if (f >= nyq) continue;
      const o = ctx.createOscillator();
      if (pw) o.setPeriodicWave(pw);
      else o.type = p.wave as OscillatorType;
      o.frequency.setValueAtTime(Math.min(nyq, f * p.pitchFrom), t);
      if (p.pitchFrom !== 1)
        o.frequency.exponentialRampToValueAtTime(f, t + Math.max(0.001, p.pitchTime));
      const pg = ctx.createGain();
      pg.gain.value = g;
      if (r > 1.01) {
        // higher partials die faster: a struck, bell-like feel
        pg.gain.setValueAtTime(g, t + p.attack);
        pg.gain.setTargetAtTime(0, t + p.attack, Math.max(0.0005, p.decay / Math.sqrt(r)));
      }
      o.connect(pg);
      pg.connect(env);
      o.start(t);
      o.stop(end);
    }
    if (p.noise > 0) {
      const len = Math.max(1, Math.ceil(Math.min(end - t, p.noiseDecay * 8 + 0.01) * sr));
      const buf = ctx.createBuffer(1, len, sr);
      const ch = buf.getChannelData(0);
      for (let j = 0; j < len; j++) ch[j] = rand() * 2 - 1;
      const src = ctx.createBufferSource();
      src.buffer = buf;
      const bf = ctx.createBiquadFilter();
      bf.type = p.noiseType;
      bf.frequency.value = Math.min(p.noiseFreq * rate, nyq);
      bf.Q.value = p.noiseQ;
      const ng = ctx.createGain();
      // a narrow band-pass loses level: compensate a little so `noise` stays roughly comparable
      const comp = p.noiseType === 'bandpass' ? Math.sqrt(Math.max(1, p.noiseQ)) * 1.5 : 1;
      ng.gain.setValueAtTime(0, t);
      ng.gain.linearRampToValueAtTime(p.noise * lvl * comp, t + 0.0005);
      ng.gain.setTargetAtTime(0, t + 0.0005, Math.max(0.0003, p.noiseDecay));
      src.connect(bf);
      bf.connect(ng);
      ng.connect(mix);
      src.start(t);
    }
  });
  return out;
}

// ---------------------------------------------------------------- post-processing
function biquad(x: Float32Array, b0: number, b1: number, b2: number, a1: number, a2: number) {
  const y = new Float32Array(x.length);
  let x1 = 0,
    x2 = 0,
    y1 = 0,
    y2 = 0;
  for (let i = 0; i < x.length; i++) {
    const xi = x[i]!;
    const v = b0 * xi + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1;
    x1 = xi;
    y2 = y1;
    y1 = v;
    y[i] = v;
  }
  return y;
}
/** A K-weighting-like prefilter: high-pass at 60 Hz, then a +4 dB high shelf at 1.5 kHz. */
function kWeight(x: Float32Array, sr: number) {
  let w = (2 * Math.PI * 60) / sr;
  let c = Math.cos(w);
  let al = Math.sin(w) / (2 * 0.707);
  let a0 = 1 + al;
  let y = biquad(
    x,
    (1 + c) / 2 / a0,
    -(1 + c) / a0,
    (1 + c) / 2 / a0,
    (-2 * c) / a0,
    (1 - al) / a0,
  );
  const A = Math.pow(10, 4 / 40);
  w = (2 * Math.PI * 1500) / sr;
  c = Math.cos(w);
  al = (Math.sin(w) / 2) * Math.sqrt(2);
  const sq = 2 * Math.sqrt(A) * al;
  a0 = A + 1 - (A - 1) * c + sq;
  y = biquad(
    y,
    (A * (A + 1 + (A - 1) * c + sq)) / a0,
    (-2 * A * (A - 1 + (A + 1) * c)) / a0,
    (A * (A + 1 + (A - 1) * c - sq)) / a0,
    (2 * (A - 1 - (A + 1) * c)) / a0,
    (A + 1 - (A - 1) * c - sq) / a0,
  );
  return y;
}
/** Loudness in dB: the loudest 50 ms window after the prefilter, so short clicks and long chimes compare fairly. */
export function loudnessDb(x: Float32Array, sr: number) {
  const y = kWeight(x, sr);
  const win = Math.max(1, Math.round(0.05 * sr));
  let acc = 0,
    best = 0;
  for (let i = 0; i < y.length; i++) {
    acc += y[i]! * y[i]!;
    if (i >= win) acc -= y[i - win]! * y[i - win]!;
    best = Math.max(best, acc / win);
  }
  return 10 * Math.log10(best + 1e-12);
}
export const peakDb = (x: Float32Array) => {
  let m = 0;
  for (const v of x) m = Math.max(m, Math.abs(v));
  return 20 * Math.log10(m + 1e-12);
};

/** DC removal, edge fades, loudness alignment (target + 20 log10(gain)) and a peak cap. Mutates x. */
function finish(x: Float32Array, sr: number, p: SoundParams) {
  let mean = 0;
  for (const v of x) mean += v;
  mean /= x.length || 1;
  for (let i = 0; i < x.length; i++) x[i]! -= mean;
  const fade = Math.min(Math.round((SOUND.fadeMs / 1000) * sr), Math.floor(x.length / 4));
  const fin = Math.min(Math.round(0.0005 * sr), fade);
  for (let i = 0; i < fin; i++) x[i]! *= i / fin;
  for (let i = 0; i < fade; i++) x[x.length - 1 - i]! *= i / fade;
  // residual DC after the fades: subtract a Hann-shaped offset, which keeps both edges at zero
  let m2 = 0,
    wsum = 0;
  const hann = (i: number) => 0.5 - 0.5 * Math.cos((2 * Math.PI * i) / Math.max(1, x.length - 1));
  for (let i = 0; i < x.length; i++) {
    m2 += x[i]!;
    wsum += hann(i);
  }
  if (wsum > 0) {
    const c = m2 / wsum;
    for (let i = 0; i < x.length; i++) x[i]! -= c * hann(i);
  }
  const target = SOUND.loudnessDb + 20 * Math.log10(Math.max(0.01, p.gain));
  let g = target - loudnessDb(x, sr);
  const pk = peakDb(x) + g;
  if (pk > SOUND.peakDb) g -= pk - SOUND.peakDb;
  const k = Math.pow(10, g / 20);
  for (let i = 0; i < x.length; i++) x[i]! *= k;
  return g;
}

/** Render one sound to normalised mono samples at `sr`. `rate` multiplies the pitch. */
export async function render(
  p: SoundParams,
  sr = 44100,
  rate = 1,
): Promise<{ data: Float32Array; sr: number; gainDb: number }> {
  const len = Math.max(1, Math.round(p.length * sr));
  const ctx = new OfflineAudioContext(1, len, sr);
  (p.engine === 'modal' ? buildModal : build)(ctx, p, rate).connect(ctx.destination);
  const buf = await ctx.startRendering();
  const data = new Float32Array(buf.getChannelData(0));
  const gainDb = finish(data, sr, p);
  return { data, sr, gainDb };
}

/** 16-bit PCM mono WAV, e.g. to embed sounds in panel firmware. */
export function wav(data: Float32Array, sr: number): Blob {
  const b = new ArrayBuffer(44 + data.length * 2);
  const v = new DataView(b);
  const str = (o: number, s: string) => {
    for (let i = 0; i < s.length; i++) v.setUint8(o + i, s.charCodeAt(i));
  };
  str(0, 'RIFF');
  v.setUint32(4, 36 + data.length * 2, true);
  str(8, 'WAVE');
  str(12, 'fmt ');
  v.setUint32(16, 16, true);
  v.setUint16(20, 1, true);
  v.setUint16(22, 1, true);
  v.setUint32(24, sr, true);
  v.setUint32(28, sr * 2, true);
  v.setUint16(32, 2, true);
  v.setUint16(34, 16, true);
  str(36, 'data');
  v.setUint32(40, data.length * 2, true);
  for (let i = 0; i < data.length; i++)
    v.setInt16(44 + i * 2, Math.round(Math.max(-1, Math.min(1, data[i]!)) * 32767), true);
  return new Blob([b], { type: 'audio/wav' });
}
