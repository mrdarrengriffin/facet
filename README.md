# Facet

Wall panel UI for Home Assistant. One design system that runs on ESP32 touch panels (ESPHome + LVGL) and in the browser on a Pi or tablet.

**Docs and design system:** https://mrdarrengriffin.github.io/facet/

> **Status: early, work in progress.** This is the first skeleton of the project. The design
> tokens here are a small starter set, the component library has one example component, and
> there's no firmware yet. Expect things to move around a lot before 1.0.

## The idea

A small screen in every room, each one showing what matters in that room right now: the music
that's playing, a kitchen timer, the washing machine, the next thing in the calendar. Cards come
and go based on live Home Assistant state, the most relevant one sits in front, and when nothing
needs you the screen goes quiet.

Everything is built from one set of design tokens and one set of components, so a panel on the
wall and a tablet on the counter look and behave the same.

- Glanceable from across the room
- Big touch targets, one action per card
- No settings on the device itself
- Small, purposeful animations

The first supported hardware is the Waveshare ESP32-P4 86 panel (4", 720 x 720). Other sizes
and aspect ratios are planned.

## Repository layout

```
packages/
  tokens/     @facet/tokens: design tokens as typed objects, plus a CSS variables build
  ui/         @facet/ui: components written in plain TypeScript (no UI framework)
firmware/     ESPHome configuration for ESP32 touch panels (planned)
site/         Docs site and design system pages (Astro)
```

## Development

You'll need Node 22 or newer and [pnpm](https://pnpm.io/).

```sh
pnpm install
pnpm dev         # docs site on http://localhost:4321/facet/
pnpm build       # build every package and the site
pnpm typecheck   # type-check every package
pnpm lint        # check formatting
```

The whole project is TypeScript with no UI framework. Components are plain functions that
return DOM elements, which keeps them small and makes it easier to mirror them in LVGL on the
firmware side.

## Versioning

Facet is a single version for now (`0.1.0`) with a hand-written [CHANGELOG](CHANGELOG.md).

## Site

The site is deployed to GitHub Pages from `main` and served under `/facet/` (GitHub Pages paths follow the repository name, capital F included). It may move to its
own domain later, at which point the Astro `site` and `base` settings in `site/astro.config.mjs`
will change.

## Contributing

Ideas and issues are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) and the
[Code of Conduct](CODE_OF_CONDUCT.md).

## Licence

[MIT](LICENSE) © Darren Griffin. Bundled fonts (Inter, JetBrains Mono) are under the SIL Open
Font License.
