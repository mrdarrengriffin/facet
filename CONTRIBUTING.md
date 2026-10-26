# Contributing to Facet

Thanks for wanting to help. Facet is early and small, so every bit of help counts, and you don't need to write code
to make a difference. Pick whatever suits you:

| I want to... | Start here |
|---|---|
| Tell you something is broken | [Report a bug](#report-a-bug) |
| Try Facet on my panel | [Test on a device](#test-on-a-device) |
| Fix a typo or explain something better | [Improve the docs](#improve-the-docs) |
| Write code | [Contribute code](#contribute-code) |
| Suggest a screen, a component or a better way | [Share an idea](#share-an-idea) |

Not sure where something fits? Open an issue and ask. There are no silly questions.

## Report a bug

[Open a bug report](https://github.com/mrdarrengriffin/facet/issues/new?template=bug.yml). The most useful reports say:

- what you did, what you expected, and what happened instead;
- where: which panel (or the docs site, and which browser);
- the firmware version, if it happened on a panel (Home Assistant shows it on the panel's device page);
- a photo or screenshot, if it is something you can see.

## Test on a device

Real panels find things the browser never will, so test reports are gold.

**Panels:** the [Waveshare ESP32-P4-86](https://mrdarrengriffin.github.io/facet/install/) (4", 720 x 720) and the
Guition ESP32-S3-4848S040 (4", 480 x 480). Other panels are planned; if you have one, an issue saying so helps us
decide what comes next.

**Install the preview build** from your browser on the [Install](https://mrdarrengriffin.github.io/facet/install/)
page (Chrome or Edge, USB-C data cable). It replaces what is on the panel now. Updates then arrive in Home Assistant.

**What to try:** go round every screen, tap, swipe and long press, switch dark and light, and leave it running for a
day. Things worth noticing: anything that looks off (clipped text, misaligned edges, wrong colours), anything slow or
janky, controls that don't do what they say, and anything that reboots or freezes.

**Report it** with the [device test report](https://github.com/mrdarrengriffin/facet/issues/new?template=device-test.yml):
panel model, firmware version, what worked, what broke. "Everything worked" is a useful report too.

## Improve the docs

Every page on the [docs site](https://mrdarrengriffin.github.io/facet/) has an **Edit this page on GitHub** link at the
bottom. It opens the file that holds that page's text: edit it in the browser and GitHub turns it into a pull request
for you. Spotted something but don't want to edit? Use **Report a problem** next to it, or
[open a docs fix](https://github.com/mrdarrengriffin/facet/issues/new?template=docs.yml).

Some pages are generated: a component's description comes from the comment at the top of its `.cpp` file, a screen's
from its spec in `renderer/specs/`. The edit link takes you to the right file either way.

## Contribute code

### How the repository fits together

| Folder | What |
|---|---|
| `renderer/` | the C++/LVGL renderer: core, foundations, components, screens, specs, tokens and the tools that check them ([renderer/README.md](renderer/README.md)) |
| `firmware/` | the ESPHome external component and the bundler that packs the renderer into it |
| `site/` | the docs site (Astro), generated from what the renderer registers |

### Work on the docs site

The site needs no C++ toolchain: it uses the published WASM build.

```sh
gh release download docs-build -p docs-build.tar.gz -R mrdarrengriffin/facet
mkdir -p renderer/out/docs && tar -xzf docs-build.tar.gz -C renderer/out/docs
cd site && npm ci
FW_OUT=renderer/out npm run dev     # or: npm run build
```

### Work on the renderer

The renderer builds to WebAssembly with Emscripten. For now it borrows LVGL, `lv_conf.h` and the fonts from an
ESPHome panel firmware build next to it (vendoring them is on the list); `local.env` at the repo root says where they
are. The variables are in [renderer/README.md, "Paths"](renderer/README.md#paths-localenv-untracked).

```sh
cd renderer/tools && npm install && cd ../..
renderer/build.sh 1.5x              # one build (1.5x = the 720 panel, 1x = the 480 panel); runs the audit
node renderer/watch.mjs             # rebuild on save while you work
```

If the build setup gets in your way, open an issue: getting a second person building Facet is worth fixing.

### Checks before a pull request

Facet's rules are checked by tools, not by memory. Run the ones your change touches:

```sh
node renderer/tools/features.mjs check     # every source in one feature (also runs in CI)
renderer/build.sh 1.5x && renderer/build.sh 1x   # build + audit: 4 px grid, clipping, symmetry, animation paths
node renderer/tools/contrast.mjs --summary # every colour pair, dark and light: 0 failures
node renderer/tools/spec_test.mjs          # screen specs
node renderer/tools/behaviour.mjs          # taps, drags, scrolling, sounds, the update path
node renderer/tools/visual.mjs             # visual regression
renderer/tools/asan.sh                     # memory errors, before anything goes on a panel
```

The house rules, in short: every value comes from a token, sizes sit on the 4 px grid, everything works at 720 and
480 in dark and light, and screens are made of real components (a new arrangement is a new layout component). The
[renderer README](renderer/README.md) has the detail.

### Pull requests

1. Fork, then branch from `main`.
2. Keep each pull request to one thing: one fix, one component, one page.
3. Say what changed and why; for anything visible, add a screenshot at 720 and 480.
4. Commit messages: a short summary line, `Area: what changed` (for example `Docs: contributing page` or
   `Toggle: thumb follows the drag`), then more detail below if it helps.

Small pull requests get reviewed fastest. For anything big, open an issue first so we can agree the shape before
you spend the time.

## Share an idea

New screens, components, panels, sounds, better words: [open an idea](https://github.com/mrdarrengriffin/facet/issues/new?template=idea.yml).
The [roadmap](https://mrdarrengriffin.github.io/facet/roadmap/) shows what is available, in progress and planned.

## Be kind

Everyone taking part agrees to the [Code of Conduct](CODE_OF_CONDUCT.md). By contributing you agree your work is
released under the project's [MIT licence](README.md#licence).
