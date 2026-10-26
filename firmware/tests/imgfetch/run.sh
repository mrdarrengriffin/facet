#!/bin/bash
# Host stress test of the panel's image fetch (firmware/fw_images.h: worker task, queue, pump, hand-off) on simulated
# ESP-IDF (shim/), twice: ASan + UBSan, then TSan. Fixtures from the imgdec test (run that once first, or this does).
#   firmware/tests/imgfetch/run.sh        Exit 0 = all checks passed in both builds, no sanitizer reports.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../../.." && pwd)
[ -f "$ROOT/local.env" ] && . "$ROOT/local.env"
FX=${FW_CACHE:-$ROOT/renderer/.cache}/imgdec/fx
[ -f "$FX/rockbottom.png" ] && [ -f "$FX/photo.jpg" ] || "$ROOT/firmware/tests/imgdec/run.sh" >/dev/null
O=${FW_CACHE:-$ROOT/renderer/.cache}/imgfetch; mkdir -p "$O"
INC="-I$HERE/shim -I$ROOT/firmware"
for san in asan tsan; do
  if [ $san = asan ]; then S="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined"; else S="-fsanitize=thread"; fi
  FL="-O1 -g $S $INC"
  gcc $FL -c "$ROOT/firmware/fw_pngle.c" -o "$O/pngle-$san.o"
  gcc $FL -c "$ROOT/firmware/fw_miniz.c" -o "$O/miniz-$san.o"
  gcc $FL -fwrapv -c "$ROOT/firmware/fw_tjpgd.c" -o "$O/tjpgd-$san.o"
  g++ $FL -std=c++17 -pthread "$HERE/fetch_test.cpp" "$O/pngle-$san.o" "$O/miniz-$san.o" "$O/tjpgd-$san.o" -o "$O/fetch-$san"
  echo "== $san"
  TSAN_OPTIONS="halt_on_error=1 report_signal_unsafe=0" "$O/fetch-$san" "$FX"
done
