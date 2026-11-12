# AGENTS.md

Facet is firmware for 4 inch ESP32 wall panels that shows and controls a home through Home Assistant.
One C++/LVGL renderer draws every screen, built to WebAssembly (the workbench, the docs site, the tests) and into the ESPHome firmware.

## Build and run

- Needs: Node 24, Python 3 with venv, Emscripten 6.0.13 (`em++` on PATH or `$EMSDK`). Once: `cd renderer/tools && npm ci`.
- `renderer/build.sh` builds both densities; `renderer/build.sh 1.5x` the 720 px panels, `1x` the 480 px panel. It only builds, never checks.
- `node renderer/watch.mjs` rebuilds 1.5x on every save. 1x builds only after a minute of quiet (or `kill -USR1 <pid>`).
- `renderer/check.sh fast --ids <ids>` before every commit; `gate` is what CI runs (budget 300 s on a built tree); `nightly` is ASan and fuzz. Every tier checks the build that is there: build first.
- Docs site: `FW_FEATURES=docs renderer/build.sh`, then `cd site && npm ci && npm run build`.
- Output paths come from `local.env` at the repo root (untracked; `FW_OUT`, `FW_CACHE`). Default `renderer/out`.

## Before a pull request

Run all of these on the current source. Each must exit 0 and print no WARN line:

1. `renderer/build.sh`
2. `renderer/check.sh gate`. A failed functional test fails the gate. Read each FAIL and WARN line (the log path is on the line).
   The gate's workbench step tests the workbench at `WORKBENCH_URL` (default `http://localhost:4322`), which serves the checkout that started it. Start it from this tree first (`workbench/run.sh`), or the step fails. CI skips it (`CI` set).
3. If anything the site shows changed (`site/`, a component's meta or header comment, `renderer/specs/`, tokens):
   `FW_FEATURES=docs renderer/build.sh && (cd site && npm run build)`
4. `git status`: only your change. Nothing `.new` in `renderer/tests/golden/`, no build output, no stray files.

Then follow `.github/pull_request_template.md`: one thing per pull request, screenshots at 720 and 480 for anything visible.

## The contract

- `renderer/CONTRACT.md` is the rule book (C, P and L ids). Cite rule ids in reviews and commits.
- Every component and screen is on it: listed in `renderer/catalogue/contract.json`, checked by `tools/contract_lint.mjs` and the rules.
  An entry is `"components/<id>": {"commit": "<short hash>", "at": "<ISO time>"}`, the commit that put it on the contract. The tools read only the key; before that commit exists write `{"commit": "", "at": ""}` and the maintainer fills it in after the merge.
- A new component: `CONTRACT.md` section 13 (recipe, checklist, definition of done) and `renderer/README.md` "Write a component".
  - Copy `components/card.cpp` with `tests/functional/card.test.mjs`: its test uses every helper in `tests/functional/_release.mjs` (bounds, golden, fuzz, monkey, leak). `list_row`'s test does not use them.
  - Its contrast pairs go in `renderer/tokens/colour.json` under `contrast.pairs`, one block with `section` = the component id.
  - Bounds at page insets: `bounds(t, [null, 0, 8, 16, 24, 32])` from `_release.mjs` (as `light_screen.test.mjs`).
  - Text alignment: `fx::use(label, "text-mid")` centres text (core/fx_core.cpp). There is no end (right) aligned style yet: adding one is a core change, say so in the pull request.
- Every value from a token, sizes on the 4 px grid, works at 720 and 480 in dark and light.

## Where things live

| Path | What |
|---|---|
| `renderer/core/` | engine, component API (`fx.h`), screen specs (`spec.h`), Home Assistant data (`ha.h`), feeds |
| `renderer/components/` | one component per file |
| `renderer/screens/`, `renderer/specs/` | system screens in C++; factory screens as JSON specs, the factory deck |
| `renderer/tokens/` | every colour, size, motion and sound; a new token is picked up by the generators (`tools/gen_*.mjs`) on the next build |
| `renderer/features.json` | what each release ships (docs and firmware flags) |
| `renderer/tests/` | rules, functional tests, behaviour cases, goldens |
| `renderer/catalogue/` | `contract.json`, plus generated `catalogue.json`, `schema.json`, `spec.schema.json` |
| `firmware/` | the ESPHome component and its bundler |
| `site/` | the docs site (Astro), generated from the renderer |

## Rules that bite

- Goldens (`golden()` in `tests/functional/_release.mjs`): a changed or missing golden fails and writes a candidate `.new.565.gz` + `.new.png`. With `{ optIn: true }` the candidates go to `<tmp>/facet-candidates/<suite>/` unless `CHECK_WRITE_CANDIDATES=1`, which writes them beside the goldens in `tests/golden/<suite>/`. A new component's goldens start as candidates: put the candidate PNGs in the pull request for a maintainer to review. Only after that review does a run with `FW_GOLDEN_ACCEPT=1` turn `.new` into the golden. Never accept your own; never leave a `.new` file in `tests/golden/`.
- No typed counts of screens, components, foundations, tokens, routes, settings or bindings in prose. Use `<Count>` (`site/src/lib/counts.ts`).
- The docs check (`site/scripts/check-dist.mjs`, part of `npm run build`) fails on words run together, typed counts, broken links, and a page with no `.md` twin or missing from `llms.txt` / `llms-full.txt`.
- A clean tree: builds write to ignored folders only. Commit regenerated files the checks produce (`renderer/catalogue/`) with the change that caused them.
- The watcher builds one density on save (1.5x). Before you say 480 works, run `renderer/build.sh 1x` and check it.
- A new source file belongs to one feature: `node renderer/tools/features.mjs check`.
- A WARN line is a failing test, even when the tier exits 0.
- Generated files are not edited by hand: `renderer/themes/facet.json`, `renderer/catalogue/*` except `contract.json`, `site/src/data/`.

## Don't

- Add a rule (here, in `CONTRACT.md`, a lint or a check) without deleting one.
- Work on themes or on unreleased (`wip`) features unless asked.
- Put anything private in the repo: machine paths, host names, addresses, tokens, personal data.
