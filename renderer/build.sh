#!/bin/bash
# Facet WASM workbench build (README.md) -> $FW_OUT/<density>/ (fw.mjs, fw.wasm, manifest.json), served by the
# workbench page.
#   ./build.sh            both densities in parallel (1.5x = Waveshare + Shelly 720, 1x = Guition 480)
#   ./build.sh 1x|1.5x    one
# Incremental: each .cpp (core/*.cpp, foundations/*.cpp, components/*.cpp) is its own object, recompiled only when it or a header in
# core/ or a generated token header (tokens/colour.json, tokens/type.json) changed; LVGL objects are cached once per lv_conf (cold ~20-60 s, after that a component edit = a few seconds).
# Paths (env, or local.env at the repo root, untracked):
#   PANEL_BUILD  ESPHome build dir of the panel firmware (LVGL tree, lv_conf.h, ArduinoJson)
#   FW_PANEL     panel firmware source dir (packages/ui.yaml for the LV_* build flags)
#   FW_FONT_PY   a Python with the esphome package (font rasteriser, tools/gen_fonts.py); FW_TTF = dir of the TTFs
#   FW_OUT       output dir (default ./out), FW_CACHE object cache (default ./.cache)
# TODO: vendor LVGL, lv_conf and the fonts into this branch so it builds on its own.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
[ -f "$HERE/../local.env" ] && . "$HERE/../local.env"
: "${FW_PANEL:?set FW_PANEL (panel firmware source dir) in the env or local.env}"
if [ $# -eq 0 ]; then
  "$0" 1.5x & a=$!; "$0" 1x & b=$!; wait $a; ra=$?; wait $b; rb=$?; exit $(( ra | rb ))
fi
DEN=$1
. ~/emsdk/emsdk_env.sh >/dev/null 2>&1
PB=${PANEL_BUILD:-$HOME/.esphome-build/waveshare-p4-86-panel}
L=$PB/managed_components/lvgl__lvgl
AJ=$PB/managed_components/bblanchon__arduinojson/src
OPT=${OPT:--O2}
C=${FW_CACHE:-$HERE/.cache}
O=$C/obj-$DEN$OPT
OUT=${FW_OUT:-$HERE/out}/$DEN
VFLAG=; [ "$DEN" = 1x ] && VFLAG=-DFW_1X
mkdir -p "$C/lvgl$OPT" "$O" "$OUT" "$C/gen"
[ -f "$PB/src/lv_conf.h" ] && [ -d "$L/src" ] || { echo "no panel build in $PB (build the panel firmware once)"; exit 1; }
t0=$(date +%s.%N)

# lv_conf = the panel's + host patches; rewritten only on change
{
  sed -e 's/^#define LV_USE_LOG 1/#define LV_USE_LOG 0/' -e '/^#define LV_LOG_LEVEL /d' \
      -e 's/^#define LV_USE_STDLIB_MALLOC LV_STDLIB_CUSTOM/#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB/' "$PB/src/lv_conf.h"
  grep -o -- '-DLV_[A-Z_]*=[0-9]*' "$FW_PANEL/packages/ui.yaml" | sed 's/-D\([A-Z_]*\)=\(.*\)/#undef \1\n#define \1 \2/'
  echo '#define LV_CONF_H'
  echo '#define LV_USE_OS LV_OS_NONE'
  echo '#define LV_ASSERT_HANDLER_INCLUDE <stdlib.h>'
  echo '#define LV_ASSERT_HANDLER abort();'
} > "$C/gen/lv_conf.h.$DEN"
cmp -s "$C/gen/lv_conf.h.$DEN" "$C/gen/lv_conf.h" || mv "$C/gen/lv_conf.h.$DEN" "$C/gen/lv_conf.h"; rm -f "$C/gen/lv_conf.h.$DEN"
CONF_SUM=$(md5sum "$C/gen/lv_conf.h" | cut -c1-8)
FL="$OPT -DLV_CONF_INCLUDE_SIMPLE -I$C/gen -I$L -I$L/src"

# LVGL: one cache for both densities (density-free)
LVGL_OBJS=$(find "$L/src" -name '*.c' | while read -r f; do echo "$C/lvgl$OPT/$(echo "$f $CONF_SUM" | md5sum | cut -c1-12).o"; done)
find "$L/src" -name '*.c' | xargs -P"$(nproc)" -I{} sh -c \
  'o='"$C/lvgl$OPT"'/$(echo "{} '"$CONF_SUM"'" | md5sum | cut -c1-12).o; [ -f $o ] || { emcc '"$FL"' -c {} -o $o.$$ && mv $o.$$ $o; }'

# colour tokens: tokens/colour.json -> colour_gen.h (written only when it changes, so an unchanged JSON rebuilds nothing)
node "$HERE/tools/gen_colour.mjs" "$C/gen/colour_gen.h"
# type tokens: tokens/type.json -> type_gen.h (styles) + this density's bitmap fonts (only the sizes/glyphs the styles use)
node "$HERE/tools/gen_type.mjs" "$C/gen/type_gen.h"
FONTS_H=$C/gen/facet_fonts.h; [ "$DEN" = 1x ] && FONTS_H=$C/gen/facet_fonts_1x.h
FW_TTF=${FW_TTF:?set FW_TTF (dir of the TTFs) in local.env} "${FW_FONT_PY:?set FW_FONT_PY in local.env}" "$HERE/tools/gen_fonts.py" "$DEN" "$FONTS_H"

# workbench + foundations + components: recompile when the source, a core header, a token header or (fonts) the font header is newer
CXX="em++ $FL $VFLAG -std=c++17 -I$HERE/core -I$AJ"
OBJS=; JOBS=
for f in "$HERE"/core/*.cpp "$HERE"/foundations/*.cpp "$HERE"/components/*.cpp; do
  [ -f "$f" ] || continue
  o="$O/$(basename "$(dirname "$f")")_$(basename "$f" .cpp).o"
  OBJS="$OBJS $o"
  dep="$f $HERE/core/*.h $C/gen/colour_gen.h $C/gen/type_gen.h"; [ "$(basename "$f")" = fonts.cpp ] && dep="$dep $FONTS_H"
  stale=0; [ -f "$o" ] || stale=1
  [ $stale = 0 ] && for d in $dep; do [ "$d" -nt "$o" ] && stale=1; done
  if [ $stale = 1 ]; then echo "[$DEN] cc $(basename "$f")"; $CXX -c "$f" -o "$o" & JOBS="$JOBS $!"; fi
done
for j in $JOBS; do wait "$j"; done
# objects of deleted components must not linger in the link: OBJS lists only live sources
em++ $OPT $LVGL_OBJS $OBJS -sALLOW_MEMORY_GROWTH=1 -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web,node \
  -sEXPORTED_FUNCTIONS='["_fw_init","_fw_meta","_fw_stage","_fw_stage_free","_fw_render","_fw_tokens","_fw_content_h","_fw_stage_resize","_fw_touch","_fw_tick","_fw_dirty","_fw_frame","_malloc","_free"]' \
  -sEXPORTED_RUNTIME_METHODS='["HEAPU8","UTF8ToString","stringToNewUTF8"]' -o "$OUT/fw.mjs"
S=$(awk "BEGIN{printf \"%.1f\", $(date +%s.%N) - $t0}")
printf '{"built": "%s", "density": "%s", "opt": "%s", "build_s": %s, "wasm_bytes": %s}\n' "$(date -Is)" "$DEN" "$OPT" "$S" "$(stat -c %s "$OUT/fw.wasm")" > "$OUT/manifest.json"
echo "[$DEN] built in ${S}s"
