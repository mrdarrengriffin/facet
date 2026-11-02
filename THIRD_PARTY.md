# Third-party code, fonts and data

Facet is MIT (`LICENSE`). These parts are other people's work, kept under their own licences; each file or folder
carries its licence text.

## In the renderer and the firmware

| What | Where | Licence |
|---|---|---|
| [LVGL](https://github.com/lvgl/lvgl) 9.5.0 | `renderer/third_party/lvgl/` (`LICENCE.txt`, `COPYRIGHTS.md` for the libraries inside it) | MIT |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) 7.4.3 | `renderer/third_party/arduinojson/` (`LICENSE.txt`) | MIT |
| [ESPHome](https://github.com/esphome/esphome) `mipi_rgb` display component, 2026.9.1 with three fixes | `esphome/components/mipi_rgb/` (`LICENSE`, `NOTICE`: the upstream commit and what changed) | GPLv3 (C++), MIT (Python) |

## Fonts

| Font | Where | Licence |
|---|---|---|
| [Inter](https://github.com/rsms/inter) | `renderer/fonts/Inter-*.ttf` (`OFL-Inter.txt`) | SIL OFL 1.1 |
| Inter and JetBrains Mono, latin subsets for the editor | `workbench/src/wasm/fonts/*.woff2` (`OFL-*.txt` beside them) | SIL OFL 1.1 |
| [JetBrains Mono](https://github.com/JetBrains/JetBrainsMono) | `renderer/fonts/JetBrainsMono-*.ttf` (`OFL-JetBrainsMono.txt`) | SIL OFL 1.1 |
| [Monocraft](https://github.com/IdreesInc/Monocraft) (Blockcraft theme) | `renderer/fonts/Monocraft.ttf` (`OFL-Monocraft.txt`) | SIL OFL 1.1 |
| [W95FA](https://fontsarena.com/w95fa-by-alina-sava/) (Win98 theme), modified to a pixel grid | `renderer/fonts/W95FA-cell.ttf` (`OFL-W95FA.txt`) | SIL OFL 1.1 |

## Build tools and test data (not in a build)

| What | Where | Licence |
|---|---|---|
| ESPHome's glyph rasteriser (2026.9.1) | `renderer/tools/esphome_font.py` (notice in the file) | MIT |
| [freetype-py](https://github.com/rougier/freetype-py), [esphome-glyphsets](https://github.com/esphome/esphome-glyphsets) | `renderer/tools/requirements.txt` (installed, not vendored) | BSD-3-Clause, Apache-2.0 |
| [resvg-js](https://github.com/thx/resvg-js), [Playwright](https://github.com/microsoft/playwright) | `renderer/tools/package.json` (installed, not vendored) | MPL-2.0, Apache-2.0 |
