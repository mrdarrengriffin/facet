# Updates entries

The Updates page is what's new in Facet, newest first, ending with how it began: one short Markdown file per entry,
`YYYY-MM-DD-slug.md`. It's for people using Facet, not a journal: every entry says something worth saying.

## What earns an entry

- A new screen, component or feature; a screen with a new look; a fix someone would notice; a new docs page.
- Not tooling, process, tests, refactors or internal rules. Fold those into the visible change they enabled, or into
  one occasional "Behind the scenes" entry. Several small related changes on one day = **one** entry.
- Before adding one, look at the latest entries: if yours extends one from the same day, edit that one instead.

## How to write it

- **Title** = what a user gets, plain, 2-6 words. Lead with the thing: "New screen: Settings", "Calendar: new
  Month view", "Media screen controls your speakers", "Smoother curves". Not a label ("Large text", "Card"), not a
  list of unrelated changes joined with "and", no internal names (spec, renderer, WASM, LVGL, port).
- **kind** = one of: `New screen`, `New feature`, `New component`, `Improved`, `Fixed`, `Design system`, `Docs`,
  `Story` (how Facet began), `Behind the scenes`. Drawn as a chip; the page filters by it.
- **Body** = 1-3 short sentences, benefit first ("Play, pause and volume now work the real speaker."), then the one
  detail that makes it interesting. No implementation detail, no "now runs on the panel renderer".
- **One good picture** where there is something to see: the feature itself, captured from the live renderer
  (the docs site's screen / component canvas at 720, dark), square or whole, never a page crop that cuts a panel or
  shows half the site chrome. Sample data only: no real names, rooms, speakers, events or places from a real home.
- **milestone: true** only for real turning points (a handful in all): a tinted box.
- Links: the docs page(s) for what changed, checked to exist (paths use hyphens: `components/clock-face/`).
- Public words only: no board ids, no quotes from chat.

```md
---
title: "Calendar: new Month view"
date: "2026-10-23"          # quoted
time: "08:10"               # local time, optional
kind: "Improved"
milestone: true             # optional, rare
image: "2026-10-23-calendar-month.png"     # or a pair: before: / after:
alt: "The Calendar Month view: six weeks, a dot per event"
links: [["Calendar", "screens/calendar/"]]
---
Switch between Agenda and Month in the header. Month shows six weeks with a dot for each event; tap a day to see
what's on.
```

Pictures: PNG in `site/public/updates/`, at most 720 px wide (shown at up to 480), named
`<date>-<slug>[-before|-after].png`. Capture a docs canvas: open the screen or component page with the docs prefs
set to dark / 720 and screenshot the `canvas.live__canvas` element. Check it for anything private before adding.
