---
title: "Curves are smooth now"
date: "2026-10-22"
time: "12:37"
area: "Design"
before: "2026-10-22-smooth-curves-before.png"
after: "2026-10-22-smooth-curves-after.png"
alt: "The ease-out curve, zoomed in"
links: [["Motion", "foundations/motion/"], ["Dial", "components/dial/"]]
---
LVGL's line drawing snaps every point to a whole pixel, so curves came out with kinks. Curves, arcs and diagonals now go through one smooth-stroke helper that draws them once, anti-aliased, like an icon.
