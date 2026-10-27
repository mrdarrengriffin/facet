// Theme parser fuzz through the WASM build (theme engine design S4; the native ASan run is tools/theme_fuzz.sh,
// 100k). Seeded mutations of the built-in themes: a Skin XOR errors, never both, never neither; at most 32 errors,
// each with a message; a refused file through the live path changes neither the live Skin nor its generation; an
// accepted file loads again to the same digest (pixels: the apply path's tests, T2). The break-it run proves the live-path check can fail.
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { seeded } from './_release.mjs';

const R = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const SEEDS = ['facet', 'blockcraft', 'win98'].map((id) => readFileSync(join(R, 'themes', `${id}.json`), 'utf8'));
const lab = (t, c, a = '') => JSON.parse(t.b.str(c, (pc) => t.b.str(a, (pa) => t.b.m.UTF8ToString(t.b.m._fw_lab(-1, pc, pa)))));
const PIECES = ['{', '}', '[', ']', ',', ':', '"', '"#FFFFFF"', '"#12345"', '1e309', '-1', '0.5', '999999', 'null', 'true', 'false',
  '"\\u0000"', '"\\ud800"', '"\\x"', '[[[[[[[[[[1]]]]]]]]]]', '"schema": 2', '"modes": []', '"size": {"layout:inset": [8, 8]}',
  '"edges": [{}, {}, {}, {}, {}, {}, {}, {}, {}]', '"pressed": {"pressed": {}}', '"accents": []', '"type": {"title": [99, 99]}',
  '"parts": {"card": {"face": {"checker": ["text", "bg"]}}}', '\t', '\n', '/* c */', 'é', '💡'];
const NAMES = ['bg', 'surface', 'text', 'accent', 'accent-text', 'card', 'control', 'knob', 'pressed', 'on', 'edges', 'face', 'radius:card', 'stroke:pixel', 'layout:inset', 'top-left', 'checker'];
function mutate(s, r) {
  for (let k = r.int(1, 4); k > 0; k--) {
    const n = s.length, at = r.int(0, Math.max(0, n - 1)), len = r.int(1, 24);
    switch (r.int(0, 8)) {
      case 0: s = s.slice(0, at) + String.fromCharCode(s.charCodeAt(at) ^ (1 << r.int(0, 6))) + s.slice(at + 1); break;
      case 1: s = s.slice(0, at) + s.slice(at + len); break;
      case 2: s = s.slice(0, at) + s.slice(at, at + len) + s.slice(at); break;
      case 3: s = s.slice(0, at) + r.pick(PIECES) + s.slice(at); break;
      case 4: s = s.slice(0, at); break;
      case 5: { const q = s.indexOf('"', at), e = q >= 0 ? s.indexOf('"', q + 1) : -1; if (e > q) s = s.slice(0, q + 1) + r.pick(NAMES) + s.slice(e); break; }
      case 6: { const m = /\d+/.exec(s.slice(at)); if (m) s = s.slice(0, at + m.index) + r.int(-40, 400) + s.slice(at + m.index + m[0].length); break; }
      case 7: { const d = r.int(1, 12); s = '['.repeat(d) + s + ']'.repeat(d); break; }
      default: s = s.slice(0, at) + ' '.repeat(r.int(1, 3000)) + s.slice(at);
    }
  }
  return s;
}
function run(t, n, seed, { partial = false } = {}) {
  const r = seeded(seed), out = { ok: 0, refused: 0, bad: [], changed: 0 };
  t.eq(lab(t, 'theme_apply', SEEDS[2]).ok, true, 'win98 live');
  const live = lab(t, 'theme_live');
  if (partial) lab(t, 'theme_break', 'partial');
  for (let i = 0; i < n; i++) {
    const s = mutate(r.pick(SEEDS), r), x = lab(t, 'theme_load', s);
    if (x.ok === !!x.errors.length) out.bad.push(`#${i}: ${x.ok ? 'a Skin and errors' : 'no Skin and no errors'}`);
    if (x.errors.length > 32 || x.errors.some((e) => !e.msg)) out.bad.push(`#${i}: ${x.errors.length} errors / an empty message`);
    if (x.ok) { out.ok++; if (i % 8 === 0 && lab(t, 'theme_load', s).digest !== x.digest) out.bad.push(`#${i}: loads differently twice`); continue; }
    out.refused++;
    if (i % 16 === 0 || partial) {
      const a = lab(t, 'theme_apply', s), l = lab(t, 'theme_live');
      if (a.ok) out.bad.push(`#${i}: theme_apply took what theme_load refused`);
      if (l.live !== live.live || l.gen !== live.gen) { out.changed++; if (partial) lab(t, 'theme_apply', SEEDS[2]); }
    }
  }
  if (partial) lab(t, 'theme_break', '');
  return out;
}

export default {
  id: 'theme_fuzz', family: 'themes', about: 'Seeded mutations of the built-in themes through the WASM parser: refused whole or loaded whole, never a partial apply.',
  tests: [
    { name: '10k seeded mutations: a Skin xor errors, the live theme unchanged by every refusal', mount: ['placeholder', { name: 'Fuzz' }], run(t) {
      const o = run(t, 10000, 20261027);
      t.log.push(`10000 cases: ${o.ok} accepted, ${o.refused} refused`);
      t.eq(o.bad.slice(0, 3), [], `no broken invariant (${o.bad.length})`);
      t.eq(o.changed, 0, 'no refusal changed the live theme');
      t.ok(o.ok > 50 && o.refused > 5000, `both outcomes exercised (${o.ok} accepted, ${o.refused} refused)`);
    } },
    { name: 'break-it: with a partial apply injected, the same fuzz sees the live theme change', mount: ['placeholder', { name: 'Fuzz' }], run(t) {
      const o = run(t, 300, 7, { partial: true });
      t.ok(o.changed > 0, `the live-path check goes red under the fault (${o.changed} of ${o.refused} refusals changed the live theme)`);
    } },
  ],
};
