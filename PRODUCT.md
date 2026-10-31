# Facet 1.0

What the first release is. Every scope decision is checked against this page; if a card or a plan disagrees with
it, this page wins or it is changed with Darren.

## Who installs it, and how

Someone with Home Assistant and one of two 4" touch panels:

- Waveshare ESP32-P4-86 (720 x 720)
- Guition ESP32-S3-4848S040 (480 x 480)

They install Facet from the docs site's Install page in Chrome or Edge over a USB-C data cable. No ESPHome setup, no
build tools, no hub.

Both panels get the same layout; the 480 panel is the reference size and the 720 panel draws it at 1.5x.

## Day one

- First boot reaches Set up without help: join Wi-Fi, link Home Assistant, land on Home with the factory deck.
- The screens: Home, Clock, Timer, Light, and a room of lights, Media, Weather, Moon, Calendar, Energy, plus Settings, Set up and toasts.
- The panel's own editor, opened from the panel's address in a browser: choose which screens are in the deck and
  their order, set each screen's settings (entities, source, options), set the panel's settings (mode, accent,
  idle, Home header buttons). Send, check and revert, with the panel's validation shown plainly.
- Dark, light and auto; the accent sets.
- Updates arrive in Home Assistant and install from there; a bad update rolls back to the previous image.

## What "works" means

The release quality checklist (plan Phase 6.0), run by Darren on a fresh Waveshare and a fresh Guition. Every line
is a yes or the release waits.

## What 1.0 is not

- The 23 unfinished features (`firmware: false` in `renderer/features.json`): they live on the `wip` branch and
  come back one at a time after the release. Blinds and curtains joins 1.0 only if it works against a real cover
  entity.
- Theme files and the theme editor.
- The composer: building new screens from parts, header slots.
- The agent API beyond what already ships behind the "Allow AI agents" switch.
- Other panels and sizes.
