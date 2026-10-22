#!/usr/bin/env node
// "What uses what sound", generated from the code (Foundations > Sound, Darren 2026-10-22): scans every FW_COMPONENT
// file in components/ and foundations/ for the calls that make a sound and resolves each to sound.json type ids:
//   fw::sound(obj, "<id>")          -> <id> (a ternary of two literals -> both)
//   fw::sound(obj, fw::st::X_SOUND) -> states.json (TAP = rest.sound, ON = on.sound, OFF = on.off_sound, BLOCKED = disabled.sound)
//   fw::sound(obj, fw::toast::sound)-> layout.json toast.sound.sound
//   fw::step_ticks(...)             -> slider-tick + slider-landmark (the core limiter, pitched by the value)
//   fw::st::blocked(...)            -> error (a blocked part, on finger down)
//   fw::find("<id>")                -> "via": the part is built from that component, so it also plays that one's sounds
// The hand-written "when" line per component is sound.json `usage` (id -> text); this script checks the two against
// each other (a component that makes a sound without a line, or a line for one that makes none) and lists the
// mismatches as `warnings`. Every sound id is checked against sound.json types.
// Run: `node tools/gen_sound_usage.mjs` writes tokens/sound_usage.json (only when it changes; the docs site reads it);
// `--json` prints it instead (the hub API runs this on every request, so the workbench is always current);
// `--check` exits 1 when tokens/sound_usage.json is out of date.
import { readFileSync, writeFileSync, readdirSync, existsSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url)), ROOT = join(HERE, '..');
const read = (f) => JSON.parse(readFileSync(join(ROOT, 'tokens', f), 'utf8'));
const sound = read('sound.json'), states = read('states.json'), layout = read('layout.json');
const types = new Set(sound.types.map((t) => t.id));
const ST = {
  TAP_SOUND: states.states?.rest?.sound, ON_SOUND: states.states?.on?.sound,
  OFF_SOUND: states.states?.on?.off_sound, BLOCKED_SOUND: states.states?.disabled?.sound,
};
const warnings = [];
const resolve = (expr, where) => {
  const ids = [...expr.matchAll(/"([a-z-]+)"/g)].map((m) => m[1]);
  for (const m of expr.matchAll(/st::([A-Z_]+_SOUND)/g)) ids.push(ST[m[1]] ?? `?${m[1]}`);
  if (/toast::sound/.test(expr)) ids.push(layout.toast?.sound?.sound ?? '?toast');
  if (!ids.length) warnings.push(`${where}: cannot resolve sound "${expr.trim()}"`);
  return ids;
};

const rows = [];
for (const dir of ['components', 'foundations']) {
  for (const f of readdirSync(join(ROOT, dir)).filter((x) => x.endsWith('.cpp')).sort()) {
    const src = readFileSync(join(ROOT, dir, f), 'utf8');
    const reg = src.match(/FW_COMPONENT\((\w+),\s*"([^"]+)",\s*"(\w+)"/);
    if (!reg) continue;
    const [, id, name, section] = reg;
    const calls = [];  // { sound, line, how }
    const lineOf = (i) => src.slice(0, i).split('\n').length;
    const code = src.replace(/\/\/[^\n]*/g, (c) => ' '.repeat(c.length));  // comments out, offsets kept
    for (const m of code.matchAll(/(?<![\w.>])(?:fw::)?sound\(\s*(?:[^,()]|\([^()]*\))+,\s*((?:[^;()]|\([^()]*\))+?)\s*(?:,\s*[^;]+)?\)\s*;/g))
      for (const s of resolve(m[1], `${dir}/${f}:${lineOf(m.index)}`)) calls.push({ sound: s, line: lineOf(m.index), how: 'sound' });
    for (const m of code.matchAll(/(?<![\w.>])(?:fw::)?step_ticks\(/g))
      for (const s of ['slider-tick', 'slider-landmark']) calls.push({ sound: s, line: lineOf(m.index), how: 'step_ticks' });
    for (const m of code.matchAll(/st::blocked\(/g)) calls.push({ sound: ST.BLOCKED_SOUND ?? 'error', line: lineOf(m.index), how: 'blocked' });
    const via = [...new Set([
      ...[...code.matchAll(/fw::find\(\s*(?:play \? )?"(\w+)"(?:\s*:\s*"(\w+)")?/g)].flatMap((m) => [m[1], m[2]]),
      ...[...code.matchAll(/(?<![\w.>])build\(\s*"(\w+)"/g)].map((m) => m[1]),  // a local build("<id>", ...) wrapper around fw::find
    ].filter(Boolean))];
    for (const c of calls) if (!types.has(c.sound)) warnings.push(`${dir}/${f}:${c.line}: unknown sound "${c.sound}" (sound.json types)`);
    const sounds = [...new Set(calls.map((c) => c.sound))];
    rows.push({ id, name, section, file: `renderer/${dir}/${f}`, sounds, calls, via });
  }
}
// "via": keep only parts that make a sound (themselves or through their own parts); a row's own list stays what its
// own code plays. A part that only plays through others is listed when sound.json usage has a line for it (Header,
// Picker row); demos that merely show a Button (Focus ring, Corner ticks, Motion) are not.
const notes = sound.usage ?? {};
const loud = new Set(rows.filter((r) => r.sounds.length).map((r) => r.id));
for (let grew = true; grew;) { grew = false; for (const r of rows) if (!loud.has(r.id) && r.via.some((v) => loud.has(v))) { loud.add(r.id); grew = true; } }
for (const r of rows) r.via = r.via.filter((v) => loud.has(v) && v !== r.id);
const used = rows.filter((r) => r.sounds.length || (r.via.length && notes[r.id]));
for (const r of used) if (r.sounds.length && !notes[r.id]) warnings.push(`${r.id} makes a sound (${r.sounds.join(', ')}) but sound.json usage has no line for it`);
for (const id of Object.keys(notes)) if (!id.startsWith('$') && !used.some((r) => r.id === id)) warnings.push(`sound.json usage.${id}: no component by that id makes a sound`);
const byType = Object.fromEntries(sound.types.map((t) => [t.id, used.filter((r) => r.sounds.includes(t.id)).map((r) => r.id)]));

const out = {
  $comment: 'GENERATED by tools/gen_sound_usage.mjs from the fw::sound / fw::step_ticks / fw::st::blocked / fw::find calls in components/ and foundations/ (+ the hand-written when-lines in sound.json `usage`). Do not edit; run the script.',
  components: used.map(({ id, name, section, file, sounds, via, calls }) => ({ id, name, section, file, sounds, via, when: notes[id] ?? '', calls: calls.map(({ sound: s, line, how }) => ({ sound: s, line, how })) })),
  byType,
  unused: sound.types.map((t) => t.id).filter((id) => !byType[id].length),
  warnings,
};
const text = JSON.stringify(out, null, 2) + '\n';
const file = join(ROOT, 'tokens', 'sound_usage.json');
if (process.argv.includes('--json')) process.stdout.write(text);
else if (process.argv.includes('--check')) { const ok = existsSync(file) && readFileSync(file, 'utf8') === text; if (!ok) console.error('tokens/sound_usage.json is out of date: run node tools/gen_sound_usage.mjs'); process.exit(ok ? 0 : 1); }
else {
  if (!existsSync(file) || readFileSync(file, 'utf8') !== text) writeFileSync(file, text);
  console.log(`sound usage: ${used.length} components, ${warnings.length} warning(s)${warnings.length ? `\n  ${warnings.join('\n  ')}` : ''}`);
}
