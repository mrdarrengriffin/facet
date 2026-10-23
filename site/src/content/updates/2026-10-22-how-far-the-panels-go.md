---
title: "How far the panels go"
date: "2026-10-22"
time: "20:30"
area: "Panels"
---
A new stress suite measures the hardware and the screen instead of our components. It adds moving things until the frame rate drops, grows one moving object from a speck to the whole screen for each kind of motion, and times raw full-screen fills, images, text and touch. Each panel gets a capability card with its limits: how many animated icons it holds at full speed, how large a fade or a slide can be, and where rotation stops being affordable. Later firmware changes show up on the card as shifts. The first cards: the smaller panel keeps about 16 animated icons at full speed and spends almost all of a full-screen redraw sending the picture to its rotated screen, while the larger one keeps about 32 and fills its screen in under 6 ms. On both, rotation is the expensive kind of motion.
