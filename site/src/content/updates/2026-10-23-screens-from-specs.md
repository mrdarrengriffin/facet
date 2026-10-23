---
title: "Rooms without a reflash"
date: "2026-10-23"
time: "00:05"
area: "Panels"
---
Screens are now described in a small JSON file, and the panel builds them from its own components. Adding a room, changing what's on a screen or pointing it at different lights no longer needs new firmware: the workbench checks the file, renders it at the panel's size, asks the panel what it can show, then sends it through Home Assistant. The panel checks it again, builds it out of sight, and only then swaps it in, so a half-sent or broken screen never appears. The rooms that come with the firmware are always one tap of Revert away. One Light screen now serves every light, and the Office room was added this way, without touching the firmware.
