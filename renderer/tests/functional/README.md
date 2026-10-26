# Functional tests

Hand-written end-to-end tests per component and screen: does it DO what it should, read back from what it renders.
`tools/behaviour.mjs` sweeps every id for generic rules (one sound per tap, disabled fires nothing, scroll limits, lag);
these check each component's own job: a slider clamps at max, a light card's state line follows Home Assistant after
a toggle, a sensor value comes back after a reconnect. Headless on the WASM build (Node, no browser), ~1 s for the lot.

```
node renderer/tools/functional.mjs                 # all, both sizes (720 = 1.5x, 480 = 1x builds that exist)
node renderer/tools/functional.mjs --ids slider -v # one component, every test + its log
node renderer/tools/functional.mjs --probe device  # what a mount shows (texts, clickables, component boxes)
```
Options: `--den 1.5x|1x|all`, `--grep <test name part>`, `--png fail|all|none` (default fail), `--out <dir>`,
`--json <file>`, `--soft` (exit 0 on failures). Build = `FW_OUT` (env or `local.env`), else `renderer/out`.
Output: `out/results.json` (per test x size: ok, fails, log, ms, png) + `out/png/<id>/<size>-<test>.png`.
Workbench page: `/wasm/tests` (hub; Re-run runs it against the live build). Gate: `facet-gate` step `functional`
(non-blocking WARN for now; `fast --ids` runs the ids' tests at 1.5x).

## Adding tests (one file per component: `<id>.test.mjs`)

```js
export default {
  id: 'slider', family: 'controls', about: 'One line: what it is.',
  tests: [
    { name: 'drag past the right end clamps to max', mount: ['slider', { value: 40 }], run(t, { den, S }) {
      const b = t.part('slider');                       // component box + its texts
      const ev = t.drag(null, { from: [b.x + 8, b.y + b.h / 2], toX: S - 2 });
      t.emitted(ev, 'change', '100');                   // event name + detail (string or /re/)
    } },
    { name: 'Home: toggle follows HA', async run(t) {
      await t.device();                                 // the device app on Home (demo deck), or t.device(deckSpec)
      t.ha.set('light.sofa_lamp', 'on', { brightness: 115 });
      t.tap('Sofa table');                              // the fake HA answers the light.toggle call
      t.has(t.part('entity_tile', 'Sofa table').texts, /^off$/i, 'state line');
    }, xfail: 'facet-181' },                            // known failure (board id); XPASS = fixed, remove it
  ],
};
```

- **Mount:** `mount: [comp, props]` or `await t.mount(comp, props)`; `await t.device(deck?)`; `t.update(props)` = the
  update path (fw_update); `t.wait(ms)`.
- **Navigation (device):** `t.send(spec)` = a sent spec (stored, never shown: facet-188), `t.unsend(id)` (revert),
  `t.show(id)` = what POST /fw/show does (`home` = straight Home), `t.stack()` (ids, root first), `t.where()` (the top).
- **Touch:** `t.tap(sel)`, `t.hold(sel, ms?)`, `t.drag(sel, {by: [dx, dy]} | {toX} | {from: [x, y], ...})`. Each returns
  the events + sounds it caused (after settling, HA answers included). Selectors: `'TEXT'` (exact, as drawn: labels
  may be upper case), `/re/`, `{clk: i}`, `{type: 'arc'}`, `{at: [x, y]}`, or a box.
- **Read:** `t.texts(box?)`, `t.part(compId, labelTextOrRe?)` -> `{x, y, w, h, texts}` (the innermost instance holding the
  label), `t.find(sel)`, `t.objs()` (the audit tree: every object's box, type, text, clk, opa, parent), `t.comps()`.
- **Fake HA (`t.ha`):** `set(e, state, attrs)` (`null` attr = HA dropped it), `unavailable(e)`, `disconnect()` (every
  bound entity reads unavailable), `reconnect()` (stored states arrive again), `calls` (service calls sent), `auto`
  (default true: toggle / turn_on / turn_off for light, switch, fan, input_boolean; cover open / close / position / tilt; vacuum start / room clean / pause / dock / suction; select option),
  `lag` (ms before an answer arrives). Unset values fall back to `tools/demo_ha.mjs`.
- **Fake image host (`t.img`, the Image component):** `serve(url, w, h, fill)` (fill `[r,g,b,a]` or `(x, y) => [...]`),
  `status(url, 404)`, `hang(url)` (never answers: the device times out), unknown URLs = 404; `asked` (URLs fetched),
  `stats()` (the cache: cap, bytes, peak, views, shown, evicted, dropped, refused, pending), `leave()` (build another
  screen: every image view goes). Serve before `await t.mount(...)`.
- **Assert (soft, all collected):** `ok`, `eq`, `has(list, want)`, `hasNot`, `emitted(evs, name, detail?)`, `notEmitted`,
  `sounds(evs, [...])` (ticks ignored), `silent(evs)`, `call({service, entity})`.
- What to cover per component: every gesture it answers (tap, hold, drag, slide-off), every prop that changes what it
  does or where things sit (min / max / step, width, act), disabled + unavailable, the update path (shows the new value,
  silently), and for anything bound to HA: the live state after a tap, an outside change, unavailable -> back, empty
  value -> back, disconnect -> reconnect. Name the board card in the test name when a test pins a known bug.
- Do not fix a component to make its new test pass in the same piece unless the brief says so: report the failure.

## Families still to cover (one agent per family)

Done: button, slider + family 1 below (controls), entity_tile, readout (cards; incl. Home light + sensor cards on the
device), device (navigation rules, facet-188), image (cache cap + fallbacks), covers + vacuum screens (WIP features, via
the `spec` component). Shared helpers for family 1: `_controls.mjs` (pixel reads for drawn / not drawn, dial ring points).

1. **Controls + inputs** (done 2026-10-26): toggle, checkbox, radio, segmented, hold_button, icon_button, speaker_button, back_button,
   dial, thermostat_dial, picker_row, check_row, list_row, more_row, focus_ring, scroll_track, bottom_sheet, toast,
   toast_layer.
2. **Cards + tiles:** card, tile, room_tile, camera_tile, stat, big_number, status_block, status_chip, status_dot,
   count_badge, hero_block, level_bar, plant_gauge, plant_row, empty_state, placeholder, qr_block (+ the Home cards
   on the device: climate, media, cover, page cards).
3. **Layout + chrome + device:** device (navigation, Settings, deck changes), header (timer slot collapses with no
   timer: facet-180), screen_chrome, section_header, grid, card_grid, column, split, group, label, icon, time,
   date_line, corner_ticks, tick_ring, arc, legend, card_previews, accent_swatches, mascot_mood.
4. **Charts + weather + sky:** bar_chart, donut, sparkline, rate_strip, clock_face, energy_week, energy_view,
   weather_icon, weather_days, weather_view (next 5 days side padding: facet-185), moon_facts, moon_icon,
   moon_phases, moon_range, moon_view, lightning_radar, lightning_view.
5. **Home domains (HA-driven views):** media_disc, media_view, media_visual (facet-183), cover_visual, covers_view,
   lock_ring, thermostat_view, vacuum_mode, timer_view, calendar_view, event_row, bins_view, print_model, print_view,
   iso_floorplan, overhead_view, agents_view.
6. **Feeds + sport:** f1_car_marker, f1_circuit, f1_flag, f1_race_control, f1_race_status, f1_standings,
   football_pitch, tennis_court, tennis_score, github_list, github_pr, github_view, pr_row.
7. **Spec screens** (renderer/specs/*.json via the device: the screen's HA calls + live values), after 1-6.
