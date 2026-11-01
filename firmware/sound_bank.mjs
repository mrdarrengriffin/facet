#!/usr/bin/env node
// The UI sounds in the panel's flash (boards with a speaker: esphome/packages/audio_*.yaml). Renders every sound of
// renderer/tokens/sound.json (each type x each pack) with the synth the docs site plays (site/src/lib/synth.ts,
// render(): Web Audio OfflineAudioContext in headless Chromium, the same cleanup and loudness alignment), as mono 16-bit
// PCM, and writes the facet_audio component's bank. Generated, never committed (esphome/.gitignore):
//   node firmware/sound_bank.mjs [--out esphome/components/facet_audio] [--rate 24000] [--check]
// --check: exit 0 = the bank in --out is up to date, 1 = stale (no browser).
// --out gets facet_sounds.bin (the clips back to back), facet_sounds.cpp (links it into rodata with .incbin, absolute
// path) and facet_sounds_gen.h (namespace facet_audio::bank: RATE, PACKS, TYPES, Clip {off, n, pitched},
// CLIPS[pack][type], PITCH_LOW / PITCH_HIGH semitones). Chromium: CHROMIUM_PATH, else /usr/bin/chromium, else
// Playwright's own (renderer/tools: npm ci, npx playwright install chromium).
// The rate: 24 kHz (the ES8311 at MCLK 256 fs; the speaker gives nothing above ~10 kHz). Pitched sounds (track > 0:
// slider-tick, slider-landmark) are pitched on the panel (sound.json `pitch` low..high over the control's range).
import { createHash } from 'node:crypto';
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { createRequire, stripTypeScriptTypes } from 'node:module';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

process.removeAllListeners('warning');  // stripTypeScriptTypes is "experimental" in Node 24
const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = dirname(HERE);
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const tokens = join(ROOT, 'renderer/tokens/sound.json');
const synthTs = join(ROOT, 'site/src/lib/synth.ts');
const out = resolve(arg('--out', join(ROOT, 'esphome/components/facet_audio')));
const RATE = Number(arg('--rate', '24000'));
const doc = JSON.parse(readFileSync(tokens, 'utf8'));
const key = createHash('sha1').update(readFileSync(tokens)).update(String(RATE)).update(readFileSync(fileURLToPath(import.meta.url))).update(readFileSync(synthTs));
const hash = key.digest('hex').slice(0, 12);
const genH = join(out, 'facet_sounds_gen.h');
const fresh = existsSync(genH) && existsSync(join(out, 'facet_sounds.bin')) && readFileSync(genH, 'utf8').includes(`source ${hash}`);
if (process.argv.includes('--check')) process.exit(fresh ? 0 : 1);
if (fresh) { console.log(`sound bank up to date (${hash})`); process.exit(0); }
const t0 = Date.now();

// one sound = base + the pack's defaults + the type's own values for that pack (minus `use`), engine from the pack
// (what the docs site plays)
const params = (type, v) => { const { use: _u, ...own } = type.variants[v.id] ?? {}; return { ...doc.base, ...v.defaults, ...own, engine: v.engine }; };
const jobs = [];
for (const v of doc.variants) for (const t of doc.types) jobs.push({ pack: v.id, type: t.id, p: params(t, v) });

// render in Chromium: the synth (no imports) with its types stripped, as one module
const synth = stripTypeScriptTypes(readFileSync(synthTs, 'utf8'));
const { chromium } = createRequire(join(ROOT, 'renderer/tools/package.json'))('playwright');
const exe = process.env.CHROMIUM_PATH ?? '/usr/bin/chromium';
const browser = await chromium.launch(existsSync(exe) ? { executablePath: exe } : {});
const page = await browser.newPage();
await page.setContent('<!doctype html><title>bank</title>');
await page.addScriptTag({ type: 'module', content: `${synth}\nsetLevels(${JSON.stringify(doc.levels ?? {})});\nwindow.__render = render;\n` });
await page.waitForFunction(() => typeof window.__render === 'function');
const pcm = await page.evaluate(async ({ jobs, RATE }) => {
  const res = [];
  for (const j of jobs) {
    const r = await window.__render(j.p, RATE);
    const i16 = new Int16Array(r.data.length);
    for (let i = 0; i < r.data.length; i++) i16[i] = Math.round(Math.max(-1, Math.min(1, r.data[i])) * 32767);
    let s = '';
    const u8 = new Uint8Array(i16.buffer);
    for (let i = 0; i < u8.length; i += 0x8000) s += String.fromCharCode(...u8.subarray(i, i + 0x8000));
    res.push(btoa(s));
  }
  return res;
}, { jobs, RATE });
await browser.close();

// the bank: clips back to back (each padded to 4 bytes), the table in a header
const parts = [];
let off = 0;  // in samples
const clips = jobs.map((j, i) => {
  const b = Buffer.from(pcm[i], 'base64');
  const n = b.length / 2, pad = (4 - (b.length % 4)) % 4;
  parts.push(b, Buffer.alloc(pad));
  const c = { ...j, off, n, pitched: Number(j.p.track) > 0 };
  off += n + pad / 2;
  return c;
});
const blob = Buffer.concat(parts);
mkdirSync(out, { recursive: true });
const bin = join(out, 'facet_sounds.bin');
writeFileSync(bin, blob);
const packs = doc.variants.map((v) => v.id), types = doc.types.map((t) => t.id);
writeFileSync(genH, `// GENERATED by firmware/sound_bank.mjs from renderer/tokens/sound.json (source ${hash}). Do not edit.
#pragma once
#include <cstdint>
namespace facet_audio {
namespace bank {
constexpr uint32_t RATE = ${RATE};
constexpr int NPACKS = ${packs.length}, NTYPES = ${types.length};
inline constexpr const char *PACKS[NPACKS] = {${packs.map((p) => JSON.stringify(p)).join(', ')}};
inline constexpr const char *TYPES[NTYPES] = {${types.map((t) => JSON.stringify(t)).join(', ')}};
constexpr int DEFAULT_PACK = ${Math.max(0, packs.indexOf(doc.default))};
constexpr float PITCH_LOW = ${Number(doc.pitch?.low ?? -6).toFixed(2)}f, PITCH_HIGH = ${Number(doc.pitch?.high ?? 6).toFixed(2)}f;  // semitones at pos 0 / 1
struct Clip { uint32_t off, n; bool pitched; };  // off / n in samples (mono int16)
inline constexpr Clip CLIPS[NPACKS][NTYPES] = {
${packs.map((p) => `  {${clips.filter((c) => c.pack === p).map((c) => `{${c.off}, ${c.n}, ${c.pitched}}`).join(', ')}},  // ${p}`).join('\n')}
};
extern "C" const int16_t facet_sounds_pcm[];
inline const int16_t *pcm() { return facet_sounds_pcm; }
}  // namespace bank
}  // namespace facet_audio
`);
writeFileSync(join(out, 'facet_sounds.cpp'), `// GENERATED by firmware/sound_bank.mjs: links facet_sounds.bin (${blob.length} bytes, source ${hash}) into rodata.
__asm__(".section .rodata.facet_sounds,\\"a\\"\\n.balign 4\\n.global facet_sounds_pcm\\nfacet_sounds_pcm:\\n.incbin \\"${bin}\\"\\n.previous\\n");
`);
const longest = clips.reduce((a, c) => (c.n > a.n ? c : a));
console.log(`sound bank: ${clips.length} sounds (${packs.length} packs x ${types.length} types) at ${RATE / 1000} kHz, ${(blob.length / 1024).toFixed(0)} KB, longest ${longest.pack}/${longest.type} ${(longest.n / RATE * 1000).toFixed(0)} ms, in ${((Date.now() - t0) / 1000).toFixed(1)} s`);
