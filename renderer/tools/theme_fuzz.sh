#!/bin/bash
# The theme parser under AddressSanitizer + UBSan (core/theme_file.cpp; theme engine design S4): builds the native
# driver tools/theme_fuzz.cpp against the generated schema of the last WASM build (build.sh -> $FW_CACHE/gen/theme_gen.h)
# and runs N seeded mutations of every themes/*.json (default 100000), then loads each built-in theme once.
#   tools/theme_fuzz.sh [N] [seed]        exit 0 = no failure, no sanitizer report
# Light (one file + the driver, seconds to build); run it through `heavy` like every native build.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd)
[ -f "$HERE/../local.env" ] && . "$HERE/../local.env"
N=${1:-100000}; SEED=${2:-1}
C=${FW_CACHE:-$HERE/.cache}
G=$C/gen
[ -f "$G/theme_gen.h" ] || { echo "run build.sh once first (generated headers in $G)"; exit 1; }
O=$C/theme-fuzz; mkdir -p "$O"
SAN="-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer -fno-sanitize-recover=undefined"
B=$O/theme_fuzz
if [ ! -x "$B" ] || [ "$HERE/core/theme_file.cpp" -nt "$B" ] || [ "$HERE/core/theme_file.h" -nt "$B" ] || [ "$HERE/tools/theme_fuzz.cpp" -nt "$B" ] || [ "$G/theme_gen.h" -nt "$B" ]; then
  g++ -std=c++17 -O1 -g $SAN -Wall -Wextra -I"$G" -I"$HERE/core" "$HERE/core/theme_file.cpp" "$HERE/tools/theme_fuzz.cpp" -o "$B.$$" && mv "$B.$$" "$B"
fi
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
"$B" --load "$HERE"/themes/*.json | sed 's/^/theme_fuzz: /'
"$B" --fuzz "$N" --seed "$SEED" "$HERE"/themes/*.json
