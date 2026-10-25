#!/bin/bash
# Publish the docs flavour of the WASM build for the Pages workflow (.github/workflows/deploy.yml), then redeploy.
#   site/scripts/upload-build.sh            (after FW_FEATURES=docs renderer/build.sh; needs gh, signed in)
# FW_OUT as for renderer/build.sh (env, else local.env, else renderer/out).
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
[ -z "${FW_OUT:-}" ] && [ -f "$ROOT/local.env" ] && FW_OUT=$(sed -n 's/^FW_OUT=//p' "$ROOT/local.env")
D=${FW_OUT:-$ROOT/renderer/out}/docs
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
tar -czf "$T/docs-build.tar.gz" -C "$D" 1.5x/fw.mjs 1.5x/fw.wasm 1.5x/manifest.json 1.5x/features.json 1x/fw.mjs 1x/fw.wasm 1x/manifest.json 1x/features.json
cd "$ROOT"
gh release view docs-build >/dev/null 2>&1 || gh release create docs-build --prerelease --title "Docs build" \
  --notes "The WASM renderer's docs build, used by the Pages workflow. Replaced on every docs publish; not a release."
gh release upload docs-build "$T/docs-build.tar.gz" --clobber
gh workflow run deploy.yml --ref main
