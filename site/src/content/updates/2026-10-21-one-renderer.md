---
title: "One renderer for browser and panel"
date: "2026-10-21"
time: "14:14"
kind: "Story"
milestone: true
links: [["Foundations", "foundations/"], ["Components", "components/"]]
---
Facet used to be built twice, once for the browser and again for the panels, and the two kept drifting apart. Now everything is written once and compiled for both, so this site, the workbench and the wall panels show exactly the same pixels. It was rebuilt from a clean slate, one piece at a time: foundations, then components, then screens.
