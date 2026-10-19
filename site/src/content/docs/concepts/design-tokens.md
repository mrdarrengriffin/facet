---
title: Design tokens
description: The named values every part of Facet is built from.
group: Concepts
order: 1
---

Design tokens are the named values behind everything you see: colours, type, layout, spacing,
corner radii, motion and even the UI sounds. Every component reads from them instead of
hard-coding values, which is what keeps a panel on the wall and a browser on a tablet looking
the same.

The tokens live in one file, `tokens.json`, in
[`@facet/tokens`](https://github.com/mrdarrengriffin/facet/tree/main/packages/tokens). The
package exports them as typed objects, and its build turns them into CSS custom properties for
the web and into ESPHome YAML (substitutions, fonts and LVGL styles) for the panels.

## Groups

| Group        | Examples                                        | Notes                                                    |
| ------------ | ----------------------------------------------- | -------------------------------------------------------- |
| `color`      | `bg`, `surface`, `text`, `accent`, `blue`, `ok` | Every colour has a dark and a light value                |
| `screenType` | `display`, `hero`, `value`, `headline`, `label` | One hero per screen; classes `.fst-<role>`               |
| `type`       | `title`, `tile`, `meta`, `caps-sm`, `clock-*`   | The smaller text inside components; classes `.ft-<name>` |
| `layout`     | `inset` 32, `gutter` 16, `group` 48, `top` 112  | Screen geometry on the 8 px grid                         |
| `size`       | `touch` 64, `button-h` 64, `dial-scale` 560     | Component sizes                                          |
| `space`      | `2` (8 px), `5` (16 px), `9` (32 px)            | Spacing inside components                                |
| `radius`     | `card`, `chip`, `button`, `round`               |                                                          |
| `motion`     | `duration.dial-sweep`, `easing.ease-out`        | Each easing has its CSS curve and its LVGL path          |
| `toast`      | `position`, `max`, `life`, `swipe`              |                                                          |
| `sound`      | `chosen`, `loudnessDb`, `peakDb`                | The UI sound set in use and how sounds are normalised    |

## Using them

```ts
import { color, layout, screenType } from '@facet/tokens';

const accent = color.accent.dark; // '#E8542A'
const inset = layout.inset; // 32
const hero = screenType.hero.size; // 96
```

```css
@import '@facet/tokens/tokens.css';

.card {
  background: var(--facet-color-surface);
  border: 1px solid var(--facet-color-line);
  padding: var(--facet-space-7);
}
```

Dark is the default theme. Light follows the system setting, and either can be forced with
`data-facet-theme="dark"` or `data-facet-theme="light"` on any element, so one preview can be
light on a dark page. The live previews on this site follow the site's own toggle that way.

## For the panels

`@facet/tokens/esphome/tokens.yaml` is generated with the package. It holds every colour (dark
and light) as substitutions, the fonts both type scales need, and LVGL style definitions for
text, buttons, icon buttons, cards, tiles and chips. Nothing uses it yet; it's ready for the
firmware.

See every token rendered on the [design system](/design-system/) page.
