---
title: "Home: cards line up in a new layout"
date: "2026-10-26"
time: "23:50"
kind: "Improved"
image: "2026-10-26-home-cards-layout.png"
alt: "Two cards side by side, and a third card alone on the row below, filling its whole width"
links: [["Cells", "components/cells/"], ["Home", "screens/home/"]]
---
Home now lays out tiles and cards with Cells, a new layout component: three tiles or two cards to a row, sharing the
width evenly at both panel sizes, and two readouts can share one card slot. A card left on its own on the last row now
fills the whole row instead of leaving a gap; tiles on a short row keep their column width.
