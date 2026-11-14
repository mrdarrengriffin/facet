#!/bin/bash
# Facet panel firmware (README.md): generate what the YAML needs, then compile with ESPHome.
#   esphome/build.sh <board-id> [prepare|config|compile]   default compile; board ids = boards/*.json
# prepare = gen/<board>/facet_wb (firmware/bundle.mjs, the release feature set at the board's scale),
# the panel's editor (workbench/scripts/editor-device.mjs into gen/<board>/facet_wb, FW_WEB=0 skips it),
# gen/settings.yaml (firmware/settings_yaml.mjs) and, on a board with a speaker, the sound bank in
# components/facet_audio (firmware/sound_bank.mjs, only when stale). All of it is generated, none of it committed.
# compile then appends the editor files to the images (firmware/web_append.mjs: they stay in flash, #185).
# Needs Node (renderer/tools: npm ci), the font rasteriser venv (renderer/tools/venv.sh) and `esphome` on PATH
# (or $ESPHOME). FACET_VERSION = this build's version, the one the Firmware entity reports (bundle.mjs --version;
# default the YAML's facet_version). A release also sets facet_version in the YAML.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$HERE")
id=${1:?usage: esphome/build.sh <board-id> [prepare|config|compile]}; step=${2:-compile}
B="$ROOT/boards/$id.json"
[ -f "$B" ] || { echo "no board $id ($(cd "$ROOT/boards" && ls *.json | sed 's/\.json$//' | tr '\n' ' '))"; exit 2; }
yaml="$HERE/facet-$id.yaml"
[ -f "$yaml" ] || { echo "no $yaml"; exit 2; }
scale=$(node -p "require('$B').scale"); speaker=$(node -p "require('$B').speaker === true")
node "$ROOT/firmware/bundle.mjs" --density "$scale" ${FACET_VERSION:+--version "$FACET_VERSION"} --out "$HERE/gen/$id"
# the Facet editor the panel serves at its address (workbench/scripts/editor-device.mjs, after bundle.mjs: that one
# deletes what it did not write): this tree's WASM build at the board's density ($FW_OUT, local.env, else renderer/out;
# renderer/build.sh <scale> first) + workbench/ (npm ci). FW_WEB=0 skips it (a build with ESPHome's own page at /).
if [ "${FW_WEB:-1}" != 0 ]; then
  [ -z "${FW_OUT:-}" ] && [ -f "$ROOT/local.env" ] && FW_OUT=$(sed -n 's/^FW_OUT=//p' "$ROOT/local.env")
  wasm="${FW_OUT:-$ROOT/renderer/out}/$scale"
  [ -f "$wasm/fw.wasm" ] || { echo "no $scale WASM build in $wasm (renderer/build.sh $scale first, or FW_WEB=0)"; exit 1; }
  node "$ROOT/workbench/scripts/editor-device.mjs" --device "$id" --den "$scale" --wasm "$wasm" --renderer "$ROOT/renderer" --out "$HERE/gen/$id/facet_wb" | tail -1
fi
node "$ROOT/firmware/settings_yaml.mjs" "$HERE/gen/settings.yaml"
if [ "$speaker" = true ]; then node "$ROOT/firmware/sound_bank.mjs" --check || node "$ROOT/firmware/sound_bank.mjs"; fi
[ "$step" = prepare ] && exit 0
${ESPHOME:-esphome} "$step" "$yaml"
# the editor files after the app image, in every image this build made (firmware/web_append.mjs; in rodata they were
# copied to PSRAM at boot on the Guition, #185). The images are where the YAML's build_path puts them.
if [ "$step" = compile ] && [ "${FW_WEB:-1}" != 0 ]; then
  suffix=$(sed -n 's/^ *build_suffix: *"\([^"]*\)".*/\1/p' "$yaml"); name=$(sed -n 's/^ *name: *\([a-z0-9-]*\).*/\1/p' "$yaml" | head -1)
  B="${ESPHOME_BUILD_ROOT:-$HERE/build}/$name$suffix/build"
  imgs=(); for f in "$B/firmware.ota.bin" "$B/firmware.factory.bin" "$B/firmware.bin" "$B/$name.bin"; do [ -f "$f" ] && imgs+=("$f"); done
  [ ${#imgs[@]} -gt 0 ] || { echo "no images in $B to append the editor files to"; exit 1; }
  node "$ROOT/firmware/web_append.mjs" "$HERE/gen/$id/facet_wb/fw_web.bin" "${imgs[@]}"
fi
