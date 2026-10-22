# Bench

Proof in numbers for every Facet component and screen (facet-153): frame cost, redrawn area and memory, measured
headless in the WASM build at each panel's size and checked against a budget per device.

| File | What |
|---|---|
| `core/bench.cpp` | WASM-only probes: per refresh, LVGL `RENDER_START..READY` (the same window as the panels' `GET /fw/perf`), flush time, flushed px and areas, full-screen redraws; heap (malloc) and LVGL object count; `fw_bench_nav` drives the device runtime. Not in the panel bundle. |
| `bench/suite.json` | Scenarios and cases: every component and screen gets `idle` + `animate` with default props (auto), plus named cases (weather icon sizes, drags, sheet loop, navigation) and the `anchors`. |
| `bench/budgets.json` | Per device: size, density, loop cap, budget (fps, p95 frame ms, idle redraw, heap, objects) and the anchors (real `/fw/perf` numbers). **Provisional.** |
| `tools/bench.mjs` | The runner (Node, no browser): one fresh WASM instance per case, fixed 16 ms virtual ticks, JSON out. |

## Run

```
cd renderer && heavy node tools/bench.mjs [--device guition|waveshare|all] [--case <id|component|prefix|tag:weather>,...]
                                          [--scenario idle|animate|drag|navigate] [--out run.json] [--list] [--quiet]
```

The full suite takes about 8 s for both devices (174 results). The host hub's `/wasm/bench` page runs it as well
(`/api/wasm/bench`) and keeps the history.

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
- Over budget: fps under `fps_min` while the content wants to move faster, p95 panel ms over `frame_ms_max`, a still
  case (not `live`) redrawing while idle, heap or objects over budget.

## Phase 2: firmware bench endpoint (spec, not built)

Goal: the same `suite.json` scenarios run on the Guition and the Waveshare, giving real numbers that replace the
5 anchors with a fit over every case, and calibrate memory and build time.

**Endpoint (workbench firmware, `firmware/fw_device.h` + a new `firmware/fw_bench.h`):**

- `POST /fw/bench {id, props?, theme?, accent?, scenario, settle_ms?, window_ms?, path?, to?}` returns `202
  {run: n}` at once. The run is a state machine on an `lv_timer` in the main loop, so it never blocks the HTTP task.
  The body has the same keys as a suite case. Limits: window ≤ 10 s, one run at a time (409 otherwise), 8 KB body.
- `GET /fw/bench` returns `{run, state: idle|running|done|error, result}`, where `result` has the same keys as a bench.mjs
  row: `wasm{}` becomes `panel{}`: `frames, active_ms, redraw_fps, render_ms_avg|p95|max, flush_ms_avg, px_avg,
  px_pct, areas_avg, full_redraws, build_ms`, plus `fps` (start-to-start, real wall time) and `slow` (missed refresh
  slots, as `/fw/perf`).
- **Per-frame data without RAM growth:** p95 comes from a 64-bucket log histogram of render µs (or a ring of the last 512
  frames as uint16 in 10 µs units, 1 KB). px and areas are sums.
- **Memory:** `heap_caps_get_free_size` + `largest_free_block` for INTERNAL and SPIRAM before the build, after it
  settles, and at the end of the window. The LVGL object count uses the same walk as `fw_bench_mem`.
  **Never read SPIRAM `largest_free_block` while frames run** (a heap walk, 1.7-2.3 ms with interrupts off, shifts the
  Guition RGB picture: Tessera KEPT_PAGES.md): SPIRAM largest block only before the build and after the window.
- **Build:** `fw::render` to the first `RENDER_READY` in µs (this calibrates `build_factor`).
- **Input:** a second LVGL pointer indev, "bench", whose read_cb plays the `path` (fractions of the screen) over
  `drag_ms`, so drags go through the same LVGL input path as a finger. `navigate` calls `fw::dev::go / back`
  (move `fw_bench_nav` into `core/device.cpp` as `dev::bench_nav` so both hosts share it).
- **Quiet window:** no ESP_LOG at INFO or above, and no HTTP polling of the panel while a run is going (the host waits
  `settle + window + 500 ms`, then GETs). The `/fw/perf` counters are left alone.

**Host:** `bench.mjs --panel guition|waveshare` takes the panel IP from the hub's `wasm-show` targets, sends each
case (`POST /fw/bench`, wait, `GET`) and writes a run with `source: "panel"`. A full suite is about 60 cases × ~1.6
scenarios × ~4 s ≈ 6-7 min per panel. Then `--calibrate` fits `a, b` over every case measured on both sides, writes it to
`budgets.json` (`measured: {a, b, n, at}`; estimates use it instead of the anchors) and sets `build_factor`. The
`/wasm/bench` page gets a Panel column (measured) next to the estimate, and calibration shows the residuals.

**Cost / risk:** about 4-6 KB of flash and about 2 KB of RAM on the workbench firmware only, with no effect on
production screens. Flashing is Darren's call (the Waveshare has been offline since 2026-10-21 18:00).
