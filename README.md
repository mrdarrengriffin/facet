<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset=".github/assets/facet-logo-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset=".github/assets/facet-logo-light.svg">
    <img alt="Facet" src=".github/assets/facet-logo-light.svg" width="182" height="64">
  </picture>
</p>

<p align="center">
  Wall panel UI for Home Assistant.<br>
  One C++/LVGL renderer that runs on ESP32 touch panels and, compiled to WebAssembly, in the browser.
</p>

<p align="center">
  <a href="https://mrdarrengriffin.github.io/facet/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/pill-docs-dark.png"><img alt="Docs" src=".github/assets/pill-docs-light.png" height="36"></picture></a> &nbsp;
  <a href="https://mrdarrengriffin.github.io/facet/components/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/pill-components-dark.png"><img alt="Components" src=".github/assets/pill-components-light.png" height="36"></picture></a> &nbsp;
  <a href="https://mrdarrengriffin.github.io/facet/roadmap/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/pill-roadmap-dark.png"><img alt="Roadmap" src=".github/assets/pill-roadmap-light.png" height="36"></picture></a>
</p>

<br>

<p align="center">
  <a href="https://mrdarrengriffin.github.io/facet/screens/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screens-dark.png"><img alt="Facet screens rendered by the renderer: Home, Media, Weather, Clock, Light, Energy, Calendar and Timer" src=".github/assets/screens-light.png"></picture></a>
</p>

<p align="center"><sub>Rendered by the same code that runs on the panel. Docs and design system: <a href="https://mrdarrengriffin.github.io/facet/">mrdarrengriffin.github.io/facet</a></sub></p>

<br>

> **Everything here is a work in progress.** Facet is being built actively: new screens are still
> being made and existing ones change as they get used on real panels, so expect things to move
> around. The [docs site](https://mrdarrengriffin.github.io/facet/) runs the renderer live in your browser: every component and screen
> there is the real thing, not a picture of it.

## <picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/icon-light-dark.svg"><source media="(prefers-color-scheme: light)" srcset=".github/assets/icon-light-light.svg"><img alt="" src=".github/assets/icon-light-light.svg" width="20" height="20"></picture>&nbsp; The idea

A small screen in every room, each one showing what matters in that room right now: the music
that's playing, a kitchen timer, the lights, the next thing in the calendar. When nothing needs
you, the screen goes quiet.

- Glanceable from across the room
- Big touch targets, one action per card
- Small, purposeful animations and sounds
- Built for the panel: every colour designed for 16-bit displays, every redraw kept small

## <picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/icon-settings-dark.svg"><source media="(prefers-color-scheme: light)" srcset=".github/assets/icon-settings-light.svg"><img alt="" src=".github/assets/icon-settings-light.svg" width="20" height="20"></picture>&nbsp; How it works

Facet is one renderer, written in C++ on [LVGL](https://lvgl.io). The same source builds two ways:

- **On the panel**, as an ESPHome external component (`firmware/`). The panel reads Home Assistant
  itself (weather, calendars, media, lights, energy) and works the moon out on its own.
- **In the browser**, compiled to WebAssembly. The docs site and the workbench use this build, so
  what you see there is pixel for pixel what the panel draws.

Everything is built from one set of tokens (colour, type, space, layout, motion, sound) and one set
of components, so every screen looks and behaves the same at either size.

**Specs.** Screens are JSON specs: a tree of real components with bindings to entities
(`renderer/specs/`). A spec names what to show, not how to draw it, so a new screen is a new
arrangement of components you already have.

**Send API.** A panel running Facet has a small HTTP API, so you can show something on it from
anywhere:

| Endpoint | What it does |
|---|---|
| `POST /fw/show` | show a component or screen with props: `{"id": "spec", "props": {"screen": "weather"}}` |
| `POST /fw/spec` | install a screen spec (`kind: screen`), the panel's set of screens (`kind: deck`), or check one (`kind: check`) |
| `GET /fw/deck` | the panel's set of screens |
| `GET /fw/info`, `/fw/meta` | what the panel runs and what it can show |
| `GET /fw/schema` | everything this firmware accepts, in one file (every request is listed there) |

Changes from anywhere but the panel's own editor are gated behind an **Allow API access** switch on the
device (off by default) and a key per agent, asked for by name and allowed with a tap on the panel. Agents start at
the panel's `/ai` (one prompt to paste) or `/llms.txt`.

## <picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/icon-home-dark.svg"><source media="(prefers-color-scheme: light)" srcset=".github/assets/icon-home-light.svg"><img alt="" src=".github/assets/icon-home-light.svg" width="20" height="20"></picture>&nbsp; Screens

Each screen has a page in the docs with a live preview you can tap and drag, what it shows and the
data it reads. A **BETA** chip on a picture: the screen is on the beta firmware, not yet in a stable release.
**COMING SOON**: it is in the docs, not on a released firmware yet. The same marks show on the panel features below.

<table>
<tr>
<td width="50%" valign="top">
<a href="https://mrdarrengriffin.github.io/facet/screens/home/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screen-home-dark.png"><img alt="Home screen" src=".github/assets/screen-home-light.png" width="200"></picture></a>
<p><a href="https://mrdarrengriffin.github.io/facet/screens/home/"><b>Home</b></a><br>
The room's cards, each one a way into a screen.</p>
</td>
<td width="50%" valign="top">
<a href="https://mrdarrengriffin.github.io/facet/screens/media/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screen-media-dark.png"><img alt="Media screen" src=".github/assets/screen-media-light.png" width="200"></picture></a>
<p><a href="https://mrdarrengriffin.github.io/facet/screens/media/"><b>Media</b></a><br>
Now playing on a turning CD, record or cassette with the album art, with transport controls and live volume.</p>
</td>
</tr>
<tr>
<td width="50%" valign="top">
<a href="https://mrdarrengriffin.github.io/facet/screens/light/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screen-light-dark.png"><img alt="Light screen" src=".github/assets/screen-light-light.png" width="200"></picture></a>
<p><a href="https://mrdarrengriffin.github.io/facet/screens/light/"><b>Light</b></a><br>
One light on a dial: drag the ring for brightness, tap to switch it.</p>
</td>
<td width="50%" valign="top">
<a href="https://mrdarrengriffin.github.io/facet/screens/clock/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screen-clock-dark.png"><img alt="Clock screen" src=".github/assets/screen-clock-light.png" width="200"></picture></a>
<p><a href="https://mrdarrengriffin.github.io/facet/screens/clock/"><b>Clock</b></a><br>
The time as the one hero on the screen. Also the idle screen.</p>
</td>
</tr>
<tr>
<td width="50%" valign="top">
<a href="https://mrdarrengriffin.github.io/facet/screens/timer/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screen-timer-dark.png"><img alt="Timer screen" src=".github/assets/screen-timer-light.png" width="200"></picture></a>
<p><a href="https://mrdarrengriffin.github.io/facet/screens/timer/"><b>Timer</b></a><br>
A running timer as the hero, with one-tap lengths when nothing is running.</p>
</td>
<td width="50%" valign="top">
<a href="https://mrdarrengriffin.github.io/facet/screens/weather/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screen-weather-dark.png"><img alt="Weather screen" src=".github/assets/screen-weather-light.png" width="200"></picture></a>
<p><a href="https://mrdarrengriffin.github.io/facet/screens/weather/"><b>Weather</b></a><br>
The temperature, one line, rain in the next hour and the hours ahead.</p>
</td>
</tr>
<tr>
<td width="50%" valign="top">
<a href="https://mrdarrengriffin.github.io/facet/screens/weather/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screen-weather_days-dark.png"><img alt="Weather, 5 days screen" src=".github/assets/screen-weather_days-light.png" width="200"></picture></a>
<p><a href="https://mrdarrengriffin.github.io/facet/screens/weather/"><b>Weather, 5 days</b></a><br>
The next five days from the Weather screen, each with its high, low and rain.</p>
</td>
<td width="50%" valign="top">
<a href="https://mrdarrengriffin.github.io/facet/screens/moon/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screen-moon-dark.png"><img alt="Moon screen" src=".github/assets/screen-moon-light.png" width="200"></picture></a>
<p><a href="https://mrdarrengriffin.github.io/facet/screens/moon/"><b>Moon</b></a><br>
Tonight's moon phase, worked out on the panel with no API.</p>
</td>
</tr>
<tr>
<td width="50%" valign="top">
<a href="https://mrdarrengriffin.github.io/facet/screens/overhead/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screen-overhead-dark.png"><img alt="Overhead screen" src=".github/assets/screen-overhead-light.png" width="200"></picture></a>
<p><a href="https://mrdarrengriffin.github.io/facet/screens/overhead/"><b>Overhead</b></a><br>
The aircraft flying over your home on a radar: the closest one, where to look for it, its height and speed.</p>
</td>
<td width="50%" valign="top">
<a href="https://mrdarrengriffin.github.io/facet/screens/calendar/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screen-calendar-dark.png"><img alt="Calendar screen" src=".github/assets/screen-calendar-light.png" width="200"></picture></a>
<p><a href="https://mrdarrengriffin.github.io/facet/screens/calendar/"><b>Calendar</b></a><br>
What's next, then today and the days ahead as an agenda or a month.</p>
</td>
</tr>
<tr>
<td width="50%" valign="top">
<a href="https://mrdarrengriffin.github.io/facet/screens/energy/"><picture><source media="(prefers-color-scheme: dark)" srcset=".github/assets/screen-energy-dark.png"><img alt="Energy screen" src=".github/assets/screen-energy-light.png" width="200"></picture></a>
<p><a href="https://mrdarrengriffin.github.io/facet/screens/energy/"><b>Energy</b></a><br>
Today's cost so far, the price you're on and today's half-hours.</p>
</td>
</tr>
</table>

### On every panel

Not screens you add: what every panel has, most of it in Settings.

| Feature | What it is |
|---|---|
| [Set up](https://mrdarrengriffin.github.io/facet/screens/setup/) | What a new panel shows until it is on Wi-Fi and added to Home Assistant, one step at a time. |
| [Settings](https://mrdarrengriffin.github.io/facet/screens/settings/) | Every setting on the panel itself: theme, accent, sounds, screens, idle and updates. |
| [Screens and Home](https://mrdarrengriffin.github.io/facet/screens/settings/) | Settings > Screens: every screen of the panel with where its data comes from, on or off; tap one for its source and options. |
| [Idle clock](https://mrdarrengriffin.github.io/facet/screens/idle/) `BETA` | The Clock without its header, shown when no one has touched the panel for a while, or the album art while music plays. |
| [Home Assistant link](https://mrdarrengriffin.github.io/facet/screens/settings/) `BETA` | Settings > Home Assistant: the panel's own link to Home Assistant, its address and token kept on the panel. |
| [API keys](https://mrdarrengriffin.github.io/facet/screens/settings/) `BETA` | Settings > API keys: the keys scripts, automations and agents use to change the panel; you allow each on the glass, and revoke them here. |
| [Update channels](https://mrdarrengriffin.github.io/facet/screens/settings/) `BETA` | Settings > Update channel: Stable releases, or Beta and Dev builds that come more often. |
| [Report a problem](https://mrdarrengriffin.github.io/facet/screens/settings/) `COMING SOON` | Settings > Report a problem: the address of the panel's report page, with its logs kept across restarts and an issue filled in. |

More screens are on the way (thermostat, lock, vacuum, cameras, plants and others): see the
[roadmap](https://mrdarrengriffin.github.io/facet/roadmap/).

## Panels

| Panel | Screen | Status |
|---|---|---|
| Waveshare ESP32-P4-86 | 4", 720 x 720 | Supported: every released screen above |
| Guition ESP32-4848S040 | 4", 480 x 480 | Supported: every released screen above (a screen change takes 0.1 to 0.2 s, the S3 draws more slowly) |

Both panels get the same layout: the 480 panel is the reference size, the 720 panel draws it at 1.5x.
Other sizes are planned.

## Repository

| Folder | What |
|---|---|
| `renderer/` | the C++/LVGL renderer: core, foundations, components, screens, specs, tokens and the tools that check them. Developer notes: [renderer/README.md](renderer/README.md) |
| `firmware/` | the ESPHome external component and the bundler that packs the renderer into it |
| `esphome/` | the panel firmware: one YAML per board and the packages they share ([esphome/README.md](esphome/README.md)) |
| `workbench/` | the local web app for working on Facet: every part live, the editor, specs, tests, golden review ([workbench/README.md](workbench/README.md)) |
| `boards/` | one file per supported panel (chip, resolution, scale, touch, flash, status), read by everything that lists boards |
| `site/` | the docs site (Astro), generated from what the renderer registers |

The earlier Facet, a TypeScript design system with the same screens, lives on the
[`prototype`](https://github.com/mrdarrengriffin/facet/tree/prototype) branch.

## Contributing

Help of any kind is welcome, and most of it needs no code: report a bug, try Facet on your panel and say how it went,
fix a line in the docs (every docs page has an **Edit this page** link), or share an idea for a screen.
[CONTRIBUTING.md](CONTRIBUTING.md) has every way in, from a two-minute report to building the renderer, and the docs
have the same guide: [Contributing](https://mrdarrengriffin.github.io/facet/contributing/).

## Inspiration

Facet learns a lot from other open-source panel projects: thanks to everyone who builds and shares them.

## Support

Facet is free and built in my spare time, with a lot of help from Claude. If it's useful to you,
a coffee helps cover the tokens: [Buy Me a Coffee](https://buymeacoffee.com/mrdarrengriffin) ·
[Ko-fi](https://ko-fi.com/mrdarrengriffin).

## Licence

MIT © Darren Griffin (`LICENSE`). LVGL, ArduinoJson, the image decoders and the fonts (SIL Open Font License) keep
their own licences: `THIRD_PARTY.md`.
