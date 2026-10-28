#!/usr/bin/env node
// Agent discovery docs served BY the panel (facet-163): /llms.txt (a short index) + /agents.md (the spec guide: the
// components and props this release ships, binding syntax, actions, budgets, the send API with curl lines), generated
// from catalogue/catalogue.json (+ spec.schema.json), tokens/settings.json and features.json (release flags), so the
// guide always matches the firmware it ships in. firmware/bundle.mjs gzips both into fw_docs_gen.h / fw_docs.cpp.
//   node tools/gen_agents.mjs [--flavour firmware|''] --out <dir>   writes <dir>/llms.txt + <dir>/agents.md
//   node tools/gen_agents.mjs --print agents.md|llms.txt            to stdout
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { hiddenIds, hiddenCards } from './features.mjs';

const R = join(dirname(fileURLToPath(import.meta.url)), '..');
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const FLAV = arg('--flavour', process.env.FW_FEATURES ?? 'firmware');
const C = JSON.parse(readFileSync(join(R, 'catalogue', 'catalogue.json'), 'utf8'));
const S = JSON.parse(readFileSync(join(R, 'tokens', 'settings.json'), 'utf8'));
const hidden = hiddenIds(FLAV), hiddenC = hiddenCards(FLAV);
const comps = Object.entries(C.components).filter(([id]) => !hidden.has(id)).sort(([a], [b]) => a.localeCompare(b));
const cards = Object.entries(C.cards ?? {}).filter(([t]) => !hiddenC.has(t));
const agents = S.settings.find((s) => s.id === 'aiAgents');
const B = C.budget;
// "Doing things" (CONTRACT.md C50): the closed action list as the catalogue carries it (core/spec.h ACTION_DOCS)
const ACT = (C.act ?? []).filter((x) => !x.spec_only);
const val = (g) => g.split('|').map((a) => a[0] === '*' ? `a short name (at most ${a.slice(1)})` : a.replace(/#(\S+)/, (_, r) => `<${r}>`)).join(' | ');
const actLines = ACT.map((x) => { const f = (x.fields ?? []).map((f) => `\`${f.name}\`${f.need === 'required' ? '' : '?'} (${f.name === 'value' ? val(f.limits) : f.limits})`).join(', '); return `- \`${x.do}\`: ${x.about}${f ? ' ' + f + '.' : ''} e.g. \`${JSON.stringify(x.example)}\``; }).join('\n');

const llms = `# Facet panel

> A Facet wall panel (ESP32 touch screen, ESPHome firmware, Home Assistant). You can make screens for it: write a
> JSON screen spec from this firmware's components, check it on the panel, install it, open it. Read /agents.md first.

Everything below is served by this panel itself and matches its firmware (catalogue ${C.hash}).

## Docs
- [Agent guide](/agents.md): the spec format, every component and prop this firmware has, bindings, actions, limits, the send API with curl examples

## Endpoints (JSON)
- GET /fw/spec: hello (protocol, catalogue hash, components, screens and revs, budgets, free memory)
- POST /fw/spec {"kind":"check","spec":{...}}: validate + test-build a screen spec, stores nothing
- POST /fw/spec {"kind":"screen","spec":{...}}: install (or update) a screen
- POST /fw/spec {"kind":"deck","spec":{...}}: replace the page list; {"kind":"revert","id":"<id>|all"} undoes
- POST /fw/show {"screen":"<id>"}: open a screen
- GET /fw/deck: the pages this panel runs; GET /fw/info: firmware build; GET /screenshot: the screen as a BMP

## Doing things
- GET /fw/act: every action this panel can run (fields, limits, one example each)
- POST /fw/act {"do":"timer.start","seconds":300}: run one (lights, climate, media, covers, locks, vacuum, timers, opening screens) -> {ok, why?, state?}

## Access
Writes need the panel setting "${agents.label}" (off by default; the owner turns it on on the panel or in Home
Assistant) and the header \`Authorization: Bearer <agent token>\` (the owner reads it in Home Assistant: the panel's
"AI agent token" entity). Without them a write (and GET /fw/ha, the panel's Home Assistant values) answers 403 with
what to do. Reading the docs and the other GET endpoints is open. Browsers: CORS is answered (OPTIONS preflight allows
Authorization, Content-Type and X-Facet-Token from any origin), so a web page can call the panel with the token.
`;

const typ = (p) => {
  if (p.type === 'select') return (p.options ?? []).join(' | ');
  if (['int', 'number', 'float', 'range'].includes(p.type)) return 'number' + (p.min != null ? ` ${p.min}..${p.max}` : '');
  return p.type ?? 'text';
};
const compLines = comps.map(([id, c]) => {
  const L = [`### ${id}${c.name ? ` (${c.name})` : ''}`];
  if (c.about) L.push(c.about);
  for (const p of c.props ?? []) L.push(`- \`${p.name}\`: ${typ(p)}${p.default != null ? ` (default ${JSON.stringify(p.default)})` : ''}${p.bindable ? ', bindable' : ''}${p.about ? `. ${p.about}` : ''}`);
  const ev = Object.entries(c.events ?? {});
  if (ev.length) L.push(`- events: ${ev.map(([e, a]) => `\`${e}\` -> ${a.join(', ')}`).join('; ')}`);
  if (typeof c.slots === 'number') L.push(`- layout with ${c.slots} slot(s): later nodes go in with "in": <this node's index>, "slot": n`);
  else if (Array.isArray(c.slots)) for (const s of c.slots) {
    const takes = Array.isArray(s.accepts) ? s.accepts.join(', ') : 'any component', max = s.max ? `, at most ${s.max}` : '';
    const how = s.template ? 'the node\'s "cell" template' : s.count ? `${s.count} numbered slots: "in": <this node's index>, "slot": n` : `"in": "${id}", "slot": "${s.name}"`;
    L.push(`- slot ${s.name} (${how}): takes ${takes}${max}${s.about ? `. ${s.about}` : ''}`);
  }
  return L.join('\n');
});

const P = 'http://<panel-ip>';
const md = `# Facet agent guide

How to make screens for this Facet panel. Generated from the firmware's own catalogue (${C.hash}, spec protocol
${C.protocol}): the components and props below are exactly the ones this panel accepts. Workflow: read GET /fw/spec,
write a spec, POST it as "check" until it is valid, POST it as "screen", then open it with /fw/show.

## Rules
- A screen is a JSON spec of registered components only. No positions, sizes, colours, fonts or sounds: the layout and
  the components decide those (style keys are refused with \`style_key\`).
- One hero per screen, keep text short, list layout = rows that touch (they draw their own hairlines).
- Ids: lower case a-z 0-9 _. Send the same id again to update a screen; \`rev\` is your label, the content decides.
- Limits: ${Object.entries(B).map(([k, v]) => `${k} ${v}`).join(', ')} (nodes / binds / bytes per screen; screens,
  pages, homes, cards per panel).

## Screen spec
\`\`\`json
{"v": 1, "id": "office", "name": "Office", "rev": 1, "role": "page",
 "params": {"entity": {"type": "entity", "domain": "light", "label": "Light", "default": "light.office"}},
 "header": {"label": "Office", "back": true},
 "layout": "list",
 "nodes": [
  {"c": "section_header", "p": {"label": "Lights"}},
  {"c": "list_row", "p": {"label": {"e": "$entity", "fmt": "name", "s": "Office"}, "sub": {"e": "$entity", "fmt": "light", "s": "On"}},
   "on": {"toggle": {"do": "light.switch", "e": "$entity"}}}]}
\`\`\`
- \`role\`: ${C.roles.join(' | ')} (page = in the page list; detail = opened from a card for one entity; both).
- \`layout\`: ${C.layouts.join(' | ')} (list = one scrolling column, center = one node centred, cards = the card grid).
- \`params\`: the screen's inputs; \`"$name"\` in any string = the value it was opened with. Types: text, select,
  entity, entities (a comma list), toggle, hidden; with default, label, domain, options, about.
- Node: \`{"c": component, "p": {prop: value | bind}, "on": {event: action}, "pending": {prop: [on text, off text]},
  "in"?: k, "slot"?: n, "each"?: "<entities param>", "sample"?: [{props}], "show"?: bind}\`.
  - \`each\` repeats the node per item of a comma list param (\`$item\` inside); \`sample\` = demo rows while it is empty.
  - Lists of rows from data: a node's \`items\` prop (Grid; List row with \`list: stack\`; Picker row with \`list: group\`;
    Scroll track) = a comma list or a JSON list, static or bound to an attribute; each row/cell fills \`$item\` /
    \`$item.key\`; \`rows\` caps it (a +N row), \`empty\` = the words with no items. No items never shows sample rows.
  - \`show\` = a bind: the node shows only while its value is set (hidden for "", 0, off, false).
  - \`"in": "header"\` puts the node in the header row; \`"in": k, "slot": n\` puts it in slot n of layout node k.

## Binding to Home Assistant
A prop marked bindable takes a bind instead of a value:
\`{"e": "light.x" | "$param", "a"?: "<attribute>", "fmt"?: <fmt>, "s"?: "<sample>", "map"?: {"<value>": "<text>"}, "else"?: "<text>", "tpl"?: "Battery {}%"}\`
- fmt: ${C.fmt.join(', ')}.
- \`s\` is shown until Home Assistant sends the value (at most 64 characters; 256 for an \`items\` list); \`map\` turns states into words; \`tpl\` wraps the value (\`{}\`).
- The panel subscribes to bound entities itself through its Home Assistant connection.

## Actions
\`"on": {"<event>": {"do": "<action>", "e": "<entity>"}}\`; each component's events allow only the actions listed with
it below (a closed whitelist). Actions: ${C.actions.join(', ')}.
- \`{"do": "nav.open", "screen": "<id>", "params": {...}}\`, \`{"do": "nav.back"}\`, \`{"do": "screen.param", "param": "<p>"}\`,
  \`{"do": "screen.toggle", "param": "<list param>"}\`; any event can also be \`"long"\` (a long press).

## Pages (the deck)
GET /fw/deck shows the page list. To add your screen as a page, send the whole deck back with a new entry
\`{"id": "<page id>", "kind": "<your screen id>"}\` (kind = a page-role screen or "home").${cards.length ? `
Home cards (\`{"t": type, "e": entity, "size", "tap", "label"?}\`): ${cards.map(([t, c]) => `${t} (${c.domains ?? 'page'}; ${c.sizes}; tap ${c.taps})`).join(', ')}.` : ''}

## Send API
Writes need "${agents.label}" on (the owner turns it on on the panel, Settings > ${S.groups.find((g) => g.id === agents.group)?.label ?? 'AI agents'},
or in Home Assistant) and the panel's agent token (Home Assistant entity "AI agent token"). A refused call answers 403
\`{"ok": false, "error": ..., "agents": "off" | "token"}\`.
\`\`\`sh
P=${P}; T=<agent token>
curl -s $P/fw/spec                                   # hello: components, screens, budgets, heap
curl -s -H "Authorization: Bearer $T" -H 'Content-Type: application/json' \\
  -d '{"kind":"check","spec":'"$(cat office.json)"'}' $P/fw/spec    # {ok, status: valid|refused, errors: [{path, code, msg}]}
curl -s -H "Authorization: Bearer $T" -H 'Content-Type: application/json' \\
  -d '{"kind":"screen","spec":'"$(cat office.json)"'}' $P/fw/spec   # {ok, status: applied|unchanged|refused}
curl -s -H "Authorization: Bearer $T" -H 'Content-Type: application/json' \\
  -d '{"screen":"office"}' $P/fw/show                               # open it
curl -s -H "Authorization: Bearer $T" -H 'Content-Type: application/json' \\
  -d '{"kind":"revert","id":"office"}' $P/fw/spec                   # back to factory (or "all")
curl -s $P/fw/deck                                   # the page list ({deck, rev, override})
curl -s -o shot.bmp $P/screenshot                    # what the glass shows (16-bit BMP)
\`\`\`
Errors carry a path (\`nodes[2].p.label\`) and a code (unknown_component, unknown_prop, bad_prop, bad_bind,
bad_action, over_budget, style_key, build ...): fix that node and check again. With Home Assistant access instead of
the panel's HTTP: the ESPHome actions \`esphome.<node>_facet_screen\` / \`_facet_deck\` / \`_facet_revert\` do the same.

## Doing things
One closed list of actions runs everywhere: a spec node's event, \`POST /fw/act\` (agents) and the Home Assistant
action \`esphome.<node>_facet_act\` \`{"action": "<the same JSON as a string>"}\` take the same actions with the same checks
and limits. Outside a spec the value a component's event would give is the \`value\` field. GET /fw/act lists them as
JSON. Answer: \`{"ok": true, "state": {...}}\` (the timers, the entity's state, the screen) or \`{"ok": false, "why": "..."}\`
(HTTP 400). Writes need the agent token, as above.
\`\`\`sh
curl -s -H "Authorization: Bearer $T" -H 'Content-Type: application/json' \\
  -d '{"do":"timer.start","seconds":300,"name":"Pasta"}' $P/fw/act
\`\`\`
${actLines}

## Components (${comps.length})
${compLines.join('\n\n')}
`;

const pr = arg('--print');
if (pr) { process.stdout.write(pr === 'llms.txt' ? llms : md); process.exit(0); }
const out = arg('--out');
if (!out) { console.error('usage: gen_agents.mjs --out <dir> | --print agents.md|llms.txt'); process.exit(2); }
if (!existsSync(out)) mkdirSync(out, { recursive: true });
for (const [f, s] of [['llms.txt', llms], ['agents.md', md]]) {
  const p = join(out, f);
  if (!existsSync(p) || readFileSync(p, 'utf8') !== s) writeFileSync(p, s);
}
