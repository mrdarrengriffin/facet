---
title: "From web components to WASM components"
date: "2026-10-21"
time: "14:14"
area: "Milestone"
milestone: true
links: [["Foundations", "foundations/"], ["Components", "components/"]]
---
Until now Facet was built twice: web components in TypeScript for the browser, then the same screens ported by hand to C++ for the panels, and the two kept drifting apart. From today every foundation and component is written once in C++ on LVGL and compiled to WebAssembly, so this site, the workbench and the wall panels all run one renderer and show exactly the same pixels. The first web version stays online as the design reference while each piece is rebuilt, one at a time.
