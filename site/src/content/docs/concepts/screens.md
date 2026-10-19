---
title: Screens
description: Full-screen views built from components, and how panels decide what to show.
group: Concepts
order: 3
---

A screen is a full-screen view made from components. Facet has Home, Agents, Media, Light,
Clock, GitHub, Energy, Weather, Lightning and Timer, plus toasts that appear over any of them.
Each has its own page under **Screens** in the sidebar, with a live preview.

## How a screen gets its data

Screens never talk to Home Assistant (or anything else) directly. They read a small store and
call actions:

```ts
import { createDevice, createStore } from '@facet/ui';

const store = createStore({ room: 'Kitchen', light: { on: true, brightness: 60 } });
const actions = {
  lightBrightness: (pct) => sendToLight(pct),
};
document.body.append(createDevice({ store, actions }).el);
```

An adapter fills the store and implements the actions. For the demo and these docs that's the
mock house in `@facet/demo-data`; for a real house it'll be a Home Assistant adapter.

## Design principles

- **One hero per screen.** The number or word you came for, large and light: the time, the
  temperature, the brightness, how many things need you.
- **Hierarchy from contrast, not size.** Secondary text is small and dim, labels are mono caps
  in the quietest colour.
- **Show less.** Home cards show one value and one line. Lists show the top three and a way
  to see the rest.
- **Whitespace with a rhythm.** One inset of 32 on every side, more space between groups than
  inside them, rows separated by hairlines rather than boxes.
- **Calm colour.** Neutrals, with the orange accent only for the one thing that matters.
- **Alignment.** One strong left edge, everything on the 8 px grid.

## Context cards

Panels don't have a fixed home screen. Each one has a **room profile**: the cards that can
appear on it and how important each one is. Cards appear and disappear based on live Home
Assistant state:

- The most relevant card is in front. The rest are a swipe away.
- Tapping a card opens its full screen.
- When nothing needs attention, the panel shows an ambient screen.

For now Home takes a fixed list of cards (`cards: ['media', 'light', ...]`); room profiles come
later.
