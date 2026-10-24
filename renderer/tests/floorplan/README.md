# Floor plan generator fixtures

Inputs for `tools/floorplan_test.mjs` (the generator in `core/floorplan_auto.cpp`, run through the WASM build). Only
public sample maps with licences that allow redistribution; each keeps its licence file next to it.

| File | Brand / source | What it is | Licence |
|---|---|---|---|
| `valetudo/roborock_s5.json` | Roborock S5 (firmware 2008), from [Valetudo](https://github.com/Hypfer/Valetudo) `backend/test/lib/robots/roborock/res/map/S5_FW2008_with_segments.json` | ValetudoMap JSON: 2 segments, floor + wall layers, robot, charger, path | Apache-2.0 (`valetudo/LICENSE`), unmodified |
| `valetudo/dreame_d9.json` | Dreame D9 (firmware 1058), Valetudo `backend/test/lib/robots/dreame/res/map/d9_1058_with_custom_named_segments.json` | 6 segments, two named (Bad, Flur), walls, robot, charger, path | Apache-2.0, unmodified |
| `valetudo/viomi_v7.json` | Viomi V7 (3irobotix), Valetudo `backend/test/lib/robots/3irobotix/res/map/viomi_v7_47_cleaned_segment_ids.json` | 2 named segments, a large unsegmented floor area | Apache-2.0, unmodified |
| `images/xiaomi_map_rooms.png` | Xiaomi / Roborock cloud map as drawn by the [Xiaomi Cloud Map Extractor](https://github.com/PiotrMachowski/Home-Assistant-custom-components-Xiaomi-Cloud-Map-Extractor) `images/map_rooms.png` | a real renderer's picture: 9 rooms in 6 colours (colours reused by rooms that don't touch), black walls, grey obstacles, path, robot | MIT (`images/LICENSE`), unmodified |
| `polygons/outlines_mm.json` | made up for these tests | 4 outlines in mm, y up (Ecovacs-style data), a slanted corner and an overlap | MIT (this repo) |

The picture tests also draw each Valetudo map the way two renderer families do (a Roborock-like palette by room id
with path + robot over it at 4x, a Dreame-like four-colour palette on transparent at 3x; colours from the open-source
map parsers: `vacuum-map-parser-base`, Apache-2.0, and Tasshack's `dreame-vacuum`, MIT), so the picture adapter is
checked against the exact masks of the same map.

## Looked at, not vendored
- **Ecovacs** (`deebot-client` test maps): GPL-3.0, not compatible with this MIT repo. The `polygons` adapter covers
  outline data; a real Ecovacs sample needs a permissively licensed map first.
- **Dreame camera entity pictures** (Tasshack's integration): no sample pictures in the repo; the Dreame-style
  renderer above uses its documented colour scheme instead.
- **Home Assistant core Roborock tests**: the mock map data is not a real map.
