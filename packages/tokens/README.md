# @facet/tokens

Design tokens for Facet: colour (dark and light), two type scales, the screen layout, sizes,
spacing, radius, motion, toasts and sound. Everything comes from one file,
[`src/tokens.json`](src/tokens.json).

## Outputs

| Import                              | What                                                                                |
| ----------------------------------- | ----------------------------------------------------------------------------------- |
| `@facet/tokens`                     | Typed objects: `color`, `screenType`, `type`, `layout`, `size`, ...                 |
| `@facet/tokens/tokens.css`          | CSS custom properties (`--facet-color-*`, ...) and type classes (`.fst-*`, `.ft-*`) |
| `@facet/tokens/tokens.json`         | The source file                                                                     |
| `@facet/tokens/esphome/tokens.yaml` | ESPHome substitutions, fonts and LVGL style definitions                             |

```ts
import { color, layout } from '@facet/tokens';

color.accent.dark; // '#E8542A'
layout.inset; // 32
```

Dark is the default theme; light follows the system, and `data-facet-theme="dark|light"` on
any element forces one for that element and everything inside it.
