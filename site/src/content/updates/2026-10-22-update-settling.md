---
title: "The panel tells you when an update is settling"
date: "2026-10-22"
time: "18:52"
area: "Panels"
links: []
---
After a firmware update, a panel runs the new build on probation for three minutes. If it restarts in that time, it goes back to the previous build, which keeps a bad update from leaving it stuck. That's easy to trip over without knowing, so the panel now shows a small "Update settling: don't unplug" pill with a countdown over whatever is on screen. It turns green with "Update confirmed" when the build is kept. Home Assistant gets a matching "Firmware on probation" sensor too.
