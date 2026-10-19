# @facet/ui

Facet components, written in plain TypeScript. No UI framework: each component is a function
that returns a DOM element.

> Early days. There's one example component so far, to set the pattern.

## Usage

```ts
import '@facet/tokens/tokens.css';
import '@facet/ui/styles.css';
import { createButton } from '@facet/ui';

document.body.append(
  createButton({ label: 'Start', onPress: () => console.log('pressed') }),
  createButton({ label: 'Cancel', variant: 'secondary' }),
);
```

## Writing components

- A component is a `create<Name>(options)` function that returns an `HTMLElement`.
- Styles go in `src/styles.css` and only use token variables (`var(--facet-...)`).
- Stay within what LVGL can draw: flat fills, 1 px outlines, rounded corners, and animation on
  opacity and transform only. Each web component should have a firmware twin later.
