#!/bin/bash
# Facet checks against a build that already exists (build.sh builds; this never builds or withholds the module).
#   ./check.sh fast [--ids a,b]   before every commit: the quick rules over everything (cached audit, lint, contrast,
#                                 specs, themes) + the given ids' pixel contrast, screen audit, behaviour and functional
#                                 tests at 1.5x; also regenerates catalogue/ from the build
#   ./check.sh gate [--ids a,b]   nightly and in CI: everything at both densities, lab test, floor plans, visual
#   ./check.sh nightly            overnight only: ASan at 1.5x (and 1x on Sundays, or with --weekly), the functional
#                                 suites marked tier: 'nightly' (theme_apply)
# One line per step (PASS / FAIL / WARN, seconds); each step's full output in $CHECK_LOG (default
# $FW_CACHE/checks/<time>-<tier>). Functional tests are reported, never fail a tier (WARN) while the suite grows.
# Exit 1 if any step failed. Build = $FW_OUT (env or local.env), else ./out.
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
[ -f "$HERE/../local.env" ] && . "$HERE/../local.env"
tier=${1:-}; shift || true
ids="" weekly=""; [ "$(date +%u)" = 7 ] && weekly=1
while [ $# -gt 0 ]; do case "$1" in --ids) ids=$2; shift ;; --weekly) weekly=1 ;; *) echo "unknown arg $1"; exit 2 ;; esac; shift; done
case "$tier" in fast|gate|nightly) ;; *) sed -n 2,9p "$0"; exit 2 ;; esac
B=${FW_OUT:-$HERE/out}
LOG=${CHECK_LOG:-${FW_CACHE:-$HERE/.cache}/checks/$(date +%Y%m%d-%H%M%S)-$tier}; mkdir -p "$LOG"
fail=0
run() {  # run <hard|soft> <name> <cmd...>
  local kind=$1 name=$2; shift 2; local t0=$SECONDS
  ( cd "$HERE" && "$@" ) >"$LOG/$name.log" 2>&1; local rc=$?
  if [ $rc = 0 ]; then echo "PASS $name ($((SECONDS - t0))s)"; return; fi
  if [ "$kind" = soft ]; then echo "WARN $name ($((SECONDS - t0))s, non-blocking) log: $LOG/$name.log"
  else fail=1; echo "FAIL $name ($((SECONDS - t0))s) log: $LOG/$name.log"; fi
  grep -E '^FAIL|✗|error' "$LOG/$name.log" | grep -viE '0 errors|0 fail' | head -8 | cut -c1-200 | sed 's/^/  /'
}
check() { run hard "$@"; }
soft() { run soft "$@"; }
dens=1.5x; [ "$tier" = fast ] || dens="1.5x 1x"
for d in $dens; do [ -f "$B/$d/fw.mjs" ] || { echo "no $d build in $B: run build.sh $d first"; exit 2; }; done

if [ "$tier" = nightly ]; then
  check asan-1.5x tools/asan.sh 1.5x
  soft functional-nightly node tools/functional.mjs --tier nightly
  [ -n "$weekly" ] && check asan-1x tools/asan.sh 1x
  [ $fail = 0 ] && echo "CHECK $tier: all green (logs $LOG)" || echo "CHECK $tier: FAILED (logs $LOG)"; exit $fail
fi

# every tier: the rules over everything that are quick (density-free or cached)
check features node tools/features.mjs check
check contrast node tools/contrast.mjs --summary
check contract-lint node tools/contract_lint.mjs --summary
check catalogue node tools/gen_catalogue.mjs --build "$B"
check spec-store node tools/functional.mjs --rules --rule spec-store
check theme-check node tools/functional.mjs --rules --rule theme-check
for d in $dens; do check "audit-$d" node tools/functional.mjs --rules --rule about,audit --den "$d"; done

if [ "$tier" = fast ]; then
  [ -n "$ids" ] || { [ $fail = 0 ] && echo "CHECK fast: all green, no --ids (logs $LOG)" || echo "CHECK fast: FAILED (logs $LOG)"; exit $fail; }
  check pixel-contrast node tools/functional.mjs --rules --tier fast --rule pixel-contrast --den 1.5x --theme all --ids "$ids"
  check audit-screens node tools/audit_screens.mjs --den 1.5x --screen "$ids"
  check behaviour node tools/behaviour.mjs --den 1.5x --kind interaction,scroll,sound --ids "$ids"
  soft functional node tools/functional.mjs --den 1.5x --ids "$ids"
else
  sel=(); [ -n "$ids" ] && sel=(--ids "$ids")
  for d in $dens; do
    check "pixel-contrast-$d" node tools/functional.mjs --rules --rule pixel-contrast --den "$d" --theme all "${sel[@]}"
    check "lab-$d" node tools/functional.mjs --rules --rule lab-tokens,noop-update --den "$d"
  done
  check floorplan node tools/functional.mjs --rules --rule floorplan
  scr=(); [ -n "$ids" ] && scr=(--screen "$ids")
  check audit-screens node tools/audit_screens.mjs "${scr[@]}"
  check behaviour node tools/behaviour.mjs "${sel[@]}"
  soft functional node tools/functional.mjs "${sel[@]}"
  comp=(); [ -n "$ids" ] && comp=(--comp "$ids")
  check visual node tools/visual.mjs "${comp[@]}"
fi
[ $fail = 0 ] && echo "CHECK $tier: all green (logs $LOG)" || echo "CHECK $tier: FAILED (logs $LOG)"
exit $fail
