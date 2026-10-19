# Facet firmware

ESPHome configuration that runs the Facet UI on ESP32 touch panels, drawn with LVGL.

> **Nothing here yet.** This folder describes the planned structure so the pieces have a home
> when they land.

## First board

**Waveshare ESP32-P4 86 panel**: a 4" 720 x 720 touch screen that fits a standard 86 mm wall
box, with an ESP32-C6 for Wi-Fi.

## Planned layout

```
firmware/
  boards/       One file per supported panel: display, touch, backlight, audio and pins.
  packages/     Shared ESPHome packages: Home Assistant connection, theme (generated from
                @facet/tokens), fonts, sounds, and the screens themselves.
  components/   Custom ESPHome components where YAML isn't enough.
  examples/     Ready-to-flash configurations that combine a board with the packages.
```

The idea is that a working panel config is short: pick a board, include the Facet packages,
and say which cards you want.

```yaml
# Sketch only. The real package names will change.
packages:
  board: github://mrdarrengriffin/facet/firmware/boards/waveshare-esp32-p4-86.yaml
  facet: github://mrdarrengriffin/facet/firmware/packages/facet.yaml
```

## How it relates to the web version

The design tokens in `packages/tokens` are the source of truth. A build step will turn them
into ESPHome substitutions (colours, sizes, fonts) so the panels and the browser version stay
in step. Components are designed in the browser first, within what LVGL can draw, and then
built here.

An install page on the docs site will let you flash a panel straight from the browser over
USB.
