# Updates entries

The Updates page is Facet's story as a timeline, newest first: one short Markdown file per milestone or visible
change, `YYYY-MM-DD-slug.md`. A highlights reel, not a journal: a title and two or three plain sentences. Public words
only (no board ids, no internal labels); quotes only when they are verbatim and about the design.

```md
---
title: "Curves are smooth now"
date: "2026-10-22"          # quoted
time: "12:37"               # local time, optional (leave out when unknown)
area: "Design"              # Beginnings, Panels, Design, Foundations, Components, Screens, Sound, Site, ...
milestone: true             # optional: a turning point, drawn as a tinted box with a dot (use rarely)
before: "2026-10-22-smooth-curves-before.png"   # a pair, files in site/public/updates/
after: "2026-10-22-smooth-curves-after.png"
# or one picture: image: "2026-10-22-segmented.png"
alt: "The ease-out curve, zoomed in"
links: [["Motion", "foundations/motion/"], ["Cambridge Dictionary", "https://dictionary.cambridge.org/..."]]
---
Two or three plain sentences.

> "A verbatim quote, if there is one."
```

Pictures: crop / resize a copy (never the original journal shot), PNG, at most 720 px wide; panel captures straight
from the site's canvases are best. Check them for anything private (place names, real calendar events, people,
devices in the house) before adding. Name them `<date>-<slug>[-before|-after].png`.
