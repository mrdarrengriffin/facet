---
title: Firmware
description: Running Facet on ESP32 touch panels with ESPHome and LVGL.
group: Concepts
order: 4
---

On ESP32 panels, Facet runs as [ESPHome](https://esphome.io/) firmware with the UI drawn by
[LVGL](https://lvgl.io/). The panel talks to Home Assistant over the native ESPHome API.

## First board

The **Waveshare ESP32-P4 86 panel**: a 4" 720 x 720 touch screen that fits a standard 86 mm
wall box, with an ESP32-C6 alongside for Wi-Fi.

## Planned structure

The [`firmware/`](https://github.com/mrdarrengriffin/facet/tree/main/firmware) folder will be
split into:

| Folder        | What goes there                                                      |
| ------------- | -------------------------------------------------------------------- |
| `boards/`     | One file per supported panel: display, touch, backlight, audio, pins |
| `packages/`   | Shared packages: theme, fonts, sounds, screens, Home Assistant link  |
| `components/` | Custom ESPHome components where YAML isn't enough                    |
| `examples/`   | Ready-to-flash configurations combining a board with the packages    |

The theme package will be generated from `@facet/tokens`, so the panels always match the web
version. UI sounds are rendered from the same packs as the web and embedded as PCM files; see
[Sounds](/docs/sounds/#on-a-panel).

## Installing

An [install page](/install/) will let you flash a panel from the browser over USB. It
isn't ready yet.
