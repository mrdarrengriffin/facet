# Bench

Proof in numbers for every Facet component and screen: frame cost, redrawn area and memory, measured
headless in the WASM build at each panel's size and checked against a budget per device.

| File | What |
|---|---|
| `core/bench.cpp` | WASM-only probes: per refresh, LVGL `RENDER_START..READY` (the same window as the panels' `GET /fw/perf`), flush time, flushed px and areas, full-screen redraws; heap (malloc) and LVGL object count; `fw_bench_nav` drives the device runtime. Not in the panel bundle. |
| `bench/suite.json` | Scenarios and cases: every component and screen gets `idle` + `animate` with default props (auto), plus named cases (weather icon sizes, drags, sheet loop, navigation) and the `anchors`. |
| `bench/budgets.json` | Per device: size, density, loop cap, budget (fps, p95 frame ms, idle redraw, heap, objects) and the anchors (real `/fw/perf` numbers). **Provisional.** |
| `tools/bench.mjs` | The runner (Node, no browser): one fresh WASM instance per case, fixed 16 ms virtual ticks, JSON out. |

## Run

```
# WASM estimate (add --repeat 3 for medians, --baseline <run.json> for the regression check, --props '{"fades":true}' to override)
cd renderer && heavy node tools/bench.mjs [--device guition-4848s040|waveshare-p4-86|all] [--case <id|component|prefix|tag:weather>,...]
                                          [--scenario idle|animate|drag|navigate] [--out run.json] [--list] [--quiet]
```

On the glass: `node tools/bench.mjs --panel guition-4848s040 --host <ip> --out panel.json` (same cases, about 6 min per panel),
then `node tools/bench.mjs --calibrate panel.json wasm.json [--write]` (estimate error, and a fit over every case
written to `budgets.json` `measured` + `build_factor`). The full WASM suite takes about 8 s for both devices. The workbench's Bench page runs it as well.

## What the numbers mean

- **WASM ms** = host CPU per refresh (render + flush), Node with TurboFan only, after a warm-up pass.
- **Panel ms (estimate)** = `a + b × WASM ms` per device. The line is fitted on every run from the anchors: scenarios
  measured on the real panel with `/fw/perf` (Motion loops 2026-10-21, mascot bench 2026-10-22). A plain ratio
  over-reads big redraws, because the panels have a fixed per-frame cost (Guition about 6.5 ms, Waveshare about 1 ms).
  Fit error on the anchors is about ±4 ms (Guition) and ±2 ms (Waveshare).
- **Panel fps** = min(loop cap 53, redraw rate while moving, 1000 / panel ms). The redraw rate is counted over the span
  from the first to the last redraw, so a 160 ms sheet opening is not reported as "5 fps".
- **Redrawn** = flushed pixels per redraw as a share of the screen; **full redraws** = refreshes that flushed the whole
  display (the 2026-10-22 Icons page lag showed as 63 of 250).
- **Heap** = malloc growth of the case after it settles (WASM, a proxy for the LVGL heap on the panel).
- **Build ms** = `fw_render` + the first frame in WASM. It isn't mapped to the panels yet (no anchor); `budgets.json`
  `build_factor` turns the flag on once Phase 2 measures it.
- **Checks per device** (`budgets.json` `checks`). Waveshare = absolute: fps `fps_min`, p95 `frame_ms_max`. Guition (it runs as fast
  as it can) = relative: **regression** (WASM ms more than `regression_pct` over the
  accepted baseline, median of `--repeat 3`; `--baseline <run.json>`), **pacing** (panel p95 over `pacing_ratio`
  x the average while it moves = judder). Every check is computed for both devices and shown; only the listed ones flag.
- Over budget: fps under `fps_min` while the content wants to move faster, p95 panel ms over `frame_ms_max`, a still
  case (not `live`) redrawing while idle, heap or objects over budget.

## Media spin

The turning album art (components/media_visual.cpp): cases tagged `media-spin` (`--case tag:media-spin`), each with
`"img": true` (every picture the case asks for is answered at once with a 640 x 640 cover with fine detail, so the
decode, scale and drawing cost what a real cover does). Measured `--repeat 3`, estimates from the anchors' fit:

| Case | Guition est. ms / frame, fps, CPU | Waveshare est. ms / frame, fps, CPU |
|---|---|---|
| vinyl, its label turning | 10.5, 31, ~33 % | 2.6, 31, ~8 % |
| CD with art, spin off (the mark turns) | 2.4, 53, ~13 % | 0.6, 53, ~3 % |
| CD with art, spin on (the default) | 30.1, 13 (capped at 12), ~36 % | 9.6, 31, ~30 % |

What it took: the art turned once per drawn angle into one buffer the art's size (bilinear, fixed point; its round mask
copied as it is) and drawn as a plain picture: LVGL's own image transform cost twice as much (Guition CD 58 ms); a
ring a partial redraw lies wholly inside or outside of is skipped (the record's label among its grooves: 22 -> 11 ms);
the redraw is the art's circle only, at most 30 a second (the CD's on the Guition: 12). The CD on the Guition: its
art turned nearest-pixel while it turns (about 15 degrees a frame there; the frame it stops on is smoothed), the
middle its clear hub covers not turned, the bilinear 2 x 2 block without per-pixel clamps: 37 -> 30 ms. Tried and
dropped: the circle redrawn as six chord-wide bands instead of its square (41 ms: each area costs more than the
corners it saves). Before: the turn itself 14 of the 37 ms, the rings and the clear hub drawn over it 6.5. Not yet
measured on the glass (no `/fw/perf` run of a build with it).

## Phase 2: the panel tests itself (built and flashed 2026-10-22: `firmware/fw_bench.h`)

The same `suite.json` cases run on the Guition and the Waveshare, giving the real numbers that calibrate the estimate.
The host sends **one batch**; the panel runs it on its own screen, one case after another (a static "Bench n/N" Status
chip on the top layer, changed between cases only), measures itself, keeps the results in RAM and hands back the report
at the end. Nothing polls the panel while it measures.

- `POST /fw/bench {run, theme?, timing?, cases: [[case, component, props, scenario, path?, to?], ...]}` returns
  `200 {ok, run, total, eta_s}` at once (409 while a batch runs, 400 over `FW_BENCH_MAX` = 96 cases; body up to 32 KB;
  202 would come out as 500 from ESPHome's web server). A state machine on an `lv_timer` in the main loop runs it.
- `GET /fw/bench` returns `{run, state: idle|running|done|interrupted, done, total, results}` with one row per case:
  `panel{frames, active_ms, redraw_fps, fps (start to start, wall time), slow, build_ms, render_ms_avg|p95|max, px_avg,
  px_pct, areas_avg, full_redraws}` + `mem{heap_kb, internal_free_after, psram_free_after, internal_largest,
  psram_largest, objs}`. `interrupted` = the panel restarted during a batch (RTC memory), so nothing waits forever.
- Frame window = LVGL `RENDER_START..RENDER_READY` (render + flush, as `/fw/perf`); p95 from a ring of the last 512
  frames (uint16, 10 us units). Build = `fw::render` to the first frame. Drags play `path` through a second LVGL
  pointer indev (the same input path as a finger); `navigate` = `fw::dev::go` / `back`.
- **Never read SPIRAM `largest_free_block` while frames run** (a heap walk, 1.7-2.3 ms with interrupts off, shifts the
  Guition RGB picture): memory is read between cases only.
- At the end, the panel shows what it showed before the batch. Boot safety (fw_device.h): after a crash reset, the
  saved page is not shown again; the device home is.

**Host:** `bench.mjs --panel guition-4848s040|waveshare-p4-86 --host <ip>` (or the workbench's Bench page, target = the panel) sends the
batch, sleeps the panel's `eta_s` (~4.5 min for the full suite), then GETs every 10 s until done. Cases the firmware
does not register, `browser_only` cases and `panel_skip` are listed as skipped. Then `--calibrate panel.json
headless.json [--write]` reports the estimate error and fits `a, b` over every moving case (`budgets.json` `measured`,
`build_factor`).

**Results 2026-10-22:** the 5 old anchors put a fixed ~6 ms per frame on the Guition that isn't there (median error
+315 %). Fitted on real cases: Guition `panel ms = 0.08 + 102.6 x WASM ms`, Waveshare `0.34 + 12.6 x`. Checked on a
second, independent batch, the median error is 14 % (Guition) and 18 % (Waveshare), p90 38 % / 57 % (small cases and
big blends are the outliers).

**Cost:** workbench firmware only (production screens untouched): the batch and results in RAM (a few KB), a 1 KB frame
ring, a second pointer indev, one 5 ms lv_timer.

## Stress / capability suite (2026-10-22, round 3)

Benchmarks the hardware and the screen, not our components (how many instances fit on a screen
before it starts lagging). The load is `bench/stress.cpp` (item `stress`, section "bench", not in the workbench nav);
the cases are generated from `suite.json` `stress` (tag `stress`, never in the full suite; `--case tag:stress`, or Tests =
Stress on the Bench page). Every target runs it: headless (estimates for both panels), this browser, each panel (its own
batch, split into batches of at most 64 cases, ramp groups never split).

- **Ramps to the knee:** n instances of 48 dp (shrinking only once they no longer fit) of `icons` (the animated Weather
  icon), `arcs` (turning lv_arc), `rects` (sliding boxes), `labels` (a number changing every frame), n in x1.5 steps
  from 1 to 1024. A group stops one step past the knee (under `stop_fps` = 20); the card reads the knee at full rate
  (within 10 % of the target's own refresh ceiling), 30 and 20 fps. Knees use frame cost, not how often the load asks
  for a frame (headless: capacity fps from the estimate; panel / browser: the real frame rate).
- **Animation boundaries:** one object whose side is 5 % to 100 % of the screen, for `move`, `fade`, `colour` (a step
  every frame) and `rotate` (transform_rotation, a layer render), plus three speeds (loop 2000 / 500 / 125 ms at 35 %).
- **Raw capability:** `refresh` (an 8 x 8 change: the ceiling), `fill` (opaque full screen, ms and MB/s), `blend` (50 %
  over the whole screen), `blit` / `blit_alpha` (a full-screen RGB565 / RGB565A8 image), `text` at 12 / 24 / 48 dp
  (glyphs per ms; glyphs = rows x columns as stress.cpp lays them out), and on the panels the flush share of a frame
  (`flush_ms_avg`: FLUSH_START..FINISH + any wait for the bus).
- **Memory:** `objects` n = 64 to 4096 static rows; bytes per object (panel: internal / PSRAM from `heap_caps`, between
  cases only), stopping before `reserve` (internal 64 KB, PSRAM 1 MB) using the last step's bytes per object.
- **Touch to frame:** `touch` scenario, 5 taps through the bench pointer: press to the first rendered frame after it.

The workbench's capability card turns a run into knees, the fps-by-size curve
per motion, raw rates, memory and touch, with the change against the previous stress run on the same target.

**Results 2026-10-22 (real, on the glass):** Guition: 62 fps ceiling, ~16 animated icons / 6 arcs / 12 boxes / 8
changing numbers at full rate; one slide or fade at full rate up to 35 % of the screen, rotation 10 %; full-screen
fill 71 ms (7 MB/s), 95 % of it the flush (90 deg software rotation); text 14 glyphs/ms at 12 dp; touch to frame
82 ms. Waveshare: 63 fps, ~32 icons / 12 arcs / 16 boxes / 16 numbers at full rate; slide / fade / colour step to
75-100 %, rotation 10 %; fill 5.6 ms (184 MB/s), image with alpha 94 ms; touch 15 ms. Objects ~200 B each, in PSRAM.
The pixel-based estimate is good for single objects and raw fills, but too optimistic for many small moving objects
(each LVGL object has a per-frame cost the fit doesn't see): the Waveshare's knees are 2-8x lower than estimated.
Objects ramp capped at 256: building thousands in one go blocks the panel past the task watchdog (fw_bench.h now
also stops a ramp whose step takes over 1 s to build).
