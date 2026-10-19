---
title: Getting started
description: What Facet is, where it's headed, and how to run the project locally.
group: Start here
order: 1
---

Facet is a wall panel UI for Home Assistant. It's one design system that runs in two places:

- **On ESP32 touch panels**, through ESPHome and LVGL. The first board is the Waveshare
  ESP32-P4 86 panel: a 4" 720 x 720 screen that fits a standard wall box.
- **In the browser**, on a Raspberry Pi, a tablet or anything else with a screen.

Both are built from the same design tokens and the same set of components, so they look and
behave the same.

> **Early days.** The design system, components and screens are here and you can try them
> all in the browser, but there's no firmware to flash and no Home Assistant adapter yet.

## Try it

- The [demo](/demo/) runs a whole panel on a mock house: tap through the screens, send
  toasts, let it fall asleep.
- The [UI explorer](/components/) has every component with controls for its props.
- Each screen has a page under **Screens** in the sidebar, with a live preview, the data it
  needs and how to configure it.

## How it works

Each panel has a room profile: which cards can appear there and how important they are. Cards
come and go based on live Home Assistant state. The most relevant one sits in front, the rest
are a swipe away, and when nothing needs you the screen drops back to an ambient view.

A desk panel might show media controls while music plays and a nudge when lights are left on.
A kitchen panel might offer one-tap timers and show how long the washing machine has left.

## Inspired by ESPControl

Facet is inspired by [ESPControl](https://github.com/jtenniswood/espcontrol) by jtenniswood, an
ESPHome control panel for smart homes. If you want a panel you can flash today, it's well worth a
look ([docs](https://jtenniswood.github.io/espcontrol/)). The two projects aren't affiliated.

## Running the project locally

You'll need Node 22 or newer and [pnpm](https://pnpm.io/).

```sh
git clone https://github.com/mrdarrengriffin/facet.git
cd facet
pnpm install
pnpm dev
```

That builds the packages and starts this docs site at `http://localhost:4321/facet/`.

Other commands, run from the repository root:

| Command          | What it does                      |
| ---------------- | --------------------------------- |
| `pnpm build`     | Builds every package and the site |
| `pnpm typecheck` | Type-checks every package         |
| `pnpm lint`      | Checks formatting with Prettier   |
| `pnpm format`    | Fixes formatting                  |

## Repository layout

| Path                  | What's in it                                                                      |
| --------------------- | --------------------------------------------------------------------------------- |
| `packages/tokens/`    | `@facet/tokens`: the design tokens, as TypeScript, CSS variables and ESPHome YAML |
| `packages/icons/`     | `@facet/icons`: master SVG icons, screen icons, weather icons and a manifest      |
| `packages/ui/`        | `@facet/ui`: components and screens written in plain TypeScript                   |
| `packages/sounds/`    | `@facet/sounds`: procedural UI sounds and the hooks that play them                |
| `packages/demo-data/` | `@facet/demo-data`: a fictional mock house for demos, docs and tests              |
| `brand/`              | The Facet mark, logo, app icons and social card                                   |
| `firmware/`           | ESPHome configuration for the panels (planned)                                    |
| `site/`               | This site, built with Astro                                                       |

## Using the packages

The packages aren't published to npm yet. Inside this repository, a page that shows a whole
panel looks like this:

```ts
import '@facet/tokens/tokens.css';
import '@facet/icons/weather.css';
import '@facet/ui/styles.css';
import { createMockHouse } from '@facet/demo-data';
import { createDevice } from '@facet/ui';

const house = createMockHouse();
document.body.append(createDevice(house).el);
```

Swap the mock house for your own store and actions to drive it from real data.
