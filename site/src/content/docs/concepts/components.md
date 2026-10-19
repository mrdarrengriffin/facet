---
title: Components
description: Small building blocks written in plain TypeScript.
group: Concepts
order: 2
---

Components are the buttons, cards, tiles, sliders and dials that screens are made of. They
live in [`@facet/ui`](https://github.com/mrdarrengriffin/facet/tree/main/packages/ui).

## No framework

Facet's components are plain TypeScript. Each one is a function that takes some options and
returns a DOM element:

```ts
import '@facet/tokens/tokens.css';
import '@facet/ui/styles.css';
import { createButton } from '@facet/ui';

const start = createButton({ label: 'Start', onPress: () => startTimer() });
document.querySelector('#controls')?.append(start);
```

There's no React, Vue or similar underneath. That keeps the components light enough for a
small Pi, and it keeps them close to how the same component is built in LVGL on a panel.

## Built for LVGL too

Every component has to work on an ESP32 panel as well as in a browser, so they stick to what
LVGL draws well:

- Flat fills and 1 px outlines. No shadows, blur or heavy gradients.
- Rounded corners from the radius tokens.
- Animation on opacity and transform of small elements only.
- Touch targets of at least 64 px.

If a design only works in a browser, it doesn't belong in a shared component.
