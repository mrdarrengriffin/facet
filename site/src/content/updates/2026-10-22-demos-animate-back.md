---
title: "Every component moves in place"
date: "2026-10-22"
time: "16:10"
area: "Components"
links: [["Dial", "components/dial/"], ["Bottom sheet", "components/bottom_sheet/"], ["Slider", "components/slider/"]]
---
Every component now takes new values in place instead of being rebuilt: a dial or slider sweeps to its new value, a
switch slides, a choice moves, a control fading to disabled or unavailable does it where it stands. Leave a demo alone
and it glides back to where it started; a chart that simply replays its own animation no longer resets first. Updates
nobody touched stay silent, and the bottom sheet now opens only as tall as its list needs.
