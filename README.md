<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="brand/facet-logo-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="brand/facet-logo-light.svg">
    <img alt="Facet" src="brand/facet-logo-light.svg" width="236" height="64">
  </picture>
</p>

# Facet

Wall panel UI for Home Assistant. One design system that runs on ESP32 touch panels (ESPHome + LVGL) and in the browser on a Pi or tablet.

**Docs and design system:** https://mrdarrengriffin.github.io/facet/

> **Status: early, work in progress.** The design tokens, icons, components, screens and UI
> sounds are in place and you can try them all in the browser: the
> [demo](https://mrdarrengriffin.github.io/facet/demo/) runs a whole panel on a mock house and
> the [UI explorer](https://mrdarrengriffin.github.io/facet/components/) has every component.
> There's no firmware or Home Assistant adapter yet. Expect things to move around before 1.0.

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

## Inspiration

Facet is inspired by [ESPControl](https://github.com/jtenniswood/espcontrol) by jtenniswood, an
ESPHome-based smart home control panel. It's a lovely project and well worth a look, especially if
you want a panel you can set up today: [jtenniswood.github.io/espcontrol](https://jtenniswood.github.io/espcontrol/).
Facet isn't affiliated with it.

## Repository layout

```
packages/
  tokens/     @facet/tokens: design tokens as typed objects, CSS variables and ESPHome YAML
  icons/      @facet/icons: master SVG icons, screen icons, weather icons, manifest
  ui/         @facet/ui: components and screens in plain TypeScript (no UI framework)
  sounds/     @facet/sounds: procedural UI sounds and the hooks that play them
  demo-data/  @facet/demo-data: a fictional mock house for demos, docs and tests
brand/        The Facet mark, logo, app icons and social card
firmware/     ESPHome configuration for ESP32 touch panels (planned)
site/         Docs, UI explorer, demo and design system pages (Astro)
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
firmware side. Screens read a small store and call actions, so the same screens run on the mock
house, in tests, or on real data.

## Versioning

Facet is a single version for now (`0.1.0`) with a hand-written [CHANGELOG](CHANGELOG.md).

## Site

The site is deployed to GitHub Pages from `main` and served under `/facet/` (GitHub Pages paths follow the repository name). It may move to its
own domain later, at which point the Astro `site` and `base` settings in `site/astro.config.mjs`
will change.

## Contributing

Ideas and issues are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) and the
[Code of Conduct](CODE_OF_CONDUCT.md).

## Licence

[MIT](LICENSE) © Darren Griffin. Bundled fonts (Inter, JetBrains Mono) are under the SIL Open
Font License.
