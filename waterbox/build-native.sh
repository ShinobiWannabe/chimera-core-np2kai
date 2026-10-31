#!/bin/sh
# build-native.sh - the native reference: NP2kai and the driver built for this
# host, no sandbox. It is where the core is debugged (gdb, printf), and what
# the gate compares the sandboxed core against.
#
# Usage: build-native.sh [np21|np2]     (default np21, the IA-32 machine)
# Output: build/native-<flavour>/run-native
#   NP2_OUT=dir      build somewhere else (the gate's negative controls)
#   NP2_EXTRA="..."  more flags for the driver only, e.g. -DNP2DRV_HOST_CLOCK
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
FLAVOUR=${1:-np21}
NP2=$root/extern/NP2kai
PORT=$here/port
MB=${MINIBOX_DIR:-$root/../chimera/extern/chimera-common-minibox}
. "$here/np2-flags.sh"
out=${NP2_OUT:-$root/build/native-$FLAVOUR}
obj=$out/obj
mkdir -p "$obj"

SOURCES=$(python3 "$root/tools/sources.py" "$NP2" "$FLAVOUR" | sed "s|^|$NP2/|")
SOURCES="$SOURCES $(ls "$PORT"/*.c "$PORT"/*.cpp 2>/dev/null) $here/np2-driver.c $here/run-native.c $LIBCHDR/unity.c $LIBCHDR/src/libchdr_codec_avhuff.c"

# The guest kit's headers (settings, slots, ECL_EXPORT) work natively too.
COMMON="-O2 -g -fno-strict-aliasing $NP2_DEFINES $NP2_INCLUDES -I$MB/extern/emulibc -I$MB/source/guest/include -I$MB/extern/jsmn"
fail=0; objs=""
for s in $SOURCES; do
  [ -f "$s" ] || continue
  rel=${s#$root/}
  o=$obj/$(printf '%s' "$rel" | tr '/' '_' | sed 's/\.[^.]*$/.o/')
  objs="$objs $o"
  if [ ! -f "$o" ] || [ "$s" -nt "$o" ] || [ "$here/np2-flags.sh" -nt "$o" ] || [ "$PORT/chimera-port.h" -nt "$o" ] || { { [ "${s%/*}" = "$here" ] || [ "${s%/*}" = "$PORT" ]; } && [ -n "$(find "$here" "$PORT" -maxdepth 1 -name '*.h' -newer "$o" -print -quit)" ]; }; then
    case "$s" in
      "$LIBCHDR/unity.c"|"$LIBCHDR/src/libchdr_codec_avhuff.c") gcc -O2 -g $LIBCHDR_FLAGS $NP2_ZEROINIT -w -c "$s" -o "$o" 2>>"$out/errors.log" || { echo "FAILED: libchdr"; fail=1; } ;;
      *.cpp) g++ $COMMON $NP2_CXXFLAGS $(np2_file_flags "$s") -w -c "$s" -o "$o" 2>>"$out/errors.log" || { echo "FAILED: $rel"; fail=1; } ;;
      *)     gcc $COMMON $NP2_CFLAGS $(np2_file_flags "$s") $([ "$s" = "$here/np2-driver.c" ] && echo "${NP2_EXTRA:-}") -w -c "$s" -o "$o" 2>>"$out/errors.log" || { echo "FAILED: $rel"; fail=1; } ;;
    esac
  fi
done
[ $fail -eq 0 ] || { echo "compile failed; see $out/errors.log"; exit 1; }
# an object no source makes any more (a file sources.py now drops) would still
# be linked by the gate's unit tests, which take every object in the folder
for o in "$obj"/*.o; do
  case " $objs " in *" $o "*) ;; *) rm -f "$o" ;; esac
done
g++ -o "$out/run-native" $objs -lm || { echo "link failed"; exit 1; }
echo "OK: $out/run-native"
