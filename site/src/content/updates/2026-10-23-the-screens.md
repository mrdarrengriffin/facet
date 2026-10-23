---
title: "The screens come across"
date: "2026-10-23"
time: "06:00"
area: "Screens"
links: [["Clock faces", "components/clock_face/"], ["Weather view", "components/weather_view/"], ["Thermostat view", "components/thermostat_view/"], ["Timer view", "components/timer_view/"], ["Spec screens", "screens/spec/"]]
---
The screens from the first Facet are now part of the new renderer: Clock, Timer, Moon, Weather, Lightning, Thermostat, Blinds and curtains, Media, Calendar, Energy, GitHub and 3D print.

Each screen is a short spec (its header, what it shows, and which Home Assistant entity it reads) on top of one body component, which is built from the same parts as everything else. A screen's More rows and rows open its other views: Moon details, the next five days, a pull request.

- The Clock has its orbit face back, with the weather under the time.
- The Weather screen has its Weather, Sun, Moon and Lightning views. The sun's arc and times, and everything about the moon, are worked out on the panel.
- The Thermostat sends its set point, mode and preset straight to Home Assistant.

Forecasts, calendar events, energy and GitHub use sample data until each source is connected.
