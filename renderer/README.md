# Facet renderer

The renderer is the C++ and LVGL code that draws every Facet screen. The same sources build two ways:

- **WebAssembly**, for the docs site, the workbench and the tests (`build.sh`, Emscripten).
- **Panel firmware**, as the ESPHome component `facet_wb` (`../firmware/bundle.mjs` packs these sources, `../esphome/`
  builds them for each board).

What a part must do to count as done is in [CONTRACT.md](CONTRACT.md). Read it before writing a component.

## What you need

- Node 24, then `cd renderer/tools && npm ci` once.
- Python 3 with `venv`. The font rasteriser makes its own venv (`renderer/.venv`) on the first build.
- Emscripten 6.0.13: `em++` on `PATH`, or `$EMSDK` (default `~/emsdk`).

Nothing else. LVGL 9.5.0 and ArduinoJson 7.4.3 are in `third_party/`, `lv_conf.h` is committed and the fonts are in
`fonts/`.

## Build

```bash
renderer/build.sh            # both densities
renderer/build.sh 1.5x       # one: 1.5x = the 720 px panels, 1x = the 480 px panel
```

The output goes to `renderer/out/<density>/` (`fw.mjs`, `fw.wasm`, `manifest.json`). The first build compiles LVGL and
takes a minute or two. After that a build only recompiles the sources that changed, or that include a header that
changed, so a component edit takes a few seconds.

`build.sh` only builds; it never runs checks.

Options, from the environment or from `local.env` at the repo root (untracked, optional):

| Variable | What |
|---|---|
| `FW_OUT` | output folder (default `renderer/out`) |
| `FW_CACHE` | object and generated-header cache (default `renderer/.cache`) |
| `FW_FEATURES` | `docs` or `firmware`: build only what that flavour ships (see "Features"), into `$FW_OUT/<flavour>/<density>/` |
| `BUILD_JOBS` | compiles at once (default: every core but one) |

`node renderer/watch.mjs` rebuilds the 1.5x build whenever you save a source, token or spec, and the 1x build after a
minute of quiet. The workbench reloads the page when the build changes.

## Check

```bash
renderer/check.sh fast [--ids a,b]               # before every commit
renderer/check.sh gate [--ids a,b] [--den 1x]    # CI on every push and pull request
renderer/check.sh nightly                        # overnight: ASan, theme fuzz, lag proxy, bench
```

Every tier checks the build that is already there, so build first. Each step prints one line (PASS, FAIL or WARN with
its time); the full output of every step goes to `$FW_CACHE/checks/<time>-<tier>/`.

| Tier | What runs |
|---|---|
| `fast` | features, colour contrast of the declared pairs, contract lint, catalogue, then the fast rules over every component at 1.5x (each one replayed from a cache unless it changed). With `--ids`: the same rules narrowed to those ids, plus their functional tests |
| `gate` | the same checks, every rule at both densities (visual at 1.5x), behaviour and every functional test. `--den` runs one density's half; CI runs the halves as two jobs |
| `nightly` | ASan at 1.5x (and 1x on Sundays), the nightly functional suites, the theme parser fuzz, the lag proxy and the bench |

Functional tests are reported but do not fail a tier yet (WARN); a failing rule fails it.

Single tools, when you want one thing:

```bash
node renderer/tools/functional.mjs --ids slider -v            # one component's functional tests, with the log
node renderer/tools/functional.mjs --rules --tier fast --ids card   # the fast rules on one component
node renderer/tools/features.mjs check                        # the features manifest
node renderer/tools/spec.mjs < spec.json                      # dry run of a screen spec or deck on the WASM build
renderer/tools/asan.sh 1.5x                                   # the AddressSanitizer build and run
node renderer/tools/bench.mjs --device all                    # frame cost estimates (bench/README.md)
```

### Rules, tests and goldens

- **Rules** (`tests/rules/*.mjs`) are generic checks that run over every component: layout bounds and the 4 px grid
  (`audit`), what is really on the pixels (`pixel-contrast`), behaviour (one sound per tap, a disabled part fires
  nothing, scroll limits), screen layout, the spec store, the built-in themes, visual references and more. Each rule
  declares its tiers. `tools/functional.mjs --rules` runs them on a shared mount (`tools/lib/rules.mjs`).
- **Functional tests** (`tests/functional/<id>.test.mjs`) check one component's or screen's own job, end to end on the
  WASM build, in Node with no browser. [tests/functional/README.md](tests/functional/README.md) explains how to
  write one.
- **Behaviour cases** (`tests/behaviour/<id>.json`) describe what a component does when touched; the `behaviour` rule
  plays them. Findings that are by design go in `tests/behaviour/_findings.json`.
- **Goldens** (`tests/golden/<suite>/<density>-<name>.565.gz`) are raw RGB565 frames. A missing or changed golden fails
  and writes a `.new.565.gz` and `.new.png` beside it. A person looks at the PNG; only then does a run with
  `FW_GOLDEN_ACCEPT=1` accept it. Goldens are never accepted automatically.
- **Visual references** (`tests/ref/`, `tests/references.json`) are the approved original designs a ported component is
  compared with (`tools/ref_capture.mjs` captures them).

Every test proves something only if it fails without the behaviour. When you add one, break the code once and watch it
go red.

## Where things live

| Path | What |
|---|---|
| `core/` | the engine: the component API (`fw.h`, `fx.h`), the registry, tokens at run time, the Pen (all drawing), motion, states, sheets, toasts, the screen spec engine (`spec.h`), Home Assistant data (`ha.h`), the device app (`device.cpp`), the data feeds (`*_source.cpp`), the WASM host (`main.cpp`) |
| `components/` | one component per file |
| `foundations/` | the documentation pages for the design system: colour, type, space, layout, motion, states, icons, charts, mascot |
| `screens/` | the system screens written in C++: Home, Settings, Screens, Screen settings, Set up, Home Assistant link, Device |
| `specs/` | the factory screens as JSON specs (Clock, Weather, Calendar, Media, Timer, Energy, Moon, Light, Living room), the factory deck (`deck.json`) and the "Make screens with AI" examples (`ai-examples/`) |
| `sources/sources.json` | where each screen's data can come from (Settings > Screens) |
| `tokens/` | the single sources: colour, type, space, layout, motion, states, chart, mascot, icons, sound, settings, theme |
| `themes/` | the built-in theme files (`facet.json` is generated from the tokens) |
| `icons/` | the SVG masters of every icon |
| `fonts/` | Inter, JetBrains Mono, Monocraft, W95FA (SIL OFL) |
| `catalogue/` | `contract.json` (the parts on the contract), plus `catalogue.json` and `spec.schema.json`, generated from the build |
| `features.json` | what each release ships (see "Features") |
| `tools/` | generators (`gen_*.mjs`: tokens into C++ headers), the test runner, lint, contrast, bench, ASan, the fake Home Assistant (`demo_ha.mjs`) |
| `tests/` | rules, functional tests, behaviour cases, goldens, visual references, fixtures |
| `bench/` | the frame-cost bench: budgets, suite and the stress item ([bench/README.md](bench/README.md)) |
| `third_party/` | LVGL and ArduinoJson, each with its licence |
| `lv_conf.h` | the LVGL configuration, shared by the WASM build and the panels |

### Tokens

Every colour, size, duration and sound comes from `tokens/*.json`. At build time the `tools/gen_*.mjs` generators turn
them into C++ headers in `$FW_CACHE/gen/`, written only when they change, so an unchanged token rebuilds nothing.
A component never writes a colour or a size itself: it names a token, and a theme can change every token at run time.
Densities share one set of tokens in dp; `fw::px()` and the generated tables give the device pixels for each density.

### Screens

A screen is either C++ (`screens/`, the system screens) or a JSON spec (`specs/`). A spec is built from components
only: a header, nodes with their props, binds to Home Assistant entities or feeds (`panel.weather`, `panel.calendar`,
...), actions from one closed list, and its Home card. `core/spec.h` documents the format and validates it; the same
validation runs on the panel when a screen is sent to it, so a bad spec is refused with errors that name the field.

### Feeds

A feed (`core/<name>_source.cpp`) fills one reserved entity, such as `panel.weather`, from whichever source the user
picked: a Home Assistant entity, a public API, or sample data. A screen binds to the feed, never to a particular
integration, so the same screen works whatever the data comes from (CONTRACT.md C51).

## Features

`features.json` is the one list of what a release carries. A feature is some screens plus the components only they
use. Each has three flags:

| Flag | What it decides |
|---|---|
| `docs` | shown on the docs site |
| `status` | the label the docs show: `done`, `wip` or `preview` |
| `firmware` | compiled into the panel firmware, and so into the factory deck, the Home cards and Settings > Screens |

Every component that is not listed under a feature belongs to the **library** and ships, because custom screens are
built from it. `FW_FEATURES=docs` or `firmware` builds only the parts that flavour shows; the docs site builds from the
`docs` flavour. `node tools/features.mjs table` prints the flags. Unfinished features live on the `wip` branch until
they come back finished; `later.roadmap` keeps their rows on the docs roadmap.

## Write a component

1. Read [CONTRACT.md](CONTRACT.md) sections 3 to 10 (structure, size, style, drawing, interaction, robustness,
   themes) and section 13 (the recipe and the definition of done).
2. Create `components/<id>.cpp`. Start from a component on the contract of the same kind: `components/toggle.cpp` for a
   control, `components/card.cpp` for a container, `components/list_row.cpp` for a row. It registers itself with
   `FW_COMPONENT(id, "Name", "components", meta)`, where the meta JSON declares its props, variants, states, slots and
   minimum size, and builds only through `core/fx.h`.
3. Add it to `catalogue/contract.json` when it meets the contract, and to a feature in `features.json` only if it is a
   screen's own part or unfinished.
4. Add `tests/behaviour/<id>.json` and `tests/functional/<id>.test.mjs`.
5. Run `renderer/build.sh 1.5x` and `renderer/check.sh fast --ids <id>`, then the gate.

Delete a component by deleting its file, its tests and its entries; `features.mjs check` lists what still names it.

## Events, sound and updates

A component reports what happened and the host decides what to do: `fw::emit(obj, "tap", detail)` sends an event,
`fw::sound(obj, "tap")` asks for a UI sound by its `tokens/sound.json` id. In WASM the page drains them every frame
(`fw_events()`); on a panel they become Home Assistant actions and the speaker.

A component's update path (`FW_UPDATE`) changes what changed in place when new props arrive, without rebuilding it,
and a value that moves animates silently; a component without one is rebuilt. A screen that receives the same values again redraws nothing
(the `noop-update` rule checks this).

## On a panel

`../firmware/` holds the ESPHome glue (`fw_device.h`: the panel's HTTP API, settings, Home Assistant link, OTA notice,
image fetch) and `bundle.mjs`, which copies these sources into an ESPHome component. `../esphome/` has one YAML per board
and the packages they share; `esphome/build.sh <board-id>` bundles and compiles. The firmware reports the hashes of the
sources it was built from (`GET /fw/info`), so the workbench can tell when a panel runs older code than the build.
