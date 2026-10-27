// Theme files (core/theme_file.cpp, renderer/CONTRACT.md section 10 + Appendix A; theme engine design T1). Each test
// fails without the behaviour it names: one per error class (the whole file is refused, with the path, and the live
// theme does not change), inheritance, state merge, presets, lengths, the guards (C47: contrast on RGB565, press
// feedback C36, geometry), never-partial-apply + its break-it, the built-ins, memory. Through fw_lab("theme_*").
// Not here yet (the apply path, theme engine T2): pixels. Nothing outside the parser reads a Skin, so no frame check
// could fail today (C0); the T2 tests compare frames.
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { facetTheme, schema } from '../../tools/gen_theme.mjs';
import { expand } from '../../tools/contrast.mjs';

const R = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const theme = (id) => readFileSync(join(R, 'themes', `${id}.json`), 'utf8');
const lab = (t, c, a = '') => JSON.parse(t.b.str(c, (pc) => t.b.str(a, (pa) => t.b.m.UTF8ToString(t.b.m._fw_lab(-1, pc, pa)))));
const J = (o) => (typeof o === 'string' ? o : JSON.stringify(o));
const T0 = { schema: 1, id: 'test' };
const M = ['placeholder', { name: 'Theme' }];  // any mount: the module the tests talk to
// a refusal: ok false, an error at `path` whose message matches `msg`, and the live theme unchanged by an apply
function refused(t, src, path, msg, what = path) {
  const before = lab(t, 'theme_live'), r = lab(t, 'theme_load', J(src));
  t.eq(r.ok, false, `${what}: refused`);
  const e = r.errors.find((x) => x.path === path && (!msg || msg.test(x.msg)));
  t.ok(!!e, `${what}: an error at "${path}"${msg ? ` matching ${msg}` : ''} (got ${JSON.stringify(r.errors.slice(0, 3))})`);
  const a = lab(t, 'theme_apply', J(src)), after = lab(t, 'theme_live');
  t.ok(!a.ok && after.live === before.live && after.gen === before.gen, `${what}: the live theme and its generation unchanged`);
  return r;
}
const loads = (t, src, what) => { const r = lab(t, 'theme_load', J(src)); t.ok(r.ok, `${what}: loads (${JSON.stringify(r.errors.slice(0, 2))})`); return r; };
const dump = (t, src) => lab(t, 'theme_dump', J(src));
const card = (o) => ({ ...T0, parts: { card: o } });

export default {
  id: 'theme_parse', family: 'themes', about: 'Theme files: schema, refusals with paths, inheritance, merge, presets, guards, never a partial apply.',
  tests: [
    // ---------------------------------------------------------------- built-ins + schema
    { name: 'built-ins: facet, blockcraft and win98 load and pass every guard', mount: M, run(t) {
      const b = lab(t, 'theme_builtins');
      t.eq(b.map((x) => x.id).sort(), ['blockcraft', 'facet', 'win98'], 'the embedded themes');
      for (const x of b) t.ok(x.ok && x.checks > 500, `${x.id}: ok with ${x.checks} guard checks (${JSON.stringify(x.errors.slice(0, 2))})`);
    } },
    { name: 'facet.json is generated from the tokens and compiles to exactly Facet\'s defaults', mount: M, run(t) {
      t.eq(theme('facet'), facetTheme(), 'themes/facet.json = gen_theme.mjs from the tokens');
      t.eq(loads(t, theme('facet'), 'facet.json').digest, lab(t, 'theme_facet').digest, 'facet.json digest = fx::theme_facet()');
      const d = dump(t, theme('facet'));
      t.eq(Object.values(d.parts).filter((p) => Object.keys(p).length).length, 0, 'Facet sets no part block (S1: the apply path is a no-op for Facet)');
    } },
    { name: 'look ranges are the one list: every look size = fw_lab tokens (fx::tune) at this density; hi accepted, hi + 1 refused', mount: M, run(t, { den }) {
      const info = lab(t, 'theme_info'), d = den === '1x' ? 1 : 0, tok = new Map(lab(t, 'tokens').map((r) => [r[0], r]));
      t.ok(info.looks.length >= 18, `${info.looks.length} look sizes`);
      for (const [n, lo15, hi15, lo1, hi1] of info.looks) {
        const r = tok.get(n);
        if (r) t.eq([r[3], r[4], r[5]], ['look', d ? lo1 : lo15, d ? hi1 : hi15], `${n}: the same range as fx::tune`);
        else t.ok(false, `${n}: missing from fw_lab tokens`);
        t.ok(lab(t, 'theme_load', J({ ...T0, size: { [n]: [hi15, hi1] } })).ok, `${n} at hi loads`);
        const r2 = lab(t, 'theme_load', J({ ...T0, size: { [n]: [hi15 + 1, hi1] } }));
        t.ok(!r2.ok && /outside/.test(r2.errors[0]?.msg), `${n} at hi + 1 refused`);
      }
      t.ok(tok.size > info.looks.length && [...tok.values()].filter((r) => r[3] === 'look').length === info.looks.length, 'fx::tune has no look size the theme schema lacks');
    } },
    // ---------------------------------------------------------------- syntax
    { name: 'syntax: line and column of the first error', mount: M, run(t) {
      const r = refused(t, '{\n "schema": 1,\n "id": "x"\n "label": "y"\n}', '', /^line 4, column 2: expected ',' or '}'/, 'a missing comma');
      t.eq(r.errors[0].line, 4, 'the error line');
      refused(t, '{"schema": 1, "id": "x",}', '', /line 1, column 25: a trailing comma/, 'a trailing comma');
      refused(t, '{"schema": 1, "id": "x"} x', '', /text after the theme/, 'text after the object');
      refused(t, '', '', /an empty file/, 'an empty file');
      refused(t, '[1]', '', /a theme is a JSON object/, 'not an object');
      refused(t, '{"schema": 1, "id": "x" /* c */}', '', /expected ','/, 'a comment');
      refused(t, '{"schema": 01, "id": "x"}', '', /expected ','/, 'a leading zero');
      refused(t, '{"schema": 1e, "id": "x"}', '', /digits in its exponent/, 'a broken number');
      refused(t, '{"schema": tru, "id": "x"}', '', /unexpected character/, 'a broken literal');
    } },
    { name: 'syntax: strings (control characters, escapes, surrogates, NUL)', mount: M, run(t) {
      refused(t, '{"schema": 1, "id": "x", "label": "a\u0001b"}', '', /control character/, 'a raw control character');
      refused(t, '{"schema": 1, "id": "x", "label": "a\\qb"}', '', /unknown escape/, 'an unknown escape');
      refused(t, '{"schema": 1, "id": "x", "label": "a\\ud800b"}', '', /lone surrogate/, 'a lone surrogate');
      refused(t, '{"schema": 1, "id": "x", "label": "a\\u12G4"}', '', /4 hex digits/, 'a short \\u escape');
      refused(t, '{"schema": 1, "id": "x", "label": "a\\u0000"}', '', /\\u0000 is not allowed/, 'an escaped NUL');
      refused(t, '{"schema": 1, "id": "x", "label": "abc', '', /unterminated string/, 'an unterminated string');
      t.ok(loads(t, '{"schema": 1, "id": "x", "label": "K\\u00fcche \\ud83d\\udca1"}', 'escapes and a surrogate pair').label === 'Küche 💡', 'escapes decoded to UTF-8');
    } },
    { name: 'syntax: a duplicate key is refused (JSON leaves it undefined)', mount: M, run(t) {
      refused(t, '{"schema": 1, "id": "x", "id": "y"}', '', /a duplicate key/, 'a duplicate top-level key');
      refused(t, '{"schema": 1, "id": "x", "parts": {"card": {"face": "bg", "face": "text"}}}', '', /a duplicate key/, 'a duplicate block key');
    } },
    { name: 'caps: over 16 KB, deeper than 8 levels, more values than the cap', mount: M, run(t) {
      const cap = lab(t, 'theme_info').max_values;
      refused(t, J({ ...T0, about: 'x'.repeat(16400) }), '', /bytes: a theme file is at most 16 KB/, 'a 16 KB file');
      refused(t, '{"schema":1,"id":"x","parts":{"card":{"edges":[{"ink":{"a":{"b":{"c":{"d":1}}}}}]}}}', '', /nested deeper than 8 levels/, 'depth 9');
      refused(t, `{"schema":1,"id":"x","about":[${new Array(cap).fill(0).join(',')}]}`, '', /more values than a theme may hold/, `${cap} values + the theme`);
      t.ok(!/more values/.test(lab(t, 'theme_load', `{"schema":1,"id":"x","about":[${new Array(cap - 8).fill(0).join(',')}]}`).errors[0]?.msg), 'just under the cap: parsed (then refused: about is not a string)');
      t.ok(loads(t, '{"schema":1,"id":"x","parts":{"card":{"edges":[{"ink":"text","w":[1,1]}]}}}', 'depth 6').ok, 'a real nesting depth loads');
    } },
    // ---------------------------------------------------------------- schema, id, modes
    { name: 'schema: newer refused on that alone, older than 1, missing, not a number', mount: M, run(t) {
      const r = refused(t, { schema: 2, id: 'x', glow: 1 }, 'schema', /^2 is newer than this panel \(1\)$/, 'schema 2');
      t.eq(r.errors.length, 1, 'a newer schema: only that error (its other keys may be new)');
      refused(t, { schema: 0, id: 'x' }, 'schema', /0 is not a theme schema \(1 \.\. 1\)/, 'schema 0');
      refused(t, { id: 'x' }, 'schema', /missing/, 'no schema');
      refused(t, { schema: '1', id: 'x' }, 'schema', /is not a schema number/, 'schema as a string');
      refused(t, '{"schema": 1e300, "id": "x"}', 'schema', /^1e\+300 is newer than this panel/, 'schema 1e300 (no overflowing cast)');
    } },
    { name: 'id and label: required id a-z 0-9 -, at most 32; label at most 48 characters', mount: M, run(t) {
      refused(t, { schema: 1 }, 'id', /missing/, 'no id');
      refused(t, { schema: 1, id: 'Big Theme' }, 'id', /is not an id/, 'an id with capitals and a space');
      refused(t, { schema: 1, id: 'x'.repeat(33) }, 'id', /is not an id/, 'a 33-character id');
      refused(t, { schema: 1, id: 5 }, 'id', /expected a string, got 5/, 'an id as a number');
      refused(t, { ...T0, label: 'é'.repeat(49) }, 'label', /longer than 48 characters/, 'a 49-character label');
      t.eq(loads(t, { ...T0, label: 'é'.repeat(48) }, '48 characters').label, 'é'.repeat(48), 'a 48-character label (96 bytes) kept whole');
    } },
    { name: 'modes: empty, unknown, twice; a one-mode theme keeps its mode', mount: M, run(t) {
      refused(t, { ...T0, modes: [] }, 'modes', /empty/, 'no modes');
      refused(t, { ...T0, modes: ['dim'] }, 'modes[0]', /is not a mode/, 'an unknown mode');
      refused(t, { ...T0, modes: ['dark', 'dark'] }, 'modes[1]', /twice/, 'a mode twice');
      refused(t, { ...T0, modes: 'dark' }, 'modes', /expected a list/, 'modes as a string');
      const d = lab(t, 'theme_dump', J({ ...T0, modes: ['light'] }));
      t.eq([d.modes, Object.keys(d.palette)], [['light'], ['light']], 'a light-only theme: one palette');
      const c = lab(t, 'theme_contrast', J({ ...T0, modes: ['light'] }));
      t.ok(c.rows.length > 100 && c.rows.every((r) => r[4] === 'light'), 'its guard checks the light mode only');
    } },
    // ---------------------------------------------------------------- colour + accents
    { name: 'colour: unknown token, not a colour, a mode the theme lacks, an accent role', mount: M, run(t) {
      refused(t, { ...T0, colour: { glow: { dark: '#FFFFFF' } } }, 'colour.glow', /not a colour token/, 'an unknown token');
      refused(t, { ...T0, colour: { bg: { dark: '#12345' } } }, 'colour.bg.dark', /is not a colour \("#RRGGBB"\)/, 'a 5-digit hex');
      refused(t, { ...T0, colour: { bg: { dark: '#GGGGGG' } } }, 'colour.bg.dark', /is not a colour/, 'a non-hex colour');
      refused(t, { ...T0, colour: { bg: '#000000' } }, 'colour.bg', /expected an object/, 'a colour without modes');
      refused(t, { ...T0, colour: { bg: { dim: '#000000' } } }, 'colour.bg.dim', /not a mode/, 'an unknown mode key');
      refused(t, { ...T0, modes: ['dark'], colour: { bg: { light: '#FFFFFF' } } }, 'colour.bg.light', /this theme has no light mode/, 'a mode the theme lacks');
      refused(t, { ...T0, colour: { accent: { dark: '#FF0000' } } }, 'colour.accent', /an accent role: set it in accents/, 'an accent role as a colour');
      const d = dump(t, { ...T0, colour: { 'text-dim': { dark: '#A0A0A0' } } });
      t.eq([d.palette.dark['text-dim'], d.palette.light['text-dim'], d.palette.dark.text], ['#A0A0A0', JSON.parse(theme('facet')).colour['text-dim'].light, JSON.parse(theme('facet')).colour.text.dark], 'a token set for one mode; the rest inherit Facet');
    } },
    { name: 'accents: 1 to 8 sets, all 5 roles per mode, ids unique, the prototype role names refused', mount: M, run(t) {
      const set = (id, extra = {}) => ({ id, dark: { accent: '#E8542A', 'accent-press': '#D54B22', 'accent-dim': '#5A2414', 'accent-text': '#E8542A', 'on-accent': '#0A0B0E' }, ...extra });
      const one = (a) => ({ ...T0, modes: ['dark'], accents: a });
      t.eq(loads(t, one([set('ember')]), 'one set').sets.map((s) => s.id), ['ember'], 'one set');
      refused(t, one([]), 'accents', /0 sets: a theme has 1 to 8/, 'no sets');
      refused(t, one(new Array(9).fill(0).map((_, i) => set(`a${i}`))), 'accents', /9 sets/, 'nine sets');
      const miss = set('x'); delete miss.dark['accent-dim'];
      refused(t, one([miss]), 'accents[0].dark.accent-dim', /missing \(every accent set names all 5 roles/, 'a missing role');
      refused(t, one([set('x', { dark: { ...set('x').dark, 'accent-ink': '#FFFFFF' } })]), 'accents[0].dark.accent-ink', /not an accent role \(accent, accent-press, accent-dim, accent-text, on-accent\)/, 'accent-ink (prototype name)');
      refused(t, { ...T0, accents: [set('x')] }, 'accents[0].light', /missing \(this theme has the light mode\)/, 'a mode the theme has, missing');
      refused(t, one([set('x', { light: set('x').dark })]), 'accents[0].light', /this theme has no light mode/, 'a mode the theme lacks');
      refused(t, one([set('x'), set('x')]), 'accents[1].id', /already the id of accents\[0\]/, 'a duplicate id');
      const noid = set('x'); delete noid.id;
      refused(t, one([noid]), 'accents[0].id', /missing/, 'no id');
    } },
    // ---------------------------------------------------------------- sizes + type + font + shape
    { name: 'size: layout sizes are fixed (C18), unknown tokens, not [1.5x, 1x], out of range is refused (C19)', mount: M, run(t) {
      for (const n of ['layout:inset', 'space:2', 'touch:min', 'layout:card-pad', 'layout:card-min-h'])
        refused(t, { ...T0, size: { [n]: [8, 8] } }, `size.${n}`, /^layout sizes are fixed by Facet \(C18\)$/, n);
      refused(t, { ...T0, size: { 'radius:round': [8, 8] } }, 'size.radius:round', /fixed \(a pill, not a length\)/, 'radius:round');
      refused(t, { ...T0, size: { 'radius:huge': [8, 8] } }, 'size.radius:huge', /not a size token/, 'an unknown size');
      refused(t, { ...T0, size: { 'radius:card': 4 } }, 'size.radius:card', /is not \[1\.5x, 1x\] px/, 'a bare number');
      refused(t, { ...T0, size: { 'radius:card': [40, 8] } }, 'size.radius:card', /^40 at 1\.5x is outside 0 \.\. 16 \(C19/, 'radius 40 at 1.5x');
      refused(t, { ...T0, size: { 'stroke:hair': [1, 0] } }, 'size.stroke:hair', /^0 at 1x is outside 1 \.\. 2/, 'stroke 0 at 1x');
      refused(t, { ...T0, size: { 'stroke:pixel': [5, 2] } }, 'size.stroke:pixel', /5 at 1\.5x is outside 1 \.\. 4/, 'pixel 5');
      refused(t, { ...T0, size: { 'radius:card': [1.5, 1] } }, 'size.radius:card[0]', /is not a whole number/, 'a fraction');
      t.eq(dump(t, { ...T0, size: { 'radius:card': [16, 12] } }).size['radius:card'], [16, 12], 'an in-range look size is kept');
    } },
    { name: 'type: within [floor, line box] at both densities; only the themeable styles', mount: M, run(t) {
      refused(t, { ...T0, type: { title: [18, 16] } }, 'type.title', /18 px at 1\.5x is below its floor 24/, 'below the floor');
      refused(t, { ...T0, type: { title: [44, 20] } }, 'type.title', /44 px at 1\.5x is above its line box 40 \(the box is fixed/, 'above the box');
      refused(t, { ...T0, type: { body: [22, 21] } }, 'type.body', /21 px at 1x is above its line box 20/, 'above the 1x box');
      refused(t, { ...T0, type: { 'c-meta': [16, 12] } }, 'type.c-meta', /not a type style a theme may size \(display, hero, value, headline, title, body, label\)/, 'a component style');
      t.eq(dump(t, { ...T0, type: { title: [32, 21] } }).type.title, [32, 21], 'title one step bigger in the same line box (as Large text)');
    } },
    { name: 'font: a role and a family of this build', mount: M, run(t) {
      refused(t, { ...T0, font: { body: 'comic-sans' } }, 'font.body', /is not in this panel's build \(inter, jetbrains-mono, monocraft, w95fa; or source: download\)/, 'an unknown family');
      refused(t, { ...T0, font: { heading: 'inter' } }, 'font.heading', /not a font role \(display, body, label, mono\)/, 'an unknown role');
      t.eq(dump(t, { ...T0, font: { body: 'jetbrains-mono' } }).font.body, 'jetbrains-mono', 'a family of the build');
      t.eq(dump(t, { ...T0, font: { body: 'monocraft' } }).font.body, 'monocraft', 'a theme family of the registry (fonts/fonts.json)');
      t.eq(dump(t, { ...T0, font: { body: { family: 'w95fa', source: 'builtin' } } }).font.body, 'w95fa', 'the object form, built in');
    } },
    { name: 'font source download: the panel fetches it; url, family id and keys checked; the fallback is the built-in family', mount: M, run(t) {
      const U = 'https://fonts.example.org/PressStart2P.ttf';
      t.eq(dump(t, { ...T0, font: { body: { family: 'press-start-2p', source: 'download', url: U } } }).font.body, 'inter', 'a download: the Skin keeps the built-in fallback for the role');
      t.eq(dump(t, { ...T0, font: { label: { family: 'press-start-2p', source: 'download', url: U, fallback: true } } }).font.label, 'jetbrains-mono', 'fallback: true is allowed on a download');
      refused(t, { ...T0, font: { body: { family: 'x', source: 'download' } } }, 'font.body', /a downloaded font needs its url/, 'no url');
      refused(t, { ...T0, font: { body: { family: 'x', source: 'download', url: 'http://a.b/x.ttf' } } }, 'font.body.url', /is not an https URL of at most 160 characters/, 'plain http');
      refused(t, { ...T0, font: { body: { family: 'x', source: 'download', url: 'https://a.b/' + 'x'.repeat(160) } } }, 'font.body.url', /at most 160/, 'a URL too long');
      refused(t, { ...T0, font: { body: { family: 'Press Start', source: 'download', url: U } } }, 'font.body.family', /is not a family id \(1 to 32 of a-z 0-9 -\)/, 'a family name, not an id');
      refused(t, { ...T0, font: { body: { family: 'x', source: 'cdn', url: U } } }, 'font.body.source', /is not a source \(builtin, download\)/, 'an unknown source');
      refused(t, { ...T0, font: { body: { family: 'x', source: 'download', url: U, size: 3 } } }, 'font.body.size', /unknown key \(family, source, url, fallback\)/, 'an unknown key');
      refused(t, { ...T0, font: { body: { family: 'inter', fallback: true } } }, 'font.body.fallback', /only a downloaded font has a fallback setting/, 'fallback on a built-in family');
      refused(t, { ...T0, font: { body: { family: 'inter', url: U } } }, 'font.body.url', /only a downloaded font has a url/, 'a url on a built-in family');
      refused(t, { ...T0, font: { body: { source: 'download', url: U } } }, 'font.body', /needs a family/, 'no family');
      refused(t, { ...T0, font: { body: { family: 'comic-sans' } } }, 'font.body.family', /is not in this panel's build/, 'object form, unknown family');
    } },
    { name: 'shape: unknown key and value; Facet\'s values expand to nothing; presets fill what parts leave unset', mount: M, run(t) {
      refused(t, { ...T0, shape: { knob: 'star' } }, 'shape.knob', /is not a knob \(circle, square, diamond, ring, pixel\)/, 'a knob shape');
      refused(t, { ...T0, shape: { glow: true } }, 'shape.glow', /unknown key/, 'an unknown shape key');
      refused(t, { ...T0, shape: { snap: 'yes' } }, 'shape.snap', /expected true or false/, 'snap as a string');
      const f = dump(t, { ...T0, shape: { corner: 'round', cap: 'round', knob: 'circle', ticks: 'lines', card: 'fill', indicator: 'text', icons: 'line', snap: false } });
      t.eq(Object.values(f.parts).filter((p) => Object.keys(p).length).length, 0, 'Facet\'s shape values: no part blocks');
      const sq = dump(t, { ...T0, shape: { corner: 'square' }, parts: { control: { radius: [4, 4] } } });
      t.eq([sq.parts.card.base.radius, sq.parts.control.base.radius], [[0, 0], [4, 4]], 'corner square: card radius 0; a part\'s own radius wins');
      const ol = dump(t, { ...T0, shape: { card: 'outline', indicator: 'bar' } });
      t.eq([ol.parts.card.base.edges?.length, ol.parts.card.on.edges?.[0]?.side, ol.parts.card.on.edges?.[0]?.ink], [1, 'bottom', 'accent'], 'card outline + indicator bar');
      const kn = dump(t, { ...T0, shape: { knob: 'square', ticks: 'dots' } });
      t.eq([kn.parts.knob.base.shape, kn.parts.tick.base.shape, kn.parts['tick-major'].base.shape], ['square', 'dots', 'dots'], 'knob and ticks presets');
    } },
    // ---------------------------------------------------------------- parts
    { name: 'parts: unknown part, unknown key, a key the part does not take, an unknown state, a state inside a state', mount: M, run(t) {
      refused(t, { ...T0, parts: { button: {} } }, 'parts.button', /not a part \(card, control, track/, 'an unknown part');
      refused(t, card({ glow: 1 }), 'parts.card.glow', /unknown key \(card takes face, radius, pad, edges, shadow, text, or a state: on, focus, pressed, disabled\)/, 'an unknown key');
      refused(t, { ...T0, parts: { tick: { face: 'text' } } }, 'parts.tick.face', /not a key of tick \(it takes ink, shape\)/, 'face on a tick');
      refused(t, card({ hover: {} }), 'parts.card.hover', /unknown key/, 'an unknown state');
      refused(t, card({ pressed: { on: { face: 'bg' } } }), 'parts.card.pressed.on', /a state inside a state/, 'a state in a state');
      refused(t, { ...T0, parts: { header: { pressed: { buttons: { names: ['close'] } } } } }, 'parts.header.pressed.buttons', /in the base block only/, 'buttons in a state');
      refused(t, { ...T0, parts: { header: { buttons: { names: ['sparkle'] } } } }, 'parts.header.buttons.names[0]', /is not a caption button/, 'an unknown caption button');
      refused(t, { ...T0, parts: { knob: { shape: 'star' } } }, 'parts.knob.shape', /is not a knob shape \(circle, square, diamond, ring, pixel, pointer\)/, 'a knob part shape');
      refused(t, { ...T0, parts: { title: { weight: 'heavy' } } }, 'parts.title.weight', /is not a weight \(regular, bold\)/, 'a weight');
    } },
    { name: 'edges: the doc\'s example path, styles, at most 8 layers, w >= 1, ink required', mount: M, run(t) {
      const r = refused(t, card({ pressed: { edges: [{ ink: 'text' }, { side: 'diagonal', ink: 'text' }] } }), 'parts.card.pressed.edges[1].side', null, 'a bad side');
      t.eq(r.errors.find((e) => e.path === 'parts.card.pressed.edges[1].side')?.msg, '"diagonal" is not a side (all, top, right, bottom, left, top-left, bottom-right)', 'the contract\'s error text, word for word');
      refused(t, card({ edges: [{ ink: 'text', style: 'dashed' }] }), 'parts.card.edges[0].style', /is not an edge style \(solid, dotted\)/, 'a dashed edge');
      refused(t, card({ edges: new Array(9).fill({ ink: 'text' }) }), 'parts.card.edges', /9 layers: at most 8/, 'nine layers');
      refused(t, card({ edges: [{ ink: 'text', w: 0 }] }), 'parts.card.edges[0].w', /at least 1 cell wide/, 'w 0 cells');
      refused(t, card({ edges: [{ ink: 'text', w: [0, 0] }] }), 'parts.card.edges[0].w', /0 px at 1\.5x: an edge is at least 1 px wide/, 'w 0 px');
      refused(t, card({ edges: [{ side: 'all' }] }), 'parts.card.edges[0].ink', /missing/, 'no ink');
      refused(t, card({ edges: [{ ink: 'glow' }] }), 'parts.card.edges[0].ink', /is not an ink \(a colour token, an accent role/, 'an unknown ink');
      refused(t, card({ edges: [{ ink: 'text', w: 'radius:huge' }] }), 'parts.card.edges[0].w', /is not a size token/, 'an unknown size as a length');
      refused(t, card({ edges: [{ ink: 'text', w: 1.5 }] }), 'parts.card.edges[0].w', /is not a whole number/, 'a fractional cell');
    } },
    { name: 'faces: gradient needs from + to, one kind per face, patterns as [ink, ink], the scrollbar thumb is one ink', mount: M, run(t) {
      refused(t, card({ face: { from: 'bg' } }), 'parts.card.face', /a gradient needs "from" and "to"/, 'half a gradient');
      refused(t, card({ face: { from: 'bg', to: 'surface', checker: ['bg', 'surface'] } }), 'parts.card.face', /one of a gradient, a checker or blocks/, 'two kinds');
      refused(t, card({ face: { checker: ['bg'] } }), 'parts.card.face.checker', /expected \[ink, ink\]/, 'a one-ink checker');
      refused(t, card({ face: { from: 'bg', to: 'surface', dir: 'up' } }), 'parts.card.face.dir', /is not a direction \(right, down\)/, 'a direction');
      refused(t, card({ face: { checker: ['bg', 'surface'], run: 2 } }), 'parts.card.face', /"run" and "gap" belong to a blocks face/, 'run on a checker');
      refused(t, card({ face: { glow: 1 } }), 'parts.card.face.glow', /unknown key/, 'an unknown face key');
      refused(t, { ...T0, parts: { scrollbar: { face: { from: 'bg', to: 'surface' } } } }, 'parts.scrollbar.face', /this face is one ink/, 'a gradient thumb');
    } },
    // ---------------------------------------------------------------- inheritance + merge + lengths
    { name: 'inheritance: a minimal theme = Facet (palette, sizes, type, fonts, no parts)', mount: M, run(t) {
      const a = dump(t, T0), f = dump(t, theme('facet'));
      for (const k of ['palette', 'size', 'type', 'font', 'shape', 'parts', 'sets']) t.eq(a[k], f[k], `${k} = Facet's`);
      t.eq([a.label, a.sets.length], ['test', 5], 'label defaults to the id; Facet\'s 5 accent sets');
    } },
    { name: 'state merge: on < focus < pressed < disabled, key by key; a list (edges) replaces whole', mount: M, run(t) {
      const d = dump(t, card({ edges: [{ ink: 'text' }, { ink: 'line', inset: 1 }], on: { face: 'bg' }, pressed: { face: 'surface-press', edges: [{ ink: 'text-dim' }] }, disabled: { text: 'text-mute' } }));
      const P = d.parts.card;
      t.eq([P.base.keys, P.base.edges.length], [['edges'], 2], 'base: its own edges');
      t.eq([P.on.face.a, P.on.edges.length], ['bg', 2], 'on: its face over the base edges');
      t.eq([P['on+pressed'].face.a, P['on+pressed'].edges.length, P['on+pressed'].edges[0].ink], ['surface-press', 1, 'text-dim'], 'on + pressed: pressed wins, its edges replace the list whole');
      t.eq([P['on+disabled'].face.a, P['on+disabled'].text], ['bg', 'text-mute'], 'on + disabled: both merged');
      t.ok(!P.focus.face, 'focus: no face (the component\'s own shows)');
      t.eq([P['on+focus'].face.a, P['on+focus+pressed'].face.a], ['bg', 'surface-press'], 'every combination is compiled: on + focus keeps the on face (a tap focuses), + pressed wins');
      t.eq(Object.keys(P).length, 16, 'all 16 combinations of on, focus, pressed, disabled');
    } },
    { name: 'lengths: cells x stroke:pixel, a size token, [1.5x, 1x] px; shadows may be negative', mount: M, run(t) {
      const d = dump(t, { ...T0, size: { 'stroke:pixel': [3, 2] }, parts: { card: { edges: [{ ink: 'text', w: 2 }, { ink: 'text', w: 'stroke:edge', inset: 2 }, { ink: 'text', w: [5, 3], inset: 4 }] }, title: { shadow: { dx: -1, dy: [2, 1], ink: 'bg' } } } });
      t.eq(d.parts.card.base.edges.map((e) => [e.w, e.inset]), [[[6, 4], [0, 0]], [[2, 2], [6, 4]], [[5, 3], [12, 8]]], 'edge lengths in px at [1.5x, 1x]');
      t.eq([d.parts.title.base.shadow.dx, d.parts.title.base.shadow.dy], [[-3, -2], [2, 1]], 'a shadow offset');
      refused(t, card({ edges: [{ ink: 'text', w: -1 }] }), 'parts.card.edges[0].w', /is not a whole number from 0/, 'a negative edge');
    } },
    { name: 'determinism: the same file twice and with its keys in another order compile to the same Skin', mount: M, run(t) {
      const w = JSON.parse(theme('win98')), rev = (o) => (Array.isArray(o) ? o.map(rev) : o && typeof o === 'object' ? Object.fromEntries(Object.entries(o).reverse().map(([k, v]) => [k, rev(v)])) : o);
      const a = lab(t, 'theme_load', theme('win98')).digest, b = lab(t, 'theme_load', theme('win98')).digest, c = lab(t, 'theme_load', J(rev(w))).digest;
      t.eq([a === b, a === c], [true, true], 'one digest');
    } },
    // ---------------------------------------------------------------- guards
    { name: 'guard contrast: a declared pair on the theme palette, with its path and ratio', mount: M, run(t) {
      const r = refused(t, { ...T0, colour: { text: { dark: '#101010' } } }, 'contrast.text', /^text on bg 1\.\d\d < 4\.5 \(text, dark, Ember\)$/, 'text on bg');
      t.ok(r.errors.every((e) => !/Rose|Violet|Teal|Sage/.test(e.msg) || !/^text on bg/.test(e.msg)), 'a failure that no accent changes is listed once, not per set');
    } },
    { name: 'guard contrast: RGB565 decides when it is worse than the exact value', mount: M, run(t) {
      // surfaces in black (exact in RGB565), then the first grey for text-dim that passes 4.5 on the exact value and
      // fails on what the panel shows (565 truncates it darker): the guard must refuse it; one step lighter loads
      const lin = (v) => { const s = v / 255; return s <= 0.03928 ? s / 12.92 : ((s + 0.055) / 1.055) ** 2.4; };
      const rat = (c) => (0.2126 * lin(c[0]) + 0.7152 * lin(c[1]) + 0.0722 * lin(c[2]) + 0.05) / 0.05;  // on black
      const q = ([r, g, b]) => [((r >> 3) << 3) | (r >> 5), ((g >> 2) << 2) | (g >> 6), ((b >> 3) << 3) | (b >> 5)];
      let pick = null;
      for (let r = 0x60; r < 0x90 && !pick; r++) for (let g = 0x60; g < 0x90 && !pick; g++) for (let b = 0x60; b < 0x90 && !pick; b++)
        if (rat([r, g, b]) >= 4.5 && rat(q([r, g, b])) < 4.5) pick = [r, g, b];
      const hx = (c) => '#' + c.map((x) => x.toString(16).padStart(2, '0')).join('').toUpperCase();
      t.ok(!!pick, `a colour that passes exact (${pick && rat(pick).toFixed(3)}) and fails on RGB565 (${pick && rat(q(pick)).toFixed(3)}): ${pick && hx(pick)}`);
      if (!pick) return;
      const black = { bg: { dark: '#000000' }, surface: { dark: '#000000' }, 'surface-press': { dark: '#000000' } };
      const th = (c) => ({ ...T0, modes: ['dark'], accents: [{ id: 'x', dark: { accent: '#E8542A', 'accent-press': '#F06A40', 'accent-dim': '#5A2414', 'accent-text': '#E8542A', 'on-accent': '#000000' } }], colour: { ...black, 'text-dim': { dark: hx(c) } } });
      refused(t, th(pick), 'contrast.text', /^text-dim on (bg|surface|surface-press) 4\.\d\d < 4\.5/, `text-dim ${hx(pick)}`);
      t.ok(loads(t, th([0xA0, 0xA0, 0xA0]), 'text-dim #A0A0A0 (passes on both)').ok, 'a clear pass loads: only the 565 value refused it');
    } },
    { name: 'guard contrast: text on every face stop of a part, every selector (gradient, checker), disabled at 3.0', mount: M, run(t) {
      refused(t, card({ face: 'text' }), 'parts.card.face', /^text on its face 1\.00 < 4\.5/, 'text on a text-coloured card');
      refused(t, { ...T0, parts: { header: { face: { from: 'bg', to: 'text' } } } }, 'parts.header.face (2nd stop)', /text on its face/, 'a gradient\'s 2nd stop');
      refused(t, card({ on: { face: { checker: ['surface', 'text'] } } }), 'parts.card.on.face (2nd stop)', /on its face/, 'a checker stop on the on card');
      refused(t, card({ disabled: { face: 'text-mute' } }), 'parts.card.disabled.face', /text-mute on its face 1\.00 < 3\.0 \(disabled/, 'disabled: text-mute at 3.0');
      refused(t, { ...T0, parts: { 'list-row': { pressed: { face: 'blue', text: 'line-hi' } } } }, 'parts.list-row.pressed.face', /line-hi on its face/, 'a part\'s own text ink on its face');
      refused(t, { ...T0, parts: { title: { text: 'surface' } } }, 'parts.title.text', /surface on (bg|surface)/, 'a text part\'s own ink');
    } },
    { name: 'guard contrast: outer edges of cards and controls, the knob, major ticks on what they sit on: 3.0 (the strongest layer counts)', mount: M, run(t) {
      refused(t, card({ edges: [{ ink: 'bg' }] }), 'parts.card.edges', /outer edge bg on bg 1\.00 < 3\.0/, 'an edge in the page colour');
      t.ok(loads(t, card({ edges: [{ ink: 'bg' }, { ink: 'text', side: 'bottom-right' }] }), 'a bevel: one strong outer half').ok, 'the strongest outer layer counts');
      t.ok(loads(t, card({ edges: [{ ink: 'text' }, { ink: 'bg', inset: 1 }] }), 'an inner layer in bg').ok, 'inner layers are decoration');
      refused(t, { ...T0, parts: { knob: { ink: 'bg' } } }, 'parts.knob.ink', /bg on bg/, 'a knob in the page colour');
      refused(t, { ...T0, parts: { 'tick-major': { ink: 'line' } } }, 'parts.tick-major.ink', /line on bg/, 'major ticks in line');
    } },
    { name: 'guard press feedback (C36): a fixed face needs a pressed look that differs', mount: M, run(t) {
      refused(t, card({ face: 'bg' }), 'parts.card.pressed', /looks the same as the rest state \(dark, Ember\): a pressed card needs a different face, edges or text \(C36\)/, 'a face and no pressed look');
      refused(t, card({ face: 'bg', on: { face: 'surface' } }), 'parts.card.on+pressed', /looks the same as on/, 'the on card pressed');
      t.ok(loads(t, card({ face: 'bg', pressed: { face: 'surface-press' } }), 'a pressed face').ok, 'a different pressed face');
      t.ok(loads(t, card({ face: 'bg', pressed: { edges: [{ ink: 'text' }] } }), 'pressed edges').ok, 'pressed edges are feedback');
      t.ok(loads(t, card({ edges: [{ ink: 'text' }] }), 'edges only').ok, 'no face fixed: the component\'s own pressed face shows');
      refused(t, card({ face: '#0A0B0E', pressed: { face: '#0A0B0F' } }), 'parts.card.pressed', /looks the same/, 'a pressed face one bit off (the same in RGB565)');
      refused(t, card({ face: 'bg', pressed: { face: { from: 'bg', to: 'bg' } } }), 'parts.card.pressed', /looks the same/, 'a gradient of one colour = the same solid face');
    } },
    { name: 'guard geometry: edges inside the padding (the page: inside the inset), patterned faces need square corners, pad on the grid', mount: M, run(t) {
      const r1 = refused(t, card({ edges: [{ ink: 'text', w: [30, 12] }] }), 'parts.card.edges', /^30 px in at 1\.5x is more than its room 24 \(its padding layout:card-pad\)/, 'a card edge wider than its padding');
      t.eq(r1.errors.length, 1, 'listed once (not again for every state combination that inherits it)');
      refused(t, { ...T0, parts: { page: { edges: [{ ink: 'text', w: [40, 16] }] } } }, 'parts.page.edges', /more than its room 32 \(its padding layout:inset\)/, 'a page frame wider than the inset');
      refused(t, { ...T0, parts: { control: { edges: [{ ink: 'text', w: [21, 2] }] } } }, 'parts.control.edges', /21 px in at 1\.5x is more than its room 20 \(half of touch:button-h less the label line box\)/, 'a control edge reaching its label');
      refused(t, { ...T0, parts: { knob: { edges: [{ ink: 'text', w: [2, 2], inset: [13, 2] }] } } }, 'parts.knob.edges', /15 px in at 1\.5x is more than its room 14 \(half of layout:dial-knob\)/, 'a knob edge past its centre');
      refused(t, { ...T0, parts: { 'arc-fill': { edges: [{ ink: 'text', w: [3, 2] }] } } }, 'parts.arc-fill.edges', /room 2 \(leave a pixel cell of stroke:indicator\)/, 'arc edges that leave no cell of the arc');
      refused(t, { ...T0, parts: { rule: { edges: [{ ink: 'text', w: [25, 2] }] } } }, 'parts.rule.edges', /more than its room 24 \(the edge cap \(C48\)\)/, 'an edge past the cap');
      refused(t, { ...T0, parts: { control: { face: { checker: ['bg', 'surface'] }, pressed: { face: 'surface-press' } } } }, 'parts.control.face', /needs square corners \(radius 12 at 1\.5x\)/, 'a rounded checker');
      t.ok(loads(t, { ...T0, parts: { control: { radius: [0, 0], face: { checker: ['bg', 'surface'] }, pressed: { face: 'surface-press' } } } }, 'a square checker').ok, 'square: drawn');
      refused(t, card({ pad: [24, 16] }), 'parts.card.pad', /^padding is layout \(C45\): a theme may pad only a part with no content/, 'card padding (layout)');
      refused(t, { ...T0, size: { 'stroke:pixel': [3, 2] }, parts: { track: { pad: [4, 2] } } }, 'parts.track.pad', /4 at 1\.5x is not whole pixel cells \(3\)/, 'a track pad off the cell');
      t.ok(loads(t, { ...T0, size: { 'stroke:pixel': [3, 2] }, parts: { track: { pad: 2 } } }, 'a track padded 2 cells').ok, 'a track may be padded in cells');
      refused(t, card({ radius: [24, 8] }), 'parts.card.radius', /24 at 1\.5x is outside 0 \.\. 16 \(the range of radius:card/, 'a part radius out of its range');
    } },
    { name: 'bounded values: shadows, the scrollbar, caption buttons, blocks; a direction only on a gradient', mount: M, run(t) {
      refused(t, { ...T0, parts: { title: { shadow: { dx: [7, 1], dy: 0, ink: 'bg' } } } }, 'parts.title.shadow', /offset 7, 0 at 1\.5x: a shadow is at most 6 px off its box/, 'a far shadow');
      refused(t, { ...T0, parts: { title: { shadow: { dx: 0, dy: 0, ink: 'bg' } } } }, 'parts.title.shadow', /a shadow needs an offset/, 'an invisible shadow');
      refused(t, { ...T0, parts: { scrollbar: { w: [49, 32] } } }, 'parts.scrollbar.w', /49 at 1\.5x is outside 1 \.\. 48 \(at most touch:min\)/, 'a scrollbar wider than a touch target');
      refused(t, { ...T0, parts: { header: { buttons: { names: ['close'], h: [49, 20] } } } }, 'parts.header.buttons', /w, h and gap at 1\.5x are 1 \.\. 48 px/, 'caption buttons taller than the header row');
      refused(t, card({ face: { blocks: ['bg', 'surface'], run: [0, 0] } }), 'parts.card.face.run', /a block is at least 1 px/, 'blocks of 0 px');
      refused(t, card({ face: { checker: ['bg', 'surface'], dir: 'down' } }), 'parts.card.face.dir', /a direction belongs to a gradient/, 'a direction on a checker');
      refused(t, { ...T0, parts: { scrollbar: { face: 'bg' } } }, 'parts.scrollbar.face', /the thumb on the canvas \(bg\) 1\.00 < 3\.0/, 'a thumb in the canvas colour, no lane');
    } },
    { name: 'errors: at most 32, the last says more were not listed; each carries its line', mount: M, run(t) {
      const o = { ...T0 };
      for (let i = 0; i < 40; i++) o[`k${i}`] = i;
      const r = lab(t, 'theme_load', JSON.stringify(o, null, 1));
      t.eq([r.errors.length, /more errors not listed/.test(r.errors[31]?.msg)], [32, true], '32 errors, the last a note');
      t.eq(r.errors[0].line, 4, 'k0 is on line 4 of the pretty-printed file');
    } },
    // ---------------------------------------------------------------- never a partial apply + break-it
    { name: 'never a partial apply: a refused file (read, compile or guard time) leaves the live Skin and its generation as they were', mount: M, run(t) {
      const good = lab(t, 'theme_apply', theme('win98'));
      t.ok(good.ok && good.id === 'win98', 'win98 live');
      const live0 = lab(t, 'theme_live');
      // valid colours first, then one bad key: a parser that writes as it reads would have applied the colours
      const bad = '{"schema": 1, "id": "half", "colour": {"bg": {"dark": "#FF0000", "light": "#FF0000"}}, "parts": {"card": {"glow": 1}}}';
      const r = lab(t, 'theme_apply', bad), live1 = lab(t, 'theme_live');
      t.eq([r.ok, live1.live, live1.gen, live1.id], [false, live0.live, live0.gen, 'win98'], 'live unchanged after the refusal');
      for (const [what, src] of [['refused while compiling (a part radius out of its range)', card({ radius: [40, 8] })],
        ['refused by a guard (whole, then contrast)', { ...T0, id: 'late', colour: { bg: { dark: '#FF0000', light: '#FF0000' }, text: { dark: '#FF0000', light: '#FF0000' } } }]]) {
        const x = lab(t, 'theme_apply', J(src)), l = lab(t, 'theme_live');
        t.eq([x.ok, l.live, l.gen], [false, live0.live, live0.gen], `${what}: live unchanged`);
      }
      const c = lab(t, 'theme_apply', theme('blockcraft'));
      t.eq([c.ok, c.id, c.gen], [true, 'blockcraft', live0.gen + 1], 'the next good file: live, one generation on');
    } },
    { name: 'break-it: with a partial apply injected, the never-partial-apply proof goes red', mount: M, run(t) {
      lab(t, 'theme_apply', theme('win98'));
      const live0 = lab(t, 'theme_live');
      lab(t, 'theme_break', 'partial');
      const r = lab(t, 'theme_apply', '{"schema": 1, "id": "half", "colour": {"bg": {"light": "#FF0000"}}, "parts": {"card": {"glow": 1}}}');
      const live1 = lab(t, 'theme_live');
      lab(t, 'theme_break', '');
      t.eq(r.ok, false, 'the file is still refused');
      t.ok(live1.live !== live0.live || live1.gen !== live0.gen, 'the proof sees the partial apply (live digest or generation changed)');
      lab(t, 'theme_apply', theme('win98'));
      const live2 = lab(t, 'theme_live');
      lab(t, 'theme_break', 'partial');
      lab(t, 'theme_apply', J({ ...T0, id: 'late', colour: { text: { dark: '#000000', light: '#F4F2EE' } } }));
      const live3 = lab(t, 'theme_live');
      lab(t, 'theme_break', '');
      t.ok(live3.live !== live2.live, 'a guard-time refusal too: the fault puts the complete-but-unguarded Skin live, the proof sees it');
      lab(t, 'theme_reset');
      t.eq(lab(t, 'theme_live').id, 'facet', 'reset: back to Facet');
    } },
    // ---------------------------------------------------------------- memory + the contrast table
    { name: 'memory: a compiled theme <= 24 KB; a load holds <= 64 KB at once (also at the value cap)', mount: M, run(t) {
      const info = lab(t, 'theme_info');
      t.ok(info.skin_bytes <= 24 * 1024, `a Skin is ${info.skin_bytes} B`);
      for (const id of ['facet', 'blockcraft', 'win98']) { const r = lab(t, 'theme_load', theme(id)); t.ok(r.peak <= 64 * 1024, `${id}: peak ${r.peak} B`); }
      const cap = info.max_values;
      for (const [what, src] of [['the value cap in numbers', `{"schema":1,"id":"x","about":[${new Array(cap - 8).fill(0).join(',')}]}`],
        ['the value cap in strings', `{"schema":1,"id":"x","about":[${new Array(cap - 8).fill('"abcdefgh"').join(',')}]}`],
        ['16 KB of one string', J({ ...T0, about: 'x'.repeat(16300) })]]) {
        const big = lab(t, 'theme_load', src);
        t.ok(big.peak > 0 && big.peak <= 64 * 1024, `${what}: peak ${big.peak} B`);
      }
    } },
    { name: 'the guard\'s declared pairs = tools/contrast.mjs on Facet (same ratios, exact and RGB565), for the theme scope', mount: M, run(t) {
      const S = schema(), rows = lab(t, 'theme_contrast', theme('facet')).rows.filter((r) => r[0].startsWith('contrast.')).map((r) => [r[0].slice(9), ...r.slice(1)]);
      const js = expand(JSON.parse(readFileSync(join(R, 'tokens', 'colour.json'), 'utf8'))).filter((r) => S.sections.includes(r.section));
      const key = (s, fg, bg, m, a) => `${s}|${fg}|${bg}|${m}|${a}`, J2 = new Map(js.map((r) => [key(r.section, r.fg, r.bg, r.theme, r.accent), r]));
      t.eq(rows.length, js.length, `${rows.length} rows = contrast.mjs's ${js.length} for sections ${S.sections.join(', ')}`);
      const off = [];
      for (const r of rows) { const j = J2.get(key(r[0], r[1], r[2], r[4], r[5])); if (!j || Math.abs(j.exact - r[6]) > 1e-6 * j.exact || Math.abs(j.panel - r[7]) > 1e-6 * j.panel || j.pass !== r[9]) off.push(`${r.slice(0, 6).join(' ')}: C++ ${r[6].toFixed(4)}/${r[7].toFixed(4)} JS ${j?.exact.toFixed(4)}/${j?.panel.toFixed(4)} ${j?.fgHex} ${j?.bgHex}`); }
      t.eq(off.slice(0, 3), [], `every row matches (ratio exact, ratio 565, pass): ${off.length} differ`);
    } },
  ],
};
