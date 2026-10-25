#!/bin/sh
# build-run-wbx.sh - builds run-wbx, the harness that drives core.wbx through
# the miniBox host directly: no frontend, no package.
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
mb="${MINIBOX_DIR:-$root/../chimera/extern/chimera-common-minibox}"
out="${NP2_OUT:-$root/build}"
mkdir -p "$out"
lib=""
for c in "$mb/build/source/host/libminiboxhost.a" "$mb/build/meson-linux/source/host/libminiboxhost.a"; do
	[ -f "$c" ] && { lib="$c"; break; }
done
[ -n "$lib" ] || { echo "no libminiboxhost.a under $mb/build" >&2; exit 1; }
gcc -O2 -g -Wall -I"$mb/source/host" -I"$here" -o "$out/run-wbx" "$here/run-wbx.c" "$lib" -lpthread -lm
echo "OK: $out/run-wbx"
