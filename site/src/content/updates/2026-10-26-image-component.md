---
title: "In progress: an Image component"
date: "2026-10-26"
time: "14:30"
kind: "New component"
---

Screens can now show pictures from a web address. An Image is sized by a size step or by the grid cell it sits in,
and fills its box (cover), shows the whole picture (contain) or zooms pixel art by whole steps so it stays sharp. In a
grid it can repeat one picture per item, taking each address from the item. Pictures share one small memory budget
per panel, the smaller panel getting much less, and everything is freed when you leave the screen. While a picture
loads the box stays quiet; if it fails, a fallback icon and a few words show instead, and a picture that was already
there stays when a refresh fails. The panel side, fetching and decoding pictures itself, comes next.
