// UI sounds on the docs site, as in the hub workbench (apps/hub/src/wasm/sound.ts playUi): every fw::sound(obj, id,
// pos) the live previews request is synthesised in the browser with the approved Facet synth (lib/synth.ts) from
// renderer/tokens/sound.json (copied to <base>sound.json by scripts/extract.mjs, fetched on the first sound).
// One sound = base + variant (pack) defaults + the type's own values for that pack (minus `use`). Pitched sounds
// (step ticks): detail = position 0..1 -> sound.json pitch.low..pitch.high semitones (fallback (pos - 0.5) x track).
// Browsers allow audio only after a gesture: the AudioContext starts on the first pointer / key press on the page.
import { render, setLevels } from '../lib/synth';
import { livePrefs } from './prefs';

type Params = Record<string, unknown> & { engine: string; track?: number };
interface Variant { id: string; name: string; engine: string; about: string; defaults: Record<string, unknown> }
interface Type { id: string; name: string; for: string; variants: Record<string, Record<string, unknown>> }
export interface SoundDoc { default: string; base: Record<string, unknown>; levels?: Record<string, number>; pitch?: { low?: number; high?: number }; variants: Variant[]; types: Type[] }

const URL_DOC = `${import.meta.env.BASE_URL.replace(/\/?$/, '/')}sound.json`;
let docP: Promise<SoundDoc> | null = null;
export const soundDoc = () => (docP ??= fetch(URL_DOC).then((r) => r.json()).then((d: SoundDoc) => { if (d.levels) setLevels(d.levels); return d; }));

let ac: AudioContext | null = null, master: GainNode | null = null;
/** played-sound counter + last id (for tests: window.__facetSound) */
const stats = { played: 0, last: '', pack: '', rate: 1, ctx: 'none' as string };
(window as unknown as { __facetSound: typeof stats }).__facetSound = stats;
// the live demo's Settings > Volume (0-100; the panel default 80 = the site's old fixed 0.7)
let vol = 80;
const gainOf = (v: number) => 0.7 * Math.max(0, Math.min(100, v)) / 80;
export function setVolume(v: number) { if (!Number.isFinite(v)) return; vol = v; if (master) master.gain.value = gainOf(v); }
function ctx() {
  if (!ac) { ac = new AudioContext({ latencyHint: 'interactive' }); master = ac.createGain(); master.gain.value = gainOf(vol); master.connect(ac.destination); }
  if (ac.state === 'suspended') void ac.resume();
  stats.ctx = ac.state;
  return ac;
}
const cache = new Map<string, Promise<AudioBuffer>>();
function buffer(p: Params) {
  const k = JSON.stringify(p);
  let b = cache.get(k);
  if (!b) {
    const c = ctx();
    b = render(p as never, c.sampleRate).then((r) => { const buf = c.createBuffer(1, r.data.length, r.sr); buf.copyToChannel(r.data as Float32Array<ArrayBuffer>, 0); return buf; });
    cache.set(k, b);
  }
  return b;
}
function params(doc: SoundDoc, t: Type, v: Variant): Params {
  const { use: _use, ...own } = t.variants[v.id] ?? {};
  return { ...doc.base, ...v.defaults, ...own, engine: v.engine } as Params;
}
export const pickVariant = (doc: SoundDoc, id: string) => doc.variants.find((x) => x.id === id) ?? doc.variants.find((x) => x.id === doc.default) ?? doc.variants[0];

/** a UI sound requested by a component (fw_events kind "sound"); nothing when sound is off or before a gesture */
export async function playUi(id: string, detail = '') {
  const pr = livePrefs();
  if (!pr.sound || !ac) return;
  const doc = await soundDoc(), t = doc.types.find((x) => x.id === id), v = pickVariant(doc, pr.pack);
  if (!t || !v) return;
  const p = params(doc, t, v), pos = detail === '' ? NaN : Number(detail);
  const buf = await buffer(p), src = ctx().createBufferSource();
  src.buffer = buf;
  if (Number.isFinite(pos) && p.track) {
    // sound.json "pitch" {low, high}: semitones at the bottom / top of the range, linear in between (as the hub
    // player); without it the old symmetric (pos - 0.5) x track
    const pr = doc.pitch, ok = pr && typeof pr.low === 'number' && typeof pr.high === 'number';
    const semis = ok ? pr!.low! + pos * (pr!.high! - pr!.low!) : (pos - 0.5) * (p.track as number);
    src.playbackRate.value = Math.pow(2, semis / 12);
  }
  src.connect(master!);
  src.start();
  stats.played++; stats.last = id; stats.pack = v.id; stats.rate = src.playbackRate.value;
}

export function initSound() {
  // the first gesture anywhere starts audio (and warms the sound list)
  const start = () => { ctx(); void soundDoc(); };
  for (const ev of ['pointerdown', 'keydown'] as const) window.addEventListener(ev, start, { capture: true, once: true });
}
