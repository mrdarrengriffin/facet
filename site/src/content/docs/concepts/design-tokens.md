---
title: Design tokens
description: The named values every part of Facet is built from.
group: Concepts
order: 1
---

Design tokens are the named values behind everything you see: colours, type sizes, spacing,
corner radii and animation timing. Every component reads from them instead of hard-coding
values, which is what keeps a panel on the wall and a browser on a tablet looking the same.

The tokens live in [`@facet/tokens`](https://github.com/mrdarrengriffin/facet/tree/main/packages/tokens)
as typed TypeScript objects. A build step turns them into CSS custom properties for the web,
and later into ESPHome substitutions for the panels.

> **Note.** The current tokens are a starter set. The full set, worked out on real hardware, will replace them.

## Groups

| Group    | Examples                                | Notes                                                  |
| -------- | --------------------------------------- | ------------------------------------------------------ |
| `color`  | `bg`, `surface`, `text`, `accent`, `ok` | Every colour has a dark and a light value              |
| `type`   | `display`, `title`, `body`, `label`     | Inter for words and numbers, JetBrains Mono for labels |
| `space`  | `1` (4 px) to `7` (64 px)               | An 8 px grid, with 4 px for tight spots                |
| `radius` | `none`, `sm`, `md`, `lg`, `pill`        |                                                        |
| `motion` | `duration.fast`, `easing.standard`      | Animate opacity and transform only                     |

## Using them

```ts
import { color, space } from '@facet/tokens';

const accent = color.accent.dark; // '#E8542A'
const gap = space['3']; // 16
```

```css
@import '@facet/tokens/tokens.css';

.card {
  background: var(--facet-color-surface);
  border: 1px solid var(--facet-color-line);
  padding: var(--facet-space-4);
}
```

Dark is the default theme. Light follows the system setting, and either can be forced with
`data-facet-theme="dark"` or `data-facet-theme="light"` on any element.

See every token rendered on the [design system](/design-system/) page.
