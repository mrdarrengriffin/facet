---
title: Screens
description: Full-screen views built from components, and how panels decide what to show.
group: Concepts
order: 3
---

A screen is a full-screen view made from components: a clock, now playing, a light dial, a
kitchen timer, the weather.

## Context cards

Panels don't have a fixed home screen. Each one has a **room profile**: the cards that can
appear on it and how important each one is. Cards appear and disappear based on live Home
Assistant state:

- The most relevant card is in front. The rest are a swipe away.
- Tapping a card opens its full screen.
- When nothing needs attention, the panel shows an ambient screen.

A desk panel might bring up media controls when music starts. A kitchen panel might show a
running timer above everything else.

## Principles

- **Glanceable.** Readable from across the room, with one clear value per card.
- **One action per card.** Big touch targets, nothing fiddly.
- **No settings on the device.** Configuration happens elsewhere.
- **Purposeful motion.** Small animations that explain what happened, not decoration.
- **The same everywhere.** Every panel uses the same components.
