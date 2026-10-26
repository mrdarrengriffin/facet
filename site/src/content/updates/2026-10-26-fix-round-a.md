---
title: "Fixes: light cards, blinds, header and the 5-day forecast"
date: "2026-10-26"
time: "14:45"
kind: "Fixed"
---

A round of fixes found by the new functional tests. Light cards now follow Home Assistant's answer straight after a
tap (Off after turning off, the brightness after turning on). Blinds and curtains show the real position and slat
angle from Home Assistant. A hold button only acts on the full hold; a quick tap does nothing. The "+N" row at the end
of a long choose-one list is no longer shown as a choice. The header no longer leaves an empty space where the timer
goes when no timer is running, and the 5-day forecast sits inside the screen's margins. A second Home page (such as
Kitchen) is now the parent of its own header shortcuts, so Back from its Timer returns to Kitchen.
