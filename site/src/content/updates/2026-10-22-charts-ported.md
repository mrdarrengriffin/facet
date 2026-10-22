---
title: "Charts on the panel renderer"
date: "2026-10-22"
time: "20:55"
area: "Components"
image: "2026-10-22-charts-ported.png"
alt: "Moon by day: a bar per evening, full moons highlighted, a moon over each new and full moon"
links: [["Bar chart", "components/bar_chart/"], ["Moon by day", "components/moon_range/"]]
---
The chart components now run on the panel renderer: the bar chart, sparkline, big number, delta chip, rate strip, donut, legend and axis, and the Moon screen's long-range chart. They all come from one shared charts library, the same one the Charts page shows, so bars, lines, dots and labels look and move the same everywhere. Bars grow, lines draw in, numbers count up, and you can drag across a chart to read it.
