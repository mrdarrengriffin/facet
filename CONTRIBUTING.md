# Contributing to Facet

Thanks for taking an interest. Facet is at a very early stage, so the most useful things right
now are ideas, feedback on the direction, and bug reports.

## Before you start

- For anything bigger than a small fix, open an issue first so we can talk it through. The
  structure is still settling and I'd hate for you to put work into something that's about to
  change.
- Be kind. Everyone taking part is expected to follow the [Code of Conduct](CODE_OF_CONDUCT.md).

## Setting up

You'll need Node 22 or newer (see `.nvmrc`) and [pnpm](https://pnpm.io/).

```sh
pnpm install
pnpm dev
```

Before opening a pull request, make sure these pass:

```sh
pnpm typecheck
pnpm lint
pnpm build
```

## Guidelines

- **TypeScript everywhere, no UI frameworks.** Components are plain functions that build DOM
  elements. This keeps them light and makes it practical to mirror them in LVGL on the panels.
- **Tokens first.** Colours, sizes, spacing and timing come from `@facet/tokens`. If you need a
  value that isn't there, add a token rather than hard-coding it.
- **Keep it LVGL-friendly.** Panels can't do shadows, blur or heavy gradients, and animation
  should stick to moving or fading small things. If a design only works in a browser, it
  probably doesn't belong in a shared component.
- **Nothing tied to one house.** No real entity ids, addresses or names in examples. Use
  obviously fake data.
- Add a line to the `Unreleased` section of `CHANGELOG.md` for anything user-facing.

## Licence

By contributing, you agree that your contributions are licensed under the [MIT License](LICENSE).
