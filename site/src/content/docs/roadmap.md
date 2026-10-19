---
title: Roadmap
description: Where Facet is headed, roughly in order.
group: Project
order: 1
---

A rough plan, not a promise. Things will shift as the hardware and the design teach us more.

## Now: the foundations

- [x] Project skeleton, CI and this site
- [x] Starter design tokens and a first component
- [ ] Move over the full token set (colour, type, layout grid, motion, sound)
- [ ] Core components: buttons, cards, tiles, sliders, dials, chips, toasts

## Next: the first panel

- [ ] Firmware for the Waveshare ESP32-P4 86 panel (ESPHome + LVGL)
- [ ] Generate the ESPHome theme from `@facet/tokens`
- [ ] First screens: clock, now playing, lights, timers, weather
- [ ] Context cards: room profiles that decide which cards show and in what order

## Then: the web version and the demo

- [ ] A full-screen web build for a Pi or tablet
- [ ] A live demo on this site running on mock data
- [ ] An interactive design system playground (props, themes, sizes)

## Later

- [ ] Browser-based installer for flashing panels over USB
- [ ] Size classes for other screens: 480 x 480, 800 x 480, 1024 x 600, portrait
- [ ] One screen description that renders both to the web and to LVGL
- [ ] Ambient screens with generative animation
