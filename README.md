<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="brand/facet-logo-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="brand/facet-logo-light.svg">
    <img alt="Facet" src="brand/facet-logo-light.svg" width="236" height="64">
  </picture>
</p>

<p align="center">
  Wall panel UI for Home Assistant.<br>
  One design system that runs on ESP32 touch panels (ESPHome + LVGL) and in the browser on a Pi or tablet.
</p>

<p align="center">
  <a href="https://mrdarrengriffin.github.io/facet/demo/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/pill-demo-dark.png"><img alt="Demo" src=".github/assets/pill-demo-light.png" height="36"></picture></a> &nbsp;
  <a href="https://mrdarrengriffin.github.io/facet/components/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/pill-components-dark.png"><img alt="Components" src=".github/assets/pill-components-light.png" height="36"></picture></a> &nbsp;
  <a href="https://mrdarrengriffin.github.io/facet/docs/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/pill-docs-dark.png"><img alt="Docs" src=".github/assets/pill-docs-light.png" height="36"></picture></a> &nbsp;
  <a href="https://mrdarrengriffin.github.io/facet/docs/roadmap/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/pill-roadmap-dark.png"><img alt="Roadmap" src=".github/assets/pill-roadmap-light.png" height="36"></picture></a>
</p>

<br>

<p align="center">
  <a href="https://mrdarrengriffin.github.io/facet/demo/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screens-dark.png"><img alt="The Facet screens with demo data: Home, Agents, Media, Light, Clock, GitHub, Energy, Weather, Lightning and Timer" src=".github/assets/screens-light.png"></picture></a>
</p>

<p align="center"><sub>Every screen running on the mock house from <code>@facet/demo-data</code>. Docs and design system: <a href="https://mrdarrengriffin.github.io/facet/">mrdarrengriffin.github.io/facet</a></sub></p>

<br>

> **Everything here is a work in progress.** The design system is being worked on actively, new
> screens are still being made and existing ones change as they get used on a real panel, so expect
> things to move around. You can already try it all in the browser: the
> [demo](https://mrdarrengriffin.github.io/facet/demo/) runs a whole panel on a mock house and the
> [UI explorer](https://mrdarrengriffin.github.io/facet/components/) has every component. Firmware
> and the Home Assistant adapter are still to come.

## <picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/icon-light-dark.svg"><source media="(prefers-color-scheme: light)" srcset=".github/assets/icon-light-light.svg"><img alt="" src=".github/assets/icon-light-light.svg" width="20" height="20"></picture>&nbsp; The idea

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

## <picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/icon-home-dark.svg"><source media="(prefers-color-scheme: light)" srcset=".github/assets/icon-home-light.svg"><img alt="" src=".github/assets/icon-home-light.svg" width="20" height="20"></picture>&nbsp; Screens

Each screen has a page in the docs with a live preview you can tap and drag, what it shows, the
data it reads and how to set it up.

<table>
<tr>
<td width="50%" valign="top">
<a href="https://mrdarrengriffin.github.io/facet/docs/screens/home/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screen-home-dark.png"><img alt="Home screen" src=".github/assets/screen-home-light.png" width="200" height="200"></picture></a>
<p><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/icon-home-dark.svg"><source media="(prefers-color-scheme: light)" srcset=".github/assets/icon-home-light.svg"><img alt="" src=".github/assets/icon-home-light.svg" width="20" height="20"></picture>&nbsp; <a href="https://mrdarrengriffin.github.io/facet/docs/screens/home/"><b>Home</b></a><br>
The room's cards: one value and one line each, four to a view.</p>
</td>
<td width="50%" valign="top">
<a href="https://mrdarrengriffin.github.io/facet/docs/screens/agents/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screen-agents-dark.png"><img alt="Agents screen" src=".github/assets/screen-agents-light.png" width="200" height="200"></picture></a>
<p><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/icon-agents-dark.svg"><source media="(prefers-color-scheme: light)" srcset=".github/assets/icon-agents-light.svg"><img alt="" src=".github/assets/icon-agents-light.svg" width="20" height="20"></picture>&nbsp; <a href="https://mrdarrengriffin.github.io/facet/docs/screens/agents/"><b>Agents</b></a><br>
Background jobs and assistants as tiles with a mascot, sorted by what needs you.</p>
</td>
</tr>
<tr>
<td width="50%" valign="top">
<a href="https://mrdarrengriffin.github.io/facet/docs/screens/media/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screen-media-dark.png"><img alt="Media screen" src=".github/assets/screen-media-light.png" width="200" height="200"></picture></a>
<p><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/icon-media-dark.svg"><source media="(prefers-color-scheme: light)" srcset=".github/assets/icon-media-light.svg"><img alt="" src=".github/assets/icon-media-light.svg" width="20" height="20"></picture>&nbsp; <a href="https://mrdarrengriffin.github.io/facet/docs/screens/media/"><b>Media</b></a><br>
Now playing on a spinning disc, with transport controls, live volume and a speaker picker.</p>
</td>
<td width="50%" valign="top">
<a href="https://mrdarrengriffin.github.io/facet/docs/screens/light/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screen-light-dark.png"><img alt="Light screen" src=".github/assets/screen-light-light.png" width="200" height="200"></picture></a>
<p><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/icon-light-dark.svg"><source media="(prefers-color-scheme: light)" srcset=".github/assets/icon-light-light.svg"><img alt="" src=".github/assets/icon-light-light.svg" width="20" height="20"></picture>&nbsp; <a href="https://mrdarrengriffin.github.io/facet/docs/screens/light/"><b>Light</b></a><br>
One light on a dial: drag the ring for brightness, tap the button to switch it.</p>
</td>
</tr>
<tr>
<td width="50%" valign="top">
<a href="https://mrdarrengriffin.github.io/facet/docs/screens/clock/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screen-clock-dark.png"><img alt="Clock screen" src=".github/assets/screen-clock-light.png" width="200" height="200"></picture></a>
<p><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/icon-clock-dark.svg"><source media="(prefers-color-scheme: light)" srcset=".github/assets/icon-clock-light.svg"><img alt="" src=".github/assets/icon-clock-light.svg" width="20" height="20"></picture>&nbsp; <a href="https://mrdarrengriffin.github.io/facet/docs/screens/clock/"><b>Clock</b></a><br>
The time as the one hero on the screen, in several faces. Also the idle screen.</p>
</td>
<td width="50%" valign="top">
<a href="https://mrdarrengriffin.github.io/facet/docs/screens/github/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screen-github-dark.png"><img alt="GitHub screen" src=".github/assets/screen-github-light.png" width="200" height="200"></picture></a>
<p><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/icon-github-dark.svg"><source media="(prefers-color-scheme: light)" srcset=".github/assets/icon-github-light.svg"><img alt="" src=".github/assets/icon-github-light.svg" width="20" height="20"></picture>&nbsp; <a href="https://mrdarrengriffin.github.io/facet/docs/screens/github/"><b>GitHub</b></a><br>
Pull requests that need you, your own open ones, and a QR code to carry on on your phone.</p>
</td>
</tr>
<tr>
<td width="50%" valign="top">
<a href="https://mrdarrengriffin.github.io/facet/docs/screens/energy/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screen-energy-dark.png"><img alt="Energy screen" src=".github/assets/screen-energy-light.png" width="200" height="200"></picture></a>
<p><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/icon-energy-dark.svg"><source media="(prefers-color-scheme: light)" srcset=".github/assets/icon-energy-light.svg"><img alt="" src=".github/assets/icon-energy-light.svg" width="20" height="20"></picture>&nbsp; <a href="https://mrdarrengriffin.github.io/facet/docs/screens/energy/"><b>Energy</b></a><br>
Today's cost so far, the price band you're in, and today's half-hours by price.</p>
</td>
<td width="50%" valign="top">
<a href="https://mrdarrengriffin.github.io/facet/docs/screens/weather/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screen-weather-dark.png"><img alt="Weather screen" src=".github/assets/screen-weather-light.png" width="200" height="200"></picture></a>
<p><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/icon-weather-dark.svg"><source media="(prefers-color-scheme: light)" srcset=".github/assets/icon-weather-light.svg"><img alt="" src=".github/assets/icon-weather-light.svg" width="20" height="20"></picture>&nbsp; <a href="https://mrdarrengriffin.github.io/facet/docs/screens/weather/"><b>Weather</b></a><br>
The temperature, one line, rain in the next hour, and the next eight hours. Five days behind a tap.</p>
</td>
</tr>
<tr>
<td width="50%" valign="top">
<a href="https://mrdarrengriffin.github.io/facet/docs/screens/lightning/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screen-lightning-dark.png"><img alt="Lightning screen" src=".github/assets/screen-lightning-light.png" width="200" height="200"></picture></a>
<p><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/icon-lightning-dark.svg"><source media="(prefers-color-scheme: light)" srcset=".github/assets/icon-lightning-light.svg"><img alt="" src=".github/assets/icon-lightning-light.svg" width="20" height="20"></picture>&nbsp; <a href="https://mrdarrengriffin.github.io/facet/docs/screens/lightning/"><b>Lightning</b></a><br>
The nearest strike, the trend and a radar of the last hour.</p>
</td>
<td width="50%" valign="top">
<a href="https://mrdarrengriffin.github.io/facet/docs/screens/timer/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screen-timer-dark.png"><img alt="Timer screen" src=".github/assets/screen-timer-light.png" width="200" height="200"></picture></a>
<p><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/icon-timer-dark.svg"><source media="(prefers-color-scheme: light)" srcset=".github/assets/icon-timer-light.svg"><img alt="" src=".github/assets/icon-timer-light.svg" width="20" height="20"></picture>&nbsp; <a href="https://mrdarrengriffin.github.io/facet/docs/screens/timer/"><b>Timer</b></a><br>
A running timer as the hero, with one-tap presets when nothing is running.</p>
</td>
</tr>
</table>

<picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/icon-toasts-dark.svg"><source media="(prefers-color-scheme: light)" srcset=".github/assets/icon-toasts-light.svg"><img alt="" src=".github/assets/icon-toasts-light.svg" width="20" height="20"></picture>&nbsp; **[Toasts](https://mrdarrengriffin.github.io/facet/docs/screens/toasts/)** sit over any screen: a timer finishing, someone at the door, an agent that needs you.

## <picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/icon-settings-dark.svg"><source media="(prefers-color-scheme: light)" srcset=".github/assets/icon-settings-light.svg"><img alt="" src=".github/assets/icon-settings-light.svg" width="20" height="20"></picture>&nbsp; Design system

One `tokens.json` drives everything: CSS variables for the web, ESPHome YAML for the firmware.
One hero per screen in a large light weight, everything else quiet, and orange only for what
matters right now.

<p align="center">
  <a href="https://mrdarrengriffin.github.io/facet/design-system/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/palette-dark.png"><img alt="Facet type and colours: the time in the light display style, a headline, body text, a caps label and ten colour swatches" src=".github/assets/palette-light.png" width="920"></picture></a>
</p>

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

The images in this README come from the site itself. After a design change, `pnpm readme:assets`
builds the site and regenerates them in `.github/assets/` (it needs Playwright and Chromium; see
the top of `scripts/readme-assets.mjs`).

### Versioning

Facet is a single version for now (`0.1.0`) with a hand-written [CHANGELOG](CHANGELOG.md).

### Site

The site is deployed to GitHub Pages from `main` and served under `/facet/` (GitHub Pages paths follow the repository name). It may move to its
own domain later, at which point the Astro `site` and `base` settings in `site/astro.config.mjs`
will change.

## Inspiration

Facet is inspired by [ESPControl](https://github.com/jtenniswood/espcontrol) by jtenniswood, an
ESPHome-based smart home control panel. It's a lovely project and well worth a look, especially if
you want a panel you can set up today: [jtenniswood.github.io/espcontrol](https://jtenniswood.github.io/espcontrol/).
Facet isn't affiliated with it.

## Contributing

Ideas and issues are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) and the
[Code of Conduct](CODE_OF_CONDUCT.md).

## Licence

[MIT](LICENSE) © Darren Griffin. Bundled fonts (Inter, JetBrains Mono) are under the SIL Open
Font License.
