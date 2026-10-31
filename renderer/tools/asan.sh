#!/bin/bash
# Native AddressSanitizer + UBSan run of the renderer (README "ASan check"): the same core, foundations, components and
# screens as the WASM build, compiled with g++ for this machine, driven headless by tools/asan_driver.cpp. Finds what
# WASM hides (an out-of-bounds write there silently corrupts the heap; on the panels it crashes: the 2026-10-22 clock
# hand). Heavy (~2 GB, minutes cold): run it through the box's job queue where there is one.
#   tools/asan.sh [1x|1.5x] [id '<props json>' ...]   no ids = every item, its defaults and every variant
# Exit 0 = no reports. Objects cached in $FW_CACHE/asan-<den> (LVGL once; a source rebuilt when it or a header it includes changed).
set -euo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd)
[ -f "$HERE/../local.env" ] && . "$HERE/../local.env"
DEN=1x; if [ "${1:-}" = 1x ] || [ "${1:-}" = 1.5x ]; then DEN=$1; shift; fi
PB=${PANEL_BUILD:-$HOME/.esphome-build/waveshare-p4-86-panel}
L=$PB/managed_components/lvgl__lvgl
AJ=$PB/managed_components/bblanchon__arduinojson/src
C=${FW_CACHE:-$HERE/.cache}
G=$C/gen  # generated token headers + lv_conf from the last WASM build (build.sh)
[ -f "$G/lv_conf.h" ] || { echo "run build.sh once first (generated headers in $G)"; exit 1; }
O=$C/asan-$DEN
# LVGL's objects per lv_conf (its md5, as build.sh): a config change (2026-10-28: TinyTTF on for theme fonts) rebuilds
# them; a stale cache had TinyTTF compiled out and the link failed
LV=$O/lv-$(md5sum "$G/lv_conf.h" | cut -c1-8); mkdir -p "$LV" "$O/obj" "$O/shim"
cat > "$O/shim/emscripten.h" <<'H'
#pragma once
#include <ctime>
#define EMSCRIPTEN_KEEPALIVE
inline double emscripten_get_now() { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }
H
SAN="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined"
FL="-O1 -g $SAN -DLV_CONF_INCLUDE_SIMPLE -I$G -I$L -I$L/src"
V=; [ "$DEN" = 1x ] && V=-DFW_1X
find "$L/src" -name '*.c' | xargs -P2 -I{} sh -c 'o='"$LV"'/$(echo "{}" | md5sum | cut -c1-12).o; [ -f $o ] || gcc '"$FL"' -c {} -o $o' 
. "$HERE/tools/lib/stale.sh"; STALE_SKIP=$L  # sources rebuilt when they or a header they include changed (depfiles)
OBJS=
for f in "$HERE"/core/*.cpp "$HERE"/foundations/*.cpp "$HERE"/components/*.cpp "$HERE"/screens/*.cpp "$HERE"/tools/asan_driver.cpp; do
  [ -f "$f" ] || continue
  o="$O/obj/$(basename "$(dirname "$f")")_$(basename "$f" .cpp).o"; OBJS="$OBJS $o"
  stale "$o" && { echo "cc $(basename "$f")"; g++ $FL $V -std=c++17 -I"$O/shim" -I"$HERE/core" -I"$AJ" -MMD -MF "$o.d" -c "$f" -o "$o"; }
done
g++ $SAN "$LV"/*.o $OBJS -o "$O/fw-asan" -lm
exec "$O/fw-asan" "$@"
