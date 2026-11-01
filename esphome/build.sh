#!/bin/bash
# Facet panel firmware (README.md): generate what the YAML needs, then compile with ESPHome.
#   esphome/build.sh <board-id> [prepare|config|compile]   default compile; board ids = boards/*.json
# prepare = gen/<board>/facet_wb (firmware/bundle.mjs, the release feature set at the board's scale),
# gen/settings.yaml (firmware/settings_yaml.mjs) and, on a board with a speaker, the sound bank in
# components/facet_audio (firmware/sound_bank.mjs, only when stale). All of it is generated, none of it committed.
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
node "$ROOT/firmware/settings_yaml.mjs" "$HERE/gen/settings.yaml"
if [ "$speaker" = true ]; then node "$ROOT/firmware/sound_bank.mjs" --check || node "$ROOT/firmware/sound_bank.mjs"; fi
[ "$step" = prepare ] && exit 0
${ESPHOME:-esphome} "$step" "$yaml"
