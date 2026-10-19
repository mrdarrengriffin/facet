# @facet/tokens

Design tokens for Facet: colour (dark and light), type, spacing, radius and motion.

> This is a **starter set**. It's enough to build the docs site and the first components. The
> full token set will replace it, and names may change before 1.0.

## Usage

```ts
import { color, type, space } from '@facet/tokens';

color.accent.dark; // '#E8542A'
type.title.size; // 28
space['3']; // 16
```

Or use the generated CSS custom properties:

```css
@import '@facet/tokens/tokens.css';

.card {
  background: var(--facet-color-surface);
  padding: var(--facet-space-4);
  border-radius: var(--facet-radius-md);
}
```

Dark is the default. Light follows `prefers-color-scheme`, and either theme can be forced with
`data-facet-theme="dark"` or `data-facet-theme="light"` on any element.

## Build

`pnpm build` compiles the TypeScript to `dist/` and writes `dist/tokens.css`.
