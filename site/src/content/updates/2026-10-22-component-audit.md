---
title: "Every component is checked automatically"
date: "2026-10-22"
time: "20:25"
area: "Foundations"
---
Every build now checks every component, in each of its variants, in both themes and at both panel sizes. It catches anything drawn outside its box or cut off (including text), shapes off the 4 px grid, touch targets that are too small, ring marks that don't line up at 12, 3, 6 and 9 o'clock, animations that leave their path, and pixels left behind by a redraw. A component isn't shown until it passes. The first run found and fixed a tick ring that was a pixel out, a disc marker off its circle, and text that lost its descenders.
