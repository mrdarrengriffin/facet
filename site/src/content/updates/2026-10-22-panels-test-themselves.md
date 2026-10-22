---
title: "The panels test themselves"
date: "2026-10-22"
time: "20:00"
area: "Panels"
---
Every component and screen now has a performance test: idle, animating, dragged and navigated, with frame time, redrawn area and memory, each checked against what each panel can do. The workbench sends the whole set to a panel in one go. The panel runs it on its own screen, measures itself and sends back the report, so its real numbers sit beside the browser's estimate. The first real runs made the estimate much better, found a crash on one demo page before anyone hit it, and showed that dropping the fades on the smaller panel makes its sheets and screen changes much quicker.
