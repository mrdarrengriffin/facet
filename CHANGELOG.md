# Changelog

All notable changes to Facet are written down here. The format is loosely based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- The full token set in `@facet/tokens`: colours, a screen type scale and a component type
  scale, the 8 px screen layout, sizes, motion, toasts and sound settings, from one
  `tokens.json`. The build also writes ESPHome YAML (substitutions, fonts, LVGL styles).
- `@facet/icons`: 30 UI icons as master SVGs, 12 screen icons, 17 animated weather icons and
  a manifest.
- `@facet/ui`: every component and screen as plain TypeScript factories. Screens read a small
  observable store and call actions. Screens: Home, Agents, Media, Light, Clock (five faces),
  GitHub, Energy, Weather, Lightning and Timer, plus toasts and an idle clock.
- `@facet/sounds`: procedural UI sounds in six named packs (Keycap by default, plus Halo,
  Arcade, Graphite, Crystal and Felt), chosen with `setPack()`, and hooks that play them on
  real taps only.
- Site: a Sounds page (`/docs/sounds/`) with every sound in every pack playable live.
- `@facet/demo-data`: a fictional mock house.
- Site: a UI explorer (`/components/`), a full demo (`/demo/`), a docs page with a live
  preview for every screen, and the new Facet mark, favicons, web manifest and social card.

### Changed

- `createButton` is replaced by `button()` and the rest of the component set.
- Colour tokens follow the panel names: `line-strong` is now `line-hi`, `info` is now `blue`.

## [0.1.0] - 2026-10-19

### Added

- Project skeleton: pnpm workspace, shared TypeScript config, CI.
- `@facet/tokens` with a starter set of colours (dark and light), type scale, spacing, radius
  and motion, plus a build that writes CSS custom properties.
- `@facet/ui` with a first example component (`createButton`).
- Docs site (Astro) with its own docs layout and built-in search: Getting started, Concepts
  and Roadmap pages, a design system page that renders the tokens, and placeholder Demo and
  Install pages.
- Deployment to GitHub Pages.

[Unreleased]: https://github.com/mrdarrengriffin/facet/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/mrdarrengriffin/facet/releases/tag/v0.1.0
