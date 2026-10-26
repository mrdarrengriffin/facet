#!/bin/bash
# Host ASan + UBSan test of the panel picture decode (firmware/fw_imgdec.h + fw_tjpgd.c + fw_pngle.c / fw_miniz.c), with real
# files: Pillow-made fixtures (fixtures.py) + a real 32 x 32 RGBA PNG from the web. Needs a Python with Pillow (FW_FONT_PY in local.env). Exit 0 = all checks passed, no reports.
#   firmware/tests/imgdec/run.sh
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../../.." && pwd)
[ -f "$ROOT/local.env" ] && . "$ROOT/local.env"
O=${FW_CACHE:-$ROOT/renderer/.cache}/imgdec; mkdir -p "$O/fx"
[ -f "$O/fx/photo.jpg" ] || "${FW_FONT_PY:-python3}" "$HERE/fixtures.py" "$O/fx"
[ -f "$O/fx/rockbottom.png" ] || curl -fsSL -o "$O/fx/rockbottom.png" \
  https://media.githubusercontent.com/media/GelatinousGoth/TboiResurrected/master/resources/gfx/items/collectibles/collectibles_562_rockbottom.png
SAN="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined"
FL="-O1 -g $SAN -I$ROOT/firmware"
gcc $FL -c "$ROOT/firmware/fw_pngle.c" -o "$O/pngle.o"
gcc $FL -c "$ROOT/firmware/fw_miniz.c" -o "$O/miniz.o"
gcc $FL -fwrapv -c "$ROOT/firmware/fw_tjpgd.c" -o "$O/tjpgd.o"
g++ $FL -std=c++17 -c "$HERE/imgdec_test.cpp" -o "$O/test.o"
g++ $SAN "$O/test.o" "$O/pngle.o" "$O/miniz.o" "$O/tjpgd.o" -o "$O/imgdec-asan"
exec "$O/imgdec-asan" "$O/fx"
