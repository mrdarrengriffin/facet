#!/bin/bash
# The workbench in one command: installs what is missing, builds the renderer if there is no build, serves it.
#   workbench/run.sh            http://<this machine>:4322/wasm   (PORT=... to change)
# Optional: HA_URL + HA_TOKEN (a real Home Assistant; else the fake one), FACET_PANELS (panels to show things on:
# lib/panels.ts), FW_OUT / FW_CACHE (as renderer/build.sh).
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(dirname "$HERE")
[ -d "$ROOT/renderer/tools/node_modules" ] || (cd "$ROOT/renderer/tools" && npm ci)
[ -d "$HERE/node_modules" ] || (cd "$HERE" && npm ci --no-audit --no-fund)
[ -f "$ROOT/local.env" ] && . "$ROOT/local.env"
OUT=${FW_OUT:-$ROOT/renderer/out}
for d in 1.5x 1x; do [ -f "$OUT/$d/fw.mjs" ] || "$ROOT/renderer/build.sh" "$d"; done
cd "$HERE" && exec npx astro dev
