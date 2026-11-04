#!/bin/bash
# Host ASan + UBSan test of the panel picture decode (firmware/fw_imgdec.h + fw_tjpgd.c + fw_pngle.c / fw_miniz.c), with real
# files: Pillow-made fixtures (fixtures.py, under renderer/.venv) + a real 32 x 32 RGBA PNG (files/gem.png). Exit 0 = all checks passed, no reports.
#   firmware/tests/imgdec/run.sh
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../../.." && pwd)
[ -f "$ROOT/local.env" ] && . "$ROOT/local.env"
O=${FW_CACHE:-$ROOT/renderer/.cache}/imgdec; mkdir -p "$O/fx"
[ -f "$O/fx/cover.jpg" ] || "$("$ROOT/renderer/tools/venv.sh")" "$HERE/fixtures.py" "$O/fx"
cp "$HERE/files/gem.png" "$O/fx/"
SAN="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined"
FL="-O1 -g $SAN -I$ROOT/firmware -I$ROOT/renderer/core"
gcc $FL -c "$ROOT/firmware/fw_pngle.c" -o "$O/pngle.o"
gcc $FL -c "$ROOT/firmware/fw_miniz.c" -o "$O/miniz.o"
gcc $FL -fwrapv -c "$ROOT/firmware/fw_tjpgd.c" -o "$O/tjpgd.o"
g++ $FL -std=c++17 -c "$HERE/imgdec_test.cpp" -o "$O/test.o"
g++ $SAN "$O/test.o" "$O/pngle.o" "$O/miniz.o" "$O/tjpgd.o" -o "$O/imgdec-asan"
exec "$O/imgdec-asan" "$O/fx"
