#!/bin/bash
# The public firmware for a release, built from its TAG (never the working tree): every board's factory + OTA image
# and its two manifests, ready for site/scripts/upload-firmware.sh (plan Phase 6.1).
#   esphome/release.sh <version> [out dir]      e.g. esphome/release.sh 1.0.0   (out default site/public/firmware)
# Checks, in order (each stops the build): the tag v<version> exists; every esphome/facet-<board>.yaml at the tag says
# facet_version: <version> (its dashboard_import reads @v<version>); then per board, in a worktree of the tag:
# FACET_VERSION=<version> esphome/build.sh <board> compile into the board's own build folder (build_suffix), the images
# copied out at once as facet-<board>-<version>.{factory,ota}.bin (the tag's WASM built first, both densities: the
# bundle checks the baked deck with it), the OTA image's version string = <version> and no
# "+wip"; then firmware/manifests.mjs writes manifest.json + ota.json. The worktree is removed whatever happens.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$HERE")
V=${1:?usage: esphome/release.sh <version> [out dir]}
OUT=${2:-$ROOT/site/public/firmware}
T=v$V
git -C "$ROOT" rev-parse -q --verify "refs/tags/$T" >/dev/null || { echo "no tag $T (tag the release commit first)"; exit 1; }
BOARDS=$(cd "$ROOT/boards" && ls *.json | sed 's/\.json$//')
for b in $BOARDS; do
  y=$(git -C "$ROOT" show "$T:esphome/facet-$b.yaml" | sed -n 's/^ *facet_version: *\([^ #]*\).*/\1/p')
  [ "$y" = "$V" ] || { echo "esphome/facet-$b.yaml at $T says facet_version $y, not $V"; exit 1; }
done
echo "tag $T ($(git -C "$ROOT" rev-parse --short "$T^{}")); both YAMLs say $V"
mkdir -p "$OUT"
W=$(mktemp -d)/facet-$T
git -C "$ROOT" worktree add -q --detach "$W" "$T"
trap 'git -C "$ROOT" worktree remove --force "$W" 2>/dev/null || true' EXIT
# the worktree's tools: this tree's installs (node_modules, the font venv) linked in, never installed again
for d in renderer/tools/node_modules renderer/.venv; do [ -e "$ROOT/$d" ] && ln -s "$ROOT/$d" "$W/$d"; done
cp "$ROOT"/esphome/components/facet_audio/facet_sounds* "$W/esphome/components/facet_audio/" 2>/dev/null || true  # the sound bank (generated; build.sh checks it is current)
# the tag's own WASM build (the firmware bundle checks the baked deck with it): its own out + cache, never this tree's
export FW_OUT="$W/renderer/out" FW_CACHE="${FACET_RELEASE_CACHE:-$HOME/.cache/facet-release}"
echo "building the tag's WASM (both densities) ..."
(cd "$W/renderer" && ./build.sh) > "$OUT/build-wasm.log" 2>&1 || { echo "WASM build failed (log $OUT/build-wasm.log)"; exit 1; }
export ESPHOME_BUILD_ROOT=${ESPHOME_BUILD_ROOT:-$HOME/.esphome-build}
for b in $BOARDS; do
  suffix=$(sed -n 's/^ *build_suffix: *"\([^"]*\)".*/\1/p' "$W/esphome/facet-$b.yaml")
  name=$(sed -n 's/^ *name: *\([a-z0-9-]*\).*/\1/p' "$W/esphome/facet-$b.yaml" | head -1)
  B="$ESPHOME_BUILD_ROOT/$name$suffix/build"
  rm -f "$B/firmware.factory.bin" "$B/firmware.ota.bin"
  echo "building $b ..."
  (cd "$W" && FACET_VERSION=$V esphome/build.sh "$b" compile) > "$OUT/build-$b.log" 2>&1 || { echo "$b: build failed (log $OUT/build-$b.log)"; exit 1; }
  mkdir -p "$OUT/$b"
  cp "$B/firmware.factory.bin" "$OUT/$b/facet-$b-$V.factory.bin"
  cp "$B/firmware.ota.bin" "$OUT/$b/facet-$b-$V.ota.bin"
  txt=$(strings "$OUT/$b/facet-$b-$V.ota.bin")  # whole, then grep (grep -q in a pipe: SIGPIPE fails it under pipefail)
  grep -qx -- "$V" <<<"$txt" || { echo "$b: the image does not say $V"; exit 1; }
  ! grep -q -- '+wip' <<<"$txt" || { echo "$b: the image says +wip"; exit 1; }
  echo "$b: $V, $(du -h "$OUT/$b/facet-$b-$V.ota.bin" | cut -f1) OTA"
done
node "$ROOT/firmware/manifests.mjs" "$OUT" "$V" "$(git -C "$ROOT" rev-parse --short "$T^{}")"
