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

## Feature flags (what ships)

`renderer/features.json` is the one manifest of what a release carries. A **feature** = pre-made screens plus the UI
only they use (their `*_view`, `f1_*`, ...), or a group of unfinished parts / foundations; per feature three flags:

| Flag | What it decides |
|---|---|
| `docs` | shown on the docs site (pages, nav, component listings, search, roadmap rows, Updates entries tagged `feature:`) |
| `status` | the label the docs show while it is shown: `done` (git's WIP / Done / edited), `wip`, `preview` |
| `firmware` | compiled into the panel firmware, and so in the factory deck, Home cards and Settings > Screens |

The firmware flag is a **screen** decision: every component or foundation not listed under a feature belongs to the
**library** (`"library"`, shipped), because people and custom specs build with it; list a component under a feature only
when it is that screen's own UI or unfinished. **Flip one:** edit its `"docs"` / `"status"` / `"firmware"` in
`renderer/features.json`; the next build checks it. Workbench: Tools > **Release** (hub `/wasm/release`) lists them.

Build-time only (nothing on the device): `tools/features.mjs` reads the manifest.
- `build.sh` runs `node tools/features.mjs check` first and fails when a source belongs to no feature (or two), a
  listed id does not exist, or a feature shown in docs / firmware needs a part of a hidden one (`find("x")` calls, spec
  nodes, `nav.open` screens, `#include` of a feature's core file). Writes `features.json` (id -> feature, flags) next
  to the build for the workbench nav ("off" tag).
- `FW_FEATURES=docs|firmware ./build.sh` = that flavour: hidden sources not compiled, their specs, deck pages and Home
  cards (`cards` = the card types that open them) left out of `specs_gen.h`, their Colour page sections out
  (`colour` aliases), `FW_ICONS=used` trims by the components left; into `$FW_OUT/<flavour>/<density>/` with its own
  cache, no audit (the full build has it). Unset = everything (the workbench).
- `firmware/bundle.mjs` = the release (firmware flags) by default; `--all` bundles every feature (a Show on device
  build of work in progress).
- `site/` builds from the docs flavour (`FW_FEATURES=docs renderer/build.sh` first, then `npm run build`).

`node tools/features.mjs table` prints the flags; `exclude docs|firmware` the files a flavour leaves out.

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

**Component tokens:** `components.<component>.<key>` = `{value, options, use}` (style choices a theme may set later, board facet-130; first: `card.press`) -> `fw::tokens::comp::<component>_<key>` (string, or float for numbers; the generator checks a string is one of its options).

**Mix tokens:** `mixes.<name>` = `{tint, over, opa: "<component>.<key>"[, toward, desat: "<component>.<key>"]}` = `lv_color_mix(tint, over, opa x 255)`, the tint first moved towards `toward` by `desat` when given, per theme x accent set (`fw::tokens::MIXES` + `fw::tokens::mix(m, theme)`); contrast pairs may name a mix as fg / bg (`tools/contrast.mjs` mixes exactly like LVGL, the Colour page too). First: `press-tint`.

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
Since the Layout foundation, inset, gutter, group, row and top live in `tokens/layout.json` (same `fw::lay::` names);
space.json's layout group keeps the component sizes (card, tile, list row, dial) and the fill check reads both files.

## Layout tokens and devices (single source, WIP)

`tokens/layout.json` = general screen geometry + the targets, from Facet `main` tokens.json `layout`, `devices`,
`sizeClass`, `density` (1x = the docs `density.1x` table) and the docs concepts/screens.md. Screen-specific values (Media
`m-*` / `mv-*`, GitHub `pr-*`, Clock `clock-*`) arrive with their screens; component sizes stay in space.json.
- **Groups** (device px, `v` = 720, `1x` = 480): screen (`inset` 32/20, `gutter` 16/16, `group` 48/32, `top` 112/72),
  header (`row` 48/32, `back-gap` 16/12, `header-time-w` 72/48 = docs `clock-w`, `chrome-btn` 48/32 = the settings button, must equal `row`), rows (`section-gap` 16/12, `more-row-h`
  64/44), overlay (`sheet-top` 96/64, `toast-top` 96/64, `toast-h` 120/80, `toast-pad` 24/16).
- **Toast behaviour** (top-level `toast` group = docs tokens.json `toast`, WIP 2026-10-22): `position` top, `max` 2,
  `life` / `life-urgent` = motion.json durations `toast-life` 6000 / `toast-life-urgent` 12000 (referenced, not copied),
  `swipe` 64/44, `swipe-out` 160/108 (docs toastLayer's sideways leave), `tap-slop` 8/8, `sound` = sound.json `notify`.
  Here and not in motion.json because it is where an overlay sits and how it behaves on the screen, beside its geometry;
  gen_layout checks the motion / sound references and the grid -> `fw::toast::top|max|life|life_urgent|sound|swipe|
  swipe_out|tap_slop`.
- **Devices:** Waveshare P4-86 720 (1.5x, reference), Guition 4848S040 480 (1x, rotated 90 deg in software for the mount),
  Shelly X1i 720 (1.5x WASM build in an Android WebView), planned 480x800 tall, 1024x600 wide, 1280x800 large.
  **Size classes** by logical size (px / density): square 480x480 dp (Home 2 cols), tall 480x720, wide 960x560 (4 cols),
  large 1280x760. **Density:** 1.5x = 1.5 px/dp, 8 px grid, the reference; 1x = 1 px/dp, 4 px grid, own snapped table
  + optical pass (thin weights one step heavier, small text one step up).
- `tools/gen_layout.mjs` (build.sh + firmware/bundle.mjs) -> `layout_gen.h` (included by `fw.h`): `fw::lay::inset|top|
  row|back_gap|header_time_w|section_gap|more_row_h|sheet_top|toast_*` + derived `screen_w|screen_h|content_w|content_h|
  header_gap|chrome_w` (656x576 / 440x388, gap 32/20, chrome 136/96), and `fw::lyt::` tables (TOKENS, DERIVED, DEVICES, CLASSES, DENSITIES).
  Fails the build on: a value off the 4 px grid or below 4, a name also in space.json's layout group, a frame that does
  not add up (header gap, content size, header items, toast past the bottom inset), a device whose dp size lands in
  another size class or is not whole dp.
- **Screen chrome rule (Darren 2026-10-22, WIP):** the time (`header-time-w`) ends the header row on the content edge on
  every screen; on Home (only, for now; may move later) the settings button (`chrome-btn`) sits a gutter before it, =
  derived `chrome_w`.
  It is the `screen_chrome` component and the Header ends with it, so a screen gets it by placing its Header at
  (inset, inset) and never places it itself. Rule text = layout.json `rules.sections` `chrome`; Layout page section
  `chrome` = the real Header on two screen tops with the corner outlined: Home (button + time) and Lights (time alone).
- Page `/wasm/foundations/layout` (`foundations/layout.cpp`, id `layout`): Screen overlay (this build's screen at true
  size: grid minor + major every `group`, inset bands, header zone, content area, Home columns; five layer chips under
  the screen toggle each layer, tap = toggle sound + `layer` event), Header zone, Groups and rows (a sample screen that
  fills the content height exactly at both densities), Overlays (sheet + toast top/bottom), Devices and density (to
  scale in dp, this build marked), Size classes (sketches at 1/4 with Home columns), Token table (+ derived + grid).
  Round 2 (Darren): every example is built from the real approved components through the registry (`fw::find(id)->build`,
  as List row builds Toggle; no new core entry point needed): header = Button (ghost, back icon, the screen label),
  Home = 4 Cards, settings = List rows (value + chevron, toggle, value, chevron; rows touch = dividers); parts that are
  not components yet (section header, more row, sheet, toast) = a plain outline box "<part>: component coming". Token
  overlays (inset, header, content, gutter, group / section gaps) are translucent marks drawn on top, measured from the
  real components' boxes.

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

Page `/wasm/foundations/motion` (`foundations/motion.cpp`): easing curves plotted from the very LVGL path callbacks (stroke atoms:
curve + linear diagonal, facet-145) with a dot along the curve + a box moving with it (1 s, tap = replay), durations as bars (log scale for ambient; tap = a run
at the real length), every pattern live on a sample (the real Button for press / release; toggle knob, fading card,
sheet, toast, screen rise, count-up, clock hand (a stroke atom re-rasterised per frame, only around the old + new hand), pulse), a reduced-motion table. Playground: `reduced_motion`, `loop` =
one pattern at full screen size over and over (the frame-rate bench). Contrast pairs: `colour.json` section `motion`.

**Frame-rate check** on the panels: `POST /fw/show {"id": "motion", "props": {"loop": "<pattern>"}}`, then
`GET /fw/perf?reset=1`, wait, `GET /fw/perf` (`firmware/fw_device.h`; browser targets: the host's device page reports
the same stats, e.g. the hub's `GET /api/wasm/perf?target=shelly[&reset=1]`, WASM export `fw_anims()`): frames counted only while an LVGL animation
runs; `fps`, `frame_ms_avg|max` (start to start), `render_ms_avg|max` (render + flush), `slow` = frames later than 1.5
refresh periods (a missed slot), `refr_period_ms` (LV_DEF_REFR_PERIOD).

**Bench (facet-153, WIP):** every component and screen measured headless against per-device budgets, with the panels'
numbers estimated from these `/fw/perf` results: `heavy node renderer/tools/bench.mjs`, details and the firmware bench
spec in `renderer/bench/README.md`; host page `/wasm/bench`.

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

### Bottom sheet frame cost (2026-10-22, browser only; panels pending a flash)

Workbench device page (`/wasm/device?target=<id with no stored show>` for 720, `guition` for 480), `bottom_sheet`
`{"loop": true}` (open + close forever), headless Chromium with 8x CPU throttling, own frame collector (WASM tick +
render + canvas copy per frame while an animation runs). Work ms per frame, 2 runs each:

| | 720 | 480 |
|---|---|---|
| idle-ish baseline (motion `pulse`) | 3.7 | 1.7 |
| the old Motion page sheet sample (the one benched on the panels above) | 21.7-22.0 | 9.7-9.9 |
| Bottom sheet (real screen + 5 picker rows) | 27.2 | 13.0-13.3 |
| + `panel: solid` (opaque panel, rise only) | 24.1 | 11.2 |
| + `flatten` (screen under it = one lv_snapshot image) | no gain (27.6) | no gain (13.5) |
| panel as a moving snapshot image (ARGB) | worse (34.1) | worse (14.8) |

Over the baseline the real sheet costs ~1.3x (720) / ~1.4x (480) the old sample, so scaled from the panel numbers above
the estimate is Guition ~125 ms per frame (~8 fps; 160 ms open = one or two in-between frames), Shelly ~55 ms (~16 fps),
Waveshare ~30 ms (~28 fps). The cost is the area blended at partial opacity each frame (full-screen scrim + a panel that
fades over most of the screen), not the number of objects. Levers left (Darren's call, "optimise later"): opaque panel
that only rises (-15-18 %), a stepped scrim (2-3 levels: fewer full-screen frames), or `instant` sheets on 1x boards.

### Toast frame cost (2026-10-22, browser only; panels pending a flash)

Same bench as the bottom sheet (workbench device page, headless Chromium, 8x CPU throttle, work ms per frame while an
animation runs, 2 runs each; the sheet re-measured as the yardstick):

| | 720 | 480 |
|---|---|---|
| Bottom sheet loop (yardstick) | 27.0-27.7 | 13.3-13.7 |
| one toast sitting (timer bar only) | 4.0 | 1.7-1.8 |
| two toasts sitting (two bars) | 4.9-5.3 | 2.1 |
| arrivals loop (a toast every 280 ms: in + restack + the oldest out) | 12.5-12.9 | 7.1 |

The bar is the only steady cost and it is close to the idle baseline (3.7 / 1.7): a stroke::edge strip per frame. The
in / out motion costs ~0.4x (720) / ~0.5x (480) of the sheet: the fading toast is drawn through a layer the size of
the toast, nothing full-screen. Scaled from the sheet's panel estimate: Guition ~65 ms per frame while a toast moves
(~15 fps: 2-3 frames in 160 ms), Shelly ~25 ms (~40 fps), Waveshare ~12 ms (60); a sitting toast's bar ~ the idle
redraw on each. Levers if the Guition needs them: a stepped bar (redraw every 4 px instead of every px), or `instant`
toasts on 1x boards.

## Chart tokens (single source, WIP)

`renderer/tokens/chart.json` (docs `@facet/ui` charts.ts + 30-charts.css, snapped to the 4 px grid) ->
`tools/gen_chart.mjs` -> `chart_gen.h` (`fw::ch::`): series colours `ch::series(t, i)` (blue / steel / text-dim, plus
the dashed compare line-hi), roles `ch::ink::highlight|select|cursor|grid|baseline|axis|unavailable|loading(t)` and
`ch::ink_of(t, i)`, the area mix (12 % of series 1 over surface / bg: mixes `chart-area(-bg)`), geometry per density
(`ch::chart_h`, `line`, `dot`, `bar_gap`, ...; the generator checks the grid, strokes 1-2 or even), chart motion
`ch::MOTION[ch::M_draw]` (enum `M_`-prefixed: `ch::gauge` / `ch::ping` are sizes) = motion.json durations + easings by
name, with a reduced mode and a **lite** mode each (`keep | dot | instant`). Plain-word fields (`roles.*.about`,
`motion.*.cost`, `lite.say`, `area.about`) are what the pages print: no token names on the page. Contrast pairs:
colour.json section `chart` (0 fails).
Page `/wasm/foundations/chart` (`foundations/chart.cpp`): series colours on a card and the canvas in both themes with
their 16-bit contrast, the other chart colours, line weights, area + grid + dotted now, sparkline, bars (rounded data
end, square foot, highlight, outline compare) + gauge (stroke atom arc), big number / axis / grid values / legend /
scrub readout, and no data / unavailable / loading (Pulse) / gaps. Scrub = press and drag (PRESS_LOCK): cursor, dot
with a surface ring and a readout follow point by point with `fw::step_ticks`; release = event `scrub` `"<i> <value>"`.
How it draws: the series line and the area are A8 masks (stroke atom `stroke_segments` + an area raster here) shown as
recoloured `lv_image`s (lv_line steps visibly through float points); grid, bars, dots, cursor and labels are plain
objects. Bench props (no page control; `/fw/show` or the device page): `loop: draw|shift|morph|bars|gauge|ping` = one
chart animating forever (for `GET /fw/perf`), `lite: auto|on|off` (auto = on in the 1x build).
Motion page section **Charts** (`fw::chart::motion_section`, motion.cpp bench prop `chart_lite`): one entry per chart
motion (duration + easing, what moves, reduced + 480 behaviour, how heavy) with a tap-to-play sample (scrub = drag).

Frame cost, estimated from the Motion / sheet bench numbers above by the area each frame repaints (not yet measured on
the panels: `loop` + `/fw/perf`):

| motion | how | 720 est. | 480 est. | 480 lite |
|---|---|---|---|---|
| draw | clip grows, only the new strip invalidated | 50+ fps | 45+ | same |
| shift | track translates one step, one raster per value | 45+ | 25-30 | line jumps, dot glides: ~50 |
| morph | re-raster line (+ area) every frame | 30-40 | 12-18 | jumps |
| bars | rect heights, the bars' area only | 50 | 35-45 | same |
| gauge | arc atom re-raster (= the Dial sweep) | 50 | 35-40 | same |
| ping / scrub | one small ring / three small objects | free | free | same |

## Mascot tokens (single source, WIP)

`renderer/tokens/mascot.json` (docs `@facet/ui` mascot.ts + 20-mascot.css, cross-checked with the approved panel firmware
`mascot.h`) -> `tools/gen_mascot.mjs` -> `mascot_gen.h` (`fw::mas::`: `G` = this density's geometry, `GEO_V` / `GEO_1X`,
`MOODS`, `PATTERNS`, `INSTANCE_OFFSET_MS`) -> runtime `core/mascot.h/.cpp`: `fw::mascot(parent, mas::needs, {colour,
delay, scale, only})`, `fw::mascot_set()`, `fw::mascot_replay()`. The mascot follows the foundations (Darren
2026-10-22): body box + everything outside it on the 4 px grid (generator checks `grid.checked`), corners = radius
tokens, line widths = stroke tokens (refs like `"stroke.edge"` resolved per density from space.json); eyes and their
1-2 px motions stay whole-px artwork (docs spacing); right eye at `rx`. `grid.changes` = old -> new list (page Tokens). Moods idle / working (reading) / done / needs / error /
sleeping, colour = a colour token per mood (pairs section `mascot`). Motion = keyframe tracks `[t permille, value]` on
x / y / height / opacity / close (eye blink) / scale (z points only), eased per segment with a motion.json easing, one
lv_anim per pattern, values written only when the whole px changes; desynced loops shift by n x `instance-offset`;
reduced motion = loops on their `rest` frame, hop skipped. No body rotation (smears, layer redraw; like the firmware);
happy arches, brows and z lines = stroke atoms (`fw::stroke_arc` / `stroke_at` / `stroke_path`, facet-145: no lv_arc / lv_line, no brow rotation); a z is one small mask re-rasterised when its scale changes, moved and faded (plain opa) as one image. Page `/wasm/foundations/mascots` (moods, motion,
reduced, desync, tokens, usage); bench props `bench: agents | big` for `/fw/perf` (Guition 2026-10-22: agents 3 x 3 = 61.8 fps, render 5.6 ms avg / 13.5 max; big = six at 4x 60 fps, render 9.5 / 23.4; reduced motion = 0 frames).

## State tokens (single source, WIP)

`tokens/states.json` = the interaction state rules every component reads (board facet-137, States round 1): it names
tokens only (no hex, no amounts) so a theme can change the rules later (facet-130). Per state (rest + tap, pressed,
disabled, unavailable, on, focus): label + about, Motion patterns, event, sound, colour roles (`token` = a colour token
or `mix` = a colour.json mix); plus the Card press style (`pressed.style`, moved here from colour.json
`components.card.press`; the tint amounts `card.press-tint` / `press-desat` stay in colour.json where the mix + contrast
test read them), the unavailable words, the focus ring (`stroke.focus` + `touch.focus-offset` from space.json), the
rules in plain words, the round-1 audit and the open questions.
- `tools/gen_states.mjs` (build.sh + firmware/bundle.mjs) -> `states_gen.h` (included at the end of `fw.h`):
  `fw::st::OFF` (= `LV_STATE_USER_1`, disabled AND unavailable), `ON` (CHECKED), `PRESSED`; `PRESS_IN/OUT` (press /
  release), `SWITCH` (toggle), `SWEEP` (value); a colour role function per state x role, e.g. `st::pressed_surface(t)`,
  `pressed_tint(t)` (mix press-tint), `disabled_ink(t)` (text-mute), `disabled_on_fill(t)`, `disabled_knob(t)`,
  `disabled_accent(t)`, `disabled_line(t)`, `unavailable_note(t)`, `on_fill/label/ink/line/mark(t)`, `focus_ring(t)`;
  `PRESS_STYLE`, `BLOCKED_EVENT/SOUND`, `SWITCH_EVENT`, `ON_SOUND/OFF_SOUND`, `UNAVAILABLE_TEXT`, `PLACEHOLDER`,
  `FOCUS_W/OFFSET`; helpers `st::blocked(obj, detail)` (event + error), `st::press_transitions(d, props)`;
  and the page tables (the ring itself = core atom `fw::focus_ring`, see Components). Fails the build on any unknown colour / mix / pattern / sound /
  space reference, a wrong LVGL state bit or a press style not in its options.
- Components take every state choice from `fw::st::` (never pick a colour token for a state themselves): Button, Icon
  button, Toggle, Slider, Dial, Card, List row, Section header, Label (`disabled`). The refactor was pixel-identical:
  2560 frames (every component variant x 720 / 480 x dark / light x 5 accent sets) hashed before and after, 0 differ.
- **Round 2 decisions (Darren 2026-10-21, all 9 picks, "I think your suggestions are great here"):** pressed keeps three
  looks (grey on boxed controls, accent-press on accent fills, the tint on big areas); a blocked part answers on finger
  DOWN everywhere (`states.json disabled.moment: press` -> `st::BLOCKED_ON_PRESS`; tap handlers are registered with
  `st::on_tap(obj, cb, user)` and start with `if (!st::tap_now(e)) return;`); every switch emits `toggle` (Toggle too;
  the workbench tap counter counts tap + toggle + back); Toggle and Slider got `unavailable` (the word in place of the
  control, same box), Icon button `unavailable` = the disabled look (no room for the word); every disabled knob is
  text-dim; square Icon button active = line-hi border like round; idle chip / badge tint = text-dim; the mix
  card-press is now `press-tint`; the focus ring (now `fw::focus_ring`) hides the ring under OFF (disabled wins over active).
- **Bleed (core `fw::bleed(obj, px)`):** marks drawn outside a box (the focus ring at the focus offset, the Section header
  action's overhanging press fill) were clipped by the component or the page's sample boxes. `bleed` sets
  `LV_OBJ_FLAG_OVERFLOW_VISIBLE` AND an ext draw size of at least px on the object and every ancestor up to the screen
  (OVERFLOW_VISIBLE alone draws 0 px: LVGL clips children to the parent's box grown by the parent's ext draw size).
  `fw::focus_ring` calls it; Section header calls it for the action. Call it once the object is in its final parent.
- Watcher race (found here): a source saved WHILE a build compiles it can end up older than its stale object, so the
  next build skips it. If a change "does nothing", `touch` the file once the watcher is idle.
- Page `/wasm/foundations/states` (`foundations/states.cpp`, id `states`, meta `"events": true` = the Events panel on
  a doc page): Rules, Pressed, Disabled, Unavailable, On and active, Focus ring, Rest and tap (spec line + colour roles
  with this theme's colours + live real components via `fw::find`), Audit, Open questions. Page text boxes are rounded
  up to the 4 px grid (the 720 type boxes are 1.5x docs values).

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

### Turn icons (WIP 2026-10-23)

An icon with `"turn": true` in `icons.json` (so far `plane`, Facet main's `planeGlyph`: a plane from above, nose up,
filled) also ships its outline: `gen_icons.mjs` reads the master's one filled path (M / L / H / V / C / Z, a cubic = 8
segments) into `ic::TURNS` / `ic::TURN_PTS` (meta header, 24 grid). `fw::icon_turned(parent, "plane", px, deg, colour)`
fills it at any heading on the device (clockwise about the box centre = the original's `rotate()` with origin 50 % 50 %,
exact span coverage on 16 sub-rows per px row, so the edges match an SVG fill); `fw::icon_turned_mask` gives the bare A8
mask for a component's own draw event (Overhead's sky radar), freed with `fw::icon_mask_free`. No turned bitmap is
resampled. The static masks at its `sizes` stay (heading 0, the Icons grid); the Icons page adds a "(heading)" tile at
eight headings. Checked against resvg renders of the original path at 32 px: per-pixel coverage within 0.11, mean 0.003.

### Animated weather (WIP 2026-10-22)

Facet `main` animates the weather icons on the Home card (72) and the Weather hero (144) (`weatherIcon(id, { animated })`
+ `weather.css`). Here: `icons.json` `animated.sizes` (feature / card / hero) -> `gen_icons.mjs` cuts each weather master
into its drawn parts (the rays group, the sun disc, each cloud, each drop / flake / hailstone / star / fog or wind line,
the bolt) and rasterises every part ALONE at those sizes (`icd::APARTS`, one A8 mask per colour; a cloud's bg fill =
an `occluder` mask drawn in the colour behind the icon, so a drifting cloud still hides the sun). Flash: +293 KB at
720, +137 KB at 480 (all 17 icons x 3 sizes). Motion = `motion.json` `weather` (keyframes per part = `weather.css`
1:1, durations `wx-*` in the ambient group, pattern `weather` on the Motion page, reduced = still) -> `motion_gen.h`
`fw::mo::wx::`. Runtime `fw::weather_icon(parent, name, size, animate, ink, behind)` / `weather_icon_set` (in place):
one lv_anim clock per icon; each part is written only when its whole-px offset, opacity step (`opa-step` 16) or ray
step changes. **No runtime rotation or scaling of masks:** the sun's rays are drawn on the device from their
geometry (`icd::RAYS`: round-capped strokes, analytic AA) at each step that moves a ray tip `rot-px` (1 px); the first
build rotated the A8 rays mask with `lv_image_set_rotation` and the sun looked wrong (soft resampled rays, centre off
by the half-pixel pivot rounding at 72 px). Static, reduced motion or a size without parts = exactly the static master
(`fw::icon`). Component `weather_icon` (props icon / size / animated / on canvas|card; update path: icon + animated in
place). Icons page section "Animated weather" (real Segmented Animated / Static, real Weather icons; the choice is
not demo-reset).

Frame cost (node, unthrottled WASM, ms per 16 ms frame; panel estimate = scaled by the Bottom sheet loop measured on
both, Guition ~60x, Waveshare ~8x):

| Icon (hero) | 480 ms / repaint % | Guition est. | 720 ms / repaint % | Waveshare est. |
|---|---|---|---|---|
| clear-day (rays, ~9 steps/s) | 0.008 / 15 % | ~3 ms per step | 0.021 / 23 % | < 1 ms |
| partly-day | 0.005 / 11 % | ~3 ms per step | 0.011 / 18 % | < 1 ms |
| rain-heavy (5 drops, every frame) | 0.040 / 100 % | ~2.5 ms / frame | 0.050 / 100 % | ~0.4 ms |
| rain-light / hail / thunder-rain | 0.02 / ~98 % | ~1.2 ms / frame | 0.024 / ~98 % | ~0.2 ms |
| cloudy / fog / wind | 0.002-0.005 / 3-17 % | < 0.5 ms | 0.003-0.005 | - |
| Bottom sheet loop (yardstick) | 0.83 / 56 % | 89-125 ms (measured) | 1.64 / 56 % | 23-30 ms |

**Fix 2026-10-22 (laggy Icons page):** the parts were child images, each move invalidating its old + new area; a page
of 20 animated icons made 30+ areas a frame, overflowed LVGL's 32 invalid areas (`LV_INV_BUF_SIZE`) and redrew the
whole 720 x 1079 stage on 25 % of frames, and the workbench copied the whole stage (3 MB) to the canvas every frame.
Now the icon box draws its parts itself (`LV_EVENT_DRAW_MAIN`, `lv_draw_image` with the same recolour) and
invalidates ONE area per icon per frame (the union of what changed); the WASM stage reports its flushed areas
(`fw_areas`) and both engines (hub workbench, docs site) copy only those. Animated section, node: tick 1.10 -> 0.21 ms
(720), 0.29 -> 0.15 ms (480), full-stage redraws 63 / 250 -> 0; Chromium at 4x CPU throttle 34 -> 56 fps (480: 59).
Single hero rain-heavy 0.050 -> 0.026 ms (720), 0.040 -> 0.018 (480): Guition est. ~1.2 ms per frame; a page of
20 animated icons ~9 ms per frame.

So an animated weather icon costs a few percent of a sheet frame; rain / hail / snow repaint their small drops every
frame, the rest a few times a second. Not yet measured on the panels (no flash for this).

### Moon phase (WIP 2026-10-22)

Facet `main` `moonIcon(frac)`: on the 48 grid a disc r 20 (`moon-dark`), the lit part = the lit limb + the terminator
(half an ellipse, rx = r x |2k - 1|, k = lit fraction) in `moon-lit`, the outline in `moon-edge` at the weather stroke
(2 px, 3 from 96). `fw::moon_icon(parent, frac, size, south)` computes the three A8 masks at the exact px on the device
(8 sub-rows per pixel row, exact spans: anti-aliased like the rasterised icons; disc + outline once per size, the lit
part per phase); `moon_icon_set` redraws only the lit mask in place. Any size token, any phase 0..1, south mirrors.
Cost: a new phase at hero 0.16 ms (480) / 0.31 ms (720) in node, est. ~9 ms on the Guition: no stored phase frames
needed. Component `moon_icon` (props phase / size / south; update path in place). Icons page: section "Moon phase"
(real Slider 0-100: the moons follow the drag live through the Slider's `input` notify, `fw::listen`) and a moon tile
in the Weather group.

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
its content, `fw_content_h` + `fw_stage_resize`; it measures the children top to bottom, since the screen centres them
and content taller than the first 64 px stage sticks out at both ends (fixed 2026-10-22: it used the bottom only and
cut a 96 px List row to 80), plus the screen padding or a bled mark (fw::bleed ext draw, deepest holder, labels skipped)
where that reaches further). Status = the .cpp plus `tokens/<id>.json` if present (any untracked
= WIP; approve = commit both; Motion also adds `core/motion.cpp` + `tools/gen_motion.mjs`). **Motion**
(`foundations/motion.cpp`): see "Motion tokens". **Type** (`foundations/type.cpp`): both scales at true size in their line boxes (hairlines) with spec + use, mono caps beside sans, number styles (units, tabular digits), glyph coverage per set, the fonts of the build with their flash cost (device switch = 720 / 480 build). **Layout & devices** (`foundations/layout.cpp`): see "Layout tokens and devices". **Spacing & radius** (`foundations/space.cpp`, id `space`): spacing steps (true-size square + 8x bar), layout tokens (true-size bars + grid), radius samples on surface cards, strokes (line, ring, outlined control), touch targets (hit area tinted around a smaller drawn back button / chip / row); every value also shows the other density (the whole screen moved to Layout & devices, round 3). **Colour** (`foundations/colour.cpp`): every token by group, name + role, the colour in
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
| `firmware/fw_device.h` | ESPHome glue: own LVGL screen, `GET /fw/info` (build hashes), `GET /fw/meta`, `POST /fw/show {id, props, theme, accent}`, `GET /fw/perf[?reset=1]` (frame stats while animations run); last request kept in flash; tall content scrolls; **probation chip**: while the running OTA image is unconfirmed (`ESP_OTA_IMG_PENDING_VERIFY`, before safe_mode's `boot_is_good_after`) a warn Status chip "Update settling: don't unplug m:ss" on the LVGL top layer (over every screen, follows the shown theme/accent), then "Update confirmed" 4 s; `/fw/info` `probation_s` (-1 = confirmed) |
| `firmware/esphome_init.py` | the external component `facet_wb` (`board`, `width`, `height`, `probation` = safe_mode `boot_is_good_after`, default 180 s) |
| `firmware/bundle.mjs` | `--density 1x\|1.5x --out <dir>` writes `<dir>/facet_wb/` (flat, fonts inlined, link keepers); `--hashes` prints the source hashes |

**Rebuild rule:** props, theme, accent and variant are runtime data (no rebuild). New or edited component/foundation
code, core, tokens or fonts need a firmware build: the device reports the hashes it was built from (`/fw/info`), the
host compares them with `firmware/bundle.mjs --hashes` (keys relative to `renderer/`) and shows "Needs firmware build" instead of sending stale code.
It also shows after an OTA rollback: a hard reset / power cut inside the 3-minute probation boots the previous image (2026-10-22).
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

Pitch follows the value (Darren 2026-10-21): every tick and landmark is requested with `fw::sound(obj, id, pos)`, pos =
the step's place in the range (step / n, 0..1), sent as the sound event's `detail` ("0.420"; "" = unpitched). The
player shifts the pitch by (pos - 0.5) x the pack's `track` semitones (sound.json: slider-tick and slider-landmark
12 in keycap / halo / arcade, 7 in graphite / crystal / felt; 0 = not pitched), so 0 % ticks lower and 100 % higher.
Browser: hub `src/wasm/sound.ts playUi(id, detail)` (playbackRate). Panels: same event shape, playback = facet-120.

Motion: components take every duration / easing from the Motion tokens (see "Motion tokens"); Button = patterns
`press` + `release` as style transitions.

## Update path

Every component takes new props IN PLACE and moves to them with its own animation, no rebuild (Darren 2026-10-22: for
everything, from the docs, the workbench and code). A new component isn't done without it.

| Where | Call |
|---|---|
| component | `FW_UPDATE(id) { ... }` after its `FW_COMPONENT` block: `bool (lv_obj_t *root, fw::Props p)`, root = the one object `build()` made, p = the FULL new props; `false` = can't (structural prop: range, state, ...) |
| C++ (device / HA side) | `fw::update(scr, comp, props_json, light, accent)` on a screen `fw::render()` built -> `""` applied / `"rebuild"` (call `render`) / error. Different component, theme or accent, no update path, or more than one root object = `"rebuild"` |
| a component holding another | `fw::update_child(fw::find("button"), root, props)` |
| WASM | `fw_update(stage, comp, props, light, accent)`; JS `Stage.update()` / `apply()` (update, else render) in the hub workbench engine and `site/src/lib/engine.ts` |

**Silent:** updates are automated (nobody touched the panel), so no sound and no events. `fw::update` runs the
component inside `fw::Quiet` (core drops `emit` / `sound` / `step_ticks` while it lives); animations an update starts
run later, so the component keeps them silent itself (the Dial's quiet sweep). While a finger holds a control the
user wins: the update waits for the next one.

Hosts: the workbench playground (control changes; "Replay opening" stays a fresh build) and the demo reset (variants,
doc sections, docs site samples: back to the starting props) call update first and rebuild only on `"rebuild"`.
Theme / accent changes stay a rebuild.

**Every component has one (phase 2, 2026-10-22): no component returns "rebuild" any more.** The shared pieces:

| Piece | What |
|---|---|
| `fw::refill(root)` | a structural prop (another kind, icon, layout, a part that comes or goes): the contents are made again inside the SAME root (children, local styles, running anims, state-style states go; user data, flags, event callbacks stay), the component's `fill()` styles it again. Snaps. |
| `fw::shape(root, sig, key = "")` | the structural props as one string on the root: true = unchanged (move in place). `key`: a component built on another's root (Picker row on the List row) keeps its own |
| `fw::st::set_state` / `set_off` | states (disabled, active, chosen) as LVGL state styles, faded in place (Motion fade) |
| `fw::st::swap` / `fade_in_children` | a part traded in place (Slider track <-> the unavailable word); contents made again for a new state fade in |
| `fw::st::built` / `fw::st::update(root, p, fill)` | the common pressable case: pressed <-> default = the press fade, anything else = refill (+ fade when disabled / unavailable changes) |
| single root | multi-object components got one: Corner ticks, Focus ring, Accent swatches = a content-sized wrapper; Count badge hidden at zero-hide; Toggle / Slider / Segmented unavailable = the same root; Bottom sheet + Toast layer = their screen box (or the overlay itself) |
| quiet | `fw::Quiet` covers the synchronous part; animations an update starts are quiet themselves (Dial / Slider sweeps: no step ticks; Toast layer pushes: no arrival event / sound) |
| `fw::bleed` | idempotent now (one callback per object, the largest px), so refills never pile callbacks on the ancestors |

Per component, what moves in place (animated) vs what snaps in place (refill):

| Component | Animates | Snaps (refill) |
|---|---|---|
| Dial | value, on / off (sweeps) | range, step, caption / unit text; state (contents fade in) |
| Slider | value (sweep), disabled, unavailable (track <-> word) | range, step, width |
| Toggle | on (knob slide), disabled | unavailable in / out (fades in) |
| Radio | on, disabled | - |
| Button / Icon button / Back button | state (pressed, active, disabled, unavailable), label text | kind, icon, width, style |
| Segmented / Accent swatches | the choice, disabled | options, width, context; unavailable (fades in) |
| Card / Tile / More row / Speaker button / Section header | pressed | everything else; unavailable / disabled (fades in) |
| List row | pressed, a toggle row's on (the Toggle's update) | everything else (fades in on disabled / unavailable) |
| Picker row | selected (the Radio's update), pressed | name, icon, status, tone, state, group |
| Header | label + time text | back / clock / chip |
| Bottom sheet | open / closed (sheet-open / sheet-close), selected (radios + screen row values) | list, title, items (rows again); screen / bench props |
| Toast | a dismissed toast comes back (toast-in) | contents (the timer keeps its progress) |
| Toast layer | push / stack / timer / action: the showing toasts leave, the new ones arrive (silent) | position, screen |
| Label, Stat, Hero block, Status dot / chip, Count badge, Corner ticks, Focus ring | Corner ticks / Focus ring state | the rest |

Test (2026-10-22): every playground control value and every variant, both directions (start -> props and back),
both densities, light + dark: update settled == a fresh build pixel for pixel (0 differ, except the Tile's looping
mascot phase), no event or sound from an update, a tap after an update reports what a tap on a fresh build reports.

**Demo state** (Darren 2026-10-22): a demo whose replay ends where it started reports it, and the hosts skip the demo
reset (no reset step: the replay plays straight away). `fw::demo_away(obj, away)` -> WASM `fw_demo_state(stage)`
(-1 nothing reported = reset as before, 0 all at their preset = skip, 1 moved away = reset); JS `Stage.demoState()`
(hub workbench + `site/src/lib/engine.ts`). Charts (Motion page): Line draws in and the same bars growing again stay at
the preset; new readings, a morph, a new gauge value or another day of bars move away.

## ASan check

`renderer/tools/asan.sh [1x|1.5x] [id '<props>' ...]` compiles the same core, foundations, components and screens
natively with g++ AddressSanitizer + UBSan and drives them headless (`tools/asan_driver.cpp`): every item with its
defaults and every variant, both themes, 400 frames, a press / drag / release, two updates, then frees the stage. No
ids = everything. Exit 0 = no reports. It finds what WASM hides: an out-of-bounds write there silently corrupts the heap,
on the panels it crashes (the 2026-10-22 clock hand). Needs one `build.sh` first (generated headers); objects cached in
`$FW_CACHE/asan-<den>`; heavy (about 2 GB, a few minutes cold): run it through the box's job queue. Run it before a
firmware build and after core or component changes that touch memory (buffers, anims, user data).

## Audit

Every component and screen is checked by `renderer/tools/audit.mjs` (Darren 2026-10-22: "tests against them to make
sure they align with the pixel grids and there's nothing off about them ... don't clip out of their box ... animations
follow the right path"). `build.sh` runs it for each density after linking; errors fail the build (the new build is
still served). Run by hand: `node renderer/tools/audit.mjs [--den 1x|1.5x] [--comp id,id] [--fresh] [--json out.json]`.
Each component x every variant x dark / light, on its workbench frame (screens: the whole screen):

| Check | What fails |
|---|---|
| outside | ink outside the component's box (+ what it bleeds on purpose: `fw::bleed`, ext draw size) |
| stale | the frame after the animations differs from a full redraw at the same moment (an animation invalidated too little) |
| clip | the same render with every object allowed to draw outside its box (`fw_unclip`) differs: something was cut off (an ancestor, a custom draw's own box). Kept: scroll containers, `fw::qa::clips(obj, why)` |
| text | a label's glyphs below its box (LVGL cuts label text there), a CLIP label wider than its box |
| grid | a drawn box (fill / border) or arc whose place in its parent or size is off the 4 px grid. Allowed: centred in its parent, filling the inside of the parent's border, edge to edge with its own 1-2 px border pushed off both sides (the Bottom sheet's rounded top edge), a 1-2 px hairline on its thin axis, `fw::qa::free(obj, why)` (placed by value / angle; a note) |
| touch | an object taking touches under `touch::min` with its ext click area |
| radial | a ring's marks at 12 / 3 / 6 / 9 o'clock off their centre lines by more than 0.5 px (`fw::tick_ring` declares which axes carry a tick: `fw::qa::radial`) |
| update | a state / value prop (on, value, tone, selected, progress, ...) changed through the update path must animate (3+ distinct frames), silently; `"audit": {"snaps": {"prop": "why"}}`, test case `"audit": {"base": {...}}` (Darren 2026-10-22: updates animate like a tap; core `fw::st::restyle` / `refill_fade`) |
| motion | while it animates (2 s from the build): ink leaving the box, an object off its declared path (`fw::qa::circle(obj, ref, cx, cy, r)`, `fw::qa::inside(obj, ref)`) |

Marks (`core/audit.h`) are empty inlines on the panels. A component says only what the audit can't know; exceptions per
component: meta `"audit": {"skip": {"<check>": "why"}}`, or the same on one variant (printed as notes). Known: the vinyl
variants of Media visuals skip clip ("vinyl awaiting design round, Darren 2026-10-22"). Results are cached per component
(`$FW_CACHE/audit-<den>.json`, keyed on core + tokens + the tool + the component's file and the components it builds).
Rule: no component is shown to Darren until it passes.

## Components

| Id | What |
|---|---|
| `button` | Facet text button: kinds primary / secondary / outline / ghost, states default / pressed / active / disabled, optional icon (plus, check, close: `fw::icon(..., ic::icon, ...)` from the Icons foundation, ink recoloured per state; meta `"icons"` declares them for `FW_ICONS=used`), hug (label width on the grid) or fixed width. Disabled = `LV_STATE_USER_1` (LVGL sends no events to `LV_STATE_DISABLED`, and a disabled tap must play Error). Children follow the state via `LV_OBJ_FLAG_STATE_TRICKLE`. |
| `icon_button` | Facet icon button (docs `iconButton()`, `.f-ibtn`): style play = `lay::ibtn_play` (96 / 64) round accent fill, on-accent glyph `ic::feature` (48 / 32), always play / pause (only they ship at feature), active = pause + Button primary's focus ring (`stroke::focus`, `touch::focus_offset` = docs focus-offset-play), tap toggles: `toggle` play / pause + toggle-on / toggle-off; round = `lay::ibtn_round` (80 / 52), square = `lay::ibtn_square` (48 / 32 = touch min, `rad::square`): surface + 1 px line, text glyph `ic::glyph` (32 / 20; icon select = the icons the manifest ships at glyph: next, prev, play, plus, close, check, back), pressed surface-press + line-hi, active accent-text glyph (round also line-hi border), tap = `tap` (detail icon) + `tap`. Disabled (`USER_1`): play accent-dim + text-dim glyph (Button primary rule), others text-mute; `blocked` + error. Button's state model (CHECKED = active, Motion press / release transitions, STATE_TRICKLE); internal prop `role: back` (Back button) or `role: settings` (Screen chrome: `lay::chrome_btn` square, `screen-settings` at `ic::icon`, tap = `navigate` "settings" + open). **Multi-part icons** (a screen icon's accent part, weather colours) look exactly as on the Icons page in every state: only the ink follows rest / pressed / disabled, active keeps the resting ink (active = border only), and pressed uses the press tint (`st::pressed_tint`) so the accent part keeps 3:1. Ext click area if a box is ever under `touch::min`. 17 variants. Pairs: colour.json section `icon_button`. WIP round 1. |
| `back_button` | Facet back button (docs `backButton()`: 48 px square, back arrow, no label): built by `icon_button` via `fw::find` (style square, `role: back`), side `touch::back` (48 / 32), compile-time checked = layout `header row` and >= `touch::min`, so it fills the header row exactly. Tap = event `back` + sound `back` (the workbench tap counter counts `tap` only); disabled = `blocked` + error. 3 variants (default / pressed / disabled). Pairs: shared `icon_button`. WIP round 1. |
| `toggle` | Facet switch (docs `toggleSwitch()`, `.f-toggle`): off / on x default / disabled (4 variants). Geometry from tokens: height `touch::min`, inset `sp::s2`, knob = height - 2 x inset, width = 2 x knob + 2 x inset (720: 80 x 48, knob 32, = docs; 480: 56 x 32, knob 24, travel 24). Tap = Motion pattern `toggle` (knob slide + track colour via `fw::animate`, reduced = instant), `fw::emit` "tap" detail on/off, sound `toggle-on` / `toggle-off`; disabled (`USER_1`, not in the docs toggle, Button rule) = hollow track + text-mute knob (off) / accent-dim track + text-dim knob (on), tap = "blocked" + `error`. No pressed look, no visible label (docs); labelled row = List row later. Pairs: colour.json section `toggle`. |
| `slider` | Facet slider (docs `slider()`, `.f-slider`): horizontal, height `touch::min` (48 / 32 = docs slider-h), track `sp::s1` 4 px round ends `line-hi`, fill `blue`, knob = height / 2 (24 / 16 = docs slider-knob) `accent`, centred on the value; box = track + half a knob each side (track inset knob / 2 = 12 / 8) so the knob never clips at min / max. Props value / min / max / step / width (dp) / state; 6 variants (0, 40, 100, step 10, disabled x 2). Press on the knob = grab; elsewhere = tap-to-jump (value set at once, knob sweeps with Motion `value`, moving stops the sweep). Sound: `fw::step_ticks` (see "Events and sound"): every step the shown value crosses (drag, sweep, jump) counts, ticks at most every `tick-min`, ends = `slider-landmark`. `change` (value) on release; disabled (`USER_1`: track line, fill text-mute, knob text-dim) = `blocked` + `error` once per press. Pairs: colour.json section `slider`. |
| `dial` | Facet light dial (docs `dial()`, `.f-dial`, Light screen; v1 lab look): square `lay::dial_scale` (560 / 376) tick scale 0-100 over 270 deg from 135 (open at the bottom), 51 ticks drawn in ONE draw event by a `TickRing` struct (count / range / minor + major length, colour, stride, numbers) shaped to move to the core as the shared step-mark helper (facet-129, not built): minor `lay::dial_tick` line-hi, every 5th `lay::dial_tick_major` blue + its number (c-meta, text-dim) `sp::s3` inside. Ticks = `lv_draw_line` (anti-aliased; endpoints snap to whole px, up to ~0.9 px off the true radial, measured 2026-10-22). `lay::dial_arc` (432 / 288) = lv_arc (already anti-aliased, no buffer). Darren 2026-10-22: ticks and arcs back from the stroke atom (atom only where LVGL is visibly pixelated): track `stroke::edge` line square ends, indicator `stroke::indicator` accent round caps, knob `lay::dial_knob` accent with a `stroke::knob` canvas ring. Centre flex column (value centred on the dial): caption (label, text-dim) `sp::s7`, value (display), `sp::s2`, unit (body, text-dim), `sp::s9`, ON (outline) / OFF (secondary) = the Button component (`lay::dial_toggle_w` x `button_h`) in a hit box that owns the tap and trickles PRESSED. 480 = docs x 2/3 onto the grid (no docs 1x). Slider value model: step index x 256, grab the knob or tap the ring = set now + Motion `value` sweep; centre (inside dial_arc / 2 - touch::target) ignored; bottom gap = nearer end, a drag never crosses it. Opening sweeps 0 -> value on every build (silent, no prop; workbench re-builds from playground changes / demo resets pass `"opening": false`, meta `opening` = the "Replay opening" button). Button: off sweeps to 0, on back to the last value (silent sweep), `toggle` on/off + toggle-on/off; dragging while off turns on, release at 0 turns off. Step ticks pitched (see Events and sound). Round 2: range props `min` / `max` / `step` (0.5 steps fine, value shows the step's decimals: 0.5 -> "21.0", "21.5") + `unit`; scale numbers = smallest 1/2/5 x 10^k interval with at most 11 numbers, minors split it in 5 (a 2 in 4); value, sweep, ticks, landmarks and pitch follow the range; release at 0 switches off only when min is 0. Disabled = text-mute scale/value/arc, text-dim knob, disabled button, `blocked` + error; unavailable = no arc value/knob/unit/button, "Unavailable" (body, text-dim). 8 variants (incl. 16-30 °C step 0.5). Pairs: colour.json section `dial`. |
| `card` | Facet Home card (docs `card()`, `.fs-card`): `lay::card_w` x `card_h` (320 x 280 / 212 x 188), surface + 1 px line border, padding `card-pad` - hair, flex column (no hand placement): top row = screen icon (`ic::icon`, text-dim) + caps `label` + accent dot (`sp::s2`, 8 / 4: docs 6 at 1x is off the grid) and the preview artwork top right (weather at `ic::card`, 72 / 48); grow; `value` (type value, one line) or `text` (type title, up to 2 lines, bottom-aligned); gap `sp::s9` so the value box ends at the docs card-value-bottom (88 / 56); `sub` (type body, text-dim or tone ok / warn / error). Props label / icon / preview / mode / value / text / sub / tone / accent (dot + accent-text value) / state; 9 variants. Pressed (Motion press / release); press style = component token `components.card.press` in colour.json (`fw::tokens::comp::card_press`, default fill; prop `press` overrides): fill = mix token `press-tint` (the accent moved halfway to text-mute, `components.card.press-desat` 0.5, then 5 % over the surface, `card.press-tint` 0.05; on RGB565 such a faint tint is one 5/6-bit step per channel, so desaturation, not lower opacity, is what makes it gentler) + line-hi border, border = 1 px accent border, border-fill = both. Unavailable (not in the docs; Button rule, `USER_1`): icon, label and an em-dash value in text-mute, sub "Unavailable", no preview, no press look. Tap = `tap` (detail = label) + sound `open`; unavailable = `blocked` + `error`. Pairs: colour.json section `card`. |
| `list_row` | Facet hairline list row (docs Settings rows `.fs-set`, picker rows `pickerRow()`): not boxed, on the canvas, 1 px `line` hairline on top (a stack = dividers between rows); height layout token `list-row-h` (96 / 64 = docs sheet-row-h, new in space.json), width = content width 2 x `card-w` + `gutter` (656 / 440), padding `sp::s4` left/right, flex row gap `sp::s4`, centred. Optional icon (`ic::icon` 24 / 16: the icon manifest ships row icons at that size; an icon@size not in the build draws nothing, text-dim), title (type title, one line, ellipsis) over optional sub (type body, text-dim; 40 + 32 / 28 + 20 = on the grid), trailing none / value (body, text-dim) / chevron (`ic::icon`, text-dim: text-mute is disabled-only) / value + chevron / toggle (the `toggle` component itself via `fw::find`, made non-clickable: the whole row is the hit area and forwards CLICKED). Pressed = Card's mix `press-tint` fill (Motion press / release on bg_opa; a tint over the canvas was tried: invisible on dark 565 or failing the toggle track 3:1 in light). Disabled (`USER_1`): text-mute icon/title/value/chevron, toggle disabled; unavailable: text-mute icon + title, trailing = "Unavailable" (text-dim). Tap: chevron rows `tap` + `open`, toggle rows = the toggle's own tap + toggle-on/off, others `tap` + `tap`; disabled / unavailable = `blocked` + `error`. 4 px grid: text parts sit in slots grown so row - slot is a multiple of 8 (480: title 28 -> 32, body 20 -> 24), so every centred top lands on the grid at both densities. Playground: trailing none + a typed value shows the value, `on` with trailing none shows a toggle, an empty value shows an em dash. 13 variants incl. `list: stack`. Pairs: colour.json section `list_row`. |
| `label` | Facet label atom (docs `label(text, type)`, `.f-label` + `fit8`): one line in a component type style (type select = the 13 c-* styles in docs order: title, card-title, tile, body, label, label-sm, meta, meta-sm, caps-sm, subtitle, date, weather, number), colour text / text-dim / accent-text / blue / ok / warn / error / disabled (= text-mute, disabled only). Box width = text width rounded up to `sp::GRID` (8 / 4, docs fit8), height = the style box (set in the component: core text-helper note). Empty text = the style's `sample` (c-number is digits only). Static. The other small parts build their text through `fw::find("label")`. 15 variants. Pairs: colour.json section `label`. WIP round 1. |
| `status_dot` | Facet status dot (docs `statusDot(tone)`, `.f-dot`): `lay::dot` (8 / 4; docs 1x 6 is off grid), tones ok / warn / error / working (accent) / info (blue) / idle (text-dim; docs text-mute is disabled-only here). internal prop `on` (not in the playground; the Status chip passes `chip`: working = accent-text there, accent drops under 3:1 on its own tint in light). Optional `pulse` = Motion pattern pulse (opacity to pulse-min, loop; reduced = still). Static. 7 variants. Pairs: section `status_dot`. WIP round 1. |
| `status_chip` | Facet status chip (docs `statusChip(tone, text)`, `.f-chip`): `lay::chip_h` (32 / 24; docs 1x 20 puts the 16 px text box off grid), padding + gap `sp::s2`, radius `rad::chip`, hug; fill = tone at component token `chip.tint` 0.14 (docs opacity chip-bg) as bg_opa (works on canvas or card; mixes `chip-<tone>` + `chip-<tone>-surface` let the contrast test check both); Status dot (`on: chip`) + Label (caps-sm, colour **text**: the docs tone-coloured text fails 4.5:1 on its own tint in light at any tint >= 6 %, so the tone is dot + fill). Optional pulse (passed to the dot). Static. 7 variants. Pairs: section `status_chip`. WIP round 1. |
| `count_badge` | Facet count badge (docs `countBadge(n, tone)`, `.f-badge`): `lay::badge_h` (32 / 24) tall and at least as wide (one digit = circle), padding `sp::s2`, round, number box >= 16 on the grid (Label, caps-sm, text, centred); fill = Status chip's tint. Counts: 0 = prop `zero` muted (idle tone, shows 0; default) or hide; 1-99 as is; 100+ = `99+`. Default tone idle (docs). Static. 7 variants. Pairs: section `count_badge`. WIP round 1. |
| `section_header` | Facet section header (docs `sectionHeader({title, count, tone, right})`, `.f-section`): `lay::section_h` (48 / 32) x content width (656 / 440), flex row gap `sp::s5` (16 / 12 = docs gutter both densities): Label title (caps-sm, text-dim), optional Count badge (count / tone / zero), grow, right = none / meta (Label meta, text-dim; docs text-mute) / action (text button, caps-sm accent-text, hit box section_h tall + `sp::s2` padding translated right so the text ends on the content edge; pressed = Card mix `press-tint` fill with Motion press / release, radius chip; disabled = text-mute, USER_1, `blocked` + error; tap = `tap` + sound `tap`). Title, badge, meta static. 8 variants. Pairs: section `section_header`. WIP round 1. |
| `corner_ticks` | Facet corner ticks atom (docs `cornerTicks(corners, len, thick)`, `.f-ticks`), WIP: core `fw::corner_ticks(obj, Corners::all\|tr_bl, show = st::ON, len = lay::corner_tick, thick = stroke::corner_tick)` in `core/atoms.h` (8 / 4 x 2, `st::on_mark` = accent, on the border box past padding + border; with a `show` state the owner needs `LV_OBJ_FLAG_STATE_TRICKLE`). Button secondary active calls it (all). Pressed (active + pressed) = `st::on_mark_pressed` (accent-text, states.json on.mark-pressed: accent was 2.87-2.99 on the pressed grey in light; Darren 2026-10-21). Cards / tiles: decide with the screens (no ticks on the approved Card for now). Page: plain surface box all / tr-bl (shown while active) + the real Button secondary active / active+pressed / default. Pairs section `corner_ticks`. |
| `focus_ring` | Facet focus ring atom (docs `focusRing(offset, r)`, `.f-ring`), WIP: core `fw::focus_ring(obj, show = st::ON, offset = st::FOCUS_OFFSET)` in `core/atoms.h` (LVGL outline `stroke::focus` 1 px, pad offset - width, radius follows the owner, hidden under `st::OFF`, `fw::bleed` so nothing clips it); replaces the generated `st::add_focus_ring`. Button primary + Icon button play call it. Page: box at focus-offset / none / space 2 / space 4 / disabled + the real Button primary active, play active, primary active+disabled. Pairs section `focus_ring`. Refactor pixel-identical: 1104 frames (Button 24 + Icon button 19 + Back 3 variants x 720 / 480 x dark / light x default + 5 accent sets) hashed before / after, 0 differ. |
| `header` | Facet screen header (docs `header()`, `.fs-header`), WIP: the row only, `lay::content_w` x `lay::row` (656 x 48 / 440 x 32; static_assert `touch::back == lay::row`), placed by the screen at (inset, inset). Flex row centred, gap `lay::gutter`: Back button (`fw::find("back_button")`, optional) + `lay::back_gap` (16 / 12) to the label (screen type `label`, caps, text-dim, grows + ellipsises: LVGL flex never shrinks, so the label is the grow), optional Status chip (working, "2 working" = docs explorer "Right chip"), then the **Screen chrome** (`fw::find("screen_chrome")`: the time; prop `settings` (bool, default off, on for Home) adds the settings button before it; prop `clock` off = none). Props label / back / clock / time (empty = live clock) / settings / chip. The Layout foundation's samples use it (`header(s, label, back)`, fixed time; Home without back). Pairs: section `header` (+ icon_button, status_chip, screen_chrome). |
| `screen_chrome` | The top-right corner of every screen (Layout rule `chrome`; docs Home `settingsButton()` + header clock, on every screen per Darren 2026-10-22), WIP: `lay::chrome_w` x `lay::row`, prop `settings` none (time only, `lay::header_time_w` wide) / button (Home: the Icon button `role: settings`, `lay::chrome_btn` square, `screen-settings` at `ic::icon`, resting look = surface, line border, ink lines, accent knob; pressed = press tint; tap = `navigate` "settings" + sound open) / here (active border, unused), a gutter, the time (screen type `label`, text-dim, `lay::header_time_w`, right-aligned). Prop `time` = fixed text; empty = live clock (localtime HH:MM, 1 s timer redrawing on a new minute, `--:--` until the device has the time). Update path: time + here in place, button on / off = refill. Pairs: section `screen_chrome`. |
| `tile` | Facet agent tile (docs `tile(agent)`, `.fs-tile`), WIP: `lay::tile_w` x `tile_h` (208 / 136), surface + 1 px line, `rad::tile`, padding `tile_pad` - hair, flex column: core mascot (`fw::mascot`, mood = docs `moodFor(kind, status)`: error/failed, working, needs, idle = sleeping, done <= 2 h = done else idle; colour = the mood's own token = the docs tile colours), grow, title (type body, 2 lines in 2 x body box, ellipsis), `sp::s5`, status row (type label: state word working accent-text / needs warn / else text-dim, detail right text-dim fit to the grid, " ctx" / " ago" dropped). Pressed = Card press (states.json pressed.style, prop `press`); the sleeping body brightens text-mute -> text-dim while pressed (2.9:1 on press-tint in dark; states.json disabled on-fill token). Unavailable = sleeping mascot in `st::unavailable_ink`, title text-mute, "Unavailable". Tap = `tap` (title) + `open`; unavailable = `blocked` + error on finger down. 8 variants. Pairs: section `tile` (+ mascot). |
| `hero_block` | Facet hero block (docs `heroBlock()`, `.fs-hero`), WIP: content width x the number's type box (hero 96 / 64, role display 144 / 96), flex row gap `lay::gutter`: number (type hero / display, text; accent = accent-text; box fit to the grid) + column (label type `label` text, `sp::s2`, line type body text-dim ellipsis) offset (hero box - column) / 2 from the top (= docs padding-top 16 at 720, 12 at 480; kept for display too). Unavailable = em dash (`st::PLACEHOLDER`, now in every number glyph set) text-mute + "Unavailable". Static. 6 variants. Pairs: section `hero_block`. |
| `more_row` | Facet more row (docs `moreRow()`, `.fs-more`), WIP: content width x `lay::more_row_h` (64 / 44), 1 px line on top, padding top `sp::s7` - hair (label at 24 / 16 like the docs rows), sides `sp::s4` (= List row, so it lines up with the rows above; docs none), flex row gap `lay::gutter`: label (type label, text-dim, grows, ellipsis) + chevron (`ic::icon`, text-dim; docs text-mute). Pressed = List row's press-tint fill; disabled = text-mute, `blocked` + error; tap = `tap` + `open`. 4 variants. The Layout foundation's rows sample uses it. Pairs: section `more_row`. |
| `stat` | Facet stat (docs `stat()`, `.fs-stat`), WIP: flex column gap `sp::s2`: label (type label, text-dim; docs text-mute) over value (type body, one line, ellipsis; tone dim / text / warn / ok). Width `lay::card_w`, or the parent's with internal prop `fill`. Unavailable = label text-mute + "Unavailable". Static. 6 variants. Pairs: section `stat`. |
| `radio` | Facet radio (docs `radio(on)`, `.f-radio`), WIP: `lay::radio` (24 / 16; docs 24 at both, 480 = x 2/3 to match `ic::icon`) round, `stroke::edge` ring **text-dim** (docs line-hi is 1.8-2.0:1 on bg, a control boundary needs ui 3:1), on = ring + `lay::radio_dot` (8 / 8, centred on the grid) in `st::on_mark` (accent); state = CHECKED (`st::ON`) trickled to the dot, so an owner just sets the state; switch = Motion toggle as style transitions (colour / opacity only); disabled = text-mute. Standalone tap selects (never un-chooses): `tap` "on" + tap; disabled = `blocked` + error. Hit area `touch::min`. 4 variants. Pairs: section `radio`. |
| `picker_row` | Facet picker row (docs `pickerRow()`, `.f-prow`), WIP: the List row component itself (`fw::find("list_row")`: hairline, `list-row-h` = layout `sheet-row-h` static_assert, icon `ic::icon`, name = title, press, disabled / unavailable, tap `tap` + tap) + a caps status (screen type `label`, text-dim; tone working = icon + status accent-text) added to its text column (slot grown onto the grid: 40 + 24 / 28 + 16 -> 48) + the Radio component trailing (non-clickable: the row is the target). Picker rows sharing a parent are one group (rows marked `LV_OBJ_FLAG_USER_1`): a tap turns its radio on and the siblings' off. Docs draw the icon at sheet-icon 32 / 20, but icons.json ships the picker-row icons at icon size only (open question). 10 variants incl. `list: group`. Pairs: section `picker_row`. |
| `speaker_button` | Facet speaker button (docs `speakerButton()`, `.f-spk`), WIP: `lay::speaker_btn_h` (48 / 32, static_asserts: fits the header row, >= touch min), hug width on the grid, surface + hair line, `rad::button`, padding `sp::s4` - hair, gap `sp::s2`: speaker `ic::small` (text-dim; accent-text while playing) + name (type c-label caps, text, max `lay::speaker_btn_max` 240 / 160 then ellipsis) + chevron-down `ic::small` text-dim. Pressed = surface-press + line-hi (Motion press / release); disabled = text-mute + `blocked` + error. Tap = `tap` (name) + sound `open` (docs data-snd open). 5 variants. Pairs: section `speaker_button`. Open: 480 icon 12 in 32 = 10 px offset (off grid). |
| `bottom_sheet` | Facet bottom sheet (docs `bottomSheet()`, `.f-bsheet`), WIP: overlay on its screen (floating, hidden while closed): scrim = one full-screen rect, bg at Motion `scrim` 0.72, tap = `close` "scrim" + sound back; panel from `lay::sheet_top` to the bottom, full width (docs left 0 right 0), bg, 1 px line-hi top edge, `rad::ring` top corners (runs `rad::ring` past the bottom), padding inset - hair / inset, gap gutter: head (`lay::row`: caps title text-dim + Close = Icon button square close via `fw::find` in a hit box that trickles PRESSED: `close` "close" + sound back) + a scrolling list of Picker rows (group: the radio follows the tap; the sheet closes after a pick). Motion `sheet-open` / `sheet-close` (scrim bg_opa + panel opa / translate as one object; reduced = fade; reversible mid-way). Workbench page: the sheet over a real "Settings" screen at device size (Header + List rows; the Speaker / Theme rows open their list, a pick updates the row value); props open / list / title / selected / screen; internal `loop` (bench), `overlay` + `still` (the Layout foundation's overlay sample), `flatten` + `panel: solid` (bench options, see "Bottom sheet frame cost"). 5 variants incl. reduced motion; the demo reset replays the opening. Pairs: section `bottom_sheet`. |
| `toast` | Facet toast (docs `toast()`, `.f-toast`), WIP: content_w x `lay::toast_h` (120/80), surface, 1 px line-hi edge, `rad::card`, padding toast-pad - hair, row gap toast-pad: icon square `lay::toast_icon` (48/32, space.json; `rad::button`, tone at chip.tint = mix chip-<tone>-surface) with the icon at `ic::glyph` in the tone ink (working accent-text, idle text-dim), title (type title) over body (type body, text-dim), optional action = Button secondary via `fw::find` (dismisses: "action"), timer bar `stroke::edge` along the bottom in the tone fill, shrinking linearly over `toast::life` / `life_urgent`, paused while pressed. Gestures: drag follows the finger (translate_x only, no fade: perf), released past `toast::swipe` = swipe, under `tap_slop` = tap, else springs back (Motion toast-in). Dismiss = event `dismiss` (tap / swipe-left / swipe-right / timeout / action / pushed) + sound back on tap or swipe, then leaves (Motion toast-out: fade + back to its edge, or sideways `swipe_out`). Protocol for the layer: internal props `layer` + `edge`, sends VALUE_CHANGED (dismissed) and READY (left), takes CANCEL (pushed out; param 1 = quiet). Props icon / tone / title / body / action / timer (off = docs static) / life; 9 variants. Pairs: section `toast`. |
| `toast_layer` | Facet toast layer (docs `toastLayer()`, `.f-toasts`), WIP: floating, non-clickable layer over its screen (only toasts take taps), up to `toast::max` Toasts newest nearest the entry edge, `lay::gutter` apart, top = first at `toast_top`, bottom = first ends at the bottom inset. Push = Motion toast-in (fade + slide from the edge), the others move a slot (y, toast-in), over max = the oldest gets CANCEL; arrival = event `toast` + sound `toast::sound` (notify). Workbench page: a real "Toasts" screen (Header + six List rows, one per tone; a tap pushes that tone's toast); props position / push (arrives on build: the demo reset replays it) / stack (1-3, toast-in apart, follow-ups quiet so a sample never re-arms its own reset) / timer / action / screen; internal `overlay` + `still` (Layout page overlays) and `loop` (bench). 8 variants (timer off so the grid stays full). Frame cost: "Toast frame cost". |
| `weather_icon` | Facet weather icon (docs `weatherIcon(id, { size, animated })`), WIP: core `fw::weather_icon`; animated = the master's parts moving by `motion.json` weather, static / reduced motion = the static master; props icon (17) / size / animated / on canvas|card (the occluder colour; card = surface box). Update path: icon + animated swap in place. See Icons > Animated weather. |
| `moon_icon` | Facet moon phase glyph (docs `moonIcon(frac)`), WIP: core `fw::moon_icon`, masks computed on the device; props phase 0..1 / size / south; 9 variants. Update path: phase redraws in place. See Icons > Moon phase. |
| `split` | **Layout** (pages are made only from components, Darren 2026-10-23): two columns over the content box, card-w + sp::s9 + the rest, each a top and a foot slot (meta `"slots": 4`; slots marked `fw::spec::SLOT`). Spec nodes go in with `"in"` + `"slot"`. |
| `group` | **Layout**: one content-width slot that wraps its nodes, lay::gutter apart both ways (tiles, cards, a row of buttons); `align` start / end, `space` none / group. `flow` wrap / stack (one column, no gap: a view's blocks); `space` also block (sp::s9) / near (sp::s7); an end-aligned bar centres its items across. |
| `status_block` | A device's state in words: caption (label, tone), value (value), line (body, text-dim); fed by bind maps. `size` value / hero (state in type hero, accent-text when working, line in title: the lab's Vacuum Rooms view) / title (state in title, line in label: its Map view); `value2` and `line2..4` join with " · " (one bind per fact); an empty caption takes no room. |
| `lock_ring` | The door lock's padlock in its ring (the panel lab's lock art): the state and the control, hold-to-unlock confirm meter (Motion hold-confirm), tap to lock, busy turn (Motion busy-turn). |
| `hold_button` | A Button that fills while held (hold-confirm) and only then emits `confirm`; shown by an entity's supported_features (`features` & `mask`). |
| `room_tile` | A room to pick for a job (vacuum rooms): tile-w x room-tile-h, picked = accent edge + corner ticks, emits `pick` (spec screen.toggle). |
| `iso_floorplan` | **Floor plans foundation, first component** (tokens/floorplan.json -> fw::fp::; the panel lab's Vacuum Map view, apps/hub panel/vacuum/iso.ts): rooms as polygons in metres (prop `plan`, data from a spec parameter: "Name\|segment\|x,z ...\|lx,lz;..."), drawn in true isometric as raised tiles (slab 0.3 m) with a low back + left wall (0.5 m), fitted into content-w x lay::plan_h; ONE RGB565 image per theme (own 2x2-sample scanline fill + round-join strokes, painter's order), labels = real labels for rooms it cleans, the dock a steel box, the run's path (text-mute 50 %, blue 80 % live). The robot = a small ARGB sprite (cylinder + LiDAR, accent while cleaning) moved with lv_anim (fp::GLIDE_MS linear): a move redraws two sprite boxes (behaviour redraw_max 0.05). Tap a room = `pick` "Name:id" (spec screen.toggle), blocked during a run; a pick redraws only that room's box. Props plan, dock, robot (+ transform a..f from the sensor's units), trail, picked, here, state; how it is drawn = the Floor plans foundation (`foundations/floorplans.cpp`). Audit snaps: picked, here, state, trail (one picture). |
| `vacuum_mode` | The robot vacuum's cleaning settings (lab Mode row + map mode button + mode sheet): style row = the List row (over "mode", "Vac + mop · Turbo · High water", chevron), style button = the map's short caps button (touch::min, hairline, at most lay::mode_btn_max, grows as the bar's spacer; only with view = map); either opens a Bottom sheet (on the screen root) with one Segmented per setting: Cleaning / Suction (not for Mop) / Water (not for Vacuum). A pick = `change` "mode <id>" / "fan <id>" / "mop <id>" (spec vacuum.control: select.select_option on the action's mode / mop select, vacuum.set_fan_speed). |
| `event_row` | One thing that happened in an activity list (lab Door lock "last 24 h"): lay::event_row_h, hairline on top, the time (label, text-dim, lay::event_time_w) + "Locked · Alex" (body, text-dim; jammed = error). Prop event "09:03\|locked\|Alex". Static. |
| `column` | **Layout**: the content box as one column, a top slot and a foot slot pinned to the bottom (meta "slots": 2): the Vacuum's view + its action bar. |
| `placeholder` | A part not built yet: a plain labelled outline (first use: the vacuum's map, waiting for the Floor plan foundation). |
| `level_bar` | A level as a straight bar with an optional threshold mark (soil moisture). |
| `plant_gauge` | One plant's soil moisture as a screen hero: caption, hero %, line, Level bar with the dry line. |
| `plant_row` | One plant in a list: the List row + a small Level bar; with an entity a tap opens its Plants screen. |
| `camera_tile` | One camera's still (stand-in until the panel fetches stills) with a Frigate-style detection box + tab, name and time. |

## Screens

Whole screens in `renderer/screens/` (`fw::add(..., "screens", ...)`, workbench `/wasm/screens/<id>`), built only from
components; each places its Header at (inset, inset), so every screen gets the screen chrome (settings button + live
clock, top right; the settings button on Home) from the layout rule. The settings button emits `navigate` "settings" and
Home's cards `navigate` <page id>: the **Device** page (`/wasm/screens/device`, `core/device.h`) navigates inside the
renderer like the panel; on a single-screen page the workbench opens that page instead (hub `src/wasm/app.ts`).

| Screen | What it is |
|---|---|
| `device` | **Device (live)** (WIP): the panel running as on the wall, `core/device.h` `fw::dev::mount(parent, start)`: starts on Home, every tap handled inside the renderer (no page loads): `navigate` <id> (settings button, Home cards) = go, `back` (Header Back) = back, a screen stack (an id already on it pops back to it), the Content rise (Motion `screen`, from above going back), a non-screen id (foundation pages from the placeholder Home) shown under a Header with Back. Emits `screen` <id> after each cut. Panels: POST /fw/show `{"id":"device"}` (core/device.cpp is in firmware/bundle.mjs). Prop `start`. |
| `home` | **PLACEHOLDER** (Darren 2026-10-22; the real Home will be laid out per room, not just a card grid): Header (no back, "Home", screen chrome with the settings button: Home only for now), a scrolling 2-column grid of real Cards (`lay::card_w` x `lay::card_h`, gutter both ways; 480 cards stay 184: 2 x 184 + 16 = 384 of 388, the 4 px left at the bottom) for the screens and workbench pages this build has (`PAGES` checked with `fw::find`: Settings, then the foundations); **placeholder cards: title only** (no icon, value or line; Darren 2026-10-22). Card tap = its own tap + `navigate` <page id>. No props. |
| `settings` | Every personalisation setting from `tokens/settings.json` (WIP): Header ("Settings", the live clock), one scrolling column from `lay::top`; per group its label (screen type `label`, caps, text-dim = docs `.fs-set-glabel`; Darren 2026-10-22: the component caps-sm was too small), `section-gap` to its rows, groups `group` apart; rows = List row / Segmented / Accent swatches / Toggle / slider row / sheet rows + one Bottom sheet. Update path for every control. Pairs: section `settings` (+ the components'). |

**Pages from components only (Darren 2026-10-23).** A screen spec is a tree of real components: no screen-specific
`*_view` body for new screens. Arrangements come from layout components (`split`, `group`) whose slots take nodes
(`"in"` / `"slot"`); a node can repeat per item of a list parameter (`"each"`, with `"sample"` entries while the list
is empty); binds carry a sample value, a map to words / tones and a template (`"s"`, `"map"`, `"else"`, `"tpl"`);
`screen.toggle` keeps a picked list (room picks); `"show"` (a bind) shows a node only while its value is set (the
vacuum hides its rooms while cleaning); `"in": "header"` puts a node in the header row (the Vacuum's Rooms | Map
Segmented, whose `screen.param` view switches the views); a bind with `"e": ""` reads a screen parameter (`"s":
"$view"`); fmt `count` / `time` / `day` (Clean all / Clean Kitchen / Clean 2 rooms, since 09:03, last run yesterday
18:42). Specs `lock`, `vacuum`, `plants`, `cameras` are built this way;
core/spec.h has the format.

## Behaviour tests

The audits check how things look; `renderer/tools/behaviour.mjs` checks that they work. Every component and screen is
built headless on the WASM build (a fresh module per gesture, so nothing leaks between tests) and driven through LVGL
input with time advanced in fixed 16 ms ticks: tap, long press (800 ms), press-slide-off-release, drag. It asserts on
what reaches the host (`fw_events`: actions, `navigate` / `back`, HA calls `ha`, `setting`), sounds, scroll position and
limits, the device screen stack, the update path and HA values. Hooks it needs that the workbench has no call for live in
`core/test_hooks.cpp` (`fw_test("stack" | "setting" | "scrolls")`, WASM only). Runs in `facet-gate full` (`behaviour`
step; `--ids` limits it; `fast --ids` runs the 1.5x interaction / scroll / sound part). About a minute for everything.

```
node renderer/tools/behaviour.mjs [--den 1x|1.5x] [--ids a,b] [--kind interaction,scroll,sound,lag] [--verbose] [--json out.json]
```

| Kind | Automatic, for every id x variant | Fails when |
|---|---|---|
| interaction | tap each clickable part; long press; press, slide off, release; spec screens: taps inside each node `fw_hits` returns; device: every navigation + Back | a disabled / unavailable part fires anything but `blocked`; one gesture fires the same action twice; sliding off fires; a spec node's handled event reaches no action, an HA call names another entity, an unavailable entity gets a call; the stack after a navigation or Back is wrong |
| scroll | every scroller with room to scroll (labels and lists under a sheet aside); a drag on content that fits | a drag does not move it; a step jumps or goes back; a fling past either end does not stop exactly at the end; an update moves it; a nested scroller of the same direction moves in the same frames; content that fits moves |
| sound | the same gestures; an update to each variant; HA values for every bound pair; device with Sounds off | more than one sound per gesture (step ticks aside); a sound not in `tokens/sound_usage.json` for the id (+ what it builds); an action with no sound; any event or sound from an update; a sound with Sounds off |
| lag (1x) | frames of the build animation, every tap, every scroll; 2 s idle; an update with the same props | a refresh past 32 dirty areas (LVGL redraws the whole screen); more than 3 frames in a row redrawing half the screen (a scroll list: 90 %); typical frame estimated over 33 ms on the Guition; any redraw while idle or after an update that changed nothing |

**Lag is a proxy.** It runs the 1x build (the Guition, the weakest panel) and estimates Guition frame time as
`a + b x WASM ms` from the bench fit (`bench/budgets.json`); pixels and dirty areas are exact, times are estimates.
Real fps needs the device (`tools/bench.mjs --panel`). Budgets: `tests/behaviour/_budgets.json`.

**Per-id files** `renderer/tests/behaviour/<id>.json` (every component and screen has one):

```json
{ "id": "toggle",
  "live": true, "live_why": "...",           // moves by design (clock, spinning disc, live sim): idle / same-update redraws allowed
  "tests": [ { "name": "tap turns on", "kind": "interaction", "props": {"on": false},
               "steps": [ {"tap": {"clk": 0}} ],
               "expect": { "events": [["toggle", "on"]], "sounds": ["toggle-on"] } } ],
  "xfail": { "interaction:press-cancel:default": "BF-012" } }
```

Steps: `tap` / `long` / `press` / `release` / `drag` (+ `"by": [dx, dy]`, fractions of the screen when |v| <= 1) on a
selector, `wait` ms, `update` props (the update path), `ha` `[entity, attribute, value]`, `setting` `"id=value"`;
`before_ha` sets HA values right after the build. Selectors: `{"text": "Start"}` or `{"text": "/re/"}` (the clickable
around that label), `{"clk": i}` (i-th clickable), `{"type": "arc"}`, `{"hit": "dial"}` (a spec node from `fw_hits`),
`{"at": [fx, fy]}`. Expect: `events` (in order, others may come between; detail = regex), `no_events`, `sounds` (exact
list, ticks aside), `ticks_min`, `silent`, `stack`, `exists` / `absent` (a selector).

**Expected fails.** A known failure is listed in the id's `xfail` (key `kind:test:variant`, `*` allowed) with its finding
id; the gate stays green on them and prints `XPASS` when one passes again (then delete the line). Findings registry:
`tests/behaviour/_findings.json` (`--write-xfail` adds today's new failures as findings: bootstrap / after a review only).

**A new component or screen ships with its file:** its automatic tests pass (or its known fails are listed with a
finding), plus a custom test for each thing it does (the action and value a tap or drag sends, the sound, what it does
disabled / unavailable, sheets it opens, where Back goes).

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
Save: the watcher builds and the nav shows it (as WIP). Delete a component = delete its file. Add its behaviour test
file `tests/behaviour/<id>.json` (section "Behaviour tests").
