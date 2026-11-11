# Facet 1.0

What 1.0 is. Every scope decision is checked against this page; if a card or a plan disagrees with it, this page
wins or it is changed with Darren. Until 1.0, Facet as a whole is a beta product: 0.x feature releases (0.6, 0.7…),
one per milestone, with fix releases (0.6.1) between them only when something can't wait (RELEASING.md "Versions").

## When it is 1.0

When the editor (#82) and config backup (#170) are done, every test passes, and there have been a few weeks without
a nasty bug: when you'd give it to a stranger with no caveats. After 1.0, the same pattern (1.1, 1.0.1); 2.0 only if
decks or settings ever break.

## Who installs it, and how

Someone with Home Assistant and one of the 4" touch panels Facet runs on so far (two):

- Waveshare ESP32-P4-86 (720 x 720)
- Guition ESP32-S3-4848S040 (480 x 480)

They install Facet from the docs site's Install page in Chrome or Edge over a USB-C data cable. No ESPHome setup, no
build tools, no hub.

Both panels get the same layout; the 480 panel is the reference size and the 720 panel draws it at 1.5x.

## Day one

- First boot reaches Set up without help: join Wi-Fi, link Home Assistant, land on Home with the factory deck.
- The screens: Home, Clock, Timer, Light, and a room of lights, Media, Weather, Moon, Calendar, Energy, plus Settings, Set up and toasts.
- The panel's own editor, opened from the panel's address in a browser: arrange the Homes and their cards, put your
  own screens in order and add or remove them (every built-in screen is always there, opened from a card), set each
  screen's settings (entities, source, options) and Home's header buttons, then save to the panel in one step; the panel checks it and keeps the version before.
- Dark, light and auto; the accent sets.
- Updates arrive in Home Assistant and install from there; a bad update rolls back to the previous image.

## What "works" means

The release quality checklist (RELEASING.md), run by Darren on a fresh Waveshare and a fresh Guition. Every line
is a yes or the release waits. Config backup (#170) is part of 1.0: export and import of the panel's setup from the
editor.

## What 1.0 is not

- The 23 unfinished features (`firmware: false` in `renderer/features.json`): they live on the `wip` branch and
  come back one at a time after the release. Blinds and curtains joins 1.0 only if it works against a real cover
  entity.
- Theme files and the theme editor.
- The composer: building new screens from parts, header slots.
- The agent API beyond what already ships behind the "Allow API access" switch and its keys.
- Other panels and sizes.
