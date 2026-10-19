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

> **Not ready yet.** Facet is at a very early stage. There's no firmware to flash and no app to install yet. For now this site is mostly a place to follow along.

## How it works

Each panel has a room profile: which cards can appear there and how important they are. Cards
come and go based on live Home Assistant state. The most relevant one sits in front, the rest
are a swipe away, and when nothing needs you the screen drops back to an ambient view.

A desk panel might show media controls while music plays and a nudge when lights are left on.
A kitchen panel might offer one-tap timers and show how long the washing machine has left.

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

| Path               | What's in it                                                     |
| ------------------ | ---------------------------------------------------------------- |
| `packages/tokens/` | `@facet/tokens`: the design tokens, plus generated CSS variables |
| `packages/ui/`     | `@facet/ui`: components written in plain TypeScript              |
| `firmware/`        | ESPHome configuration for the panels (planned)                   |
| `site/`            | This site, built with Astro                                      |
