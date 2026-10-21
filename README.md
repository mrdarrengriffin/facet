# Facet WASM workbench

A fresh, minimal place to build Facet components from scratch in C++/LVGL, compiled to WebAssembly and shown in a
browser workbench page. This is the orphan branch `wasm` of the Facet repo: it has no history shared with `main`
(docs site) and holds only the workbench and the components built in it.

The page (served by the host app that embeds this build) has a left nav (Foundations / Components / Screens, filled
from what the build registers), and per component a preview, a variants grid, a playground (props -> live re-render),
Dark/Light and device toggles.

## Repository layout

| Folder | What |
|---|---|
| `renderer/` | the C++/LVGL renderer: `core/` (component API, registry, WASM host), `components/`, `foundations/`, `tokens/` (colour source), `tools/` (generators), `build.sh` + `watch.mjs` (WASM build and watcher) |
| `firmware/` | device side: ESPHome external component `facet_wb` (`fw_device.h`, `esphome_init.py`) and the bundler that packs `renderer/` into it (`bundle.mjs`) |
| `local.env` | this machine's paths (untracked), read by `renderer/build.sh` and `firmware/bundle.mjs` |

Later, as on `main`: `site/` (Astro docs) and `packages/` (pnpm workspace). All paths below are relative to
`renderer/` unless they start with `firmware/`.

## Workflow: WIP -> approved -> one commit per component

- Every component starts as **WIP**: a new `components/<id>.cpp`, untracked.
- Iterate on it with the watcher running; the page updates on save.
- When it is approved, commit **that one file** on `wasm` (`git add renderer/components/<id>.cpp && git commit`). It then shows
  as **Done**.
- Builds always compile every file in `components/` (WIP and Done) into the module; commits are per component file.

### Status key (derived from git, no status files)

| Key | Meaning |
|---|---|
| WIP (amber dot) | file untracked in this worktree |
| Done (green dot) | committed and unchanged |
| Done, edited (green ring) | committed, modified since |

The workbench reads `git ls-files` + `git status --porcelain` on `components/` and maps each file to its component
ids by its `FW_COMPONENT(<id>, ...)` blocks. Shown next to every nav item, on the component heading, and as a key in
the nav footer.

## Preview zoom

- **1:1 (default):** true scale, one panel pixel = one physical screen pixel (CSS size = panel px / devicePixelRatio).
- **Fit:** the biggest whole-number scale (in screen pixels) that fits the box, or shrink when it does not fit.
- **2x:** two screen pixels per panel pixel.

**Screen / Component only** (`?view=component`, default Screen): the main preview is one WASM display of the device's real size (720 or 480 px, Facet `bg`) with the component centred on it at true size, so zoom scales the whole screen.

Rendering is nearest-neighbour at every zoom; touch is mapped from the canvas box back to panel pixels, so it is
correct at any zoom. The variants grid is always 1:1.

**Pixel grid** (sidebar Settings, Off / Grid / Grid + boxes, per browser `localStorage wb.pixgrid`; hub `src/wasm/pixgrid.ts`): under the preview, the component magnified nearest-neighbour (crop = its objects + 8 px, origin on the 4 px grid) with a 1 px grid and every 4th line stronger at stage coordinates; hover = x / y, pixel colour and the objects under it; boxes = every LVGL object outlined with x,y w x h (core export `fw_rects(stage)` = `[{d, x, y, w, h}]`, coords as drawn incl. translate) plus a list (size, place in its parent, gaps right / bottom, "off the 4 px grid"). Live while it animates. **Size studies:** meta `"compare": [{density?, label, about, props, on}]` = debug-only props (not variants, not in the playground) shown off + on in the Pixel grid view for that density. None declared now (the Toggle 480 study was removed after Darren picked 56 x 32).

## Colour tokens (single source)

`tokens/colour.json` is the only place colours live in this branch: per token `group`, `dark`, `light`, `role`; the
groups (page sections); `contrast` (kinds + pairs, below); `accent` = the accent sets (five
roles each: accent, accent-press, accent-dim, accent-text, on-accent, both themes) + the default set. Values are the
approved Facet tokens (`main`: `packages/tokens/src/tokens.json` color.*, accent sets from
`packages/settings/src/settings.json` setting `accent`).

`build.sh` runs `tools/gen_colour.mjs` -> `$FW_CACHE/gen/colour_gen.h` (rewritten only on change; every object depends
on it, so editing the JSON rebuilds like a component). It defines `fw::Theme` (one `lv_color_t` per token, `-` -> `_`:
`fw::T->text_dim`) and `fw::tokens::` tables (COLOURS, GROUPS, ACCENTS, CONTRAST). Components read `fw::T` only, never
a hex. `fw::theme(light, set)` gives any theme (e.g. to show both side by side).

**Accent selection:** a theme = dark/light + one accent set; the set replaces the five accent roles in both themes (on
the panel: the Accent setting, default Ember). The workbench passes it to every render (`fw_render(..., light,
accent)`, toolbar Accent switch, `?accent=<id>`).

### Contrast test (every variation)

`tokens/colour.json` `contrast.pairs` declares every place two colours meet: `{section, fg, bg, kind, use}` (`bg` may
be a list: a transparent control sits on the canvas or a card). Foundations first, then one block per component
(`section` = component id). **A new component (or a changed look) adds/updates its pairs** for every kind x state it
draws; a label pair covers that control's icon (same colour, stricter threshold). Kinds (WCAG 2.x AA): `text` 4.5,
`large` 3 (>= 24 px regular / 18.66 px bold at panel px), `ui` 3 (fills, borders, focus rings, icons, state marks),
`disabled` 3 (Facet rule: WCAG exempts disabled controls, Facet wants them readable), `decor` no requirement (1 px
outlines/separators, shown for reference). `node renderer/tools/contrast.mjs` tests every pair x theme x accent set on
the exact values AND the RGB565 values the panel shows (worse counts), lists failures, exit 1 if any (`--all`,
`--json`, `--summary`); `build.sh` prints the summary line (never fails the build). Colour page sections
"Contrast: summary / failures / <section>" draw the same matrix in WASM (selected accent = toolbar, dark + light side
by side, failing rows tinted, ratio red + FAIL).

## Type tokens (single source)

`tokens/type.json` holds every type style: `scale` (screen / component), `font` (sans = Inter, mono = JetBrains Mono),
`weight`, `size`, `box` (line box), `lineHeight`, `letterSpacing`, `case` (`upper` = mono caps), `glyphs` (glyph set),
`use`, `sample`, and `1x` (the 480 px panels' values). Sizes are **device px**: top level = the 720 px reference (1.5x),
`1x` = 480 px (Facet's optical pass: thin weights one step heavier, small text one step up; component scale = x 2/3,
min 12, boxes on 4). Plus `glyphsets` (digits / numeric / ascii / text = GF Latin Core), `numbers` (how numbers and
units are set) and `tabular` (styles whose digits share one advance). Values = Facet `main` tokens.json `screenType` +
`type` (the text styles the docs Type page shows; clock digit styles come with the Clock component).

`build.sh` runs `tools/gen_type.mjs` -> `type_gen.h` (`fw::ty::Id` per style, `-` -> `_`: `ty::display`, `ty::c_meta`;
`fw::ty::STYLES` for this density) and `tools/gen_fonts.py <density>` -> `facet_fonts[_1x].h`: one 4 bpp bitmap font
per (family, weight, size, tabular) the styles use, with only the union of their glyph sets (skips work when nothing
changed; editing the JSON rebuilds). Components use `fw::text(parent, ty::body, "...", fw::T->text)` = font, letter
spacing, case and the text-box rule (height = box, cap height centred in it; `wrap_w` = paragraph on the box pitch),
`fw::set_text`, `fw::font(ty::x)`; never a size. `fw::font(dp, mono)` = nearest text font for one-offs.

## Space tokens (single source)

`tokens/space.json` holds spacing, layout, radius, stroke and touch tokens in **device px**: `v` = the 720 px reference
(1.5x, 8 px grid) = Facet `main` tokens.json `space` (steps 1-10), `layout` (inset, gutter, group, row, top, card/tile),
`radius`, `stroke`, `size` (touch targets); `1x` = 480 px (4 px grid). 480 sources per group (`groups[].source1x`):
layout + touch = the docs `density.1x` table; space + radius = x 2/3 snapped to the 4 px grid, strokes 1-2 kept,
>= 3 px even. **Grid rule (Darren 2026-10-21): every spacing, layout, radius and touch value is a multiple of 4 at both
densities, no odd numbers**; `gen_space.mjs` fails the build on an off-grid value, on a 720 value only on the 2 px grid
without a `grid2` reason (approved docs value waiting for Darren: space 4 = 14, radius chip = 6), and when 2 Home cards or
3 tiles + gutters + insets do not fill the screen exactly (480: 20 + 3 x 136 + 2 x 16 + 20). `fw::px()` snaps one-off
geometry to the 4 px grid too. Legacy `space.screen` / `space.gutter` (layout v2, inset 16) left out: layout v3 `lay::inset` 32.
`build.sh` (and `firmware/bundle.mjs`) run `tools/gen_space.mjs` -> `space_gen.h`: `fw::sp::s1..s10` + `sp::S[]` +
`sp::GRID`, `fw::lay::inset|gutter|group|row|top|card_w|...`, `fw::rad::card|chip|button|...`, `fw::stroke::hair|...`,
`fw::touch::target|min|back|...`, already device px (never wrap in `px()`); `fw::px(dp)` stays for one-off geometry.

## Motion tokens (single source)

`tokens/motion.json` = Facet `main` `tokens.json` `motion` (durations, easings, `transition.rise`) + how `@facet/ui` uses
them (bottom sheet, toast, screen rise, dial sweep, press / toggle transitions, the `prefers-reduced-motion` rules),
`opacity.scrim` / `pulse-min`. **20 durations** (groups: interaction, ambient loops, timing), **3 easings** (`linear`,
`ease-out` = cubic-bezier(0, 0, 0.58, 1), `ease-in-out` = (0.42, 0, 0.58, 1)), **12 patterns** = duration + easing + what
animates + offset (`v` 720 / `1x` 480 px, on the 4 px grid) + reduced-motion mode: press, release, toggle, fade,
sheet-open, sheet-close, toast-in, toast-out, screen (the Facet "Content rise", = the old lab's `screens.ts` `T.rise`:
cut, then blocks grouped into rows by their top (side by side = one row), header first, each row fades in + rises 8 /
4 px (one grid step) over 160 ms ease-out, 40 ms per row, at most 6 steps; back to Home they drop in from above), value (dial-sweep count-up), clock-hand, pulse.
`tools/gen_motion.mjs` (build.sh + bundle.mjs) -> `motion_gen.h`: `fw::mo::ms::<duration>`, `fw::mo::ease::<easing>`
(LVGL path callbacks: an easing naming an LVGL built-in must match that built-in's bezier exactly, the generator checks;
an easing without `lvgl` gets a generated `lv_cubic_bezier` path function, usable in style transitions too),
`fw::mo::Id` (`mo::press`, `mo::sheet_open`, ...) + `mo::PATTERNS[]` (ms, easing, offset px for this density, stagger,
steps, reduced mode) and the Motion page tables. Never a raw duration or easing in a component.

**Reduced motion** (one setting, off by default): nothing moves. Modes per pattern: `keep` (colour / opacity only:
press, release, fade), `fade` (opacity only, at most `fade` 150 ms: sheets, toasts), `instant` (toggle, screen, value,
clock-hand), `still` (loops stop on their resting frame: pulse). Runtime (`core/motion.cpp`): per display
(`mo::set_reduced(display, on)`; `fw::render` sets it from the props key `reduced_motion`, else the panel default
`mo::set_reduced_default`), `mo::reduced(obj)`, `mo::duration(obj, pattern, channel)` (0 = instant).

API (`core/fw.h`): `mo::transition(&dsc, mo::press, PROPS)` = a style transition for a colour pattern;
`fw::animate(obj, mo::value, {exec, from, to, mo::VALUE})` = one animation with the pattern's duration + easing,
collapsed by reduced motion (`loop` = there and back forever); `fw::appear(obj, mo::sheet_open, in, dir)` = opacity +
translate_y by the pattern's offset from wherever it is now; `fw::rise(rows, n, home)` = the screen pattern. Movement
is translate only (no transforms that need a layer); opacity via the `opa` style (multiplied into every draw, no layer).

Page `/wasm/foundations/motion` (`foundations/motion.cpp`): easing curves plotted from the very LVGL path callbacks with
a dot along the curve + a box moving with it (1 s, tap = replay), durations as bars (log scale for ambient; tap = a run
at the real length), every pattern live on a sample (the real Button for press / release; toggle knob, fading card,
sheet, toast, screen rise, count-up, clock hand, pulse), a reduced-motion table. Playground: `reduced_motion`, `loop` =
one pattern at full screen size over and over (the frame-rate bench). Contrast pairs: `colour.json` section `motion`.

**Frame-rate check** on the panels: `POST /fw/show {"id": "motion", "props": {"loop": "<pattern>"}}`, then
`GET /fw/perf?reset=1`, wait, `GET /fw/perf` (`firmware/fw_device.h`; browser targets: the host's device page reports
the same stats, e.g. the hub's `GET /api/wasm/perf?target=shelly[&reset=1]`, WASM export `fw_anims()`): frames counted only while an LVGL animation
runs; `fps`, `frame_ms_avg|max` (start to start), `render_ms_avg|max` (render + flush), `slow` = frames later than 1.5
refresh periods (a missed slot), `refr_period_ms` (LV_DEF_REFR_PERIOD).

Results 2026-10-21 (full-screen loop, dark, 4 s each; one bench script in the host repo runs all
three targets). fps / render ms avg (panels: LVGL render + flush; Shelly: WASM tick + render + canvas copy):

| Pattern | Waveshare P4 (720) | Guition (480, 90 deg sw rotation) | Shelly X1i (720, WASM in Webview Kiosk) |
|---|---|---|---|
| press / release | 53 / 0.8 | 53 / 2.6 | 38 / 5.2 |
| toggle | 53 / 0.7 | 52 / 1.6 | 29 / 4.9 |
| fade | 59 / 4.1 | **39 / 21** | 36 / 12 |
| sheet open | **32 / 23** | **12 / 89** | **20 / 42** |
| sheet close | **36 / 24** | **11 / 89** | **20 / 42** |
| toast in / out | 53-60 / 4 | 51-59 / 10.5 | 36-40 / 9 |
| screen (content rise) | 53 / 6.6 | **19 / 44** | 37 / 19 |
| value | 58 / 1.1 | 57 / 3.5 | 39 / 6.2 |
| clock hand | 60 / 4.6 | 49 / 15 | 43 / 10 |
| pulse | 53 / 0.4 | 52 / 0.9 | 40 / 4.4 |

Ceilings: the panels' ESPHome loop caps a trivial animation at ~53 fps (refresh 16 ms + loop jitter, ~17 % "slow"
frames even at 1 ms render); the Shelly's browser path tops out near 40 fps (each repaint copies the whole 720 x 720
stage to the canvas, ~4-5 ms, plus the WebView's own compositing). Cost = large areas blended at partial opacity each
frame (the sheet scrim, every row of a screen rise), not movement.
**Decision (Darren 2026-10-21): keep both fades as designed** (the sheet dims the screen as it opens; the screen
transition stays the content rise with its fade); optimise later. These numbers are the reference for that work:
the expensive parts are the sheet dim fade and the screen rise fade. Candidate fixes: a stepped dim (2-3 opacity
steps), rise without the fade, or `instant` on 1x boards.

## Icons (single source)

`tokens/icons.json` = every icon the Facet docs define (Facet `main` `packages/icons/manifest.json`), masters copied 1:1
into `renderer/icons/` (Facet's own, MIT): **74 icons** = 37 UI (`icons/*.svg`, 24 grid, one colour), 20 screen icons
(`icons/screens/`, base + one accent part), 17 weather (`icons/weather/` static masters, 48 grid, colour layers; the
animation and the moon phase, drawn in code from the phase, are later work). `screen-bins` (bin collection screen) is drawn here, not in `main`: handle, lid bar, tapered can, two accent ribs at x 9.75 / 14.25 so at 16 px each rib is one full pixel with a 25 % valley either side (at 12 px they would fuse; screen icons do not ship at 12). Each icon lists its `sizes` = **size
tokens** (`v` = 720 px, `1x` = 480 px, both on the 4 px grid): `small` 16/12, `icon` 24/16 (= `touch.icon`), `glyph`
32/20, `detail` 40/28, `feature` 48/32, `card` 72/48, `hero` 144/96, mapped from the manifest's `sizes_used` (720 px);
icons the docs list for the component library only ship at `icon`; plus/check/close add `icon` for the Button.
`compare` = the Icons page's size-comparison row (the docs' Optical sizes demo: speaker, timer, done, back, play at
small/icon/glyph/feature; the extra sizes are compare-only).

`tools/gen_icons.mjs` (run by `build.sh` + `firmware/bundle.mjs`; rasteriser = resvg, `cd renderer/tools && npm install`
once) -> `icons_gen.h` (`fw::ic::Size`, `ic::PX[]` for this density, the icon table, page sections) + per density
`facet_icons.h` / `facet_icons_1x.h` (included by `core/icons.cpp` only; the bundle inlines it). **Every icon x size is
rasterised at its exact pixel size, never scaled on the panel** (the old build scaled 24 -> 16 and it turned to
noise), with Facet's optical stroke (1.5 / 2 / 2.5 / 3 px at 16 / 24 / 32 / 48, linear, clamped; weather 2 px, 3 from
96). Output = A8 alpha masks cropped to their ink, one per colour layer: each layer is rendered white with every other
colour black, so a cloud's bg fill hides what is behind it without a bg layer. LVGL draws A8 straight through the
mask blend with the image recolour (no decode, no cache). **Flash: 720 build 287 KB, 480 build 139 KB** (182 icon x
size, 302 masks, tables 6 KB). `FW_ICONS=used` (local.env or env) = only the `icon@size` pairs components declare in
their meta (`"icons": ["plus@icon"]`): Button alone = 3 masks, < 1 KB; the Icons page then shows what the build has.

API (`core/fw.h`): `fw::icon(parent, "name", fw::ic::<size>, colour)` -> an `ic::PX[size]` square box (nullptr when the
icon@size is not in the build); ink layers take `colour`, a screen icon's accent part `T->accent`, weather layers their
tokens (text-dim, accent, blue, steel; the cloud stroke = ink). `fw::icon_colour(icon, colour, selector, transition)`
recolours the ink per state (the Button's pressed / active / disabled looks). Contrast pairs: `colour.json` section
`icons` (ink, accent, text-dim, blue, steel on bg + surface, kind ui). Page `/wasm/foundations/icons`: size comparison,
one section per group, every icon, flash per size and kind; playground search / size / accent tint / boxes.

## Sound (single source)

`tokens/sound.json` = the approved Facet UI sounds, copied 1:1 from `main` (`packages/sounds/src/packs.json`, synth
`BASE`, tokens `sound` levels, the docs' meaning lines): `types[]` (10: id, name, `for`, per-variant values + `use`),
`variants[]` (the 6 packs: keycap default, halo, arcade = basic engine; graphite, crystal, felt = modal), `base`,
`levels` (peak/loudness/fades, sample rates). One sound = `base` + `variant.defaults` + `types[i].variants[v]`;
procedural, no samples (nothing to licence). Page `/wasm/foundations/sound` = hub HTML (about hearing, not pixels): a
compact table like the docs' "Try them" grid (row per type: name, what it's for, a play button per variant column; a
pack header plays the whole pack), synthesised in the browser with `render()` of `@facet/sounds` (main); status = the JSON
alone (no .cpp). **Later, not built:** playback on the panels (pre-rendered WAVs / firmware), sound rules (one tick per
value step on sliders/dials, which control plays which sound, toast sound, sound on/off + volume settings).

## Foundations

`foundations/<id>.cpp`, same `FW_COMPONENT` API with section `foundations`. Meta `"layout": "doc"` = a document page:
one full-width WASM stage per variant (heading + `about` in HTML), `"frame": [w, 0]` = auto height (the stage follows
its content, `fw_content_h` + `fw_stage_resize`). Status = the .cpp plus `tokens/<id>.json` if present (any untracked
= WIP; approve = commit both; Motion also adds `core/motion.cpp` + `tools/gen_motion.mjs`). **Motion**
(`foundations/motion.cpp`): see "Motion tokens". **Type** (`foundations/type.cpp`): both scales at true size in their line boxes (hairlines) with spec + use, mono caps beside sans, number styles (units, tabular digits), glyph coverage per set, the fonts of the build with their flash cost (device switch = 720 / 480 build). **Spacing & radius** (`foundations/space.cpp`, id `space`): spacing steps (true-size square + 8x bar), layout tokens (true-size bars + grid), radius samples on surface cards, strokes (line, ring, outlined control), touch targets (hit area tinted around a smaller drawn back button / chip / row), the whole screen at true size (grid minor + major, inset bands, header row, content from `top`, Home 2 columns with card padding); every value also shows the other density. **Colour** (`foundations/colour.cpp`): every token by group, name + role, the colour in
its dark and light context with its hex (chips drawn by LVGL in RGB565, as on the panel), the accent sets, the contrast
matrix (summary per accent x theme, all failures, every pair per section; see Contrast test). **Icons**
(`foundations/icons.cpp`): see Icons.

## Fast loop

```
node renderer/watch.mjs
```

Save a `.cpp`/`.h` in `components/`, `foundations/` or `core/`, or `tokens/*.json` -> incremental rebuild of both densities (a few seconds) -> the page
notices the new `manifest.json` within a second and re-renders in place (playground values kept). One-off build:
`renderer/build.sh [1.5x|1x]`. First build is slower (LVGL objects, cached).

### Paths (`local.env`, untracked)

`renderer/build.sh` and `firmware/bundle.mjs` read these from the environment or `local.env` at the repo root:

| Var | What |
|---|---|
| `PANEL_BUILD` | ESPHome build dir of the panel firmware (LVGL tree, `lv_conf.h`, ArduinoJson) |
| `FW_PANEL` | panel firmware source dir (`packages/ui.yaml` for the `LV_*` build flags) |
| `FW_FONT_PY` | a Python with the `esphome` package (its FreeType rasteriser + glyph sets, used by `tools/gen_fonts.py`) |
| `FW_TTF` | dir of the TTFs, named `<Family>@<weight>@False@v1.ttf` (ESPHome's Google Fonts cache; Inter, JetBrains Mono, SIL OFL) |
| `FW_OUT` | output dir (default `renderer/out`): `<density>/fw.mjs`, `fw.wasm`, `manifest.json` |
| `FW_CACHE` | object cache (default `renderer/.cache`) |

**TODO:** vendor LVGL, `lv_conf.h` and the TTFs into this branch so it builds on its own, without a panel
firmware build next to it.

## Device firmware (Show on device)

The same core + foundations + components also run on real panels, as an ESPHome firmware that shows whatever the
workbench sends (the toolbar's **Show on device**). Facet side (this branch, no device specifics):

| Path | What |
|---|---|
| `core/fw_core.cpp` | shared by every host: themes, registry, `fw::render(screen, comp, props, light, accent)` (the WASM host calls it too) |
| `firmware/fw_device.h` | ESPHome glue: own LVGL screen, `GET /fw/info` (build hashes), `GET /fw/meta`, `POST /fw/show {id, props, theme, accent}`, `GET /fw/perf[?reset=1]` (frame stats while animations run); last request kept in flash; tall content scrolls |
| `firmware/esphome_init.py` | the external component `facet_wb` (`board`, `width`, `height`) |
| `firmware/bundle.mjs` | `--density 1x\|1.5x --out <dir>` writes `<dir>/facet_wb/` (flat, fonts inlined, link keepers); `--hashes` prints the source hashes |

**Rebuild rule:** props, theme, accent and variant are runtime data (no rebuild). New or edited component/foundation
code, core, tokens or fonts need a firmware build: the device reports the hashes it was built from (`/fw/info`), the
host compares them with `firmware/bundle.mjs --hashes` (keys relative to `renderer/`) and shows "Needs firmware build" instead of sending stale code.
Board YAML (display, touch, Wi-Fi) lives with the host that builds the firmware, not here.

## Renderer layout (renderer/)

| Path | What |
|---|---|
| `components/*.cpp` | one component per file (the only place new work goes) |
| `tokens/colour.json`, `tools/gen_colour.mjs` | colour tokens (single source) and their header generator |
| `foundations/*.cpp` | foundation pages (Colour, Type, Spacing & radius) |
| `tokens/motion.json`, `tools/gen_motion.mjs`, `core/motion.cpp` | motion tokens (single source), header generator, runtime (reduced motion, animate / appear / rise) |
| `tokens/space.json`, `tools/gen_space.mjs` | spacing / layout / radius / stroke / touch tokens (single source) and their header generator |
| `core/fw.h` | the component API: `FW_COMPONENT`, `fw::T` colours (tokens/colour.json), `fw::text(parent, ty::<style>, ...)` / `fw::font(ty::<style>)` (tokens/type.json), `fw::px(dp)`, `fw::force_state` |
| `core/fw_core.cpp` | themes, registry, `fw::render` (shared by the WASM host and the device firmware) |
| `core/main.cpp` | host: LVGL + "stages" (one display per canvas), exports `fw_init/meta/stage/stage_free/render/touch/tick/dirty/frame` |
| `core/fonts.cpp` | this density's generated fonts (`facet_fonts*.h`), style -> font, font report |
| `tokens/type.json`, `tools/gen_type.mjs`, `tools/gen_fonts.py`, `tools/facet_font.inc` | type tokens (single source), style header generator, font generator, bitmap font runtime |
| `tokens/icons.json`, `icons/`, `tools/gen_icons.mjs`, `core/icons.cpp` | icons: manifest (single source), SVG masters, A8 mask generator (resvg, `tools/package.json`), `fw::icon` runtime |
| `build.sh`, `watch.mjs` | build and watcher |

Densities: `1.5x` = 720 px panels, `1x` = 480 px panels. Write sizes in dp; `fw::px()` scales.

## Events and sound

A component reports what happened, the host decides what to do (`core/fw.h`): `fw::emit(obj, "tap", detail)` = an
event, `fw::sound(obj, "tap")` = a UI sound request by `tokens/sound.json` type id (tap, error, toggle-on, ...).
WASM host: queued, drained by the page every frame (`fw_events()` -> `[{stage, kind, name, detail}]`); the workbench
shows them in the component page's **Events** panel (tap count, newest first, source = Preview or the variant) and
plays sounds in the browser with the default pack (Sound switch, per browser). Device: logged (`facet_wb` ESP log);
**TODO** sound playback on the panels. Rule from the docs: sounds only on real taps = LVGL `CLICKED` (dropped after a
scroll); a disabled control plays `error`.

Step ticks (Darren 2026-10-21): every slider / dial / arc calls `fw::step_ticks(obj, from, to, n)` when its SHOWN value
moves between steps (core `fw_core.cpp`, one limiter per display). Every step counts; a `slider-tick` sound request is
made only when the last one is at least Motion `tick-min` (26 ms, motion.json timing; average rate, not snapped to 16 ms frames) ago, so a slow drag ticks every
step and a fast one thins out (never more ticks than steps). Landmark steps (`LANDMARKS` = the ends, add 0.5 for 50 %)
request `slider-landmark` (sound.json: the tick a little stronger and lower) and are never dropped: too soon, they wait
out the interval on a one-shot `lv_timer`. In practice LVGL reads the pointer every ~33 ms, so a fast drag patters at
one tick per read.

Motion: components take every duration / easing from the Motion tokens (see "Motion tokens"); Button = patterns
`press` + `release` as style transitions.

## Components

| Id | What |
|---|---|
| `button` | Facet text button: kinds primary / secondary / outline / ghost, states default / pressed / active / disabled, optional icon (plus, check, close: `fw::icon(..., ic::icon, ...)` from the Icons foundation, ink recoloured per state; meta `"icons"` declares them for `FW_ICONS=used`), hug (label width on the grid) or fixed width. Disabled = `LV_STATE_USER_1` (LVGL sends no events to `LV_STATE_DISABLED`, and a disabled tap must play Error). Children follow the state via `LV_OBJ_FLAG_STATE_TRICKLE`. |
| `toggle` | Facet switch (docs `toggleSwitch()`, `.f-toggle`): off / on x default / disabled (4 variants). Geometry from tokens: height `touch::min`, inset `sp::s2`, knob = height - 2 x inset, width = 2 x knob + 2 x inset (720: 80 x 48, knob 32, = docs; 480: 56 x 32, knob 24, travel 24). Tap = Motion pattern `toggle` (knob slide + track colour via `fw::animate`, reduced = instant), `fw::emit` "tap" detail on/off, sound `toggle-on` / `toggle-off`; disabled (`USER_1`, not in the docs toggle, Button rule) = hollow track + text-mute knob (off) / accent-dim track + text-dim knob (on), tap = "blocked" + `error`. No pressed look, no visible label (docs); labelled row = List row later. Pairs: colour.json section `toggle`. |
| `slider` | Facet slider (docs `slider()`, `.f-slider`): horizontal, height `touch::min` (48 / 32 = docs slider-h), track `sp::s1` 4 px round ends `line-hi`, fill `blue`, knob = height / 2 (24 / 16 = docs slider-knob) `accent`, centred on the value; box = track + half a knob each side (track inset knob / 2 = 12 / 8) so the knob never clips at min / max. Props value / min / max / step / width (dp) / state; 6 variants (0, 40, 100, step 10, disabled x 2). Press on the knob = grab; elsewhere = tap-to-jump (value set at once, knob sweeps with Motion `value`, moving stops the sweep). Sound: `fw::step_ticks` (see "Events and sound"): every step the shown value crosses (drag, sweep, jump) counts, ticks at most every `tick-min`, ends = `slider-landmark`. `change` (value) on release; disabled (`USER_1`: track line, fill text-mute, knob text-dim) = `blocked` + `error` once per press. Pairs: colour.json section `slider`. |

## Add a component

Create `components/<id>.cpp`:

```cpp
#include "fw.h"
FW_COMPONENT(button, "Button", "components", R"({
  "frame": [240, 120],
  "props": [{"name": "label", "type": "text", "default": "OK"},
            {"name": "state", "type": "select", "options": ["", "pressed", "disabled"], "default": ""}],
  "variants": [{"label": "default", "props": {}}, {"label": "pressed", "props": {"state": "pressed"}}]
})") {
  lv_obj_t *b = lv_obj_create(parent);   // parent = the stage (flex, centred, bg = fw::T->bg)
  ...                                    // props in p: p["label"] | "OK"
  fw::force_state(b, p);
}
```

Meta: `frame` = stage size in dp; prop types `text`, `select` (`options`), `range` (`min/max/step`), `bool`;
`variants` = props over the defaults; `"demo": true` tags it. Section = `foundations`, `components` or `screens`.
Save: the watcher builds and the nav shows it (as WIP). Delete a component = delete its file.
