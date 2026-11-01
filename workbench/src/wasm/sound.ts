// Workbench, Foundations > Sound (/wasm/foundations/sound). HTML, not drawn in WASM (it is about
// hearing), in the doc layout of the other foundation pages: rules (why / when / never / which / levels / packs / ticks),
// every sound type x pack playable, when each type plays, "what uses what" (generated from the code), the step-tick
// pitch range previewed by ear (Low / High + a real Slider and Dial + a sweep; play-only, never saved), the packs.
// Single source renderer/tokens/sound.json via /api/wasm/sound (GET adds `generated` =
// renderer/tools/gen_sound_usage.mjs --json; read only). Sounds are synthesised with the docs site's synth
// (site/src/lib/synth.ts render()): same parameters, same buffers as the docs and the panel's sound bank.
import { render } from '../../../site/src/lib/synth';

type Params = Record<string, unknown> & { engine: string; track?: number };
interface Variant { id: string; name: string; engine: string; about: string; defaults: Record<string, unknown> }
interface Type { id: string; name: string; for: string; when?: string; not?: string; variants: Record<string, Record<string, unknown>> }
interface Section { id: string; title: string; items: string[] }
interface UseRow { id: string; name: string; section: string; file: string; sounds: string[]; via: string[]; when: string; calls: { sound: string; line: number; how: string }[] }
interface Generated { components: UseRow[]; byType: Record<string, string[]>; unused: string[]; warnings: string[] }
export interface SoundDoc {
  default: string; base: Record<string, unknown>; variants: Variant[]; types: Type[];
  pitch?: { low: number; high: number }; rules?: { sections: Section[] }; generated?: Generated | null; generatedError?: string;
}
/** a live WASM component for the pitch sample (app.ts: a Stage, sounds routed to playUi); null = not built */
export type Mount = (id: string, props: Record<string, unknown>) => HTMLCanvasElement | null;

const h = (tag: string, cls = '', text = '') => { const e = document.createElement(tag); if (cls) e.className = cls; if (text) e.textContent = text; return e; };

/** One sound's full parameters: base + variant defaults + the type's own values for that variant (minus `use`). */
export function params(doc: SoundDoc, type: Type, v: Variant): Params {
  const { use: _use, ...own } = type.variants[v.id] ?? {};
  return { ...doc.base, ...v.defaults, ...own, engine: v.engine } as Params;
}

let ac: AudioContext | null = null, master: GainNode | null = null;
const cache = new Map<string, Promise<AudioBuffer>>();
function ctx() {
  if (!ac) { ac = new AudioContext({ latencyHint: 'interactive' }); master = ac.createGain(); master.gain.value = 0.7; master.connect(ac.destination); }
  if (ac.state === 'suspended') void ac.resume();
  return ac;
}
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
// sound.json "pitch": {low, high} = semitones at the bottom / top of a control's range for pitched (track > 0) sounds;
// without it the old symmetric (pos - 0.5) x track applies. `saved` = the token; `pitchRange` = what plays: the Sound
// page's Low / High preview (play-only, never written back; changing the token is a code edit he
// asks for). The preview lives in this page session only: kept across a page rebuild (theme switch), gone on reload.
let saved: { low: number; high: number } | null = null;
let pitchRange: { low: number; high: number } | null = null;
let draft = false;  // the page is previewing other Low / High values than the token
// step ticks arrive in bursts: the page drains a frame's events at once, and on a slow device (the Shelly X1i: few
// frames while a dial is dragged) several ticks land in one drain and would start together as one clump. Ticks are
// queued TICK_GAP apart on the audio clock instead; one more than TICK_LAG behind is dropped (the finger moved on).
const TICK_GAP = 0.028, TICK_LAG = 0.12;
let tickAt = 0;
async function playOnce(p: Params, value?: number) {
  const buf = await buffer(p), src = ctx().createBufferSource();
  let when = 0;
  if (value !== undefined && p.track) {
    const now = ac!.currentTime;
    when = Math.max(now, tickAt);
    if (when - now > TICK_LAG) return;
    tickAt = when + TICK_GAP;
  }
  src.buffer = buf;
  // value-tracking sounds (slider tick): pitch follows the 0-100 value, as in @facet/sounds play()
  if (value !== undefined && p.track) {
    const semis = pitchRange ? pitchRange.low + (value / 100) * (pitchRange.high - pitchRange.low) : ((value - 50) / 100) * p.track;
    src.playbackRate.value = Math.pow(2, semis / 12);
  }
  src.connect(master!);
  src.start(when);
}
const wait = (ms: number) => new Promise((r) => setTimeout(r, ms));
const SWEEP = [10, 25, 40, 55, 70, 85, 100];
/** play a type as heard: ticks = a short rising sweep (the pitch follows the value), landmarks = both ends */
async function play(doc: SoundDoc, t: Type, v: Variant) {
  const p = params(doc, t, v);
  if (t.id === 'slider-tick') { for (const x of SWEEP) { void playOnce(p, x); await wait(70); } return; }
  if (t.id === 'slider-landmark') { void playOnce(p, 0); await wait(260); return playOnce(p, 100); }
  return playOnce(p);
}
const playMs = (id: string) => (id === 'slider-tick' ? 520 : id === 'slider-landmark' ? 420 : 260);

// gaps after each sound when a pack column header plays the whole pack (same as the Facet docs page)
const GAP: Record<string, number> = { 'slider-tick': 200, 'slider-landmark': 300, confirm: 750, 'timer-done': 1300, notify: 900 };
let run = 0, build = 0;
let unmark: (() => void) | null = null;
const flash = (el: Element, ms: number) => { el.classList.add('on'); setTimeout(() => el.classList.remove('on'), ms); };
const TRI = '<svg viewBox="0 0 24 24" width="12" height="12" aria-hidden="true"><path d="M8 5.5v13l10.5-6.5z" fill="currentColor"/></svg>';
const playBtn = (label: string, onClick: (b: HTMLButtonElement) => void) => {
  const b = h('button', 'wb-snd-play') as HTMLButtonElement;
  b.type = 'button';
  b.title = label;
  b.setAttribute('aria-label', label);
  b.innerHTML = TRI;
  b.onclick = () => onClick(b);
  return b;
};
const section = (title: string, lead = '') => {
  const s = h('section', 'wb-doc-sec');
  s.append(h('h2', '', title));
  if (lead) s.append(h('p', 'wb-lead', lead));
  return s;
};
const callout = (kind: 'info' | 'bad', text: string) => { const c = h('div', `wb-callout ${kind}`); c.append(h('span', 'dot'), h('span', '', text)); return c; };
const compHref = (r: { section: string; id: string }) => `/wasm/${r.section}/${r.id}${location.search}`;
const semi = (n: number) => `${n > 0 ? '+' : n < 0 ? '−' : ''}${Math.abs(n)}`;

export async function soundPage(main: HTMLElement, head: HTMLElement, mount?: Mount) {
  const my = ++build;
  main.replaceChildren(head, h('p', 'wb-lead', 'Loading sounds...'));
  let doc: SoundDoc;
  try { doc = await soundDoc(true); } catch (e) { main.replaceChildren(head, h('p', 'wb-lead', `No sound list: ${String(e)}`)); return; }
  if (my !== build) return;  // the page was rebuilt while loading
  const sel = () => pickVariant(doc);
  const typeOf = (id: string) => doc.types.find((t) => t.id === id);
  const playType = (id: string, b?: Element) => { const t = typeOf(id); if (!t) return; run++; if (b) flash(b, playMs(id)); void play(doc, t, sel()); };

  const lead = h('p', 'wb-lead', 'Sound gives a flat glass panel its feel: each sound stands for one meaning, plays only when a person did something (or the house has news), and every pack voices the same ten meanings. The rules below are the contract; the tables are generated from sound.json and the component code. Play buttons use the selected pack (sidebar Sound).');

  // 1 rules
  const rules = section('Rules', 'Why Facet has sound and when it may play. Same text as sound.json rules (the docs site reads it there).');
  const grid = h('div', 'wb-snd-rules');
  for (const s of doc.rules?.sections ?? []) {
    const card = h('div', 'wb-snd-rule'), ul = h('ul');
    card.append(h('h3', '', s.title));
    for (const it of s.items) ul.append(h('li', '', it));
    card.append(ul);
    grid.append(card);
  }
  rules.append(grid);

  // 2 every type x pack (the docs "Try them" grid); a pack's header plays all its sounds in a row and selects it
  const all = section('The sounds', `${doc.types.length} sound types x ${doc.variants.length} packs, synthesised in the browser with the approved Facet engine (procedural, no samples). A pack name plays its sounds in a row and selects it. Ticks play a short rising sweep, the landmark both ends.`);
  const cells = new Map<string, HTMLButtonElement>(), packBtns = new Map<string, HTMLButtonElement>();
  const playCell = (t: Type, v: Variant) => { const b = cells.get(`${t.id}/${v.id}`); if (b) flash(b, playMs(t.id)); return play(doc, t, v); };
  const table = h('table', 'wb-snd'), thead = h('thead'), hr = h('tr'), tbody = h('tbody');
  hr.append(h('th', 'name', 'Sound'), h('th', 'for', 'What it\'s for'));
  for (const v of doc.variants) {
    const th = h('th', 'pack'), b = h('button', 'wb-snd-pack', v.name) as HTMLButtonElement;
    b.type = 'button';
    b.title = `${v.name} (${v.engine}${v.id === doc.default ? ', default' : ''}): ${v.about} Click = play all ${doc.types.length}.`;
    if (v.id === doc.default) b.classList.add('def');
    packBtns.set(v.id, b);
    b.onclick = async () => {
      setSoundVariant(v.id === doc.default ? '' : v.id);
      const token = ++run;
      flash(b, 1200);
      for (const t of doc.types) { if (token !== run) return; await playCell(t, v); await wait(GAP[t.id] ?? 420); }
    };
    th.append(b);
    hr.append(th);
  }
  thead.append(hr);
  for (const t of doc.types) {
    const tr = h('tr'), name = h('th', 'name', t.name);
    name.title = t.id;
    tr.append(name, h('td', 'for', t.for));
    for (const v of doc.variants) {
      const td = h('td', 'cell'), use = String(t.variants[v.id]?.use ?? '');
      const b = playBtn(`${v.name}: ${t.name}${use ? ` (${use})` : ''}`, () => { run++; void playCell(t, v); });
      cells.set(`${t.id}/${v.id}`, b);
      td.append(b);
      tr.append(td);
    }
    tbody.append(tr);
  }
  table.append(thead, tbody);
  const scroll = h('div', 'wb-snd-scroll');
  scroll.append(table);
  all.append(scroll);

  // 3 when each type plays (sound.json types[].when / not) + who plays it (generated)
  const gen = doc.generated ?? null, rowsById = new Map((gen?.components ?? []).map((r) => [r.id, r]));
  const when = section('When each sound plays', 'Per sound: when it plays, when it must not, and the components that play it (from the code).');
  const tlist = h('div', 'wb-snd-types');
  for (const t of doc.types) {
    const card = h('div', 'wb-snd-type'), top = h('div', 'wb-snd-type-top');
    const b = playBtn(`Play ${t.name} (${sel().name})`, (el) => playType(t.id, el));
    top.append(b, h('strong', '', t.name), h('code', '', t.id));
    card.append(top, h('p', 'wb-snd-for', t.for));
    const dl = h('dl');
    const add = (k: string, v: Node | string) => { const dd = h('dd'); dd.append(v); dl.append(h('dt', '', k), dd); };
    if (t.when) add('When', t.when);
    if (t.not) add('Not', t.not);
    const users = gen?.byType[t.id] ?? [];
    const chips = h('span', 'wb-snd-users');
    if (users.length) for (const id of users) { const r = rowsById.get(id); const a = h('a', 'wb-snd-chip', r?.name ?? id) as HTMLAnchorElement; if (r) a.href = compHref(r); chips.append(a); }
    else chips.append(h('span', 'wb-snd-none', gen ? 'no component yet' : 'not generated'));
    add('Used by', chips);
    card.append(dl);
    tlist.append(card);
  }
  when.append(tlist);

  // 4 what uses what (generated from the code: fw::sound / step_ticks / st::blocked / fw::find + sound.json usage lines)
  const uses = section('What uses what', 'Every component that makes a sound, generated from its code on each load (renderer/tools/gen_sound_usage.mjs: fw::sound, fw::step_ticks, fw::st::blocked, and parts built with fw::find), with the hand-written when-line from sound.json usage. Press a sound to hear it.');
  if (!gen) uses.append(callout('bad', `Not generated: ${doc.generatedError ?? 'no data'}`));
  else {
    const list = h('div', 'wb-snd-uses');
    const hd = h('div', 'wb-snd-use wb-snd-use-head');
    hd.append(h('span', '', 'Component'), h('span', '', 'Sounds'), h('span', '', 'When'));
    list.append(hd);
    for (const r of gen.components) {
      const row = h('div', 'wb-snd-use'), nm = h('div', 'wb-snd-use-name');
      const a = h('a', '', r.name) as HTMLAnchorElement;
      a.href = compHref(r);
      nm.append(a);
      const lines = (s: string) => r.calls.filter((c) => c.sound === s).map((c) => c.line);
      const src = h('span', 'wb-snd-src', r.file.replace(/^renderer\//, ''));
      src.title = r.calls.map((c) => `${c.sound}: line ${c.line} (${c.how})`).join('\n');
      nm.append(src);
      if (r.via.length) nm.append(h('span', 'wb-snd-src', `via ${r.via.map((v) => rowsById.get(v)?.name ?? v).join(', ')}`));
      const ss = h('div', 'wb-snd-sounds');
      for (const s of r.sounds) {
        const c = h('button', 'wb-snd-chip wb-snd-chip-play') as HTMLButtonElement;
        c.type = 'button';
        c.innerHTML = TRI;
        c.append(document.createTextNode(s));
        c.title = `Play ${s} (${sel().name}); ${r.file} line ${lines(s).join(', ')}`;
        c.onclick = () => playType(s, c);
        ss.append(c);
      }
      if (!r.sounds.length) ss.append(h('span', 'wb-snd-none', 'through its parts'));
      row.append(nm, ss, h('div', 'wb-snd-when', r.when || '(no line in sound.json usage)'));
      list.append(row);
    }
    uses.append(list);
    if (gen.unused.length) uses.append(callout('info', `Not played by any component yet: ${gen.unused.join(', ')} (screens and house events will: a saved setting, a finished timer).`));
    for (const w of gen.warnings) uses.append(callout('bad', w));
  }

  // 5 pitch range (step ticks): Low / High by ear, a real Slider + Dial and a sweep, Save -> sound.json pitch
  const pitch = section('Pitch range', 'Step ticks rise with the value: linear from Low (semitones at the bottom of the range) to High (at the top). Move Low and High to preview another range by ear: drag the Slider or the Dial, or play a sweep. Play-only: it changes what you hear on this page until you reload, never the token (sound.json pitch, changed by a code edit).');
  const box = h('div', 'wb-snd-pitch'), ctl = h('div', 'wb-snd-pitch-ctl');
  const tok = () => saved ?? { low: -6, high: 3 };
  const cur = () => pitchRange ?? tok();
  const rng = (label: string, key: 'low' | 'high', min: number, max: number) => {
    const row = h('label', 'wb-ctl'), inp = h('input') as HTMLInputElement, out = h('output');
    Object.assign(inp, { type: 'range', min: String(min), max: String(max), step: '0.5', value: String(cur()[key]) });
    inp.setAttribute('aria-label', `${label} (semitones)`);
    const show = () => { out.textContent = `${semi(Number(inp.value))} st`; };
    inp.oninput = () => { pitchRange = { ...cur(), [key]: Number(inp.value) }; draft = true; show(); state(); };
    // letting go plays the end it sets: the landmark there, at the new pitch
    inp.onchange = () => { const t = typeOf('slider-landmark'); if (t) { run++; void playOnce(params(doc, t, sel()), key === 'low' ? 0 : 100); } };
    show();
    row.append(h('span', '', label), inp, out);
    return { row, inp, show };
  };
  const lo = rng('Low', 'low', -12, 0), hi = rng('High', 'high', 0, 12);
  const span = h('p', 'wb-snd-pitch-read'), msg = h('span', 'wb-snd-msg');
  const state = () => {
    const c = cur(), s = tok(), dirty = c.low !== s.low || c.high !== s.high;
    span.textContent = `Bottom ${semi(c.low)} st, top ${semi(c.high)} st: a span of ${c.high - c.low} semitones (an octave is 12).`;
    resetB.disabled = !dirty;
    msg.textContent = dirty ? `Preview only. Token (sound.json pitch): ${semi(s.low)} / ${semi(s.high)}` : `Token values (sound.json pitch): ${semi(s.low)} / ${semi(s.high)}`;
    draft = dirty;
  };
  const set = (low: number, high: number) => { pitchRange = { low, high }; lo.inp.value = String(low); hi.inp.value = String(high); lo.show(); hi.show(); state(); };
  const btn = (text: string, cls = '') => { const b = h('button', `wb-snd-btn ${cls}`, text) as HTMLButtonElement; b.type = 'button'; return b; };
  const upB = btn('Sweep up'), downB = btn('Sweep down'), resetB = btn('Reset to token');
  const sweep = async (dir: 1 | -1, b: Element) => {
    const tk = typeOf('slider-tick'), lm = typeOf('slider-landmark');
    if (!tk || !lm) return;
    const token = ++run, v = sel(), pt = params(doc, tk, v), pl = params(doc, lm, v);
    flash(b, 1500);
    for (let i = 0; i <= 20; i++) {
      if (token !== run) return;
      const x = dir > 0 ? i * 5 : 100 - i * 5;
      void playOnce(x === 0 || x === 100 ? pl : pt, x);
      await wait(70);
    }
  };
  upB.onclick = () => void sweep(1, upB);
  downB.onclick = () => void sweep(-1, downB);
  resetB.onclick = () => { const t = tok(); set(t.low, t.high); };
  const bar = h('div', 'wb-snd-pitch-bar');
  bar.append(upB, downB, resetB, msg);
  ctl.append(lo.row, hi.row, span, bar);
  const live = h('div', 'wb-snd-live');
  if (mount) {
    for (const [id, props, label] of [
      ['slider', { value: 40, step: 1, width: 256 }, 'Slider, 100 steps'],
      ['dial', { value: 40, step: 1, sweep: false }, 'Dial, 100 steps'],
    ] as [string, Record<string, unknown>, string][]) {
      const fig = h('figure', 'wb-var'), cv = mount(id, props);
      if (!cv) continue;
      fig.append(cv, h('figcaption', '', label));
      live.append(fig);
    }
  }
  box.append(ctl, live);
  pitch.append(box);
  state();

  // 6 packs
  const packs = section('Packs', 'A pack voices all ten sounds with one character; selecting one here or in the sidebar changes every sound on the workbench.');
  const legend = h('div', 'wb-snd-legend');
  for (const v of doc.variants) {
    const row = h('div', 'wb-snd-vrow'), n = h('strong', '', v.name);
    if (v.id === doc.default) n.append(h('span', 'wb-tag', 'default'));
    row.append(n, h('span', '', `${v.engine}: ${v.about}`));
    legend.append(row);
  }
  packs.append(legend);

  const mark = () => { const s = sel().id; for (const [id, b] of packBtns) { b.classList.toggle('sel', id === s); b.setAttribute('aria-pressed', String(id === s)); } };
  mark();
  unmark?.();
  unmark = onSoundVariant(mark);
  const body = h('div', 'wb-doc');
  body.append(rules, all, when, uses, pitch, packs);
  main.replaceChildren(head, lead, body);
}

// The selected sound variant (pack): ONE workbench setting shared by the Sound page and component sounds (Events
// panel taps) and sent with Show on device (`sound`, for later device playback). ?sound=<id> wins, else the last pick
// (localStorage wb.soundVariant), else the doc default. '' = default.
let variant = (() => {
  const u = new URLSearchParams(location.search).get('sound');
  if (u !== null) return u;
  try { return localStorage.getItem('wb.soundVariant') ?? ''; } catch { return ''; }
})();
const subs = new Set<() => void>();
export const soundVariant = () => variant;
export function setSoundVariant(id: string) {
  variant = id;
  try { localStorage.setItem('wb.soundVariant', id); } catch { /* private mode */ }
  const u = new URL(location.href);
  if (id) u.searchParams.set('sound', id); else u.searchParams.delete('sound');
  history.replaceState(null, '', u.pathname + u.search);
  for (const f of subs) f();
}
export const onSoundVariant = (f: () => void) => { subs.add(f); return () => subs.delete(f); };
let docP: Promise<SoundDoc> | null = null;
export const soundDoc = (fresh = false): Promise<SoundDoc> => (fresh || !docP ? (docP = fetch('/api/wasm/sound', { cache: 'no-store' }).then((r) => r.json()).then((d: SoundDoc) => {
  const pr = d.pitch;
  saved = pr && typeof pr.low === 'number' && typeof pr.high === 'number' ? { low: pr.low, high: pr.high } : null;
  if (!draft) pitchRange = saved;  // a preview on the Sound page survives a page rebuild (a browser reload starts from the token)
  return d;
})) : docP);
/** the selected variant resolved against the doc (unknown/empty -> the doc default) */
export const pickVariant = (doc: SoundDoc) => doc.variants.find((x) => x.id === variant) ?? doc.variants.find((x) => x.id === doc.default) ?? doc.variants[0];

// UI sounds requested by components (fw::sound(obj, "<type id>") -> fw_events): played in the browser with the
// selected variant of tokens/sound.json, the same synth as the table above. The panels have no audio path yet.
// detail = the pitch position 0..1 of a pitched sound (fw::sound(obj, id, pos); step ticks: step / n), "" = none:
// the pitch runs from sound.json pitch.low to pitch.high semitones (the Sound page preview while one is set).
export async function playUi(id: string, detail = '') {
  const doc = await soundDoc(), t = doc.types.find((x) => x.id === id), v = pickVariant(doc);
  const pos = detail === '' ? NaN : Number(detail);
  if (t && v) await playOnce(params(doc, t, v), Number.isFinite(pos) ? pos * 100 : undefined);
}
