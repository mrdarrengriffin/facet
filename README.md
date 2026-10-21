# Facet WASM workbench

A fresh, minimal place to build Facet components from scratch in C++/LVGL, compiled to WebAssembly and shown in a
browser workbench page. This is the orphan branch `wasm` of the Facet repo: it has no history shared with `main`
(docs site) and holds only the workbench and the components built in it.

The page (served by the host app that embeds this build) has a left nav (Foundations / Components / Screens, filled
from what the build registers), and per component a preview, a variants grid, a playground (props -> live re-render),
Dark/Light and device toggles.

## Workflow: WIP -> approved -> one commit per component

- Every component starts as **WIP**: a new `components/<id>.cpp`, untracked.
- Iterate on it with the watcher running; the page updates on save.
- When it is approved, commit **that one file** on `wasm` (`git add components/<id>.cpp && git commit`). It then shows
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

Rendering is nearest-neighbour at every zoom; touch is mapped from the canvas box back to panel pixels, so it is
correct at any zoom. The variants grid is always 1:1.

## Fast loop

```
node watch.mjs
```

Save a `.cpp`/`.h` in `components/` or `core/` -> incremental rebuild of both densities (a few seconds) -> the page
notices the new `manifest.json` within a second and re-renders in place (playground values kept). One-off build:
`./build.sh [1.5x|1x]`. First build is slower (LVGL objects, cached).

### Paths (`local.env`, untracked)

`build.sh` reads these from the environment or a `local.env` next to it:

| Var | What |
|---|---|
| `PANEL_BUILD` | ESPHome build dir of the panel firmware (LVGL tree, `lv_conf.h`, ArduinoJson) |
| `FW_PANEL` | panel firmware source dir (`include/fonts_gen*.h`, `packages/ui.yaml` for the `LV_*` build flags) |
| `FW_OUT` | output dir (default `./out`): `<density>/fw.mjs`, `fw.wasm`, `manifest.json` |
| `FW_CACHE` | object cache (default `./.cache`) |

**TODO:** vendor LVGL, `lv_conf.h` and the generated fonts into this branch so it builds on its own, without a panel
firmware build next to it.

## Layout

| Path | What |
|---|---|
| `components/*.cpp` | one component per file (the only place new work goes) |
| `core/fw.h` | the component API: `FW_COMPONENT`, `fw::T` colours (Facet tokens, both themes), `fw::px(dp)`, `fw::font(dp, mono)`, `fw::force_state` |
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
