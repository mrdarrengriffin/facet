# Panel firmware

The ESPHome firmware that runs Facet on a panel: the same renderer as the docs site and the workbench, bundled into
an ESPHome component, plus each board's hardware.

| File | What it is |
|---|---|
| `facet-<board>.yaml` | The firmware the install page flashes, one per board in `../boards/` (no Wi-Fi or keys baked in) |
| `packages/board_<board>.yaml` | The board's hardware: display, touch, backlight, buttons, relays, memory settings |
| `packages/workbench.yaml` | What every board shares: LVGL, the `facet_wb` component, Home Assistant actions, OTA updates |
| `packages/net_public.yaml` | Onboarding: Improv serial, Improv BLE where the board has it, the setup hotspot |
| `packages/audio_<board>.yaml` | UI sounds on a board with a speaker |
| `board/` | Small C++ helpers the packages include (boot guard, setup hotspot and screen, S3 reset + update trial, direct RGB flush, LCD diagnostics) |
| `components/mipi_rgb/` | ESPHome's `mipi_rgb` display with three fixes and frame counters for the Guition (GPLv3, see its `NOTICE`) |
| `components/facet_audio/` | Plays the UI sounds from a bank in flash |

## Build

```bash
cd renderer/tools && npm ci && cd ../..
esphome/build.sh waveshare-p4-86            # or guition-4848s040; add `prepare` to only generate
```

`build.sh` generates `gen/<board>/facet_wb` (`firmware/bundle.mjs`), `gen/settings.yaml` (`firmware/settings_yaml.mjs`)
and, for a board with a speaker, the sound bank (`firmware/sound_bank.mjs`, rendered by the Facet synth, `renderer/tools/synth.ts`, in
headless Chromium), then runs `esphome compile`. Nothing it generates is committed.

## Updates on the Guition

Every reset on the Guition ends in a short deep sleep (a full hardware reset: the RGB panel does not come back from a
software one). The bootloader would roll back a new image across that sleep, so a new image is marked valid at once
and goes on an **update trial** instead (`board/fw_trial.h`): it must run healthy (90 s with Wi-Fi and Home
Assistant's API connected, or 10 min with Wi-Fi) within 3 boots, or the panel boots the previous image again.

`GET /fw/info` `lcd` counts frames the RGB panel's bounce buffers underran (`missed`), and which jobs ran in them
(`with`: refresh, flush, fetch, decode; `frames_with`: how many frames each job ran in). It should stay flat.

## Your own build

Keep your network and names in your own YAML and include these packages, as `facet-<board>.yaml` does. Set
`facet_gen` to the folder that holds `<board>/facet_wb` when it is not `gen/` here.
