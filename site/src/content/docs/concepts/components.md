---
title: Components
description: Small building blocks written in plain TypeScript.
group: Concepts
order: 2
---

Components are the buttons, cards, tiles, sliders and dials that screens are made of. They
live in [`@facet/ui`](https://github.com/mrdarrengriffin/facet/tree/main/packages/ui), and you
can try every one of them, with controls for their props, in the [UI explorer](/components/).

## No framework

Facet's components are plain TypeScript. Each one is a function that takes some options and
returns a DOM element:

```ts
import '@facet/tokens/tokens.css';
import '@facet/icons/weather.css';
import '@facet/ui/styles.css';
import { button } from '@facet/ui';

const start = button({ kind: 'primary', text: 'Start', onTap: () => startTimer() });
document.querySelector('#controls')?.append(start);
```

There's no React, Vue or similar underneath. That keeps the components light enough for a
small Pi, and it keeps them close to how the same component is built in LVGL on a panel.

Components that change over time return the element with a `set()` method attached, so you
update them in place rather than rebuilding them:

```ts
const weather = card({ label: 'Weather', icon: 'weather', preview, value: '12°', sub: 'dry' });
weather.set({ value: '13°', sub: 'rain within the hour' });
```

## What's there

| Group        | Components                                                                                |
| ------------ | ----------------------------------------------------------------------------------------- |
| Atoms        | `label`, `statusDot`, `statusChip`, `cornerTicks`, `focusRing`, `mascotMood`              |
| Icons        | `icon`, `screenIcon`, `weatherIcon`, `moonIcon` (from `@facet/icons`)                     |
| Buttons      | `button` (four kinds × four states), `iconButton` (play, round, square), `backButton`     |
| Controls     | `slider`, `dial`, `thermostatDial`, `bigSwitch`, `arc`, `disc`, `tickRing`                |
| Screen parts | `header`, `card`, `tile`, `heroBlock`, `row`, `moreRow`, `stat`, card previews            |
| Lists        | `countBadge`, `sectionHeader`, `stateChip`, `prRow`, `checkRow`, `emptyState`, `qrBlock`  |
| Pickers      | `radio`, `pickerRow`, `speakerButton`, `bottomSheet`                                      |
| Toasts       | `toast`, `toastLayer`                                                                     |
| Charts       | `barChart`, `rateStrip`, `sparkline`, `bigNumber`, `deltaChip`, `donut`, `legend`, `axis` |
| Clock        | `timeText`, `dateLine`, `clockHero`, `clockRing`, `clockAnalogue`, `clockContext`         |
| Radar        | `lightningRadar`                                                                          |
| Moon         | `moonData`, `moonTimes`, `moonLine` (worked out locally, no API)                          |
| 3D print     | `fromGcode`, `fromStl`, `fromShape`, `render`, `palette` (voxel model, software render)   |

## Built for LVGL too

Every component has to work on an ESP32 panel as well as in a browser, so they stick to what
LVGL draws well:

- Flat fills and 1 px outlines. No shadows, blur or gradients.
- SVG only for what LVGL draws natively: arcs and scale ticks.
- Animation on opacity and transform of small elements only.
- Touch targets of 64 px, never smaller than 48.
- Every box on the 8 px grid, including the line boxes of text.

If a design only works in a browser, it doesn't belong in a shared component.
