#!/bin/bash
# renderer/.venv: the Python tools/gen_fonts.py runs under (tools/requirements.txt). build.sh and firmware/bundle.mjs
# make it on first use; again after requirements.txt changes. Prints the venv's python.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd)
V=$HERE/.venv R=$HERE/tools/requirements.txt
exec 9>"$HERE/.venv.lock"; flock 9  # both densities build at once: one makes it, the other waits
if ! cmp -s "$R" "$V/requirements.txt" 2>/dev/null; then
  python3 -m venv "$V" >&2
  "$V/bin/pip" install -q --disable-pip-version-check -r "$R" >&2
  cp "$R" "$V/requirements.txt"
fi
echo "$V/bin/python"
