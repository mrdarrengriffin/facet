---
title: Roadmap
description: Where Facet is headed, area by area.
group: Project
order: 1
---

A rough plan, not a promise. Things will shift as the hardware and the design teach us more.
Ticked items are done, the ones with an orange dot are in progress.

## Design system

- [x] The full token set: colour, two type scales, layout grid, motion, toasts, sound
- [x] One `tokens.json` for the web and the panels
- [x] Icons: master SVGs, screen icons and animated weather icons
- [x] Procedural UI sounds
- [x] The Facet mark, favicons and social card
- [ ] Size classes for other screens: 480 × 480, 800 × 480, 1024 × 600, portrait

## Components

- [x] Buttons, icon buttons and the back button
- [x] Dial, slider, arc and the media disc
- [x] Cards, tiles, rows, chips, lists and pickers
- [x] Toasts and the toast layer
- [x] Charts, clocks and the lightning radar
- [x] The moon phase glyph and the moon calculation (`moonIcon`, `moonData`)
- [x] Checkboxes
- [ ] One component description that renders to the web and to LVGL

## Screens

- [x] Home, Agents, Media, Light and Clock
- [x] GitHub, Energy, Weather, Lightning and Timer
- [x] Moon: the phase drawn as the hero, worked out on the device (no API)
- [x] 3D print: the model fills up layer by layer, drawn in software (gcode, STL or a shape)
- [ ] 3D print on the firmware: frames rendered by a server
- [x] Thermostat: the room temperature on a heating ring, set point as the knob, modes and presets
- [ ] Thermostat on the firmware
- [x] Blinds and curtains: the window drawn per kind (roller, Venetian with tilt, curtains), drag to a position
- [ ] Blinds and curtains on the firmware
- [x] Calendar: the next event as the hero, then two weeks of days, a month view; calendars shown or hidden from a sheet
- [ ] Calendar on the firmware
- [x] Switch: one big button for anything with an on and an off, power and time underneath
- [ ] Switch on the firmware
- [x] Settings: theme, accent, text size, sounds, idle and behaviour, from one schema (`@facet/settings`)
- [x] Settings docs page: every setting live on a panel, accent contrast, the ESPHome YAML
- [ ] Settings on the firmware (accent, text size and sound pack)
- [x] Toasts over any screen and an idle clock
- [~] Context cards: room profiles that decide which cards show and in what order
- [ ] Ambient screens with generative animation

## Firmware

- [x] Generate the ESPHome theme from `@facet/tokens`
- [~] Firmware for the first panel (ESPHome + LVGL)
- [ ] Build the screens in LVGL from the same layouts
- [ ] A Home Assistant adapter for the store and actions

## Boards

The first target is the Waveshare panel; the Guition one comes second.

- [~] **First:** Waveshare ESP32-P4 86 panel (4", 720 × 720)
- [ ] **Second:** Guition ESP32-4848S040 (ESP32-S3, 4", 480 × 480), which needs the smaller size class
- [ ] A full-screen web build for a Pi or tablet
- [ ] More panels and sizes

## Docs and site

- [x] Docs, search and the design system page
- [x] A live demo running on mock data
- [x] A UI explorer with controls for every component
- [x] A page with a live preview for every screen
- [ ] Size classes in the explorer and the demo

## Install wizard

- [ ] Flash a panel from the browser over USB
- [ ] Set up Wi-Fi and Home Assistant from the same page
