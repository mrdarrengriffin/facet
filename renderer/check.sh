#!/bin/bash
# Facet checks against a build that already exists (build.sh builds; this never builds or withholds the module).
#   ./check.sh fast [--ids a,b]   before every commit, 1.5x: lint, contrast, features, catalogue (regenerated from the
#                                 build) + the fast rules (tests/rules) over every component, each replayed from the
#                                 runner's input-hash cache unless the component changed (--ids: only those; behaviour
#                                 included) + the given ids' functional tests. No PNGs, no goldens
#   ./check.sh gate [--ids a,b] [--den 1.5x|1x]   nightly and in CI: every rule at both densities (lab, floor plans,
#                                 behaviour included; visual at 1.5x), every functional test, and locally the workbench Playwright tests. --den: one density's half
#                                 (CI runs the two halves as two jobs; the density-free checks run with 1.5x)
#   ./check.sh nightly            overnight only: ASan at 1.5x (and 1x on Sundays, or with --weekly), the functional
#                                 suites marked tier: 'nightly' (theme_apply), the theme parser fuzz, the lag proxy
#                                 (bench --lag: host CPU timing, so alone), the bench headless
# Budgets (a tier on an already built tree; the build's time is recorded next to it, not counted): fast under 10 s,
# gate under 300 s, nightly whatever it takes.
# One line per step (PASS / FAIL / WARN, seconds); each step's full output in $CHECK_LOG (default
# $FW_CACHE/checks/<time>-<tier>). Functional tests are reported, never fail a tier (WARN) while the suite grows.
# Exit 1 if any step failed. Build = $FW_OUT (env or local.env), else ./out.
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
[ -f "$HERE/../local.env" ] && . "$HERE/../local.env"
tier=${1:-}; shift || true
ids="" den="" weekly=""; [ "$(date +%u)" = 7 ] && weekly=1
while [ $# -gt 0 ]; do case "$1" in --ids) ids=$2; shift ;; --den) den=$2; shift ;; --weekly) weekly=1 ;; *) echo "unknown arg $1"; exit 2 ;; esac; shift; done
case "$tier" in fast|gate|nightly) ;; *) sed -n 2,12p "$0"; exit 2 ;; esac
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
# steps that share nothing run at once: start each, then join prints their lines in order (and sets fail)
bg=()
start() { local kind=$1 name=$2; shift 2; run "$kind" "$name" "$@" >"$LOG/$name.line" & bg+=("$name"); }
join() { wait; for n in "${bg[@]}"; do cat "$LOG/$n.line"; grep -q "^FAIL $n " "$LOG/$n.line" && fail=1; done; bg=(); }
dens=1.5x; [ "$tier" = fast ] || dens=${den:-1.5x 1x}
[ -z "$den" ] || [ "$tier" = gate ] || { echo "--den is for the gate tier"; exit 2; }
has() { case " $dens " in *" $1 "*) return 0 ;; esac; return 1; }
for d in $dens; do [ -f "$B/$d/fw.mjs" ] || { echo "no $d build in $B: run build.sh $d first"; exit 2; }; done

if [ "$tier" = nightly ]; then
  check asan-1.5x tools/asan.sh 1.5x
  soft functional-nightly node tools/functional.mjs --tier nightly
  check theme-fuzz tools/theme_fuzz.sh
  check lag node tools/bench.mjs --lag --quiet --out "$LOG/lag.json"
  soft bench node tools/bench.mjs --quiet --out "$LOG/bench.json"
  [ -n "$weekly" ] && check asan-1x tools/asan.sh 1x
  [ $fail = 0 ] && echo "CHECK $tier: all green (logs $LOG)" || echo "CHECK $tier: FAILED (logs $LOG)"; exit $fail
fi

# every tier: the quick checks that are not rules (density-free; with --den, in the 1.5x half)
if has 1.5x; then
check features node tools/features.mjs check
check contrast node tools/contrast.mjs --summary
check contract-lint node tools/contract_lint.mjs --summary
check http-status node --test tools/http_status.test.mjs
check report-page node --test tools/report_page.test.mjs
check backlight node --test tools/backlight.test.mjs
check channels node --test tools/channels.test.mjs
check fw-notes bash -c 'o=$(mktemp) && c++ -std=c++17 -Wall -Wextra -Werror -o "$o" ../esphome/board/fw_notes.test.cpp && "$o"; r=$?; rm -f "$o"; exit $r'
check fw-settle bash -c 'o=$(mktemp) && c++ -std=c++17 -Wall -Wextra -Werror -o "$o" ../esphome/board/fw_settle.test.cpp && "$o"; r=$?; rm -f "$o"; exit $r'
check fw-trial bash -c 'o=$(mktemp) && c++ -std=c++17 -Wall -Wextra -Werror -o "$o" ../esphome/board/fw_trial.test.cpp && "$o"; r=$?; rm -f "$o"; exit $r'
check fw-shot bash -c 'o=$(mktemp) && c++ -std=c++17 -Wall -Wextra -Werror -o "$o" ../firmware/fw_shot.test.cpp && "$o"; r=$?; rm -f "$o"; exit $r'
check fw-report bash -c 'o=$(mktemp) && c++ -std=c++17 -Wall -Wextra -Werror -o "$o" ../firmware/fw_report.test.cpp && "$o"; r=$?; rm -f "$o"; exit $r'
check ota-pw bash -c 'o=$(mktemp) && c++ -std=c++17 -Wall -Wextra -Werror -o "$o" ../esphome/board/ota_pw.test.cpp && OTA_PW_GLUE=../esphome/board/update_password.h "$o"; r=$?; rm -f "$o"; exit $r'
check catalogue node tools/gen_catalogue.mjs --build "$B"
fi

sel=(); [ -n "$ids" ] && sel=(--ids "$ids")
if [ "$tier" = fast ]; then
  check rules node tools/functional.mjs --rules --tier fast --den 1.5x --theme all "${sel[@]}"
  [ -n "$ids" ] || { [ $fail = 0 ] && echo "CHECK fast: all green, no --ids (logs $LOG)" || echo "CHECK fast: FAILED (logs $LOG)"; exit $fail; }
  soft functional node tools/functional.mjs --den 1.5x --ids "$ids"
else
  # at once: the rules (pixel contrast, visual and behaviour, the longest, each on its own), the functional tests
  if has 1.5x; then
    start hard rules-1.5x-contrast node tools/functional.mjs --rules --tier gate --rule pixel-contrast --den 1.5x --theme all "${sel[@]}"
    start hard rules-1.5x-visual node tools/functional.mjs --rules --tier gate --rule visual --den 1.5x "${sel[@]}"
    start hard rules-1.5x node tools/functional.mjs --rules --tier gate --skip pixel-contrast,visual,behaviour --den 1.5x "${sel[@]}"
  fi
  has 1x && start hard rules-1x node tools/functional.mjs --rules --tier gate --skip behaviour --den 1x --theme all "${sel[@]}"
  if [ -n "$den" ]; then
    start hard rules-behaviour-$den node tools/functional.mjs --rules --tier gate --rule behaviour --den "$den" "${sel[@]}"
    start soft functional-$den node tools/functional.mjs --den "$den" "${sel[@]}"
  else
    start hard rules-behaviour node tools/functional.mjs --rules --tier gate --rule behaviour "${sel[@]}"
    start soft functional node tools/functional.mjs "${sel[@]}"
  fi
  # the workbench's Playwright tests (the panel's editor, the link page): the whole gate only, on a machine with the
  # workbench running (WORKBENCH_URL, default localhost:4322: it serves this tree and the build in $FW_OUT); CI has none
  # the panel's picture decode + fetch on simulated ESP-IDF (ASan, TSan): density-free, the whole gate only
  if [ -z "$ids" ] && has 1.5x; then
    start hard fw-images bash -c '../firmware/tests/imgdec/run.sh && ../firmware/tests/imgfetch/run.sh'
  fi
  if [ -z "$ids" ]; then
    if [ -n "${CI:-}" ]; then echo "SKIP workbench (CI: no workbench server)"
    else start hard workbench bash -c 'cd ../workbench && node --test --test-concurrency=1 tests/editor/*.test.mjs tests/link/*.test.mjs'; fi
  fi
  join
fi
[ $fail = 0 ] && echo "CHECK $tier: all green (logs $LOG)" || echo "CHECK $tier: FAILED (logs $LOG)"
exit $fail
