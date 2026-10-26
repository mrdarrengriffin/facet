---
title: "Rotate the screen, and toasts from Home Assistant on the panel"
date: "2026-10-26"
time: "18:00"
kind: "New feature"
---
Settings has a Rotate screen choice (0, 90, 180 or 270 degrees) for a panel mounted on its side or upside down. It
applies at once without a restart, touch turns with the picture, and Home Assistant gets it as a select. Home
Assistant can now send a toast to the panel with the new facet_toast action (title, body, icon, kind, duration). Work
in progress features stay off the panel firmware: the build now refuses one that slips in.
