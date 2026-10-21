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

## Colour tokens (single source)

`tokens/colour.json` is the only place colours live in this branch: per token `group`, `dark`, `light`, `role`; the
groups (page sections); `contrast` pairs (fg, bg, min ratio) for the Colour page; `accent` = the accent sets (five
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

## Foundations

`foundations/<id>.cpp`, same `FW_COMPONENT` API with section `foundations`. Meta `"layout": "doc"` = a document page:
one full-width WASM stage per variant (heading + `about` in HTML), `"frame": [w, 0]` = auto height (the stage follows
its content, `fw_content_h` + `fw_stage_resize`). Status = the .cpp plus `tokens/<id>.json` if present (any untracked
= WIP; approve = commit both). **Colour** (`foundations/colour.cpp`): every token by group, name + role, the colour in
its dark and light context with its hex (chips drawn by LVGL in RGB565, as on the panel), the accent sets, contrast
pairs with WCAG ratios (red = below the pair's min).

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
| `FW_PANEL` | panel firmware source dir (`include/fonts_gen*.h`, `packages/ui.yaml` for the `LV_*` build flags) |
| `FW_OUT` | output dir (default `renderer/out`): `<density>/fw.mjs`, `fw.wasm`, `manifest.json` |
| `FW_CACHE` | object cache (default `renderer/.cache`) |

**TODO:** vendor LVGL, `lv_conf.h` and the generated fonts into this branch so it builds on its own, without a panel
firmware build next to it.

## Device firmware (Show on device)

The same core + foundations + components also run on real panels, as an ESPHome firmware that shows whatever the
workbench sends (the toolbar's **Show on device**). Facet side (this branch, no device specifics):

| Path | What |
|---|---|
| `core/fw_core.cpp` | shared by every host: themes, registry, `fw::render(screen, comp, props, light, accent)` (the WASM host calls it too) |
| `firmware/fw_device.h` | ESPHome glue: own LVGL screen, `GET /fw/info` (build hashes), `GET /fw/meta`, `POST /fw/show {id, props, theme, accent}`; last request kept in flash; tall content scrolls |
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
| `foundations/*.cpp` | foundation pages (Colour) |
| `core/fw.h` | the component API: `FW_COMPONENT`, `fw::T` colours (generated from tokens/colour.json), `fw::px(dp)`, `fw::font(dp, mono)`, `fw::force_state` |
| `core/fw_core.cpp` | themes, registry, `fw::render` (shared by the WASM host and the device firmware) |
| `core/main.cpp` | host: LVGL + "stages" (one display per canvas), exports `fw_init/meta/stage/stage_free/render/touch/tick/dirty/frame` |
| `core/fonts.cpp` | the panel's generated Facet fonts per density |
| `build.sh`, `watch.mjs` | build and watcher |

Densities: `1.5x` = 720 px panels, `1x` = 480 px panels. Write sizes in dp; `fw::px()` scales.

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
