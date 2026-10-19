# Changelog

All notable changes to Facet are written down here. The format is loosely based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- 3D print: a 3D print screen. The model sits in an isometric view and fills up from the bed
  as the layers go: printed part solid in flat tones, the newest four layers in the accent, the
  rest a ghost. The hero is the printer's own %. The model is a voxel grid built from sliced
  gcode (`fromGcode`), an STL (`fromStl`) or a shape function (`fromShape`) and drawn in
  software (`render`), so a server can send the same frames to a device. `PanelData` gains
  `print`; the mock house prints a lighthouse; new screen icon `print`.
- `@facet/settings`: one schema for every personalisation setting (theme, accent, text
  size, UI sounds, sound pack, volume, clock face, idle, transition, toast position). The
  build checks it (accent contrast in both themes, defaults, sound packs) and writes the
  ESPHome entities plus web_server groups, so the panel, Home Assistant and the docs share
  one list. Five accent sets: Ember (default), Rose, Violet, Teal and Mono.
- The full token set in `@facet/tokens`: colours, a screen type scale and a component type
  scale, the 8 px screen layout, sizes, motion, toasts and sound settings, from one
  `tokens.json`. The build also writes ESPHome YAML (substitutions, fonts, LVGL styles).
- `@facet/icons`: 30 UI icons as master SVGs, 12 screen icons, 17 animated weather icons and
  a manifest.
- `@facet/ui`: every component and screen as plain TypeScript factories. Screens read a small
  observable store and call actions. Screens: Home, Agents, Media, Light, Clock (five faces),
  GitHub, Energy, Weather, Lightning and Timer, plus toasts and an idle clock.
- `@facet/sounds`: procedural UI sounds in six named packs (Keycap by default, plus Halo,
  Arcade, Graphite, Crystal and Felt), chosen with `setPack()`, and hooks that play them on
  real taps only.
- Screen transition option: `createDevice({ transition })`, `rise` (default) fades the new
  screen's rows in from 8 px below, header first, or `instant` for slower devices. Timings in
  `motion.transition.rise`; the demo has a switch for it.
- Home: a Timer card, first in the grid, shown only while a timer is running (time left, a
  ring that empties, the end time). Tap it for the Timer screen.
- Energy: "Last 7 days" opens a week view: the week's cost as the hero, use per day against
  the week before, and a pair of bars for each day (tap one to read it). `EnergyData` gains
  `days` (daily totals, today last).
- Moon: a Moon screen (the moon drawn from its phase as the hero, one line under it, and a
  details view with how much is lit, the age, moonrise and moonset, the next full and new moon
  and the next four phases) and a Moon card on Home. `moonIcon(frac)` in `@facet/icons` draws
  the phase on the weather grid; `moonData(now, { lat, lon })` in `@facet/ui` works it all out
  on the device (phase times to about a minute, rise and set to a few minutes). New colour
  tokens `moon-lit`, `moon-dark`, `moon-edge`; `PanelData` gains `moon`.
- Site: a Sounds page (`/docs/sounds/`) with every sound in every pack playable live.
- Screen pages: options under each live preview change the mock house's data (playing or
  paused, brightness, timer state, pull requests, price band, weather, storm, agents per mood,
  clock face, toasts). `@facet/demo-data` gains scene helpers (`mediaWith`, `agentsWith`,
  `timersWith`, `githubWith`, `energyWith`, `weatherWith`, `lightningWith`) and
  `house.setScene()` for the parts that update live.
- `@facet/demo-data`: a fictional mock house.
- Media visuals: the Media screen can show a cassette, CD, vinyl record or reel-to-reel deck
  in place of the disc (`mediaScreen(ctx, { visual })`, `setVisual()`), drawn to the real
  sizes and turning at the real speeds. Also on their own as `mediaVisual()`. `MediaState`
  gains `art`, the mock tracks have made-up covers, and the screen page has a Visual option.
- Site: a UI explorer (`/components/`), a full demo (`/demo/`), a docs page with a live
  preview for every screen, and the new Facet mark, favicons, web manifest and social card.

### Changed

- Idle: after `idleAfter` (default 60 s) the device shows the clock and dims to
  `idleBrightness` (default 0.7); the next touch goes to Home. A toast wakes the dimmed screen.
  The demo uses the default minute.
- `createButton` is replaced by `button()` and the rest of the component set.
- Empty states: data that hasn't arrived reads "loading" for the first 10 s after the store
  is made (`LOADING_MS`, `store.loading()`, `missing(store)`), then "no data".
- Colour tokens follow the panel names: `line-strong` is now `line-hi`, `info` is now `blue`.
- GitHub list and PR detail, toasts and the speaker sheet follow the screen layout: the list
  uses section labels and the same hairline rows as the summary; the detail has a 400 px text
  column, the QR block on the right and up to 10 checks in two columns; toasts are 656 × 120
  at the screen inset; picker rows are 96 px hairline rows under a 48 px sheet head. New
  `layout` tokens (`pr-col-w`, `check-row-h`, `section-gap`, `toast-*`, `sheet-*`); the
  ESPHome YAML now carries the layout as `l_*` substitutions.

### Fixed

- The slider, light dial and toast swipe no longer get stuck when the page has a text
  selection over them (the browser started a native drag and cancelled the pointer).

## [0.1.0] - 2026-10-19

### Added

- Project skeleton: pnpm workspace, shared TypeScript config, CI.
- `@facet/tokens` with a starter set of colours (dark and light), type scale, spacing, radius
  and motion, plus a build that writes CSS custom properties.
- `@facet/ui` with a first example component (`createButton`).
- Docs site (Astro) with its own docs layout and built-in search: Getting started, Concepts
  and Roadmap pages, a design system page that renders the tokens, and placeholder Demo and
  Install pages.
- Deployment to GitHub Pages.

[Unreleased]: https://github.com/mrdarrengriffin/facet/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/mrdarrengriffin/facet/releases/tag/v0.1.0
