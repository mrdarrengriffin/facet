# Third-party sources

Vendored as released, unchanged, so the renderer builds without ESPHome's component cache. Each keeps its licence.

| Dir | Version | From | Kept |
|---|---|---|---|
| `lvgl/` | 9.5.0 | https://github.com/lvgl/lvgl/archive/refs/tags/v9.5.0.tar.gz (sha256 `34a955cdf3a2d005507b704e87357af669a114523b6d3f77b5344fdc68717bc6`) | `src/`, `lvgl.h`, `lv_version.h`, `lvgl_private.h`, `LICENCE.txt` (MIT), `COPYRIGHTS.md` |
| `arduinojson/` | 7.4.3 | https://github.com/bblanchon/ArduinoJson/archive/refs/tags/v7.4.3.tar.gz (sha256 `a2a8a277fc3cd0404499199bf1468682ea3688f354be0eaf6af8648366911257`) | `src/`, `LICENSE.txt` (MIT) |

The panel firmware gets the same versions from ESPHome (its `managed_components`); keep the two in step. To update: replace
the kept files from the new release tarball and change this table.
