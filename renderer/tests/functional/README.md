# Functional tests

Hand-written end-to-end tests per component and screen: does it DO what it should, read back from what it renders.
The `behaviour` rule (`tests/rules/behaviour.mjs`) sweeps every id for generic rules (one sound per tap, disabled fires nothing, scroll limits);
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
Workbench page: Tests (Re-run runs it against the live build). Gate: `check.sh gate` step `functional`
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
    }, xfail: 'why it fails today' },                   // known failure; XPASS = fixed, remove it
  ],
};
```

- **Mount:** `mount: [comp, props]` or `await t.mount(comp, props)`; `await t.device(deck?)`; `t.update(props)` = the
  update path (fw_update); `t.wait(ms)`.
- **Navigation (device):** `t.send(spec)` = a sent spec (stored, never shown), `t.unsend(id)` (revert),
  `t.show(id)` = what POST /fw/show does (`home` = straight Home), `t.stack()` (ids, root first), `t.where()` (the top).
- **From HA:** `t.toast({title, body, kind, icon, duration})` = what the panel's `facet_toast` action puts on the
  `facet.toast` pair (returns the events). `t.setting(id)` = a device setting's value now; `t.setting(id, v)` sets it. `t.device()` starts with Idle after = 0 (no idle clock while a test waits).
- **Rotate screen:** `t.turn(deg)` = the firmware's rotate hook (ESPHome turns the picture, LVGL stays upright);
  `t.glass([x, y])` = where a UI point shows on the glass, `t.tapGlass(px, py)` = a finger there (ESPHome's touch map).
- **Tier:** `tier: 'nightly'` on a suite = it runs only in `check.sh nightly` (`functional.mjs --tier nightly`), never
  in fast or gate, whatever `--ids` names (theme_apply: it walks every theme).
- **Time:** `t.since()` = ms since the mount was built (mounts settle ~4.5 s), `t.until(ms)` = wait until then.
- **Touch:** `t.tap(sel)`, `t.hold(sel, ms?)`, `t.drag(sel, {by: [dx, dy]} | {toX} | {from: [x, y], ...})`. Each returns
  the events + sounds it caused (after settling, HA answers included). Selectors: `'TEXT'` (exact, as drawn: labels
  may be upper case), `/re/`, `{clk: i}`, `{type: 'arc'}`, `{at: [x, y]}`, or a box.
- **Read:** `t.texts(box?)`, `t.part(compId, labelTextOrRe?)` -> `{x, y, w, h, texts}` (the innermost instance holding the
  label), `t.find(sel)`, `t.objs()` (the audit tree: every object's box, type, text, clk, opa, parent), `t.comps()`.
- **Fake HA (`t.ha`):** `set(e, state, attrs)` (`null` attr = HA dropped it), `unavailable(e)`, `disconnect()` (every
  bound entity reads unavailable), `reconnect()` (stored states arrive again), `calls` (service calls sent), `auto`
  (default true: toggle / turn_on / turn_off for light, switch, fan, input_boolean; cover open / close / position / tilt; vacuum start / room clean / pause / dock / suction; select option),
  `lag` (ms before an answer arrives). Unset values fall back to `tools/demo_ha.mjs`.
- **Fake image host (`t.img`, pictures: the album art):** `serve(url, w, h, fill)` (fill `[r,g,b,a]` or `(x, y) => [...]`),
  `status(url, 404)`, `hang(url)` (never answers: the device times out), unknown URLs = the demo players' art
  (`tools/demo_art.mjs`) or 404; `asked` (URLs fetched), `stats()` (the cache: cap, bytes, peak, views, shown, evicted,
  dropped, refused, pending). Serve before `await t.mount(...)`.
- **Assert (soft, all collected):** `ok`, `eq`, `has(list, want)`, `hasNot`, `emitted(evs, name, detail?)`, `notEmitted`,
  `sounds(evs, [...])` (ticks ignored), `silent(evs)`, `call({service, entity})`.
- What to cover per component: every gesture it answers (tap, hold, drag, slide-off), every prop that changes what it
  does or where things sit (min / max / step, width, act), disabled + unavailable, the update path (shows the new value,
  silently), and for anything bound to HA: the live state after a tap, an outside change, unavailable -> back, empty
  value -> back, disconnect -> reconnect. Name the board card in the test name when a test pins a known bug.
- Do not fix a component to make its new test pass in the same piece unless the brief says so: report the failure.

## Still to cover

Every component and screen on main gets `tests/functional/<id>.test.mjs` (a spec screen: `<id>_screen.test.mjs`). Without
one today: accent_swatches, arc, bins_art, bins_next, calendar_month, calendar_row, card_previews, clock_orbit, column,
corner_ticks, count_badge, cover_visual, donut, empty_state, group, hero_block, icon, label, legend, mascot_mood,
media_hero, media_visual, moon_icon, progress_ring, qr_block, rate_strip, sky_radar, slider_row, sparkline, split,
status_line, tick_ring, timer_chip, vacuum_robot, weather_icon; the system screens ha_link, home, screen_set, screens,
settings, setup; the spec screens living_room and weather_days. Shared helpers for controls: `_controls.mjs` (pixel
reads for drawn / not drawn, dial ring points).
