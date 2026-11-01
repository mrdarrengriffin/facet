# Facet workbench

A local web app for working on Facet: every component, foundation and screen of the current renderer build, live and
at true size, with its props, variants, states, pixel grid, tap areas and outlines; the screen editor; sending specs
and showing components on a real panel; the functional tests and the golden review.

## Run it

```bash
workbench/run.sh
```

Then open `http://<this machine>:4322/wasm`. `run.sh` installs what is missing (`renderer/tools`, `workbench`), builds
the renderer at both densities when there is no build yet, and starts the server. After that, `node renderer/watch.mjs`
in another terminal rebuilds on every save and the open page follows. You need what the renderer needs (Node 24,
Python 3, Emscripten: `renderer/README.md`).

## Settings

All optional, from the environment:

| Variable | What |
|---|---|
| `HA_URL`, `HA_TOKEN` | a Home Assistant and a long-lived access token: bound screens show its real states and the actions reach it. Without them the workbench uses the fake Home Assistant (`renderer/tools/demo_ha.mjs`) and actions are accepted but not sent |
| `FACET_PANELS` | panels to show things on and send specs to: `<name>=<host>@<board-id>`, comma separated, e.g. `office=192.168.1.40@waveshare-p4-86`. A panel runs the workbench firmware (`esphome/`) with "Allow AI agents" on; the workbench reads its agent token from Home Assistant |
| `FW_OUT`, `FW_CACHE` | where the renderer build is (as `renderer/build.sh`; `local.env` at the repo root works too) |
| `PORT` | default 4322 |

## Pages

| Page | What |
|---|---|
| `/wasm` | the build: Foundations, Components, Screens, each live, with props, variants, Pixel grid, Tap areas, Components outlines and the Layout lab; Show on device |
| `/wasm/editor` | the screen editor (the same code a panel serves from flash, built by `scripts/editor-device.mjs`) |
| `/wasm/editor-lab` | the prototype of the next editor |
| `/wasm/specs` | check a screen spec or the deck against the build, send it to a panel, revert |
| `/wasm/tests` | the functional tests, run against the live build, with a picture per test |
| `/wasm/goldens` | golden candidates the tests wrote: compare and accept or reject |
| `/wasm/release` | what each feature ships (`renderer/features.json`) |
| `/wasm/contract` | `renderer/CONTRACT.md`, readable |

## Where things live

| Path | What |
|---|---|
| `src/wasm/` | the client: the shell and router (`app.ts`), the WASM engine (`engine.ts`), the views (`pixgrid.ts`, `taps.ts`, `outline.ts`, `lablayout.ts`, `sound.ts`, `theme-editor.ts`), the editor (`editor*.ts`) |
| `src/pages/wasm/` | the pages |
| `src/pages/api/wasm/` | build status, specs, show on device, tests, goldens, sound, motion, screen sources |
| `src/pages/api/panel/` | Home Assistant for the renderer's binds: states, actions, asks, the panel link's reads (a real HA or the fake one) |
| `src/pages/facet-wasm/` | serves the renderer build at `/facet-wasm/` |
| `src/lib/` | Home Assistant (`ha.ts`), panels (`panels.ts`, `panel-auth.ts`), screen sources, status from git, repo paths |
| `scripts/editor-device.mjs` | builds the editor a panel serves from flash (`firmware/fw_device.h` "Web UI") |
