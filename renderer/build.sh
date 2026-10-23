#!/bin/bash
# Facet WASM workbench build (README.md) -> $FW_OUT/<density>/ (fw.mjs, fw.wasm, manifest.json), served by the
# workbench page.
#   ./build.sh            both densities in parallel (1.5x = Waveshare + Shelly 720, 1x = Guition 480)
#   ./build.sh 1x|1.5x    one
# Incremental: each .cpp (core/*.cpp, foundations/*.cpp, components/*.cpp) is its own object, recompiled only when it or a header in
# core/ or a generated token header (tokens/colour.json, tokens/type.json, tokens/space.json, tokens/layout.json, tokens/icons.json, tokens/motion.json, tokens/states.json, tokens/mascot.json) changed; LVGL objects are cached once per lv_conf (cold ~20-60 s, after that a component edit = a few seconds).
# Paths (env, or local.env at the repo root, untracked):
#   PANEL_BUILD  ESPHome build dir of the panel firmware (LVGL tree, lv_conf.h, ArduinoJson)
#   FW_PANEL     panel firmware source dir (packages/ui.yaml for the LV_* build flags)
#   FW_FONT_PY   a Python with the esphome package (font rasteriser, tools/gen_fonts.py); FW_TTF = dir of the TTFs
#   FW_OUT       output dir (default ./out), FW_CACHE object cache (default ./.cache)
# TODO: vendor LVGL, lv_conf and the fonts into this branch so it builds on its own.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
[ -f "$HERE/../local.env" ] && . "$HERE/../local.env"
# BUILD_JOBS = compiles at once (LVGL and the renderer's own files): default 2 in the quiet hours (00-06: the box is
# shared and heavy caps a job at 3 GB; a cold build of every file at once was OOM-killed) else half the cores
if [ -z "${BUILD_JOBS:-}" ]; then h=$(date +%-H); if [ "$h" -lt 6 ]; then BUILD_JOBS=2; else BUILD_JOBS=$(( $(nproc) / 2 )); fi; fi
[ "$BUILD_JOBS" -ge 1 ] 2>/dev/null || BUILD_JOBS=1
export BUILD_JOBS
: "${FW_PANEL:?set FW_PANEL (panel firmware source dir) in the env or local.env}"
if [ $# -eq 0 ]; then
  if [ "$BUILD_JOBS" -lt 4 ]; then ra=0; "$0" 1.5x || ra=$?; rb=0; "$0" 1x || rb=$?; exit $(( ra | rb )); fi  # one density at a time
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
find "$L/src" -name '*.c' | xargs -P"$BUILD_JOBS" -I{} sh -c \
  'o='"$C/lvgl$OPT"'/$(echo "{} '"$CONF_SUM"'" | md5sum | cut -c1-12).o; [ -f $o ] || { emcc '"$FL"' -c {} -o $o.$$ && mv $o.$$ $o; }'

# colour tokens: tokens/colour.json -> colour_gen.h (written only when it changes, so an unchanged JSON rebuilds nothing)
node "$HERE/tools/gen_colour.mjs" "$C/gen/colour_gen.h"
# contrast test (tools/contrast.mjs): every declared pair x theme x accent set; reported, never fails the build
[ "$DEN" = 1x ] || node "$HERE/tools/contrast.mjs" --summary || true
# type tokens: tokens/type.json -> type_gen.h (styles) + this density's bitmap fonts (only the sizes/glyphs the styles use)
node "$HERE/tools/gen_type.mjs" "$C/gen/type_gen.h"
# space tokens: tokens/space.json -> space_gen.h (fw::sp::, fw::lay::, fw::rad::, fw::stroke::, fw::touch::)
node "$HERE/tools/gen_space.mjs" "$C/gen/space_gen.h"
# layout tokens: tokens/layout.json -> layout_gen.h (fw::lay:: screen geometry, fw::lyt:: devices / size classes / density)
node "$HERE/tools/gen_layout.mjs" "$C/gen/layout_gen.h"
# motion tokens: tokens/motion.json -> motion_gen.h (fw::mo:: durations, easings, patterns)
node "$HERE/tools/gen_motion.mjs" "$C/gen/motion_gen.h"
# state tokens: tokens/states.json -> states_gen.h (fw::st:: state bits, colour roles, press / switch motion, events + sounds)
node "$HERE/tools/gen_states.mjs" "$C/gen/states_gen.h"
# mascot tokens: tokens/mascot.json -> mascot_gen.h (fw::mas:: geometry per density, moods, motion keyframe tracks)
node "$HERE/tools/gen_mascot.mjs" "$C/gen/mascot_gen.h"
node "$HERE/tools/gen_chart.mjs" "$C/gen/chart_gen.h"  # tokens/chart.json (+ colour, motion) -> fw::ch:: chart colours, geometry, chart motion
# settings schema: tokens/settings.json -> settings_gen.h (fw::set:: the personalisation settings, screens/settings.cpp)
node "$HERE/tools/gen_settings.mjs" "$C/gen/settings_gen.h"
node "$HERE/tools/gen_specs.mjs" "$C/gen/specs_gen.h"  # factory screen specs (core/spec.h)
# icons: tokens/icons.json + icons/*.svg -> icons_gen.h (table, density-free) + this density's A8 masks (tools/gen_icons.mjs,
# resvg: `cd tools && npm install` once; FW_ICONS=used in local.env = only the icons components declare)
node "$HERE/tools/gen_icons.mjs" --meta "$C/gen/icons_gen.h"
ICONS_H=$C/gen/facet_icons.h; [ "$DEN" = 1x ] && ICONS_H=$C/gen/facet_icons_1x.h
FW_ICONS=${FW_ICONS:-all} node "$HERE/tools/gen_icons.mjs" --data "$DEN" "$ICONS_H"
FONTS_H=$C/gen/facet_fonts.h; [ "$DEN" = 1x ] && FONTS_H=$C/gen/facet_fonts_1x.h
FW_TTF=${FW_TTF:?set FW_TTF (dir of the TTFs) in local.env} "${FW_FONT_PY:?set FW_FONT_PY in local.env}" "$HERE/tools/gen_fonts.py" "$DEN" "$FONTS_H"

# workbench + foundations + components: recompile when the source, a core header, a token header or (fonts) the font header is newer
CXX="em++ $FL $VFLAG -std=c++17 -I$HERE/core -I$AJ"
OBJS=; JOBS=
for f in "$HERE"/core/*.cpp "$HERE"/foundations/*.cpp "$HERE"/components/*.cpp "$HERE"/screens/*.cpp "$HERE"/bench/*.cpp; do
  [ -f "$f" ] || continue
  o="$O/$(basename "$(dirname "$f")")_$(basename "$f" .cpp).o"
  OBJS="$OBJS $o"
  dep="$f $HERE/core/*.h $C/gen/colour_gen.h $C/gen/type_gen.h $C/gen/space_gen.h $C/gen/layout_gen.h $C/gen/icons_gen.h $C/gen/motion_gen.h $C/gen/states_gen.h $C/gen/mascot_gen.h $C/gen/settings_gen.h $C/gen/chart_gen.h $C/gen/specs_gen.h"; [ "$(basename "$f")" = fonts.cpp ] && dep="$dep $FONTS_H"; [ "$(basename "$f")" = icons.cpp ] && dep="$dep $ICONS_H"
  stale=0; [ -f "$o" ] || stale=1
  [ $stale = 0 ] && for d in $dep; do [ "$d" -nt "$o" ] && stale=1; done
  if [ $stale = 1 ]; then
    while [ "$(jobs -rp | wc -l)" -ge "$BUILD_JOBS" ]; do wait -n || CC_FAIL=1; done  # at most BUILD_JOBS at once
    echo "[$DEN] cc $(basename "$f")"; $CXX -c "$f" -o "$o" & JOBS="$JOBS $!"
  fi
done
while [ "$(jobs -rp | wc -l)" -gt 0 ]; do wait -n || CC_FAIL=1; done
[ "${CC_FAIL:-0}" = 0 ] || exit 1
# objects of deleted components must not linger in the link: OBJS lists only live sources
em++ $OPT $LVGL_OBJS $OBJS -sALLOW_MEMORY_GROWTH=1 -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web,node \
  -sEXPORTED_FUNCTIONS='["_fw_init","_fw_meta","_fw_stage","_fw_stage_free","_fw_render","_fw_update","_fw_demo_state","_fw_tokens","_fw_content_h","_fw_stage_resize","_fw_touch","_fw_tick","_fw_dirty","_fw_frame","_fw_areas","_fw_events","_fw_ha_wants","_fw_ha_put","_fw_spec","_fw_catalogue","_fw_spec_storage","_fw_anims","_fw_rects","_malloc","_free"]' \
  -sEXPORTED_RUNTIME_METHODS='["HEAPU8","UTF8ToString","stringToNewUTF8"]' -o "$OUT/fw.mjs"
S=$(awk "BEGIN{printf \"%.1f\", $(date +%s.%N) - $t0}")
printf '{"built": "%s", "density": "%s", "opt": "%s", "build_s": %s, "wasm_bytes": %s}\n' "$(date -Is)" "$DEN" "$OPT" "$S" "$(stat -c %s "$OUT/fw.wasm")" > "$OUT/manifest.json"
echo "[$DEN] built in ${S}s"
# component audit (tools/audit.mjs, README "Audit"; Darren 2026-10-22): clipping, stale pixels, grid, text cut off, touch
# size, ring symmetry, motion paths, for every component / screen x variant x theme at this density. The new build is
# already in place (the page still reloads); errors fail the build so they are seen. Cached per component.
node "$HERE/tools/audit.mjs" --den "$DEN" --quiet --build "${FW_OUT:-$HERE/out}" | sed "s/^/[$DEN] /" ; [ "${PIPESTATUS[0]}" = 0 ] || { echo "[$DEN] audit: errors above"; exit 1; }
# the spec catalogue + JSON Schema (core/spec.h; catalogue/) and the spec store test (power loss, refusals)
if [ "$DEN" = 1.5x ]; then
  node "$HERE/tools/gen_catalogue.mjs" --build "${FW_OUT:-$HERE/out}" | sed "s/^/[$DEN] /"
  node "$HERE/tools/spec_test.mjs" --build "${FW_OUT:-$HERE/out}" | tail -1 | sed "s/^/[$DEN] /"
fi
