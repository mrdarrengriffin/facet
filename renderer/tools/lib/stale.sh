# stale <object> [extra deps...]: true when the object must be rebuilt: no object, no depfile (<object>.d, written by
# the compiler's -MMD), or the source, a header it includes or an extra dep is newer than it (or gone). Headers under
# $STALE_SKIP (the LVGL tree: never edited) are not checked. Sourced by build.sh and tools/asan.sh.
stale() {
  local o=$1 x; shift
  [ -f "$o" ] && [ -f "$o.d" ] || return 0
  for x in "$@" $(sed -e 's/^[^:]*://' -e 's/\\$//' "$o.d"); do
    case "$x" in "${STALE_SKIP:-//}"/*) continue ;; esac
    [ -e "$x" ] || return 0
    [ "$x" -nt "$o" ] && return 0
  done
  return 1
}
